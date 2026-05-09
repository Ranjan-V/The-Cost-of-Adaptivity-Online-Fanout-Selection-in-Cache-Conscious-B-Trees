#include "../include/btree/btree.h"
#include "../include/adaptive/segmented_adaptive_btree.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
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

volatile long long shifting_sink = 0;

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

struct Operation {
    enum Type { READ, UPDATE };

    Type type;
    int key;
    int value;

    Operation(Type op_type = READ, int op_key = 0, int op_value = 0)
        : type(op_type), key(op_key), value(op_value) {}
};

struct PhaseSpec {
    std::string name;
    int hot_segment;
    double read_ratio;
    double hot_access_ratio;

    PhaseSpec(const std::string& phase_name,
              int phase_hot_segment,
              double phase_read_ratio,
              double phase_hot_access_ratio)
        : name(phase_name)
        , hot_segment(phase_hot_segment)
        , read_ratio(phase_read_ratio)
        , hot_access_ratio(phase_hot_access_ratio) {}
};

class ZipfSampler {
public:
    ZipfSampler(size_t item_count, double theta)
        : cdf_() {
        if (item_count == 0) {
            item_count = 1;
        }
        cdf_.reserve(item_count);

        double sum = 0.0;
        for (size_t i = 1; i <= item_count; ++i) {
            sum += 1.0 / std::pow(static_cast<double>(i), theta);
        }

        double cumulative = 0.0;
        for (size_t i = 1; i <= item_count; ++i) {
            cumulative += (1.0 / std::pow(static_cast<double>(i), theta)) / sum;
            cdf_.push_back(cumulative);
        }
        cdf_.back() = 1.0;
    }

    int next(std::mt19937& rng) const {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double r = dist(rng);
        std::vector<double>::const_iterator it =
            std::lower_bound(cdf_.begin(), cdf_.end(), r);
        return static_cast<int>(it - cdf_.begin());
    }

private:
    std::vector<double> cdf_;
};

size_t partition_for_key(int key, size_t record_count, size_t partition_count) {
    if (partition_count == 0) {
        return 0;
    }
    if (key < 0) {
        return 0;
    }
    size_t safe_records = record_count == 0 ? 1 : record_count;
    size_t partition_size = (safe_records + partition_count - 1) / partition_count;
    if (partition_size == 0) {
        partition_size = 1;
    }
    size_t partition = static_cast<size_t>(key) / partition_size;
    if (partition >= partition_count) {
        partition = partition_count - 1;
    }
    return partition;
}

void segment_bounds(size_t segment_id,
                    size_t record_count,
                    size_t partition_count,
                    int& start_key,
                    int& end_key) {
    size_t safe_records = record_count == 0 ? 1 : record_count;
    size_t safe_partitions = partition_count == 0 ? 1 : partition_count;
    size_t partition_size = (safe_records + safe_partitions - 1) / safe_partitions;
    size_t start = segment_id * partition_size;
    size_t end = std::min(safe_records - 1, start + partition_size - 1);

    if (start >= safe_records) {
        start = safe_records - 1;
        end = safe_records - 1;
    }

    start_key = static_cast<int>(start);
    end_key = static_cast<int>(end);
}

std::vector<Operation> generate_phase_operations(const PhaseSpec& phase,
                                                 size_t phase_index,
                                                 size_t record_count,
                                                 size_t operations_per_phase,
                                                 size_t partition_count,
                                                 double theta,
                                                 unsigned seed) {
    std::vector<Operation> operations;
    operations.reserve(operations_per_phase);

    size_t safe_records = record_count == 0 ? 1 : record_count;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> probability(0.0, 1.0);
    std::uniform_int_distribution<int> uniform_key(
        0, static_cast<int>(safe_records - 1));

    int hot_start = 0;
    int hot_end = static_cast<int>(safe_records - 1);
    if (phase.hot_segment >= 0) {
        size_t hot_segment = static_cast<size_t>(phase.hot_segment);
        if (hot_segment >= partition_count) {
            hot_segment = partition_count - 1;
        }
        segment_bounds(hot_segment, safe_records, partition_count, hot_start, hot_end);
    }

    size_t hot_span = static_cast<size_t>(hot_end - hot_start + 1);
    ZipfSampler hot_zipf(hot_span == 0 ? 1 : hot_span, theta);

    for (size_t i = 0; i < operations_per_phase; ++i) {
        bool use_hot_segment =
            phase.hot_segment >= 0 && probability(rng) < phase.hot_access_ratio;

        int key = 0;
        if (use_hot_segment) {
            int offset = hot_zipf.next(rng);
            if (offset >= static_cast<int>(hot_span)) {
                offset = static_cast<int>(hot_span - 1);
            }
            key = hot_start + offset;
        } else {
            key = uniform_key(rng);
        }

        bool is_read = probability(rng) < phase.read_ratio;
        if (is_read) {
            operations.push_back(Operation(Operation::READ, key, 0));
        } else {
            int value = static_cast<int>(safe_records +
                                         phase_index * operations_per_phase + i);
            operations.push_back(Operation(Operation::UPDATE, key, value));
        }
    }

    return operations;
}

class IndexAdapter {
public:
    virtual ~IndexAdapter() {}
    virtual std::string name() const = 0;
    virtual void begin_load() {}
    virtual void end_load() {}
    virtual void begin_phase(const PhaseSpec&) {}
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

class StaticRegionalBTreeAdapter : public IndexAdapter {
public:
    StaticRegionalBTreeAdapter(size_t record_count,
                               size_t partition_count,
                               size_t fanout)
        : record_count_(record_count == 0 ? 1 : record_count)
        , partition_count_(partition_count == 0 ? 1 : partition_count)
        , fanout_(fanout)
        , trees_() {
        trees_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            trees_.push_back(new cabtree::BPlusTree<int, int>(fanout_));
        }
    }

    ~StaticRegionalBTreeAdapter() {
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
        trees_[partition_for_key(key, record_count_, partition_count_)]->insert(key, value);
    }

    bool read(int key, int& value) {
        return trees_[partition_for_key(key, record_count_, partition_count_)]->search(key, value);
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
    size_t record_count_;
    size_t partition_count_;
    size_t fanout_;
    std::vector<cabtree::BPlusTree<int, int>*> trees_;
};

class SegmentedAdapter : public IndexAdapter {
public:
    SegmentedAdapter(size_t record_count,
                     size_t partition_count,
                     size_t adaptation_interval,
                     bool adaptation_enabled)
        : tree_(partition_count,
                0,
                static_cast<int>(record_count > 0 ? record_count - 1 : 0),
                64,
                adaptation_interval,
                8,
                256,
                32)
        , adaptation_enabled_(adaptation_enabled) {}

    std::string name() const {
        return adaptation_enabled_ ? "Segmented Adaptive" : "Segmented No Adapt";
    }

    void begin_load() {
        tree_.set_adaptation_enabled(false);
    }

    void end_load() {
        tree_.reset_adaptation_state();
        tree_.set_adaptation_enabled(adaptation_enabled_);
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
    bool adaptation_enabled_;
};

class OraclePhasedAdapter : public IndexAdapter {
public:
    OraclePhasedAdapter(size_t record_count,
                        size_t partition_count,
                        size_t hot_fanout)
        : record_count_(record_count == 0 ? 1 : record_count)
        , partition_count_(partition_count == 0 ? 1 : partition_count)
        , default_fanout_(64)
        , hot_fanout_(hot_fanout)
        , trees_()
        , records_(partition_count_)
        , fanouts_(partition_count_, default_fanout_)
        , adaptations_(0)
        , restructures_(0) {
        trees_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            trees_.push_back(new cabtree::BPlusTree<int, int>(default_fanout_));
        }
    }

    ~OraclePhasedAdapter() {
        for (size_t i = 0; i < trees_.size(); ++i) {
            delete trees_[i];
        }
    }

    std::string name() const {
        std::ostringstream out;
        out << "Oracle Hot f=" << hot_fanout_;
        return out.str();
    }

    void begin_phase(const PhaseSpec& phase) {
        for (size_t i = 0; i < partition_count_; ++i) {
            size_t desired = default_fanout_;
            if (phase.hot_segment >= 0 &&
                i == static_cast<size_t>(phase.hot_segment)) {
                desired = hot_fanout_;
            }

            if (desired != fanouts_[i]) {
                rebuild_segment(i, desired);
                ++adaptations_;
            }
        }
    }

    void insert_or_update(int key, int value) {
        size_t segment_id = partition_for_key(key, record_count_, partition_count_);
        records_[segment_id][key] = value;
        trees_[segment_id]->insert(key, value);
    }

    bool read(int key, int& value) {
        return trees_[partition_for_key(key, record_count_, partition_count_)]->search(key, value);
    }

    size_t final_fanout() const {
        size_t total = 0;
        for (size_t i = 0; i < fanouts_.size(); ++i) {
            total += fanouts_[i];
        }
        return fanouts_.empty() ? 0 : total / fanouts_.size();
    }

    size_t adaptations() const {
        return adaptations_;
    }

    size_t restructures() const {
        return restructures_;
    }

private:
    void rebuild_segment(size_t segment_id, size_t target_fanout) {
        cabtree::BPlusTree<int, int>* rebuilt =
            new cabtree::BPlusTree<int, int>(target_fanout);

        std::unordered_map<int, int>::const_iterator it;
        for (it = records_[segment_id].begin();
             it != records_[segment_id].end();
             ++it) {
            rebuilt->insert(it->first, it->second);
        }

        delete trees_[segment_id];
        trees_[segment_id] = rebuilt;
        fanouts_[segment_id] = target_fanout;
        ++restructures_;
    }

    size_t record_count_;
    size_t partition_count_;
    size_t default_fanout_;
    size_t hot_fanout_;
    std::vector<cabtree::BPlusTree<int, int>*> trees_;
    std::vector<std::unordered_map<int, int> > records_;
    std::vector<size_t> fanouts_;
    size_t adaptations_;
    size_t restructures_;
};

struct PhaseResult {
    std::string phase;
    std::string index_name;
    int hot_segment;
    size_t operations;
    size_t reads;
    size_t updates;
    size_t misses;
    double load_ms;
    double reconfigure_ms;
    double run_ms;
    double throughput_ops_sec;
    double throughput_with_reconfig_ops_sec;
    double p50_us;
    double p95_us;
    double p99_us;
    double max_us;
    size_t final_fanout;
    size_t adaptations;
    size_t restructures;

    PhaseResult()
        : hot_segment(-1)
        , operations(0)
        , reads(0)
        , updates(0)
        , misses(0)
        , load_ms(0.0)
        , reconfigure_ms(0.0)
        , run_ms(0.0)
        , throughput_ops_sec(0.0)
        , throughput_with_reconfig_ops_sec(0.0)
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

std::vector<PhaseResult> run_index(IndexAdapter& index,
                                   const std::vector<PhaseSpec>& phases,
                                   const std::vector<std::vector<Operation> >& operations_by_phase,
                                   size_t record_count) {
    std::vector<PhaseResult> results;

    index.begin_load();
    Timer load_timer;
    for (size_t i = 0; i < record_count; ++i) {
        index.insert_or_update(static_cast<int>(i), static_cast<int>(i * 10));
    }
    double load_ms = load_timer.elapsed_ms();
    index.end_load();

    for (size_t p = 0; p < phases.size(); ++p) {
        const PhaseSpec& phase = phases[p];
        const std::vector<Operation>& operations = operations_by_phase[p];

        PhaseResult result;
        result.phase = phase.name;
        result.index_name = index.name();
        result.hot_segment = phase.hot_segment;
        result.operations = operations.size();
        result.load_ms = (p == 0) ? load_ms : 0.0;

        size_t adaptations_before = index.adaptations();
        size_t restructures_before = index.restructures();

        Timer reconfigure_timer;
        index.begin_phase(phase);
        result.reconfigure_ms = reconfigure_timer.elapsed_ms();

        std::vector<double> latencies_us;
        latencies_us.reserve(operations.size());

        int value = 0;
        Timer run_timer;
        for (size_t i = 0; i < operations.size(); ++i) {
            Timer op_timer;
            const Operation& op = operations[i];
            if (op.type == Operation::READ) {
                ++result.reads;
                bool found = index.read(op.key, value);
                if (found) {
                    shifting_sink += value;
                } else {
                    ++result.misses;
                }
            } else {
                ++result.updates;
                index.insert_or_update(op.key, op.value);
                shifting_sink += op.value;
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

        double run_plus_reconfig = result.run_ms + result.reconfigure_ms;
        result.throughput_with_reconfig_ops_sec = run_plus_reconfig > 0.0
            ? static_cast<double>(result.operations) / run_plus_reconfig * 1000.0
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

void print_header() {
    std::cout << std::left
              << std::setw(13) << "Phase"
              << std::setw(22) << "Index"
              << std::right
              << std::setw(12) << "Throughput"
              << std::setw(11) << "incl_recfg"
              << std::setw(9) << "p95us"
              << std::setw(9) << "p99us"
              << std::setw(10) << "reconfms"
              << std::setw(8) << "fanout"
              << std::setw(7) << "adapt"
              << std::setw(8) << "rebuild"
              << std::endl;
    std::cout << std::string(109, '-') << std::endl;
}

void print_result(const PhaseResult& r) {
    std::cout << std::left
              << std::setw(13) << r.phase
              << std::setw(22) << r.index_name
              << std::right
              << std::setw(12) << std::fixed << std::setprecision(0)
              << r.throughput_ops_sec
              << std::setw(11) << r.throughput_with_reconfig_ops_sec
              << std::setw(9) << std::fixed << std::setprecision(3)
              << r.p95_us
              << std::setw(9) << r.p99_us
              << std::setw(10) << std::fixed << std::setprecision(3)
              << r.reconfigure_ms
              << std::setw(8) << r.final_fanout
              << std::setw(7) << r.adaptations
              << std::setw(8) << r.restructures
              << std::endl;
}

void write_csv(const std::vector<PhaseResult>& results, const std::string& path) {
    std::ofstream out(path.c_str());
    if (!out) {
        std::cout << "WARNING: could not write " << path << std::endl;
        return;
    }

    out << "phase,index,hot_segment,operations,reads,updates,misses,load_ms,"
        << "reconfigure_ms,run_ms,throughput_ops_sec,"
        << "throughput_with_reconfig_ops_sec,p50_us,p95_us,p99_us,max_us,"
        << "final_fanout,adaptations,restructures\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const PhaseResult& r = results[i];
        out << r.phase << ','
            << r.index_name << ','
            << r.hot_segment << ','
            << r.operations << ','
            << r.reads << ','
            << r.updates << ','
            << r.misses << ','
            << r.load_ms << ','
            << r.reconfigure_ms << ','
            << r.run_ms << ','
            << r.throughput_ops_sec << ','
            << r.throughput_with_reconfig_ops_sec << ','
            << r.p50_us << ','
            << r.p95_us << ','
            << r.p99_us << ','
            << r.max_us << ','
            << r.final_fanout << ','
            << r.adaptations << ','
            << r.restructures << '\n';
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
    return parsed > 0.0 ? parsed : fallback;
}

int main(int argc, char* argv[]) {
    size_t record_count = parse_size_arg(argc, argv, 1, 50000);
    size_t operations_per_phase = parse_size_arg(argc, argv, 2, 25000);
    double theta = parse_double_arg(argc, argv, 3, 0.99);
    size_t adaptation_interval = parse_size_arg(argc, argv, 4, 5000);
    const size_t partition_count = 8;

    std::cout << "\n============================================================\n";
    std::cout << "  Shifting Hot-Region + Oracle Benchmark\n";
    std::cout << "============================================================\n";
    std::cout << "Records: " << record_count
              << ", Operations/phase: " << operations_per_phase
              << ", Zipf theta: " << theta
              << ", Adapt interval: " << adaptation_interval << std::endl;
    std::cout << "Usage: bench_shifting.exe [records] [ops_per_phase] [theta] [adapt_interval]\n";
    std::cout << "CSV output: results/shifting_summary.csv\n";
    std::cout << "Oracle rows know the hot segment at phase boundaries; reconfiguration time is reported separately.\n\n";

    ensure_results_dir();

    std::vector<PhaseSpec> phases;
    phases.push_back(PhaseSpec("Hot-S0", 0, 0.95, 0.95));
    phases.push_back(PhaseSpec("Hot-S7", 7, 0.95, 0.95));
    phases.push_back(PhaseSpec("Uniform", -1, 0.95, 0.0));
    phases.push_back(PhaseSpec("Hot-S3-A", 3, 0.50, 0.85));

    std::vector<std::vector<Operation> > operations_by_phase;
    for (size_t p = 0; p < phases.size(); ++p) {
        operations_by_phase.push_back(
            generate_phase_operations(phases[p],
                                      p,
                                      record_count,
                                      operations_per_phase,
                                      partition_count,
                                      theta,
                                      static_cast<unsigned>(7777 + p)));
    }

    std::vector<PhaseResult> all_results;
    print_header();

    {
        StaticBTreeAdapter index(64);
        std::vector<PhaseResult> results =
            run_index(index, phases, operations_by_phase, record_count);
        for (size_t i = 0; i < results.size(); ++i) {
            all_results.push_back(results[i]);
            print_result(results[i]);
        }
    }
    {
        StaticBTreeAdapter index(256);
        std::vector<PhaseResult> results =
            run_index(index, phases, operations_by_phase, record_count);
        for (size_t i = 0; i < results.size(); ++i) {
            all_results.push_back(results[i]);
            print_result(results[i]);
        }
    }

    const size_t static_fanouts[] = {8, 16, 32, 64, 128, 256};
    for (size_t f = 0; f < sizeof(static_fanouts) / sizeof(static_fanouts[0]); ++f) {
        StaticRegionalBTreeAdapter index(record_count, partition_count, static_fanouts[f]);
        std::vector<PhaseResult> results =
            run_index(index, phases, operations_by_phase, record_count);
        for (size_t i = 0; i < results.size(); ++i) {
            all_results.push_back(results[i]);
            print_result(results[i]);
        }
    }

    const size_t oracle_hot_fanouts[] = {8, 16, 32};
    for (size_t f = 0; f < sizeof(oracle_hot_fanouts) / sizeof(oracle_hot_fanouts[0]); ++f) {
        OraclePhasedAdapter index(record_count, partition_count, oracle_hot_fanouts[f]);
        std::vector<PhaseResult> results =
            run_index(index, phases, operations_by_phase, record_count);
        for (size_t i = 0; i < results.size(); ++i) {
            all_results.push_back(results[i]);
            print_result(results[i]);
        }
    }

    {
        SegmentedAdapter index(record_count, partition_count, adaptation_interval, false);
        std::vector<PhaseResult> results =
            run_index(index, phases, operations_by_phase, record_count);
        for (size_t i = 0; i < results.size(); ++i) {
            all_results.push_back(results[i]);
            print_result(results[i]);
        }
    }
    {
        SegmentedAdapter index(record_count, partition_count, adaptation_interval, true);
        std::vector<PhaseResult> results =
            run_index(index, phases, operations_by_phase, record_count);
        for (size_t i = 0; i < results.size(); ++i) {
            all_results.push_back(results[i]);
            print_result(results[i]);
        }
    }

    write_csv(all_results, "results/shifting_summary.csv");

    std::cout << "\nChecksum: " << shifting_sink << std::endl;
    std::cout << "Benchmark complete.\n";

    return 0;
}
