#include "../include/btree/btree.h"
#include "../include/monitor/monitor.h"
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

using cache_adaptive::AccessMonitor;
using cache_adaptive::SegmentedAdaptiveBPlusTree;

volatile long long overhead_sink = 0;

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
        : type(op_type)
        , key(op_key)
        , value(op_value) {}
};

struct WorkloadSpec {
    std::string name;
    double read_ratio;
    bool zipfian;

    WorkloadSpec(const std::string& workload_name,
                 double workload_read_ratio,
                 bool use_zipfian)
        : name(workload_name)
        , read_ratio(workload_read_ratio)
        , zipfian(use_zipfian) {}
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

std::vector<Operation> generate_workload(const WorkloadSpec& spec,
                                         size_t record_count,
                                         size_t operation_count,
                                         double theta,
                                         unsigned seed) {
    std::vector<Operation> operations;
    operations.reserve(operation_count);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> probability(0.0, 1.0);
    std::uniform_int_distribution<int> uniform_key(
        0, static_cast<int>(record_count == 0 ? 0 : record_count - 1));
    ZipfSampler zipf(record_count, theta);

    for (size_t i = 0; i < operation_count; ++i) {
        int key = spec.zipfian ? zipf.next(rng) : uniform_key(rng);
        if (probability(rng) < spec.read_ratio) {
            operations.push_back(Operation(Operation::READ, key, 0));
        } else {
            operations.push_back(Operation(Operation::UPDATE,
                                           key,
                                           static_cast<int>(record_count + i)));
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
    virtual void insert_or_update(int key, int value) = 0;
    virtual bool read(int key, int& value) = 0;
    virtual size_t final_fanout() const = 0;
    virtual size_t adaptations() const = 0;
    virtual size_t restructures() const = 0;
    virtual size_t monitored_nodes() const = 0;
    virtual size_t skipped_hysteresis() const { return 0; }
    virtual size_t skipped_cost() const { return 0; }
    virtual size_t skipped_cooldown() const { return 0; }
};

class StaticBTreeAdapter : public IndexAdapter {
public:
    StaticBTreeAdapter()
        : tree_(64) {}

    std::string name() const {
        return "Static B+Tree";
    }

    void insert_or_update(int key, int value) {
        tree_.insert(key, value);
    }

    bool read(int key, int& value) {
        return tree_.search(key, value);
    }

    size_t final_fanout() const {
        return 64;
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

    size_t monitored_nodes() const {
        return 0;
    }

private:
    cabtree::BPlusTree<int, int> tree_;
};

class StaticRegionalBTreeAdapter : public IndexAdapter {
public:
    StaticRegionalBTreeAdapter(size_t record_count, size_t partition_count)
        : record_count_(record_count == 0 ? 1 : record_count)
        , partition_count_(partition_count == 0 ? 1 : partition_count)
        , partition_size_((record_count_ + partition_count_ - 1) / partition_count_)
        , trees_() {
        trees_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            trees_.push_back(new cabtree::BPlusTree<int, int>(64));
        }
    }

    ~StaticRegionalBTreeAdapter() {
        for (size_t i = 0; i < trees_.size(); ++i) {
            delete trees_[i];
        }
    }

    std::string name() const {
        return "Static Region";
    }

    void insert_or_update(int key, int value) {
        trees_[partition_for(key)]->insert(key, value);
    }

    bool read(int key, int& value) {
        return trees_[partition_for(key)]->search(key, value);
    }

    size_t final_fanout() const {
        return 64;
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

    size_t monitored_nodes() const {
        return 0;
    }

private:
    size_t partition_for(int key) const {
        if (key < 0) {
            return 0;
        }
        size_t partition = static_cast<size_t>(key) / partition_size_;
        if (partition >= partition_count_) {
            partition = partition_count_ - 1;
        }
        return partition;
    }

    size_t record_count_;
    size_t partition_count_;
    size_t partition_size_;
    std::vector<cabtree::BPlusTree<int, int>*> trees_;
};

class StaticRegionalWithMonitorAdapter : public IndexAdapter {
public:
    StaticRegionalWithMonitorAdapter(size_t record_count, size_t partition_count)
        : record_count_(record_count == 0 ? 1 : record_count)
        , partition_count_(partition_count == 0 ? 1 : partition_count)
        , partition_size_((record_count_ + partition_count_ - 1) / partition_count_)
        , trees_()
        , monitors_() {
        trees_.reserve(partition_count_);
        monitors_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            trees_.push_back(new cabtree::BPlusTree<int, int>(64));
            monitors_.push_back(new AccessMonitor(10000, 0.1, 32, 0.99));
        }
    }

    ~StaticRegionalWithMonitorAdapter() {
        for (size_t i = 0; i < trees_.size(); ++i) {
            delete trees_[i];
            delete monitors_[i];
        }
    }

    std::string name() const {
        return "Static Region + Monitor";
    }

    void insert_or_update(int key, int value) {
        size_t segment = partition_for(key);
        monitors_[segment]->record_access(key);
        trees_[segment]->insert(key, value);
    }

    bool read(int key, int& value) {
        size_t segment = partition_for(key);
        monitors_[segment]->record_access(key);
        return trees_[segment]->search(key, value);
    }

    size_t final_fanout() const {
        return 64;
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

    size_t monitored_nodes() const {
        size_t total = 0;
        for (size_t i = 0; i < monitors_.size(); ++i) {
            total += monitors_[i]->get_unique_node_count();
        }
        return total;
    }

private:
    size_t partition_for(int key) const {
        if (key < 0) {
            return 0;
        }
        size_t partition = static_cast<size_t>(key) / partition_size_;
        if (partition >= partition_count_) {
            partition = partition_count_ - 1;
        }
        return partition;
    }

    size_t record_count_;
    size_t partition_count_;
    size_t partition_size_;
    std::vector<cabtree::BPlusTree<int, int>*> trees_;
    std::vector<AccessMonitor*> monitors_;
};

class SegmentedCostAblationAdapter : public IndexAdapter {
public:
    enum Mode {
        NO_RECORDS,
        RECORDS_ONLY,
        MONITOR_RECORDS
    };

    SegmentedCostAblationAdapter(size_t record_count,
                                 size_t partition_count,
                                 Mode mode)
        : record_count_(record_count == 0 ? 1 : record_count)
        , partition_count_(partition_count == 0 ? 1 : partition_count)
        , partition_size_((record_count_ + partition_count_ - 1) / partition_count_)
        , mode_(mode)
        , sampling_rate_(32)
        , segments_() {
        segments_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            Segment* segment = new Segment(mode_);
            if (segment->records) {
                segment->records->reserve(partition_size_);
            }
            segments_.push_back(segment);
        }
    }

    ~SegmentedCostAblationAdapter() {
        for (size_t i = 0; i < segments_.size(); ++i) {
            delete segments_[i];
        }
    }

    std::string name() const {
        if (mode_ == NO_RECORDS) {
            return "Segmented No Records";
        }
        if (mode_ == RECORDS_ONLY) {
            return "Segmented Records Only";
        }
        return "Segmented Monitor + Records";
    }

    void insert_or_update(int key, int value) {
        Segment& segment = *segments_[partition_for(key)];
        record_monitor_sample(segment, key);
        if (segment.records) {
            (*segment.records)[key] = value;
        }
        segment.tree->insert(key, value);
    }

    bool read(int key, int& value) {
        Segment& segment = *segments_[partition_for(key)];
        record_monitor_sample(segment, key);
        return segment.tree->search(key, value);
    }

    size_t final_fanout() const {
        return 64;
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

    size_t monitored_nodes() const {
        size_t total = 0;
        for (size_t i = 0; i < segments_.size(); ++i) {
            if (segments_[i]->monitor) {
                total += segments_[i]->monitor->get_unique_node_count();
            }
        }
        return total;
    }

private:
    struct Segment {
        explicit Segment(Mode mode)
            : tree(new cabtree::BPlusTree<int, int>(64))
            , monitor(NULL)
            , records(NULL)
            , access_count(0) {
            if (mode == MONITOR_RECORDS) {
                monitor = new AccessMonitor(10000, 0.1, 1, 0.99);
            }
            if (mode == RECORDS_ONLY || mode == MONITOR_RECORDS) {
                records = new std::unordered_map<int, int>();
            }
        }

        ~Segment() {
            delete tree;
            delete monitor;
            delete records;
        }

        cabtree::BPlusTree<int, int>* tree;
        AccessMonitor* monitor;
        std::unordered_map<int, int>* records;
        size_t access_count;
    };

    size_t partition_for(int key) const {
        if (key < 0) {
            return 0;
        }
        size_t partition = static_cast<size_t>(key) / partition_size_;
        if (partition >= partition_count_) {
            partition = partition_count_ - 1;
        }
        return partition;
    }

    void record_monitor_sample(Segment& segment, int key) {
        if (!segment.monitor) {
            return;
        }
        ++segment.access_count;
        if ((segment.access_count % sampling_rate_) == 0) {
            segment.monitor->record_access(key);
        }
    }

    size_t record_count_;
    size_t partition_count_;
    size_t partition_size_;
    Mode mode_;
    size_t sampling_rate_;
    std::vector<Segment*> segments_;
};

class SegmentedAdaptiveAblationAdapter : public IndexAdapter {
public:
    SegmentedAdaptiveAblationAdapter(size_t record_count,
                                     size_t partition_count,
                                     size_t adaptation_interval,
                                     bool enable_adaptation)
        : tree_(partition_count,
                0,
                static_cast<int>(record_count > 0 ? record_count - 1 : 0),
                64,
                adaptation_interval,
                8,
                256,
                32)
        , enable_adaptation_(enable_adaptation) {}

    std::string name() const {
        return enable_adaptation_
            ? "Segmented Full Adaptive"
            : "Segmented Monitor + Records";
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

    size_t monitored_nodes() const {
        size_t total = 0;
        for (size_t i = 0; i < tree_.segment_count(); ++i) {
            total += tree_.get_segment_stats(i).unique_nodes;
        }
        return total;
    }

    size_t skipped_hysteresis() const {
        return tree_.get_stats().rebuilds_skipped_hysteresis;
    }

    size_t skipped_cost() const {
        return tree_.get_stats().rebuilds_skipped_cost;
    }

    size_t skipped_cooldown() const {
        return tree_.get_stats().rebuilds_skipped_cooldown;
    }

private:
    SegmentedAdaptiveBPlusTree<int, int> tree_;
    bool enable_adaptation_;
};

struct BenchmarkResult {
    std::string workload;
    std::string index_name;
    size_t operations;
    double run_ms;
    double throughput_ops_sec;
    double p95_sample_us;
    size_t final_fanout;
    size_t adaptations;
    size_t restructures;
    size_t monitored_nodes;
    size_t skipped_hysteresis;
    size_t skipped_cost;
    size_t skipped_cooldown;

    BenchmarkResult()
        : operations(0)
        , run_ms(0.0)
        , throughput_ops_sec(0.0)
        , p95_sample_us(0.0)
        , final_fanout(0)
        , adaptations(0)
        , restructures(0)
        , monitored_nodes(0)
        , skipped_hysteresis(0)
        , skipped_cost(0)
        , skipped_cooldown(0) {}
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

BenchmarkResult run_once(IndexAdapter& index,
                         const WorkloadSpec& workload,
                         const std::vector<Operation>& operations,
                         size_t record_count) {
    BenchmarkResult result;
    result.workload = workload.name;
    result.index_name = index.name();
    result.operations = operations.size();

    index.begin_load();
    for (size_t i = 0; i < record_count; ++i) {
        index.insert_or_update(static_cast<int>(i), static_cast<int>(i * 10));
    }
    index.end_load();

    std::vector<double> sampled_latencies;
    sampled_latencies.reserve(operations.size() / 128 + 1);

    int value = 0;
    Timer run_timer;
    for (size_t i = 0; i < operations.size(); ++i) {
        const bool sample_latency = ((i & 127u) == 0u);
        Timer op_timer;
        const Operation& op = operations[i];
        if (op.type == Operation::READ) {
            if (index.read(op.key, value)) {
                overhead_sink += value;
            }
        } else {
            index.insert_or_update(op.key, op.value);
            overhead_sink += op.value;
        }
        if (sample_latency) {
            sampled_latencies.push_back(op_timer.elapsed_us());
        }
    }
    result.run_ms = run_timer.elapsed_ms();

    std::sort(sampled_latencies.begin(), sampled_latencies.end());
    result.p95_sample_us = percentile(sampled_latencies, 0.95);
    result.throughput_ops_sec = result.run_ms > 0.0
        ? static_cast<double>(result.operations) / result.run_ms * 1000.0
        : 0.0;
    result.final_fanout = index.final_fanout();
    result.adaptations = index.adaptations();
    result.restructures = index.restructures();
    result.monitored_nodes = index.monitored_nodes();
    result.skipped_hysteresis = index.skipped_hysteresis();
    result.skipped_cost = index.skipped_cost();
    result.skipped_cooldown = index.skipped_cooldown();

    return result;
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
              << std::setw(18) << "Workload"
              << std::setw(30) << "Index"
              << std::right
              << std::setw(12) << "Ops/sec"
              << std::setw(11) << "vs static"
              << std::setw(11) << "vs region"
              << std::setw(12) << "p95us"
              << std::setw(9) << "fanout"
              << std::setw(8) << "adapt"
              << std::setw(8) << "rebuild"
              << std::setw(9) << "nodes"
              << std::endl;
    std::cout << std::string(128, '-') << std::endl;
}

void print_result(const BenchmarkResult& result,
                  double static_throughput,
                  double regional_throughput) {
    double ratio = static_throughput > 0.0
        ? result.throughput_ops_sec / static_throughput
        : 0.0;
    double regional_ratio = regional_throughput > 0.0
        ? result.throughput_ops_sec / regional_throughput
        : 0.0;
    std::cout << std::left
              << std::setw(18) << result.workload
              << std::setw(30) << result.index_name
              << std::right
              << std::setw(12) << std::fixed << std::setprecision(0)
              << result.throughput_ops_sec
              << std::setw(11) << std::fixed << std::setprecision(2)
              << ratio
              << std::setw(11) << std::fixed << std::setprecision(2)
              << regional_ratio
              << std::setw(12) << std::fixed << std::setprecision(3)
              << result.p95_sample_us
              << std::setw(9) << result.final_fanout
              << std::setw(8) << result.adaptations
              << std::setw(8) << result.restructures
              << std::setw(9) << result.monitored_nodes
              << std::endl;
}

void write_csv(const std::vector<BenchmarkResult>& results,
               const std::string& path) {
    std::ofstream out(path.c_str());
    if (!out) {
        std::cout << "WARNING: could not write " << path << std::endl;
        return;
    }
    out << "workload,index,operations,run_ms,throughput_ops_sec,p95_sample_us,"
        << "final_fanout,adaptations,restructures,monitored_nodes,"
        << "skipped_hysteresis,skipped_cost,skipped_cooldown\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const BenchmarkResult& r = results[i];
        out << r.workload << ','
            << r.index_name << ','
            << r.operations << ','
            << r.run_ms << ','
            << r.throughput_ops_sec << ','
            << r.p95_sample_us << ','
            << r.final_fanout << ','
            << r.adaptations << ','
            << r.restructures << ','
            << r.monitored_nodes << ','
            << r.skipped_hysteresis << ','
            << r.skipped_cost << ','
            << r.skipped_cooldown << '\n';
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
    size_t operation_count = parse_size_arg(argc, argv, 2, 50000);
    double theta = parse_double_arg(argc, argv, 3, 0.99);
    size_t adaptation_interval = parse_size_arg(argc, argv, 4, 5000);

    std::cout << "\n============================================================\n";
    std::cout << "  Adaptive Monitor Overhead Benchmark\n";
    std::cout << "============================================================\n";
    std::cout << "Records: " << record_count
              << ", Operations/workload: " << operation_count
              << ", Zipf theta: " << theta
              << ", Adapt interval: " << adaptation_interval << std::endl;
    std::cout << "Usage: bench_overhead.exe [records] [operations] [theta] [adapt_interval]\n";
    std::cout << "CSV output: results/overhead_summary.csv\n\n";

    std::srand(12345);
    ensure_results_dir();

    std::vector<WorkloadSpec> workloads;
    workloads.push_back(WorkloadSpec("Uniform-B", 0.95, false));
    workloads.push_back(WorkloadSpec("Zipf-B", 0.95, true));
    workloads.push_back(WorkloadSpec("Zipf-A", 0.50, true));

    std::vector<BenchmarkResult> all_results;
    print_header();

    for (size_t w = 0; w < workloads.size(); ++w) {
        std::vector<Operation> operations =
            generate_workload(workloads[w],
                              record_count,
                              operation_count,
                              theta,
                              static_cast<unsigned>(6789 + w));

        StaticBTreeAdapter static_index;
        BenchmarkResult static_result =
            run_once(static_index, workloads[w], operations, record_count);
        all_results.push_back(static_result);
        double static_throughput = static_result.throughput_ops_sec;
        print_result(static_result, static_throughput, 0.0);

        StaticRegionalBTreeAdapter regional_index(record_count, 8);
        BenchmarkResult regional_result =
            run_once(regional_index, workloads[w], operations, record_count);
        all_results.push_back(regional_result);
        double regional_throughput = regional_result.throughput_ops_sec;
        print_result(regional_result, static_throughput, regional_throughput);

        StaticRegionalWithMonitorAdapter regional_monitor_index(record_count, 8);
        BenchmarkResult regional_monitor_result =
            run_once(regional_monitor_index, workloads[w], operations, record_count);
        all_results.push_back(regional_monitor_result);
        print_result(regional_monitor_result, static_throughput, regional_throughput);

        SegmentedCostAblationAdapter segmented_no_records(
            record_count, 8, SegmentedCostAblationAdapter::NO_RECORDS);
        BenchmarkResult segmented_no_records_result =
            run_once(segmented_no_records, workloads[w], operations, record_count);
        all_results.push_back(segmented_no_records_result);
        print_result(segmented_no_records_result, static_throughput, regional_throughput);

        SegmentedCostAblationAdapter segmented_records_only(
            record_count, 8, SegmentedCostAblationAdapter::RECORDS_ONLY);
        BenchmarkResult segmented_records_only_result =
            run_once(segmented_records_only, workloads[w], operations, record_count);
        all_results.push_back(segmented_records_only_result);
        print_result(segmented_records_only_result, static_throughput, regional_throughput);

        SegmentedCostAblationAdapter segmented_monitor_records(
            record_count, 8, SegmentedCostAblationAdapter::MONITOR_RECORDS);
        BenchmarkResult segmented_monitor_records_result =
            run_once(segmented_monitor_records, workloads[w], operations, record_count);
        all_results.push_back(segmented_monitor_records_result);
        print_result(segmented_monitor_records_result, static_throughput, regional_throughput);

        SegmentedAdaptiveAblationAdapter segmented_full(
            record_count, 8, adaptation_interval, true);
        BenchmarkResult segmented_full_result =
            run_once(segmented_full, workloads[w], operations, record_count);
        all_results.push_back(segmented_full_result);
        print_result(segmented_full_result, static_throughput, regional_throughput);
    }

    write_csv(all_results, "results/overhead_summary.csv");

    std::cout << "\nChecksum: " << overhead_sink << std::endl;
    std::cout << "Benchmark complete.\n";
    return 0;
}
