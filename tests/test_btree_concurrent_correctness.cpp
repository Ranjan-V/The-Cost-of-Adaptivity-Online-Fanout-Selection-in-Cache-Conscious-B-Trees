#include "../include/btree/btree.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef CABTREE_SANITIZER_MODE
#define CABTREE_SANITIZER_MODE "none"
#endif

namespace {
const int kThreads = 4;
const int kKeys = 20000;
const uint64_t kSeed = 20260928ULL;

struct Outcome {
    std::string operation;
    size_t expected_count, observed_count;
    uint64_t expected_checksum, observed_checksum;
    size_t misses;
    bool invariants, reference, lookup, range;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void wait_for_start(std::atomic<int>& ready, std::atomic<bool>& start) {
    ready.fetch_add(1, std::memory_order_release);
    while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
}

uint64_t checksum(const std::vector<std::pair<int, int> >& records) {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < records.size(); ++i) {
        hash = (hash ^ static_cast<uint32_t>(records[i].first)) * 1099511628211ULL;
        hash = (hash ^ static_cast<uint32_t>(records[i].second)) * 1099511628211ULL;
    }
    return hash;
}

Outcome verify(cabtree::BPlusTree<int, int>& tree,
               const std::vector<std::pair<int, int> >& expected,
               const std::string& operation, size_t concurrent_misses) {
    Outcome out;
    out.operation = operation;
    out.expected_count = expected.size();
    out.observed_count = tree.size();
    out.misses = concurrent_misses;
    std::string reason;
    out.invariants = tree.validate_invariants(&reason);
    require(out.invariants, operation + ": " + reason);
    const std::vector<std::pair<int, int> > observed = tree.export_sorted();
    out.reference = observed == expected;
    out.expected_checksum = checksum(expected);
    out.observed_checksum = checksum(observed);
    require(out.reference, operation + ": final-state reference mismatch");
    require(out.expected_count == out.observed_count, operation + ": key count mismatch");
    out.lookup = true;
    for (size_t i = 0; i < expected.size(); ++i) {
        int value = 0;
        if (!tree.search(expected[i].first, value) || value != expected[i].second) {
            out.lookup = false;
            break;
        }
    }
    require(out.lookup, operation + ": post-join lookup mismatch");
    out.range = true;
    if (!expected.empty()) {
        const size_t first = expected.size() / 4;
        const size_t last = expected.size() * 3 / 4;
        const std::vector<int> values = tree.range_query(expected[first].first,
                                                         expected[last].first);
        out.range = values.size() == last - first + 1;
        for (size_t i = 0; out.range && i < values.size(); ++i)
            out.range = values[i] == expected[first + i].second;
    }
    require(out.range, operation + ": post-join range mismatch");
    return out;
}

Outcome disjoint_inserts() {
    cabtree::BPlusTree<int, int> tree(64);
    std::atomic<int> ready(0); std::atomic<bool> start(false);
    std::vector<std::thread> threads;
    for (int worker = 0; worker < kThreads; ++worker) {
        threads.push_back(std::thread([&, worker]() {
            wait_for_start(ready, start);
            const int begin = worker * (kKeys / kThreads);
            const int end = begin + kKeys / kThreads;
            for (int key = begin; key < end; ++key) tree.insert(key, key * 3 + 1);
        }));
    }
    while (ready.load(std::memory_order_acquire) != kThreads) std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (size_t i = 0; i < threads.size(); ++i) threads[i].join();
    std::vector<std::pair<int, int> > expected;
    for (int key = 0; key < kKeys; ++key) expected.push_back(std::make_pair(key, key * 3 + 1));
    return verify(tree, expected, "disjoint-inserts", 0);
}

Outcome disjoint_updates() {
    cabtree::BPlusTree<int, int> tree(64);
    for (int key = 0; key < kKeys; ++key) tree.insert(key, key);
    std::atomic<int> ready(0); std::atomic<bool> start(false);
    std::vector<std::thread> threads;
    for (int worker = 0; worker < kThreads; ++worker) {
        threads.push_back(std::thread([&, worker]() {
            wait_for_start(ready, start);
            for (int key = worker; key < kKeys; key += kThreads)
                tree.insert(key, 1000000 + key);
        }));
    }
    while (ready.load(std::memory_order_acquire) != kThreads) std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (size_t i = 0; i < threads.size(); ++i) threads[i].join();
    std::vector<std::pair<int, int> > expected;
    for (int key = 0; key < kKeys; ++key) expected.push_back(std::make_pair(key, 1000000 + key));
    return verify(tree, expected, "disjoint-updates", 0);
}

Outcome controlled_read_update() {
    cabtree::BPlusTree<int, int> tree(64);
    for (int key = 0; key < kKeys; ++key) tree.insert(key, key);
    std::atomic<int> ready(0); std::atomic<bool> start(false);
    std::atomic<size_t> misses(0);
    std::vector<std::thread> threads;
    for (int worker = 0; worker < 2; ++worker) {
        threads.push_back(std::thread([&, worker]() {
            wait_for_start(ready, start);
            for (int key = worker; key < kKeys; key += 2) tree.insert(key, 2000000 + key);
        }));
    }
    for (int reader = 0; reader < 2; ++reader) {
        threads.push_back(std::thread([&, reader]() {
            wait_for_start(ready, start);
            for (int pass = 0; pass < 4; ++pass)
                for (int key = reader; key < kKeys; key += 2) {
                    int value = 0;
                    if (!tree.search(key, value)) misses.fetch_add(1, std::memory_order_relaxed);
                }
        }));
    }
    while (ready.load(std::memory_order_acquire) != kThreads) std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (size_t i = 0; i < threads.size(); ++i) threads[i].join();
    std::vector<std::pair<int, int> > expected;
    for (int key = 0; key < kKeys; ++key) expected.push_back(std::make_pair(key, 2000000 + key));
    return verify(tree, expected, "controlled-read-update", misses.load());
}

std::string compiler_name() {
#ifdef __clang__
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "UNKNOWN";
#endif
}

std::string compiler_version() {
#ifdef __VERSION__
    return __VERSION__;
#else
    return "UNKNOWN";
#endif
}

void write_evidence(const std::string& path, const std::vector<Outcome>& rows) {
    std::ofstream out(path.c_str());
    require(static_cast<bool>(out), "cannot open evidence output: " + path);
    out << "compiler,compiler_version,sanitizer_mode,thread_count,operation_type,seed,"
           "expected_key_count,observed_key_count,checksum_expected,checksum_observed,"
           "miss_count,invariant_status,reference_match,lookup_status,range_status,exit_status\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        const Outcome& row = rows[i];
        out << compiler_name() << ',' << compiler_version() << ',' << CABTREE_SANITIZER_MODE
            << ',' << kThreads << ',' << row.operation << ',' << kSeed << ','
            << row.expected_count << ',' << row.observed_count << ','
            << row.expected_checksum << ',' << row.observed_checksum << ',' << row.misses
            << ',' << (row.invariants ? "STRUCTURAL_INVARIANTS_OK" : "FAIL")
            << ',' << (row.reference ? "FINAL_STATE_REFERENCE_MATCH" : "FAIL")
            << ',' << (row.lookup ? "LOOKUP_CHECK_OK" : "FAIL")
            << ',' << (row.range ? "RANGE_CHECK_OK" : "FAIL") << ",PASS\n";
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<Outcome> rows;
        rows.push_back(disjoint_inserts());
        rows.push_back(disjoint_updates());
        rows.push_back(controlled_read_update());
        if (argc > 1) write_evidence(argv[1], rows);
        std::cout << "FINAL_STATE_REFERENCE_MATCH\nSTRUCTURAL_INVARIANTS_OK\n"
                     "LOOKUP_CHECK_OK\nRANGE_CHECK_OK\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "CONCURRENT_CORRECTNESS_FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
