// g++ -std=c++20 RunTradeReceiver.cpp -o RunTradeReceiver -I../include -DPOOL_MSG_COUNT=400000

#include "TradeReceiver.hpp"

/**************************************************************************/
void runMarketDataReceiverToSequencerPipeline() {
    std::ofstream file("log_TradeReceiver.txt"); 
    AsyncLogger logger(file); // Can use std::cout instead of file

    logger.log("runMarketDataReceiverToSequencerPipeline Start\n");

    using MsgPool = LockFreeThreadSafePool<ITCHTradeMsg, true>;
    using TradeReceiverToSequencerQ = CustomSPSCLockFreeQueue<ITCHTradeMsg*>;
    using SequencerToXQ = CustomSPSCLockFreeQueue<ITCHTradeMsg*>; // can use CustomMPMCLockFreeQueue as well

    using TradeDataSequencerT = TradeDataSequencer<ITCHTradeMsg, TradeReceiverToSequencerQ, 
                                            SequencerToXQ, MsgPool>;
    using MulticastTradeDataReceiverT = MulticastTradeDataReceiver<ITCHTradeMsg, 
                                            TradeReceiverToSequencerQ, MsgPool>;

    TradeReceiverToSequencerQ tradeReceiverToSequencerQ;
    SequencerToXQ sendQ;
    MsgPool msgPool;

    MulticastTradeDataReceiverT multicastTradeReceiver(tradeReceiverToSequencerQ, msgPool, logger);
    TradeDataSequencerT tradeSequencer(tradeReceiverToSequencerQ, sendQ, msgPool, logger);
    
    multicastTradeReceiver.connect();
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Check for readiness using a different method, login msg?

    logger.log("Starting Threads for TradeDataSequencer and MulticastTradeDataReceiver\n");

    std::vector<std::thread> threads {};

    try {
        threads.emplace_back(&TradeDataSequencerT::run, &tradeSequencer);
        threads.emplace_back(&MulticastTradeDataReceiverT::run, &multicastTradeReceiver);
    } catch (const std::exception& ex) {
        std::cerr << "Exception in Spawning Threads: " << ex.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
    logger.log("Stopping Threads for TradeDataSequencer and MulticastTradeDataReceiver\n");
    tradeSequencer.stop();
    multicastTradeReceiver.stop();
 
    for (auto& thr : threads) 
        thr.join();
    
    logger.log("runMarketDataReceiverToSequencerPipeline End\n");
}

/**************************************************************************/
int main() {
    runMarketDataReceiverToSequencerPipeline();
}