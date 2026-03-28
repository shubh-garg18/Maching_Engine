/*
Invariants:
1. remaining quantity>=0
2. filled quantity+remaining quantity==order quantity
3. If order is in a Price Level, then price_level!=nullptr
4. next/prev is valid iff order is resting
5. Order is accessed via unordered_map with order_id
6. price > 0 for limit orders; market orders do not rely on price.
7. Order timestamp is immutable and defines FIFO priority within a PriceLevel.
*/

#ifndef ORDER_HPP
#define ORDER_HPP // Order.hpp

#include "utils/Types.hpp"
#include "utils/TimeUtils.hpp"
#include<string>
#include<utility>
#include<cstdint>
#include<cassert>

namespace MatchEngine{

struct PriceLevel;

struct Order{
    std::string user_id="Shubh";
    std::string order_id;
    Side side;
    OrderType type;
    double price=0.0;
    uint64_t original_quantity=0;
    uint64_t filled_quantity=0;
    Order* next=nullptr;
    Order* prev=nullptr;
    PriceLevel* price_level=nullptr;
    const TimeUtils::Timestamp timestamp_ns;
    const TimeUtils::Timestamp wall_timestamp_ns;
    OrderStatus status=OrderStatus::CREATED;

    //Stop loss
    double stop_price=0.0;
    bool is_triggered=false;

    // Core Constructor
    Order(std::string uid, std::string id, Side s, OrderType t,
          double p, uint64_t qty, double stop_p, const TimeUtils::Timestamp& tstamp)
        : user_id(std::move(uid)), order_id(std::move(id)),
          side(s), type(t), price(p),
          original_quantity(qty), timestamp_ns(tstamp),
          wall_timestamp_ns(TimeUtils::wall_time_ns()), 
          status(OrderStatus::CREATED), stop_price(stop_p) {
              assert(qty>0);
              if (type == OrderType::LIMIT) assert(price > 0.0);
              if (type == OrderType::STOP_LOSS || type == OrderType::STOP_LIMIT) assert(stop_price > 0.0);
              if (type == OrderType::STOP_LIMIT) assert(price > 0.0);                
          }

    // No user ID
    Order(std::string id, Side s, OrderType t, double p, uint64_t qty,
          const TimeUtils::Timestamp& tstamp)
        : Order("Shubh", std::move(id), s, t, p, qty, 0.0, tstamp) {}

    // Market order
    Order(std::string id, Side s, OrderType t, uint64_t qty,
          const TimeUtils::Timestamp& tstamp)
        : Order("Shubh", std::move(id), s, t, 0.0, qty, 0.0, tstamp) {}

    // Limit order with user_id (no stop)
    Order(std::string uid, std::string id, Side s, OrderType t,
        double p, uint64_t qty, const TimeUtils::Timestamp& tstamp)
        : Order(std::move(uid), std::move(id), s, t,
                p, qty, 0.0, tstamp) {}
    
    // Stop order (no user_id)
    Order(std::string id, Side s, OrderType t,
        double p, uint64_t qty, double stop_p,
        const TimeUtils::Timestamp& tstamp)
        : Order("Shubh", std::move(id), s, t, p, qty, stop_p, tstamp) {}


    uint64_t remaining_quantity() const{
        return original_quantity-filled_quantity;
    }

    //Increase filled_quantity when an order is partially filled
    void fill_quantity(uint64_t qty) {
        assert(qty<=remaining_quantity());
        filled_quantity+=qty;
        if(remaining_quantity()==0) status=OrderStatus::COMPLETED;
        else status=OrderStatus::PARTIALLY_FILLED;
    }


    bool is_filled() const{
        return remaining_quantity()==0;
    }
};

} // namespace MatchEngine

#endif // ORDER_HPP