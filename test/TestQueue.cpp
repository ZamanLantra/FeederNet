// g++ -std=c++20 TestQueue.cpp -o TestQueue -I../include -O3 -DQUEUE_CAPACITY=2048

#include "Queue.hpp"

template <typename Q>
void testQueue(const std::string& queueType) {
    std::cout << "Testing with " << queueType << "...\n";
    
    Queue<Q> queue;
    double* msg1 = new double(5.0);
    double* msg2 = new double(6.0);
    queue.enqueue(msg1);
    queue.enqueue(msg2);
    double* received1 = queue.dequeue();
    double* received2 = queue.dequeue();
    if (received1) {
        std::cout << "Received: " << *received1 << std::endl;      
    } else {
        std::cout << "Queue received1 is empty." << std::endl;
    }
    if (received2) {
        std::cout << "Received: " << *received2 << std::endl;
    } else {
        std::cout << "Queue received2 is empty." << std::endl;
    }
    delete received1, received2;
}

int main() {
    
    testQueue<LockedQueue<double*>>("LockedQueue");
    testQueue<CustomSPSCLockFreeQueue<double*>>("CustomSPSCLockFreeQueue");
    testQueue<CustomMPMCLockFreeQueue<double*>>("CustomMPMCLockFreeQueue");
    testQueue<BoostLockFreeQueue<double*>>("BoostLockFreeQueue");
#ifdef USE_MOODYCAMEL_QUEUE
    testQueue<MoodycamelLockFreeQueue<double*>>("MoodycamelLockFreeQueue");
#endif
}
