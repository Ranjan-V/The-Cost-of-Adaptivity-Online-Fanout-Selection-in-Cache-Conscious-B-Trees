#include "adaptive/adaptive_btree_regression.h"
#include <iostream>
#include <ctime>

using namespace cache_adaptive;

int tests_passed = 0, tests_failed = 0;

#define TEST_ASSERT(condition, test_name) \
    do { \
        if (condition) { std::cout << "[PASS] " << test_name << std::endl; tests_passed++; } \
        else { std::cout << "[FAIL] " << test_name << std::endl; tests_failed++; } \
    } while(0)

void test_basic_construction() {
    AdaptiveBPlusTreeRegression<int, int> tree;
    TEST_ASSERT(tree.get_current_fanout() == 64, "Default fanout should be 64");
}

void test_insert_search() {
    AdaptiveBPlusTreeRegression<int, int> tree;
    for (int i = 1; i <= 10; i++) tree.insert(i, i * 100);
    int value = 0;
    bool found = tree.search(5, value);
    TEST_ASSERT(found && value == 500, "Should find inserted value");
}

void test_adaptation_triggers() {
    AdaptiveBPlusTreeRegression<int, int> tree(64, 100);
    for (int i = 0; i < 500; i++) tree.insert(i, i);
    TEST_ASSERT(tree.get_stats().total_adaptations > 0, "Should trigger adaptations");
}

void test_restructure_preserves_data() {
    AdaptiveBPlusTreeRegression<int, int> tree(128, 20, 8, 128);
    for (int i = 0; i < 40; i++) tree.insert(i, i * 10);
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

    TEST_ASSERT(tree.get_stats().restructure_count > 0, "Should actually restructure");
    TEST_ASSERT(all_found, "Fanout restructuring should preserve key-value data");
}

void test_hot_pattern() {
    AdaptiveBPlusTreeRegression<int, int> tree(64, 50);
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 10; i++) {
            int value = 0;
            tree.search(i % 5, value);
        }
    }
    TEST_ASSERT(tree.get_stats().total_adaptations > 0, "Should adapt to hot pattern");
}

void test_cold_pattern() {
    AdaptiveBPlusTreeRegression<int, int> tree(16, 50);
    for (int i = 0; i < 200; i++) tree.insert(i, i);
    for (int i = 0; i < 200; i++) {
        int value = 0;
        tree.search(i, value);
    }
    TEST_ASSERT(tree.get_stats().total_adaptations > 0, "Should adapt to cold pattern");
}

void test_cache_hit_tracking() {
    AdaptiveBPlusTreeRegression<int, int> tree(64, 50);
    for (int i = 0; i < 50; i++) tree.insert(i, i);
    for (int round = 0; round < 20; round++) {
        for (int i = 0; i < 5; i++) {
            int value = 0;
            tree.search(i, value);
        }
    }
    TEST_ASSERT(tree.get_cache_hit_rate() > 0.0, "Should track cache hit rate");
}

void test_regression_learning() {
    AdaptiveBPlusTreeRegression<int, int> tree(64, 100);
    for (int i = 0; i < 1000; i++) {
        tree.insert(i, i);
        int value = 0;
        tree.search(i % 100, value);
    }
    TEST_ASSERT(tree.get_stats().total_adaptations > 0, "Regression should learn");
}

int main() {
    srand(time(NULL));
    
    std::cout << "\n========================================\n"
              << "  Adaptive B-Tree (Regression) Tests\n"
              << "========================================\n" << std::endl;
    
    test_basic_construction();
    test_insert_search();
    test_adaptation_triggers();
    test_restructure_preserves_data();
    test_hot_pattern();
    test_cold_pattern();
    test_cache_hit_tracking();
    test_regression_learning();
    
    std::cout << "\n========================================\n"
              << "  Test Results\n"
              << "========================================\n"
              << "PASSED: " << tests_passed << "\n"
              << "FAILED: " << tests_failed << "\n"
              << "TOTAL:  " << (tests_passed + tests_failed) << "\n"
              << "========================================\n" << std::endl;
    
    std::cout << "\n=== Regression Demo ===\n";
    AdaptiveBPlusTreeRegression<int, int> demo(64, 100);
    for (int i = 0; i < 200; i++) demo.insert(i, i * 10);
    
    std::cout << "\nPhase 1: Hot workload...\n";
    for (int i = 0; i < 500; i++) { int v = 0; demo.search(i % 10, v); }
    std::cout << "Fanout after hot: " << demo.get_current_fanout() << std::endl;
    
    std::cout << "\nPhase 2: Cold workload...\n";
    for (int i = 0; i < 500; i++) { int v = 0; demo.search(i, v); }
    std::cout << "Fanout after cold: " << demo.get_current_fanout() << std::endl;
    
    demo.print_summary();
    return (tests_failed == 0) ? 0 : 1;
}
