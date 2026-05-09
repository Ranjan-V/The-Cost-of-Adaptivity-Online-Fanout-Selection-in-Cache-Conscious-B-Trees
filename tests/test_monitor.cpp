#include "monitor/monitor.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>
#include <vector>

using namespace cache_adaptive;

int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, test_name) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << test_name << std::endl; \
            ++tests_passed; \
        } else { \
            std::cout << "[FAIL] " << test_name << std::endl; \
            ++tests_failed; \
        } \
    } while (0)

bool in_unit_interval(double value) {
    return value >= 0.0 && value <= 1.0;
}

void test_basic_construction() {
    AccessMonitor monitor;

    TEST_ASSERT(monitor.get_total_accesses() == 0,
                "Initial total accesses should be 0");
    TEST_ASSERT(monitor.get_sampled_accesses() == 0,
                "Initial sampled accesses should be 0");
    TEST_ASSERT(monitor.get_unique_node_count() == 0,
                "Initial unique node count should be 0");
    TEST_ASSERT(monitor.get_sampling_rate() == 10,
                "Default sampling rate should be 1/10");
}

void test_exact_integer_tracking() {
    AccessMonitor monitor(10000, 0.1, 1, 0.99);

    for (int i = 0; i < 10; ++i) monitor.record_access(0);
    for (int i = 0; i < 3; ++i) monitor.record_access(1);

    TEST_ASSERT(monitor.get_total_accesses() == 13,
                "Exact mode should count every access");
    TEST_ASSERT(monitor.get_sampled_accesses() == 13,
                "Exact mode should sample every access");
    TEST_ASSERT(monitor.get_unique_node_count() == 2,
                "Exact mode should track both integer node IDs");
    TEST_ASSERT(monitor.get_access_count(0) == 10,
                "Integer node ID 0 should be tracked safely");
    TEST_ASSERT(monitor.get_access_count(1) == 3,
                "Integer node ID 1 should have exact count");
    TEST_ASSERT(monitor.get_heat_score(0) > monitor.get_heat_score(1),
                "More frequently accessed integer node should be hotter");
}

void test_pointer_tracking() {
    AccessMonitor monitor(10000, 0.1, 1, 0.99);
    int nodes[3] = {0, 0, 0};

    for (int i = 0; i < 20; ++i) monitor.record_access(&nodes[1]);
    for (int i = 0; i < 5; ++i) monitor.record_access(&nodes[2]);

    double hot_heat = monitor.get_heat_score(&nodes[1]);
    double cold_heat = monitor.get_heat_score(&nodes[2]);
    std::vector<std::pair<void*, double> > hot_pairs =
        monitor.get_hottest_node_pairs(1);

    TEST_ASSERT(monitor.get_unique_node_count() == 2,
                "Pointer API should track two node addresses");
    TEST_ASSERT(in_unit_interval(hot_heat) && in_unit_interval(cold_heat),
                "Pointer heat scores should stay in [0,1]");
    TEST_ASSERT(hot_heat > cold_heat,
                "More frequently touched pointer should be hotter");
    TEST_ASSERT(hot_pairs.size() == 1 && hot_pairs[0].first == &nodes[1],
                "Pointer pair API should return the hottest pointer");
}

void test_sampling_estimated_counts() {
    AccessMonitor monitor;

    for (int i = 0; i < 100; ++i) {
        monitor.record_access(42);
    }

    TEST_ASSERT(monitor.get_total_accesses() == 100,
                "Default monitor should count all observed accesses");
    TEST_ASSERT(monitor.get_sampled_accesses() == 10,
                "Default monitor should sample one in ten accesses");
    TEST_ASSERT(monitor.get_access_count(42) == 100,
                "Sampled node count should be scaled to an estimate");
    TEST_ASSERT(monitor.is_hot_node(42),
                "Repeated sampled accesses should produce a hot node");
}

void test_decay_all() {
    AccessMonitor monitor(10000, 0.1, 1, 0.99);

    for (int i = 0; i < 20; ++i) {
        monitor.record_access(7);
    }

    double before = monitor.get_heat_score(7);
    monitor.decay_all(0.25);
    double after = monitor.get_heat_score(7);

    TEST_ASSERT(before > 0.7,
                "Repeated exact accesses should become hot before decay");
    TEST_ASSERT(after < before,
                "decay_all should reduce heat scores");
    TEST_ASSERT(monitor.is_cold_node(7),
                "Strong decay should make the node cold");
}

void test_hottest_nodes_ordering() {
    AccessMonitor monitor(10000, 0.1, 1, 1.0);

    for (int i = 0; i < 100; ++i) monitor.record_access(1);
    for (int i = 0; i < 50; ++i) monitor.record_access(2);
    for (int i = 0; i < 25; ++i) monitor.record_access(3);
    for (int i = 0; i < 10; ++i) monitor.record_access(4);

    std::vector<NodeAccessInfo> hottest = monitor.get_hottest_nodes(3);

    TEST_ASSERT(hottest.size() == 3,
                "Should return the requested top 3 nodes");
    TEST_ASSERT(hottest[0].node_id == 1,
                "Hottest node should be node 1");
    TEST_ASSERT(hottest[1].node_id == 2,
                "Second hottest node should be node 2");
    TEST_ASSERT(hottest[2].node_id == 3,
                "Third hottest node should be node 3");
}

void test_zipfian_hot_set_detection() {
    AccessMonitor monitor(10000, 0.1, 1, 0.95);

    for (int i = 0; i < 5000; ++i) {
        if (i % 5 < 4) {
            monitor.record_access(i % 2);
        } else {
            monitor.record_access(2 + (i % 8));
        }
    }

    std::vector<NodeAccessInfo> hottest = monitor.get_hottest_nodes(2);
    std::set<size_t> hot_set;
    for (size_t i = 0; i < hottest.size(); ++i) {
        hot_set.insert(hottest[i].node_id);
    }

    TEST_ASSERT(hottest.size() == 2,
                "Zipfian test should return two hottest nodes");
    TEST_ASSERT(hot_set.count(0) == 1 && hot_set.count(1) == 1,
                "Zipfian 80/20 workload should identify the hot 20 percent");
    TEST_ASSERT(hottest[0].heat_score > monitor.get_heat_score(5),
                "Hot-set heat should exceed a cold node heat");
}

void test_monitor_stats() {
    AccessMonitor monitor(10000, 0.1, 1, 1.0);

    for (int i = 0; i < 30; ++i) monitor.record_access(11);
    for (int i = 0; i < 3; ++i) monitor.record_access(12);

    MonitorStats stats = monitor.get_monitor_stats();

    TEST_ASSERT(stats.total_accesses == 33,
                "Stats should report total accesses");
    TEST_ASSERT(stats.sampled_accesses == 33,
                "Stats should report sampled accesses");
    TEST_ASSERT(stats.unique_nodes == 2,
                "Stats should report unique nodes");
    TEST_ASSERT(stats.avg_heat_score > 0.0,
                "Stats should report positive average heat");
    TEST_ASSERT(stats.hot_nodes_count >= 1,
                "Stats should detect a hot node");
    TEST_ASSERT(stats.cold_nodes_count >= 1,
                "Stats should detect a cold node");
}

void test_clear() {
    AccessMonitor monitor(10000, 0.1, 1, 0.99);

    for (int i = 0; i < 100; ++i) {
        monitor.record_access(i % 10);
    }

    monitor.clear();

    TEST_ASSERT(monitor.get_total_accesses() == 0,
                "clear should reset total accesses");
    TEST_ASSERT(monitor.get_sampled_accesses() == 0,
                "clear should reset sampled accesses");
    TEST_ASSERT(monitor.get_unique_node_count() == 0,
                "clear should remove all tracked nodes");
    TEST_ASSERT(monitor.get_access_count(0) == 0,
                "clear should reset per-node counters");
    TEST_ASSERT(monitor.get_heat_score(0) == 0.0,
                "clear should reset per-node heat");
}

void test_record_access_overhead() {
    AccessMonitor monitor;
    const int warmup = 1000;
    const int iterations = 1000000;

    for (int i = 0; i < warmup; ++i) {
        monitor.record_access(99);
    }

    std::chrono::high_resolution_clock::time_point start =
        std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        monitor.record_access(99);
    }

    std::chrono::high_resolution_clock::time_point end =
        std::chrono::high_resolution_clock::now();

    double elapsed_ns = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start).count());
    double avg_ns = elapsed_ns / static_cast<double>(iterations);

    std::cout << "Average record_access cost: " << avg_ns << " ns" << std::endl;
    TEST_ASSERT(avg_ns < 50.0,
                "record_access average overhead should stay below 50 ns");
}

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Access Monitor Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;

    test_basic_construction();
    test_exact_integer_tracking();
    test_pointer_tracking();
    test_sampling_estimated_counts();
    test_decay_all();
    test_hottest_nodes_ordering();
    test_zipfian_hot_set_detection();
    test_monitor_stats();
    test_clear();
    test_record_access_overhead();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "PASSED: " << tests_passed << std::endl;
    std::cout << "FAILED: " << tests_failed << std::endl;
    std::cout << "TOTAL:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << "========================================\n" << std::endl;

    return tests_failed == 0 ? 0 : 1;
}
