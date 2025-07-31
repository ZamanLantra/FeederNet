/*
    % brew install zeromq
    % brew install cppzmq
        
    % g++ -std=c++20 -O3 -I../include -o TestAggTradeMQSender TestAggTradeMQSender.cpp -lzmq 
    // -L/opt/homebrew/lib -I/opt/homebrew/include
*/

#include "AggTradeMQSender.hpp"

const std::string path = "../tradefiles";
const std::string tradeFile = "ETHUSDC-trades-2025-06-20X.csv";

using Pool = LockFreeThreadSafePool<ITCHTradeMsg, true>;
using MyQq = CustomSPSCLockFreeQueue<ITCHTradeMsg*>;
using AggregatedTradeMQSenderT = AggregatedTradeMQSender<ITCHTradeMsg, MyQq, Pool, false>;

void sendTradesToAggTradeMQSender(MyQq& q, AsyncLogger& logger) {
    logger.log("sendTradesToAggTradeMQSender start\n");
    
    TradeMsgStore tradeMsgStore(path);
    const size_t msgCount = tradeMsgStore.size();
    logger.log("TradeMsgStore loaded [%zu] from path [%s]\n", msgCount, path.c_str());

    for (size_t i = 0; i < msgCount; ++i) {
        if (!q.enqueue(tradeMsgStore.get(i))) {
            throw "Message Queue Exhausted";
        }
    }
    logger.log("sendTradesToAggTradeMQSender end\n");
}

int main() {
    std::ofstream file("../logs/log_AggTradeMQSender.txt"); 
    AsyncLogger logger(file); // Can use std::cout instead of file

    logger.log("Main AggregatedTradeMQSender Start\n");

    MyQq queue;
    Pool pool;

    AggregatedTradeMQSenderT aggTradeMQSender(queue, pool, logger);
    aggTradeMQSender.connect();

    try {
        std::thread aggTradeMQSenderThread(&AggregatedTradeMQSenderT::run, &aggTradeMQSender);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // using this thread as the worker for trade message sending
        sendTradesToAggTradeMQSender(queue, logger);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        aggTradeMQSender.stop();

        aggTradeMQSenderThread.join();
    } 
    catch (const std::exception& ex) {
        std::cerr << "Exception in Spawning Threads: " << ex.what() << "\n";
    }

    logger.log("Main AggregatedTradeMQSender End\n");

    return 0;
}