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
    
}// namespace MatchEngine


#endif // TYPES_HPP