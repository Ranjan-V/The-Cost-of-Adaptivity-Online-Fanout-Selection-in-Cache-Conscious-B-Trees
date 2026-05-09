#include "predictor/predictor.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace cache_adaptive;

// Test counter
int tests_passed = 0;
int tests_failed = 0;

// Helper macro for assertions
#define TEST_ASSERT(condition, test_name) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << test_name << std::endl; \
            tests_passed++; \
        } else { \
            std::cout << "[FAIL] " << test_name << std::endl; \
            tests_failed++; \
        } \
    } while(0)

/**
 * Test 1: Basic construction
 */
void test_basic_construction() {
    FanoutPredictor predictor;
    
    PredictionStats stats = predictor.get_stats();
    TEST_ASSERT(stats.predictions_made == 0,
                "New predictor should have 0 predictions");
    TEST_ASSERT(predictor.get_smoothed_heat() == 0.0,
                "Initial smoothed heat should be 0.0");
}

/**
 * Test 2: Hot node prediction (heat > 0.7)
 */
void test_hot_node_prediction() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec = predictor.predict_fanout(0.9, 1000);
    
    TEST_ASSERT(rec.recommended_fanout >= 8 && rec.recommended_fanout <= 16,
                "Hot node should get small fanout (8-16)");
    TEST_ASSERT(rec.confidence > 0.7,
                "Hot node prediction should have high confidence");
    TEST_ASSERT(rec.reasoning.find("Hot") != std::string::npos ||
                rec.reasoning.find("L1") != std::string::npos,
                "Reasoning should mention hot node or L1 cache");
}

/**
 * Test 3: Warm node prediction (0.5 < heat < 0.7)
 */
void test_warm_node_prediction() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec = predictor.predict_fanout(0.6, 500);
    
    TEST_ASSERT(rec.recommended_fanout >= 16 && rec.recommended_fanout <= 64,
                "Warm node should get medium fanout (16-64)");
    TEST_ASSERT(rec.confidence > 0.5,
                "Warm node prediction should have moderate confidence");
}

/**
 * Test 4: Cold node prediction (heat < 0.3)
 */
void test_cold_node_prediction() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec = predictor.predict_fanout(0.1, 10);
    
    TEST_ASSERT(rec.recommended_fanout >= 128,
                "Cold node should get large fanout (128+)");
    TEST_ASSERT(rec.confidence > 0.6,
                "Cold node prediction should have good confidence");
    TEST_ASSERT(rec.reasoning.find("Cold") != std::string::npos ||
                rec.reasoning.find("Disk") != std::string::npos,
                "Reasoning should mention cold node or disk");
}

/**
 * Test 5: New/unknown node (heat = 0.0)
 */
void test_new_node_prediction() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec = predictor.predict_fanout(0.0, 0);
    
    TEST_ASSERT(rec.recommended_fanout == 64,
                "New node should get default fanout (64)");
    TEST_ASSERT(rec.confidence == 0.5,
                "New node should have 0.5 confidence");
    TEST_ASSERT(rec.reasoning.find("default") != std::string::npos ||
                rec.reasoning.find("Default") != std::string::npos,
                "Reasoning should mention default");
}

/**
 * Test 6: Exponential smoothing
 */
void test_exponential_smoothing() {
    FanoutPredictor predictor(8, 256, 64, 0.5);  // alpha = 0.5
    
    // First prediction: heat = 1.0
    predictor.predict_fanout(1.0, 100);
    double smooth1 = predictor.get_smoothed_heat();
    
    // Second prediction: heat = 0.0
    predictor.predict_fanout(0.0, 100);
    double smooth2 = predictor.get_smoothed_heat();
    
    TEST_ASSERT(smooth1 == 1.0,
                "First smoothed heat should equal input");
    TEST_ASSERT(smooth2 > 0.0 && smooth2 < 1.0,
                "Smoothed heat should be between extremes");
    TEST_ASSERT(smooth2 < smooth1,
                "Smoothed heat should decrease after low input");
}

/**
 * Test 7: Access count confidence boost
 */
void test_access_count_boost() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec1 = predictor.predict_fanout(0.5, 10);
    predictor.reset();
    FanoutRecommendation rec2 = predictor.predict_fanout(0.5, 1000);
    
    TEST_ASSERT(rec2.confidence >= rec1.confidence,
                "Higher access count should boost confidence");
}

/**
 * Test 8: Fanout bounds enforcement
 */
void test_fanout_bounds() {
    FanoutPredictor predictor(16, 128, 64);
    
    // Test hot node (should be clamped to min)
    FanoutRecommendation rec1 = predictor.predict_fanout(1.0, 1000);
    TEST_ASSERT(rec1.recommended_fanout >= 16,
                "Fanout should respect minimum bound");
    
    // Test cold node (should be clamped to max)
    predictor.reset();
    FanoutRecommendation rec2 = predictor.predict_fanout(0.0, 1000);
    TEST_ASSERT(rec2.recommended_fanout <= 128,
                "Fanout should respect maximum bound");
}

/**
 * Test 9: Batch prediction
 */
void test_batch_prediction() {
    FanoutPredictor predictor;
    
    std::vector<double> heats = {0.9, 0.6, 0.3, 0.1};
    std::vector<size_t> counts = {1000, 500, 100, 10};
    
    std::vector<FanoutRecommendation> recs = predictor.predict_batch(heats, counts);
    
    TEST_ASSERT(recs.size() == 4,
                "Batch prediction should return 4 results");
    TEST_ASSERT(recs[0].recommended_fanout < recs[3].recommended_fanout,
                "Hot node should have smaller fanout than cold");
}

/**
 * Test 10: Statistics tracking
 */
void test_statistics() {
    FanoutPredictor predictor;
    
    for (int i = 0; i < 10; i++) {
        predictor.predict_fanout(0.5, 100);
    }
    
    PredictionStats stats = predictor.get_stats();
    
    TEST_ASSERT(stats.predictions_made == 10,
                "Should track 10 predictions");
    TEST_ASSERT(stats.avg_predicted_fanout > 0.0,
                "Should calculate average fanout");
    TEST_ASSERT(stats.prediction_confidence > 0.0,
                "Should calculate confidence");
}

/**
 * Test 11: Reset functionality
 */
void test_reset() {
    FanoutPredictor predictor;
    
    // Make some predictions
    for (int i = 0; i < 5; i++) {
        predictor.predict_fanout(0.8, 100);
    }
    
    TEST_ASSERT(predictor.get_smoothed_heat() > 0.0,
                "Smoothed heat should be non-zero before reset");
    
    // Reset
    predictor.reset();
    
    TEST_ASSERT(predictor.get_smoothed_heat() == 0.0,
                "Smoothed heat should be 0.0 after reset");
    TEST_ASSERT(predictor.get_stats().predictions_made == 0,
                "Prediction count should be 0 after reset");
}

/**
 * Test 12: Custom cache sizes
 */
void test_custom_cache_sizes() {
    FanoutPredictor predictor;
    
    // Set custom cache sizes
    predictor.set_cache_sizes(32, 128, 512);
    
    // Predict for hot node
    FanoutRecommendation rec = predictor.predict_fanout(0.9, 1000);
    
    // Should work without errors
    TEST_ASSERT(rec.recommended_fanout > 0,
                "Should handle custom cache sizes");
}

/**
 * Test 13: Custom key/pointer sizes
 */
void test_custom_key_pointer_sizes() {
    FanoutPredictor predictor;
    
    // Set custom sizes (4-byte keys, 4-byte pointers)
    predictor.set_key_pointer_sizes(4, 4);
    
    FanoutRecommendation rec = predictor.predict_fanout(0.9, 1000);
    
    TEST_ASSERT(rec.recommended_fanout > 0,
                "Should handle custom key/pointer sizes");
}

/**
 * Test 14: Monotonic heat progression
 */
void test_monotonic_heat() {
    FanoutPredictor predictor;
    
    // Gradually increase heat
    FanoutRecommendation rec1 = predictor.predict_fanout(0.2, 100);
    FanoutRecommendation rec2 = predictor.predict_fanout(0.5, 100);
    FanoutRecommendation rec3 = predictor.predict_fanout(0.8, 100);
    
    TEST_ASSERT(rec1.recommended_fanout >= rec2.recommended_fanout,
                "Lower heat should recommend higher fanout");
    TEST_ASSERT(rec2.recommended_fanout >= rec3.recommended_fanout,
                "Fanout should decrease as heat increases");
}

/**
 * Test 15: Stability under repeated predictions
 */
void test_stability() {
    FanoutPredictor predictor(8, 256, 64, 0.1);  // Low alpha for stability
    
    // Make many predictions with same heat
    size_t fanout_sum = 0;
    for (int i = 0; i < 100; i++) {
        FanoutRecommendation rec = predictor.predict_fanout(0.7, 500);
        fanout_sum += rec.recommended_fanout;
    }
    
    double avg_fanout = fanout_sum / 100.0;
    
    // Make one more prediction
    FanoutRecommendation final_rec = predictor.predict_fanout(0.7, 500);
    
    // Should be close to average (within 20%)
    double diff = std::abs(final_rec.recommended_fanout - avg_fanout);
    TEST_ASSERT(diff < avg_fanout * 0.2,
                "Predictions should be stable with low alpha");
}

/**
 * Test 16: Confidence increases with experience
 */
void test_confidence_growth() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec1 = predictor.predict_fanout(0.5, 100);
    double conf1 = predictor.get_stats().prediction_confidence;
    
    // Make many more predictions
    for (int i = 0; i < 1000; i++) {
        predictor.predict_fanout(0.5, 100);
    }
    
    double conf2 = predictor.get_stats().prediction_confidence;
    
    TEST_ASSERT(conf2 > conf1,
                "Confidence should increase with more predictions");
}

/**
 * Test 17: Edge case - very high heat
 */
void test_very_high_heat() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec = predictor.predict_fanout(1.0, 10000);
    
    TEST_ASSERT(rec.recommended_fanout >= 8,
                "Should handle heat = 1.0");
    TEST_ASSERT(rec.confidence <= 1.0,
                "Confidence should not exceed 1.0");
}

/**
 * Test 18: Lukewarm node (0.3 < heat < 0.5)
 */
void test_lukewarm_node() {
    FanoutPredictor predictor;
    
    FanoutRecommendation rec = predictor.predict_fanout(0.4, 200);
    
    TEST_ASSERT(rec.recommended_fanout >= 32 && rec.recommended_fanout <= 128,
                "Lukewarm node should get medium-large fanout");
    TEST_ASSERT(rec.confidence > 0.5,
                "Should have reasonable confidence");
}

/**
 * Main test runner
 */
int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Fanout Predictor Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    test_basic_construction();
    test_hot_node_prediction();
    test_warm_node_prediction();
    test_cold_node_prediction();
    test_new_node_prediction();
    test_exponential_smoothing();
    test_access_count_boost();
    test_fanout_bounds();
    test_batch_prediction();
    test_statistics();
    test_reset();
    test_custom_cache_sizes();
    test_custom_key_pointer_sizes();
    test_monotonic_heat();
    test_stability();
    test_confidence_growth();
    test_very_high_heat();
    test_lukewarm_node();
    
    // Print summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "PASSED: " << tests_passed << std::endl;
    std::cout << "FAILED: " << tests_failed << std::endl;
    std::cout << "TOTAL:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Print example predictions
    std::cout << "\nExample Predictions:" << std::endl;
    std::cout << "========================================" << std::endl;
    
    FanoutPredictor demo;
    
    double test_heats[] = {0.95, 0.7, 0.5, 0.3, 0.1, 0.0};
    const char* labels[] = {"Very Hot", "Hot", "Warm", "Lukewarm", "Cold", "New"};
    
    for (int i = 0; i < 6; i++) {
        FanoutRecommendation rec = demo.predict_fanout(test_heats[i], 100);
        std::cout << labels[i] << " (heat=" << test_heats[i] << "):" << std::endl;
        std::cout << "  Fanout: " << rec.recommended_fanout << std::endl;
        std::cout << "  Confidence: " << rec.confidence << std::endl;
        std::cout << "  Reasoning: " << rec.reasoning << std::endl;
        std::cout << std::endl;
    }
    
    demo.print_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}