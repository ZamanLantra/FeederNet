#include <pqxx/pqxx>
#include <thread>
#include <chrono>

#include "Queue.hpp"
#include "MemoryPool.hpp"
#include "AsyncLogger.hpp"
#include "Messages.hpp"

namespace Const {
#ifndef DB_BATCH_SIZE
    constexpr std::size_t commitBatchSize = 1000;
#else
    constexpr size_t commitBatchSize = DB_BATCH_SIZE; // Use user-defined capacity
#endif
};

/**************************************************************************/
template <typename TradeMsg, MyQ RecvMsgQueue, MyPool Pool, bool DESTROY_MESSAGES = true>
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
                "(message_type, sequence_number, trade_id, timestamp, price, quantity, buyer_is_maker, best_match, symbol) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)");
            logger_.log("DBManager: Connected to DB\n");
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager connect() error: " << e.what() << std::endl;
            throw;
        }
    }
    void run() {
        // runSingle();
        // runBatch();
        runCopy();
    }
private:
    void runSingle() {
        logger_.log("DBManager SINGLE run\n");
        try {
            while (runFlag_.load(std::memory_order_relaxed)) {
                TradeMsgPtr msg = recvQueue_.dequeue();
                if (!msg) {
                    std::this_thread::yield();
                    continue;
                }
                commitSingle(msg);
                if constexpr (DESTROY_MESSAGES) {
                    msgPool_.deallocate(msg);
                } 
            }
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager run() error: " << e.what() << std::endl;
        }
    }
    void commitSingle(const TradeMsgPtr msg) {
        try {
            pqxx::work txn(*conn_);
            // std::time_t db_time = std::time(nullptr);

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
                    std::string(msg->symbol)
                }
            );

            txn.commit();
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager commit() error: " << e.what() << std::endl;
        }
    }

    void runBatch() {
        logger_.log("DBManager run BATCH\n");
        std::vector<TradeMsgPtr> batch;
        batch.reserve(Const::commitBatchSize);

        try {
            while (runFlag_.load(std::memory_order_relaxed)) {
                TradeMsgPtr msg = recvQueue_.dequeue();
                if (!msg) {
                    if (!batch.empty()) {
                        commitBatch(batch);
                        if constexpr (DESTROY_MESSAGES) {
                            for (auto m : batch) 
                                msgPool_.deallocate(m);
                        }
                        batch.clear();
                    }
                    else {
                        std::this_thread::yield();
                    }
                    continue;
                }

                batch.emplace_back(msg);

                if (batch.size() >= Const::commitBatchSize) {
                    commitBatch(batch);
                    if constexpr (DESTROY_MESSAGES) {
                        for (auto m : batch) 
                            msgPool_.deallocate(m);
                    }
                    batch.clear();
                }
            }

            if (!batch.empty()) {
                logger_.log("DBManager flush remaining BATCH\n");
                commitBatch(batch);
                if constexpr (DESTROY_MESSAGES) {
                    for (auto m : batch) 
                        msgPool_.deallocate(m);
                }
            }
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager run() error: " << e.what() << std::endl;
        }
    }
    void commitBatch(const std::vector<TradeMsgPtr>& batch) {
        try {
            pqxx::work txn(*conn_);
            // std::time_t db_time = std::time(nullptr);

            for (const auto& msg : batch) {
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
                        std::string(msg->symbol)
                    }
                );
            }

            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "DBManager commit() error: " << e.what() << std::endl;
        }
    }

    void runCopy() {
        logger_.log("DBManager run COPY\n");
        std::vector<TradeMsgPtr> batch;
        batch.reserve(Const::commitBatchSize);

        try {
            while (runFlag_.load(std::memory_order_relaxed)) {
                TradeMsgPtr msg = recvQueue_.dequeue();
                if (!msg) {
                    if (!batch.empty()) {
                        commitCopy(batch);
                        if constexpr (DESTROY_MESSAGES) {
                            for (auto m : batch) 
                                msgPool_.deallocate(m);
                        }
                        batch.clear();
                    }
                    else {
                        std::this_thread::yield();
                    }
                    continue;
                }

                batch.emplace_back(msg);

                if (batch.size() >= Const::commitBatchSize) {
                    commitCopy(batch);
                    if constexpr (DESTROY_MESSAGES) {
                        for (auto m : batch) 
                            msgPool_.deallocate(m);
                    }
                    batch.clear();
                }
            }

            if (!batch.empty()) {
                logger_.log("DBManager flush remaining BATCH\n");
                commitCopy(batch);
                if constexpr (DESTROY_MESSAGES) {
                    for (auto m : batch) 
                        msgPool_.deallocate(m);
                }
            }
        } 
        catch (const std::exception& e) {
            std::cerr << "DBManager run() error: " << e.what() << std::endl;
        }
    }
    void commitCopy(const std::vector<TradeMsgPtr>& batch) {
        try {
            pqxx::work txn(*conn_);
            std::time_t db_time = std::time(nullptr);

            // std::time_t raw_time = std::time(nullptr);
            // std::tm* timeinfo = std::gmtime(&raw_time);
            // char buffer[32];
            // std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

            auto stream = pqxx::stream_to::table(txn, { "trades" });

            for (const auto& msg : batch) {
                stream.write_row(std::make_tuple(
                    std::string(1, msg->message_type),
                    msg->sequence_number,
                    msg->trade_id,
                    msg->timestamp,
                    msg->price,
                    msg->quantity,
                    msg->buyer_is_maker,
                    msg->best_match,
                    std::string(msg->symbol)
                ));
            }

            stream.complete();
            txn.commit();
        } catch (const std::exception& e) {
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