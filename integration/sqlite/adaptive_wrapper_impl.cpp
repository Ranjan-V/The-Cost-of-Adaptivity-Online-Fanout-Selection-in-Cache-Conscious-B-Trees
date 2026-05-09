/**
 * C++ Implementation for Adaptive Wrapper
 * Implements the C API defined in adaptive_wrapper.h
 */

#include "adaptive_wrapper.h"
#include "adaptive_complete_sqlite.h"

extern "C" {

AdaptiveControllerHandle adaptive_create(AdaptiveAlgorithm algo) {
    using namespace sqlite_adaptive;
    
    // Create controller (algorithm selection not fully implemented yet)
    // All use default constructor for now
    sqlite_adaptive::AdaptiveController* controller = 
        new sqlite_adaptive::AdaptiveController();
    
    return reinterpret_cast<AdaptiveControllerHandle>(controller);
}

void adaptive_destroy(AdaptiveControllerHandle handle) {
    if (handle) {
        sqlite_adaptive::AdaptiveController* controller = 
            reinterpret_cast<sqlite_adaptive::AdaptiveController*>(handle);
        delete controller;
    }
}

void adaptive_record_access(AdaptiveControllerHandle handle, unsigned int page_id) {
    if (handle) {
        sqlite_adaptive::AdaptiveController* controller = 
            reinterpret_cast<sqlite_adaptive::AdaptiveController*>(handle);
        controller->record_page_access(page_id);
    }
}

unsigned int adaptive_get_fanout(AdaptiveControllerHandle handle) {
    if (handle) {
        sqlite_adaptive::AdaptiveController* controller = 
            reinterpret_cast<sqlite_adaptive::AdaptiveController*>(handle);
        return static_cast<unsigned int>(controller->get_recommended_fanout());
    }
    return 64;  // Default fanout
}

unsigned int adaptive_get_current_fanout(AdaptiveControllerHandle handle) {
    if (handle) {
        sqlite_adaptive::AdaptiveController* controller = 
            reinterpret_cast<sqlite_adaptive::AdaptiveController*>(handle);
        return static_cast<unsigned int>(controller->get_current_fanout());
    }
    return 64;  // Default fanout
}

double adaptive_get_cache_rate(AdaptiveControllerHandle handle) {
    if (handle) {
        sqlite_adaptive::AdaptiveController* controller = 
            reinterpret_cast<sqlite_adaptive::AdaptiveController*>(handle);
        return controller->get_cache_hit_rate();
    }
    return 0.0;
}

void adaptive_print_stats(AdaptiveControllerHandle handle) {
    if (handle) {
        sqlite_adaptive::AdaptiveController* controller = 
            reinterpret_cast<sqlite_adaptive::AdaptiveController*>(handle);
        controller->print_stats();
    }
}

} // extern "C"