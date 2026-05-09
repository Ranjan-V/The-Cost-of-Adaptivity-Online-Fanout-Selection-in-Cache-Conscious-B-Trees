#include "../include/btree/btree.h"
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <iomanip>
#include <string>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace cabtree;

volatile long long benchmark_sink = 0;

class Timer {
public:
    Timer() {
        reset();
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
    
    void reset() {
#ifdef _WIN32
        QueryPerformanceCounter(&start_);
#else
        start_ = std::chrono::steady_clock::now();
#endif
    }
    
private:
#ifdef _WIN32
    LARGE_INTEGER start_;
#else
    std::chrono::steady_clock::time_point start_;
#endif
};

struct BenchmarkResult {
    std::string name;
    double insert_time_ms;
    double search_time_ms;
    double range_time_ms;
    double delete_time_ms;
    size_t insert_operations;
    size_t search_operations;
    size_t range_operations;
    size_t delete_operations;
    int tree_height;

    BenchmarkResult()
        : insert_time_ms(0.0)
        , search_time_ms(0.0)
        , range_time_ms(0.0)
        , delete_time_ms(0.0)
        , insert_operations(0)
        , search_operations(0)
        , range_operations(0)
        , delete_operations(0)
        , tree_height(0) {}
};

void print_header() {
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "B+ Tree Baseline Benchmark" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

void print_timed_operation(const std::string& label, double time_ms, size_t operations) {
    std::cout << std::left << std::setw(16) << label;
    if (operations == 0) {
        std::cout << "N/A" << std::endl;
        return;
    }

    double ops_per_sec = (time_ms > 0.0)
        ? (static_cast<double>(operations) / time_ms * 1000.0)
        : 0.0;

    std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(4)
              << time_ms << " ms"
              << " (" << std::fixed << std::setprecision(2)
              << ops_per_sec << " ops/sec)" << std::endl;
}

double safe_ops_per_sec(double time_ms, size_t operations) {
    return (operations > 0 && time_ms > 0.0)
        ? (static_cast<double>(operations) / time_ms * 1000.0)
        : 0.0;
}

void print_result(const BenchmarkResult& result) {
    std::cout << "\nBenchmark: " << result.name << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "Operations:     insert=" << result.insert_operations
              << ", search=" << result.search_operations
              << ", range=" << result.range_operations
              << ", delete=" << result.delete_operations << std::endl;
    std::cout << "Tree Height:    " << result.tree_height << std::endl;
    print_timed_operation("Insert Time:", result.insert_time_ms, result.insert_operations);
    print_timed_operation("Search Time:", result.search_time_ms, result.search_operations);
    print_timed_operation("Range Time:", result.range_time_ms, result.range_operations);
    print_timed_operation("Delete Time:", result.delete_time_ms, result.delete_operations);
}

BenchmarkResult benchmark_sequential(int fanout, size_t num_ops) {
    BenchmarkResult result;
    result.name = "Sequential Workload (fanout=" + std::to_string(fanout) + ")";
    result.insert_operations = num_ops;
    result.search_operations = num_ops;
    result.range_operations = num_ops;
    result.delete_operations = num_ops;
    
    BPlusTree<int, int> tree(fanout);
    Timer timer;
    
    timer.reset();
    for (size_t i = 0; i < num_ops; ++i) {
        tree.insert(static_cast<int>(i), static_cast<int>(i * 10));
    }
    result.insert_time_ms = timer.elapsed_ms();
    result.tree_height = tree.height();
    
    timer.reset();
    int value = 0;
    for (size_t i = 0; i < num_ops; ++i) {
        if (tree.search(static_cast<int>(i), value)) {
            benchmark_sink += value;
        }
    }
    result.search_time_ms = timer.elapsed_ms();
    
    timer.reset();
    for (size_t i = 0; i < result.range_operations; ++i) {
        int start_key = static_cast<int>((i * 17) % num_ops);
        int end_key = static_cast<int>(i * 100 + 50);
        end_key = start_key + 50;
        std::vector<int> range = tree.range_query(start_key, end_key);
        benchmark_sink += static_cast<long long>(range.size());
    }
    result.range_time_ms = timer.elapsed_ms();
    
    timer.reset();
    for (size_t i = 0; i < num_ops; ++i) {
        if (tree.remove(static_cast<int>(i))) {
            benchmark_sink++;
        }
    }
    result.delete_time_ms = timer.elapsed_ms();
    
    return result;
}

BenchmarkResult benchmark_random(int fanout, size_t num_ops) {
    BenchmarkResult result;
    result.name = "Random Workload (fanout=" + std::to_string(fanout) + ")";
    result.insert_operations = num_ops;
    result.search_operations = num_ops;
    result.range_operations = num_ops;
    result.delete_operations = num_ops;
    
    std::vector<int> keys;
    std::mt19937 rng(12345);
    int max_key = static_cast<int>(num_ops * 10);
    std::uniform_int_distribution<int> dist(0, max_key);
    
    for (size_t i = 0; i < num_ops; ++i) {
        int key = dist(rng);
        keys.push_back(key);
    }
    
    BPlusTree<int, int> tree(fanout);
    Timer timer;
    
    timer.reset();
    for (size_t i = 0; i < keys.size(); ++i) {
        tree.insert(keys[i], keys[i] * 10);
    }
    result.insert_time_ms = timer.elapsed_ms();
    result.tree_height = tree.height();
    
    timer.reset();
    int value = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (tree.search(keys[i], value)) {
            benchmark_sink += value;
        }
    }
    result.search_time_ms = timer.elapsed_ms();
    
    timer.reset();
    for (size_t i = 0; i < result.range_operations; ++i) {
        int start = dist(rng);
        std::vector<int> range = tree.range_query(start, start + 100);
        benchmark_sink += static_cast<long long>(range.size());
    }
    result.range_time_ms = timer.elapsed_ms();
    
    timer.reset();
    for (size_t i = 0; i < keys.size(); ++i) {
        if (tree.remove(keys[i])) {
            benchmark_sink++;
        }
    }
    result.delete_time_ms = timer.elapsed_ms();
    
    return result;
}

BenchmarkResult benchmark_skewed(int fanout, size_t num_ops) {
    BenchmarkResult result;
    result.name = "Skewed Workload - Zipfian (fanout=" + std::to_string(fanout) + ")";
    result.insert_operations = num_ops;
    result.search_operations = num_ops;
    result.range_operations = 0;
    result.delete_operations = 0;
    
    std::vector<int> keys;
    std::mt19937 rng(12345);
    
    int hot_range = static_cast<int>(num_ops / 5);
    int num_ops_int = static_cast<int>(num_ops);
    
    for (size_t i = 0; i < num_ops; ++i) {
        double prob = static_cast<double>(rng()) / static_cast<double>(rng.max());
        int key;
        if (prob < 0.8) {
            key = rng() % hot_range;
        } else {
            key = hot_range + (rng() % (num_ops_int - hot_range));
        }
        keys.push_back(key);
    }
    
    BPlusTree<int, int> tree(fanout);
    Timer timer;
    
    timer.reset();
    for (size_t i = 0; i < num_ops; ++i) {
        tree.insert(static_cast<int>(i), static_cast<int>(i * 10));
    }
    result.insert_time_ms = timer.elapsed_ms();
    
    timer.reset();
    int value = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (tree.search(keys[i], value)) {
            benchmark_sink += value;
        }
    }
    result.search_time_ms = timer.elapsed_ms();
    
    result.range_time_ms = 0;
    result.delete_time_ms = 0;
    result.tree_height = tree.height();
    
    return result;
}

void fanout_comparison(size_t num_ops) {
    std::cout << "\n\nFanout Comparison (Sequential, " << num_ops << " ops)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    int fanouts[] = {4, 8, 16, 32, 64, 128, 256};
    
    std::cout << std::setw(10) << "Fanout"
              << std::setw(12) << "Height"
              << std::setw(15) << "Insert (ms)"
              << std::setw(15) << "Search (ms)"
              << std::setw(15) << "Range (ms)"
              << std::setw(18) << "Search ops/sec" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (size_t i = 0; i < 7; ++i) {
        BenchmarkResult result = benchmark_sequential(fanouts[i], num_ops);
        
        double search_ops_per_sec =
            safe_ops_per_sec(result.search_time_ms, result.search_operations);

        std::cout << std::setw(10) << fanouts[i]
                  << std::setw(12) << result.tree_height
                  << std::setw(15) << std::fixed << std::setprecision(4) << result.insert_time_ms
                  << std::setw(15) << result.search_time_ms
                  << std::setw(15) << result.range_time_ms
                  << std::setw(18) << std::fixed << std::setprecision(2)
                  << search_ops_per_sec
                  << std::endl;
    }
}

int main(int argc, char* argv[]) {
    print_header();
    
    size_t num_ops = 100000;
    
    if (argc > 1) {
        num_ops = static_cast<size_t>(std::atoi(argv[1]));
    }
    
    std::cout << "\nRunning benchmarks with " << num_ops << " operations..." << std::endl;
    
    BenchmarkResult seq_result = benchmark_sequential(64, num_ops);
    print_result(seq_result);
    
    BenchmarkResult rand_result = benchmark_random(64, num_ops);
    print_result(rand_result);
    
    BenchmarkResult skewed_result = benchmark_skewed(64, num_ops);
    print_result(skewed_result);
    
    fanout_comparison(100000);
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "Benchmark Complete!" << std::endl;
    std::cout << "Checksum: " << benchmark_sink << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    return 0;
}
