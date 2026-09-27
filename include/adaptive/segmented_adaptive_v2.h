#ifndef CABTREE_SEGMENTED_ADAPTIVE_V2_H
#define CABTREE_SEGMENTED_ADAPTIVE_V2_H

#include "../btree/btree.h"
#include "../monitor/bounded_monitor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cabtree {

template<typename Key, typename Value>
class SegmentedAdaptiveV2 {
public:
    struct Stats {
        size_t evaluations, maintained, hysteresis_skips, cooldown_skips, cost_skips;
        size_t rebuilds, monitor_events, monitor_samples, monitor_bytes;
        double monitor_ms, rebuild_ms, max_rebuild_ms;
        size_t temp_rebuild_bytes;
        std::vector<std::pair<size_t, size_t> > transitions;
        std::vector<std::string> decision_history;
        Stats() : evaluations(0), maintained(0), hysteresis_skips(0), cooldown_skips(0),
            cost_skips(0), rebuilds(0), monitor_events(0), monitor_samples(0),
            monitor_bytes(0), monitor_ms(0), rebuild_ms(0), max_rebuild_ms(0), temp_rebuild_bytes(0) {}
    };
    SegmentedAdaptiveV2(size_t segments, Key min_key, Key max_key, size_t fanout = 64,
                        size_t interval = 5000, size_t sample_rate = 32,
                        size_t min_fanout = 8, size_t max_fanout = 256,
                        const std::vector<size_t>& candidates = std::vector<size_t>())
        : min_(min_key), max_(max_key), interval_(interval), min_f_(min_fanout),
          max_f_(max_fanout), candidates_(candidates), enabled_(true), global_access_(0) {
        static_assert(std::is_arithmetic<Key>::value, "numeric keys required");
        if (!segments || max_key < min_key || fanout < 3 || min_fanout < 3 || max_fanout < min_fanout)
            throw std::invalid_argument("invalid segmented V2 configuration");
        if (candidates_.empty()) {
            const size_t defaults[] = {8, 16, 32, 64, 128, 256};
            candidates_.assign(defaults, defaults + 6);
        }
        for (size_t i = 0; i < candidates_.size(); ++i) {
            if (candidates_[i] < min_f_ || candidates_[i] > max_f_ ||
                (i && candidates_[i] <= candidates_[i - 1]))
                throw std::invalid_argument("candidate fanouts must be sorted, unique, in range");
        }
        parts_.reserve(segments);
        try {
            for (size_t i = 0; i < segments; ++i)
                parts_.push_back(new Part(fanout, sample_rate));
        } catch (...) {
            for (size_t i = 0; i < parts_.size(); ++i) delete parts_[i];
            throw;
        }
    }
    ~SegmentedAdaptiveV2() { for (size_t i = 0; i < parts_.size(); ++i) delete parts_[i]; }
    void insert(const Key& key, const Value& value) {
        Part& p = *parts_[segment_index(key)];
        observe(p, key); p.tree->insert(key, value); consider(p);
    }
    bool search(const Key& key, Value& value) {
        Part& p = *parts_[segment_index(key)];
        observe(p, key); bool found = p.tree->search(key, value); consider(p); return found;
    }
    std::vector<Value> range_query(const Key& start, const Key& end) {
        std::vector<Value> result;
        if (end < start) return result;
        const size_t first = segment_index(start), last = segment_index(end);
        for (size_t i = first; i <= last; ++i) {
            Part& p = *parts_[i];
            observe(p, start);
            std::vector<Value> chunk = p.tree->range_query(start, end);
            result.insert(result.end(), chunk.begin(), chunk.end()); consider(p);
        }
        return result;
    }
    size_t size() const {
        size_t n = 0; for (size_t i = 0; i < parts_.size(); ++i) n += parts_[i]->tree->size(); return n;
    }
    size_t approximate_tree_bytes() const {
        size_t n = 0;
        for (size_t i = 0; i < parts_.size(); ++i) n += parts_[i]->tree->approximate_owned_bytes();
        return n;
    }
    size_t segment_index(const Key& key) const {
        if (key <= min_) return 0;
        if (key >= max_) return parts_.size() - 1;
        long double span = static_cast<long double>(max_) - static_cast<long double>(min_) + 1.0L;
        size_t index = static_cast<size_t>((static_cast<long double>(key) - min_) * parts_.size() / span);
        return std::min(index, parts_.size() - 1);
    }
    size_t fanout(size_t segment) const { return parts_.at(segment)->fanout; }
    void set_adaptation_enabled(bool yes) { enabled_ = yes; }
    void reset_measurement_state() {
        stats_ = Stats();
        for (size_t i = 0; i < parts_.size(); ++i) {
            parts_[i]->monitor.reset();
            parts_[i]->accesses = 0;
            parts_[i]->last_rebuild = 0;
            parts_[i]->pending = parts_[i]->fanout;
            parts_[i]->votes = 0;
        }
    }
    void force_rebuild(size_t segment, size_t fanout) {
        if (segment >= parts_.size() || fanout < min_f_ || fanout > max_f_)
            throw std::invalid_argument("invalid force_rebuild target");
        if (parts_[segment]->fanout != fanout) rebuild(*parts_[segment], fanout);
    }
    Stats stats() const {
        Stats result = stats_;
        for (size_t i = 0; i < parts_.size(); ++i) {
            result.monitor_events += parts_[i]->monitor.events();
            result.monitor_samples += parts_[i]->monitor.samples();
            result.monitor_bytes += parts_[i]->monitor.memory_bytes();
        }
        return result;
    }
private:
    struct Part {
        BoundedMonitor<Key> monitor;
        BPlusTree<Key, Value>* tree;
        size_t fanout, accesses, last_rebuild, pending, votes;
        Part(size_t f, size_t rate) : monitor(256, 4, 16, rate),
            tree(new BPlusTree<Key, Value>(static_cast<int>(f))),
            fanout(f), accesses(0), last_rebuild(0), pending(f), votes(0) {}
        ~Part() { delete tree; }
        Part(const Part&); Part& operator=(const Part&);
    };
    void observe(Part& p, const Key& key) {
        typedef std::chrono::steady_clock Clock;
        ++p.accesses; ++global_access_;
        if ((p.accesses & 127u) == 0u) {
            Clock::time_point begin = Clock::now();
            p.monitor.record(key);
            stats_.monitor_ms += 128.0 * std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        } else p.monitor.record(key);
    }
    void consider(Part& p) {
        if (!enabled_ || !interval_ || p.accesses % interval_) return;
        ++stats_.evaluations;
        if (p.monitor.samples() < 20) {
            ++stats_.maintained;
            history(p, p.fanout, "insufficient_evidence"); return;
        }
        const double concentration = p.monitor.dominant_fraction();
        size_t target = p.fanout;
        if (concentration >= 0.08 && p.fanout > 32) target = nearest_candidate(32);
        else if (concentration < 0.02 && p.fanout < 64) target = nearest_candidate(64);
        if (target == p.fanout) {
            ++stats_.maintained; p.votes = 0; p.pending = target;
            history(p, target, "maintain"); return;
        }
        if (p.pending != target) { p.pending = target; p.votes = 1; }
        else ++p.votes;
        if (p.votes < 2) { ++stats_.hysteresis_skips; history(p, target, "hysteresis"); return; }
        if (p.last_rebuild && p.accesses - p.last_rebuild < interval_ * 4) {
            ++stats_.cooldown_skips; history(p, target, "cooldown"); return;
        }
        const double ratio = std::log(static_cast<double>(std::max(target, p.fanout)) /
                                      static_cast<double>(std::min(target, p.fanout))) / std::log(2.0);
        const double strength = target < p.fanout ? concentration * 12.0 : (0.4 - concentration) * 2.0;
        const double benefit_score = static_cast<double>(interval_) * ratio * std::max(0.0, strength);
        const double count = static_cast<double>(p.tree->size());
        // V1 scores insert replay as n log n; V2's bulk rebuild is linear.
        // These are heuristic scores, not calibrated nanoseconds.
        const double cost_score = count <= 1.0 ? 64.0 : 0.15 * count;
        if (benefit_score <= cost_score) { ++stats_.cost_skips; history(p, target, "cost"); return; }
        history(p, target, "rebuild");
        rebuild(p, target);
    }
    void history(const Part& p, size_t target, const char* reason) {
        stats_.decision_history.push_back(std::to_string(p.accesses) + ":" +
            std::to_string(p.fanout) + ">" + std::to_string(target) + ":" + reason);
    }
    size_t nearest_candidate(size_t desired) const {
        size_t best = candidates_[0];
        for (size_t i = 1; i < candidates_.size(); ++i) {
            const size_t a = candidates_[i] > desired ? candidates_[i] - desired : desired - candidates_[i];
            const size_t b = best > desired ? best - desired : desired - best;
            if (a < b) best = candidates_[i];
        }
        return best;
    }
    void rebuild(Part& p, size_t target) {
        typedef std::chrono::steady_clock Clock;
        Clock::time_point begin = Clock::now();
        std::vector<std::pair<Key, Value> > records = p.tree->export_sorted();
        stats_.temp_rebuild_bytes = std::max(stats_.temp_rebuild_bytes,
            records.capacity() * sizeof(std::pair<Key, Value>));
        BPlusTree<Key, Value>* replacement = new BPlusTree<Key, Value>(static_cast<int>(target));
        try {
            replacement->bulk_load_sorted(records);
            stats_.transitions.push_back(std::make_pair(p.fanout, target));
        }
        catch (...) { delete replacement; throw; }
        delete p.tree; p.tree = replacement;
        p.fanout = target; p.last_rebuild = p.accesses; p.pending = target; p.votes = 0;
        stats_.monitor_events += p.monitor.events();
        stats_.monitor_samples += p.monitor.samples();
        p.monitor.reset();
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        stats_.rebuild_ms += elapsed; stats_.max_rebuild_ms = std::max(stats_.max_rebuild_ms, elapsed);
        ++stats_.rebuilds;
    }
    std::vector<Part*> parts_;
    Key min_, max_;
    size_t interval_, min_f_, max_f_;
    std::vector<size_t> candidates_;
    bool enabled_;
    size_t global_access_;
    Stats stats_;
    SegmentedAdaptiveV2(const SegmentedAdaptiveV2&);
    SegmentedAdaptiveV2& operator=(const SegmentedAdaptiveV2&);
};

} // namespace cabtree
#endif
