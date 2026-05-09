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

volatile long long dynamic_sink = 0;

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

void ensure_results_dir() {
#ifdef _WIN32
    _mkdir("results");
#else
    mkdir("results", 0755);
#endif
}

size_t segment_for_key(int key, size_t record_count, size_t segment_count) {
    const size_t safe_records = record_count == 0 ? 1 : record_count;
    const size_t safe_segments = segment_count == 0 ? 1 : segment_count;
    size_t segment_size = (safe_records + safe_segments - 1) / safe_segments;
    if (segment_size == 0) {
        segment_size = 1;
    }
    if (key < 0) {
        return 0;
    }
    size_t segment = static_cast<size_t>(key) / segment_size;
    if (segment >= safe_segments) {
        segment = safe_segments - 1;
    }
    return segment;
}

std::vector<Operation> generate_window_workload(size_t record_count,
                                                size_t operation_count,
                                                size_t phase_index,
                                                double theta,
                                                double read_ratio,
                                                double hot_access_ratio,
                                                double hot_fraction,
                                                unsigned seed,
                                                int& hot_start,
                                                int& hot_end) {
    const size_t safe_records = record_count == 0 ? 1 : record_count;
    size_t hot_span = static_cast<size_t>(
        std::max(1.0, static_cast<double>(safe_records) * hot_fraction));
    if (hot_span > safe_records) {
        hot_span = safe_records;
    }

    const size_t phase_count = (safe_records + hot_span - 1) / hot_span;
    const size_t phase_slot = phase_count == 0 ? 0 : (phase_index % phase_count);
    size_t start = phase_slot * hot_span;
    if (start >= safe_records) {
        start = 0;
    }
    size_t end = std::min(safe_records - 1, start + hot_span - 1);

    hot_start = static_cast<int>(start);
    hot_end = static_cast<int>(end);

    std::vector<Operation> operations;
    operations.reserve(operation_count);

    std::mt19937 rng(seed + static_cast<unsigned>(phase_index * 9973U));
    std::uniform_real_distribution<double> probability(0.0, 1.0);
    std::uniform_int_distribution<int> uniform_key(
        0, static_cast<int>(safe_records - 1));
    ZipfSampler hot_zipf(hot_span, theta);

    for (size_t i = 0; i < operation_count; ++i) {
        int key = 0;
        if (probability(rng) < hot_access_ratio) {
            int offset = hot_zipf.next(rng);
            if (offset >= static_cast<int>(hot_span)) {
                offset = static_cast<int>(hot_span - 1);
            }
            key = hot_start + offset;
        } else {
            key = uniform_key(rng);
        }

        const bool is_read = probability(rng) < read_ratio;
        if (is_read) {
            operations.push_back(Operation(Operation::READ, key, 0));
        } else {
            operations.push_back(Operation(Operation::UPDATE,
                                           key,
                                           static_cast<int>(safe_records + phase_index * operation_count + i)));
        }
    }

    return operations;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double raw = p * static_cast<double>(values.size() - 1);
    size_t index = static_cast<size_t>(raw + 0.5);
    if (index >= values.size()) {
        index = values.size() - 1;
    }
    return values[index];
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
    virtual size_t cost_skips() const = 0;
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

    size_t cost_skips() const {
        return 0;
    }

private:
    cabtree::BPlusTree<int, int> tree_;
    int fanout_;
};

class StaticRegionalAdapter : public IndexAdapter {
public:
    StaticRegionalAdapter(size_t record_count, size_t segment_count)
        : record_count_(record_count == 0 ? 1 : record_count)
        , segment_count_(segment_count == 0 ? 1 : segment_count)
        , trees_() {
        trees_.reserve(segment_count_);
        for (size_t i = 0; i < segment_count_; ++i) {
            trees_.push_back(new cabtree::BPlusTree<int, int>(64));
        }
    }

    ~StaticRegionalAdapter() {
        for (size_t i = 0; i < trees_.size(); ++i) {
            delete trees_[i];
        }
    }

    std::string name() const {
        return "Static Region x8";
    }

    void insert_or_update(int key, int value) {
        trees_[segment_for_key(key, record_count_, segment_count_)]->insert(key, value);
    }

    bool read(int key, int& value) {
        return trees_[segment_for_key(key, record_count_, segment_count_)]->search(key, value);
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

    size_t cost_skips() const {
        return 0;
    }

private:
    size_t record_count_;
    size_t segment_count_;
    std::vector<cabtree::BPlusTree<int, int>*> trees_;
};

class SegmentedAdaptiveAdapter : public IndexAdapter {
public:
    SegmentedAdaptiveAdapter(size_t record_count,
                             size_t segment_count,
                             size_t adaptation_interval)
        : tree_(segment_count,
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
        tree_.reset_adaptation_state();
        tree_.set_adaptation_enabled(true);
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

    size_t cost_skips() const {
        return tree_.get_stats().rebuilds_skipped_cost;
    }

private:
    SegmentedAdaptiveBPlusTree<int, int> tree_;
};

struct WindowResult {
    std::string index_name;
    size_t phase;
    int hot_start;
    int hot_end;
    size_t operations;
    size_t reads;
    size_t updates;
    double elapsed_ms;
    double throughput;
    double p95_us;
    double p99_us;
    size_t fanout;
    size_t adaptations;
    size_t restructures;
    size_t cost_skips;
};

WindowResult run_window(IndexAdapter& index,
                        const std::vector<Operation>& operations,
                        size_t phase,
                        int hot_start,
                        int hot_end) {
    WindowResult result;
    result.index_name = index.name();
    result.phase = phase;
    result.hot_start = hot_start;
    result.hot_end = hot_end;
    result.operations = operations.size();
    result.reads = 0;
    result.updates = 0;
    result.elapsed_ms = 0.0;
    result.throughput = 0.0;
    result.p95_us = 0.0;
    result.p99_us = 0.0;
    result.fanout = 0;
    result.adaptations = 0;
    result.restructures = 0;
    result.cost_skips = 0;

    std::vector<double> latency_samples;
    latency_samples.reserve(operations.size() / 1024 + 1);

    Timer run_timer;
    for (size_t i = 0; i < operations.size(); ++i) {
        const Operation& op = operations[i];
        const bool sample_latency = (i % 1024) == 0;
        Timer op_timer;

        if (op.type == Operation::UPDATE) {
            index.insert_or_update(op.key, op.value);
            ++result.updates;
        } else {
            int value = 0;
            if (index.read(op.key, value)) {
                dynamic_sink += value;
            }
            ++result.reads;
        }

        if (sample_latency) {
            latency_samples.push_back(op_timer.elapsed_us());
        }
    }

    result.elapsed_ms = run_timer.elapsed_ms();
    result.throughput =
        result.elapsed_ms > 0.0
            ? (static_cast<double>(operations.size()) * 1000.0) / result.elapsed_ms
            : 0.0;
    result.p95_us = percentile(latency_samples, 0.95);
    result.p99_us = percentile(latency_samples, 0.99);
    result.fanout = index.final_fanout();
    result.adaptations = index.adaptations();
    result.restructures = index.restructures();
    result.cost_skips = index.cost_skips();
    return result;
}

void load_index(IndexAdapter& index, size_t record_count) {
    index.begin_load();
    for (size_t i = 0; i < record_count; ++i) {
        index.insert_or_update(static_cast<int>(i), static_cast<int>(i));
    }
    index.end_load();
}

int main(int argc, char** argv) {
    size_t record_count = 1000000;
    size_t total_operations = 5000000;
    size_t window_operations = 1000000;
    double theta = 0.99;
    size_t adaptation_interval = 5000;

    if (argc > 1) {
        record_count = static_cast<size_t>(std::strtoull(argv[1], NULL, 10));
    }
    if (argc > 2) {
        total_operations = static_cast<size_t>(std::strtoull(argv[2], NULL, 10));
    }
    if (argc > 3) {
        window_operations = static_cast<size_t>(std::strtoull(argv[3], NULL, 10));
    }
    if (argc > 4) {
        theta = std::atof(argv[4]);
    }
    if (argc > 5) {
        adaptation_interval = static_cast<size_t>(std::strtoull(argv[5], NULL, 10));
    }
    if (window_operations == 0) {
        window_operations = 1;
    }

    const size_t segment_count = 8;
    const double read_ratio = 0.95;
    const double hot_access_ratio = 0.80;
    const double hot_fraction = 0.20;
    const size_t phase_count =
        (total_operations + window_operations - 1) / window_operations;

    ensure_results_dir();

    std::cout << "\n============================================================\n";
    std::cout << "  Sliding Hotspot Dynamic Benchmark\n";
    std::cout << "============================================================\n";
    std::cout << "Records: " << record_count
              << ", Total operations: " << total_operations
              << ", Window operations: " << window_operations
              << ", Zipf theta: " << theta
              << ", Adapt interval: " << adaptation_interval << "\n";
    std::cout << "Usage: bench_dynamic.exe [records] [total_ops] [window_ops] [theta] [adapt_interval]\n";
    std::cout << "CSV output: results/dynamic_hotspot_summary.csv\n\n";

    std::vector<IndexAdapter*> indexes;
    indexes.push_back(new StaticBTreeAdapter(64));
    indexes.push_back(new StaticRegionalAdapter(record_count, segment_count));
    indexes.push_back(new SegmentedAdaptiveAdapter(record_count,
                                                   segment_count,
                                                   adaptation_interval));

    for (size_t i = 0; i < indexes.size(); ++i) {
        Timer load_timer;
        load_index(*indexes[i], record_count);
        std::cout << "Loaded " << indexes[i]->name()
                  << " in " << std::fixed << std::setprecision(2)
                  << load_timer.elapsed_ms() << " ms\n";
    }

    std::ofstream csv("results/dynamic_hotspot_summary.csv");
    csv << "index,phase,hot_start,hot_end,operations,reads,updates,elapsed_ms,ops_sec,"
        << "p95_us,p99_us,fanout,adaptations,rebuilds,cost_skips\n";

    std::cout << "\n"
              << std::left << std::setw(24) << "Index"
              << std::right << std::setw(8) << "phase"
              << std::setw(14) << "hot_start"
              << std::setw(14) << "ops/sec"
              << std::setw(10) << "p95us"
              << std::setw(8) << "fanout"
              << std::setw(8) << "adapt"
              << std::setw(8) << "rebuild"
              << "\n";
    std::cout << std::string(94, '-') << "\n";

    for (size_t phase = 0; phase < phase_count; ++phase) {
        const size_t remaining = total_operations - phase * window_operations;
        const size_t ops_this_phase = std::min(window_operations, remaining);
        int hot_start = 0;
        int hot_end = 0;
        std::vector<Operation> operations =
            generate_window_workload(record_count,
                                     ops_this_phase,
                                     phase,
                                     theta,
                                     read_ratio,
                                     hot_access_ratio,
                                     hot_fraction,
                                     20260509U,
                                     hot_start,
                                     hot_end);

        for (size_t i = 0; i < indexes.size(); ++i) {
            WindowResult result = run_window(*indexes[i],
                                             operations,
                                             phase,
                                             hot_start,
                                             hot_end);

            std::cout << std::left << std::setw(24) << result.index_name
                      << std::right << std::setw(8) << result.phase
                      << std::setw(14) << result.hot_start
                      << std::setw(14) << std::fixed << std::setprecision(0)
                      << result.throughput
                      << std::setw(10) << std::setprecision(3) << result.p95_us
                      << std::setw(8) << result.fanout
                      << std::setw(8) << result.adaptations
                      << std::setw(8) << result.restructures
                      << "\n";

            csv << result.index_name << ","
                << result.phase << ","
                << result.hot_start << ","
                << result.hot_end << ","
                << result.operations << ","
                << result.reads << ","
                << result.updates << ","
                << std::fixed << std::setprecision(6) << result.elapsed_ms << ","
                << std::fixed << std::setprecision(2) << result.throughput << ","
                << std::fixed << std::setprecision(6) << result.p95_us << ","
                << std::fixed << std::setprecision(6) << result.p99_us << ","
                << result.fanout << ","
                << result.adaptations << ","
                << result.restructures << ","
                << result.cost_skips << "\n";
        }
    }

    for (size_t i = 0; i < indexes.size(); ++i) {
        delete indexes[i];
    }

    std::cout << "\nChecksum: " << dynamic_sink << "\n";
    std::cout << "Benchmark complete.\n";
    return 0;
}
