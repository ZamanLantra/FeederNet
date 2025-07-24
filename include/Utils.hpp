#pragma once

#include <unistd.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include "Messages.hpp"

/**************************************************************************/
class TradeMsgStore {
public:
    TradeMsgStore(const std::string& fileName) {
        std::cout << "TradeMsgStore start\n";
        std::ifstream file(fileName);
        if (!file)
            throw std::runtime_error("Trade file open failed");
        std::string line { "" };
        std::cout << "TradeMsgStore file openned\n";

        while (std::getline(file, line)) {
            parseTrade(line);
        }
        std::cout << "TradeMsgStore loaded " << size() << " trades from " << fileName << " file\n"; 
    }
    ITCHTradeMsgPtr get(size_t index) {
        if (index >= vec_.size()) 
            return nullptr;
        return &(vec_[index]);
    }
    size_t size() const { return vec_.size(); }
private:
    /*
    | Field Index | Possible Meaning        | Description                                 |
    | ----------- | ----------------------- | ------------------------------------------- |
    | 0           | Trade ID                | Unique identifier for the trade             |
    | 1           | Price                   | Price at which the trade executed           |
    | 2           | Quantity (base asset)   | Amount of the base asset traded             |
    | 3           | Quote quantity or value | Quote asset amount involved (price \* qty)  |
    | 4           | Timestamp               | Unix timestamp or nanosecond timestamp      |
    | 5           | Is Buyer Maker (bool)   | True if buyer is maker (passive order)      |
    | 6           | Is Best Match (bool)    | True if this trade is the best price match? |
    */
    void parseTrade(std::string& line) {
        ITCHTradeMsg msg {};
        msg.message_type = 'P';
        msg.sequence_number = vec_.size();
        
        std::istringstream ss(line);
        std::string token {};
        
        std::getline(ss, token, ',');
        msg.trade_id = std::stoull(token);

        std::getline(ss, token, ',');
        msg.price = std::stod(token);

        std::getline(ss, token, ',');
        msg.quantity = std::stod(token);

        std::getline(ss, token, ','); // Skip Quote Quantity as it is (price*quantity)
        
        std::getline(ss, token, ',');
        msg.timestamp = std::stoull(token);

        std::getline(ss, token, ',');
        msg.buyer_is_maker = (token == "True");

        std::getline(ss, token, ',');
        msg.best_match = (token == "True");

        vec_.emplace_back(msg);
    }
    std::vector<ITCHTradeMsg> vec_;
};
