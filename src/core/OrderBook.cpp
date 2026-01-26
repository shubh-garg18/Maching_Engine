#include "core/OrderBook.hpp"
#include "core/Order.hpp"

#include <cassert>

namespace MatchEngine{

// Insert for limit order
void OrderBook::insert_limit(Order* order){
    assert(order);
    assert(order->type==OrderType::LIMIT);
    assert(order->price_level == nullptr);

    order->status=OrderStatus::OPEN;

    auto& book=(order->side == Side::BUY) ? bids : asks;
    auto it = book.find(order->price);
    if(it != book.end()) it->second->add_order(order);
    else{
        PriceLevel* level=new PriceLevel(order->price);
        level->add_order(order);
        book[order->price]=level;
    }

    //add in order map
    orders[order->order_id]=order;

    best_bid = bids.empty() ? nullptr : prev(bids.end())->second;
    best_ask = asks.empty() ? nullptr : asks.begin()->second;
}

//returns true if order was cancelled
bool OrderBook::cancel_order(std::string order_id){
    auto it=orders.find(order_id);
    if(it==orders.end()) return false;

    Order* order=it->second;

    assert(order->price_level != nullptr);
    assert(orders.find(order->order_id) != orders.end());

    assert(order->status == OrderStatus::OPEN ||
       order->status == OrderStatus::PARTIALLY_FILLED);

       
       PriceLevel* level=order->price_level;
       level->remove_order(order);
       
       level->reduce_quantity(order->remaining_quantity());

    if(level->is_empty()){ 
        remove_price_level(order->side, level);        
    }

    order->price_level=nullptr;    
    orders.erase(it);
    order->status=OrderStatus::CANCELLED;
    return true;
}

PriceLevel* OrderBook::get_best_bid() { return best_bid; }

PriceLevel* OrderBook::get_best_ask() { return best_ask; }

const PriceLevel* OrderBook::get_best_bid() const { return best_bid; }

const PriceLevel* OrderBook::get_best_ask() const { return best_ask; }

//removes the price level if empty
void OrderBook::remove_price_level(Side side, PriceLevel* level){
    auto& book=(side == Side::BUY) ? bids : asks;
    book.erase(level->price);
    delete level;

    best_bid = bids.empty() ? nullptr : prev(bids.end())->second;
    best_ask = asks.empty() ? nullptr : asks.begin()->second;
}

//returns the best price level on the opposite side
PriceLevel* OrderBook::get_best_opposite(Side side){
    if(side == Side::BUY) {
        return asks.empty() ? nullptr : asks.begin()->second;
    } 
    return bids.empty() ? nullptr : std::prev(bids.end())->second;
}

const PriceLevel* OrderBook::get_best_opposite(Side side) const{
    if(side == Side::BUY) {
        return asks.empty() ? nullptr : asks.begin()->second;
    } 
    return bids.empty() ? nullptr : std::prev(bids.end())->second;
}


}// namespace MatchEngine
