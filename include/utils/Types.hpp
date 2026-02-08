#ifndef TYPES_HPP
#define TYPES_HPP // Types.hpp

#include <cstdint>
#include <vector>

namespace MatchEngine{
    enum class Side:uint8_t{
        BUY,
        SELL
    };
    
    enum class OrderType:uint8_t{
        LIMIT,
        MARKET,
        IOC,
        FOK
    };
    
    enum class OrderStatus:uint8_t{
        CREATED,
        OPEN,        
        PARTIALLY_FILLED,
        COMPLETED,
        CANCELLED
    };

// Struct for Market Data
struct BBO{
    bool has_bid;
    double bid_price;
    uint64_t bid_quantity;

    bool has_ask;
    double ask_price;
    uint64_t ask_quantity;
};

// Struct for Level 2 (Depth Snapshot)
struct L2Level{
    double price;
    uint64_t quantity;
};

struct L2Snapshot{
    std::vector<L2Level> bids;
    std::vector<L2Level> asks;
};
    
}// namespace MatchEngine


#endif // TYPES_HPP