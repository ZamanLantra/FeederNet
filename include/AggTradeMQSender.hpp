#pragma once

#include <zmq.hpp>

#include "Queue.hpp"
#include "HashMap.hpp"
#include "MemoryPool.hpp"
#include "AsyncLogger.hpp"
#include "Messages.hpp"
#include "Utils.hpp"

// using MyHashMap = HashMap<FixedSizedChainingHashMap<std::string, std::pair<double, double>>>;

/**************************************************************************/
template <typename TradeMsg, MyQ RecvMsgQueue, MyPool Pool, bool DESTROY_MESSAGES = true>
class AggregatedTradeMQSender {
public:
    using TradeMsgPtr = TradeMsg*;
    AggregatedTradeMQSender(RecvMsgQueue& recvQueue, Pool& pool, AsyncLogger& logger) 
            : recvQueue_(recvQueue)
            , msgPool_(pool)
            , logger_(logger) {
        
        currentTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    void connect() {
        logger_.log("AggregatedTradeMQSender connect\n");
        context_ = std::make_unique<zmq::context_t>(1);
        publisher_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::pub);
        publisher_->bind("tcp://*:5555");
    }
    void stop() {
        logger_.log("AggregatedTradeMQSender stop\n");
        runFlag_.store(false, std::memory_order_relaxed);
        if (aggMap_.size() > 0) 
            SendMQ();
        std::cout << "Overall received " << recvedMsgs_ << " sent " << sentMsgs_ << std::endl;
    }
    void run() {
        logger_.log("AggregatedTradeMQSender run\n");
        try {
            while (runFlag_.load(std::memory_order_relaxed)) {
                TradeMsgPtr msg = recvQueue_.dequeue();
                if (!msg) {
                    std::this_thread::yield();
                    continue;
                }
                const uint64_t msgTime = msg->timestamp / 1000;
                if (msgTime != currentTime_) {
                    SendMQ();
                    currentTime_ = msgTime;
                }
                AggregateTrade(msg);
                if constexpr (DESTROY_MESSAGES) {
                    msgPool_.deallocate(msg);
                } 
                ++recvedMsgs_;
            }
        } 
        catch (const std::exception& e) {
            std::cerr << "AggregatedTradeMQSender run() error: " << e.what() << std::endl;
        }
    }
private:
    void SendMQ() {
        char buffer[128];
        for (auto& [sym, val] : aggMap_) {
            double vwap = val.first / val.second;
            int len = std::snprintf(buffer, sizeof(buffer), "%s,%llu,%.6f",
                                sym.c_str(), currentTime_, vwap);

            if (len > 0 && len < static_cast<int>(sizeof(buffer))) {
                zmq::message_t message(buffer, len);
                publisher_->send(message, zmq::send_flags::none);
                logger_.log("Sent: %s\n", buffer);
                ++sentMsgs_;
            }
        }
        aggMap_.clear();
    }
    void AggregateTrade(TradeMsgPtr msg) {
        std::string symbol(msg->symbol, strnlen(msg->symbol, 8));
        auto& val = aggMap_[symbol];
        val.first += msg->price * msg->quantity;
        val.second += msg->quantity;
    }
    RecvMsgQueue& recvQueue_;
    Pool& msgPool_;
    AsyncLogger& logger_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> publisher_;
    alignas(64) std::atomic<bool> runFlag_{true};
    uint64_t currentTime_ {};
    std::unordered_map<std::string, std::pair<double, double>> aggMap_; // symbol -> (sum(price*qty), sum(qty))
    size_t recvedMsgs_ {}, sentMsgs_ {};
};
