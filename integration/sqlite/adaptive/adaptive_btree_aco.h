#ifndef ADAPTIVE_BTREE_ACO_H
#define ADAPTIVE_BTREE_ACO_H

#include "../btree/btree.h"
#include "../monitor/monitor.h"
#include "../predictor/predictor.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace cache_adaptive {

/**
 * Ant Colony Optimization Agent for Fanout Adaptation
 * 
 * Uses ACO (Swarm Intelligence) approach:
 * - Ants explore different fanout values
 * - Pheromone trails mark good solutions
 * - Best solutions attract more ants
 * - Inspired by real ant foraging behavior
 */
class ACOAgent {
private:
    // Pheromone matrix: [state][action]
    // States: 0=cold, 1=lukewarm, 2=warm, 3=hot
    // Actions: 0=decrease, 1=maintain, 2=increase
    double pheromone_[4][3];
    
    // ACO parameters
    double evaporation_rate_;  // Rho: how fast pheromone decays
    double alpha_;             // Pheromone importance
    double beta_;              // Heuristic importance
    double q_;                 // Pheromone deposit amount
    
    // Ant statistics
    size_t num_ants_;
    size_t iterations_;
    size_t total_updates_;
    
    // Best solution tracking
    int best_actions_[4];      // Best action for each state
    double best_reward_;
    
public:
    ACOAgent(size_t num_ants = 10,
             double evaporation = 0.1,
             double alpha = 1.0,
             double beta = 2.0)
        : evaporation_rate_(evaporation)
        , alpha_(alpha)
        , beta_(beta)
        , q_(1.0)
        , num_ants_(num_ants)
        , iterations_(0)
        , total_updates_(0)
        , best_reward_(-999.0)
    {
        // Initialize pheromone trails equally
        for (int s = 0; s < 4; s++) {
            for (int a = 0; a < 3; a++) {
                pheromone_[s][a] = 1.0;
            }
            best_actions_[s] = 1;  // Start with maintain
        }
    }
    
    /**
     * Convert heat to state
     */
    int heat_to_state(double heat) const {
        if (heat > 0.7) return 3;  // Hot
        if (heat > 0.5) return 2;  // Warm
        if (heat > 0.3) return 1;  // Lukewarm
        return 0;                  // Cold
    }
    
    /**
     * Calculate heuristic value for action
     * Heuristic: how promising is this action based on heat?
     */
    double get_heuristic(int state, int action) const {
        // Hot state: prefer decrease (action 0)
        // Cold state: prefer increase (action 2)
        // Warm/Lukewarm: prefer maintain (action 1)
        
        if (state == 3) {  // Hot
            if (action == 0) return 2.0;      // Decrease is good
            if (action == 1) return 1.0;      // Maintain is okay
            return 0.5;                        // Increase is bad
        } else if (state == 0) {  // Cold
            if (action == 2) return 2.0;      // Increase is good
            if (action == 1) return 1.0;      // Maintain is okay
            return 0.5;                        // Decrease is bad
        } else {  // Warm/Lukewarm
            if (action == 1) return 2.0;      // Maintain is good
            return 1.0;                        // Others are okay
        }
    }
    
    /**
     * Select action using ACO probability
     * P(action) ∝ [pheromone]^alpha * [heuristic]^beta
     */
    int select_action(int state) {
        total_updates_++;
        
        // Calculate probabilities for each action
        double probabilities[3];
        double sum = 0.0;
        
        for (int a = 0; a < 3; a++) {
            double pheromone_factor = pow(pheromone_[state][a], alpha_);
            double heuristic_factor = pow(get_heuristic(state, a), beta_);
            probabilities[a] = pheromone_factor * heuristic_factor;
            sum += probabilities[a];
        }
        
        // Normalize probabilities
        for (int a = 0; a < 3; a++) {
            probabilities[a] /= sum;
        }
        
        // Roulette wheel selection
        double r = (rand() % 1000) / 1000.0;
        double cumulative = 0.0;
        
        for (int a = 0; a < 3; a++) {
            cumulative += probabilities[a];
            if (r <= cumulative) {
                return a;
            }
        }
        
        return 1;  // Default: maintain
    }
    
    /**
     * Update pheromone trails
     */
    void update(int state, int action, double reward) {
        // Pheromone evaporation (all trails decay)
        for (int s = 0; s < 4; s++) {
            for (int a = 0; a < 3; a++) {
                pheromone_[s][a] *= (1.0 - evaporation_rate_);
                pheromone_[s][a] = std::max(0.1, pheromone_[s][a]);  // Min threshold
            }
        }
        
        // Deposit pheromone on successful path
        double deposit = q_ * std::max(0.0, reward);
        pheromone_[state][action] += deposit;
        
        // Cap maximum pheromone
        pheromone_[state][action] = std::min(10.0, pheromone_[state][action]);
        
        // Update best solution
        if (reward > best_reward_) {
            best_reward_ = reward;
            best_actions_[state] = action;
            
            // Extra pheromone boost for best solution
            pheromone_[state][action] += q_;
        }
        
        iterations_++;
    }
    
    /**
     * Get best action based on pheromone trails
     */
    int get_best_action(int state) const {
        int best = 0;
        double best_pheromone = pheromone_[state][0];
        
        for (int a = 1; a < 3; a++) {
            if (pheromone_[state][a] > best_pheromone) {
                best_pheromone = pheromone_[state][a];
                best = a;
            }
        }
        
        return best;
    }
    
    /**
     * Print pheromone trails (ant paths)
     */
    void print_pheromone_trails() const {
        std::cout << "\n=== ACO Agent Pheromone Trails ===" << std::endl;
        std::cout << "State      | Decrease | Maintain | Increase" << std::endl;
        std::cout << "-----------|----------|----------|----------" << std::endl;
        
        const char* states[] = {"Cold     ", "Lukewarm ", "Warm     ", "Hot      "};
        for (int s = 0; s < 4; s++) {
            std::cout << states[s] << " |";
            for (int a = 0; a < 3; a++) {
                printf(" %8.4f |", pheromone_[s][a]);
            }
            std::cout << std::endl;
        }
        std::cout << "===================================" << std::endl;
        std::cout << "Iterations: " << iterations_ << std::endl;
        std::cout << "Total Updates: " << total_updates_ << std::endl;
        std::cout << "Best Reward: " << best_reward_ << std::endl;
        std::cout << "Num Ants: " << num_ants_ << std::endl;
        std::cout << "Evaporation Rate: " << evaporation_rate_ << std::endl;
        std::cout << "===================================\n" << std::endl;
    }
};

/**
 * Adaptive B-Tree Statistics (ACO version)
 */
struct AdaptiveStatsACO {
    size_t total_adaptations;
    size_t fanout_increases;
    size_t fanout_decreases;
    size_t fanout_maintained;
    double avg_reward;
    size_t restructure_count;
    
    AdaptiveStatsACO()
        : total_adaptations(0)
        , fanout_increases(0)
        , fanout_decreases(0)
        , fanout_maintained(0)
        , avg_reward(0.0)
        , restructure_count(0) {}
};

/**
 * Cache-Adaptive B+ Tree (Ant Colony Optimization Version)
 */
template<typename KeyType, typename ValueType>
class AdaptiveBPlusTreeACO {
private:
    cabtree::BPlusTree<KeyType, ValueType>* tree_;
    size_t current_fanout_;
    
    AccessMonitor monitor_;
    FanoutPredictor predictor_;
    ACOAgent aco_agent_;
    
    std::unordered_map<KeyType, size_t> key_to_node_id_;
    size_t next_node_id_;
    
    size_t access_count_;
    size_t cache_hits_;
    double total_reward_;
    
    size_t adaptation_interval_;
    size_t min_fanout_;
    size_t max_fanout_;
    
    AdaptiveStatsACO stats_;
    
public:
    AdaptiveBPlusTreeACO(size_t initial_fanout = 64,
                        size_t adaptation_interval = 1000,
                        size_t min_fanout = 8,
                        size_t max_fanout = 256)
        : current_fanout_(initial_fanout)
        , monitor_()
        , predictor_(min_fanout, max_fanout, initial_fanout)
        , aco_agent_(10, 0.1, 1.0, 2.0)  // 10 ants, 0.1 evaporation
        , next_node_id_(0)
        , access_count_(0)
        , cache_hits_(0)
        , total_reward_(0.0)
        , adaptation_interval_(adaptation_interval)
        , min_fanout_(min_fanout)
        , max_fanout_(max_fanout)
    {
        tree_ = new cabtree::BPlusTree<KeyType, ValueType>(current_fanout_);
    }
    
    ~AdaptiveBPlusTreeACO() {
        delete tree_;
    }
    
    void insert(const KeyType& key, const ValueType& value) {
        size_t node_id = get_or_create_node_id(key);
        monitor_.record_access(node_id);
        access_count_++;
        
        tree_->insert(key, value);
        
        if (access_count_ % adaptation_interval_ == 0) {
            adapt_fanout();
        }
    }
    
    bool search(const KeyType& key, ValueType& value) {
        size_t node_id = get_or_create_node_id(key);
        monitor_.record_access(node_id);
        access_count_++;
        
        double heat = monitor_.get_heat_score(node_id);
        if (heat > 0.7) {
            cache_hits_++;
        }
        
        bool found = tree_->search(key, value);
        
        if (access_count_ % adaptation_interval_ == 0) {
            adapt_fanout();
        }
        
        return found;
    }
    
    std::vector<ValueType> range_query(const KeyType& start, const KeyType& end) {
        size_t node_id = get_or_create_node_id(start);
        monitor_.record_access(node_id);
        access_count_++;
        
        std::vector<ValueType> results = tree_->range_query(start, end);
        
        if (access_count_ % adaptation_interval_ == 0) {
            adapt_fanout();
        }
        
        return results;
    }
    
    size_t get_current_fanout() const { return current_fanout_; }
    AdaptiveStatsACO get_stats() const { return stats_; }
    
    double get_cache_hit_rate() const {
        return (access_count_ > 0) ? 
               (static_cast<double>(cache_hits_) / access_count_) : 0.0;
    }
    
    void print_summary() const {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Adaptive B-Tree (ACO) Summary" << std::endl;
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
        aco_agent_.print_pheromone_trails();
    }
    
private:
    size_t get_or_create_node_id(const KeyType& key) {
        auto it = key_to_node_id_.find(key);
        if (it != key_to_node_id_.end()) {
            return it->second;
        }
        
        size_t new_id = next_node_id_++;
        key_to_node_id_[key] = new_id;
        return new_id;
    }
    
    void adapt_fanout() {
        stats_.total_adaptations++;
        
        std::vector<NodeAccessInfo> nodes = monitor_.get_tracked_nodes();
        if (nodes.empty()) return;
        
        double avg_heat = 0.0;
        for (const auto& node : nodes) {
            avg_heat += node.heat_score;
        }
        avg_heat /= nodes.size();
        
        // Convert to ACO state
        int current_state = aco_agent_.heat_to_state(avg_heat);
        
        // Select action using ACO
        int action = aco_agent_.select_action(current_state);
        
        size_t old_fanout = current_fanout_;
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
        
        // Calculate reward
        double cache_hit_rate = get_cache_hit_rate();
        double reward = cache_hit_rate * 10.0 - 5.0;
        
        if (avg_heat > 0.7 && new_fanout <= 16) {
            reward += 2.0;
        } else if (avg_heat < 0.3 && new_fanout >= 128) {
            reward += 2.0;
        }
        
        total_reward_ += reward;
        stats_.avg_reward = total_reward_ / stats_.total_adaptations;
        
        // Update ACO pheromones
        aco_agent_.update(current_state, action, reward);
        
        if (new_fanout != old_fanout) {
            current_fanout_ = new_fanout;
            restructure_tree();
        }
    }
    
    void restructure_tree() {
        stats_.restructure_count++;
        delete tree_;
        tree_ = new cabtree::BPlusTree<KeyType, ValueType>(current_fanout_);
    }
};

} // namespace cache_adaptive

#endif // ADAPTIVE_BTREE_ACO_H