// g++ -std=c++20 RunTradeReceiver.cpp -o RunTradeReceiver -I../include -DPOOL_MSG_COUNT=400000

#include "TradeReceiver.hpp"
#include "DBManager.hpp"

const std::string connStr = "dbname=trades user=postgres password=postgres host=timescaledb";

using MsgPool = LockFreeThreadSafePool<ITCHTradeMsg, true>;
using TradeReceiverToSequencerQ = CustomSPSCLockFreeQueue<ITCHTradeMsg*>;
using SequencerToDownstreamQ = CustomSPSCLockFreeQueue<ITCHTradeMsg*>; // can use CustomMPMCLockFreeQueue as well

using TradeDataSequencerT = TradeDataSequencer<ITCHTradeMsg, TradeReceiverToSequencerQ, 
                                        SequencerToDownstreamQ, MsgPool>;
using MulticastTradeDataReceiverT = MulticastTradeDataReceiver<ITCHTradeMsg, 
                                        TradeReceiverToSequencerQ, MsgPool>;
using DBManagerT = DBManager<ITCHTradeMsg, SequencerToDownstreamQ, MsgPool>;

/**************************************************************************/
void runMarketDataReceiverToSequencerPipeline() {
    std::ofstream file("logs/log_TradeReceiver.txt"); 
    AsyncLogger logger(file); // Can use std::cout instead of file

    logger.log("runMarketDataReceiverToSequencerPipeline Start\n");

    TradeReceiverToSequencerQ tradeReceiverToSequencerQ;
    SequencerToDownstreamQ downstreamQ;
    MsgPool msgPool;

    MulticastTradeDataReceiverT multicastTradeReceiver(tradeReceiverToSequencerQ, msgPool, logger);
    TradeDataSequencerT tradeSequencer(tradeReceiverToSequencerQ, downstreamQ, msgPool, logger);
    DBManagerT dbManager(connStr, downstreamQ, msgPool, logger);

    dbManager.connect();
    multicastTradeReceiver.connect();
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Check for readiness using a different method, login msg?

    logger.log("Starting Threads for TradeDataSequencer and MulticastTradeDataReceiver\n");

    std::vector<std::thread> threads {};

    try {
        threads.emplace_back(&TradeDataSequencerT::run, &tradeSequencer);
        threads.emplace_back(&MulticastTradeDataReceiverT::run, &multicastTradeReceiver);
        threads.emplace_back(&DBManagerT::run, &dbManager);
    } 
    catch (const std::exception& ex) {
        std::cerr << "Exception in Spawning Threads: " << ex.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(120));
    logger.log("Stopping Threads for TradeDataSequencer and MulticastTradeDataReceiver\n");
    tradeSequencer.stop();
    multicastTradeReceiver.stop();
    dbManager.stop();
 
    for (auto& thr : threads) 
        thr.join();
    
    logger.log("runMarketDataReceiverToSequencerPipeline End\n");
}

/**************************************************************************/
int main() {
    runMarketDataReceiverToSequencerPipeline();
}