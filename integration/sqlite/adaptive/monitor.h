#ifndef MONITOR_H
#define MONITOR_H

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace cache_adaptive {

/**
 * Statistics for monitored access patterns
 */
struct MonitorStats {
    size_t total_accesses;
    size_t unique_nodes;
    double avg_heat_score;
    size_t hot_nodes_count;   // Nodes with heat > 0.7
    size_t cold_nodes_count;  // Nodes with heat < 0.3
    
    MonitorStats() 
        : total_accesses(0)
        , unique_nodes(0)
        , avg_heat_score(0.0)
        , hot_nodes_count(0)
        , cold_nodes_count(0) {}
};

/**
 * Node access tracking information
 */
struct NodeAccessInfo {
    size_t node_id;
    size_t access_count;
    double heat_score;
    
    NodeAccessInfo(size_t id = 0, size_t count = 0, double heat = 0.0)
        : node_id(id), access_count(count), heat_score(heat) {}
};

/**
 * Access Pattern Monitor
 * 
 * Tracks node access patterns to identify hot (frequently accessed)
 * and cold (rarely accessed) nodes for cache-adaptive optimization.
 * 
 * Heat Score Calculation:
 * - Uses exponential decay: heat_i = access_i / (total_accesses^α)
 * - α (alpha) controls sensitivity (default 0.5)
 * - Normalizes to [0.0, 1.0] range
 */
class AccessMonitor {
private:
    // Access tracking
    std::unordered_map<size_t, size_t> access_counts_;
    size_t total_accesses_;
    size_t window_size_;
    double alpha_;  // Heat score decay factor
    
    // Statistics
    size_t hot_threshold_count_;
    size_t cold_threshold_count_;
    
public:
    /**
     * Constructor
     * @param window_size Number of accesses before recalculating heat scores
     * @param alpha Decay factor for heat calculation (0.0-1.0)
     */
    explicit AccessMonitor(size_t window_size = 10000, double alpha = 0.5)
        : total_accesses_(0)
        , window_size_(window_size)
        , alpha_(alpha)
        , hot_threshold_count_(0)
        , cold_threshold_count_(0) {
        
        if (alpha_ < 0.0) alpha_ = 0.0;
        if (alpha_ > 1.0) alpha_ = 1.0;
    }
    
    /**
     * Record an access to a node
     */
    void record_access(size_t node_id) {
        access_counts_[node_id]++;
        total_accesses_++;
        
        // Periodically recalculate heat scores to prevent overflow
        if (total_accesses_ % window_size_ == 0) {
            recalculate_thresholds();
        }
    }
    
    /**
     * Get access count for a specific node
     */
    size_t get_access_count(size_t node_id) const {
        auto it = access_counts_.find(node_id);
        return (it != access_counts_.end()) ? it->second : 0;
    }
    
    /**
     * Calculate heat score for a node (0.0 = cold, 1.0 = hot)
     */
    double get_heat_score(size_t node_id) const {
        auto it = access_counts_.find(node_id);
        if (it == access_counts_.end() || total_accesses_ == 0) {
            return 0.0;
        }
        
        // Heat score with exponential decay
        double raw_score = static_cast<double>(it->second) / 
                          std::pow(static_cast<double>(total_accesses_), alpha_);
        
        // Normalize to [0, 1] using sigmoid-like function
        return std::min(1.0, raw_score * 100.0);
    }
    
    /**
     * Check if a node is "hot" (frequently accessed)
     * Hot threshold: heat_score > 0.7
     */
    bool is_hot_node(size_t node_id) const {
        return get_heat_score(node_id) > 0.7;
    }
    
    /**
     * Check if a node is "cold" (rarely accessed)
     * Cold threshold: heat_score < 0.3
     */
    bool is_cold_node(size_t node_id) const {
        double heat = get_heat_score(node_id);
        return heat > 0.0 && heat < 0.3;
    }
    
    /**
     * Get list of hottest N nodes
     */
    std::vector<NodeAccessInfo> get_hottest_nodes(size_t n) const {
        std::vector<NodeAccessInfo> nodes;
        nodes.reserve(access_counts_.size());
        
        // Collect all nodes with their heat scores
        for (const auto& pair : access_counts_) {
            nodes.push_back(NodeAccessInfo(
                pair.first,
                pair.second,
                get_heat_score(pair.first)
            ));
        }
        
        // Sort by heat score (descending)
        std::sort(nodes.begin(), nodes.end(),
            [](const NodeAccessInfo& a, const NodeAccessInfo& b) {
                return a.heat_score > b.heat_score;
            });
        
        // Return top N
        if (nodes.size() > n) {
            nodes.resize(n);
        }
        
        return nodes;
    }
    
    /**
     * Get all tracked nodes with their information
     */
    std::vector<NodeAccessInfo> get_tracked_nodes() const {
        std::vector<NodeAccessInfo> nodes;
        nodes.reserve(access_counts_.size());
        
        for (const auto& pair : access_counts_) {
            nodes.push_back(NodeAccessInfo(
                pair.first,
                pair.second,
                get_heat_score(pair.first)
            ));
        }
        
        return nodes;
    }
    
    /**
     * Get monitoring statistics
     */
    MonitorStats get_stats() const {
        MonitorStats stats;
        stats.total_accesses = total_accesses_;
        stats.unique_nodes = access_counts_.size();
        
        if (access_counts_.empty()) {
            return stats;
        }
        
        // Calculate average heat score and count hot/cold nodes
        double total_heat = 0.0;
        for (const auto& pair : access_counts_) {
            double heat = get_heat_score(pair.first);
            total_heat += heat;
            
            if (heat > 0.7) {
                stats.hot_nodes_count++;
            } else if (heat < 0.3 && heat > 0.0) {
                stats.cold_nodes_count++;
            }
        }
        
        stats.avg_heat_score = total_heat / access_counts_.size();
        return stats;
    }
    
    /**
     * Clear all monitoring data
     */
    void clear() {
        access_counts_.clear();
        total_accesses_ = 0;
        hot_threshold_count_ = 0;
        cold_threshold_count_ = 0;
    }
    
    /**
     * Get total number of accesses recorded
     */
    size_t get_total_accesses() const {
        return total_accesses_;
    }
    
    /**
     * Get number of unique nodes being tracked
     */
    size_t get_unique_node_count() const {
        return access_counts_.size();
    }
    
    /**
     * Print monitoring summary
     */
    void print_summary() const {
        MonitorStats stats = get_stats();
        
        std::cout << "\n=== Access Monitor Summary ===" << std::endl;
        std::cout << "Total Accesses: " << stats.total_accesses << std::endl;
        std::cout << "Unique Nodes: " << stats.unique_nodes << std::endl;
        std::cout << "Average Heat: " << stats.avg_heat_score << std::endl;
        std::cout << "Hot Nodes (>0.7): " << stats.hot_nodes_count << std::endl;
        std::cout << "Cold Nodes (<0.3): " << stats.cold_nodes_count << std::endl;
        
        // Show top 5 hottest nodes
        std::vector<NodeAccessInfo> hot = get_hottest_nodes(5);
        if (!hot.empty()) {
            std::cout << "\nTop " << hot.size() << " Hottest Nodes:" << std::endl;
            for (size_t i = 0; i < hot.size(); i++) {
                std::cout << "  #" << (i+1) << " Node " << hot[i].node_id 
                         << ": " << hot[i].access_count << " accesses, "
                         << "heat=" << hot[i].heat_score << std::endl;
            }
        }
        std::cout << "==============================\n" << std::endl;
    }
    
private:
    /**
     * Recalculate hot/cold thresholds (called periodically)
     */
    void recalculate_thresholds() {
        hot_threshold_count_ = 0;
        cold_threshold_count_ = 0;
        
        for (const auto& pair : access_counts_) {
            double heat = get_heat_score(pair.first);
            if (heat > 0.7) {
                hot_threshold_count_++;
            } else if (heat < 0.3 && heat > 0.0) {
                cold_threshold_count_++;
            }
        }
    }
};

} // namespace cache_adaptive

#endif // MONITOR_H