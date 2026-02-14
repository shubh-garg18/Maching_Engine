/*
Invariants:
1. Matching loop lives in this file.
2. Matching Engine is a singleton.
*/

#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP //MatchingEngine.hpp

#include "OrderBook.hpp"
#include "FeeCalculator/FeeCalculator.hpp"
#include "publisher/TradePublisher.hpp"
#include<string>
#include<vector>
#include<cstdint>

namespace MatchEngine{


/*
Invariants
1. Exactly one buy and one sell per trade.
2. quantity>0
3. price equals the resting order’s price.
4. Timestamp is monotonic per incoming order
*/
struct Trade{
    std::string user_id;
    std::string buy_order_id;
    std::string sell_order_id;
    double price;
    uint64_t quantity;
    uint64_t timestamp;

    //Fees
    double maker_fee;
    double taker_fee;
};

struct MatchingEngine{
    OrderBook& order_book;
    FeeCalculator& fees_calculator;

    // Last traded price for stop loss triggering
    double last_trade_price=0.0;

    std::vector<Trade> trades;

    //Trade Publisher
    TradePublisher* trade_publisher=nullptr;
    void set_trade_publisher(TradePublisher* p){
        trade_publisher=p;
    }

    uint64_t last_timestamp=0;
    explicit MatchingEngine(OrderBook& book, FeeCalculator& fee_calculator);

    // Matching Loop
    void matching_loop(Order* order);

    // Order type Dispatcher
    void process_order(Order* order);

    // Order types
    void process_limit_order(Order* order);
    void process_market_order(Order* order);
    void process_ioc_order(Order* order);
    void process_fok_order(Order* order);

    // Helper function 
    static bool cross(const Order* order, const PriceLevel* level);

    // Stop loss orders
    void process_stop_order(Order* order);
    void check_stop_orders();


private:
    Trade generate_trades(uint64_t trade_qty, Order* incoming, Order* resting);
};

}// namespace MatchEngine

#endif // MATCHING_ENGINE_HPP