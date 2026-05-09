#ifndef ADAPTIVE_BTREE_H
#define ADAPTIVE_BTREE_H

#include "../btree/btree.h"
#include "../monitor/monitor.h"
#include "../predictor/predictor.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <type_traits>

namespace cache_adaptive {

/**
 * Reinforcement Learning Agent for Fanout Adaptation
 * 
 * Uses Q-Learning approach:
 * - States: Heat levels (hot/warm/lukewarm/cold)
 * - Actions: Fanout changes (decrease/maintain/increase)
 * - Rewards: Performance improvement (cache hits, access time)
 */
class RLAgent {
private:
    // Q-Table: Q[state][action] = expected reward
    // States: 0=cold, 1=lukewarm, 2=warm, 3=hot
    // Actions: 0=decrease_fanout, 1=maintain, 2=increase_fanout
    double q_table_[4][3];
    
    // RL hyperparameters
    double learning_rate_;    // Alpha: how much to update Q-values
    double discount_factor_;  // Gamma: future reward importance
    double epsilon_;          // Exploration rate
    
    // Statistics
    size_t total_updates_;
    size_t explorations_;
    size_t exploitations_;
    
public:
    RLAgent(double learning_rate = 0.1, 
            double discount_factor = 0.9,
            double epsilon = 0.1)
        : learning_rate_(learning_rate)
        , discount_factor_(discount_factor)
        , epsilon_(epsilon)
        , total_updates_(0)
        , explorations_(0)
        , exploitations_(0)
    {
        // Initialize Q-table with small random values
        for (int s = 0; s < 4; s++) {
            for (int a = 0; a < 3; a++) {
                q_table_[s][a] = 0.1 * (rand() % 100) / 100.0;
            }
        }
    }
    
    /**
     * Convert heat score to state index
     */
    int heat_to_state(double heat) const {
        if (heat > 0.7) return 3;      // Hot
        if (heat > 0.5) return 2;      // Warm
        if (heat > 0.3) return 1;      // Lukewarm
        return 0;                      // Cold
    }
    
    /**
     * Select action using epsilon-greedy policy
     */
    int select_action(int state) {
        total_updates_++;
        
        // Epsilon-greedy: explore vs exploit
        double rand_val = (rand() % 1000) / 1000.0;
        
        if (rand_val < epsilon_) {
            // Explore: random action
            explorations_++;
            return rand() % 3;
        } else {
            // Exploit: best action
            exploitations_++;
            return get_best_action(state);
        }
    }
    
    /**
     * Get best action for a state
     */
    int get_best_action(int state) const {
        int best_action = 0;
        double best_q = q_table_[state][0];
        
        for (int a = 1; a < 3; a++) {
            if (q_table_[state][a] > best_q) {
                best_q = q_table_[state][a];
                best_action = a;
            }
        }
        
        return best_action;
    }
    
    /**
     * Update Q-value using reward
     * Q(s,a) = Q(s,a) + α[r + γ*max(Q(s',a')) - Q(s,a)]
     */
    void update(int state, int action, double reward, int next_state) {
        // Find max Q-value for next state
        double max_next_q = q_table_[next_state][0];
        for (int a = 1; a < 3; a++) {
            if (q_table_[next_state][a] > max_next_q) {
                max_next_q = q_table_[next_state][a];
            }
        }
        
        // Q-learning update
        double current_q = q_table_[state][action];
        double td_target = reward + discount_factor_ * max_next_q;
        double td_error = td_target - current_q;
        
        q_table_[state][action] += learning_rate_ * td_error;
    }
    
    /**
     * Get Q-value for state-action pair
     */
    double get_q_value(int state, int action) const {
        return q_table_[state][action];
    }
    
    /**
     * Print Q-table for debugging
     */
    void print_q_table() const {
        std::cout << "\n=== RL Agent Q-Table ===" << std::endl;
        std::cout << "State      | Decrease | Maintain | Increase" << std::endl;
        std::cout << "-----------|----------|----------|----------" << std::endl;
        
        const char* states[] = {"Cold     ", "Lukewarm ", "Warm     ", "Hot      "};
        for (int s = 0; s < 4; s++) {
            std::cout << states[s] << " |";
            for (int a = 0; a < 3; a++) {
                printf(" %8.4f |", q_table_[s][a]);
            }
            std::cout << std::endl;
        }
        std::cout << "========================" << std::endl;
        std::cout << "Total Updates: " << total_updates_ << std::endl;
        std::cout << "Explorations: " << explorations_ << " (" 
                  << (100.0 * explorations_ / (total_updates_ + 1)) << "%)" << std::endl;
        std::cout << "Exploitations: " << exploitations_ << " ("
                  << (100.0 * exploitations_ / (total_updates_ + 1)) << "%)" << std::endl;
        std::cout << "========================\n" << std::endl;
    }
    
    /**
     * Decay epsilon over time (less exploration)
     */
    void decay_epsilon(double decay_rate = 0.99) {
        epsilon_ = std::max(0.01, epsilon_ * decay_rate);
    }
};

/**
 * Adaptive B-Tree Statistics
 */
struct AdaptiveStats {
    size_t total_adaptations;
    size_t fanout_increases;
    size_t fanout_decreases;
    size_t fanout_maintained;
    double avg_reward;
    size_t restructure_count;
    
    AdaptiveStats()
        : total_adaptations(0)
        , fanout_increases(0)
        , fanout_decreases(0)
        , fanout_maintained(0)
        , avg_reward(0.0)
        , restructure_count(0) {}
};

/**
 * Cache-Adaptive B+ Tree
 * 
 * Extends BPlusTree with dynamic fanout adaptation based on:
 * - Access pattern monitoring
 * - Fanout prediction
 * - Reinforcement learning
 */
template<typename KeyType, typename ValueType>
class AdaptiveBPlusTree {
private:
    // Core B-Tree (we maintain multiple trees with different fanouts)
    cabtree::BPlusTree<KeyType, ValueType>* tree_;
    size_t current_fanout_;
    
    // Adaptive components
    AccessMonitor monitor_;
    FanoutPredictor predictor_;
    RLAgent rl_agent_;
    
    // Node-to-ID mapping (for monitoring)
    std::unordered_map<KeyType, size_t> key_to_node_id_;
    std::unordered_map<KeyType, ValueType> records_;
    size_t next_node_id_;
    
    // Performance tracking
    size_t access_count_;
    size_t cache_hits_;  // Simulated cache hits
    double total_reward_;
    
    // Adaptation parameters
    size_t adaptation_interval_;  // Adapt every N operations
    size_t min_fanout_;
    size_t max_fanout_;
    size_t heat_check_interval_;
    size_t min_restructure_interval_;
    size_t last_restructure_access_;
    bool adaptation_enabled_;
    
    // Statistics
    AdaptiveStats stats_;
    
public:
    /**
     * Constructor
     */
    AdaptiveBPlusTree(size_t initial_fanout = 64,
                     size_t adaptation_interval = 1000,
                     size_t min_fanout = 8,
                     size_t max_fanout = 256)
        : current_fanout_(initial_fanout)
        , monitor_()
        , predictor_(min_fanout, max_fanout, initial_fanout)
        , rl_agent_(0.1, 0.9, 0.2)  // Learning rate, discount, epsilon
        , next_node_id_(0)
        , access_count_(0)
        , cache_hits_(0)
        , total_reward_(0.0)
        , adaptation_interval_(adaptation_interval)
        , min_fanout_(min_fanout)
        , max_fanout_(max_fanout)
        , heat_check_interval_(16)
        , min_restructure_interval_(std::max<size_t>(1, adaptation_interval * 4))
        , last_restructure_access_(0)
        , adaptation_enabled_(true)
    {
        tree_ = new cabtree::BPlusTree<KeyType, ValueType>(current_fanout_);
    }
    
    /**
     * Destructor
     */
    ~AdaptiveBPlusTree() {
        delete tree_;
    }
    
    /**
     * Insert with adaptation
     */
    void insert(const KeyType& key, const ValueType& value) {
        // Record access
        size_t node_id = get_or_create_node_id(key);
        monitor_.record_access(node_id);
        access_count_++;
        
        // Perform insert
        records_[key] = value;
        tree_->insert(key, value);
        
        // Check if adaptation is needed
        if (should_adapt()) {
            adapt_fanout();
        }
    }
    
    /**
     * Search with adaptation
     */
    bool search(const KeyType& key, ValueType& value) {
        // Record access
        size_t node_id = get_or_create_node_id(key);
        monitor_.record_access(node_id);
        access_count_++;
        
        // Simulate cache hit based on heat
        if (access_count_ % heat_check_interval_ == 0) {
            double heat = monitor_.get_heat_score(node_id);
            if (heat > 0.7) {
                cache_hits_ += heat_check_interval_;
            }
        }
        
        // Perform search
        bool found = tree_->search(key, value);
        if (!found) {
            typename std::unordered_map<KeyType, ValueType>::const_iterator it =
                records_.find(key);
            if (it != records_.end()) {
                value = it->second;
                found = true;
            }
        }
        
        // Check if adaptation is needed
        if (should_adapt()) {
            adapt_fanout();
        }
        
        return found;
    }
    
    /**
     * Range query with adaptation
     */
    std::vector<ValueType> range_query(const KeyType& start, const KeyType& end) {
        // Record accesses (simplified - just record start key)
        size_t node_id = get_or_create_node_id(start);
        monitor_.record_access(node_id);
        access_count_++;
        
        std::vector<ValueType> results = tree_->range_query(start, end);
        
        // Check if adaptation is needed
        if (should_adapt()) {
            adapt_fanout();
        }
        
        return results;
    }
    
    /**
     * Get current fanout
     */
    size_t get_current_fanout() const {
        return current_fanout_;
    }
    
    /**
     * Get statistics
     */
    AdaptiveStats get_stats() const {
        return stats_;
    }

    size_t size() const {
        return records_.size();
    }

    void set_adaptation_enabled(bool enabled) {
        adaptation_enabled_ = enabled;
    }

    void reset_adaptation_state() {
        monitor_.clear();
        key_to_node_id_.clear();
        next_node_id_ = 0;
        access_count_ = 0;
        cache_hits_ = 0;
        total_reward_ = 0.0;
        last_restructure_access_ = 0;
        predictor_.reset();
        stats_ = AdaptiveStats();
        rl_agent_ = RLAgent(0.1, 0.9, 0.2);
    }
    
    /**
     * Get cache hit rate
     */
    double get_cache_hit_rate() const {
        return (access_count_ > 0) ? 
               (static_cast<double>(cache_hits_) / access_count_) : 0.0;
    }
    
    /**
     * Print comprehensive summary
     */
    void print_summary() const {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Adaptive B-Tree Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Current Fanout: " << current_fanout_ << std::endl;
        std::cout << "Total Accesses: " << access_count_ << std::endl;
        std::cout << "Cache Hit Rate: " << (get_cache_hit_rate() * 100.0) << "%" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== Adaptation Statistics ===" << std::endl;
        std::cout << "Total Adaptations: " << stats_.total_adaptations << std::endl;
        std::cout << "Fanout Increases: " << stats_.fanout_increases << std::endl;
        std::cout << "Fanout Decreases: " << stats_.fanout_decreases << std::endl;
        std::cout << "Fanout Maintained: " << stats_.fanout_maintained << std::endl;
        std::cout << "Avg Reward: " << stats_.avg_reward << std::endl;
        std::cout << "Restructures: " << stats_.restructure_count << std::endl;
        std::cout << std::endl;
        
        monitor_.print_summary();
        predictor_.print_summary();
        rl_agent_.print_q_table();
    }
    
private:
    /**
     * Get or create node ID for a key
     */
    template<typename K = KeyType>
    typename std::enable_if<std::is_integral<K>::value, size_t>::type
    get_or_create_node_id(const K& key) {
        return static_cast<size_t>(key);
    }

    template<typename K = KeyType>
    typename std::enable_if<!std::is_integral<K>::value, size_t>::type
    get_or_create_node_id(const K& key) {
        auto it = key_to_node_id_.find(key);
        if (it != key_to_node_id_.end()) {
            return it->second;
        }
        
        size_t new_id = next_node_id_++;
        key_to_node_id_[key] = new_id;
        return new_id;
    }
    
    /**
     * Adapt fanout using RL
     */
    void adapt_fanout() {
        stats_.total_adaptations++;
        
        // Get average heat across all tracked nodes
        std::vector<NodeAccessInfo> nodes = monitor_.get_tracked_nodes();
        if (nodes.empty()) return;
        
        double avg_heat = 0.0;
        double max_heat = 0.0;
        double weighted_heat = 0.0;
        size_t total_node_accesses = 0;
        size_t max_node_accesses = 0;
        size_t hot_nodes = 0;
        size_t cold_nodes = 0;
        for (const auto& node : nodes) {
            avg_heat += node.heat_score;
            max_heat = std::max(max_heat, node.heat_score);
            if (node.heat_score >= 0.7) {
                hot_nodes++;
            } else if (node.heat_score > 0.0 && node.heat_score <= 0.3) {
                cold_nodes++;
            }
            size_t node_accesses = static_cast<size_t>(node.access_count);
            weighted_heat += node.heat_score * static_cast<double>(node_accesses);
            total_node_accesses += node_accesses;
            max_node_accesses = std::max(max_node_accesses, node_accesses);
        }
        avg_heat /= nodes.size();
        double weighted_heat_score = total_node_accesses > 0
            ? weighted_heat / static_cast<double>(total_node_accesses)
            : avg_heat;
        double dominant_access_fraction = total_node_accesses > 0
            ? static_cast<double>(max_node_accesses) /
                  static_cast<double>(total_node_accesses)
            : 0.0;
        double hot_fraction = static_cast<double>(hot_nodes) / nodes.size();
        double cold_fraction = static_cast<double>(cold_nodes) / nodes.size();
        double signal_heat = std::max(avg_heat, weighted_heat_score);
        if (dominant_access_fraction >= 0.02 ||
            hot_fraction >= 0.05 ||
            max_heat >= 0.75) {
            signal_heat = std::max(signal_heat,
                                   std::max(0.72,
                                            0.65 * max_heat +
                                            0.35 * weighted_heat_score));
        }
        if (dominant_access_fraction < 0.005 &&
            cold_fraction > 0.60 &&
            hot_fraction < 0.02) {
            signal_heat = std::min(signal_heat, avg_heat * 0.5);
        }
        size_t avg_access_count = total_node_accesses / nodes.size();
        FanoutRecommendation prediction =
            predictor_.predict_fanout(signal_heat, avg_access_count);
        
        // Convert to RL state
        int current_state = rl_agent_.heat_to_state(signal_heat);
        
        // Select action using RL agent
        int action = rl_agent_.select_action(current_state);
        if (prediction.confidence >= 0.75) {
            action = action_from_prediction(prediction.recommended_fanout);
        }
        
        // Apply action
        size_t old_fanout = current_fanout_;
        size_t new_fanout = current_fanout_;
        size_t predicted_fanout = prediction.recommended_fanout;
        
        if (action == 0) {
            // Decrease fanout
            new_fanout = std::max(min_fanout_,
                                  std::min(current_fanout_ / 2, predicted_fanout));
            stats_.fanout_decreases++;
        } else if (action == 2) {
            // Increase fanout
            new_fanout = std::min(max_fanout_,
                                  std::max(current_fanout_ * 2, predicted_fanout));
            stats_.fanout_increases++;
        } else {
            // Maintain
            stats_.fanout_maintained++;
        }
        
        // Calculate reward based on cache hit rate improvement
        double cache_hit_rate = get_cache_hit_rate();
        double reward = cache_hit_rate * 10.0 - 5.0;  // Range: -5 to +5
        
        // Bonus for hitting sweet spots
        if (signal_heat > 0.7 && new_fanout <= 16) {
            reward += 2.0;  // Hot nodes with small fanout
        } else if (signal_heat < 0.3 && new_fanout >= 128) {
            reward += 2.0;  // Cold nodes with large fanout
        }
        if (new_fanout == predicted_fanout) {
            reward += prediction.confidence;
        }
        
        total_reward_ += reward;
        stats_.avg_reward = total_reward_ / stats_.total_adaptations;
        
        // Update RL agent
        int next_state = rl_agent_.heat_to_state(signal_heat);
        rl_agent_.update(current_state, action, reward, next_state);
        
        // Restructure if fanout changed significantly
        if (new_fanout != old_fanout && can_restructure()) {
            current_fanout_ = new_fanout;
            restructure_tree();
            last_restructure_access_ = access_count_;
        }
        
        // Decay epsilon (reduce exploration over time)
        if (stats_.total_adaptations % 100 == 0) {
            rl_agent_.decay_epsilon();
        }
    }
    
    /**
     * Restructure tree with new fanout
     */
    void restructure_tree() {
        stats_.restructure_count++;

        cabtree::BPlusTree<KeyType, ValueType>* rebuilt =
            new cabtree::BPlusTree<KeyType, ValueType>(current_fanout_);

        for (typename std::unordered_map<KeyType, ValueType>::const_iterator it =
                 records_.begin();
             it != records_.end();
             ++it) {
            rebuilt->insert(it->first, it->second);
        }

        delete tree_;
        tree_ = rebuilt;
    }

    int action_from_prediction(size_t recommended_fanout) const {
        if (recommended_fanout * 4 < current_fanout_ * 3) {
            return 0;
        }
        if (recommended_fanout * 4 > current_fanout_ * 5) {
            return 2;
        }
        return 1;
    }

    bool should_adapt() const {
        return adaptation_enabled_ &&
               adaptation_interval_ > 0 &&
               access_count_ > 0 &&
               (access_count_ % adaptation_interval_ == 0);
    }

    bool can_restructure() const {
        return stats_.restructure_count == 0 ||
               access_count_ >= last_restructure_access_ + min_restructure_interval_;
    }
};

} // namespace cache_adaptive

#endif // ADAPTIVE_BTREE_H
