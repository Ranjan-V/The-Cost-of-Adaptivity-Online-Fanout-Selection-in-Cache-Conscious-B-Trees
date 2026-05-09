#include "../include/btree/btree.h"

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

// GCC 6.3.0/MinGW installations using the win32 thread model often do not
// expose std::thread.  Keep the benchmark C++11-compatible by using Win32
// worker threads on Windows and std::thread on platforms where it is present.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <direct.h>
#include <process.h>
#include <windows.h>
#else
#include <chrono>
#include <sys/stat.h>
#include <thread>
#endif

struct TimedOperation {
    bool update;
    int key;
    int value;

    TimedOperation(bool op_update = false, int op_key = 0, int op_value = 0)
        : update(op_update), key(op_key), value(op_value) {}
};

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

private:
#ifdef _WIN32
    LARGE_INTEGER start_;
#else
    std::chrono::steady_clock::time_point start_;
#endif
};

void ensure_results_dir() {
#ifdef _WIN32
    _mkdir("results");
#else
    mkdir("results", 0755);
#endif
}

std::vector<TimedOperation> generate_workload(size_t record_count,
                                              size_t operation_count,
                                              double read_ratio,
                                              unsigned seed) {
    std::vector<TimedOperation> operations;
    operations.reserve(operation_count);

    const size_t safe_records = record_count == 0 ? 1 : record_count;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> key_dist(
        0, static_cast<int>(safe_records - 1));
    std::uniform_real_distribution<double> probability(0.0, 1.0);

    for (size_t i = 0; i < operation_count; ++i) {
        const int key = key_dist(rng);
        const bool update = probability(rng) >= read_ratio;
        const int value = static_cast<int>(safe_records + i);
        operations.push_back(TimedOperation(update, key, value));
    }

    return operations;
}

struct ThreadRunResult {
    size_t threads;
    size_t operations;
    size_t reads;
    size_t updates;
    double elapsed_ms;
    double throughput;
    long long checksum;

    ThreadRunResult()
        : threads(0)
        , operations(0)
        , reads(0)
        , updates(0)
        , elapsed_ms(0.0)
        , throughput(0.0)
        , checksum(0) {}
};

struct WorkerContext {
    cabtree::BPlusTree<int, int>* tree;
    const std::vector<TimedOperation>* operations;
    std::atomic<size_t>* ready;
    std::atomic<bool>* start;
    std::vector<long long>* checksums;
    std::vector<size_t>* reads;
    std::vector<size_t>* updates;
    size_t begin;
    size_t end;
    size_t slot;
};

void run_worker(WorkerContext* context) {
    context->ready->fetch_add(1, std::memory_order_release);
    while (!context->start->load(std::memory_order_acquire)) {
        cabtree::cpu_yield();
    }

    long long local_checksum = 0;
    size_t local_reads = 0;
    size_t local_updates = 0;
    for (size_t i = context->begin; i < context->end; ++i) {
        const TimedOperation& op = (*context->operations)[i];
        if (op.update) {
            context->tree->insert(op.key, op.value);
            ++local_updates;
        } else {
            int value = 0;
            if (context->tree->search(op.key, value)) {
                local_checksum += value;
            }
            ++local_reads;
        }
    }

    (*context->checksums)[context->slot] = local_checksum;
    (*context->reads)[context->slot] = local_reads;
    (*context->updates)[context->slot] = local_updates;
}

#ifdef _WIN32
unsigned __stdcall run_worker_win32(void* arg) {
    run_worker(static_cast<WorkerContext*>(arg));
    return 0;
}
#endif

ThreadRunResult run_thread_count(
    cabtree::BPlusTree<int, int>& tree,
    const std::vector<TimedOperation>& operations,
    size_t thread_count) {
    ThreadRunResult result;
    result.threads = thread_count;
    result.operations = operations.size();

    std::atomic<size_t> ready(0);
    std::atomic<bool> start(false);
    std::vector<long long> checksums(thread_count, 0);
    std::vector<size_t> reads(thread_count, 0);
    std::vector<size_t> updates(thread_count, 0);
    std::vector<WorkerContext> contexts(thread_count);

#ifdef _WIN32
    std::vector<HANDLE> workers(thread_count, NULL);
#else
    std::vector<std::thread*> workers;
    workers.reserve(thread_count);
#endif

    for (size_t t = 0; t < thread_count; ++t) {
        const size_t begin = (operations.size() * t) / thread_count;
        const size_t end = (operations.size() * (t + 1)) / thread_count;

        contexts[t].tree = &tree;
        contexts[t].operations = &operations;
        contexts[t].ready = &ready;
        contexts[t].start = &start;
        contexts[t].checksums = &checksums;
        contexts[t].reads = &reads;
        contexts[t].updates = &updates;
        contexts[t].begin = begin;
        contexts[t].end = end;
        contexts[t].slot = t;

#ifdef _WIN32
        unsigned thread_id = 0;
        workers[t] = reinterpret_cast<HANDLE>(
            _beginthreadex(NULL, 0, run_worker_win32, &contexts[t], 0, &thread_id));
        if (workers[t] == NULL) {
            std::cerr << "Failed to create worker thread " << t << "\n";
            std::exit(2);
        }
#else
        workers.push_back(new std::thread(run_worker, &contexts[t]));
#endif
    }

    while (ready.load(std::memory_order_acquire) < thread_count) {
        cabtree::cpu_yield();
    }

    Timer timer;
    start.store(true, std::memory_order_release);

#ifdef _WIN32
    for (size_t i = 0; i < workers.size(); ++i) {
        WaitForSingleObject(workers[i], INFINITE);
        CloseHandle(workers[i]);
    }
#else
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i]->join();
        delete workers[i];
    }
#endif

    result.elapsed_ms = timer.elapsed_ms();
    for (size_t i = 0; i < thread_count; ++i) {
        result.checksum += checksums[i];
        result.reads += reads[i];
        result.updates += updates[i];
    }
    result.throughput =
        result.elapsed_ms > 0.0
            ? (static_cast<double>(result.operations) * 1000.0) / result.elapsed_ms
            : 0.0;

    return result;
}

int main(int argc, char** argv) {
    size_t record_count = 1000000;
    size_t operation_count = 10000000;
    double read_ratio = 0.95;

    if (argc > 1) {
        record_count = static_cast<size_t>(std::strtoull(argv[1], NULL, 10));
    }
    if (argc > 2) {
        operation_count = static_cast<size_t>(std::strtoull(argv[2], NULL, 10));
    }
    if (argc > 3) {
        read_ratio = std::atof(argv[3]);
    }

    if (read_ratio < 0.0) {
        read_ratio = 0.0;
    }
    if (read_ratio > 1.0) {
        read_ratio = 1.0;
    }

    ensure_results_dir();

    std::cout << "\n============================================================\n";
    std::cout << "  Optimistic Lock Coupling Thread Scalability Benchmark\n";
    std::cout << "============================================================\n";
    std::cout << "Records: " << record_count
              << ", Operations: " << operation_count
              << ", Read ratio: " << read_ratio << "\n";
    std::cout << "Usage: bench_threads.exe [records] [operations] [read_ratio]\n";
    std::cout << "CSV output: results/thread_scalability.csv\n\n";

    cabtree::BPlusTree<int, int> tree(64);

    Timer load_timer;
    for (size_t i = 0; i < record_count; ++i) {
        tree.insert(static_cast<int>(i), static_cast<int>(i));
    }
    const double load_ms = load_timer.elapsed_ms();

    std::cout << "Loaded " << tree.size()
              << " keys in " << std::fixed << std::setprecision(2)
              << load_ms << " ms\n";

    std::cout << "Generating workload...\n";
    std::vector<TimedOperation> operations =
        generate_workload(record_count, operation_count, read_ratio, 20260509U);

    const size_t thread_counts[] = {1, 4, 8, 16, 32};
    std::vector<ThreadRunResult> results;

    std::cout << "\n"
              << std::left << std::setw(10) << "threads"
              << std::right << std::setw(14) << "ops/sec"
              << std::setw(12) << "reads"
              << std::setw(12) << "updates"
              << std::setw(12) << "ms"
              << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t i = 0; i < sizeof(thread_counts) / sizeof(thread_counts[0]); ++i) {
        ThreadRunResult result = run_thread_count(tree, operations, thread_counts[i]);
        results.push_back(result);

        std::cout << std::left << std::setw(10) << result.threads
                  << std::right << std::setw(14) << std::fixed
                  << std::setprecision(0) << result.throughput
                  << std::setw(12) << result.reads
                  << std::setw(12) << result.updates
                  << std::setw(12) << std::setprecision(2) << result.elapsed_ms
                  << "\n";
    }

    std::ofstream csv("results/thread_scalability.csv");
    csv << "threads,records,operations,read_ratio,reads,updates,elapsed_ms,ops_sec,checksum\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const ThreadRunResult& r = results[i];
        csv << r.threads << ","
            << record_count << ","
            << r.operations << ","
            << read_ratio << ","
            << r.reads << ","
            << r.updates << ","
            << std::fixed << std::setprecision(6) << r.elapsed_ms << ","
            << std::fixed << std::setprecision(2) << r.throughput << ","
            << r.checksum << "\n";
    }

    long long checksum = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        checksum += results[i].checksum;
    }

    std::cout << "\nChecksum: " << checksum << "\n";
    std::cout << "Benchmark complete.\n";

    return 0;
}
