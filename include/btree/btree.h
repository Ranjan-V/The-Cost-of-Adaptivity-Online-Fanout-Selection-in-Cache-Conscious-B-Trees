#ifndef CABTREE_BTREE_H
#define CABTREE_BTREE_H

#include <vector>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sched.h>
#endif

namespace cabtree {

inline void cpu_yield() {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

template<typename Key, typename Value>
class BPlusTree {
public:
    explicit BPlusTree(int fanout = 64);
    ~BPlusTree();
    
    bool insert(const Key& key, const Value& value);
    bool search(const Key& key, Value& value) const;
    bool remove(const Key& key);
    std::vector<Value> range_query(const Key& start, const Key& end) const;
    // Caller must exclude concurrent writers while exporting or replacing a tree.
    std::vector<std::pair<Key, Value> > export_sorted() const;
    void bulk_load_sorted(const std::vector<std::pair<Key, Value> >& records);
    
    size_t size() const { return num_keys_.load(std::memory_order_acquire); }
    bool empty() const { return size() == 0; }
    int height() const;
    int fanout() const { return fanout_; }
    void clear();
    
    struct Stats {
        size_t num_keys;
        size_t num_internal_nodes;
        size_t num_leaf_nodes;
        int height;
        double avg_keys_per_leaf;
        double avg_keys_per_internal;
    };
    
    Stats get_stats() const;
    size_t approximate_owned_bytes() const;

private:
    struct Node {
        bool is_leaf;
        int num_keys;
        Node* parent;
        Key* keys;
        void** pointers;
        Node* next;
        std::atomic<uint64_t> version_lock;
        
        Node(bool leaf, int capacity);
        ~Node();
        
    private:
        Node(const Node&);
        Node& operator=(const Node&);
    };
    
    std::atomic<Node*> root_;
    int fanout_;
    std::atomic<size_t> num_keys_;
    Node* first_leaf_;
    
    static bool is_write_locked(uint64_t version) { return (version & 1ULL) != 0; }
    static uint64_t stable_version(Node* node);
    static bool validate_node(Node* node, uint64_t version);
    static void lock_node(Node* node);
    static void unlock_node(Node* node);
    static void unlock_path(std::vector<Node*>& path);

    Node* find_leaf(const Key& key) const;
    Node* find_leaf_optimistic(const Key& key) const;
    bool lock_path_to_leaf(const Key& key, std::vector<Node*>& path);
    bool update_existing_locked(const Key& key, const Value& value);
    bool insert_with_locked_path(const Key& key, const Value& value);
    void insert_in_leaf(Node* leaf, const Key& key, Value* value);
    int get_left_index(Node* parent, Node* child);
    void insert_in_parent(Node* left, const Key& key, Node* right);
    void insert_in_node(Node* node, int left_index, const Key& key, Node* right);
    void delete_tree(Node* node);
    void count_nodes(Node* node, size_t& internal, size_t& leaf) const;
};

// Implementation
template<typename Key, typename Value>
BPlusTree<Key, Value>::Node::Node(bool leaf, int capacity) 
    : is_leaf(leaf), num_keys(0), parent(NULL), next(NULL), version_lock(0) {
    std::unique_ptr<Key[]> pending_keys(new Key[capacity]);
    std::unique_ptr<void*[]> pending_pointers(new void*[capacity + 1]);
    keys = pending_keys.release();
    pointers = pending_pointers.release();
    std::memset(pointers, 0, sizeof(void*) * (capacity + 1));
}

template<typename Key, typename Value>
BPlusTree<Key, Value>::Node::~Node() {
    if (is_leaf) {
        for (int i = 0; i < num_keys; ++i) {
            delete static_cast<Value*>(pointers[i]);
        }
    }
    delete[] keys;
    delete[] pointers;
}

template<typename Key, typename Value>
BPlusTree<Key, Value>::BPlusTree(int fanout) 
    : root_(NULL), fanout_(fanout), num_keys_(0), first_leaf_(NULL) {
    if (fanout < 3) {
        throw std::invalid_argument("Fanout must be at least 3");
    }
    Node* root = new Node(true, fanout_);
    root_.store(root, std::memory_order_release);
    first_leaf_ = root;
}

template<typename Key, typename Value>
BPlusTree<Key, Value>::~BPlusTree() {
    delete_tree(root_.load(std::memory_order_acquire));
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::delete_tree(Node* node) {
    if (!node) return;
    
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; ++i) {
            delete_tree(static_cast<Node*>(node->pointers[i]));
        }
    }
    delete node;
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::clear() {
    delete_tree(root_.load(std::memory_order_acquire));
    Node* root = new Node(true, fanout_);
    root_.store(root, std::memory_order_release);
    first_leaf_ = root;
    num_keys_.store(0, std::memory_order_release);
}

template<typename Key, typename Value>
uint64_t BPlusTree<Key, Value>::stable_version(Node* node) {
    uint64_t version = node->version_lock.load(std::memory_order_acquire);
    return version;
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::validate_node(Node* node, uint64_t version) {
    uint64_t current = node->version_lock.load(std::memory_order_acquire);
    return current == version && !is_write_locked(current);
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::lock_node(Node* node) {
    uint64_t version = node->version_lock.load(std::memory_order_relaxed);
    for (;;) {
        while (is_write_locked(version)) {
            cpu_yield();
            version = node->version_lock.load(std::memory_order_relaxed);
        }

        if (node->version_lock.compare_exchange_weak(
                version,
                version + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return;
        }
    }
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::unlock_node(Node* node) {
    node->version_lock.fetch_add(1, std::memory_order_release);
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::unlock_path(std::vector<Node*>& path) {
    for (typename std::vector<Node*>::reverse_iterator it = path.rbegin();
         it != path.rend();
         ++it) {
        unlock_node(*it);
    }
    path.clear();
}

template<typename Key, typename Value>
typename BPlusTree<Key, Value>::Node* 
BPlusTree<Key, Value>::find_leaf(const Key& key) const {
    Node* node = root_.load(std::memory_order_acquire);
    
    while (!node->is_leaf) {
        int i = 0;
        while (i < node->num_keys && key >= node->keys[i]) {
            i++;
        }
        node = static_cast<Node*>(node->pointers[i]);
    }
    
    return node;
}

template<typename Key, typename Value>
typename BPlusTree<Key, Value>::Node*
BPlusTree<Key, Value>::find_leaf_optimistic(const Key& key) const {
    for (;;) {
        Node* observed_root = root_.load(std::memory_order_acquire);
        Node* node = observed_root;
        bool restart = false;

        while (node && !node->is_leaf) {
            uint64_t version = stable_version(node);
            if (is_write_locked(version)) {
                restart = true;
                break;
            }

            int i = 0;
            while (i < node->num_keys && key >= node->keys[i]) {
                i++;
            }
            Node* child = static_cast<Node*>(node->pointers[i]);

            if (!validate_node(node, version)) {
                restart = true;
                break;
            }
            node = child;
        }

        if (!restart &&
            observed_root == root_.load(std::memory_order_acquire)) {
            return node;
        }
        cpu_yield();
    }
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::search(const Key& key, Value& value) const {
    for (;;) {
        Node* leaf = find_leaf_optimistic(key);
        if (!leaf) {
            return false;
        }

        uint64_t version = stable_version(leaf);
        if (is_write_locked(version)) {
            cpu_yield();
            continue;
        }

        bool found = false;
        for (int i = 0; i < leaf->num_keys; ++i) {
            if (leaf->keys[i] == key) {
                value = *static_cast<Value*>(leaf->pointers[i]);
                found = true;
                break;
            }
        }

        if (validate_node(leaf, version)) {
            return found;
        }
    }
}

template<typename Key, typename Value>
int BPlusTree<Key, Value>::get_left_index(Node* parent, Node* child) {
    for (int i = 0; i <= parent->num_keys; ++i) {
        if (parent->pointers[i] == child) {
            return i - 1;
        }
    }
    return -1;
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::lock_path_to_leaf(const Key& key,
                                              std::vector<Node*>& path) {
    path.clear();

    for (;;) {
        Node* node = root_.load(std::memory_order_acquire);
        if (!node) {
            return false;
        }

        lock_node(node);
        if (node != root_.load(std::memory_order_acquire)) {
            unlock_node(node);
            cpu_yield();
            continue;
        }

        path.push_back(node);
        while (!node->is_leaf) {
            int i = 0;
            while (i < node->num_keys && key >= node->keys[i]) {
                i++;
            }

            Node* child = static_cast<Node*>(node->pointers[i]);
            lock_node(child);
            path.push_back(child);
            node = child;
        }

        return true;
    }
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::update_existing_locked(const Key& key,
                                                   const Value& value) {
    for (;;) {
        Node* leaf = find_leaf_optimistic(key);
        if (!leaf) {
            return false;
        }

        lock_node(leaf);
        for (int i = 0; i < leaf->num_keys; ++i) {
            if (leaf->keys[i] == key) {
                *static_cast<Value*>(leaf->pointers[i]) = value;
                unlock_node(leaf);
                return true;
            }
        }
        unlock_node(leaf);
        return false;
    }
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::insert_with_locked_path(const Key& key,
                                                    const Value& value) {
    std::vector<Node*> path;
    if (!lock_path_to_leaf(key, path)) {
        return false;
    }

    Node* leaf = path.back();
    
    // Check if key exists
    for (int i = 0; i < leaf->num_keys; ++i) {
        if (leaf->keys[i] == key) {
            *static_cast<Value*>(leaf->pointers[i]) = value;
            unlock_path(path);
            return false;
        }
    }
    
    // Key doesn't exist, insert new
    if (leaf->num_keys < fanout_ - 1) {
        insert_in_leaf(leaf, key, new Value(value));
        num_keys_.fetch_add(1, std::memory_order_release);
        unlock_path(path);
        return true;
    }
    
    // Leaf is full, need to split
    Node* new_leaf = new Node(true, fanout_);
    
    // Copy all entries to temp array
    Key* temp_keys = new Key[fanout_];
    void** temp_pointers = new void*[fanout_];
    
    int insertion_index = 0;
    while (insertion_index < leaf->num_keys && leaf->keys[insertion_index] < key) {
        insertion_index++;
    }
    
    for (int i = 0, j = 0; i < leaf->num_keys; ++i, ++j) {
        if (j == insertion_index) {
            j++;
        }
        temp_keys[j] = leaf->keys[i];
        temp_pointers[j] = leaf->pointers[i];
    }
    temp_keys[insertion_index] = key;
    temp_pointers[insertion_index] = new Value(value);
    
    // Split
    int split = fanout_ / 2;
    leaf->num_keys = 0;
    for (int i = 0; i < split; ++i) {
        leaf->keys[i] = temp_keys[i];
        leaf->pointers[i] = temp_pointers[i];
        leaf->num_keys++;
    }
    
    new_leaf->num_keys = 0;
    for (int i = split; i < fanout_; ++i) {
        new_leaf->keys[i - split] = temp_keys[i];
        new_leaf->pointers[i - split] = temp_pointers[i];
        new_leaf->num_keys++;
    }
    
    delete[] temp_keys;
    delete[] temp_pointers;
    
    new_leaf->next = leaf->next;
    leaf->next = new_leaf;
    new_leaf->parent = leaf->parent;
    
    num_keys_.fetch_add(1, std::memory_order_release);
    
    insert_in_parent(leaf, new_leaf->keys[0], new_leaf);
    
    unlock_path(path);
    return true;
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::insert(const Key& key, const Value& value) {
    if (update_existing_locked(key, value)) {
        return false;
    }

    return insert_with_locked_path(key, value);
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::insert_in_leaf(Node* leaf, const Key& key, Value* value) {
    int insertion_point = 0;
    while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key) {
        insertion_point++;
    }
    
    for (int i = leaf->num_keys; i > insertion_point; --i) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->pointers[i] = leaf->pointers[i - 1];
    }
    
    leaf->keys[insertion_point] = key;
    leaf->pointers[insertion_point] = value;
    leaf->num_keys++;
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::insert_in_parent(Node* left, const Key& key, Node* right) {
    if (left == root_.load(std::memory_order_acquire)) {
        Node* new_root = new Node(false, fanout_);
        new_root->keys[0] = key;
        new_root->pointers[0] = left;
        new_root->pointers[1] = right;
        new_root->num_keys = 1;
        left->parent = new_root;
        right->parent = new_root;
        root_.store(new_root, std::memory_order_release);
        return;
    }
    
    Node* parent = left->parent;
    int left_index = get_left_index(parent, left);
    
    if (parent->num_keys < fanout_) {
        insert_in_node(parent, left_index, key, right);
        return;
    }
    
    // Parent is full, split
    Node* new_parent = new Node(false, fanout_);
    
    Key* temp_keys = new Key[fanout_ + 1];
    void** temp_pointers = new void*[fanout_ + 2];
    
    for (int i = 0, j = 0; i < parent->num_keys + 1; ++i, ++j) {
        if (j == left_index + 2) {
            j++;
        }
        temp_pointers[j] = parent->pointers[i];
    }
    
    for (int i = 0, j = 0; i < parent->num_keys; ++i, ++j) {
        if (j == left_index + 1) {
            j++;
        }
        temp_keys[j] = parent->keys[i];
    }
    
    temp_pointers[left_index + 2] = right;
    temp_keys[left_index + 1] = key;
    
    int split = fanout_ / 2;
    
    parent->num_keys = 0;
    for (int i = 0; i < split; ++i) {
        parent->pointers[i] = temp_pointers[i];
        parent->keys[i] = temp_keys[i];
        parent->num_keys++;
    }
    parent->pointers[split] = temp_pointers[split];
    
    new_parent->num_keys = 0;
    for (int i = split + 1; i < fanout_ + 1; ++i) {
        new_parent->pointers[i - split - 1] = temp_pointers[i];
        new_parent->keys[i - split - 1] = temp_keys[i];
        new_parent->num_keys++;
    }
    new_parent->pointers[new_parent->num_keys] = temp_pointers[fanout_ + 1];
    
    Key split_key = temp_keys[split];
    
    delete[] temp_keys;
    delete[] temp_pointers;
    
    new_parent->parent = parent->parent;
    for (int i = 0; i <= new_parent->num_keys; ++i) {
        Node* child = static_cast<Node*>(new_parent->pointers[i]);
        child->parent = new_parent;
    }
    
    insert_in_parent(parent, split_key, new_parent);
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::insert_in_node(Node* node, int left_index, const Key& key, Node* right) {
    for (int i = node->num_keys; i > left_index + 1; --i) {
        node->keys[i] = node->keys[i - 1];
        node->pointers[i + 1] = node->pointers[i];
    }
    
    node->keys[left_index + 1] = key;
    node->pointers[left_index + 2] = right;
    node->num_keys++;
    right->parent = node;
}

template<typename Key, typename Value>
bool BPlusTree<Key, Value>::remove(const Key& key) {
    Node* leaf = find_leaf_optimistic(key);
    if (!leaf) {
        return false;
    }

    lock_node(leaf);
    
    int i = 0;
    while (i < leaf->num_keys && leaf->keys[i] != key) {
        i++;
    }
    
    if (i == leaf->num_keys) {
        unlock_node(leaf);
        return false;
    }
    
    delete static_cast<Value*>(leaf->pointers[i]);
    
    for (int j = i; j < leaf->num_keys - 1; ++j) {
        leaf->keys[j] = leaf->keys[j + 1];
        leaf->pointers[j] = leaf->pointers[j + 1];
    }
    
    leaf->num_keys--;
    num_keys_.fetch_sub(1, std::memory_order_release);
    unlock_node(leaf);
    
    return true;
}

template<typename Key, typename Value>
std::vector<Value> BPlusTree<Key, Value>::range_query(const Key& start, const Key& end) const {
    for (;;) {
        std::vector<Value> results;
        Node* leaf = find_leaf_optimistic(start);
        bool restart = false;

        while (leaf) {
            uint64_t version = stable_version(leaf);
            if (is_write_locked(version)) {
                restart = true;
                break;
            }

            Node* next = leaf->next;
            for (int i = 0; i < leaf->num_keys; ++i) {
                if (leaf->keys[i] >= start && leaf->keys[i] <= end) {
                    results.push_back(*static_cast<Value*>(leaf->pointers[i]));
                }
                if (leaf->keys[i] > end) {
                    if (validate_node(leaf, version)) {
                        return results;
                    }
                    restart = true;
                    break;
                }
            }

            if (restart || !validate_node(leaf, version)) {
                restart = true;
                break;
            }
            leaf = next;
        }

        if (!restart) {
            return results;
        }
        cpu_yield();
    }
}

template<typename Key, typename Value>
std::vector<std::pair<Key, Value> > BPlusTree<Key, Value>::export_sorted() const {
    std::vector<std::pair<Key, Value> > result;
    result.reserve(size());
    for (Node* leaf = first_leaf_; leaf; leaf = leaf->next) {
        for (int i = 0; i < leaf->num_keys; ++i) {
            result.push_back(std::make_pair(
                leaf->keys[i], *static_cast<Value*>(leaf->pointers[i])));
        }
    }
    return result;
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::bulk_load_sorted(
    const std::vector<std::pair<Key, Value> >& records) {
    for (size_t i = 1; i < records.size(); ++i) {
        if (!(records[i - 1].first < records[i].first)) {
            throw std::invalid_argument("bulk_load_sorted requires strictly increasing keys");
        }
    }

    // All allocated nodes are tracked until the new root is ready; this also
    // makes a failed allocation leave the original tree untouched.
    std::vector<Node*> allocated;
    std::vector<std::pair<Node*, Key> > level;
    Node* first = NULL;
    Node* replacement = NULL;
    try {
        if (records.empty()) {
            std::unique_ptr<Node> pending(new Node(true, fanout_));
            replacement = pending.get();
            allocated.push_back(replacement);
            pending.release();
            first = replacement;
        } else {
            const size_t leaf_capacity = static_cast<size_t>(fanout_ - 1);
            const size_t leaf_count = (records.size() + leaf_capacity - 1) / leaf_capacity;
            const size_t leaf_base = records.size() / leaf_count;
            const size_t leaf_extra = records.size() % leaf_count;
            size_t offset = 0;
            Node* previous = NULL;
            for (size_t group = 0; group < leaf_count; ++group) {
                std::unique_ptr<Node> pending(new Node(true, fanout_));
                Node* leaf = pending.get();
                allocated.push_back(leaf);
                pending.release();
                if (!first) first = leaf;
                if (previous) previous->next = leaf;
                previous = leaf;
                const size_t count = leaf_base + (group < leaf_extra ? 1 : 0);
                for (size_t j = 0; j < count; ++j) {
                    leaf->keys[j] = records[offset + j].first;
                    leaf->pointers[j] = new Value(records[offset + j].second);
                    ++leaf->num_keys;
                }
                level.push_back(std::make_pair(leaf, leaf->keys[0]));
                offset += count;
            }
            while (level.size() > 1) {
                std::vector<std::pair<Node*, Key> > upper;
                const size_t child_capacity = static_cast<size_t>(fanout_);
                const size_t parent_count = (level.size() + child_capacity - 1) / child_capacity;
                const size_t base = level.size() / parent_count;
                const size_t extra = level.size() % parent_count;
                size_t cursor = 0;
                for (size_t group = 0; group < parent_count; ++group) {
                    std::unique_ptr<Node> pending(new Node(false, fanout_));
                    Node* parent = pending.get();
                    allocated.push_back(parent);
                    pending.release();
                    const size_t children = base + (group < extra ? 1 : 0);
                    for (size_t j = 0; j < children; ++j) {
                        Node* child = level[cursor + j].first;
                        parent->pointers[j] = child;
                        child->parent = parent;
                        if (j > 0) {
                            parent->keys[j - 1] = level[cursor + j].second;
                            ++parent->num_keys;
                        }
                    }
                    upper.push_back(std::make_pair(parent, level[cursor].second));
                    cursor += children;
                }
                level.swap(upper);
            }
            replacement = level[0].first;
        }
    } catch (...) {
        for (size_t i = 0; i < allocated.size(); ++i) delete allocated[i];
        throw;
    }
    Node* old = root_.exchange(replacement, std::memory_order_acq_rel);
    first_leaf_ = first;
    num_keys_.store(records.size(), std::memory_order_release);
    delete_tree(old);
}

template<typename Key, typename Value>
int BPlusTree<Key, Value>::height() const {
    int h = 0;
    Node* node = root_.load(std::memory_order_acquire);
    
    while (node && !node->is_leaf) {
        h++;
        node = static_cast<Node*>(node->pointers[0]);
    }
    
    return h;
}

template<typename Key, typename Value>
typename BPlusTree<Key, Value>::Stats BPlusTree<Key, Value>::get_stats() const {
    Stats stats;
    stats.num_keys = num_keys_.load(std::memory_order_acquire);
    stats.height = height();
    
    size_t internal = 0, leaf = 0;
    count_nodes(root_.load(std::memory_order_acquire), internal, leaf);
    
    stats.num_internal_nodes = internal;
    stats.num_leaf_nodes = leaf;
    stats.avg_keys_per_leaf = leaf > 0 ? (double)stats.num_keys / leaf : 0.0;
    stats.avg_keys_per_internal = internal > 0 ? (double)stats.num_keys / internal : 0.0;
    
    return stats;
}

template<typename Key, typename Value>
void BPlusTree<Key, Value>::count_nodes(Node* node, size_t& internal, size_t& leaf) const {
    if (!node) return;
    
    if (node->is_leaf) {
        leaf++;
    } else {
        internal++;
        for (int i = 0; i <= node->num_keys; ++i) {
            count_nodes(static_cast<Node*>(node->pointers[i]), internal, leaf);
        }
    }
}

template<typename Key, typename Value>
size_t BPlusTree<Key, Value>::approximate_owned_bytes() const {
    size_t internal = 0, leaves = 0;
    count_nodes(root_.load(std::memory_order_acquire), internal, leaves);
    return (internal + leaves) *
               (sizeof(Node) + sizeof(Key) * static_cast<size_t>(fanout_) +
                sizeof(void*) * static_cast<size_t>(fanout_ + 1)) +
           size() * sizeof(Value);
}

} // namespace cabtree

#endif // CABTREE_BTREE_H
