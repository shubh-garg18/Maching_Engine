/*
Invariants:
1. OrderBook comprises of two sides (bid(desc) and ask(asc))
2. Market Order never rests on the book
3. Limit Order if qty remains, updated on the same side while matches done on opposite side.
4. Cached BBO (best bid / best ask) always reflects the top of each side.
5. Empty PriceLevel does not exist.
6. Every resting order exists in exactly one PriceLevel and in the order_id map.
*/

#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP //OrderBook.hpp

#include "Order.hpp"
#include "PriceLevel.hpp"
#include "market_data/BBO.hpp"
#include "market_data/L2Snapshot.hpp"
#include<string>
#include<map>
#include<unordered_map>

namespace MatchEngine{

struct OrderBook{
    PriceLevel* best_bid;
    PriceLevel* best_ask;

    //Price lookup
    std::map<double,PriceLevel*> bids;
    std::map<double,PriceLevel*> asks;

    //Order lookup
    std::unordered_map<std::string,Order*> orders;

    //Constructor
    OrderBook()
        : best_bid(nullptr), best_ask(nullptr) {}

    //Insert limit order
    void insert_limit(Order* order);

    //Cancel order
    bool cancel_order(std::string id);

    //Get Best bid and ask price
    PriceLevel* get_best_bid();
    PriceLevel* get_best_ask();

    //const overloading
    const PriceLevel* get_best_bid() const;
    const PriceLevel* get_best_ask() const;

    //Remove Price Level if empty
    void remove_price_level(Side side, PriceLevel* level);

    PriceLevel* get_best_opposite(Side side);

    // const overload
    const PriceLevel* get_best_opposite(Side side) const;

    // Pre Scan loop
    bool can_fully_fill(const Order* order) const;

    // Snapshots and BBO calculation
    BBO get_bbo() const;
    L2Snapshot get_l2_snapshot(size_t depth)const;

};

}// namespace MatchEngine

#endif // ORDERBOOK_HPP
