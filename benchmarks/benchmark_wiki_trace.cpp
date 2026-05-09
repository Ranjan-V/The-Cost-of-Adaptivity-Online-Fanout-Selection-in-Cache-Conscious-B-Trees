#include "../include/btree/btree.h"
#include "../include/adaptive/segmented_adaptive_btree.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <direct.h>
#include <windows.h>
#else
#include <chrono>
#include <sys/stat.h>
#endif

using cache_adaptive::SegmentedAdaptiveBPlusTree;

volatile long long wiki_sink = 0;

class Timer {
public:
    Timer() {
        reset();
    }

    void reset() {
#ifdef _WIN32
        QueryPerformanceCounter(&start_);
#else
        start_ = std::chrono::steady_clock::now();
#endif
    }

    double elapsed_ms() const {
#ifdef _WIN32
        LARGE_INTEGER end;
        LARGE_INTEGER frequency;
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&frequency);
        return (static_cast<double>(end.QuadPart - start_.QuadPart) * 1000.0) /
               static_cast<double>(frequency.QuadPart);
#else
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
#endif
    }

    double elapsed_us() const {
        return elapsed_ms() * 1000.0;
    }

private:
#ifdef _WIN32
    LARGE_INTEGER start_;
#else
    std::chrono::steady_clock::time_point start_;
#endif
};

struct TraceRow {
    std::string day;
    int article_id;
    long long views;

    TraceRow() : day(), article_id(0), views(0) {}
    TraceRow(const std::string& trace_day, int id, long long count)
        : day(trace_day), article_id(id), views(count) {}
};

struct Operation {
    enum Type { READ, UPDATE };

    Type type;
    int key;
    int value;

    Operation(Type op_type = READ, int op_key = 0, int op_value = 0)
        : type(op_type), key(op_key), value(op_value) {}
};

struct DayWorkload {
    std::string day;
    std::vector<Operation> operations;
};

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            quoted = !quoted;
        } else if (c == ',' && !quoted) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    fields.push_back(current);
    return fields;
}

std::vector<TraceRow> load_trace_csv(const std::string& path,
                                     size_t max_articles,
                                     size_t& unique_articles) {
    std::vector<TraceRow> rows;
    unique_articles = 0;

    std::ifstream in(path.c_str());
    if (!in) {
        return rows;
    }

    std::string line;
    if (!std::getline(in, line)) {
        return rows;
    }

    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() < 3) {
            continue;
        }

        int article_id = std::atoi(fields[1].c_str());
        if (article_id < 0 || static_cast<size_t>(article_id) >= max_articles) {
            continue;
        }

        long long views = std::atoll(fields[2].c_str());
        if (views <= 0) {
            continue;
        }

        rows.push_back(TraceRow(fields[0], article_id, views));
        unique_articles =
            std::max(unique_articles, static_cast<size_t>(article_id + 1));
    }

    return rows;
}

std::vector<TraceRow> generate_fallback_trace(size_t days,
                                              size_t articles_per_day,
                                              size_t max_articles,
                                              size_t& unique_articles) {
    std::vector<TraceRow> rows;
    unique_articles = std::max(max_articles, static_cast<size_t>(1));

    const size_t segments = 8;
    const size_t segment_size = std::max<size_t>(1, max_articles / segments);

    for (size_t day = 0; day < days; ++day) {
        size_t hot_segment = (day * 3) % segments;
        size_t hot_start = hot_segment * segment_size;
        for (size_t rank = 0; rank < articles_per_day; ++rank) {
            size_t id = 0;
            if (rank < articles_per_day * 8 / 10) {
                id = hot_start + (rank % segment_size);
            } else {
                id = (rank * 97 + day * 271) % max_articles;
            }
            if (id >= max_articles) {
                id = max_articles - 1;
            }
            long long views =
                static_cast<long long>((articles_per_day - rank + 1) * 100 + day * 17);
            std::ostringstream label;
            label << "fallback-day-" << (day + 1);
            rows.push_back(TraceRow(label.str(), static_cast<int>(id), views));
        }
    }

    return rows;
}

std::vector<DayWorkload> build_day_workloads(const std::vector<TraceRow>& rows,
                                             size_t operations_per_day,
                                             double update_ratio,
                                             unsigned seed) {
    std::map<std::string, std::vector<TraceRow> > by_day;
    for (size_t i = 0; i < rows.size(); ++i) {
        by_day[rows[i].day].push_back(rows[i]);
    }

    std::vector<DayWorkload> workloads;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> probability(0.0, 1.0);

    for (std::map<std::string, std::vector<TraceRow> >::const_iterator day_it =
             by_day.begin();
         day_it != by_day.end();
         ++day_it) {
        const std::vector<TraceRow>& day_rows = day_it->second;
        std::vector<double> cdf;
        cdf.reserve(day_rows.size());

        double total = 0.0;
        for (size_t i = 0; i < day_rows.size(); ++i) {
            total += static_cast<double>(std::max<long long>(1, day_rows[i].views));
        }

        double cumulative = 0.0;
        for (size_t i = 0; i < day_rows.size(); ++i) {
            cumulative +=
                static_cast<double>(std::max<long long>(1, day_rows[i].views)) / total;
            cdf.push_back(cumulative);
        }
        if (!cdf.empty()) {
            cdf.back() = 1.0;
        }

        DayWorkload workload;
        workload.day = day_it->first;
        workload.operations.reserve(operations_per_day);

        for (size_t op = 0; op < operations_per_day; ++op) {
            std::vector<double>::const_iterator sample =
                std::lower_bound(cdf.begin(), cdf.end(), probability(rng));
            size_t index = static_cast<size_t>(sample - cdf.begin());
            if (index >= day_rows.size()) {
                index = day_rows.empty() ? 0 : day_rows.size() - 1;
            }

            int key = day_rows.empty() ? 0 : day_rows[index].article_id;
            if (probability(rng) < update_ratio) {
                workload.operations.push_back(
                    Operation(Operation::UPDATE, key, static_cast<int>(op + workloads.size() * operations_per_day)));
            } else {
                workload.operations.push_back(Operation(Operation::READ, key, 0));
            }
        }

        workloads.push_back(workload);
    }

    return workloads;
}

class IndexAdapter {
public:
    virtual ~IndexAdapter() {}
    virtual std::string name() const = 0;
    virtual void begin_load() {}
    virtual void end_load() {}
    virtual void insert_or_update(int key, int value) = 0;
    virtual bool read(int key, int& value) = 0;
    virtual size_t final_fanout() const = 0;
    virtual size_t adaptations() const = 0;
    virtual size_t restructures() const = 0;
};

class StaticBTreeAdapter : public IndexAdapter {
public:
    explicit StaticBTreeAdapter(size_t fanout)
        : tree_(fanout), fanout_(fanout) {}

    std::string name() const {
        std::ostringstream out;
        out << "Static B+Tree f=" << fanout_;
        return out.str();
    }

    void insert_or_update(int key, int value) {
        tree_.insert(key, value);
    }

    bool read(int key, int& value) {
        return tree_.search(key, value);
    }

    size_t final_fanout() const {
        return fanout_;
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

private:
    cabtree::BPlusTree<int, int> tree_;
    size_t fanout_;
};

class StaticRegionalAdapter : public IndexAdapter {
public:
    StaticRegionalAdapter(size_t article_count, size_t segment_count, size_t fanout)
        : article_count_(article_count == 0 ? 1 : article_count)
        , segment_count_(segment_count == 0 ? 1 : segment_count)
        , fanout_(fanout)
        , segment_size_((article_count_ + segment_count_ - 1) / segment_count_)
        , trees_() {
        trees_.reserve(segment_count_);
        for (size_t i = 0; i < segment_count_; ++i) {
            trees_.push_back(new cabtree::BPlusTree<int, int>(fanout_));
        }
    }

    ~StaticRegionalAdapter() {
        for (size_t i = 0; i < trees_.size(); ++i) {
            delete trees_[i];
        }
    }

    std::string name() const {
        std::ostringstream out;
        if (fanout_ == 64) {
            out << "Static Region x8";
        } else {
            out << "Static Region f=" << fanout_;
        }
        return out.str();
    }

    void insert_or_update(int key, int value) {
        trees_[segment_for(key)]->insert(key, value);
    }

    bool read(int key, int& value) {
        return trees_[segment_for(key)]->search(key, value);
    }

    size_t final_fanout() const {
        return fanout_;
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

private:
    size_t segment_for(int key) const {
        if (key < 0) {
            return 0;
        }
        size_t segment = static_cast<size_t>(key) / std::max<size_t>(1, segment_size_);
        if (segment >= segment_count_) {
            segment = segment_count_ - 1;
        }
        return segment;
    }

    size_t article_count_;
    size_t segment_count_;
    size_t fanout_;
    size_t segment_size_;
    std::vector<cabtree::BPlusTree<int, int>*> trees_;
};

class SegmentedAdapter : public IndexAdapter {
public:
    SegmentedAdapter(size_t article_count,
                     size_t segment_count,
                     size_t adaptation_interval,
                     bool enable_adaptation)
        : tree_(segment_count,
                0,
                static_cast<int>(article_count > 0 ? article_count - 1 : 0),
                64,
                adaptation_interval,
                8,
                256,
                32)
        , enable_adaptation_(enable_adaptation) {}

    std::string name() const {
        return enable_adaptation_ ? "Segmented Adaptive" : "Segmented No Adapt";
    }

    void begin_load() {
        tree_.set_adaptation_enabled(false);
    }

    void end_load() {
        tree_.reset_adaptation_state();
        tree_.set_adaptation_enabled(enable_adaptation_);
    }

    void insert_or_update(int key, int value) {
        tree_.insert(key, value);
    }

    bool read(int key, int& value) {
        return tree_.search(key, value);
    }

    size_t final_fanout() const {
        return tree_.get_average_fanout();
    }

    size_t adaptations() const {
        return tree_.get_stats().total_adaptations;
    }

    size_t restructures() const {
        return tree_.get_stats().total_restructures;
    }

private:
    SegmentedAdaptiveBPlusTree<int, int> tree_;
    bool enable_adaptation_;
};

struct BenchmarkResult {
    std::string day;
    std::string index_name;
    size_t operations;
    size_t reads;
    size_t updates;
    size_t misses;
    double load_ms;
    double run_ms;
    double throughput_ops_sec;
    double p50_us;
    double p95_us;
    double p99_us;
    double max_us;
    size_t final_fanout;
    size_t adaptations;
    size_t restructures;

    BenchmarkResult()
        : operations(0)
        , reads(0)
        , updates(0)
        , misses(0)
        , load_ms(0.0)
        , run_ms(0.0)
        , throughput_ops_sec(0.0)
        , p50_us(0.0)
        , p95_us(0.0)
        , p99_us(0.0)
        , max_us(0.0)
        , final_fanout(0)
        , adaptations(0)
        , restructures(0) {}
};

double percentile(const std::vector<double>& sorted_values, double p) {
    if (sorted_values.empty()) {
        return 0.0;
    }
    double position = p * static_cast<double>(sorted_values.size() - 1);
    size_t index = static_cast<size_t>(position + 0.5);
    if (index >= sorted_values.size()) {
        index = sorted_values.size() - 1;
    }
    return sorted_values[index];
}

std::vector<BenchmarkResult> run_index(IndexAdapter& index,
                                       const std::vector<DayWorkload>& workloads,
                                       size_t article_count) {
    std::vector<BenchmarkResult> results;

    index.begin_load();
    Timer load_timer;
    for (size_t id = 0; id < article_count; ++id) {
        index.insert_or_update(static_cast<int>(id), static_cast<int>(id * 10));
    }
    double load_ms = load_timer.elapsed_ms();
    index.end_load();

    for (size_t day = 0; day < workloads.size(); ++day) {
        const DayWorkload& workload = workloads[day];
        BenchmarkResult result;
        result.day = workload.day;
        result.index_name = index.name();
        result.operations = workload.operations.size();
        result.load_ms = day == 0 ? load_ms : 0.0;

        size_t adaptations_before = index.adaptations();
        size_t restructures_before = index.restructures();

        std::vector<double> latencies_us;
        latencies_us.reserve(workload.operations.size());

        int value = 0;
        Timer run_timer;
        for (size_t i = 0; i < workload.operations.size(); ++i) {
            Timer op_timer;
            const Operation& op = workload.operations[i];
            if (op.type == Operation::READ) {
                ++result.reads;
                bool found = index.read(op.key, value);
                if (found) {
                    wiki_sink += value;
                } else {
                    ++result.misses;
                }
            } else {
                ++result.updates;
                index.insert_or_update(op.key, op.value);
                wiki_sink += op.value;
            }
            latencies_us.push_back(op_timer.elapsed_us());
        }
        result.run_ms = run_timer.elapsed_ms();

        std::sort(latencies_us.begin(), latencies_us.end());
        result.p50_us = percentile(latencies_us, 0.50);
        result.p95_us = percentile(latencies_us, 0.95);
        result.p99_us = percentile(latencies_us, 0.99);
        result.max_us = latencies_us.empty() ? 0.0 : latencies_us.back();
        result.throughput_ops_sec = result.run_ms > 0.0
            ? static_cast<double>(result.operations) / result.run_ms * 1000.0
            : 0.0;
        result.final_fanout = index.final_fanout();
        result.adaptations = index.adaptations() - adaptations_before;
        result.restructures = index.restructures() - restructures_before;
        results.push_back(result);
    }

    return results;
}

void ensure_results_dir() {
#ifdef _WIN32
    _mkdir("results");
#else
    mkdir("results", 0755);
#endif
}

void write_csv(const std::vector<BenchmarkResult>& results,
               const std::string& path,
               const std::string& trace_source) {
    std::ofstream out(path.c_str());
    if (!out) {
        std::cout << "WARNING: could not write " << path << std::endl;
        return;
    }

    out << "trace_source,day,index,operations,reads,updates,misses,load_ms,run_ms,"
        << "throughput_ops_sec,p50_us,p95_us,p99_us,max_us,final_fanout,"
        << "adaptations,restructures\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const BenchmarkResult& r = results[i];
        out << trace_source << ','
            << r.day << ','
            << r.index_name << ','
            << r.operations << ','
            << r.reads << ','
            << r.updates << ','
            << r.misses << ','
            << r.load_ms << ','
            << r.run_ms << ','
            << r.throughput_ops_sec << ','
            << r.p50_us << ','
            << r.p95_us << ','
            << r.p99_us << ','
            << r.max_us << ','
            << r.final_fanout << ','
            << r.adaptations << ','
            << r.restructures << '\n';
    }
}

struct AggregateRow {
    std::string index_name;
    size_t operations;
    double run_ms;
    double throughput_ops_sec;
    double avg_p95_us;
    size_t final_fanout;
    size_t adaptations;
    size_t restructures;

    AggregateRow()
        : operations(0)
        , run_ms(0.0)
        , throughput_ops_sec(0.0)
        , avg_p95_us(0.0)
        , final_fanout(0)
        , adaptations(0)
        , restructures(0) {}
};

std::vector<AggregateRow> aggregate_results(const std::vector<BenchmarkResult>& results) {
    std::map<std::string, AggregateRow> aggregate;
    std::map<std::string, size_t> counts;

    for (size_t i = 0; i < results.size(); ++i) {
        const BenchmarkResult& r = results[i];
        AggregateRow& row = aggregate[r.index_name];
        row.index_name = r.index_name;
        row.operations += r.operations;
        row.run_ms += r.run_ms;
        row.avg_p95_us += r.p95_us;
        row.final_fanout = r.final_fanout;
        row.adaptations += r.adaptations;
        row.restructures += r.restructures;
        counts[r.index_name] += 1;
    }

    std::vector<AggregateRow> output;
    for (std::map<std::string, AggregateRow>::iterator it = aggregate.begin();
         it != aggregate.end();
         ++it) {
        AggregateRow row = it->second;
        row.throughput_ops_sec = row.run_ms > 0.0
            ? static_cast<double>(row.operations) / row.run_ms * 1000.0
            : 0.0;
        row.avg_p95_us /= static_cast<double>(std::max<size_t>(1, counts[it->first]));
        output.push_back(row);
    }

    std::sort(output.begin(), output.end(),
              [](const AggregateRow& a, const AggregateRow& b) {
                  return a.throughput_ops_sec > b.throughput_ops_sec;
              });
    return output;
}

void print_aggregate(const std::vector<AggregateRow>& rows) {
    double baseline = 0.0;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].index_name == "Static B+Tree f=64") {
            baseline = rows[i].throughput_ops_sec;
            break;
        }
    }

    std::cout << std::left
              << std::setw(24) << "Index"
              << std::right
              << std::setw(12) << "Throughput"
              << std::setw(10) << "vs f64"
              << std::setw(10) << "p95us"
              << std::setw(8) << "fanout"
              << std::setw(8) << "adapt"
              << std::setw(8) << "rebuild"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (size_t i = 0; i < rows.size(); ++i) {
        const AggregateRow& row = rows[i];
        double speedup = baseline > 0.0 ? row.throughput_ops_sec / baseline : 0.0;
        std::cout << std::left
                  << std::setw(24) << row.index_name
                  << std::right
                  << std::setw(12) << std::fixed << std::setprecision(0)
                  << row.throughput_ops_sec
                  << std::setw(10) << std::fixed << std::setprecision(2)
                  << speedup
                  << std::setw(10) << std::fixed << std::setprecision(3)
                  << row.avg_p95_us
                  << std::setw(8) << row.final_fanout
                  << std::setw(8) << row.adaptations
                  << std::setw(8) << row.restructures
                  << std::endl;
    }
}

size_t parse_size_arg(int argc, char* argv[], int index, size_t fallback) {
    if (argc <= index) {
        return fallback;
    }
    long long parsed = std::atoll(argv[index]);
    return parsed > 0 ? static_cast<size_t>(parsed) : fallback;
}

double parse_double_arg(int argc, char* argv[], int index, double fallback) {
    if (argc <= index) {
        return fallback;
    }
    double parsed = std::atof(argv[index]);
    return parsed >= 0.0 ? parsed : fallback;
}

int main(int argc, char* argv[]) {
    std::string trace_path = argc > 1 ? argv[1] : "data/wiki_pageviews_trace.csv";
    size_t operations_per_day = parse_size_arg(argc, argv, 2, 20000);
    size_t adaptation_interval = parse_size_arg(argc, argv, 3, 5000);
    size_t max_articles = parse_size_arg(argc, argv, 4, 10000);
    double update_ratio = parse_double_arg(argc, argv, 5, 0.02);
    const size_t segment_count = 8;

    std::cout << "\n============================================================\n";
    std::cout << "  Wikipedia Pageview Trace Benchmark\n";
    std::cout << "============================================================\n";
    std::cout << "Trace: " << trace_path
              << ", Operations/day: " << operations_per_day
              << ", Adapt interval: " << adaptation_interval
              << ", Max articles: " << max_articles
              << ", Update ratio: " << update_ratio << std::endl;
    std::cout << "Usage: bench_wiki_trace.exe [trace_csv] [ops_per_day] [adapt_interval] [max_articles] [update_ratio]\n";
    std::cout << "CSV output: results/wiki_trace_summary.csv\n\n";

    ensure_results_dir();

    size_t article_count = 0;
    std::vector<TraceRow> trace_rows =
        load_trace_csv(trace_path, max_articles, article_count);
    std::string trace_source = "wikimedia";

    if (trace_rows.empty()) {
        trace_source = "fallback";
        article_count = 0;
        trace_rows = generate_fallback_trace(7, 500, max_articles, article_count);
        std::cout << "WARNING: trace file not found or empty; using deterministic fallback trace.\n";
    }

    article_count = std::max<size_t>(article_count, 1);
    std::vector<DayWorkload> workloads =
        build_day_workloads(trace_rows, operations_per_day, update_ratio, 424242);

    std::cout << "Trace source: " << trace_source
              << ", Days: " << workloads.size()
              << ", Rows: " << trace_rows.size()
              << ", Unique articles loaded: " << article_count << "\n\n";

    std::vector<BenchmarkResult> all_results;

    {
        StaticBTreeAdapter index(64);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    {
        StaticBTreeAdapter index(256);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    {
        StaticRegionalAdapter index(article_count, segment_count, 64);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    {
        StaticRegionalAdapter index(article_count, segment_count, 32);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    {
        StaticRegionalAdapter index(article_count, segment_count, 128);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    {
        SegmentedAdapter index(article_count, segment_count, adaptation_interval, false);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    {
        SegmentedAdapter index(article_count, segment_count, adaptation_interval, true);
        std::vector<BenchmarkResult> results = run_index(index, workloads, article_count);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    write_csv(all_results, "results/wiki_trace_summary.csv", trace_source);

    std::cout << "Aggregate results:\n";
    std::vector<AggregateRow> aggregate = aggregate_results(all_results);
    print_aggregate(aggregate);

    std::cout << "\nChecksum: " << wiki_sink << std::endl;
    std::cout << "Benchmark complete.\n";

    return 0;
}
