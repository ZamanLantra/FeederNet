/*
    g++ -std=c++20 -O3 -g TestDBManager.cpp -o TestDBManager \
        -I../include -I/opt/homebrew/Cellar/libpqxx/7.10.1/include -I/opt/homebrew/Cellar/boost/1.88.0/include \
        -L/opt/homebrew/Cellar/boost/1.88.0/lib -L/opt/homebrew/Cellar/libpqxx/7.10.1/lib -lpqxx
*/

#include "DBManager.hpp"
#include "Utils.hpp"
#include "Messages.hpp"

const std::string connStr = "dbname=trades user=postgres password=postgres host=localhost port=5432";
// const std::string tradeFilePath = "../../ETHUSDC-trades-2025-06-20.csv";
const std::string tradeFilePath = "../ETHUSDC-trades-2025-06-20.csv";

using Pool = LockFreeThreadSafePool<ITCHTradeMsg, true>;
using MyQq = CustomSPSCLockFreeQueue<ITCHTradeMsg*>; // can use CustomMPMCLockFreeQueue as well

using DBManagerT = DBManager<ITCHTradeMsg, MyQq, Pool, false>;

void sendTradesToDBManager(MyQq& q, AsyncLogger& logger) {
    logger.log("sendTradesToDBManager start\n");
    
    TradeMsgStore tradeMsgStore(tradeFilePath);
    const size_t msgCount = tradeMsgStore.size();
    logger.log("TradeMsgStore loaded [%zu]\n", msgCount);

    for (size_t i = 0; i < msgCount; ++i) {
        if (!q.enqueue(tradeMsgStore.get(i))) {
            throw "Message Queue Exhausted";
        }
    }
    logger.log("sendTradesToDBManager end\n");
}

/**************************************************************************/
int main() {
    AsyncLogger logger(std::cout);
    logger.log("TestDBManager Start\n");
    MyQq queue;
    Pool pool;
    
    DBManagerT dbManager(connStr, queue, pool, logger);
    dbManager.connect();

    try {
        std::thread dbTread(&DBManagerT::run, &dbManager);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // using this thread as the worker for trade message sending
        sendTradesToDBManager(queue, logger);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        dbManager.stop();

        dbTread.join();
    } 
    catch (const std::exception& ex) {
        std::cerr << "Exception in Spawning Threads: " << ex.what() << "\n";
    }

    logger.log("TestDBManager End\n");
}

/*
docker rm timescaledb
docker run -d \
  --name timescaledb_v2 \
  -p 5432:5432 \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_PASSWORD=postgres \
  -e POSTGRES_DB=trades \
  timescale/timescaledb:latest-pg14

psql -h 127.0.0.1 -U postgres -d trades
OR
docker exec -it timescaledb psql -U postgres -d trades

CREATE TABLE trades (
    message_type CHAR(1),
    sequence_number BIGINT,
    trade_id BIGINT,
    timestamp BIGINT,
    price DOUBLE PRECISION,
    quantity DOUBLE PRECISION,
    buyer_is_maker BOOLEAN,
    best_match BOOLEAN,
    db_time TIMESTAMP
);

trades=# select count(*) from trades;
trades=# truncate table trades;

Within 2 seconds -> BATCH_SIZE = 1,000
    COPY - 398,870 (ALL) | within 1 sec -> 246,000
    BATCH - 21,000
    SINGLE - 6,406
*/