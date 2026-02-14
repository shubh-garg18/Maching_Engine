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
#include<string>
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
    uint64_t timestamp=0;
    OrderStatus status=OrderStatus::CREATED;

    //Stop loss
    double stop_price=0.0;
    bool is_triggered=false;

    // Core Constructor
    Order(std::string uid, std::string id, Side s, OrderType t,
          double p, uint64_t qty, uint64_t ts)
        : user_id(std::move(uid)), order_id(std::move(id)),
          side(s), type(t), price(p),
          original_quantity(qty), timestamp(ts) {
              assert(qty>0);
              if(type!=OrderType::MARKET) assert(price>=0.0);
          }

    // No user ID
    Order(std::string id, Side s, OrderType t, double p, uint64_t qty, uint64_t ts)
        : Order("Shubh", std::move(id), s, t, p, qty, ts) {}

    // Market order
    Order(std::string id, Side s, OrderType t, uint64_t qty, uint64_t ts)
        : Order("Shubh", std::move(id), s, t, 0.0, qty, ts) {}

    uint64_t remaining_quantity() const{
        return original_quantity-filled_quantity;
    }

    //Increase filled_quantity when an order is partially filled
    void fill_quantity(uint64_t qty) {
        assert(qty<=remaining_quantity());
        filled_quantity+=qty;
    }


    bool is_filled() const{
        return remaining_quantity()==0;
    }
};

} // namespace MatchEngine

#endif // ORDER_HPP