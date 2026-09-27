#include "../include/btree/btree.h"
#include "../include/monitor/bounded_monitor.h"
#include "../include/adaptive/segmented_adaptive_v2.h"
#include "../include/adaptive/segmented_adaptive_btree.h"
#include "test_utils.h"
#include <stdexcept>
#include <utility>
#include <vector>

int main() {
    const int fanouts[] = {8, 16, 32, 64, 128, 256};
    const int sizes[] = {0, 1, 7, 8, 9, 63, 64, 65, 4096};
    for (size_t f = 0; f < sizeof(fanouts)/sizeof(fanouts[0]); ++f) {
        for (size_t z = 0; z < sizeof(sizes)/sizeof(sizes[0]); ++z) {
            std::vector<std::pair<int, int> > input;
            for (int i = 0; i < sizes[z]; ++i) input.push_back(std::make_pair(i * 2, i + 7));
            cabtree::BPlusTree<int, int> loaded(fanouts[f]), inserted(fanouts[f]);
            loaded.bulk_load_sorted(input);
            for (size_t i = 0; i < input.size(); ++i) inserted.insert(input[i].first, input[i].second);
            test_utils::require(loaded.size() == inserted.size(),
                                "bulk-loaded and inserted trees must have equal sizes");
            test_utils::require(loaded.export_sorted() == input,
                                "bulk-loaded tree must preserve sorted input");
            for (size_t i = 0; i < input.size(); ++i) {
                int a = -1, b = -1;
                test_utils::require(loaded.search(input[i].first, a),
                                    "bulk-loaded tree must find every input key");
                test_utils::require(inserted.search(input[i].first, b),
                                    "insert-built tree must find every input key");
                test_utils::require(a == b,
                                    "bulk-loaded and insert-built values must agree");
            }
            test_utils::require(
                loaded.range_query(0, sizes[z] * 2) ==
                    inserted.range_query(0, sizes[z] * 2),
                "bulk-loaded and insert-built range results must agree");
            if (!input.empty()) {
                loaded.insert(input[0].first, 999);
                int value = 0;
                test_utils::require(
                    loaded.search(input[0].first, value) && value == 999,
                    "updating an existing key must replace its value");
                test_utils::require(loaded.size() == input.size(),
                                    "updating an existing key must not change size");
                test_utils::require(loaded.remove(input[0].first),
                                    "removing an existing key must succeed");
                test_utils::require(loaded.size() + 1 == input.size(),
                                    "removing one key must reduce size by one");
            }
        }
    }
    {
        cabtree::BPlusTree<int, int> tree;
        std::vector<std::pair<int, int> > bad;
        bad.push_back(std::make_pair(2, 2)); bad.push_back(std::make_pair(1, 1));
        bool rejected = false;
        try { tree.bulk_load_sorted(bad); } catch (const std::invalid_argument&) { rejected = true; }
        test_utils::require(rejected,
                            "bulk loading unsorted input must throw invalid_argument");
    }
    {
        cabtree::BoundedMonitor<int> a(64, 4, 8, 16), b(64, 4, 8, 16);
        for (int i = 0; i < 10000; ++i) { a.record(i % 100); b.record(i % 100); }
        test_utils::require(a.samples() == b.samples(),
                            "bounded monitors must sample deterministically");
        test_utils::require(a.estimate(32) == b.estimate(32),
                            "bounded monitor estimates must be deterministic");
        const size_t bytes = a.memory_bytes();
        for (int i = 0; i < 100000; ++i) a.record(i);
        test_utils::require(a.memory_bytes() == bytes,
                            "bounded monitor memory must remain constant");
        a.reset();
        test_utils::require(a.events() == 0 && a.samples() == 0,
                            "bounded monitor reset must clear event and sample counts");
    }
    {
        cache_adaptive::SegmentedAdaptiveBPlusTree<int, int> v1(8, 0, 999, 64, 5000);
        cabtree::SegmentedAdaptiveV2<int, int> v2(8, 0, 999, 64, 5000);
        v1.set_adaptation_enabled(false); v2.set_adaptation_enabled(false);
        for (int i = 0; i < 1000; ++i) { v1.insert(i, i); v2.insert(i, i); }
        v1.force_rebuild(3, 32); v2.force_rebuild(3, 32);
        for (int i = 0; i < 1000; ++i) {
            int a = -1, b = -1;
            test_utils::require(v1.search(i, a) && v2.search(i, b) && a == b,
                                "V1 and V2 must preserve equal values after rebuild");
        }
        for (int i = 0; i < 1000; i += 3) { v1.insert(i, -i); v2.insert(i, -i); }
        test_utils::require(v1.size() == v2.size(),
                            "V1 and V2 sizes must agree after updates");
        test_utils::require(v1.range_query(100, 300) == v2.range_query(100, 300),
                            "V1 and V2 range results must agree after updates");
    }
}
