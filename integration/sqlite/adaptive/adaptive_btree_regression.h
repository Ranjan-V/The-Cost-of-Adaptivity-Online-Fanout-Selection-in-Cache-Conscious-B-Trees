#ifndef ADAPTIVE_BTREE_REGRESSION_H
#define ADAPTIVE_BTREE_REGRESSION_H

#include "../btree/btree.h"
#include "../monitor/monitor.h"
#include "../predictor/predictor.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>

namespace cache_adaptive {

class RegressionAgent {
private:
    double w0_, w1_, w2_;
    double learning_rate_;
    std::vector<double> heat_history_, reward_history_;
    size_t updates_;
    double total_reward_;
    
public:
    explicit RegressionAgent(double learning_rate = 0.01)
        : w0_(64.0), w1_(-50.0), w2_(0.001), learning_rate_(learning_rate)
        , updates_(0), total_reward_(0.0)
    {
        heat_history_.reserve(1000);
        reward_history_.reserve(1000);
    }
    
    double predict_fanout(double heat, size_t access_count) const {
        double predicted = w0_ + w1_ * heat + w2_ * access_count;
        return std::max(8.0, std::min(256.0, predicted));
    }
    
    int select_action(double heat, size_t access_count, size_t current_fanout) {
        double predicted = predict_fanout(heat, access_count);
        if (predicted < current_fanout * 0.75) return 0;
        else if (predicted > current_fanout * 1.25) return 2;
        else return 1;
    }
    
    void update(double heat, size_t /* access_count */, double reward) {
        updates_++;
        total_reward_ += reward;
        
        if (heat_history_.size() < 1000) {
            heat_history_.push_back(heat);
            reward_history_.push_back(reward);
        }
        
        double gradient_w1 = -heat * reward * 0.1;
        w1_ += learning_rate_ * gradient_w1;
        w1_ = std::max(-100.0, std::min(-10.0, w1_));
        
        if (updates_ % 100 == 0 && !reward_history_.empty()) {
            double avg_reward = total_reward_ / updates_;
            if (avg_reward < 0) w0_ += 1.0;
            else if (avg_reward > 2.0) w0_ -= 1.0;
            w0_ = std::max(32.0, std::min(128.0, w0_));
        }
    }
    
    void print_model() const {
        std::cout << "\n=== Regression Agent Model ===" << std::endl;
        std::cout << "Linear Model: fanout = " << w0_ << " + (" << w1_ 
                  << ")*heat + (" << w2_ << ")*access_count" << std::endl;
        std::cout << "Updates: " << updates_ << std::endl;
        std::cout << "Avg Reward: " << (updates_ > 0 ? total_reward_ / updates_ : 0.0) << std::endl;
        std::cout << "Training Samples: " << heat_history_.size() << std::endl;
        std::cout << "\nSample Predictions:" << std::endl;
        std::cout << "  Heat=0.1 (cold)  -> Fanout=" << predict_fanout(0.1, 100) << std::endl;
        std::cout << "  Heat=0.5 (warm)  -> Fanout=" << predict_fanout(0.5, 100) << std::endl;
        std::cout << "  Heat=0.9 (hot)   -> Fanout=" << predict_fanout(0.9, 100) << std::endl;
        std::cout << "==============================\n" << std::endl;
    }
};

struct AdaptiveStatsRegression {
    size_t total_adaptations, fanout_increases, fanout_decreases, fanout_maintained, restructure_count;
    double avg_reward;
    AdaptiveStatsRegression() : total_adaptations(0), fanout_increases(0), fanout_decreases(0)
        , fanout_maintained(0), restructure_count(0), avg_reward(0.0) {}
};

template<typename KeyType, typename ValueType>
class AdaptiveBPlusTreeRegression {
private:
    cabtree::BPlusTree<KeyType, ValueType>* tree_;
    size_t current_fanout_, next_node_id_, access_count_, cache_hits_;
    size_t adaptation_interval_, min_fanout_, max_fanout_;
    double total_reward_;
    AccessMonitor monitor_;
    FanoutPredictor predictor_;
    RegressionAgent regression_agent_;
    std::unordered_map<KeyType, size_t> key_to_node_id_;
    AdaptiveStatsRegression stats_;
    
public:
    AdaptiveBPlusTreeRegression(size_t initial_fanout = 64, size_t adaptation_interval = 1000,
                               size_t min_fanout = 8, size_t max_fanout = 256)
        : tree_(new cabtree::BPlusTree<KeyType, ValueType>(initial_fanout))
        , current_fanout_(initial_fanout), next_node_id_(0)
        , access_count_(0), cache_hits_(0)
        , adaptation_interval_(adaptation_interval)
        , min_fanout_(min_fanout), max_fanout_(max_fanout)
        , total_reward_(0.0), monitor_()
        , predictor_(min_fanout, max_fanout, initial_fanout)
        , regression_agent_(0.01) {}
    
    ~AdaptiveBPlusTreeRegression() { delete tree_; }
    
    void insert(const KeyType& key, const ValueType& value) {
        size_t node_id = get_or_create_node_id(key);
        monitor_.record_access(node_id);
        access_count_++;
        tree_->insert(key, value);
        if (access_count_ % adaptation_interval_ == 0) adapt_fanout();
    }
    
    bool search(const KeyType& key, ValueType& value) {
        size_t node_id = get_or_create_node_id(key);
        monitor_.record_access(node_id);
        access_count_++;
        if (monitor_.get_heat_score(node_id) > 0.7) cache_hits_++;
        bool found = tree_->search(key, value);
        if (access_count_ % adaptation_interval_ == 0) adapt_fanout();
        return found;
    }
    
    std::vector<ValueType> range_query(const KeyType& start, const KeyType& end) {
        monitor_.record_access(get_or_create_node_id(start));
        access_count_++;
        std::vector<ValueType> results = tree_->range_query(start, end);
        if (access_count_ % adaptation_interval_ == 0) adapt_fanout();
        return results;
    }
    
    size_t get_current_fanout() const { return current_fanout_; }
    AdaptiveStatsRegression get_stats() const { return stats_; }
    double get_cache_hit_rate() const {
        return (access_count_ > 0) ? static_cast<double>(cache_hits_) / access_count_ : 0.0;
    }
    
    void print_summary() const {
        std::cout << "\n========================================\n"
                  << "  Adaptive B-Tree (Regression) Summary\n"
                  << "========================================\n"
                  << "Current Fanout: " << current_fanout_ << "\n"
                  << "Total Accesses: " << access_count_ << "\n"
                  << "Cache Hit Rate: " << (get_cache_hit_rate() * 100.0) << "%\n\n"
                  << "=== Adaptation Statistics ===\n"
                  << "Total Adaptations: " << stats_.total_adaptations << "\n"
                  << "Fanout Increases: " << stats_.fanout_increases << "\n"
                  << "Fanout Decreases: " << stats_.fanout_decreases << "\n"
                  << "Fanout Maintained: " << stats_.fanout_maintained << "\n"
                  << "Avg Reward: " << stats_.avg_reward << "\n"
                  << "Restructures: " << stats_.restructure_count << "\n" << std::endl;
        monitor_.print_summary();
        predictor_.print_summary();
        regression_agent_.print_model();
    }
    
private:
    size_t get_or_create_node_id(const KeyType& key) {
        auto it = key_to_node_id_.find(key);
        if (it != key_to_node_id_.end()) return it->second;
        return key_to_node_id_[key] = next_node_id_++;
    }
    
    void adapt_fanout() {
        stats_.total_adaptations++;
        std::vector<NodeAccessInfo> nodes = monitor_.get_tracked_nodes();
        if (nodes.empty()) return;
        
        double avg_heat = 0.0;
        size_t total_count = 0;
        for (const auto& node : nodes) {
            avg_heat += node.heat_score;
            total_count += node.access_count;
        }
        avg_heat /= nodes.size();
        
        int action = regression_agent_.select_action(avg_heat, total_count / nodes.size(), current_fanout_);
        size_t new_fanout = current_fanout_;
        
        if (action == 0) {
            new_fanout = std::max(min_fanout_, current_fanout_ / 2);
            stats_.fanout_decreases++;
        } else if (action == 2) {
            new_fanout = std::min(max_fanout_, current_fanout_ * 2);
            stats_.fanout_increases++;
        } else {
            stats_.fanout_maintained++;
        }
        
        double reward = get_cache_hit_rate() * 10.0 - 5.0;
        if (avg_heat > 0.7 && new_fanout <= 16) reward += 2.0;
        else if (avg_heat < 0.3 && new_fanout >= 128) reward += 2.0;
        
        total_reward_ += reward;
        stats_.avg_reward = total_reward_ / stats_.total_adaptations;
        regression_agent_.update(avg_heat, total_count / nodes.size(), reward);
        
        if (new_fanout != current_fanout_) {
            current_fanout_ = new_fanout;
            stats_.restructure_count++;
            delete tree_;
            tree_ = new cabtree::BPlusTree<KeyType, ValueType>(current_fanout_);
        }
    }
};

} // namespace cache_adaptive

#endif // ADAPTIVE_BTREE_REGRESSION_H