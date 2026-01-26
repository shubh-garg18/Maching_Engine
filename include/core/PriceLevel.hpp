/*
Invariants:
1. price is positive
2. quantity is non-negative and equals the sum of remaining_quantity of all orders in this PriceLevel.
3. Orders inside a PriceLevel are strictly FIFO (head = oldest, tail = newest).
4. Each PriceLevel contains orders sorted with their arrival time in the order book.
5. Empty PriceLevel does not exist
6. prev/next pointers are updated accordingly to access O(1) insert/cancel.
7. head->prev == nullptr and tail->next == nullptr.
8. Each Order in this PriceLevel has:
   - order->price == this->price
   - order->price_level == this
9. 3 functions: add_order, remove_order, update_quantity
*/

#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP //PriceLevel.hpp

#include "Order.hpp"
#include<cstdint>
#include<cassert>

namespace MatchEngine{
    
struct PriceLevel{
    double price;
    uint64_t total_quantity;
    uint64_t order_count;
    PriceLevel* prev;
    PriceLevel* next;
    Order* head;
    Order* tail;

    // default constructor
    PriceLevel()
        :price(0.0), total_quantity(0), order_count(0), prev(nullptr), next(nullptr), head(nullptr), tail(nullptr) {}

    explicit PriceLevel(double price_):
        price(price_), total_quantity(0), order_count(0), prev(nullptr), next(nullptr), head(nullptr), tail(nullptr) {}

    // Disable copy constructor
    PriceLevel(const PriceLevel&)=delete;
    PriceLevel& operator=(const PriceLevel&)=delete;

    // Add order to the end of the queue (newest order, lowest time priority)
    void add_order(Order* order){
        assert(order);
        assert(order->price_level==nullptr);
        assert(order->remaining_quantity()>0);

        order->price_level=this;
        order->next=nullptr;
        order->prev=tail;

        if(tail) tail->next=order;
        else head=order;
        tail=order;

        total_quantity+=order->remaining_quantity();
        ++order_count;
    }
    // Remove order
    void remove_order(Order* order){
        assert(order);
        assert(order->price_level==this);

        if(order->prev) order->prev->next=order->next;
        else head=order->next;

        if(order->next) order->next->prev=order->prev;
        else tail=order->prev;

        --order_count;

        order->price_level=nullptr;
        order->prev=nullptr;
        order->next=nullptr;
    }

    // Check if Price Level is empty or not
    bool is_empty() const{
        return (order_count==0);
    }

    Order* get_head_order() const{
        return head;
    }

    //Reduce total quantity in partial fills
    void reduce_quantity(uint64_t qty){
        assert(qty<=total_quantity);
        total_quantity-=qty;        
    }
};

}// namespace MatchEngine

#endif // PRICE_LEVEL_HPP