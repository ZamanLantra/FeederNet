// g++ -std=c++20 RunTradeServer.cpp -o RunTradeServer  -I../include

#include "TradeServer.hpp"

const std::string tradeFilePath = "../../ETHUSDC-trades-2025-06-20.csv";

/**************************************************************************/
int main() {
    TradeServer tradeServer(tradeFilePath, true);
    tradeServer.run();
}