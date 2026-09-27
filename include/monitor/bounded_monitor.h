#ifndef CABTREE_BOUNDED_MONITOR_H
#define CABTREE_BOUNDED_MONITOR_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace cabtree {

template<typename Key>
class BoundedMonitor {
public:
    struct Entry { Key key; uint64_t count; Entry(const Key& k, uint64_t n) : key(k), count(n) {} };
    BoundedMonitor(size_t width = 256, size_t depth = 4, size_t top_k = 16,
                   size_t sample_rate = 32, uint64_t seed = 0x9e3779b97f4a7c15ULL)
        : width_(width), depth_(depth), top_k_(top_k), rate_(sample_rate), seed_(seed),
          sketch_(width * depth, 0), events_(0), samples_(0) {
        if (!width || !depth || !top_k || !rate_) throw std::invalid_argument("monitor dimensions/rate must be positive");
        heavy_.reserve(top_k_);
    }
    void record(const Key& key) {
        ++events_;
        if (events_ % rate_) return;
        ++samples_;
        uint64_t estimate = std::numeric_limits<uint64_t>::max();
        for (size_t row = 0; row < depth_; ++row) {
            uint64_t& cell = sketch_[row * width_ + bucket(key, row)];
            ++cell;
            estimate = std::min(estimate, cell);
        }
        for (size_t i = 0; i < heavy_.size(); ++i) {
            if (heavy_[i].key == key) { heavy_[i].count = estimate; return; }
        }
        if (heavy_.size() < top_k_) { heavy_.push_back(Entry(key, estimate)); return; }
        size_t smallest = 0;
        for (size_t i = 1; i < heavy_.size(); ++i)
            if (heavy_[i].count < heavy_[smallest].count) smallest = i;
        if (estimate > heavy_[smallest].count) heavy_[smallest] = Entry(key, estimate);
    }
    uint64_t estimate(const Key& key) const {
        uint64_t result = std::numeric_limits<uint64_t>::max();
        for (size_t row = 0; row < depth_; ++row)
            result = std::min(result, sketch_[row * width_ + bucket(key, row)]);
        return result;
    }
    double dominant_fraction() const {
        uint64_t largest = 0;
        for (size_t i = 0; i < heavy_.size(); ++i)
            largest = std::max(largest, heavy_[i].count);
        return samples_ ? std::min(1.0, static_cast<double>(largest) / samples_) : 0.0;
    }
    void reset() {
        std::fill(sketch_.begin(), sketch_.end(), 0);
        heavy_.clear(); events_ = 0; samples_ = 0;
    }
    size_t memory_bytes() const {
        return sizeof(*this) + sketch_.capacity() * sizeof(uint64_t) +
               heavy_.capacity() * sizeof(Entry);
    }
    uint64_t events() const { return events_; }
    uint64_t samples() const { return samples_; }
    size_t tracked() const { return heavy_.size(); }
private:
    size_t bucket(const Key& key, size_t row) const {
        uint64_t x = static_cast<uint64_t>(std::hash<Key>()(key)) ^ seed_ ^
                     (0x9e3779b97f4a7c15ULL * (row + 1));
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return static_cast<size_t>(x % width_);
    }
    size_t width_, depth_, top_k_, rate_;
    uint64_t seed_;
    std::vector<uint64_t> sketch_;
    std::vector<Entry> heavy_;
    uint64_t events_, samples_;
};

} // namespace cabtree
#endif
