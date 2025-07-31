#pragma once

#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include "Messages.hpp"

namespace fs = std::filesystem;

/**************************************************************************/
class TradeMsgStore {
public:
    TradeMsgStore(const std::string& fileName, const std::string& path) {
        ReadFile(fileName, path);
    }
    TradeMsgStore(const std::string& dirPath) {
        std::cout << "TradeMsgStore reading directory: " << dirPath << "\n";
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (!entry.is_regular_file())
                continue;
            std::string fileName = entry.path().filename().string();
            if (fileName.ends_with(".csv")) {
                ReadFile(fileName, fs::path(dirPath));
            }
        }
        std::sort(store_.begin(), store_.end(), 
            [](auto& m1, auto& m2) { return m1.timestamp < m2.timestamp; });    
        for (uint64_t i = 0; i < size(); ++i) {
            store_[i].sequence_number = i;
        }
        std::cout << "TradeMsgStore loaded total " << size() << " trades\n";
    }
    ITCHTradeMsgPtr get(size_t index) {
        if (index >= store_.size()) 
            return nullptr;
        return &(store_[index]);
    }
    size_t size() const { return store_.size(); }
private:
    void ReadFile(const std::string& fileName, const std::string& path = "") {
        std::ifstream file(fs::path(path) / fileName);
        if (!file)
            throw std::runtime_error("Trade file open failed");
        std::string line { "" };

        size_t pos = fileName.find('-');
        std::string symbol = (pos != std::string::npos) ? fileName.substr(0, pos) : fileName;
        size_t count {};

        while (std::getline(file, line)) {
            parseTrade(line, symbol);
            ++count;
        }
        std::cout << "TradeMsgStore loaded " << count << " trades of symbol " << symbol << 
                    " from " << fileName << " file\n"; 
    }
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
    void parseTrade(std::string& line, const std::string& symbol) {
        ITCHTradeMsg msg {};
        msg.message_type = 'P';
        msg.sequence_number = store_.size();
        
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

        std::memcpy(msg.symbol, symbol.data(), std::min(symbol.size(), sizeof(msg.symbol)));

        store_.emplace_back(msg);
    }
    std::vector<ITCHTradeMsg> store_;
};
