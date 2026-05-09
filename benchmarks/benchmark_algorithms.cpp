#include "adaptive/adaptive_algorithms.h"
#include "monitor/monitor.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <ctime>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <chrono>
#endif

using namespace cache_adaptive;

class BenchmarkTimer {
public:
    BenchmarkTimer() {
        reset();
    }

    void reset() {
#ifdef _WIN32
        QueryPerformanceCounter(&start_);
#else
        start_ = std::chrono::steady_clock::now();
#endif
    }

    double elapsed_ms() const {
#ifdef _WIN32
        LARGE_INTEGER end;
        LARGE_INTEGER frequency;
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&frequency);
        return (static_cast<double>(end.QuadPart - start_.QuadPart) * 1000.0) /
               static_cast<double>(frequency.QuadPart);
#else
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
#endif
    }

private:
#ifdef _WIN32
    LARGE_INTEGER start_;
#else
    std::chrono::steady_clock::time_point start_;
#endif
};

/**
 * Workload generator
 */
class WorkloadGenerator {
public:
    enum Pattern { HOT, COLD, MIXED, ZIPFIAN, SHIFTING };
    
    static std::vector<int> generate(Pattern pattern, size_t size, size_t key_range = 1000) {
        std::vector<int> workload;
        workload.reserve(size);
        
        switch (pattern) {
            case HOT:
                // 80% of accesses to 20% of keys
                for (size_t i = 0; i < size; i++) {
                    if (rand() % 100 < 80) {
                        workload.push_back(rand() % (key_range / 5));
                    } else {
                        workload.push_back(rand() % key_range);
                    }
                }
                break;
                
            case COLD:
                // Uniform random access
                for (size_t i = 0; i < size; i++) {
                    workload.push_back(rand() % key_range);
                }
                break;
                
            case MIXED:
                // 50% hot, 50% cold
                for (size_t i = 0; i < size; i++) {
                    if (i % 2 == 0) {
                        workload.push_back(rand() % (key_range / 5));
                    } else {
                        workload.push_back(rand() % key_range);
                    }
                }
                break;
                
            case ZIPFIAN:
                // Zipfian distribution (realistic)
                for (size_t i = 0; i < size; i++) {
                    double r = (rand() % 1000) / 1000.0;
                    int key = static_cast<int>(pow(r, 0.5) * key_range);
                    workload.push_back(key % key_range);
                }
                break;
                
            case SHIFTING:
                // Workload shifts halfway through
                for (size_t i = 0; i < size / 2; i++) {
                    workload.push_back(rand() % 10);  // Hot
                }
                for (size_t i = size / 2; i < size; i++) {
                    workload.push_back(rand() % key_range);  // Cold
                }
                break;
        }
        
        return workload;
    }
    
    static std::string pattern_name(Pattern p) {
        switch (p) {
            case HOT: return "Hot (80/20)";
            case COLD: return "Cold (Uniform)";
            case MIXED: return "Mixed (50/50)";
            case ZIPFIAN: return "Zipfian";
            case SHIFTING: return "Shifting";
            default: return "Unknown";
        }
    }
};

/**
 * Simulation environment
 */
class SimulationEnvironment {
private:
    AccessMonitor monitor_;
    size_t current_fanout_;
    size_t min_fanout_;
    size_t max_fanout_;
    
    double total_reward_;
    size_t adaptations_;
    size_t cache_hits_;
    size_t total_accesses_;
    
public:
    SimulationEnvironment(size_t initial_fanout = 64, 
                         size_t min_f = 8, 
                         size_t max_f = 256)
        : current_fanout_(initial_fanout)
        , min_fanout_(min_f)
        , max_fanout_(max_f)
        , total_reward_(0.0)
        , adaptations_(0)
        , cache_hits_(0)
        , total_accesses_(0)
    {}
    
    double step(AdaptationAlgorithm* agent, int key) {
        total_accesses_++;
        monitor_.record_access(key);
        
        // Simulate cache hit based on heat
        double heat = monitor_.get_heat_score(key);
        if (heat > 0.7) {
            cache_hits_++;
        }
        
        // Every 100 accesses, adapt
        if (total_accesses_ % 100 == 0) {
            adaptations_++;
            
            // Get action from agent
            size_t count = monitor_.get_access_count(key);
            int action = agent->select_action(heat, count);
            
            // Apply action
            if (action == 0) {
                current_fanout_ = std::max(min_fanout_, current_fanout_ / 2);
            } else if (action == 2) {
                current_fanout_ = std::min(max_fanout_, current_fanout_ * 2);
            }
            
            // Calculate reward
            double cache_rate = static_cast<double>(cache_hits_) / total_accesses_;
            double reward = cache_rate * 10.0 - 5.0;
            
            // Strategic bonus
            if (heat > 0.7 && current_fanout_ <= 16) {
                reward += 2.0;
            } else if (heat < 0.3 && current_fanout_ >= 128) {
                reward += 2.0;
            }
            
            total_reward_ += reward;
            
            // Update agent
            agent->update(heat, action, reward);
            
            return reward;
        }
        
        return 0.0;
    }
    
    void reset() {
        monitor_.clear();
        current_fanout_ = 64;
        total_reward_ = 0.0;
        adaptations_ = 0;
        cache_hits_ = 0;
        total_accesses_ = 0;
    }
    
    double get_avg_reward() const {
        return adaptations_ > 0 ? total_reward_ / adaptations_ : 0.0;
    }
    
    double get_cache_hit_rate() const {
        return total_accesses_ > 0 ? 
               static_cast<double>(cache_hits_) / total_accesses_ : 0.0;
    }
    
    size_t get_final_fanout() const { return current_fanout_; }
};

/**
 * Run benchmark for one algorithm
 */
struct BenchmarkResult {
    std::string algorithm;
    std::string workload;
    double avg_reward;
    double cache_hit_rate;
    size_t final_fanout;
    double elapsed_time_ms;
};

BenchmarkResult run_benchmark(AdaptationAlgorithm* agent, 
                              WorkloadGenerator::Pattern pattern,
                              size_t workload_size = 5000) {
    BenchmarkResult result;
    result.algorithm = agent->get_name();
    result.workload = WorkloadGenerator::pattern_name(pattern);
    
    // Generate workload
    std::vector<int> workload = WorkloadGenerator::generate(pattern, workload_size);
    
    // Run simulation
    SimulationEnvironment env;
    BenchmarkTimer timer;
    
    for (int key : workload) {
        env.step(agent, key);
    }
    
    // Collect results
    result.avg_reward = env.get_avg_reward();
    result.cache_hit_rate = env.get_cache_hit_rate();
    result.final_fanout = env.get_final_fanout();
    result.elapsed_time_ms = timer.elapsed_ms();
    
    return result;
}

/**
 * Main benchmark runner
 */
int main() {
    srand(time(NULL));
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Algorithm Comparison Benchmark" << std::endl;
    std::cout << "  Q-Learning vs Linear Regression vs Genetic Algorithm" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Create algorithms
    QLearningAgent qlearn(0.1, 0.9, 0.2);
    LinearRegressionAgent linreg(0.01);
    GeneticAlgorithmAgent genetic(20, 0.1);
    
    AdaptationAlgorithm* algorithms[] = {&qlearn, &linreg, &genetic};
    
    // Workload patterns to test
    WorkloadGenerator::Pattern patterns[] = {
        WorkloadGenerator::HOT,
        WorkloadGenerator::COLD,
        WorkloadGenerator::MIXED,
        WorkloadGenerator::ZIPFIAN,
        WorkloadGenerator::SHIFTING
    };
    
    std::vector<BenchmarkResult> all_results;
    
    // Run benchmarks
    std::cout << "Running benchmarks (5 workloads x 3 algorithms = 15 tests)...\n" << std::endl;
    
    for (auto pattern : patterns) {
        std::cout << "\nWorkload: " << WorkloadGenerator::pattern_name(pattern) << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (auto* algo : algorithms) {
            algo->reset();
            BenchmarkResult result = run_benchmark(algo, pattern, 50000);
            all_results.push_back(result);
            
            std::cout << std::left << std::setw(35) << algo->get_name()
                     << " | Reward: " << std::fixed << std::setprecision(2) << std::setw(6) << result.avg_reward
                     << " | Cache: " << std::setw(5) << (result.cache_hit_rate * 100) << "%"
                     << " | Fanout: " << std::setw(3) << result.final_fanout
                     << " | Time: " << std::fixed << std::setprecision(4)
                     << result.elapsed_time_ms << "ms" << std::endl;
        }
    }
    
    // Summary table
    std::cout << "\n\n========================================" << std::endl;
    std::cout << "  COMPLETE RESULTS SUMMARY" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    std::cout << std::left << std::setw(35) << "Algorithm"
              << std::setw(20) << "Workload"
              << std::setw(12) << "Avg Reward"
              << std::setw(12) << "Cache Hit%"
              << std::setw(10) << "Fanout"
              << "Time(ms)" << std::endl;
    std::cout << std::string(95, '=') << std::endl;
    
    for (const auto& r : all_results) {
        std::cout << std::left << std::setw(35) << r.algorithm
                  << std::setw(20) << r.workload
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << r.avg_reward
                  << std::setw(12) << (r.cache_hit_rate * 100)
                  << std::setw(10) << r.final_fanout
                  << r.elapsed_time_ms << std::endl;
    }
    
    // Calculate average performance per algorithm
    std::cout << "\n========================================" << std::endl;
    std::cout << "  AVERAGE PERFORMANCE BY ALGORITHM" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    for (auto* algo : algorithms) {
        double total_reward = 0.0;
        double total_cache = 0.0;
        int count = 0;
        
        for (const auto& r : all_results) {
            if (r.algorithm == algo->get_name()) {
                total_reward += r.avg_reward;
                total_cache += r.cache_hit_rate;
                count++;
            }
        }
        
        std::cout << std::left << std::setw(35) << algo->get_name() << std::endl;
        std::cout << "  Average Reward:    " << (total_reward / count) << std::endl;
        std::cout << "  Average Cache Hit: " << (total_cache / count * 100) << "%" << std::endl;
        std::cout << std::endl;
    }
    
    // Determine winner
    std::cout << "========================================" << std::endl;
    std::cout << "  WINNER DETERMINATION" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    std::string best_algo;
    double best_score = -999.0;
    
    for (auto* algo : algorithms) {
        double score = 0.0;
        int count = 0;
        
        for (const auto& r : all_results) {
            if (r.algorithm == algo->get_name()) {
                // Combined score: 70% reward + 30% cache hit rate
                score += 0.7 * r.avg_reward + 0.3 * (r.cache_hit_rate * 10);
                count++;
            }
        }
        
        score /= count;
        
        std::cout << algo->get_name() << std::endl;
        std::cout << "  Combined Score: " << score << std::endl;
        
        if (score > best_score) {
            best_score = score;
            best_algo = algo->get_name();
        }
    }
    
    std::cout << "\nWINNER: " << best_algo << std::endl;
    std::cout << "   Score: " << best_score << std::endl;
    std::cout << "\n========================================\n" << std::endl;
    
    // Print detailed stats for each algorithm
    qlearn.print_summary();
    linreg.print_summary();
    genetic.print_summary();
    
    return 0;
}
