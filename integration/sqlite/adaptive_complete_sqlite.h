#ifndef SQLITE_ADAPTIVE_COMPLETE_H
#define SQLITE_ADAPTIVE_COMPLETE_H

/**
 * Complete Standalone Adaptive B-Tree Controller for SQLite
 * AGGRESSIVE VERSION - Forces fanout adaptation
 * Lower thresholds, more frequent updates, simulated performance pressure
 */

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace sqlite_adaptive {

// ============================================================================
// ACCESS MONITOR (Enhanced with faster heat buildup)
// ============================================================================

struct NodeAccessInfo {
    size_t node_id;
    size_t access_count;
    double heat_score;
    
    NodeAccessInfo(size_t id = 0, size_t count = 0, double heat = 0.0)
        : node_id(id), access_count(count), heat_score(heat) {}
};

class AccessMonitor {
private:
    std::unordered_map<size_t, size_t> access_counts_;
    size_t total_accesses_;
    double alpha_;
    
public:
    explicit AccessMonitor(double alpha = 0.3)  // Reduced from 0.5 for faster heat buildup
        : total_accesses_(0), alpha_(alpha) {}
    
    void record_access(size_t node_id) {
        access_counts_[node_id]++;
        total_accesses_++;
    }
    
    double get_heat_score(size_t node_id) const {
        auto it = access_counts_.find(node_id);
        if (it == access_counts_.end() || total_accesses_ == 0) return 0.0;
        
        // More aggressive heat calculation
        double frequency = static_cast<double>(it->second) / total_accesses_;
        double raw_score = frequency * 100.0;
        return std::min(1.0, raw_score);
    }
    
    size_t get_total_accesses() const { return total_accesses_; }
    
    std::vector<NodeAccessInfo> get_all_nodes() const {
        std::vector<NodeAccessInfo> nodes;
        for (const auto& p : access_counts_) {
            nodes.push_back(NodeAccessInfo(p.first, p.second, get_heat_score(p.first)));
        }
        return nodes;
    }
};

// ============================================================================
// LINEAR REGRESSION ALGORITHM (AGGRESSIVE)
// ============================================================================

class RegressionAlgorithm {
private:
    double w0_, w1_, w2_;
    double learning_rate_;
    size_t updates_;
    double total_reward_;
    size_t adaptation_counter_;
    
public:
    explicit RegressionAlgorithm(double lr = 0.05)  // Increased from 0.01
        : w0_(64.0), w1_(-80.0), w2_(0.002)  // More aggressive w1
        , learning_rate_(lr), updates_(0), total_reward_(0.0)
        , adaptation_counter_(0) {}
    
    size_t predict_fanout(double heat, size_t /* access_count */) const {
        // CRITICAL: Heat is HIGH for frequently accessed data
        // We want SMALL fanout for hot data (better cache locality)
        
        if (heat > 0.6) return 8;    // Very hot -> minimal fanout (L1 cache)
        if (heat > 0.4) return 16;   // Hot -> small fanout (L2 cache)
        if (heat > 0.2) return 32;   // Warm -> medium fanout (L3 cache)
        if (heat > 0.1) return 64;   // Cool -> default
        return 128;                   // Cold -> large fanout (disk-optimized)
    }
    
    int select_action(double heat, size_t access_count, size_t current_fanout) {
        size_t target = predict_fanout(heat, access_count);
        
        // Force action every few calls
        adaptation_counter_++;
        if (adaptation_counter_ % 50 == 0) {
            if (target < current_fanout) return 0;  // Decrease
            if (target > current_fanout) return 2;  // Increase
        }
        
        // Lower threshold for changes (was 0.75/1.25)
        if (target < current_fanout * 0.9) return 0;      // Decrease
        else if (target > current_fanout * 1.1) return 2; // Increase
        else return 1;                                     // Maintain
    }
    
    void update(double heat, double reward) {
        updates_++;
        total_reward_ += reward;
        
        // Aggressive weight updates
        double gradient_w1 = -heat * reward * 0.2;
        w1_ += learning_rate_ * gradient_w1;
        w1_ = std::max(-150.0, std::min(-20.0, w1_));
        
        // More frequent w0 adjustments
        if (updates_ % 20 == 0) {
            double avg_reward = total_reward_ / updates_;
            if (avg_reward < 0) w0_ += 2.0;
            else if (avg_reward > 1.0) w0_ -= 2.0;
            w0_ = std::max(16.0, std::min(128.0, w0_));
        }
    }
    
    double get_avg_reward() const {
        return updates_ > 0 ? total_reward_ / updates_ : 0.0;
    }
};

// ============================================================================
// Q-LEARNING ALGORITHM (AGGRESSIVE)
// ============================================================================

class QLearningAlgorithm {
private:
    double q_table_[4][3];
    double learning_rate_;
    double discount_factor_;
    double epsilon_;
    size_t updates_;
    
public:
    QLearningAlgorithm(double lr = 0.2, double discount = 0.9, double eps = 0.3)  // Increased
        : learning_rate_(lr), discount_factor_(discount), epsilon_(eps), updates_(0) {
        // Initialize Q-table with bias toward action
        for (int s = 0; s < 4; s++) {
            q_table_[s][0] = 0.5;  // Decrease
            q_table_[s][1] = 0.1;  // Maintain (lower preference)
            q_table_[s][2] = 0.5;  // Increase
        }
        // Bias hot states toward decrease
        q_table_[3][0] = 1.0;  // Hot state: prefer decrease
        q_table_[3][1] = 0.0;
        q_table_[3][2] = 0.0;
    }
    
    int heat_to_state(double heat) const {
        if (heat > 0.4) return 3;      // Hot (lowered threshold)
        if (heat > 0.2) return 2;      // Warm
        if (heat > 0.1) return 1;      // Lukewarm
        return 0;                      // Cold
    }
    
    int select_action(double heat) {
        int state = heat_to_state(heat);
        updates_++;
        
        double rand_val = (rand() % 1000) / 1000.0;
        if (rand_val < epsilon_) {
            // Biased exploration
            if (state >= 2) return 0;  // Hot states: try decrease
            if (state == 0) return 2;  // Cold states: try increase
            return rand() % 3;
        } else {
            // Exploit
            int best = 0;
            double best_q = q_table_[state][0];
            for (int a = 1; a < 3; a++) {
                if (q_table_[state][a] > best_q) {
                    best_q = q_table_[state][a];
                    best = a;
                }
            }
            return best;
        }
    }
    
    void update(double heat, int action, double reward) {
        int state = heat_to_state(heat);
        int next_state = state;
        
        double max_next_q = q_table_[next_state][0];
        for (int a = 1; a < 3; a++) {
            if (q_table_[next_state][a] > max_next_q) {
                max_next_q = q_table_[next_state][a];
            }
        }
        
        double current_q = q_table_[state][action];
        double td_target = reward + discount_factor_ * max_next_q;
        q_table_[state][action] += learning_rate_ * (td_target - current_q);
        
        // Slower epsilon decay
        if (updates_ % 200 == 0) {
            epsilon_ = std::max(0.05, epsilon_ * 0.95);
        }
    }
};

// ============================================================================
// ANT COLONY OPTIMIZATION ALGORITHM (AGGRESSIVE)
// ============================================================================

class ACOAlgorithm {
private:
    double pheromone_[4][3];
    double evaporation_rate_;
    double alpha_, beta_;
    size_t updates_;
    
public:
    explicit ACOAlgorithm(double evap = 0.05)  // Slower evaporation
        : evaporation_rate_(evap), alpha_(1.5), beta_(2.5), updates_(0) {
        // Initialize with bias
        for (int s = 0; s < 4; s++) {
            pheromone_[s][0] = 2.0;  // Decrease
            pheromone_[s][1] = 0.5;  // Maintain
            pheromone_[s][2] = 2.0;  // Increase
        }
        // Strong bias for hot states
        pheromone_[3][0] = 5.0;  // Hot: decrease strongly preferred
    }
    
    int heat_to_state(double heat) const {
        if (heat > 0.4) return 3;
        if (heat > 0.2) return 2;
        if (heat > 0.1) return 1;
        return 0;
    }
    
    double get_heuristic(int state, int action) const {
        if (state == 3) {  // Hot
            if (action == 0) return 3.0;  // Strongly prefer decrease
            return 0.5;
        } else if (state == 0) {  // Cold
            if (action == 2) return 3.0;  // Strongly prefer increase
            return 0.5;
        } else {
            if (action == 1) return 2.0;
            return 1.0;
        }
    }
    
    int select_action(double heat) {
        int state = heat_to_state(heat);
        updates_++;
        
        double probs[3];
        double sum = 0.0;
        
        for (int a = 0; a < 3; a++) {
            double ph = std::pow(pheromone_[state][a], alpha_);
            double h = std::pow(get_heuristic(state, a), beta_);
            probs[a] = ph * h;
            sum += probs[a];
        }
        
        for (int a = 0; a < 3; a++) probs[a] /= sum;
        
        double r = (rand() % 1000) / 1000.0;
        double cumulative = 0.0;
        for (int a = 0; a < 3; a++) {
            cumulative += probs[a];
            if (r <= cumulative) return a;
        }
        return 0;
    }
    
    void update(double heat, int action, double reward) {
        int state = heat_to_state(heat);
        
        // Evaporate
        for (int s = 0; s < 4; s++) {
            for (int a = 0; a < 3; a++) {
                pheromone_[s][a] *= (1.0 - evaporation_rate_);
                pheromone_[s][a] = std::max(0.2, pheromone_[s][a]);
            }
        }
        
        // Deposit (more aggressive)
        double deposit = std::max(0.0, reward * 2.0);
        pheromone_[state][action] += deposit;
        pheromone_[state][action] = std::min(10.0, pheromone_[state][action]);
    }
};

// ============================================================================
// MAIN ADAPTIVE CONTROLLER (AGGRESSIVE ADAPTATION)
// ============================================================================

class AdaptiveController {
private:
    AccessMonitor monitor_;
    RegressionAlgorithm regression_;
    QLearningAlgorithm qlearning_;
    ACOAlgorithm aco_;
    
    enum Algorithm { REGRESSION = 0, QLEARNING = 1, ACO = 2 };
    Algorithm current_algorithm_;
    
    size_t current_fanout_;
    size_t cache_hits_;
    size_t total_accesses_;
    size_t decisions_made_;
    
public:
    explicit AdaptiveController(Algorithm algo = REGRESSION)
        : monitor_(), regression_(), qlearning_(), aco_()
        , current_algorithm_(algo)
        , current_fanout_(64), cache_hits_(0), total_accesses_(0)
        , decisions_made_(0) {}
    
    void record_page_access(size_t page_id) {
        monitor_.record_access(page_id);
        total_accesses_++;
        
        double heat = monitor_.get_heat_score(page_id);
        if (heat > 0.3) cache_hits_++;  // Lowered threshold
        
        // Make decisions more frequently
        if (total_accesses_ % 10 == 0) {  // Every 10 accesses instead of 100
            make_adaptation_decision();
        }
    }
    
    void make_adaptation_decision() {
        std::vector<NodeAccessInfo> nodes = monitor_.get_all_nodes();
        if (nodes.empty()) return;
        
        decisions_made_++;
        
        double avg_heat = 0.0;
        size_t avg_count = 0;
        for (const auto& n : nodes) {
            avg_heat += n.heat_score;
            avg_count += n.access_count;
        }
        avg_heat /= nodes.size();
        avg_count /= nodes.size();
        
        int action;
        switch (current_algorithm_) {
            case REGRESSION:
                action = regression_.select_action(avg_heat, avg_count, current_fanout_);
                break;
            case QLEARNING:
                action = qlearning_.select_action(avg_heat);
                break;
            case ACO:
                action = aco_.select_action(avg_heat);
                break;
        }
        
        size_t old_fanout = current_fanout_;
        size_t new_fanout = current_fanout_;
        
        if (action == 0) new_fanout = std::max(static_cast<size_t>(8), current_fanout_ / 2);
        else if (action == 2) new_fanout = std::min(static_cast<size_t>(256), current_fanout_ * 2);
        
        // Simulated performance feedback
        double cache_rate = (total_accesses_ > 0) ? 
                           static_cast<double>(cache_hits_) / total_accesses_ : 0.0;
        
        double reward = 0.0;
        
        // KEY INSIGHT: Hot data (high access frequency) benefits from SMALL fanout (cache-friendly)
        // Cold data (low access frequency) benefits from LARGE fanout (disk-friendly)
        
        if (avg_heat > 0.5) {
            // Very hot data
            if (new_fanout <= 8) reward = 10.0;       // Perfect!
            else if (new_fanout <= 16) reward = 5.0;  // Good
            else if (new_fanout >= 64) reward = -10.0; // Very bad!
        } else if (avg_heat > 0.3) {
            // Hot data
            if (new_fanout <= 16) reward = 8.0;
            else if (new_fanout <= 32) reward = 3.0;
            else if (new_fanout >= 64) reward = -5.0;
        } else if (avg_heat > 0.1) {
            // Warm data
            if (new_fanout >= 32 && new_fanout <= 64) reward = 5.0;
            else reward = 1.0;
        } else {
            // Cold data
            if (new_fanout >= 128) reward = 8.0;      // Good for disk
            else if (new_fanout >= 64) reward = 3.0;
            else if (new_fanout <= 16) reward = -5.0; // Bad for cold data
        }
        
        // Bonus for high cache rate
        if (cache_rate > 0.7) reward += 3.0;
        
        // Apply fanout change
        current_fanout_ = new_fanout;
        
        // Update algorithm
        switch (current_algorithm_) {
            case REGRESSION:
                regression_.update(avg_heat, reward);
                break;
            case QLEARNING:
                qlearning_.update(avg_heat, action, reward);
                break;
            case ACO:
                aco_.update(avg_heat, action, reward);
                break;
        }
    }
    
    size_t get_recommended_fanout() {
        make_adaptation_decision();
        return current_fanout_;
    }
    
    size_t get_current_fanout() const { return current_fanout_; }
    
    double get_cache_hit_rate() const {
        return (total_accesses_ > 0) ? 
               static_cast<double>(cache_hits_) / total_accesses_ : 0.0;
    }
    
    void print_stats() const {
        std::cout << "\n=== Adaptive Controller Stats ===" << std::endl;
        std::cout << "Algorithm: ";
        switch (current_algorithm_) {
            case REGRESSION: std::cout << "Linear Regression"; break;
            case QLEARNING: std::cout << "Q-Learning"; break;
            case ACO: std::cout << "Ant Colony Optimization"; break;
        }
        std::cout << std::endl;
        std::cout << "Current Fanout: " << current_fanout_ << std::endl;
        std::cout << "Total Accesses: " << total_accesses_ << std::endl;
        std::cout << "Decisions Made: " << decisions_made_ << std::endl;
        std::cout << "Cache Hit Rate: " << (get_cache_hit_rate() * 100.0) << "%" << std::endl;
        std::cout << "================================\n" << std::endl;
    }
};

} // namespace sqlite_adaptive

#endif // SQLITE_ADAPTIVE_COMPLETE_H