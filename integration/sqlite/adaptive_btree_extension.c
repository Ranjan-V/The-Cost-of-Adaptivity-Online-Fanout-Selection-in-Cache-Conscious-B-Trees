/**
 * SQLite Adaptive B-Tree Extension
 * C interface only - calls C++ code through wrapper
 */

#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

#include "adaptive_wrapper.h"
#include <stdio.h>
#include <string.h>

// Global adaptive controller
static AdaptiveControllerHandle g_adaptive_controller = NULL;
static int g_monitoring_enabled = 1;
static unsigned long long g_query_count = 0;

/**
 * Initialize adaptive controller
 */
static void adaptive_init(AdaptiveAlgorithm algo) {
    if (g_adaptive_controller) {
        adaptive_destroy(g_adaptive_controller);
    }
    g_adaptive_controller = adaptive_create(algo);
    g_query_count = 0;
}

/**
 * SQL Function: adaptive_set_algorithm('regression'|'qlearning'|'aco')
 */
static void adaptive_set_algorithm_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (argc != 1) {
        sqlite3_result_error(context, "Usage: adaptive_set_algorithm('regression'|'qlearning'|'aco')", -1);
        return;
    }
    
    const unsigned char *algo_str = sqlite3_value_text(argv[0]);
    AdaptiveAlgorithm algo = ADAPTIVE_REGRESSION;
    
    if (sqlite3_stricmp((const char*)algo_str, "regression") == 0) {
        algo = ADAPTIVE_REGRESSION;
    } else if (sqlite3_stricmp((const char*)algo_str, "qlearning") == 0) {
        algo = ADAPTIVE_QLEARNING;
    } else if (sqlite3_stricmp((const char*)algo_str, "aco") == 0) {
        algo = ADAPTIVE_ACO;
    } else {
        sqlite3_result_error(context, "Invalid algorithm. Use: regression, qlearning, or aco", -1);
        return;
    }
    
    adaptive_init(algo);
    sqlite3_result_text(context, (const char*)algo_str, -1, SQLITE_TRANSIENT);
}

/**
 * SQL Function: adaptive_enable(1|0)
 */
static void adaptive_enable_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (argc != 1) {
        sqlite3_result_error(context, "Usage: adaptive_enable(1|0)", -1);
        return;
    }
    
    g_monitoring_enabled = sqlite3_value_int(argv[0]);
    sqlite3_result_int(context, g_monitoring_enabled);
}

/**
 * SQL Function: adaptive_record_access(page_id)
 */
static void adaptive_record_access_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (!g_adaptive_controller || !g_monitoring_enabled) {
        sqlite3_result_null(context);
        return;
    }
    
    if (argc != 1) {
        sqlite3_result_error(context, "Usage: adaptive_record_access(page_id)", -1);
        return;
    }
    
    unsigned int page_id = (unsigned int)sqlite3_value_int(argv[0]);
    adaptive_record_access(g_adaptive_controller, page_id);
    g_query_count++;
    
    sqlite3_result_int(context, page_id);
}

/**
 * SQL Function: adaptive_get_fanout()
 */
static void adaptive_get_fanout_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (!g_adaptive_controller) {
        sqlite3_result_int(context, 64); // Default
        return;
    }
    
    unsigned int fanout = adaptive_get_fanout(g_adaptive_controller);
    sqlite3_result_int(context, fanout);
}

/**
 * SQL Function: adaptive_get_stats()
 * Returns JSON with statistics
 */
static void adaptive_get_stats_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (!g_adaptive_controller) {
        sqlite3_result_text(context, "{\"error\":\"Not initialized\"}", -1, SQLITE_TRANSIENT);
        return;
    }
    
    unsigned int current_fanout = adaptive_get_current_fanout(g_adaptive_controller);
    double cache_rate = adaptive_get_cache_rate(g_adaptive_controller);
    
    char stats[512];
    snprintf(stats, sizeof(stats),
        "{"
        "\"queries\":%llu,"
        "\"current_fanout\":%u,"
        "\"cache_hit_rate\":%.4f"
        "}",
        g_query_count,
        current_fanout,
        cache_rate
    );
    
    sqlite3_result_text(context, stats, -1, SQLITE_TRANSIENT);
}

/**
 * Extension entry point
 */
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_adaptivebtree_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
) {
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    
    // Initialize with default algorithm (regression - the winner!)
    adaptive_init(ADAPTIVE_REGRESSION);
    
    // Register SQL functions
    rc = sqlite3_create_function(db, "adaptive_set_algorithm", 1,
        SQLITE_UTF8 | SQLITE_DETERMINISTIC,
        NULL, adaptive_set_algorithm_func, NULL, NULL);
    
    if (rc == SQLITE_OK) {
        rc = sqlite3_create_function(db, "adaptive_enable", 1,
            SQLITE_UTF8 | SQLITE_DETERMINISTIC,
            NULL, adaptive_enable_func, NULL, NULL);
    }
    
    if (rc == SQLITE_OK) {
        rc = sqlite3_create_function(db, "adaptive_record_access", 1,
            SQLITE_UTF8,
            NULL, adaptive_record_access_func, NULL, NULL);
    }
    
    if (rc == SQLITE_OK) {
        rc = sqlite3_create_function(db, "adaptive_get_fanout", 0,
            SQLITE_UTF8,
            NULL, adaptive_get_fanout_func, NULL, NULL);
    }
    
    if (rc == SQLITE_OK) {
        rc = sqlite3_create_function(db, "adaptive_get_stats", 0,
            SQLITE_UTF8,
            NULL, adaptive_get_stats_func, NULL, NULL);
    }
    
    return rc;
}