#ifndef TRADE_PUBLISHER_HPP
#define TRADE_PUBLISHER_HPP

#include "../market_data/TradeEvent.hpp"
#include <vector>

namespace MatchEngine{

struct TradePublisher{
    virtual ~TradePublisher()=default;
    virtual void publish(const TradeEvent& trade)=0;
};

struct InMemoryTradePublisher: public TradePublisher{
    std::vector<TradeEvent> events;

    void publish(const TradeEvent& trade) override{
        events.push_back(trade);
    }
};

}


#endif