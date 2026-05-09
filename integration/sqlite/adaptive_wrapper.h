#ifndef SQLITE_ADAPTIVE_WRAPPER_H
#define SQLITE_ADAPTIVE_WRAPPER_H

/**
 * C Wrapper for SQLite Adaptive B-Tree Integration
 * Bridges SQLite (C code) with Adaptive Controller (C++ code)
 * 
 * This header contains ONLY declarations.
 * Implementation is in adaptive_wrapper_impl.cpp
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque pointer to C++ controller */
typedef struct AdaptiveController_opaque* AdaptiveControllerHandle;

/* Algorithm types */
typedef enum {
    ADAPTIVE_REGRESSION = 0,
    ADAPTIVE_QLEARNING = 1,
    ADAPTIVE_ACO = 2
} AdaptiveAlgorithm;

/**
 * Create adaptive controller
 */
AdaptiveControllerHandle adaptive_create(AdaptiveAlgorithm algo);

/**
 * Destroy adaptive controller
 */
void adaptive_destroy(AdaptiveControllerHandle handle);

/**
 * Record page access
 */
void adaptive_record_access(AdaptiveControllerHandle handle, unsigned int page_id);

/**
 * Get recommended fanout
 */
unsigned int adaptive_get_fanout(AdaptiveControllerHandle handle);

/**
 * Get current fanout
 */
unsigned int adaptive_get_current_fanout(AdaptiveControllerHandle handle);

/**
 * Get cache hit rate
 */
double adaptive_get_cache_rate(AdaptiveControllerHandle handle);

/**
 * Print statistics
 */
void adaptive_print_stats(AdaptiveControllerHandle handle);

#ifdef __cplusplus
}
#endif

#endif // SQLITE_ADAPTIVE_WRAPPER_H