#include "core/MatchingEngine.hpp"
#include "core/OrderBook.hpp"
#include "FeeCalculator/FeeCalculator.hpp"


#include<cassert>

namespace MatchEngine{

MatchingEngine::MatchingEngine(OrderBook& book, FeeCalculator& fee_calculator)
    :order_book(book), fees_calculator(fee_calculator){}

//Generate Trade with fees
Trade MatchingEngine::generate_trades(uint64_t trade_qty, Order* incoming, Order* resting){
    Trade t;
    t.user_id=incoming->user_id;
    t.quantity=trade_qty;
    t.price=resting->price;
    t.timestamp=incoming->timestamp;
    if(incoming->side==Side::BUY){
        t.buy_order_id=incoming->order_id;
        t.sell_order_id=resting->order_id;
    }
    else{
        t.buy_order_id=resting->order_id;
        t.sell_order_id=incoming->order_id;
    }

    // Fees
    double notional=t.price*(double)trade_qty;

    //MAKER=Resting, TAKER=Incoming
    if(incoming->user_id!=resting->user_id){
        fees_calculator.update_volume(resting->order_id, notional);
        fees_calculator.update_volume(incoming->order_id, notional);        
    }

    t.maker_fee=fees_calculator.maker_fee(resting->order_id, t.price, trade_qty);
    t.taker_fee=fees_calculator.taker_fee(incoming->order_id,t.price, trade_qty);

    assert(t.taker_fee>=0 || t.maker_fee<=0);

    //Publish Trade by TradePublisher
    if(trade_publisher){
        TradeEvent ev{
            t.user_id,
            t.buy_order_id,
            t.sell_order_id,
            t.price,
            t.quantity,
            t.timestamp,
            t.maker_fee,
            t.taker_fee
        };
        trade_publisher->publish(ev);
    }

    return t;
}

// Order type dispatcher
void MatchingEngine::process_order(Order* order){
    assert(order->status==OrderStatus::CREATED);
    switch(order->type){
        case OrderType::LIMIT: 
            process_limit_order(order); 
            break;
        case OrderType::MARKET: 
            process_market_order(order); 
            break;
        case OrderType::IOC: 
            process_ioc_order(order); 
            break;
        case OrderType::FOK: 
            process_fok_order(order); 
            break;
    }
}

// Matching Loop common for any type of order
void MatchingEngine::matching_loop(Order* order){
    Side side=order->side;

    while(order->remaining_quantity()>0){
        PriceLevel* level=order_book.get_best_opposite(side);
        if(!level) break;
        assert(level->head != nullptr);

        if(order->type!=OrderType::MARKET){
            if(!cross(order, level)) break;
        }

        Order* resting=level->get_head_order();
        assert(resting);

        uint64_t trade_qty=std::min(order->remaining_quantity(), resting->remaining_quantity());
        assert(trade_qty>0);

        order->fill_quantity(trade_qty);
        resting->fill_quantity(trade_qty);
        level->reduce_quantity(trade_qty);

        Trade t=generate_trades(trade_qty, order, resting);
        trades.push_back(t);

        assert(t.quantity > 0);
        assert(t.price == resting->price);

        if(resting->is_filled()){
            resting->status=OrderStatus::COMPLETED;
            level->remove_order(resting);
            if(level->is_empty()){
                order_book.remove_price_level(resting->side, level);
                level=nullptr;
            }
        }
        else{
            resting->status=OrderStatus::PARTIALLY_FILLED;
        }
    }
}

// Insert for limit order
void MatchingEngine::process_limit_order(Order* order){
    assert(order);
    assert(order->price_level == nullptr);
    assert(order->type == OrderType::LIMIT);

    matching_loop(order);
    if(!order->is_filled()){
        order_book.insert_limit(order);
        if(order->filled_quantity){
            order->status=OrderStatus::PARTIALLY_FILLED;
        }
        else{
            order->status=OrderStatus::OPEN;
        }
    }
    else{
        order->status=OrderStatus::COMPLETED;
    }
}

// Insert for market order
void MatchingEngine::process_market_order(Order* order){
    assert(order);
    assert(order->price_level == nullptr);
    assert(order->type == OrderType::MARKET);

    matching_loop(order);
    if(!order->filled_quantity){
        order->status=OrderStatus::CANCELLED;
    }
    else if(order->remaining_quantity()){
        order->status=OrderStatus::PARTIALLY_FILLED;
    }
    else{
        order->status=OrderStatus::COMPLETED;
    }
    assert(order->status != OrderStatus::OPEN);

}

// Insert for IOC order
void MatchingEngine::process_ioc_order(Order* order){
    assert(order);
    assert(order->price_level == nullptr);
    assert(order->type == OrderType::IOC);

    matching_loop(order);
    if(!order->filled_quantity){
        order->status=OrderStatus::CANCELLED;
    }
    else if(order->remaining_quantity()){
        order->status=OrderStatus::PARTIALLY_FILLED;
    }
    else{
        order->status=OrderStatus::COMPLETED;
    }
    assert(order->status != OrderStatus::OPEN);
}

// Insert for FOK order
void MatchingEngine::process_fok_order(Order* order){
    assert(order);
    assert(order->price_level == nullptr);
    assert(order->type == OrderType::FOK);

    if(!order_book.can_fully_fill(order)){
        order->status=OrderStatus::CANCELLED;
        return;
    }
    matching_loop(order);
    assert(order->is_filled());
    order->status=OrderStatus::COMPLETED;
}

// Pre Scan loop
bool OrderBook::can_fully_fill(const Order* order) const{
    uint64_t required_qty=order->original_quantity;
    Side side=order->side;
    const PriceLevel* level=get_best_opposite(side);
    while(level and required_qty){
        if(!MatchingEngine::cross(order, level)) break;
        required_qty-=std::min<uint64_t>(required_qty, level->total_quantity);
        level=level->next;
    }
    return required_qty==0;
}

// Helper function to check if price Level crosses
bool MatchingEngine::cross(const Order* order, const PriceLevel* level){
    Side side=order->side;
    bool crosses =
            (side == Side::BUY  && order->price >= level->price) ||
            (side == Side::SELL && order->price <= level->price);
    
   return crosses;
}

}// namespace MatchEngine