#ifndef SEGMENTED_ADAPTIVE_BTREE_H
#define SEGMENTED_ADAPTIVE_BTREE_H

#include "../btree/btree.h"
#include "../monitor/monitor.h"
#include "../predictor/predictor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cache_adaptive {

struct SegmentedAdaptiveSegmentStats {
    size_t segment_id;
    size_t fanout;
    size_t records;
    size_t total_accesses;
    size_t sampled_accesses;
    size_t unique_nodes;
    size_t adaptations;
    size_t fanout_increases;
    size_t fanout_decreases;
    size_t fanout_maintained;
    size_t restructures;
    size_t rebuilds_skipped_hysteresis;
    size_t rebuilds_skipped_cost;
    size_t rebuilds_skipped_cooldown;
    double avg_heat;
    double cache_hit_rate;
    double last_estimated_benefit;
    double last_estimated_cost;

    SegmentedAdaptiveSegmentStats()
        : segment_id(0)
        , fanout(0)
        , records(0)
        , total_accesses(0)
        , sampled_accesses(0)
        , unique_nodes(0)
        , adaptations(0)
        , fanout_increases(0)
        , fanout_decreases(0)
        , fanout_maintained(0)
        , restructures(0)
        , rebuilds_skipped_hysteresis(0)
        , rebuilds_skipped_cost(0)
        , rebuilds_skipped_cooldown(0)
        , avg_heat(0.0)
        , cache_hit_rate(0.0)
        , last_estimated_benefit(0.0)
        , last_estimated_cost(0.0) {}
};

struct SegmentedAdaptiveStats {
    size_t segment_count;
    size_t total_records;
    size_t total_accesses;
    size_t total_adaptations;
    size_t fanout_increases;
    size_t fanout_decreases;
    size_t fanout_maintained;
    size_t total_restructures;
    size_t rebuilds_skipped_hysteresis;
    size_t rebuilds_skipped_cost;
    size_t rebuilds_skipped_cooldown;
    double avg_fanout;
    double cache_hit_rate;

    SegmentedAdaptiveStats()
        : segment_count(0)
        , total_records(0)
        , total_accesses(0)
        , total_adaptations(0)
        , fanout_increases(0)
        , fanout_decreases(0)
        , fanout_maintained(0)
        , total_restructures(0)
        , rebuilds_skipped_hysteresis(0)
        , rebuilds_skipped_cost(0)
        , rebuilds_skipped_cooldown(0)
        , avg_fanout(0.0)
        , cache_hit_rate(0.0) {}
};

template<typename KeyType, typename ValueType>
class SegmentedAdaptiveBPlusTree {
private:
    struct Segment {
        cabtree::BPlusTree<KeyType, ValueType>* tree;
        AccessMonitor monitor;
        FanoutPredictor predictor;
        std::unordered_map<KeyType, ValueType> records;

        size_t current_fanout;
        size_t access_count;
        size_t cache_hits;
        size_t adaptations;
        size_t fanout_increases;
        size_t fanout_decreases;
        size_t fanout_maintained;
        size_t restructures;
        size_t last_restructure_access;
        size_t next_adaptation_access;
        size_t pending_fanout;
        size_t pending_votes;
        size_t rebuilds_skipped_hysteresis;
        size_t rebuilds_skipped_cost;
        size_t rebuilds_skipped_cooldown;
        double last_estimated_benefit;
        double last_estimated_cost;

        Segment(size_t initial_fanout,
                size_t min_fanout,
                size_t max_fanout)
            : tree(new cabtree::BPlusTree<KeyType, ValueType>(
                  static_cast<int>(initial_fanout)))
            , monitor(10000, 0.1, 1, 0.99)
            , predictor(min_fanout, max_fanout, initial_fanout)
            , records()
            , current_fanout(initial_fanout)
            , access_count(0)
            , cache_hits(0)
            , adaptations(0)
            , fanout_increases(0)
            , fanout_decreases(0)
            , fanout_maintained(0)
            , restructures(0)
            , last_restructure_access(0)
            , next_adaptation_access(0)
            , pending_fanout(initial_fanout)
            , pending_votes(0)
            , rebuilds_skipped_hysteresis(0)
            , rebuilds_skipped_cost(0)
            , rebuilds_skipped_cooldown(0)
            , last_estimated_benefit(0.0)
            , last_estimated_cost(0.0) {}

        ~Segment() {
            delete tree;
        }

    private:
        Segment(const Segment&);
        Segment& operator=(const Segment&);
    };

    std::vector<Segment*> segments_;
    size_t segment_count_;
    KeyType min_key_;
    KeyType max_key_;
    size_t initial_fanout_;
    size_t adaptation_interval_;
    size_t min_fanout_;
    size_t max_fanout_;
    size_t heat_check_interval_;
    size_t monitor_sampling_rate_;
    size_t min_restructure_interval_;
    bool adaptation_enabled_;

public:
    SegmentedAdaptiveBPlusTree(size_t segment_count = 8,
                               KeyType min_key = KeyType(),
                               KeyType max_key = static_cast<KeyType>(1000000),
                               size_t initial_fanout = 64,
                               size_t adaptation_interval = 1000,
                               size_t min_fanout = 8,
                               size_t max_fanout = 256,
                               size_t monitor_sampling_rate = 10)
        : segments_()
        , segment_count_(segment_count == 0 ? 1 : segment_count)
        , min_key_(min_key)
        , max_key_(max_key)
        , initial_fanout_(initial_fanout)
        , adaptation_interval_(adaptation_interval)
        , min_fanout_(min_fanout)
        , max_fanout_(max_fanout)
        , heat_check_interval_(64)
        , monitor_sampling_rate_(monitor_sampling_rate == 0 ? 1 : monitor_sampling_rate)
        , min_restructure_interval_(std::max<size_t>(1, adaptation_interval * 4))
        , adaptation_enabled_(true) {
        static_assert(std::is_arithmetic<KeyType>::value,
                      "SegmentedAdaptiveBPlusTree requires arithmetic keys");

        if (max_key_ < min_key_) {
            throw std::invalid_argument("max_key must be >= min_key");
        }
        if (initial_fanout_ < 3) {
            initial_fanout_ = 3;
        }
        if (min_fanout_ < 3) {
            min_fanout_ = 3;
        }
        if (max_fanout_ < min_fanout_) {
            max_fanout_ = min_fanout_;
        }

        segments_.reserve(segment_count_);
        for (size_t i = 0; i < segment_count_; ++i) {
            segments_.push_back(new Segment(initial_fanout_,
                                            min_fanout_,
                                            max_fanout_));
        }
    }

    ~SegmentedAdaptiveBPlusTree() {
        for (size_t i = 0; i < segments_.size(); ++i) {
            delete segments_[i];
        }
    }

    void insert(const KeyType& key, const ValueType& value) {
        const size_t segment_id = get_segment_index(key);
        Segment& segment = *segments_[segment_id];
        record_segment_access(segment, key);

        segment.records[key] = value;
        segment.tree->insert(key, value);

        if (should_adapt(segment)) {
            adapt_segment(segment);
        }
    }

    bool search(const KeyType& key, ValueType& value) {
        const size_t segment_id = get_segment_index(key);
        Segment& segment = *segments_[segment_id];
        record_segment_access(segment, key);

        if (segment.access_count % heat_check_interval_ == 0) {
            double heat = segment.monitor.get_heat_score(key);
            if (heat > 0.7) {
                segment.cache_hits += heat_check_interval_;
            }
        }

        bool found = segment.tree->search(key, value);
        if (!found) {
            typename std::unordered_map<KeyType, ValueType>::const_iterator it =
                segment.records.find(key);
            if (it != segment.records.end()) {
                value = it->second;
                found = true;
            }
        }

        if (should_adapt(segment)) {
            adapt_segment(segment);
        }

        return found;
    }

    std::vector<ValueType> range_query(const KeyType& start,
                                       const KeyType& end) {
        std::vector<ValueType> results;
        if (end < start) {
            return results;
        }

        size_t first = get_segment_index(start);
        size_t last = get_segment_index(end);
        if (last < first) {
            return results;
        }

        for (size_t i = first; i <= last; ++i) {
            Segment& segment = *segments_[i];
            record_segment_access(segment, start);
            std::vector<ValueType> partial =
                segment.tree->range_query(start, end);
            results.insert(results.end(), partial.begin(), partial.end());

            if (should_adapt(segment)) {
                adapt_segment(segment);
            }
        }

        return results;
    }

    size_t size() const {
        size_t total = 0;
        for (size_t i = 0; i < segments_.size(); ++i) {
            total += segments_[i]->records.size();
        }
        return total;
    }

    size_t segment_count() const {
        return segment_count_;
    }

    size_t get_segment_index(const KeyType& key) const {
        if (key <= min_key_) {
            return 0;
        }
        if (key >= max_key_) {
            return segment_count_ - 1;
        }

        const long double min_value = static_cast<long double>(min_key_);
        const long double max_value = static_cast<long double>(max_key_);
        const long double key_value = static_cast<long double>(key);
        const long double span = (max_value - min_value) + 1.0L;
        const long double offset = key_value - min_value;

        size_t segment =
            static_cast<size_t>((offset * segment_count_) / span);
        if (segment >= segment_count_) {
            segment = segment_count_ - 1;
        }
        return segment;
    }

    size_t get_segment_fanout(size_t segment_id) const {
        if (segment_id >= segments_.size()) {
            return 0;
        }
        return segments_[segment_id]->current_fanout;
    }

    size_t get_average_fanout() const {
        if (segments_.empty()) {
            return 0;
        }
        size_t total = 0;
        for (size_t i = 0; i < segments_.size(); ++i) {
            total += segments_[i]->current_fanout;
        }
        return total / segments_.size();
    }

    SegmentedAdaptiveSegmentStats get_segment_stats(size_t segment_id) const {
        SegmentedAdaptiveSegmentStats stats;
        if (segment_id >= segments_.size()) {
            return stats;
        }

        const Segment& segment = *segments_[segment_id];
        MonitorStats monitor_stats = segment.monitor.get_monitor_stats();
        stats.segment_id = segment_id;
        stats.fanout = segment.current_fanout;
        stats.records = segment.records.size();
        stats.total_accesses = segment.access_count;
        stats.sampled_accesses = monitor_stats.sampled_accesses;
        stats.unique_nodes = monitor_stats.unique_nodes;
        stats.adaptations = segment.adaptations;
        stats.fanout_increases = segment.fanout_increases;
        stats.fanout_decreases = segment.fanout_decreases;
        stats.fanout_maintained = segment.fanout_maintained;
        stats.restructures = segment.restructures;
        stats.rebuilds_skipped_hysteresis = segment.rebuilds_skipped_hysteresis;
        stats.rebuilds_skipped_cost = segment.rebuilds_skipped_cost;
        stats.rebuilds_skipped_cooldown = segment.rebuilds_skipped_cooldown;
        stats.avg_heat = monitor_stats.avg_heat_score;
        stats.cache_hit_rate = segment.access_count > 0
            ? static_cast<double>(segment.cache_hits) /
                  static_cast<double>(segment.access_count)
            : 0.0;
        stats.last_estimated_benefit = segment.last_estimated_benefit;
        stats.last_estimated_cost = segment.last_estimated_cost;
        return stats;
    }

    SegmentedAdaptiveStats get_stats() const {
        SegmentedAdaptiveStats stats;
        stats.segment_count = segments_.size();

        size_t total_fanout = 0;
        size_t total_cache_hits = 0;
        for (size_t i = 0; i < segments_.size(); ++i) {
            const Segment& segment = *segments_[i];
            stats.total_records += segment.records.size();
            stats.total_accesses += segment.access_count;
            stats.total_adaptations += segment.adaptations;
            stats.fanout_increases += segment.fanout_increases;
            stats.fanout_decreases += segment.fanout_decreases;
            stats.fanout_maintained += segment.fanout_maintained;
            stats.total_restructures += segment.restructures;
            stats.rebuilds_skipped_hysteresis += segment.rebuilds_skipped_hysteresis;
            stats.rebuilds_skipped_cost += segment.rebuilds_skipped_cost;
            stats.rebuilds_skipped_cooldown += segment.rebuilds_skipped_cooldown;
            total_fanout += segment.current_fanout;
            total_cache_hits += segment.cache_hits;
        }

        stats.avg_fanout = segments_.empty()
            ? 0.0
            : static_cast<double>(total_fanout) /
                  static_cast<double>(segments_.size());
        stats.cache_hit_rate = stats.total_accesses > 0
            ? static_cast<double>(total_cache_hits) /
                  static_cast<double>(stats.total_accesses)
            : 0.0;
        return stats;
    }

    void set_adaptation_enabled(bool enabled) {
        adaptation_enabled_ = enabled;
    }

    void reset_adaptation_state() {
        for (size_t i = 0; i < segments_.size(); ++i) {
            Segment& segment = *segments_[i];
            segment.monitor.clear();
            segment.predictor.reset();
            segment.access_count = 0;
            segment.cache_hits = 0;
            segment.adaptations = 0;
            segment.fanout_increases = 0;
            segment.fanout_decreases = 0;
            segment.fanout_maintained = 0;
            segment.restructures = 0;
            segment.last_restructure_access = 0;
            segment.next_adaptation_access = 0;
            segment.pending_fanout = segment.current_fanout;
            segment.pending_votes = 0;
            segment.rebuilds_skipped_hysteresis = 0;
            segment.rebuilds_skipped_cost = 0;
            segment.rebuilds_skipped_cooldown = 0;
            segment.last_estimated_benefit = 0.0;
            segment.last_estimated_cost = 0.0;
        }
    }

    void print_summary() const {
        SegmentedAdaptiveStats stats = get_stats();
        std::cout << "\n========================================\n"
                  << "  Segmented Adaptive B-Tree Summary\n"
                  << "========================================\n"
                  << "Segments: " << stats.segment_count << "\n"
                  << "Records: " << stats.total_records << "\n"
                  << "Total Accesses: " << stats.total_accesses << "\n"
                  << "Average Fanout: " << stats.avg_fanout << "\n"
                  << "Cache Hit Rate: " << (stats.cache_hit_rate * 100.0)
                  << "%\n"
                  << "Total Adaptations: " << stats.total_adaptations << "\n"
                  << "Total Restructures: " << stats.total_restructures
                  << "\nSkipped By Hysteresis: "
                  << stats.rebuilds_skipped_hysteresis
                  << "\nSkipped By Cost: "
                  << stats.rebuilds_skipped_cost
                  << "\nSkipped By Cooldown: "
                  << stats.rebuilds_skipped_cooldown
                  << "\n";

        for (size_t i = 0; i < segments_.size(); ++i) {
            SegmentedAdaptiveSegmentStats segment = get_segment_stats(i);
            std::cout << "  Segment " << i
                      << ": fanout=" << segment.fanout
                      << ", records=" << segment.records
                      << ", accesses=" << segment.total_accesses
                      << ", adaptations=" << segment.adaptations
                      << ", restructures=" << segment.restructures
                      << ", heat=" << segment.avg_heat
                      << "\n";
        }
        std::cout << "========================================\n" << std::endl;
    }

private:
    void record_segment_access(Segment& segment, const KeyType& key) {
        ++segment.access_count;
        if (monitor_sampling_rate_ == 1 ||
            (segment.access_count % monitor_sampling_rate_) == 0) {
            segment.monitor.record_access(key);
        }
    }

    bool should_adapt(const Segment& segment) const {
        return adaptation_enabled_ &&
               adaptation_interval_ > 0 &&
               segment.access_count > 0 &&
               segment.access_count >= segment.next_adaptation_access &&
               (segment.access_count % adaptation_interval_ == 0);
    }

    bool can_restructure(const Segment& segment) const {
        return segment.restructures == 0 ||
               segment.access_count >=
                   segment.last_restructure_access +
                   min_restructure_interval_;
    }

    void adapt_segment(Segment& segment) {
        ++segment.adaptations;

        std::vector<NodeAccessInfo> nodes = segment.monitor.get_tracked_nodes();
        if (nodes.empty()) {
            ++segment.fanout_maintained;
            defer_adaptation(segment, 1);
            return;
        }
        if (segment.monitor.get_sampled_accesses() < 20) {
            ++segment.fanout_maintained;
            defer_adaptation(segment, 1);
            return;
        }

        double avg_heat = 0.0;
        double max_heat = 0.0;
        double weighted_heat = 0.0;
        size_t total_accesses = 0;
        size_t max_node_accesses = 0;
        size_t hot_nodes = 0;
        size_t cold_nodes = 0;

        for (size_t i = 0; i < nodes.size(); ++i) {
            const NodeAccessInfo& node = nodes[i];
            avg_heat += node.heat_score;
            max_heat = std::max(max_heat, node.heat_score);
            if (node.heat_score >= 0.7) {
                ++hot_nodes;
            } else if (node.heat_score > 0.0 && node.heat_score <= 0.3) {
                ++cold_nodes;
            }

            size_t node_accesses = static_cast<size_t>(node.access_count);
            total_accesses += node_accesses;
            max_node_accesses = std::max(max_node_accesses, node_accesses);
            weighted_heat += node.heat_score *
                             static_cast<double>(node_accesses);
        }

        avg_heat /= static_cast<double>(nodes.size());
        double weighted_heat_score = total_accesses > 0
            ? weighted_heat / static_cast<double>(total_accesses)
            : avg_heat;
        double dominant_access_fraction = total_accesses > 0
            ? static_cast<double>(max_node_accesses) /
                  static_cast<double>(total_accesses)
            : 0.0;
        double hot_fraction =
            static_cast<double>(hot_nodes) / static_cast<double>(nodes.size());
        double cold_fraction =
            static_cast<double>(cold_nodes) / static_cast<double>(nodes.size());

        double signal_heat = std::max(avg_heat, weighted_heat_score);
        if (dominant_access_fraction < 0.02 &&
            nodes.size() >= 16 &&
            hot_fraction < 0.10) {
            signal_heat = std::min(signal_heat, 0.25);
        } else if (dominant_access_fraction >= 0.02 ||
                   hot_fraction >= 0.05) {
            signal_heat = std::max(signal_heat,
                                   std::max(0.72,
                                            0.65 * max_heat +
                                            0.35 * weighted_heat_score));
        } else if (dominant_access_fraction < 0.005 &&
                   cold_fraction > 0.60 &&
                   hot_fraction < 0.02) {
            signal_heat = std::min(signal_heat, avg_heat * 0.5);
        }

        size_t avg_access_count = total_accesses / nodes.size();
        FanoutRecommendation prediction =
            segment.predictor.predict_fanout(signal_heat, avg_access_count);
        size_t target_fanout =
            choose_target_fanout(signal_heat,
                                 dominant_access_fraction,
                                 prediction.recommended_fanout);

        if (target_fanout == segment.current_fanout) {
            ++segment.fanout_maintained;
            segment.pending_fanout = segment.current_fanout;
            segment.pending_votes = 0;
            defer_adaptation(segment, 2);
            return;
        }

        if (!same_direction(segment.current_fanout,
                            segment.pending_fanout,
                            target_fanout)) {
            segment.pending_fanout = target_fanout;
            segment.pending_votes = 1;
        } else if (segment.pending_fanout == target_fanout) {
            ++segment.pending_votes;
        } else {
            segment.pending_fanout = target_fanout;
            segment.pending_votes = 1;
        }

        if (segment.pending_votes < required_votes(target_fanout, segment.current_fanout)) {
            ++segment.rebuilds_skipped_hysteresis;
            ++segment.fanout_maintained;
            defer_adaptation(segment, 1);
            return;
        }

        if (!can_restructure(segment)) {
            ++segment.rebuilds_skipped_cooldown;
            ++segment.fanout_maintained;
            segment.next_adaptation_access =
                segment.last_restructure_access + min_restructure_interval_;
            return;
        }

        if (!passes_cost_gate(segment,
                              target_fanout,
                              signal_heat,
                              dominant_access_fraction,
                              hot_fraction,
                              nodes.size())) {
            ++segment.rebuilds_skipped_cost;
            ++segment.fanout_maintained;
            defer_adaptation(segment, 4);
            return;
        }

        if (target_fanout < segment.current_fanout) {
            ++segment.fanout_decreases;
        } else {
            ++segment.fanout_increases;
        }
        rebuild_segment(segment, target_fanout);
        defer_adaptation(segment, 2);
    }

    void defer_adaptation(Segment& segment, size_t intervals) {
        if (adaptation_interval_ == 0) {
            segment.next_adaptation_access = segment.access_count;
            return;
        }
        segment.next_adaptation_access =
            segment.access_count + adaptation_interval_ * std::max<size_t>(1, intervals);
    }

    bool same_direction(size_t current_fanout,
                        size_t previous_target,
                        size_t new_target) const {
        if (previous_target == current_fanout || new_target == current_fanout) {
            return previous_target == new_target;
        }
        return (previous_target < current_fanout && new_target < current_fanout) ||
               (previous_target > current_fanout && new_target > current_fanout);
    }

    size_t required_votes(size_t target_fanout, size_t current_fanout) const {
        const size_t larger = std::max(target_fanout, current_fanout);
        const size_t smaller = std::max<size_t>(1, std::min(target_fanout, current_fanout));
        return (larger >= smaller * 4) ? 2 : 3;
    }

    bool passes_cost_gate(Segment& segment,
                          size_t target_fanout,
                          double signal_heat,
                          double dominant_access_fraction,
                          double hot_fraction,
                          size_t unique_nodes) {
        const bool shrinking = target_fanout < segment.current_fanout;
        const bool growing = target_fanout > segment.current_fanout;

        if (shrinking) {
            const bool concentrated =
                signal_heat >= 0.68 &&
                (dominant_access_fraction >= 0.08 || hot_fraction >= 0.15);
            if (!concentrated) {
                segment.last_estimated_benefit = 0.0;
                segment.last_estimated_cost = estimate_rebuild_cost(segment);
                return false;
            }
        }

        if (growing) {
            const bool started_too_small = segment.current_fanout < 64;
            const bool broad_access =
                signal_heat <= 0.35 &&
                unique_nodes >= 16 &&
                dominant_access_fraction < 0.02;
            if (!started_too_small || !broad_access) {
                segment.last_estimated_benefit = 0.0;
                segment.last_estimated_cost = estimate_rebuild_cost(segment);
                return false;
            }
        }

        const double benefit = estimate_rebuild_benefit(segment,
                                                        target_fanout,
                                                        signal_heat,
                                                        dominant_access_fraction,
                                                        hot_fraction,
                                                        unique_nodes);
        const double cost = estimate_rebuild_cost(segment);
        segment.last_estimated_benefit = benefit;
        segment.last_estimated_cost = cost;
        return benefit > cost;
    }

    double estimate_rebuild_benefit(const Segment& segment,
                                    size_t target_fanout,
                                    double signal_heat,
                                    double dominant_access_fraction,
                                    double hot_fraction,
                                    size_t unique_nodes) const {
        const size_t current = std::max<size_t>(1, segment.current_fanout);
        const size_t target = std::max<size_t>(1, target_fanout);
        const double fanout_ratio =
            std::log(static_cast<double>(std::max(current, target)) /
                     static_cast<double>(std::min(current, target))) /
            std::log(2.0);
        const size_t since_rebuild =
            segment.restructures == 0
                ? segment.access_count
                : segment.access_count - segment.last_restructure_access;
        const double payback_window =
            static_cast<double>(std::max(adaptation_interval_, since_rebuild));

        double signal_strength = 0.0;
        if (target_fanout < segment.current_fanout) {
            signal_strength =
                std::max(0.0, signal_heat - 0.60) * 3.0 +
                dominant_access_fraction * 12.0 +
                hot_fraction * 2.0;
        } else {
            const double coverage =
                std::min(1.0, static_cast<double>(unique_nodes) / 64.0);
            signal_strength =
                std::max(0.0, 0.40 - signal_heat) * 2.0 + coverage;
        }

        return payback_window * fanout_ratio * signal_strength;
    }

    double estimate_rebuild_cost(const Segment& segment) const {
        const double records = static_cast<double>(segment.records.size());
        if (records <= 1.0) {
            return 64.0;
        }
        return 0.15 * records *
               (1.0 + std::log(records) / std::log(2.0));
    }

    size_t choose_target_fanout(double signal_heat,
                                double dominant_access_fraction,
                                size_t predicted_fanout) const {
        size_t target = predicted_fanout;

        if (signal_heat >= 0.92 && dominant_access_fraction >= 0.30) {
            target = std::min(target, std::max(min_fanout_, static_cast<size_t>(8)));
        } else if (signal_heat >= 0.72 || dominant_access_fraction >= 0.05) {
            target = std::min(max_fanout_,
                              std::max(min_fanout_, static_cast<size_t>(32)));
        } else if (signal_heat >= 0.50) {
            target = std::min(max_fanout_,
                              std::max(min_fanout_, static_cast<size_t>(32)));
        } else if (signal_heat >= 0.30) {
            target = std::min(max_fanout_,
                              std::max(min_fanout_, static_cast<size_t>(64)));
        } else {
            target = std::min(max_fanout_,
                              std::max(static_cast<size_t>(128), target));
        }

        return std::max(min_fanout_, std::min(max_fanout_, target));
    }

    void rebuild_segment(Segment& segment, size_t new_fanout) {
        cabtree::BPlusTree<KeyType, ValueType>* rebuilt =
            new cabtree::BPlusTree<KeyType, ValueType>(
                static_cast<int>(new_fanout));

        for (typename std::unordered_map<KeyType, ValueType>::const_iterator it =
                 segment.records.begin();
             it != segment.records.end();
             ++it) {
            rebuilt->insert(it->first, it->second);
        }

        delete segment.tree;
        segment.tree = rebuilt;
        segment.current_fanout = new_fanout;
        ++segment.restructures;
        segment.last_restructure_access = segment.access_count;
        segment.pending_fanout = new_fanout;
        segment.pending_votes = 0;
    }

private:
    SegmentedAdaptiveBPlusTree(const SegmentedAdaptiveBPlusTree&);
    SegmentedAdaptiveBPlusTree& operator=(const SegmentedAdaptiveBPlusTree&);
};

} // namespace cache_adaptive

#endif // SEGMENTED_ADAPTIVE_BTREE_H
