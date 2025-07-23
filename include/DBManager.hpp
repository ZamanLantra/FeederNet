#include <pqxx/pqxx>
#include <thread>
#include <chrono>

#include "Queue.hpp"
#include "MemoryPool.hpp"
#include "AsyncLogger.hpp"
#include "Messages.hpp"

template <typename TradeMsg, MyQ RecvMsgQueue, MyPool Pool>
class DBManager {
public:
    using TradeMsgPtr = TradeMsg*;
    DBManager(const std::string& connStr, RecvMsgQueue& recvQueue, Pool& pool, AsyncLogger& logger) 
            : connStr_(connStr)
            , recvQueue_(recvQueue)
            , msgPool_(pool)
            , logger_(logger) {
        
    }
    void stop() {
        logger_.log("DBManager stop\n");
        runFlag_.store(false, std::memory_order_relaxed);
    }
    void connect() {
        try {
            conn_ = std::make_unique<pqxx::connection>(connStr_);
            if (!conn_->is_open()) {
                throw std::runtime_error("Failed to open DB connection");
            }
            conn_->prepare("insert_trade",
                "INSERT INTO trades "
                "(message_type, sequence_number, trade_id, timestamp, price, quantity, buyer_is_maker, best_match, db_time) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, to_timestamp($9))");
            logger_.log("DBManager: Connected to DB\n");
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager connect() error: " << e.what() << std::endl;
            throw;
        }
    }
    void run() {
        logger_.log("DBManager run\n");
        try {
            while (runFlag_.load(std::memory_order_relaxed)) {
                TradeMsgPtr msg = recvQueue_.dequeue();
                if (!msg) {
                    std::this_thread::yield();
                    continue;
                }
                commit(msg);
                msgPool_.deallocate(msg);
            }
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager run() error: " << e.what() << std::endl;
        }
    }

private:
    void commit(const TradeMsgPtr msg) {
        try {
            pqxx::work txn(*conn_);
            std::time_t db_time = std::time(nullptr);

            txn.exec(
                pqxx::prepped{"insert_trade"},
                pqxx::params{
                    std::string(1, msg->message_type),
                    msg->sequence_number,
                    msg->trade_id,
                    msg->timestamp,
                    msg->price,
                    msg->quantity,
                    msg->buyer_is_maker,
                    msg->best_match,
                    db_time
                }
            );

            txn.commit();
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager commit() error: " << e.what() << std::endl;
        }
    }
    const std::string& connStr_;
    RecvMsgQueue& recvQueue_;
    Pool& msgPool_;
    AsyncLogger& logger_;
    alignas(64) std::atomic<bool> runFlag_{true};
    std::unique_ptr<pqxx::connection> conn_;
};