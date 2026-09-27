#include "../include/btree/btree.h"
#include "../include/monitor/bounded_monitor.h"
#include "../include/adaptive/segmented_adaptive_v2.h"
#include "../include/adaptive/segmented_adaptive_btree.h"
#include <cassert>
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
            assert(loaded.size() == inserted.size());
            assert(loaded.export_sorted() == input);
            for (size_t i = 0; i < input.size(); ++i) {
                int a = -1, b = -1;
                assert(loaded.search(input[i].first, a));
                assert(inserted.search(input[i].first, b));
                assert(a == b);
            }
            assert(loaded.range_query(0, sizes[z] * 2) == inserted.range_query(0, sizes[z] * 2));
            if (!input.empty()) {
                loaded.insert(input[0].first, 999);
                int value = 0; assert(loaded.search(input[0].first, value) && value == 999);
                assert(loaded.size() == input.size());
                assert(loaded.remove(input[0].first));
                assert(loaded.size() + 1 == input.size());
            }
        }
    }
    {
        cabtree::BPlusTree<int, int> tree;
        std::vector<std::pair<int, int> > bad;
        bad.push_back(std::make_pair(2, 2)); bad.push_back(std::make_pair(1, 1));
        bool rejected = false;
        try { tree.bulk_load_sorted(bad); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    {
        cabtree::BoundedMonitor<int> a(64, 4, 8, 16), b(64, 4, 8, 16);
        for (int i = 0; i < 10000; ++i) { a.record(i % 100); b.record(i % 100); }
        assert(a.samples() == b.samples());
        assert(a.estimate(32) == b.estimate(32));
        const size_t bytes = a.memory_bytes();
        for (int i = 0; i < 100000; ++i) a.record(i);
        assert(a.memory_bytes() == bytes);
        a.reset(); assert(a.events() == 0 && a.samples() == 0);
    }
    {
        cache_adaptive::SegmentedAdaptiveBPlusTree<int, int> v1(8, 0, 999, 64, 5000);
        cabtree::SegmentedAdaptiveV2<int, int> v2(8, 0, 999, 64, 5000);
        v1.set_adaptation_enabled(false); v2.set_adaptation_enabled(false);
        for (int i = 0; i < 1000; ++i) { v1.insert(i, i); v2.insert(i, i); }
        v1.force_rebuild(3, 32); v2.force_rebuild(3, 32);
        for (int i = 0; i < 1000; ++i) {
            int a = -1, b = -1;
            assert(v1.search(i, a) && v2.search(i, b) && a == b);
        }
        for (int i = 0; i < 1000; i += 3) { v1.insert(i, -i); v2.insert(i, -i); }
        assert(v1.size() == v2.size());
        assert(v1.range_query(100, 300) == v2.range_query(100, 300));
    }
}
