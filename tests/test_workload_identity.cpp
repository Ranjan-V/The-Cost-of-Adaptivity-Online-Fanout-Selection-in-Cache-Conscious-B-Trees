#include "../include/btree/btree.h"
#include "../include/adaptive/segmented_adaptive_btree.h"
#include "../include/adaptive/segmented_adaptive_v2.h"

#include <cassert>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace {
struct Operation { bool update; int key; int value; };

std::uint64_t mix(std::uint64_t hash, std::uint64_t value) {
    hash ^= value + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6) + (hash >> 2);
    return hash;
}

template <typename Index>
std::uint64_t replay(Index& index, const std::vector<Operation>& operations,
                     std::size_t& misses) {
    std::uint64_t checksum = UINT64_C(1469598103934665603);
    misses = 0;
    for (std::size_t i = 0; i < operations.size(); ++i) {
        const Operation& op = operations[i];
        if (op.update) {
            index.insert(op.key, op.value);
            checksum = mix(checksum, static_cast<std::uint64_t>(op.value));
        } else {
            int value = 0;
            if (!index.search(op.key, value)) ++misses;
            checksum = mix(checksum, static_cast<std::uint64_t>(value));
        }
    }
    return checksum;
}
}  // namespace

int main() {
    const int records = 10000;
    const int operation_count = 100000;
    std::mt19937_64 generator(UINT64_C(20260921));
    std::uniform_int_distribution<int> keys(0, records - 1);
    std::uniform_int_distribution<int> actions(0, 99);
    std::vector<Operation> operations;
    operations.reserve(operation_count);
    std::uint64_t fingerprint = UINT64_C(1469598103934665603);

    for (int i = 0; i < operation_count; ++i) {
        Operation op = {actions(generator) < 5, keys(generator), records + i};
        operations.push_back(op);
        fingerprint = mix(fingerprint, static_cast<std::uint64_t>(op.update));
        fingerprint = mix(fingerprint, static_cast<std::uint64_t>(op.key));
        fingerprint = mix(fingerprint, static_cast<std::uint64_t>(op.value));
    }

    cabtree::BPlusTree<int, int> baseline(64);
    cache_adaptive::SegmentedAdaptiveBPlusTree<int, int> v1(8, 0, records - 1, 64, 5000);
    cabtree::SegmentedAdaptiveV2<int, int> v2(8, 0, records - 1, 64, 5000);
    v1.set_adaptation_enabled(false);
    v2.set_adaptation_enabled(false);
    for (int key = 0; key < records; ++key) {
        baseline.insert(key, key); v1.insert(key, key); v2.insert(key, key);
    }

    std::size_t baseline_misses = 0, v1_misses = 0, v2_misses = 0;
    const std::uint64_t baseline_checksum = replay(baseline, operations, baseline_misses);
    const std::uint64_t v1_checksum = replay(v1, operations, v1_misses);
    const std::uint64_t v2_checksum = replay(v2, operations, v2_misses);

    assert(fingerprint != 0);
    assert(baseline_misses == 0 && v1_misses == 0 && v2_misses == 0);
    assert(baseline_checksum == v1_checksum && baseline_checksum == v2_checksum);
    assert(baseline.size() == v1.size() && baseline.size() == v2.size());
    const std::vector<int> baseline_values = baseline.range_query(0, records - 1);
    assert(baseline_values == v1.range_query(0, records - 1));
    assert(baseline_values == v2.range_query(0, records - 1));
    assert(baseline.validate_invariants());
    return 0;
}
