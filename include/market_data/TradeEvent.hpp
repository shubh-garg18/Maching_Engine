/*
Invariants:
1. Every emitted trade corresponds to exactly one execution
2. Order of trades == execution order
3. No mutation of engine/book state
4. Trade stream is append-only
5. Emission happens immediately after trade creation
6. Consumers cannot back-modify trades
*/

#ifndef TRADE_EVENT_HPP
#define TRADE_EVENT_HPP

#include <string>
#include <cstdint>

namespace MatchEngine {

struct TradeEvent{
    std::string user_id;
    std::string buy_order_id;
    std::string sell_order_id;
    double price;
    uint64_t quantity;
    uint64_t timestamp;

    double maker_fee;
    double taker_fee;    
};

} // namespace MatchEngine

#endif // TRADE_EVENT_HPP