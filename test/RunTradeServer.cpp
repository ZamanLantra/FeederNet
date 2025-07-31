// g++ -std=c++20 RunTradeServer.cpp -o RunTradeServer  -I../include

#include "TradeServer.hpp"

// const std::string path = "../../";
// const std::string tradeFile = "ETHUSDC-trades-2025-06-20.csv";

const std::string path = "../../tradefiles";

/**************************************************************************/
int main() {
    // TradeServer tradeServer(tradeFile, path, true);
    TradeServer tradeServer(path, true);
    tradeServer.run();
}