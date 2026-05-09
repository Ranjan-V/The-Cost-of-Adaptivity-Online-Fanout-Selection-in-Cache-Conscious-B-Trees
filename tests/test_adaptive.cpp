#include "adaptive/adaptive_btree.h"
#include <iostream>
#include <cassert>
#include <ctime>

using namespace cache_adaptive;

// Test counter
int tests_passed = 0;
int tests_failed = 0;

// Helper macro
#define TEST_ASSERT(condition, test_name) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << test_name << std::endl; \
            tests_passed++; \
        } else { \
            std::cout << "[FAIL] " << test_name << std::endl; \
            tests_failed++; \
        } \
    } while(0)

/**
 * Test 1: Basic construction
 */
void test_basic_construction() {
    AdaptiveBPlusTree<int, int> tree;
    
    TEST_ASSERT(tree.get_current_fanout() == 64,
                "Default fanout should be 64");
}

/**
 * Test 2: Insert and search
 */
void test_insert_search() {
    AdaptiveBPlusTree<int, int> tree;
    
    // Insert some values
    for (int i = 1; i <= 10; i++) {
        tree.insert(i, i * 100);
    }
    
    // Search for a value
    int value;
    bool found = tree.search(5, value);
    
    TEST_ASSERT(found && value == 500,
                "Should find inserted value");
}

/**
 * Test 3: Adaptation triggers
 */
void test_adaptation_triggers() {
    AdaptiveBPlusTree<int, int> tree(64, 100);  // Adapt every 100 ops

    // Perform many operations to trigger adaptation
    for (int i = 0; i < 500; i++) {
        tree.insert(i, i);
    }
    
    AdaptiveStats stats = tree.get_stats();
    TEST_ASSERT(stats.total_adaptations > 0,
                "Should trigger adaptations after 500 ops");
}

/**
 * Test 4: Restructuring preserves data
 */
void test_restructure_preserves_data() {
    AdaptiveBPlusTree<int, int> tree(128, 20, 8, 128);

    for (int i = 0; i < 40; i++) {
        tree.insert(i, i * 10);
    }

    for (int i = 0; i < 400; i++) {
        int value = 0;
        tree.search(i % 4, value);
    }

    bool all_found = true;
    for (int i = 0; i < 40; i++) {
        int value = 0;
        bool found = tree.search(i, value);
        if (!found || value != i * 10) {
            all_found = false;
            break;
        }
    }

    TEST_ASSERT(tree.get_stats().restructure_count > 0,
                "Hot workload should trigger at least one restructure");
    TEST_ASSERT(all_found,
                "Fanout restructuring should preserve key-value data");
}

/**
 * Test 5: Hot access pattern (should decrease fanout)
 */
void test_hot_pattern_adaptation() {
    AdaptiveBPlusTree<int, int> tree(64, 50);
    
    // Create hot access pattern (same keys repeatedly)
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 10; i++) {
            int value;
            tree.search(i % 5, value);  // Access only 5 keys repeatedly
        }
    }
    
    AdaptiveStats stats = tree.get_stats();
    TEST_ASSERT(stats.total_adaptations > 0,
                "Should adapt to hot pattern");
}

/**
 * Test 6: Cold access pattern (should increase fanout)
 */
void test_cold_pattern_adaptation() {
    AdaptiveBPlusTree<int, int> tree(16, 50);
    
    // Insert many keys
    for (int i = 0; i < 200; i++) {
        tree.insert(i, i);
    }
    
    // Create cold access pattern (spread across many keys)
    for (int i = 0; i < 200; i++) {
        int value;
        tree.search(i, value);  // Each key accessed once
    }
    
    AdaptiveStats stats = tree.get_stats();
    TEST_ASSERT(stats.total_adaptations > 0,
                "Should adapt to cold pattern");
}

/**
 * Test 7: Range query
 */
void test_range_query() {
    AdaptiveBPlusTree<int, int> tree;
    
    // Insert values
    for (int i = 1; i <= 100; i++) {
        tree.insert(i, i * 10);
    }
    
    // Range query
    std::vector<int> results = tree.range_query(10, 20);
    
    TEST_ASSERT(results.size() > 0,
                "Range query should return results");
}

/**
 * Test 8: Cache hit rate tracking
 */
void test_cache_hit_tracking() {
    AdaptiveBPlusTree<int, int> tree(64, 50);
    
    // Insert keys
    for (int i = 0; i < 50; i++) {
        tree.insert(i, i);
    }
    
    // Create hot pattern (high cache hit rate)
    for (int round = 0; round < 20; round++) {
        for (int i = 0; i < 5; i++) {
            int value;
            tree.search(i, value);
        }
    }
    
    double hit_rate = tree.get_cache_hit_rate();
    TEST_ASSERT(hit_rate > 0.0,
                "Should track cache hit rate");
}

/**
 * Test 9: Statistics collection
 */
void test_statistics() {
    AdaptiveBPlusTree<int, int> tree(64, 100);
    
    // Generate workload
    for (int i = 0; i < 500; i++) {
        tree.insert(i, i);
        int value;
        tree.search(i % 50, value);
    }
    
    AdaptiveStats stats = tree.get_stats();
    
    TEST_ASSERT(stats.total_adaptations > 0,
                "Should collect adaptation statistics");
    
    size_t total_actions = stats.fanout_increases + 
                          stats.fanout_decreases + 
                          stats.fanout_maintained;
    TEST_ASSERT(total_actions == stats.total_adaptations,
                "Action counts should sum to total adaptations");
}

/**
 * Test 10: Fanout bounds
 */
void test_fanout_bounds() {
    AdaptiveBPlusTree<int, int> tree(64, 50, 16, 128);
    
    // Run many adaptations
    for (int i = 0; i < 1000; i++) {
        tree.insert(i, i);
        int value;
        tree.search(i % 100, value);
    }
    
    size_t final_fanout = tree.get_current_fanout();
    TEST_ASSERT(final_fanout >= 16 && final_fanout <= 128,
                "Fanout should respect bounds");
}

/**
 * Test 11: RL learning over time
 */
void test_rl_learning() {
    AdaptiveBPlusTree<int, int> tree(64, 100);
    
    // Phase 1: Hot pattern
    for (int i = 0; i < 500; i++) {
        int value;
        tree.search(i % 10, value);  // Hot
    }
    
    AdaptiveStats stats1 = tree.get_stats();
    
    // Phase 2: Continue hot pattern
    for (int i = 0; i < 500; i++) {
        int value;
        tree.search(i % 10, value);
    }
    
    AdaptiveStats stats2 = tree.get_stats();
    
    TEST_ASSERT(stats2.total_adaptations > stats1.total_adaptations,
                "RL should continue learning");
}

/**
 * Test 12: Workload shift detection
 */
void test_workload_shift() {
    AdaptiveBPlusTree<int, int> tree(64, 50);
    
    // Insert keys
    for (int i = 0; i < 100; i++) {
        tree.insert(i, i);
    }
    
    // Phase 1: Hot pattern
    for (int i = 0; i < 200; i++) {
        int value;
        tree.search(i % 5, value);
    }
    
    // Phase 2: Cold pattern
    for (int i = 0; i < 200; i++) {
        int value;
        tree.search(i, value);
    }

    TEST_ASSERT(tree.get_stats().total_adaptations > 0,
                "Should detect workload shift");
}

/**
 * Test 13: Mixed workload
 */
void test_mixed_workload() {
    AdaptiveBPlusTree<int, int> tree(64, 100);
    
    // Insert keys
    for (int i = 0; i < 200; i++) {
        tree.insert(i, i);
    }
    
    // Mixed: some hot, some cold
    for (int i = 0; i < 500; i++) {
        int value;
        if (i % 3 == 0) {
            tree.search(i % 10, value);  // Hot
        } else {
            tree.search(i, value);        // Cold
        }
    }
    
    AdaptiveStats stats = tree.get_stats();
    TEST_ASSERT(stats.total_adaptations > 0,
                "Should handle mixed workload");
}

/**
 * Test 14: Performance under adaptation
 */
void test_performance() {
    AdaptiveBPlusTree<int, int> tree(64, 200);
    
    clock_t start = clock();
    
    // Large workload
    for (int i = 0; i < 1000; i++) {
        tree.insert(i, i);
    }
    
    for (int i = 0; i < 1000; i++) {
        int value;
        tree.search(i % 100, value);
    }
    
    clock_t end = clock();
    double elapsed = double(end - start) / CLOCKS_PER_SEC;
    
    TEST_ASSERT(elapsed < 10.0,
                "Should complete 2000 ops in reasonable time");
}

/**
 * Main test runner
 */
int main() {
    // Seed random for RL agent
    srand(time(NULL));
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Adaptive B-Tree Test Suite" << std::endl;
    std::cout << "  (with Reinforcement Learning)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    test_basic_construction();
    test_insert_search();
    test_adaptation_triggers();
    test_restructure_preserves_data();
    test_hot_pattern_adaptation();
    test_cold_pattern_adaptation();
    test_range_query();
    test_cache_hit_tracking();
    test_statistics();
    test_fanout_bounds();
    test_rl_learning();
    test_workload_shift();
    test_mixed_workload();
    test_performance();
    
    // Print summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "PASSED: " << tests_passed << std::endl;
    std::cout << "FAILED: " << tests_failed << std::endl;
    std::cout << "TOTAL:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Detailed demo
    std::cout << "\n=== Detailed Adaptive Demo ===" << std::endl;
    AdaptiveBPlusTree<int, int> demo(64, 100);
    
    // Insert data
    for (int i = 0; i < 200; i++) {
        demo.insert(i, i * 10);
    }
    
    // Hot workload
    std::cout << "\nPhase 1: Hot workload (10 keys repeatedly)..." << std::endl;
    for (int i = 0; i < 500; i++) {
        int value;
        demo.search(i % 10, value);
    }
    
    std::cout << "Fanout after hot workload: " << demo.get_current_fanout() << std::endl;
    
    // Cold workload
    std::cout << "\nPhase 2: Cold workload (spread across all keys)..." << std::endl;
    for (int i = 0; i < 500; i++) {
        int value;
        demo.search(i, value);
    }
    
    std::cout << "Fanout after cold workload: " << demo.get_current_fanout() << std::endl;
    
    // Print full summary
    demo.print_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}
