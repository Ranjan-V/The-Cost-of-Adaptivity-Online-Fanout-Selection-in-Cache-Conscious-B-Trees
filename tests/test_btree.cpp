#include "../include/btree/btree.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <string>

using namespace cabtree;

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) \
    void name(); \
    void name##_runner() { \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            tests_passed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (const std::exception& e) { \
            tests_failed++; \
            std::cout << "FAILED: " << e.what() << std::endl; \
        } \
    } \
    void name()

#define ASSERT(condition, message) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + message); \
    }

TEST(test_empty_tree) {
    BPlusTree<int, int> tree(4);
    
    ASSERT(tree.empty(), "New tree should be empty");
    ASSERT(tree.size() == 0, "New tree should have size 0");
    ASSERT(tree.height() == 0, "Empty tree should have height 0");
}

TEST(test_single_insert) {
    BPlusTree<int, int> tree(4);
    
    bool inserted = tree.insert(10, 100);
    
    ASSERT(inserted, "Insert should return true for new key");
    ASSERT(tree.size() == 1, "Tree should have 1 element");
    ASSERT(!tree.empty(), "Tree should not be empty");
}

TEST(test_single_search) {
    BPlusTree<int, int> tree(4);
    
    tree.insert(10, 100);
    int value = 0;
    bool found = tree.search(10, value);
    
    ASSERT(found, "Search should find inserted key");
    ASSERT(value == 100, "Search should return correct value");
}

TEST(test_search_not_found) {
    BPlusTree<int, int> tree(4);
    
    tree.insert(10, 100);
    int value = 0;
    bool found = tree.search(20, value);
    
    ASSERT(!found, "Search should return false for missing key");
}

TEST(test_multiple_inserts) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 0; i < 10; ++i) {
        tree.insert(i, i * 10);
    }
    
    ASSERT(tree.size() == 10, "Tree should have 10 elements");
    
    for (int i = 0; i < 10; ++i) {
        int value = 0;
        bool found = tree.search(i, value);
        ASSERT(found, "All inserted keys should be found");
        ASSERT(value == i * 10, "Values should match");
    }
}

TEST(test_duplicate_insert) {
    BPlusTree<int, int> tree(4);
    
    tree.insert(10, 100);
    bool inserted = tree.insert(10, 200);
    
    ASSERT(!inserted, "Inserting duplicate key should return false");
    ASSERT(tree.size() == 1, "Size should remain 1");
    
    int value = 0;
    tree.search(10, value);
    ASSERT(value == 200, "Value should be updated");
}

TEST(test_insert_descending) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 100; i > 0; --i) {
        tree.insert(i, i * 10);
    }
    
    ASSERT(tree.size() == 100, "Tree should have 100 elements");
    
    for (int i = 1; i <= 100; ++i) {
        int value = 0;
        bool found = tree.search(i, value);
        ASSERT(found, "All keys should be found");
        ASSERT(value == i * 10, "Values should match");
    }
}

TEST(test_remove_single) {
    BPlusTree<int, int> tree(4);
    
    tree.insert(10, 100);
    bool removed = tree.remove(10);
    
    ASSERT(removed, "Remove should return true");
    ASSERT(tree.size() == 0, "Tree should be empty");
    
    int value = 0;
    bool found = tree.search(10, value);
    ASSERT(!found, "Removed key should not be found");
}

TEST(test_remove_not_found) {
    BPlusTree<int, int> tree(4);
    
    tree.insert(10, 100);
    bool removed = tree.remove(20);
    
    ASSERT(!removed, "Removing non-existent key should return false");
    ASSERT(tree.size() == 1, "Size should remain unchanged");
}

TEST(test_remove_multiple) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 0; i < 20; ++i) {
        tree.insert(i, i * 10);
    }
    
    for (int i = 0; i < 10; ++i) {
        tree.remove(i);
    }
    
    ASSERT(tree.size() == 10, "Tree should have 10 elements remaining");
    
    int value = 0;
    for (int i = 0; i < 10; ++i) {
        bool found = tree.search(i, value);
        ASSERT(!found, "Removed keys should not be found");
    }
    
    for (int i = 10; i < 20; ++i) {
        bool found = tree.search(i, value);
        ASSERT(found, "Non-removed keys should still be found");
    }
}

TEST(test_range_query_simple) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 0; i < 20; ++i) {
        tree.insert(i, i * 10);
    }
    
    std::vector<int> results = tree.range_query(5, 10);
    
    ASSERT(results.size() == 6, "Range [5,10] should return 6 elements");
    
    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT(results[i] == (int)((5 + i) * 10), "Range query values should be correct");
    }
}

TEST(test_range_query_empty) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 0; i < 10; ++i) {
        tree.insert(i * 2, i);
    }
    
    std::vector<int> results = tree.range_query(21, 25);
    
    ASSERT(results.size() == 0, "Range query outside data should return empty");
}

TEST(test_range_query_full) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 0; i < 10; ++i) {
        tree.insert(i, i * 10);
    }
    
    std::vector<int> results = tree.range_query(0, 100);
    
    ASSERT(results.size() == 10, "Range covering all data should return all elements");
}

TEST(test_large_sequential_insert) {
    BPlusTree<int, int> tree(64);
    
    const int N = 10000;
    
    for (int i = 0; i < N; ++i) {
        tree.insert(i, i * 10);
    }
    
    ASSERT(tree.size() == (size_t)N, "Tree should contain all inserted elements");
    
    int value = 0;
    for (int i = 0; i < 100; ++i) {
        int key = i * (N / 100);
        bool found = tree.search(key, value);
        ASSERT(found, "Sample key should be found");
        ASSERT(value == key * 10, "Sample value should be correct");
    }
}

TEST(test_large_random_insert) {
    BPlusTree<int, int> tree(64);
    
    const int N = 10000;
    std::vector<int> keys;
    
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, N * 10);
    
    for (int i = 0; i < N; ++i) {
        int key = dist(rng);
        keys.push_back(key);
        tree.insert(key, key * 10);
    }
    
    int value = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        bool found = tree.search(keys[i], value);
        ASSERT(found, "Random key should be found");
        ASSERT(value == keys[i] * 10, "Random value should be correct");
    }
}

TEST(test_insert_delete_cycle) {
    BPlusTree<int, int> tree(16);
    
    for (int cycle = 0; cycle < 100; ++cycle) {
        for (int i = 0; i < 100; ++i) {
            tree.insert(cycle * 1000 + i, i);
        }
        
        for (int i = 0; i < 50; ++i) {
            tree.remove(cycle * 1000 + i);
        }
    }
    
    ASSERT(tree.size() == 5000, "Tree should have 5000 elements after cycles");
}

TEST(test_string_keys) {
    BPlusTree<std::string, int> tree(8);
    
    tree.insert("apple", 1);
    tree.insert("banana", 2);
    tree.insert("cherry", 3);
    tree.insert("date", 4);
    
    ASSERT(tree.size() == 4, "Tree should have 4 elements");
    
    int value = 0;
    bool found = tree.search("banana", value);
    ASSERT(found, "String key should be found");
    ASSERT(value == 2, "String key value should be correct");
}

TEST(test_string_range_query) {
    BPlusTree<std::string, int> tree(8);
    
    tree.insert("apple", 1);
    tree.insert("apricot", 2);
    tree.insert("banana", 3);
    tree.insert("cherry", 4);
    
    std::vector<int> results = tree.range_query("apple", "banana");
    
    ASSERT(results.size() == 3, "String range query should return 3 elements");
}

TEST(test_clear) {
    BPlusTree<int, int> tree(4);
    
    for (int i = 0; i < 100; ++i) {
        tree.insert(i, i * 10);
    }
    
    tree.clear();
    
    ASSERT(tree.size() == 0, "Tree should be empty after clear");
    ASSERT(tree.empty(), "Tree should be empty");
    
    int value = 0;
    bool found = tree.search(50, value);
    ASSERT(!found, "No keys should be found after clear");
}

TEST(test_statistics) {
    BPlusTree<int, int> tree(16);
    
    for (int i = 0; i < 1000; ++i) {
        tree.insert(i, i * 10);
    }
    
    BPlusTree<int, int>::Stats stats = tree.get_stats();
    
    ASSERT(stats.num_keys == 1000, "Stats should report correct key count");
    ASSERT(stats.num_leaf_nodes > 0, "Stats should report leaf nodes");
    ASSERT(stats.height > 0, "Stats should report positive height");
    
    std::cout << "\n  Stats: " << stats.num_keys << " keys, "
              << stats.num_leaf_nodes << " leaves, "
              << stats.num_internal_nodes << " internal, "
              << "height " << stats.height;
}

TEST(test_performance_benchmark) {
    BPlusTree<int, int> tree(64);
    const int N = 100000;
    
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        tree.insert(i, i * 10);
    }
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    double insert_time = std::chrono::duration<double, std::milli>(end - start).count();
    
    start = std::chrono::high_resolution_clock::now();
    int value = 0;
    volatile long long checksum = 0;
    for (int i = 0; i < N; ++i) {
        if (tree.search(i, value)) {
            checksum += value;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    double search_time = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "\n  Performance: "
              << N << " inserts in " << insert_time << "ms, "
              << N << " searches in " << search_time << "ms"
              << ", checksum=" << checksum;
    
    ASSERT(tree.size() == (size_t)N, "All elements should be inserted");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "B+ Tree Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    test_empty_tree_runner();
    test_single_insert_runner();
    test_single_search_runner();
    test_search_not_found_runner();
    test_multiple_inserts_runner();
    test_duplicate_insert_runner();
    test_insert_descending_runner();
    test_remove_single_runner();
    test_remove_not_found_runner();
    test_remove_multiple_runner();
    
    std::cout << std::endl;
    std::cout << "Range Query Tests:" << std::endl;
    test_range_query_simple_runner();
    test_range_query_empty_runner();
    test_range_query_full_runner();
    
    std::cout << std::endl;
    std::cout << "Stress Tests:" << std::endl;
    test_large_sequential_insert_runner();
    test_large_random_insert_runner();
    test_insert_delete_cycle_runner();
    
    std::cout << std::endl;
    std::cout << "String Key Tests:" << std::endl;
    test_string_keys_runner();
    test_string_range_query_runner();
    
    std::cout << std::endl;
    std::cout << "Utility Tests:" << std::endl;
    test_clear_runner();
    test_statistics_runner();
    
    std::cout << std::endl;
    std::cout << "Performance Test:" << std::endl;
    test_performance_benchmark_runner();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    std::cout << "========================================" << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
