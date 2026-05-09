#include "../include/btree/btree.h"
#include "../include/adaptive/adaptive_btree.h"
#include "../include/adaptive/adaptive_btree_regression.h"
#include "../include/adaptive/adaptive_btree_aco.h"
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
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <chrono>
#endif

using cache_adaptive::AdaptiveBPlusTree;
using cache_adaptive::AdaptiveBPlusTreeACO;
using cache_adaptive::AdaptiveBPlusTreeRegression;
using cache_adaptive::SegmentedAdaptiveBPlusTree;

volatile long long ycsb_sink = 0;

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

struct WorkloadSpec {
    std::string name;
    std::string description;
    double read_ratio;

    WorkloadSpec(const std::string& workload_name,
                 const std::string& workload_description,
                 double workload_read_ratio)
        : name(workload_name)
        , description(workload_description)
        , read_ratio(workload_read_ratio) {}
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
    ZipfSampler sampler(record_count, theta);

    for (size_t i = 0; i < operation_count; ++i) {
        int key = sampler.next(rng);
        bool is_read = probability(rng) < spec.read_ratio;
        if (is_read) {
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
};

class StaticBTreeAdapter : public IndexAdapter {
public:
    explicit StaticBTreeAdapter(int fanout)
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
        return static_cast<size_t>(fanout_);
    }

    size_t adaptations() const {
        return 0;
    }

    size_t restructures() const {
        return 0;
    }

private:
    cabtree::BPlusTree<int, int> tree_;
    int fanout_;
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
        return "Static Region x8";
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

class AdaptiveRLAdapter : public IndexAdapter {
public:
    explicit AdaptiveRLAdapter(size_t adaptation_interval)
        : tree_(64, adaptation_interval, 8, 256) {}

    std::string name() const {
        return "Adaptive RL";
    }

    void insert_or_update(int key, int value) {
        tree_.insert(key, value);
    }

    void begin_load() {
        tree_.set_adaptation_enabled(false);
    }

    void end_load() {
        tree_.set_adaptation_enabled(true);
        tree_.reset_adaptation_state();
    }

    bool read(int key, int& value) {
        return tree_.search(key, value);
    }

    size_t final_fanout() const {
        return tree_.get_current_fanout();
    }

    size_t adaptations() const {
        return tree_.get_stats().total_adaptations;
    }

    size_t restructures() const {
        return tree_.get_stats().restructure_count;
    }

private:
    AdaptiveBPlusTree<int, int> tree_;
};

class AdaptiveRegressionAdapter : public IndexAdapter {
public:
    explicit AdaptiveRegressionAdapter(size_t adaptation_interval)
        : tree_(64, adaptation_interval, 8, 256) {}

    std::string name() const {
        return "Adaptive Regression";
    }

    void insert_or_update(int key, int value) {
        tree_.insert(key, value);
    }

    void begin_load() {
        tree_.set_adaptation_enabled(false);
    }

    void end_load() {
        tree_.set_adaptation_enabled(true);
        tree_.reset_adaptation_state();
    }

    bool read(int key, int& value) {
        return tree_.search(key, value);
    }

    size_t final_fanout() const {
        return tree_.get_current_fanout();
    }

    size_t adaptations() const {
        return tree_.get_stats().total_adaptations;
    }

    size_t restructures() const {
        return tree_.get_stats().restructure_count;
    }

private:
    AdaptiveBPlusTreeRegression<int, int> tree_;
};

class SegmentedAdaptiveAdapter : public IndexAdapter {
public:
    SegmentedAdaptiveAdapter(size_t record_count,
                             size_t partition_count,
                             size_t adaptation_interval)
        : tree_(partition_count,
                0,
                static_cast<int>(record_count > 0 ? record_count - 1 : 0),
                64,
                adaptation_interval,
                8,
                256,
                32) {}

    std::string name() const {
        return "Segmented Adaptive";
    }

    void begin_load() {
        tree_.set_adaptation_enabled(false);
    }

    void end_load() {
        tree_.set_adaptation_enabled(true);
        tree_.reset_adaptation_state();
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
};

class AdaptiveACOAdapter : public IndexAdapter {
public:
    explicit AdaptiveACOAdapter(size_t adaptation_interval)
        : tree_(64, adaptation_interval, 8, 256) {}

    std::string name() const {
        return "Adaptive ACO";
    }

    void insert_or_update(int key, int value) {
        tree_.insert(key, value);
    }

    void begin_load() {
        tree_.set_adaptation_enabled(false);
    }

    void end_load() {
        tree_.set_adaptation_enabled(true);
        tree_.reset_adaptation_state();
    }

    bool read(int key, int& value) {
        return tree_.search(key, value);
    }

    size_t final_fanout() const {
        return tree_.get_current_fanout();
    }

    size_t adaptations() const {
        return tree_.get_stats().total_adaptations;
    }

    size_t restructures() const {
        return tree_.get_stats().restructure_count;
    }

private:
    AdaptiveBPlusTreeACO<int, int> tree_;
};

struct BenchmarkResult {
    std::string workload;
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

BenchmarkResult run_once(IndexAdapter& index,
                         const WorkloadSpec& workload,
                         const std::vector<Operation>& operations,
                         size_t record_count) {
    BenchmarkResult result;
    result.workload = workload.name;
    result.index_name = index.name();
    result.operations = operations.size();

    index.begin_load();
    Timer load_timer;
    for (size_t i = 0; i < record_count; ++i) {
        index.insert_or_update(static_cast<int>(i), static_cast<int>(i * 10));
    }
    result.load_ms = load_timer.elapsed_ms();
    index.end_load();

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
                ycsb_sink += value;
            } else {
                ++result.misses;
            }
        } else {
            ++result.updates;
            index.insert_or_update(op.key, op.value);
            ycsb_sink += op.value;
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
        ? (static_cast<double>(result.operations) / result.run_ms * 1000.0)
        : 0.0;
    result.final_fanout = index.final_fanout();
    result.adaptations = index.adaptations();
    result.restructures = index.restructures();

    return result;
}

void print_result_header() {
    std::cout << std::left
              << std::setw(11) << "Workload"
              << std::setw(22) << "Index"
              << std::right
              << std::setw(12) << "Throughput"
              << std::setw(10) << "p50us"
              << std::setw(10) << "p95us"
              << std::setw(10) << "p99us"
              << std::setw(10) << "maxus"
              << std::setw(10) << "loadms"
              << std::setw(9) << "fanout"
              << std::setw(8) << "adapt"
              << std::setw(8) << "rebuild"
              << std::endl;
    std::cout << std::string(120, '-') << std::endl;
}

void print_result(const BenchmarkResult& r) {
    std::cout << std::left
              << std::setw(11) << r.workload
              << std::setw(22) << r.index_name
              << std::right
              << std::setw(12) << std::fixed << std::setprecision(0)
              << r.throughput_ops_sec
              << std::setw(10) << std::fixed << std::setprecision(3)
              << r.p50_us
              << std::setw(10) << r.p95_us
              << std::setw(10) << r.p99_us
              << std::setw(10) << r.max_us
              << std::setw(10) << std::fixed << std::setprecision(2)
              << r.load_ms
              << std::setw(9) << r.final_fanout
              << std::setw(8) << r.adaptations
              << std::setw(8) << r.restructures
              << std::endl;
}

void write_csv(const std::vector<BenchmarkResult>& results,
               const std::string& path) {
    std::ofstream out(path.c_str());
    if (!out) {
        std::cout << "WARNING: could not write " << path << std::endl;
        return;
    }

    out << "workload,index,operations,reads,updates,misses,load_ms,run_ms,"
        << "throughput_ops_sec,p50_us,p95_us,p99_us,max_us,final_fanout,"
        << "adaptations,restructures\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const BenchmarkResult& r = results[i];
        out << r.workload << ','
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
    std::cout << "  YCSB-Style Cache-Adaptive B-Tree Benchmark\n";
    std::cout << "============================================================\n";
    std::cout << "Records: " << record_count
              << ", Operations/workload: " << operation_count
              << ", Zipf theta: " << theta
              << ", Adapt interval: " << adaptation_interval << std::endl;
    std::cout << "Usage: bench_ycsb.exe [records] [operations] [theta] [adapt_interval]\n";
    std::cout << "CSV output: results/ycsb_summary.csv\n\n";

    std::srand(12345);

    std::vector<WorkloadSpec> workloads;
    workloads.push_back(WorkloadSpec("YCSB-A", "50% reads / 50% updates", 0.50));
    workloads.push_back(WorkloadSpec("YCSB-B", "95% reads / 5% updates", 0.95));
    workloads.push_back(WorkloadSpec("YCSB-C", "100% reads", 1.00));

    std::vector<BenchmarkResult> all_results;
    print_result_header();

    for (size_t w = 0; w < workloads.size(); ++w) {
        const WorkloadSpec& workload = workloads[w];
        std::vector<Operation> operations =
            generate_workload(workload,
                              record_count,
                              operation_count,
                              theta,
                              static_cast<unsigned>(12345 + w));

        {
            StaticBTreeAdapter index(64);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
        {
            StaticBTreeAdapter index(256);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
        {
            StaticRegionalBTreeAdapter index(record_count, 8);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
        {
            AdaptiveRLAdapter index(adaptation_interval);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
        {
            AdaptiveRegressionAdapter index(adaptation_interval);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
        {
            SegmentedAdaptiveAdapter index(record_count, 8, adaptation_interval);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
        {
            AdaptiveACOAdapter index(adaptation_interval);
            BenchmarkResult r = run_once(index, workload, operations, record_count);
            all_results.push_back(r);
            print_result(r);
        }
    }

    write_csv(all_results, "results/ycsb_summary.csv");

    std::cout << "\nChecksum: " << ycsb_sink << std::endl;
    std::cout << "Benchmark complete.\n";

    return 0;
}
