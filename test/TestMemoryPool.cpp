/*
$ g++ -std=c++20 -O3 -o TestMemoryPool TestMemoryPool.cpp -I../ -DPOOL_MSG_COUNT=100000
$ numactl --physcpubind=4 ./TestMemoryPool
*/

#include "MemoryPool.hpp"
#include <thread>

struct alignas(64) Msg {
    double d[5];
    size_t i;
    char c;
};

/**************************************************************************/
template <typename Pool>
void testMemoryPool(const std::string& poolType) {
    using namespace std::chrono;

    std::cout << "Testing with " << poolType << "using count: " << Const::poolMsgCount << "...\n";
    MemoryPool<Pool> pool;

    std::vector<Msg*> allocatedMsgs(Const::poolMsgCount);

    auto start_allocate = high_resolution_clock::now();
    for (size_t i = 0; i < Const::poolMsgCount; ++i) {
        Msg* msg = pool.allocate();
        msg->i = i;
        allocatedMsgs[i] = msg;
    }
    auto end_allocate = high_resolution_clock::now();

    for (size_t i = 0; i < Const::poolMsgCount; ++i) {
        if (allocatedMsgs[i] == nullptr) {
            std::cout << "Failed to allocate msg at index: " << i << " poolType:" << poolType << std::endl;
            continue;
        }
        else if (allocatedMsgs[i]->i != i) {
            std::cout << "Mismatch at index: " << i << ", expected: " << i 
                      << ", got: " << allocatedMsgs[i]->i << " poolType:" << poolType << std::endl;
        }
    }

    auto start_deallocate = high_resolution_clock::now();
    for (auto& msg : allocatedMsgs) {
        pool.deallocate(msg);
    }
    auto end_deallocate = high_resolution_clock::now();


    auto duration_allocate = duration_cast<milliseconds>(end_allocate - start_allocate).count();
    auto duration_deallocate = duration_cast<milliseconds>(end_deallocate - start_deallocate).count();
    
    std::cout << "\tAllocate Time: " << duration_allocate << " ms " << 
        (((double)duration_allocate / Const::poolMsgCount) * 1'000'000) << " ns/op\n";
    std::cout << "\tDeallocate Time: " << duration_deallocate << "ms " << 
        (((double)duration_deallocate / Const::poolMsgCount) * 1'000'000)<< " ns/op\n";
}

/**************************************************************************/
template <typename Pool>
void concurrentPoolTest(const std::string& poolType, size_t numThreads, bool verify = false) {
    MemoryPool<Pool> pool;
    const size_t msgsPerThread = Const::poolMsgCount / numThreads;
    
    std::cout << "Running concurrent test with " << numThreads << " threads, " << msgsPerThread << 
            " messages per thread with " << poolType << "using count: " << Const::poolMsgCount << "...\n";

    std::atomic<bool> errorDetected{false};

    std::vector<std::vector<Msg*>> threadAllocations(numThreads);

    auto worker = [&](size_t threadId) {
        auto& allocated = threadAllocations[threadId];
        allocated.reserve(msgsPerThread);

        for (size_t i = 0; i < msgsPerThread; ++i) {
            Msg* msg = pool.allocate();
            if (!msg) {
                std::cout << "Msg is NULL at index: " << i << " on thread: " << numThreads << "\n";
                return;
            }
            msg->i = threadId * msgsPerThread + i;  // unique tag
            allocated.emplace_back(msg);
        }

        if (verify) {
            for (size_t i = 0; i < msgsPerThread; ++i) {
                if (allocated[i]->i != threadId * msgsPerThread + i) {
                    std::cout << "Data corruption detected in thread " << threadId 
                            << " at index " << i << "\n";
                    errorDetected = true;
                }
            }
        }

        for (auto* msg : allocated) {
            pool.deallocate(msg);
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back(worker, t);
    }

    for (auto& thr : threads) {
        thr.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "\tTest completed in " << elapsed_ms << " ms.\n";
    if (verify) {
        if (!errorDetected) {
            std::cout << "\tNo data corruption detected. Pool is thread-safe under test.\n";
        } else {
            std::cout << "\tData corruption detected! Pool is NOT thread-safe.\n";
        }
    }
}

int main() {
    
    testMemoryPool<BoostPool<Msg, false>>("BoostPool<Msg, false>");
    testMemoryPool<CustomLockedPool<Msg, false>>("CustomLockedPool<Msg, false>");
    testMemoryPool<CustomLockFreePool<Msg, false>>("CustomLockFreePool<Msg, false>");

    concurrentPoolTest<BoostPool<Msg, true>>("BoostPool<Msg, true>", 10, true);
    concurrentPoolTest<LockFreeThreadSafePool<Msg, true>>("LockFreeThreadSafePool<Msg, true>", 10, true);
    
    // concurrentPoolTest<CustomLockedPool<Msg, false>>("CustomLockedPool<Msg, true>", 10, true);
    // concurrentPoolTest<CustomLockFreePool<Msg, true>>("CustomLockFreePool<Msg, true>", 10, true);
    // concurrentPoolTest<FollyIndexedMemPool<Msg, true>>("FollyIndexedMemPool<Msg, true>", 10, true);

    return 0;
}

/*
Testing with BoostPool<Msg, false>using count: 100000...
BoostPool initialized for type: 3Msg
	Allocate Time: 4 ms 40 ns/op
	Deallocate Time: 7738ms 77380 ns/op
Testing with CustomLockedPool<Msg, false>using count: 100000...
CustomLockedPool initialized for type: 3Msg
	Allocate Time: 0 ms 0 ns/op
	Deallocate Time: 0ms 0 ns/op
Testing with CustomLockFreePool<Msg, false>using count: 100000...
CustomLockFreePool initialized for type: 3Msg
Warning: CustomLockFreePool is thread-safe always
	Allocate Time: 1 ms 10 ns/op
	Deallocate Time: 1ms 10 ns/op
BoostPool initialized for type: 3Msg

Running concurrent test with 10 threads, 10000 messages per thread with BoostPool<Msg, true>using count: 100000...
	Test completed in 15357 ms.
	No data corruption detected. Pool is thread-safe under test.
Running concurrent test with 10 threads, 10000 messages per thread with LockFreeThreadSafePool<Msg, true>using count: 100000...
	Test completed in 32 ms.
	No data corruption detected. Pool is thread-safe under test.
*/