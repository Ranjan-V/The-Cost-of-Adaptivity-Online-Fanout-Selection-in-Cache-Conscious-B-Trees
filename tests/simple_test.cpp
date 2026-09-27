#include "btree/btree.h"
#include "monitor/monitor.h"
#include <iostream>

// B-Tree uses cabtree namespace
using namespace cabtree;
// Monitor uses cache_adaptive namespace
using cache_adaptive::AccessMonitor;

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Quick Sanity Check" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Test B-Tree
    std::cout << "[1/2] Testing B+ Tree..." << std::endl;
    BPlusTree<int, int> tree(64);
    
    // Insert some values
    for (int i = 1; i <= 10; i++) {
        tree.insert(i, i * 100);
    }
    
    // Search for a value
    int value = 0;
    bool found = tree.search(5, value);
    
    if (found && value == 500) {
        std::cout << "  OK B+ Tree is working!" << std::endl;
        std::cout << "    Inserted 10 values, found key=5 with value=500" << std::endl;
    } else {
        std::cout << "  ERROR B+ Tree has issues!" << std::endl;
        return 1;
    }
    
    // Test Monitor
    std::cout << "\n[2/2] Testing Access Monitor..." << std::endl;
    AccessMonitor monitor;
    
    // Record some accesses
    for (int i = 0; i < 100; i++) {
        monitor.record_access(1);
    }
    for (int i = 0; i < 10; i++) {
        monitor.record_access(2);
    }
    
    size_t count1 = monitor.get_access_count(1);
    double heat1 = monitor.get_heat_score(1);
    bool is_hot = monitor.is_hot_node(1);
    
    if (count1 == 100 && heat1 > 0.0 && is_hot) {
        std::cout << "  OK Access Monitor is working!" << std::endl;
        std::cout << "    Node 1: " << count1 << " accesses, heat=" << heat1 << std::endl;
    } else {
        std::cout << "  ERROR Access Monitor has issues!" << std::endl;
        return 1;
    }
    
    // Success
    std::cout << "\n========================================" << std::endl;
    std::cout << "  OK All Systems Operational" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return 0;
}
