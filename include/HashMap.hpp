#pragma once

#include <vector>
#include <list>
#include <array>
#include <stack>
#include <string>
#include <iostream>
#include <unordered_map>

namespace Const {
#ifndef HASH_BUCKETS
    constexpr size_t initBuckets = 1 << 20; // 1M - Default bucket count
#else
    constexpr size_t initBuckets = HASH_BUCKETS; // Use user-defined bucket count
#endif
};

template <typename HM>
concept MyHM = requires(HM hm, typename HM::key_type key, typename HM::value_type val) {
    { hm.insert(key, val) } -> std::same_as<void>;
    { hm.contains(key) } -> std::same_as<bool>;
    { hm.erase(key) } -> std::same_as<bool>;
    { hm.find(key) } -> std::same_as<typename HM::value_type*>;
    { hm[key] } -> std::same_as<typename HM::value_type&>;
};

/**************************************************************************
Supported HM types include ChainingHashMap, FixedSizedChainingHashMap,  
OpenAddressingHashMap and STLHashMap. Check TestHashMap.cpp for usage examples.
**************************************************************************/
template <MyHM HM>
class HashMap {
public:
    HashMap() { }
	HashMap(HashMap const&) = delete;
	HashMap& operator=(HashMap const&) = delete;
    HashMap(HashMap&&) = default;
    HashMap& operator=(HashMap&&) = default;
    void insert(const typename HM::key_type& key, const typename HM::value_type& value) { 
        hashmap_.insert(key, value); }
    bool contains(const typename HM::key_type& key) const { return hashmap_.contains(key); }
    bool erase(const typename HM::key_type& key) { return hashmap_.erase(key); }
    HM::value_type* find(const typename HM::key_type& key) { return hashmap_.find(key); }
    HM::value_type& operator[](const typename HM::key_type& key) { return hashmap_[key]; }
private:
    HM hashmap_;
};

/**************************************************************************/
template <typename Key, typename Value>
class ChainingHashMap {
public:
    using key_type = Key;
    using value_type = Value;
    ChainingHashMap() 
            : table_(Const::initBuckets)
            , mask_(Const::initBuckets - 1) {
        std::cout << "ChainingHashMap initialized " << std::endl;
        if (Const::initBuckets == 0 || (Const::initBuckets & (Const::initBuckets - 1)) != 0) {
            throw std::runtime_error("initBuckets must be non-zero and a power of 2");
        }     
    }
    Value& operator[](const Key& key) {
        size_t index = std::hash<Key>()(key) & mask_;
        for (auto& node : table_[index]) {
            if (node.key == key) {
                return node.value;
            }
        }
        table_[index].emplace_back(key, Value{});
        return table_[index].back().value;
    }
    void insert(const Key& key, const Value& value) {
        size_t index = std::hash<Key>()(key) & mask_;
        for (auto& node : table_[index]) {
            if (node.key == key) {
                node.value = value;
                return;
            }
        }
        table_[index].emplace_back(key, value);
    }
    bool contains(const Key& key) const {
        size_t index = std::hash<Key>()(key) & mask_;
        for (const auto& node : table_[index]) {
            if (node.key == key) {
                return true;
            }
        }
        return false;
    }
    bool erase(const Key& key) {
        size_t index = std::hash<Key>()(key) & mask_;
        for (auto& node : table_[index]) {
            if (node.key == key) {
                table_[index].remove(node);
                return true;
            }
        }
        return false;
    }
    Value* find(const Key& key) {
        size_t index = std::hash<Key>()(key) & mask_;
        for (auto& node : table_[index]) {
            if (node.key == key) {
                return &node.value;
            }
        }
        return nullptr;
    }
private:
    struct Node {
        Key key;
        Value value;
        Node(const Key& k, const Value& v) : key(k), value(v) {}
        bool operator==(const Node& other) {
            return key == other.key;
        }
    };
    std::vector<std::list<Node>> table_;
    size_t mask_ = 0;
};

/**************************************************************************/
template <typename Key, typename Value>
class FixedSizedChainingHashMap {
public:
    using key_type = Key;
    using value_type = Value;
    FixedSizedChainingHashMap() 
            : buckets_(Const::initBuckets, nullptr) 
            , nodesPool_(Const::initBuckets * 16)
            , mask_(Const::initBuckets - 1) {
        std::cout << "FixedSizedChainingHashMap initialized " << std::endl;
        if (Const::initBuckets == 0 || (Const::initBuckets & (Const::initBuckets - 1)) != 0) {
            throw std::runtime_error("initBuckets must be non-zero and a power of 2");
        }
        for (size_t i = 0; i < nodesPool_.size(); ++i) {
            freeNodes_.push(&nodesPool_[i]);
        }
    }
    Value& operator[](const Key& key) {
        size_t index = std::hash<Key>()(key) & mask_;
        Node* node = buckets_[index];
        Node* previous = nullptr;
        while (node != nullptr) { // Traverse the linked list in the bucket
            if (node->key == key) {
                return node->value;
            }
            previous = node;
            node = node->next;
        }
        // If we reach here, the key is not found, so we need to insert it
        if (freeNodes_.empty()) {
            throw std::runtime_error("No free nodes available in the pool");
        }
        Node* new_node = freeNodes_.top(); freeNodes_.pop();
        new_node->key = key;
        new_node->value = Value{};
        new_node->next = nullptr;
        if (buckets_[index] == nullptr) {   // If the bucket is empty, insert as head
            buckets_[index] = new_node;
        }
        else {                              // Else insert at the end of the linked list
            previous->next = new_node; 
        }
        return new_node->value;
    }
    void insert(const Key& key, const Value& value) {
        size_t index = std::hash<Key>()(key) & mask_;
        if (freeNodes_.empty()) {
            throw std::runtime_error("No free nodes available in the pool");
        }
        Node* current = freeNodes_.top(); freeNodes_.pop();
        current->key = key;
        current->value = value;
        current->next = nullptr;
        Node* node = buckets_[index];
        if (node == nullptr) {
            buckets_[index] = current;
            return;
        }
        while (node->next != nullptr) {
            node = node->next;
        }
        node->next = current;
    }
    bool contains(const Key& key) const {
        size_t index = std::hash<Key>()(key) & mask_;
        Node* node = buckets_[index];
        while (node != nullptr) {
            if (node->key == key) {
                return true;
            }
            node = node->next;
        }
        return false;
    }
    bool erase(const Key& key) {
        size_t index = std::hash<Key>()(key) & mask_;
        Node* node = buckets_[index];
        Node* prev = nullptr;
        while (node != nullptr) {
            if (node->key == key) {
                if (prev == nullptr) {
                    buckets_[index] = node->next; // erase head
                } else {
                    prev->next = node->next; // erase from middle or end
                }
                freeNodes_.push(node); // Return node to free pool
                return true;
            }
            prev = node;
            node = node->next;
        }
        
        return false;
    }
    Value* find(const Key& key) {
        size_t index = std::hash<Key>()(key) & mask_;
        Node* node = buckets_[index];
        while (node != nullptr) {
            if (node->key == key) {
                return &node->value;
            }
            node = node->next;
        }
        return nullptr;
    }
private:
    struct Node {
        Key key;
        Value value;
        Node* next;
        Node() : key(), value(), next(nullptr) {}
        Node(const Key& k, const Value& v) : key(k), value(v), next(nullptr) {}
    };
    std::vector<Node*> buckets_;
    std::vector<Node> nodesPool_;
    std::stack<Node*> freeNodes_;
    size_t mask_ = 0;
};

/**************************************************************************/
template <typename Key, typename Value>
class OpenAddressingHashMap {
public:
    using key_type = Key;
    using value_type = Value;
    OpenAddressingHashMap() 
            : table_(Const::initBuckets)
            , mask_(Const::initBuckets - 1) {
        std::cout << "OpenAddressingHashMap initialized " << std::endl;
    }
    Value& operator[](const Key& key) {
        size_t index = getHash(key);
        const size_t originalIndex = index;
        Node* node = nullptr; // Guarateed that node is not null after the loop
        while (true) {
            node = &(table_[index]);
            if (node->status == Status::EMPTY || node->status == Status::DELETED) {
                if ((size_ + 1) > table_.size() * maxLoadFactor_) {
                    reHash();
                    return (*this)[key];  // re-call operator[] after rehash
                }
                node->key_ = key;
                node->value_ = Value{};
                node->status = Status::OCCUPIED;
                ++size_;
                return node->value_;
            } 
            else if (node->status == Status::OCCUPIED && node->key_ == key) {
                return node->value_;
            }

            index = (index + 1) & mask_;
            if (index == originalIndex) {
                throw std::runtime_error("HashMap is full");
            }
        }
        return node->value_;
    }
    void insert(const Key& key, const Value& value) {
        if ((size_ + 1) > table_.size() * maxLoadFactor_) {
            reHash();
        }
        size_t index = getHash(key);
        const size_t originalIndex = index;
        while (table_[index].status == Status::OCCUPIED) {
            if (table_[index].key_ == key) {
                table_[index].value_ = value;
                return;
            }
            index = (index + 1) & mask_;
        }

        table_[index].key_ = key;
        table_[index].value_ = value;
        table_[index].status = Status::OCCUPIED;
        ++size_;
    }
    bool contains(const Key& key) const {
        size_t index = getHash(key);
        const size_t originalIndex = index;

        while (table_[index].status != Status::EMPTY) {
            if (table_[index].status == Status::OCCUPIED && table_[index].key_ == key) {
                return true;
            }
            index = (index + 1) & mask_;
            if (index == originalIndex) {
                break;
            }
        }
        return false;
    }
    bool erase(const Key& key) {
        size_t index = getHash(key);
        const size_t originalIndex = index;

        while (table_[index].status != Status::EMPTY) {
            if (table_[index].status == Status::OCCUPIED && table_[index].key_ == key) {
                table_[index].status = Status::DELETED;
                --size_;
                return true;
            }
            index = (index + 1) & mask_;
            if (index == originalIndex) {
                break;
            }
        }

        return false;
    }
    Value* find(const Key& key) {
        size_t index = getHash(key);
        const size_t originalIndex = index;

        while (table_[index].status != Status::EMPTY) {
            if (table_[index].status == Status::OCCUPIED && table_[index].key_ == key) {
                return &table_[index].value_;
            }
            index = (index + 1) & mask_;
            if (index == originalIndex) {
                break;
            }
        }
        return nullptr;
    }
private:
    enum class Status { EMPTY, OCCUPIED, DELETED };
    struct Node {
        Key key_;
        Value value_;
        Status status = Status::EMPTY;
    };
    std::vector<Node> table_;
    size_t size_ = 0;
    size_t mask_ = 0;
    float maxLoadFactor_ = 0.7f;

    size_t getHash(const Key& key) const {
        return std::hash<Key>()(key) & mask_;
    }
    void reHash() {
        std::vector<Node> oldTable_ = std::move(table_);
        size_t newSize = oldTable_.size() * 2;
        mask_ = newSize - 1;
        table_ = std::vector<Node>(newSize);
        size_ = 0;
        for (const auto& node : oldTable_) {
            if (node.status == Status::OCCUPIED) {
                insert(node.key_, node.value_);
            }
        }
    }
};

/**************************************************************************/
template <typename Key, typename Value>
class STLHashMap {
public:
    using key_type = Key;
    using value_type = Value;
    STLHashMap() {
        std::cout << "STLHashMap initialized " << std::endl;
    }
    Value& operator[](const Key& key) {
        return map_[key];
    }
    void insert(const Key& key, const Value& value) {
        map_.insert({key, value});
    }
    bool contains(const Key& key) const {
        return map_.contains(key);
    }
    bool erase(const Key& key) {
        return map_.erase(key) > 0;
    }
    Value* find(const Key& key) {
        auto it = map_.find(key);
        return it != map_.end() ? &it->second : nullptr;
    }
private:
    std::unordered_map<Key, Value> map_;
};

#ifdef USE_ABSL_FLAT_HASH_MAP 
/*
Unable to achieve good performance with absl::flat_hash_map. 

TODO : Check Why?

cmake file is provided for testing.

$ git clone https://github.com/abseil/abseil-cpp.git
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j16

🚀 Benchmarking OrderBook with 1000000 orders
AbslFlatHashMap initialized
    🟢 Insert Time: 87 ms → 1.14943e+07 ops/sec
    🟡 Update Time: 59 ms → 1.69492e+07 ops/sec
    🔴 Cancel Time: 77 ms → 1.2987e+07 ops/sec
*/ 
#include "abseil-cpp/absl/container/flat_hash_map.h"
/**************************************************************************/
template <typename Key, typename Value>
class AbslFlatHashMap {
public:
    using key_type = Key;
    using value_type = Value;
    AbslFlatHashMap() {
        std::cout << "AbslFlatHashMap initialized" << std::endl;
        // map_.reserve(Const::initBuckets / 2);
    }
    Value& operator[](const Key& key) {
        return map_[key];
    }
    void insert(const Key& key, const Value& value) {
        map_.insert({key, value});
    }
    bool contains(const Key& key) const {
        return map_.contains(key);
    }
    bool erase(const Key& key) {
        return map_.erase(key) > 0;
    }
    Value* find(const Key& key) {
        auto it = map_.find(key);
        return it != map_.end() ? &it->second : nullptr;
    }

private:
    absl::flat_hash_map<Key, Value> map_;
};
#endif

/**************************************************************************/