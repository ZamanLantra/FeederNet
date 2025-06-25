#pragma once

#pragma pack(push,1)
struct ITCHTradeMsg {
    char message_type;          // 'P' for trade message
    uint64_t sequence_number;   // Sequence number for gap detection
    uint64_t trade_id;          // trade id
    uint64_t timestamp;         // timestamp in microseconds or nanoseconds
    double price;               // trade price
    double quantity;            // trade quantity
    bool buyer_is_maker;        // flags
    bool best_match;            // flags
};

struct GapRequestMsg {
    char type;                  // '0' for Gap Request and '1' for replay
    uint64_t start_seq;
    uint64_t end_seq;
};
#pragma pack(pop)

using ITCHTradeMsgPtr = ITCHTradeMsg*;
using GapRequestMsgPtr = GapRequestMsg*;

constexpr size_t ITCHTradeMsgSize = sizeof(ITCHTradeMsg);
constexpr size_t GapRequestMsgSize = sizeof(GapRequestMsg);