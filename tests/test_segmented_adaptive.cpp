#include "adaptive/segmented_adaptive_btree.h"

#include <cstdlib>
#include <ctime>
#include <iostream>
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

void test_basic_construction() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50);

    TEST_ASSERT(tree.segment_count() == 8, "Should create eight segments");
    TEST_ASSERT(tree.get_average_fanout() == 64, "Average fanout should start at 64");
    TEST_ASSERT(tree.get_segment_fanout(0) == 64, "Segment 0 fanout should start at 64");
    TEST_ASSERT(tree.get_segment_fanout(7) == 64, "Segment 7 fanout should start at 64");
}

void test_segment_routing_boundaries() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50);

    TEST_ASSERT(tree.get_segment_index(0) == 0, "Minimum key should map to segment 0");
    TEST_ASSERT(tree.get_segment_index(99) == 0, "End of first range should map to segment 0");
    TEST_ASSERT(tree.get_segment_index(100) == 1, "Boundary key should map to segment 1");
    TEST_ASSERT(tree.get_segment_index(799) == 7, "Maximum key should map to last segment");
    TEST_ASSERT(tree.get_segment_index(900) == 7, "Out-of-range high key should clamp to last segment");
}

void test_insert_search_across_segments() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50);
    const int keys[] = {0, 99, 100, 199, 350, 599, 799};

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        tree.insert(keys[i], keys[i] * 10);
    }

    bool all_found = true;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        int value = 0;
        bool found = tree.search(keys[i], value);
        if (!found || value != keys[i] * 10) {
            all_found = false;
            break;
        }
    }

    TEST_ASSERT(all_found, "Should find values across segment boundaries");
    TEST_ASSERT(tree.size() == sizeof(keys) / sizeof(keys[0]), "Size should count all segments");
}

void test_update_existing_key() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50);

    tree.insert(150, 1);
    tree.insert(150, 2);

    int value = 0;
    bool found = tree.search(150, value);

    TEST_ASSERT(found && value == 2, "Update should replace existing value");
    TEST_ASSERT(tree.size() == 1, "Update should not create duplicate records");
}

void test_range_query_crosses_segments() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50);

    for (int key = 95; key <= 105; ++key) {
        tree.insert(key, key * 10);
    }

    std::vector<int> values = tree.range_query(98, 102);

    bool correct = values.size() == 5;
    for (size_t i = 0; i < values.size(); ++i) {
        int expected = static_cast<int>(98 + i) * 10;
        if (values[i] != expected) {
            correct = false;
            break;
        }
    }

    TEST_ASSERT(correct, "Range query should cross segment boundary in key order");
}

void test_hot_segment_adapts_independently() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50, 8, 256);

    tree.set_adaptation_enabled(false);
    for (int key = 0; key < 800; ++key) {
        tree.insert(key, key);
    }
    tree.reset_adaptation_state();
    tree.set_adaptation_enabled(true);

    for (int i = 0; i < 600; ++i) {
        int value = 0;
        tree.search(i % 10, value);
    }

    SegmentedAdaptiveSegmentStats hot = tree.get_segment_stats(0);
    SegmentedAdaptiveSegmentStats untouched = tree.get_segment_stats(4);

    TEST_ASSERT(hot.adaptations > 0, "Hot segment should adapt");
    TEST_ASSERT(hot.fanout < 64, "Hot segment should shrink fanout");
    TEST_ASSERT(untouched.adaptations == 0, "Untouched segment should not adapt");
    TEST_ASSERT(untouched.fanout == 64, "Untouched segment fanout should remain unchanged");
}

void test_cold_segment_can_grow() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 16, 50, 8, 256);

    tree.set_adaptation_enabled(false);
    for (int key = 0; key < 800; ++key) {
        tree.insert(key, key);
    }
    tree.reset_adaptation_state();
    tree.set_adaptation_enabled(true);

    for (int i = 0; i < 800; ++i) {
        int value = 0;
        tree.search(600 + ((i * 37) % 97), value);
    }

    SegmentedAdaptiveSegmentStats cold = tree.get_segment_stats(6);
    SegmentedAdaptiveSegmentStats untouched = tree.get_segment_stats(0);

    TEST_ASSERT(cold.adaptations > 0, "Uniform cold segment should adapt");
    TEST_ASSERT(cold.fanout > 16, "Uniform cold segment should grow fanout");
    TEST_ASSERT(untouched.adaptations == 0, "Cold segment adaptation should not touch other segments");
}

void test_cost_policy_blocks_default_cold_growth() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50, 8, 256);

    tree.set_adaptation_enabled(false);
    for (int key = 0; key < 800; ++key) {
        tree.insert(key, key);
    }
    tree.reset_adaptation_state();
    tree.set_adaptation_enabled(true);

    for (int i = 0; i < 1000; ++i) {
        int value = 0;
        tree.search(600 + ((i * 37) % 97), value);
    }

    SegmentedAdaptiveSegmentStats segment = tree.get_segment_stats(6);

    TEST_ASSERT(segment.adaptations > 0, "Cost policy should still evaluate cold segment");
    TEST_ASSERT(segment.fanout == 64, "Cost policy should not grow default fanout on weak cold evidence");
    TEST_ASSERT(segment.rebuilds_skipped_cost > 0, "Cost policy should count rejected rebuilds");
}

void test_total_stats() {
    SegmentedAdaptiveBPlusTree<int, int> tree(8, 0, 799, 64, 50);

    tree.set_adaptation_enabled(false);
    for (int key = 0; key < 400; ++key) {
        tree.insert(key, key);
    }
    tree.reset_adaptation_state();
    tree.set_adaptation_enabled(true);

    for (int i = 0; i < 200; ++i) {
        int value = 0;
        tree.search(i % 20, value);
    }

    SegmentedAdaptiveStats stats = tree.get_stats();

    TEST_ASSERT(stats.segment_count == 8, "Stats should report segment count");
    TEST_ASSERT(stats.total_records == 400, "Stats should report total records");
    TEST_ASSERT(stats.total_accesses == 200, "Stats should report run accesses");
    TEST_ASSERT(stats.total_adaptations > 0, "Stats should aggregate adaptations");
}

int main() {
    srand(static_cast<unsigned>(time(NULL)));

    std::cout << "\n========================================\n"
              << "  Segmented Adaptive B-Tree Tests\n"
              << "========================================\n" << std::endl;

    test_basic_construction();
    test_segment_routing_boundaries();
    test_insert_search_across_segments();
    test_update_existing_key();
    test_range_query_crosses_segments();
    test_hot_segment_adapts_independently();
    test_cold_segment_can_grow();
    test_cost_policy_blocks_default_cold_growth();
    test_total_stats();

    std::cout << "\n========================================\n"
              << "  Test Results\n"
              << "========================================\n"
              << "PASSED: " << tests_passed << "\n"
              << "FAILED: " << tests_failed << "\n"
              << "TOTAL:  " << (tests_passed + tests_failed) << "\n"
              << "========================================\n" << std::endl;

    return tests_failed == 0 ? 0 : 1;
}
