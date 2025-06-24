#pragma once

#include <mutex>
#include <iostream>
#include <vector>
#include <stack>
#include <atomic>
#include <concepts>
#include <boost/pool/object_pool.hpp>

namespace Const {
#ifndef POOL_MSG_COUNT
    constexpr size_t poolMsgCount = 1 << 18; // 256K - Default message count in the pool
#else
    constexpr size_t poolMsgCount = POOL_MSG_COUNT; // Use user-defined bucket count
#endif
};

template <typename T>
concept MyPool = requires(T pool, typename T::MsgPtr msg) {
    { pool.allocate() } -> std::convertible_to<typename T::MsgPtr>;
    { pool.deallocate(msg) } -> std::same_as<void>;
};

/**************************************************************************
Supported Q types include BoostPool, CustomLockedPool, CustomLockFreePool and
LockFreeThreadSafePool. Check TestPool.cpp for usage examples.
**************************************************************************/
template <MyPool P, bool ThreadSafe = false>
class MemoryPool {
public:
    MemoryPool() { }
    P::MsgPtr allocate() {
        return pool_.allocate();
    }
    void deallocate(P::MsgPtr msg) {
        pool_.deallocate(msg);
    }
private:
    P pool_;
};

/**************************************************************************/
template <class Msg, bool ThreadSafe = false> 
class BoostPool {
public:
    using MsgPtr = Msg*;
    BoostPool() {
        std::cout << "BoostPool initialized for type: " << typeid(Msg).name() << "\n";
    }
    MsgPtr allocate() {
        if constexpr (ThreadSafe) {
            std::lock_guard<std::mutex> lock(mtx_);
            return pool_.construct();
        }
        return pool_.construct();
    }
    void deallocate(MsgPtr obj) {
        if constexpr (ThreadSafe) {
            std::lock_guard<std::mutex> lock(mtx_);
            pool_.destroy(obj);
        } 
        else {
            pool_.destroy(obj);
        }
    }
private:
    boost::object_pool<Msg> pool_;
    std::mutex mtx_; // Only used if ThreadSafe is true
};

/**************************************************************************/
template <class Msg, bool ThreadSafe = true> 
class CustomLockedPool {
public:
    using MsgPtr = Msg*;
    CustomLockedPool()
            : pool_(Const::poolMsgCount) {
        std::cout << "CustomLockedPool initialized for type: " << typeid(Msg).name() << "\n";
        for (size_t i = 0; i < pool_.size(); ++i) {
            freeMsgs_.push(&pool_[i]);
        }
    }
    MsgPtr allocate() {
        if constexpr (ThreadSafe) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (freeMsgs_.empty()) {
                throw std::runtime_error("No free messages available in the pool");
            }
            MsgPtr msg = freeMsgs_.top(); freeMsgs_.pop();
            return msg;
        }
        if (freeMsgs_.empty()) {
            throw std::runtime_error("No free messages available in the pool");
        }
        MsgPtr msg = freeMsgs_.top(); freeMsgs_.pop();
        return msg;
    }
    void deallocate(MsgPtr msg) {
        if constexpr (ThreadSafe) {
            std::lock_guard<std::mutex> lock(mutex_);
            freeMsgs_.push(msg);
        }
        else {
            freeMsgs_.push(msg);
        }
    }
private:
    std::vector<Msg> pool_;
    std::stack<MsgPtr> freeMsgs_;
    std::mutex mutex_;  // only used when ThreadSafe = true
};

/**************************************************************************/
template <class Msg, bool ThreadSafe = true> 
class CustomLockFreePool {
public:
    using MsgPtr = Msg*;
    CustomLockFreePool()
            : pool_(Const::poolMsgCount)
            , freeMsgs_(Const::poolMsgCount)
            , head_(Const::poolMsgCount - 1) {
        std::cout << "CustomLockFreePool initialized for type: " << 
            typeid(Msg).name() << "\n";
        if constexpr (!ThreadSafe)
            std::cout << "Warning: CustomLockFreePool is thread-safe always\n";
        for (size_t i = 0; i < pool_.size(); ++i) {
            freeMsgs_[i] = &pool_[i];
        }
    }
    MsgPtr allocate() {
        int64_t currentHead = head_.load(std::memory_order_acquire);
        while (currentHead >= 0) {
            MsgPtr msg = freeMsgs_[static_cast<size_t>(currentHead)];
            // try to pop the head atomically
            if (head_.compare_exchange_weak(currentHead, currentHead - 1,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                return msg;
            }
            // currentHead updated by compare_exchange_weak, loop again
        }
        return nullptr;
    }
    void deallocate(MsgPtr msg) {
        if (msg == nullptr) {
            throw std::runtime_error("Cannot deallocate a null message");
        }
        int64_t currentHead = head_.load(std::memory_order_relaxed);
        while (true) {
            int64_t nextHead = currentHead + 1;
            if (nextHead >= static_cast<int64_t>(Const::poolMsgCount)) {
                throw std::runtime_error("Pool overflow on deallocate");
            }
            freeMsgs_[static_cast<size_t>(nextHead)] = msg;
            if (head_.compare_exchange_weak(currentHead, nextHead,
                    std::memory_order_release, std::memory_order_relaxed)) {
                return;
            }
            // currentHead updated by compare_exchange_weak, retry
        }
    }
private:
    std::vector<Msg> pool_;
    std::vector<MsgPtr> freeMsgs_;
    alignas(64) std::atomic<int64_t> head_;
};

/**************************************************************************/
template <typename Msg, bool ThreadSafe = true>
class LockFreeThreadSafePool {
public:
    using MsgPtr = Msg*;
    LockFreeThreadSafePool() 
            : pool_(Const::poolMsgCount)
            , nextFree_(Const::poolMsgCount) {
        for (size_t i = 0; i < Const::poolMsgCount; ++i) {
            nextFree_[i] = i - 1;
        }
        head_.store(pack(Const::poolMsgCount - 1, 0), std::memory_order_relaxed);
    }

    MsgPtr allocate() {
        size_t oldHead = head_.load(std::memory_order_acquire);
        while (true) {
            size_t index;
            uint32_t tag;
            unpack(oldHead, index, tag);

            if (index == -1) {
                return nullptr;
            }

            size_t nextIndex = nextFree_[index];

            size_t newHead = pack(nextIndex, tag + 1);

            if (head_.compare_exchange_weak(oldHead, newHead,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                return &pool_[index];
            }
            // else retry with updated oldHead
        }
    }

    void deallocate(MsgPtr msg) {
        if (msg == nullptr) {
            throw std::runtime_error("Cannot deallocate nullptr");
        }
        size_t index = (size_t)(msg - &pool_[0]);

        size_t oldHead = head_.load(std::memory_order_acquire);
        while (true) {
            size_t headIndex;
            uint32_t tag;
            unpack(oldHead, headIndex, tag);
            nextFree_[index] = headIndex;
            size_t newHead = pack(index, tag + 1);
            if (head_.compare_exchange_weak(oldHead, newHead,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                return;
            }
        }
    }
private:
    std::vector<Msg> pool_;
    std::vector<size_t> nextFree_;
    std::atomic<size_t> head_; // lower 32 bits: index, upper 32 bits: tag

    static constexpr size_t INDEX_MASK = 0xFFFFFFFF;
    static constexpr size_t TAG_SHIFT = 32;

    inline size_t pack(size_t index, uint32_t tag) {
        return (static_cast<size_t>(tag) << TAG_SHIFT) | (static_cast<uint32_t>(index));
    }

    inline void unpack(size_t packed, size_t &index, uint32_t &tag) {
        index = static_cast<size_t>(packed & INDEX_MASK);
        tag = static_cast<uint32_t>(packed >> TAG_SHIFT);
    }
};

#ifdef USE_FOLLY_MEM_POOL
#include <folly/IndexedMemPool.h>
/*
Need to install folly, check...
*/
/**************************************************************************/
template <typename Msg, bool ThreadSafe = true>
class FollyIndexedMemPool {
public:
    using MsgPtr = Msg*;
    FollyIndexedMemPool() : pool_(Const::poolMsgCount) {
        std::cout << "FollyIndexedMemPool initialized for type: "
                  << typeid(Msg).name() << ", capacity: " << Const::poolMsgCount << "\n";
    }
    MsgPtr allocate() {
        auto handle = pool_.lease();
        if (!handle) {
            return nullptr;
        }
        return handle.release();
    }
    void deallocate(MsgPtr msg) {
        pool_.snapshot().lease_all();
        pool_.give_back(msg);
    }
private:
    folly::IndexedMemPool<Msg> pool_;
};
#endif

/**************************************************************************/