#include "core/OrderBook.hpp"
#include "core/Order.hpp"

#include <cassert>
#include <iostream>

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

// returns best bid and ask price and quantity
BBO OrderBook::get_bbo() const{
    BBO bbo{};
    if(best_bid){
        bbo.has_bid=true;
        bbo.bid_price=best_bid->price;
        bbo.bid_quantity=best_bid->total_quantity;
    }

    if(best_ask){
        bbo.has_ask=true;
        bbo.ask_price=best_ask->price;
        bbo.ask_quantity=best_ask->total_quantity;
    }

    return bbo;
}

// returns all price levels up to the specified depth on both sides
L2Snapshot OrderBook::get_l2_snapshot(size_t depth) const{
    L2Snapshot snap;

    //Bids->descending order
    size_t count=0;
    for(auto it=bids.rbegin();it!=bids.rend();it++){
        if(count==depth) break;
        snap.bids.push_back({
            it->second->price,
            it->second->total_quantity
        });
        count++;
    }

    //Asks->ascending order
    count=0;
    for(auto it=asks.begin();it!=asks.end();it++){
        if(count==depth) break;
        snap.asks.push_back({
            it->second->price,
            it->second->total_quantity
        });
        count++;
    }
    return snap;
}

}// namespace MatchEngine
