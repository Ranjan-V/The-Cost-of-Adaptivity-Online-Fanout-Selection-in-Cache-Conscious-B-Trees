#include "monitor/monitor.h"
#include "predictor/predictor.h"
#include <iostream>
#include <iomanip>

using namespace cache_adaptive;

/**
 * Demonstrate Monitor + Predictor integration
 */
int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Module 2 + 3 Integration Demo" << std::endl;
    std::cout << "  Monitor + Predictor Working Together" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Initialize components
    AccessMonitor monitor;
    FanoutPredictor predictor(8, 256, 64, 0.3);
    
    // Simulate access patterns for different nodes
    std::cout << "Simulating access patterns...\n" << std::endl;
    
    // Node 1: HOT (1000 accesses)
    for (int i = 0; i < 1000; i++) {
        monitor.record_access(1);
    }
    
    // Node 2: WARM (200 accesses)
    for (int i = 0; i < 200; i++) {
        monitor.record_access(2);
    }
    
    // Node 3: LUKEWARM (50 accesses)
    for (int i = 0; i < 50; i++) {
        monitor.record_access(3);
    }
    
    // Node 4: COLD (5 accesses)
    for (int i = 0; i < 5; i++) {
        monitor.record_access(4);
    }
    
    // Node 5: NEW (0 accesses yet, just discovered)
    // Don't access it yet
    
    // Display monitoring results
    std::cout << "=== Access Monitoring Results ===" << std::endl;
    MonitorStats mon_stats = monitor.get_stats();
    std::cout << "Total Accesses: " << mon_stats.total_accesses << std::endl;
    std::cout << "Unique Nodes: " << mon_stats.unique_nodes << std::endl;
    std::cout << "Hot Nodes: " << mon_stats.hot_nodes_count << std::endl;
    std::cout << "Cold Nodes: " << mon_stats.cold_nodes_count << std::endl;
    std::cout << std::endl;
    
    // Get fanout recommendations for each node
    std::cout << "=== Fanout Recommendations ===" << std::endl;
    std::cout << std::left << std::setw(10) << "Node ID" 
              << std::setw(12) << "Accesses"
              << std::setw(12) << "Heat"
              << std::setw(12) << "Fanout"
              << std::setw(15) << "Confidence"
              << "Reasoning" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    std::vector<size_t> node_ids = {1, 2, 3, 4, 5};
    
    for (size_t node_id : node_ids) {
        size_t access_count = monitor.get_access_count(node_id);
        double heat = monitor.get_heat_score(node_id);
        
        // Get fanout recommendation
        FanoutRecommendation rec = predictor.predict_fanout(heat, access_count);
        
        // Display results
        std::cout << std::left << std::setw(10) << node_id
                  << std::setw(12) << access_count
                  << std::fixed << std::setprecision(3)
                  << std::setw(12) << heat
                  << std::setw(12) << rec.recommended_fanout
                  << std::setw(15) << rec.confidence
                  << rec.reasoning << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show hottest nodes
    std::cout << "=== Hottest Nodes (Top 3) ===" << std::endl;
    std::vector<NodeAccessInfo> hottest = monitor.get_hottest_nodes(3);
    for (size_t i = 0; i < hottest.size(); i++) {
        std::cout << "#" << (i+1) << " Node " << hottest[i].node_id
                  << ": " << hottest[i].access_count << " accesses, "
                  << "heat=" << std::fixed << std::setprecision(3) 
                  << hottest[i].heat_score << std::endl;
    }
    std::cout << std::endl;
    
    // Simulate workload shift (Node 4 becomes hot)
    std::cout << "=== Simulating Workload Shift ===" << std::endl;
    std::cout << "Node 4 suddenly becomes hot (1000 more accesses)...\n" << std::endl;
    
    for (int i = 0; i < 1000; i++) {
        monitor.record_access(4);
    }
    
    // Get new recommendation for Node 4
    size_t new_access_count = monitor.get_access_count(4);
    double new_heat = monitor.get_heat_score(4);
    FanoutRecommendation new_rec = predictor.predict_fanout(new_heat, new_access_count);
    
    std::cout << "Node 4 Update:" << std::endl;
    std::cout << "  Accesses: " << new_access_count << std::endl;
    std::cout << "  Heat: " << new_heat << std::endl;
    std::cout << "  NEW Fanout: " << new_rec.recommended_fanout << std::endl;
    std::cout << "  Confidence: " << new_rec.confidence << std::endl;
    std::cout << "  Reasoning: " << new_rec.reasoning << std::endl;
    std::cout << std::endl;
    
    // Print component summaries
    monitor.print_summary();
    predictor.print_summary();
    
    // Demonstrate batch processing
    std::cout << "=== Batch Processing Demo ===" << std::endl;
    
    std::vector<NodeAccessInfo> all_nodes = monitor.get_tracked_nodes();
    std::vector<double> heats;
    std::vector<size_t> counts;
    
    for (const auto& node : all_nodes) {
        heats.push_back(node.heat_score);
        counts.push_back(node.access_count);
    }
    
    // Reset predictor for fair batch test
    predictor.reset();
    std::vector<FanoutRecommendation> batch_recs = predictor.predict_batch(heats, counts);
    
    std::cout << "Processed " << batch_recs.size() << " nodes in one batch" << std::endl;
    
    size_t total_fanout = 0;
    for (const auto& rec : batch_recs) {
        total_fanout += rec.recommended_fanout;
    }
    std::cout << "Average recommended fanout: " 
              << (total_fanout / batch_recs.size()) << std::endl;
    std::cout << std::endl;
    
    // Success message
    std::cout << "========================================" << std::endl;
    std::cout << "  OK Integration Demo Complete!" << std::endl;
    std::cout << "  Monitor and Predictor working together" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return 0;
}
