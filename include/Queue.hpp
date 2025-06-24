#pragma once

#include <vector>
#include <atomic>
#include <iostream>
#include <concepts>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <boost/lockfree/queue.hpp>
#include "moodycamel/concurrentqueue.h"

namespace Const {
#ifndef QUEUE_CAPACITY
    constexpr size_t queueCapacity = 1 << 10; // 1024 - Default queue capacity
#else
    constexpr size_t queueCapacity = QUEUE_CAPACITY; // Use user-defined capacity
#endif
};

template <typename T>
concept MsgPtr = std::is_pointer_v<T>;

template <typename Q>
concept MyQ = requires(Q q, typename Q::value_type ptr) {
    { q.enqueue(ptr) } -> std::convertible_to<bool>;
    { q.dequeue() } -> std::convertible_to<typename Q::value_type>;
};

/**************************************************************************
Supported Q types include LockedQueue, CustomSPSCLockFreeQueue, BoostLockFreeQueue,
CustomMPMCLockFreeQueue and MoodycamelLockFreeQueue. Check TestQueue.cpp for usage examples.
**************************************************************************/
template <MyQ Q>
class Queue {
public:
    Queue() { }
	Queue(Queue const&) = delete;
	Queue& operator=(Queue const&) = delete;
    Queue(Queue&&) = default;
    Queue& operator=(Queue&&) = default;
    bool enqueue(Q::value_type ptr) { return queue_.enqueue(ptr); }
    Q::value_type dequeue() { return queue_.dequeue(); }
private:
    Q queue_;
};

/**************************************************************************/
template<MsgPtr T>
class LockedQueue {
public:
    using value_type = T;
    LockedQueue() {
        std::cout << "Using LockedQueue\n";
    }
    inline bool enqueue(T ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(ptr);
        cv_.notify_one();
        return true; 
    }
    inline T dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]{ return !queue_.empty(); });
        T msg = queue_.front();
        queue_.pop();
        return msg;
    }
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

/**************************************************************************/
template <MsgPtr T>
class CustomSPSCLockFreeQueue {
public:
    using value_type = T;
    CustomSPSCLockFreeQueue() 
            : buffer_(Const::queueCapacity)
            , capacity_(Const::queueCapacity)
            , mask_(Const::queueCapacity - 1) {
        if (capacity_ == 0 || (capacity_ & mask_) != 0) {
            throw std::invalid_argument("Capacity must be a power of two and greater than zero.");
        }
        std::cout << "Using CustomSPSCLockFreeQueue " << Const::queueCapacity << " capacity...\n";
    }
    inline bool enqueue(T ptr) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1);
        if (next_tail - head_.load(std::memory_order_acquire) > capacity_) {
            return false;
        }
        buffer_[tail & mask_] = std::move(ptr);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    inline T dequeue() {
        if (head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed)) {
            return nullptr; 
        }
        const size_t head = head_.fetch_add(1, std::memory_order_acq_rel);
        return buffer_[head & mask_];
    }
private:
    std::vector<T> buffer_;
    alignas(64) std::atomic<size_t> head_{ 0 };
    alignas(64) std::atomic<size_t> tail_{ 0 };
    size_t capacity_{ 0 };
    size_t mask_{ 0 };
};

/**************************************************************************/
template <MsgPtr T>
class CustomMPMCLockFreeQueue {
public:
    using value_type = T;
    CustomMPMCLockFreeQueue() 
            : buffer_(Const::queueCapacity)
            , capacity_(Const::queueCapacity)
            , mask_(Const::queueCapacity - 1) {
        if (capacity_ == 0 || (capacity_ & mask_) != 0) {
            throw std::invalid_argument("Capacity must be a power of two and greater than zero.");
        }
        std::cout << "Using CustomMPMCLockFreeQueue " << Const::queueCapacity << " capacity...\n";
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }
    inline bool enqueue(T ptr) {
        Cell* cell;
        size_t pos = tail_.load(std::memory_order_relaxed);
        while (true) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);
            if (diff == 0) {
                if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
        cell->data = ptr;
        cell->seq.store(pos + 1, std::memory_order_release);
        return true;
    }
    inline T dequeue() {
        Cell* cell;
        size_t pos = head_.load(std::memory_order_relaxed);
        while (true) {
            cell = &buffer_[pos & mask_];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos + 1);
            if (diff == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return nullptr;
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
        T value = cell->data;
        cell->seq.store(pos + capacity_, std::memory_order_release);
        return value;
    }
private:
    struct Cell {
        std::atomic<size_t> seq;
        T data;
    };
    std::vector<Cell> buffer_;
    alignas(64) std::atomic<size_t> head_{ 0 };
    alignas(64) std::atomic<size_t> tail_{ 0 };
    size_t capacity_{ 0 };
    size_t mask_{ 0 };
};

/**************************************************************************/
template <MsgPtr T>
class BoostLockFreeQueue {
public:
    using value_type = T;
    BoostLockFreeQueue() {
        std::cout << "Using BoostLockFreeQueue " << Const::queueCapacity << " capacity...\n";
    }
    inline bool enqueue(T ptr) {
        return queue_.push(ptr);
    }
    inline T dequeue() {
        T msg = nullptr;
        queue_.pop(msg);
        return msg;
    }
private:
    boost::lockfree::queue<T, 
        boost::lockfree::capacity<Const::queueCapacity>, 
        boost::lockfree::fixed_sized<true>> queue_;
};

/**************************************************************************/
template <MsgPtr T>
class MoodycamelLockFreeQueue {
public:
    using value_type = T;
    MoodycamelLockFreeQueue() : queue_(Const::queueCapacity) {
        std::cout << "Using MoodycamelLockFreeQueue " << Const::queueCapacity << " capacity...\n";
    }
    inline bool enqueue(T ptr) {
        return queue_.enqueue(ptr);
    }
    inline T dequeue() {
        T msg = nullptr;
        queue_.try_dequeue(msg);
        return msg;
    }
private:
    moodycamel::ConcurrentQueue<T> queue_;
};

/**************************************************************************/
