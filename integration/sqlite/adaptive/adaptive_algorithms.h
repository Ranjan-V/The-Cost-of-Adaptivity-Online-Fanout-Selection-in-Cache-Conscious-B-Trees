#ifndef ADAPTIVE_ALGORITHMS_H
#define ADAPTIVE_ALGORITHMS_H

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

namespace cache_adaptive {

/**
 * Base class for adaptation algorithms
 */
class AdaptationAlgorithm {
public:
    virtual ~AdaptationAlgorithm() {}
    virtual std::string get_name() const = 0;
    virtual int select_action(double heat, size_t access_count) = 0;
    virtual void update(double heat, int action, double reward) = 0;
    virtual void print_summary() const = 0;
    virtual void reset() = 0;
};

/**
 * Algorithm 1: Q-Learning (Reinforcement Learning)
 * - Model-free RL
 * - Learns optimal policy through trial and error
 * - Best for: Dynamic environments with delayed rewards
 */
class QLearningAgent : public AdaptationAlgorithm {
private:
    double q_table_[4][3];  // [state][action]
    double learning_rate_;
    double discount_factor_;
    double epsilon_;
    size_t updates_;
    size_t explorations_;
    
    int heat_to_state(double heat) const {
        if (heat > 0.7) return 3;  // Hot
        if (heat > 0.5) return 2;  // Warm
        if (heat > 0.3) return 1;  // Lukewarm
        return 0;                  // Cold
    }
    
public:
    QLearningAgent(double lr = 0.1, double gamma = 0.9, double eps = 0.2)
        : learning_rate_(lr), discount_factor_(gamma), epsilon_(eps)
        , updates_(0), explorations_(0)
    {
        for (int s = 0; s < 4; s++) {
            for (int a = 0; a < 3; a++) {
                q_table_[s][a] = 0.1 * (rand() % 100) / 100.0;
            }
        }
    }
    
    std::string get_name() const override {
        return "Q-Learning (Reinforcement Learning)";
    }
    
    int select_action(double heat, size_t access_count) override {
        updates_++;
        int state = heat_to_state(heat);
        
        // Epsilon-greedy
        if ((rand() % 1000) / 1000.0 < epsilon_) {
            explorations_++;
            return rand() % 3;
        }
        
        // Exploit: best action
        int best = 0;
        for (int a = 1; a < 3; a++) {
            if (q_table_[state][a] > q_table_[state][best]) {
                best = a;
            }
        }
        return best;
    }
    
    void update(double heat, int action, double reward) override {
        int state = heat_to_state(heat);
        int next_state = state;  // Simplified
        
        double max_q = q_table_[next_state][0];
        for (int a = 1; a < 3; a++) {
            if (q_table_[next_state][a] > max_q) {
                max_q = q_table_[next_state][a];
            }
        }
        
        double td_target = reward + discount_factor_ * max_q;
        double td_error = td_target - q_table_[state][action];
        q_table_[state][action] += learning_rate_ * td_error;
        
        // Decay epsilon
        if (updates_ % 100 == 0) {
            epsilon_ = std::max(0.01, epsilon_ * 0.99);
        }
    }
    
    void print_summary() const override {
        std::cout << "\n=== " << get_name() << " ===" << std::endl;
        std::cout << "Updates: " << updates_ << std::endl;
        std::cout << "Exploration Rate: " << (100.0 * explorations_ / (updates_ + 1)) << "%" << std::endl;
        std::cout << "Current Epsilon: " << epsilon_ << std::endl;
        std::cout << "Q-Table Preview:" << std::endl;
        std::cout << "  Hot state: Decrease=" << q_table_[3][0] 
                  << ", Maintain=" << q_table_[3][1]
                  << ", Increase=" << q_table_[3][2] << std::endl;
    }
    
    void reset() override {
        updates_ = 0;
        explorations_ = 0;
        epsilon_ = 0.2;
        for (int s = 0; s < 4; s++) {
            for (int a = 0; a < 3; a++) {
                q_table_[s][a] = 0.1;
            }
        }
    }
};

/**
 * Algorithm 2: Linear Regression (Statistical Learning)
 * - Simple, interpretable
 * - Assumes linear relationship between heat and optimal fanout
 * - Best for: Stable, predictable patterns
 */
class LinearRegressionAgent : public AdaptationAlgorithm {
private:
    // Linear model: fanout = w0 + w1*heat + w2*access_count
    double w0_, w1_, w2_;
    double learning_rate_;
    
    // Training data history
    std::vector<double> heat_history_;
    std::vector<size_t> count_history_;
    std::vector<double> reward_history_;
    
    size_t updates_;
    
public:
    LinearRegressionAgent(double lr = 0.01)
        : w0_(64.0), w1_(-50.0), w2_(0.0)  // Initial weights
        , learning_rate_(lr)
        , updates_(0)
    {
        heat_history_.reserve(1000);
        count_history_.reserve(1000);
        reward_history_.reserve(1000);
    }
    
    std::string get_name() const override {
        return "Linear Regression (Statistical)";
    }
    
    int select_action(double heat, size_t access_count) override {
        updates_++;
        
        // Predict optimal fanout using linear model
        double predicted_fanout = w0_ + w1_ * heat + w2_ * (access_count / 1000.0);
        
        // Map to action (relative to current trend)
        if (predicted_fanout < 32) {
            return 0;  // Decrease
        } else if (predicted_fanout > 96) {
            return 2;  // Increase
        } else {
            return 1;  // Maintain
        }
    }
    
    void update(double heat, int action, double reward) override {
        // Store training data
        if (heat_history_.size() < 1000) {
            heat_history_.push_back(heat);
            reward_history_.push_back(reward);
        }
        
        // Gradient descent update (simplified)
        // Target: adjust weights to maximize reward
        double gradient_w1 = -heat * reward;
        w1_ += learning_rate_ * gradient_w1;
        
        // Clamp weights to reasonable range
        w1_ = std::max(-100.0, std::min(0.0, w1_));
    }
    
    void print_summary() const override {
        std::cout << "\n=== " << get_name() << " ===" << std::endl;
        std::cout << "Updates: " << updates_ << std::endl;
        std::cout << "Model: fanout = " << w0_ << " + " << w1_ << "*heat + " << w2_ << "*count" << std::endl;
        std::cout << "Training Samples: " << heat_history_.size() << std::endl;
        
        if (!reward_history_.empty()) {
            double avg_reward = 0.0;
            for (double r : reward_history_) avg_reward += r;
            avg_reward /= reward_history_.size();
            std::cout << "Average Reward: " << avg_reward << std::endl;
        }
    }
    
    void reset() override {
        w0_ = 64.0;
        w1_ = -50.0;
        w2_ = 0.0;
        updates_ = 0;
        heat_history_.clear();
        count_history_.clear();
        reward_history_.clear();
    }
};

/**
 * Algorithm 3: Genetic Algorithm (Evolutionary Optimization)
 * - Population-based search
 * - Evolves good solutions over generations
 * - Best for: Complex, non-linear optimization
 */
class GeneticAlgorithmAgent : public AdaptationAlgorithm {
private:
    struct Chromosome {
        double heat_thresholds[3];  // Thresholds for state transitions
        int action_map[4];           // Best action for each state
        double fitness;
        
        Chromosome() : fitness(0.0) {
            heat_thresholds[0] = 0.3;
            heat_thresholds[1] = 0.5;
            heat_thresholds[2] = 0.7;
            for (int i = 0; i < 4; i++) {
                action_map[i] = rand() % 3;
            }
        }
    };
    
    std::vector<Chromosome> population_;
    size_t population_size_;
    size_t generation_;
    double mutation_rate_;
    
    int heat_to_state(double heat, const Chromosome& chromo) const {
        if (heat > chromo.heat_thresholds[2]) return 3;
        if (heat > chromo.heat_thresholds[1]) return 2;
        if (heat > chromo.heat_thresholds[0]) return 1;
        return 0;
    }
    
    void evolve_population() {
        // Sort by fitness
        std::sort(population_.begin(), population_.end(),
            [](const Chromosome& a, const Chromosome& b) {
                return a.fitness > b.fitness;
            });
        
        // Keep top 50%
        size_t keep = population_size_ / 2;
        
        // Breed new generation
        for (size_t i = keep; i < population_size_; i++) {
            // Crossover: combine two parents
            Chromosome& parent1 = population_[rand() % keep];
            Chromosome& parent2 = population_[rand() % keep];
            Chromosome child;
            
            for (int j = 0; j < 3; j++) {
                child.heat_thresholds[j] = (rand() % 2 == 0) ? 
                    parent1.heat_thresholds[j] : parent2.heat_thresholds[j];
            }
            
            for (int j = 0; j < 4; j++) {
                child.action_map[j] = (rand() % 2 == 0) ?
                    parent1.action_map[j] : parent2.action_map[j];
            }
            
            // Mutation
            if ((rand() % 100) / 100.0 < mutation_rate_) {
                child.action_map[rand() % 4] = rand() % 3;
            }
            
            population_[i] = child;
        }
        
        generation_++;
    }
    
public:
    GeneticAlgorithmAgent(size_t pop_size = 20, double mutation = 0.1)
        : population_size_(pop_size)
        , generation_(0)
        , mutation_rate_(mutation)
    {
        population_.resize(pop_size);
    }
    
    std::string get_name() const override {
        return "Genetic Algorithm (Evolutionary)";
    }
    
    int select_action(double heat, size_t access_count) override {
        // Use best chromosome
        Chromosome& best = population_[0];
        int state = heat_to_state(heat, best);
        return best.action_map[state];
    }
    
    void update(double heat, int action, double reward) override {
        // Update fitness of current best chromosome
        population_[0].fitness = 0.9 * population_[0].fitness + 0.1 * reward;
        
        // Evolve every 10 updates
        if (generation_ % 10 == 9) {
            evolve_population();
        }
    }
    
    void print_summary() const override {
        std::cout << "\n=== " << get_name() << " ===" << std::endl;
        std::cout << "Generation: " << generation_ << std::endl;
        std::cout << "Population Size: " << population_size_ << std::endl;
        std::cout << "Best Fitness: " << population_[0].fitness << std::endl;
        std::cout << "Best Strategy:" << std::endl;
        
        const char* actions[] = {"Decrease", "Maintain", "Increase"};
        const char* states[] = {"Cold", "Lukewarm", "Warm", "Hot"};
        
        for (int s = 0; s < 4; s++) {
            std::cout << "  " << states[s] << " -> " 
                     << actions[population_[0].action_map[s]] << std::endl;
        }
    }
    
    void reset() override {
        population_.clear();
        population_.resize(population_size_);
        generation_ = 0;
    }
};

} // namespace cache_adaptive

#endif // ADAPTIVE_ALGORITHMS_H