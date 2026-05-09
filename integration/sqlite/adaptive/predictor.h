#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>

namespace cache_adaptive {

/**
 * Prediction statistics and metrics
 */
struct PredictionStats {
    size_t predictions_made;
    double avg_predicted_fanout;
    double prediction_confidence;
    size_t cache_line_size;
    
    PredictionStats()
        : predictions_made(0)
        , avg_predicted_fanout(0.0)
        , prediction_confidence(0.0)
        , cache_line_size(64) {}
};

/**
 * Fanout recommendation with confidence score
 */
struct FanoutRecommendation {
    size_t recommended_fanout;
    double confidence;          // 0.0 to 1.0
    double heat_score;
    size_t access_count;
    std::string reasoning;
    
    FanoutRecommendation()
        : recommended_fanout(64)
        , confidence(0.5)
        , heat_score(0.0)
        , access_count(0)
        , reasoning("Default") {}
    
    FanoutRecommendation(size_t fanout, double conf, double heat, size_t count, const std::string& reason)
        : recommended_fanout(fanout)
        , confidence(conf)
        , heat_score(heat)
        , access_count(count)
        , reasoning(reason) {}
};

/**
 * Fanout Predictor
 * 
 * Predicts optimal B-Tree node fanout based on access patterns and heat scores.
 * Uses exponential smoothing for stability and cache-aware sizing.
 * 
 * Strategy:
 * - Hot nodes (heat > 0.7): Small fanout (8-16) to fit in L1 cache
 * - Warm nodes (0.3 < heat < 0.7): Medium fanout (32-64) for L2/L3 cache  
 * - Cold nodes (heat < 0.3): Large fanout (128-256) for disk optimization
 */
class FanoutPredictor {
private:
    // Configuration
    size_t min_fanout_;
    size_t max_fanout_;
    size_t default_fanout_;
    
    // Cache sizes (in bytes)
    size_t l1_cache_line_;  // Typically 64 bytes
    size_t l2_cache_line_;  // Typically 256 bytes
    size_t l3_cache_line_;  // Typically 1024 bytes
    
    // Smoothing parameters
    double alpha_;           // Exponential smoothing factor (0.0-1.0)
    double smoothed_heat_;   // Smoothed heat score
    
    // Statistics
    size_t predictions_made_;
    double total_predicted_fanout_;
    
    // Key size estimation (for cache calculations)
    size_t key_size_;
    size_t pointer_size_;
    
public:
    /**
     * Constructor
     * @param min_fanout Minimum allowed fanout (default: 8)
     * @param max_fanout Maximum allowed fanout (default: 256)
     * @param default_fanout Default fanout for new nodes (default: 64)
     * @param alpha Exponential smoothing factor (default: 0.3)
     */
    explicit FanoutPredictor(
        size_t min_fanout = 8,
        size_t max_fanout = 256,
        size_t default_fanout = 64,
        double alpha = 0.3
    ) : min_fanout_(min_fanout)
      , max_fanout_(max_fanout)
      , default_fanout_(default_fanout)
      , l1_cache_line_(64)
      , l2_cache_line_(256)
      , l3_cache_line_(1024)
      , alpha_(alpha)
      , smoothed_heat_(0.0)
      , predictions_made_(0)
      , total_predicted_fanout_(0.0)
      , key_size_(8)      // Assume 64-bit keys
      , pointer_size_(8)  // Assume 64-bit pointers
    {
        // Validate parameters
        if (min_fanout_ < 4) min_fanout_ = 4;
        if (max_fanout_ > 512) max_fanout_ = 512;
        if (default_fanout_ < min_fanout_) default_fanout_ = min_fanout_;
        if (default_fanout_ > max_fanout_) default_fanout_ = max_fanout_;
        if (alpha_ < 0.0) alpha_ = 0.0;
        if (alpha_ > 1.0) alpha_ = 1.0;
    }
    
    /**
     * Predict optimal fanout based on heat score
     * @param heat_score Node heat score (0.0 to 1.0)
     * @param access_count Number of accesses to the node
     * @return Fanout recommendation with confidence
     */
    FanoutRecommendation predict_fanout(double heat_score, size_t access_count) {
        // Update smoothed heat using exponential smoothing
        if (predictions_made_ == 0) {
            smoothed_heat_ = heat_score;
        } else {
            smoothed_heat_ = alpha_ * heat_score + (1.0 - alpha_) * smoothed_heat_;
        }
        
        predictions_made_++;
        
        // Determine fanout based on smoothed heat
        size_t recommended_fanout;
        double confidence;
        std::string reasoning;
        
        if (smoothed_heat_ > 0.7) {
            // HOT NODE: Optimize for L1 cache
            recommended_fanout = calculate_cache_optimal_fanout(l1_cache_line_);
            confidence = std::min(0.95, 0.7 + (smoothed_heat_ - 0.7) * 0.5);
            reasoning = "Hot node - L1 cache optimized";
            
        } else if (smoothed_heat_ > 0.5) {
            // WARM NODE: Optimize for L2 cache
            recommended_fanout = calculate_cache_optimal_fanout(l2_cache_line_);
            confidence = 0.6 + (smoothed_heat_ - 0.5) * 0.4;
            reasoning = "Warm node - L2 cache optimized";
            
        } else if (smoothed_heat_ > 0.3) {
            // LUKEWARM NODE: Optimize for L3 cache
            recommended_fanout = calculate_cache_optimal_fanout(l3_cache_line_);
            confidence = 0.5 + (smoothed_heat_ - 0.3) * 0.3;
            reasoning = "Lukewarm node - L3 cache optimized";
            
        } else if (smoothed_heat_ > 0.0) {
            // COLD NODE: Maximize fanout for disk efficiency
            recommended_fanout = max_fanout_;
            confidence = 0.7 + (0.3 - smoothed_heat_) * 0.3;
            reasoning = "Cold node - Disk I/O optimized";
            
        } else {
            // NEW/UNKNOWN NODE: Use default
            recommended_fanout = default_fanout_;
            confidence = 0.5;
            reasoning = "New node - Using default fanout";
        }
        
        // Apply access count boost to confidence
        if (access_count > 100) {
            confidence = std::min(1.0, confidence + 0.1);
        } else if (access_count > 1000) {
            confidence = std::min(1.0, confidence + 0.2);
        }
        
        // Ensure fanout is within bounds
        recommended_fanout = std::max(min_fanout_, std::min(max_fanout_, recommended_fanout));
        
        // Update statistics
        total_predicted_fanout_ += recommended_fanout;
        
        return FanoutRecommendation(
            recommended_fanout,
            confidence,
            smoothed_heat_,
            access_count,
            reasoning
        );
    }
    
    /**
     * Predict fanout for multiple nodes in batch
     */
    std::vector<FanoutRecommendation> predict_batch(
        const std::vector<double>& heat_scores,
        const std::vector<size_t>& access_counts
    ) {
        std::vector<FanoutRecommendation> recommendations;
        size_t n = std::min(heat_scores.size(), access_counts.size());
        
        recommendations.reserve(n);
        for (size_t i = 0; i < n; i++) {
            recommendations.push_back(predict_fanout(heat_scores[i], access_counts[i]));
        }
        
        return recommendations;
    }
    
    /**
     * Get prediction statistics
     */
    PredictionStats get_stats() const {
        PredictionStats stats;
        stats.predictions_made = predictions_made_;
        stats.cache_line_size = l1_cache_line_;
        
        if (predictions_made_ > 0) {
            stats.avg_predicted_fanout = total_predicted_fanout_ / predictions_made_;
            stats.prediction_confidence = calculate_overall_confidence();
        }
        
        return stats;
    }
    
    /**
     * Reset predictor state
     */
    void reset() {
        smoothed_heat_ = 0.0;
        predictions_made_ = 0;
        total_predicted_fanout_ = 0.0;
    }
    
    /**
     * Get current smoothed heat value
     */
    double get_smoothed_heat() const {
        return smoothed_heat_;
    }
    
    /**
     * Set cache line sizes (advanced tuning)
     */
    void set_cache_sizes(size_t l1, size_t l2, size_t l3) {
        l1_cache_line_ = l1;
        l2_cache_line_ = l2;
        l3_cache_line_ = l3;
    }
    
    /**
     * Set key and pointer sizes for accurate cache calculations
     */
    void set_key_pointer_sizes(size_t key_size, size_t pointer_size) {
        key_size_ = key_size;
        pointer_size_ = pointer_size;
    }
    
    /**
     * Print prediction summary
     */
    void print_summary() const {
        PredictionStats stats = get_stats();
        
        std::cout << "\n=== Fanout Predictor Summary ===" << std::endl;
        std::cout << "Predictions Made: " << stats.predictions_made << std::endl;
        std::cout << "Avg Predicted Fanout: " << stats.avg_predicted_fanout << std::endl;
        std::cout << "Prediction Confidence: " << stats.prediction_confidence << std::endl;
        std::cout << "Current Smoothed Heat: " << smoothed_heat_ << std::endl;
        std::cout << "L1 Cache Line: " << l1_cache_line_ << " bytes" << std::endl;
        std::cout << "L2 Cache Line: " << l2_cache_line_ << " bytes" << std::endl;
        std::cout << "L3 Cache Line: " << l3_cache_line_ << " bytes" << std::endl;
        std::cout << "================================\n" << std::endl;
    }
    
private:
    /**
     * Calculate optimal fanout for given cache line size
     */
    size_t calculate_cache_optimal_fanout(size_t cache_line_size) const {
        // Node structure: [key1, ptr1, key2, ptr2, ..., keyN, ptrN, ptr(N+1)]
        // For fanout F: need F keys + (F+1) pointers
        size_t entry_size = key_size_ + pointer_size_;
        size_t extra_pointer = pointer_size_;
        
        // Calculate max fanout that fits in cache line
        if (cache_line_size <= extra_pointer) {
            return min_fanout_;
        }
        
        size_t available = cache_line_size - extra_pointer;
        size_t fanout = available / entry_size;
        
        // Ensure fanout is within bounds and power of 2 (optional optimization)
        fanout = std::max(min_fanout_, std::min(max_fanout_, fanout));
        
        // Round to nearest power of 2 for better alignment
        fanout = round_to_power_of_2(fanout);
        
        return fanout;
    }
    
    /**
     * Round to nearest power of 2 (for cache alignment)
     */
    size_t round_to_power_of_2(size_t n) const {
        if (n <= 4) return 4;
        if (n <= 8) return 8;
        if (n <= 16) return 16;
        if (n <= 32) return 32;
        if (n <= 64) return 64;
        if (n <= 128) return 128;
        return 256;
    }
    
    /**
     * Calculate overall prediction confidence
     */
    double calculate_overall_confidence() const {
        if (predictions_made_ == 0) return 0.0;
        
        // Confidence increases with more predictions
        double base_confidence = 0.5;
        double experience_bonus = std::min(0.3, predictions_made_ / 1000.0);
        
        // Higher confidence if smoothed heat is stable (not zero)
        double stability_bonus = (smoothed_heat_ > 0.0) ? 0.2 : 0.0;
        
        return std::min(1.0, base_confidence + experience_bonus + stability_bonus);
    }
};

} // namespace cache_adaptive

#endif // PREDICTOR_H