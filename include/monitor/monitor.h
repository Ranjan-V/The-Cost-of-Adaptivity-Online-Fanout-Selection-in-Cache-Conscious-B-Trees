#ifndef MONITOR_H
#define MONITOR_H

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <type_traits>

namespace cache_adaptive {

/**
 * Runtime summary for the access monitor.
 */
struct MonitorStats {
    uint64_t total_accesses;
    uint64_t sampled_accesses;
    size_t unique_nodes;
    double avg_heat_score;
    size_t hot_nodes_count;
    size_t cold_nodes_count;
    size_t sampling_rate;
    double alpha;
    double decay_factor;

    MonitorStats()
        : total_accesses(0)
        , sampled_accesses(0)
        , unique_nodes(0)
        , avg_heat_score(0.0)
        , hot_nodes_count(0)
        , cold_nodes_count(0)
        , sampling_rate(1)
        , alpha(0.1)
        , decay_factor(0.99) {}
};

/**
 * Compact metadata for one monitored node.
 *
 * access_count stores sampled touches. Public snapshots report an estimated
 * count by multiplying by the configured sampling rate.
 */
struct NodeStats {
    std::atomic<uint64_t> access_count;
    double heat_score;
    uint64_t last_timestamp;
    uint64_t last_sample_tick;

    NodeStats()
        : access_count(0)
        , heat_score(0.0)
        , last_timestamp(0)
        , last_sample_tick(0) {}

    NodeStats(const NodeStats& other)
        : access_count(other.access_count.load(std::memory_order_relaxed))
        , heat_score(other.heat_score)
        , last_timestamp(other.last_timestamp)
        , last_sample_tick(other.last_sample_tick) {}

    NodeStats& operator=(const NodeStats& other) {
        if (this != &other) {
            access_count.store(other.access_count.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
            heat_score = other.heat_score;
            last_timestamp = other.last_timestamp;
            last_sample_tick = other.last_sample_tick;
        }
        return *this;
    }
};

/**
 * Public snapshot of one tracked node.
 */
struct NodeAccessInfo {
    void* node_ptr;
    size_t node_id;
    uint64_t access_count;
    double heat_score;
    uint64_t last_timestamp;

    NodeAccessInfo(void* ptr = NULL,
                   uint64_t count = 0,
                   double heat = 0.0,
                   uint64_t timestamp = 0)
        : node_ptr(ptr)
        , node_id(decode_node_id(ptr))
        , access_count(count)
        , heat_score(heat)
        , last_timestamp(timestamp) {}

private:
    static size_t decode_node_id(void* ptr) {
        const uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
        return static_cast<size_t>((raw & static_cast<uintptr_t>(1)) ?
                                   (raw >> 1) : raw);
    }
};

/**
 * Low-contention monitor for B-Tree node accesses.
 *
 * Design goals:
 * - No global mutex on the hot path. Metadata is split across fixed shards.
 * - Every access increments an atomic total counter, but only 1/N accesses
 *   touch the unordered_map by default.
 * - Heat uses a sampled exponential moving average:
 *     heat_new = alpha_eff * 1 + (1 - alpha_eff) * heat_old
 *   where alpha_eff compensates for the sampling interval.
 * - Idle nodes are decayed lazily when read or touched, plus decay_all() for
 *   explicit periodic sweeps.
 */
class AccessMonitor {
private:
    enum { kShardCount = 64 };

    class SpinLock {
    private:
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

    public:
        SpinLock() {}

        void lock() {
            while (flag_.test_and_set(std::memory_order_acquire)) {
            }
        }

        void unlock() {
            flag_.clear(std::memory_order_release);
        }
    };

    class ScopedLock {
    private:
        SpinLock& lock_;

    public:
        explicit ScopedLock(SpinLock& lock)
            : lock_(lock) {
            lock_.lock();
        }

        ~ScopedLock() {
            lock_.unlock();
        }

        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;
    };

    struct Shard {
        mutable SpinLock lock;
        std::unordered_map<void*, NodeStats> nodes;
    };

    std::array<Shard, kShardCount> shards_;
    std::atomic<uint64_t> total_accesses_;
    std::atomic<uint64_t> sampled_accesses_;

    double alpha_;
    double effective_alpha_;
    double decay_factor_;
    size_t sampling_rate_;
    double hot_threshold_;
    double cold_threshold_;

public:
    explicit AccessMonitor(size_t window_size = 10000,
                           double alpha = 0.1,
                           size_t sampling_rate = 10,
                           double decay_factor = 0.99)
        : total_accesses_(0)
        , sampled_accesses_(0)
        , alpha_(clamp_probability(alpha, 0.1))
        , effective_alpha_(alpha_)
        , decay_factor_(clamp_probability(decay_factor, 0.99))
        , sampling_rate_(sampling_rate == 0 ? 1 : sampling_rate)
        , hot_threshold_(0.7)
        , cold_threshold_(0.3) {
        // Retain the legacy constructor parameter without pretending that the
        // sampled EMA is a fixed-size window.
        (void)window_size;
        effective_alpha_ = 1.0 - std::pow(1.0 - alpha_,
                                          static_cast<double>(sampling_rate_));
        effective_alpha_ = clamp_probability(effective_alpha_, alpha_);
    }

    AccessMonitor(const AccessMonitor&) = delete;
    AccessMonitor& operator=(const AccessMonitor&) = delete;

    void record_access(void* node_ptr) {
        record_access_ptr(node_ptr);
    }

    template<typename NodeKey>
    void record_access(NodeKey node_key) {
        record_access_ptr(normalize_node(node_key));
    }

    uint64_t get_access_count(void* node_ptr) const {
        return get_access_count_ptr(node_ptr);
    }

    template<typename NodeKey>
    uint64_t get_access_count(NodeKey node_key) const {
        return get_access_count_ptr(normalize_node(node_key));
    }

    double get_heat_score(void* node_ptr) const {
        return get_heat_score_ptr(node_ptr);
    }

    template<typename NodeKey>
    double get_heat_score(NodeKey node_key) const {
        return get_heat_score_ptr(normalize_node(node_key));
    }

    bool is_hot_node(void* node_ptr) const {
        return get_heat_score_ptr(node_ptr) >= hot_threshold_;
    }

    template<typename NodeKey>
    bool is_hot_node(NodeKey node_key) const {
        return is_hot_node(normalize_node(node_key));
    }

    bool is_cold_node(void* node_ptr) const {
        const double heat = get_heat_score_ptr(node_ptr);
        return heat > 0.0 && heat <= cold_threshold_;
    }

    template<typename NodeKey>
    bool is_cold_node(NodeKey node_key) const {
        return is_cold_node(normalize_node(node_key));
    }

    std::vector<NodeAccessInfo> get_hottest_nodes(size_t n) const {
        std::vector<NodeAccessInfo> nodes = get_tracked_nodes();

        std::sort(nodes.begin(), nodes.end(),
            [](const NodeAccessInfo& a, const NodeAccessInfo& b) {
                if (a.heat_score == b.heat_score) {
                    return a.access_count > b.access_count;
                }
                return a.heat_score > b.heat_score;
            });

        if (nodes.size() > n) {
            nodes.resize(n);
        }

        return nodes;
    }

    std::vector<std::pair<void*, double> > get_hottest_node_pairs(size_t n) const {
        std::vector<NodeAccessInfo> hot = get_hottest_nodes(n);
        std::vector<std::pair<void*, double> > result;
        result.reserve(hot.size());

        for (size_t i = 0; i < hot.size(); ++i) {
            result.push_back(std::make_pair(hot[i].node_ptr, hot[i].heat_score));
        }

        return result;
    }

    std::vector<NodeAccessInfo> get_tracked_nodes() const {
        std::vector<NodeAccessInfo> result;
        const uint64_t now_tick = sampled_accesses_.load(std::memory_order_relaxed);

        for (size_t i = 0; i < kShardCount; ++i) {
            const Shard& shard = shards_[i];
            ScopedLock lock(shard.lock);

            for (std::unordered_map<void*, NodeStats>::const_iterator it =
                     shard.nodes.begin();
                 it != shard.nodes.end();
                 ++it) {
                const NodeStats& stats = it->second;
                const uint64_t sampled_count =
                    stats.access_count.load(std::memory_order_relaxed);
                const uint64_t estimated_count =
                    sampled_count * static_cast<uint64_t>(sampling_rate_);
                const double heat =
                    apply_lazy_decay(stats.heat_score,
                                     stats.last_sample_tick,
                                     now_tick);

                result.push_back(NodeAccessInfo(it->first,
                                                estimated_count,
                                                heat,
                                                stats.last_timestamp));
            }
        }

        return result;
    }

    MonitorStats get_monitor_stats() const {
        MonitorStats stats;
        stats.total_accesses = total_accesses_.load(std::memory_order_relaxed);
        stats.sampled_accesses = sampled_accesses_.load(std::memory_order_relaxed);
        stats.sampling_rate = sampling_rate_;
        stats.alpha = alpha_;
        stats.decay_factor = decay_factor_;

        std::vector<NodeAccessInfo> nodes = get_tracked_nodes();
        stats.unique_nodes = nodes.size();

        if (nodes.empty()) {
            return stats;
        }

        double total_heat = 0.0;
        for (size_t i = 0; i < nodes.size(); ++i) {
            total_heat += nodes[i].heat_score;
            if (nodes[i].heat_score >= hot_threshold_) {
                ++stats.hot_nodes_count;
            } else if (nodes[i].heat_score > 0.0 &&
                       nodes[i].heat_score <= cold_threshold_) {
                ++stats.cold_nodes_count;
            }
        }

        stats.avg_heat_score = total_heat / static_cast<double>(nodes.size());
        return stats;
    }

    MonitorStats get_stats() const {
        return get_monitor_stats();
    }

    void decay_all() {
        decay_all(decay_factor_);
    }

    void decay_all(double factor) {
        const double bounded_factor = clamp_probability(factor, decay_factor_);
        const uint64_t now_tick = sampled_accesses_.load(std::memory_order_relaxed);
        const uint64_t now_time = current_time_nanos();

        for (size_t i = 0; i < kShardCount; ++i) {
            Shard& shard = shards_[i];
            ScopedLock lock(shard.lock);

            for (std::unordered_map<void*, NodeStats>::iterator it =
                     shard.nodes.begin();
                 it != shard.nodes.end();
                 ++it) {
                NodeStats& stats = it->second;
                stats.heat_score = clamp_heat(stats.heat_score * bounded_factor);
                stats.last_sample_tick = now_tick;
                stats.last_timestamp = now_time;
            }
        }
    }

    void clear() {
        for (size_t i = 0; i < kShardCount; ++i) {
            Shard& shard = shards_[i];
            ScopedLock lock(shard.lock);
            shard.nodes.clear();
        }

        total_accesses_.store(0, std::memory_order_relaxed);
        sampled_accesses_.store(0, std::memory_order_relaxed);
    }

    uint64_t get_total_accesses() const {
        return total_accesses_.load(std::memory_order_relaxed);
    }

    uint64_t get_sampled_accesses() const {
        return sampled_accesses_.load(std::memory_order_relaxed);
    }

    size_t get_unique_node_count() const {
        size_t count = 0;
        for (size_t i = 0; i < kShardCount; ++i) {
            const Shard& shard = shards_[i];
            ScopedLock lock(shard.lock);
            count += shard.nodes.size();
        }
        return count;
    }

    size_t get_sampling_rate() const {
        return sampling_rate_;
    }

    double get_alpha() const {
        return alpha_;
    }

    double get_decay_factor() const {
        return decay_factor_;
    }

    void print_summary() const {
        MonitorStats stats = get_monitor_stats();

        std::cout << "\n=== Access Monitor Summary ===" << std::endl;
        std::cout << "Total Accesses: " << stats.total_accesses << std::endl;
        std::cout << "Sampled Accesses: " << stats.sampled_accesses << std::endl;
        std::cout << "Sampling Rate: 1/" << stats.sampling_rate << std::endl;
        std::cout << "Unique Nodes: " << stats.unique_nodes << std::endl;
        std::cout << "Average Heat: " << stats.avg_heat_score << std::endl;
        std::cout << "Hot Nodes (>=" << hot_threshold_ << "): "
                  << stats.hot_nodes_count << std::endl;
        std::cout << "Cold Nodes (<=" << cold_threshold_ << "): "
                  << stats.cold_nodes_count << std::endl;

        std::vector<NodeAccessInfo> hot = get_hottest_nodes(5);
        if (!hot.empty()) {
            std::cout << "\nTop " << hot.size() << " Hottest Nodes:" << std::endl;
            for (size_t i = 0; i < hot.size(); ++i) {
                std::cout << "  #" << (i + 1)
                          << " Node " << hot[i].node_id
                          << ": " << hot[i].access_count
                          << " estimated accesses, heat="
                          << hot[i].heat_score << std::endl;
            }
        }

        std::cout << "==============================\n" << std::endl;
    }

private:
    void record_access_ptr(void* node_ptr) {
        if (node_ptr == NULL) {
            return;
        }

        const uint64_t total =
            total_accesses_.fetch_add(1, std::memory_order_relaxed) + 1;

        if (sampling_rate_ > 1 && (total % sampling_rate_) != 0) {
            return;
        }

        const uint64_t sample_tick =
            sampled_accesses_.fetch_add(1, std::memory_order_relaxed) + 1;
        Shard& shard = shards_[shard_index(node_ptr)];
        const uint64_t now_time = current_time_nanos();

        ScopedLock lock(shard.lock);
        NodeStats& stats = shard.nodes[node_ptr];

        const double decayed_heat =
            apply_lazy_decay(stats.heat_score,
                             stats.last_sample_tick,
                             sample_tick);
        const double updated_heat =
            effective_alpha_ + (1.0 - effective_alpha_) * decayed_heat;

        stats.access_count.fetch_add(1, std::memory_order_relaxed);
        stats.heat_score = clamp_heat(updated_heat);
        stats.last_timestamp = now_time;
        stats.last_sample_tick = sample_tick;
    }

    uint64_t get_access_count_ptr(void* node_ptr) const {
        if (node_ptr == NULL) {
            return 0;
        }

        const Shard& shard = shards_[shard_index(node_ptr)];
        ScopedLock lock(shard.lock);

        std::unordered_map<void*, NodeStats>::const_iterator it =
            shard.nodes.find(node_ptr);
        if (it == shard.nodes.end()) {
            return 0;
        }

        const uint64_t sampled_count =
            it->second.access_count.load(std::memory_order_relaxed);
        return sampled_count * static_cast<uint64_t>(sampling_rate_);
    }

    double get_heat_score_ptr(void* node_ptr) const {
        if (node_ptr == NULL) {
            return 0.0;
        }

        const uint64_t now_tick = sampled_accesses_.load(std::memory_order_relaxed);
        const Shard& shard = shards_[shard_index(node_ptr)];
        ScopedLock lock(shard.lock);

        std::unordered_map<void*, NodeStats>::const_iterator it =
            shard.nodes.find(node_ptr);
        if (it == shard.nodes.end()) {
            return 0.0;
        }

        return apply_lazy_decay(it->second.heat_score,
                                it->second.last_sample_tick,
                                now_tick);
    }

    size_t shard_index(void* node_ptr) const {
        const uintptr_t raw = reinterpret_cast<uintptr_t>(node_ptr);
        return static_cast<size_t>((raw >> 3) % kShardCount);
    }

    static uint64_t current_time_nanos() {
        typedef std::chrono::high_resolution_clock Clock;
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now().time_since_epoch()).count());
    }

    static double clamp_probability(double value, double fallback) {
        if (value != value) {
            return fallback;
        }
        if (value < 0.0) {
            return 0.0;
        }
        if (value > 1.0) {
            return 1.0;
        }
        return value;
    }

    static double clamp_heat(double value) {
        if (value != value || value < 0.0) {
            return 0.0;
        }
        if (value > 1.0) {
            return 1.0;
        }
        return value;
    }

    double apply_lazy_decay(double heat,
                            uint64_t last_tick,
                            uint64_t now_tick) const {
        if (heat <= 0.0 || decay_factor_ >= 1.0 || now_tick <= last_tick) {
            return clamp_heat(heat);
        }

        const uint64_t missed_ticks = now_tick - last_tick;
        const double factor =
            std::pow(decay_factor_, static_cast<double>(missed_ticks));
        return clamp_heat(heat * factor);
    }

    template<typename T>
    static typename std::enable_if<std::is_pointer<T>::value, void*>::type
    normalize_node(T node_key) {
        return const_cast<void*>(static_cast<const void*>(node_key));
    }

    static void* normalize_node(std::nullptr_t) {
        return NULL;
    }

    template<typename T>
    static typename std::enable_if<!std::is_pointer<T>::value, void*>::type
    normalize_node(T node_key) {
        const uintptr_t raw = static_cast<uintptr_t>(node_key);
        return reinterpret_cast<void*>(
            (raw << 1) | static_cast<uintptr_t>(1));
    }
};

} // namespace cache_adaptive

#endif // MONITOR_H
