#ifndef TRADE_PUBLISHER_HPP
#define TRADE_PUBLISHER_HPP

#include "../market_data/TradeEvent.hpp"
#include <vector>

namespace MatchEngine{

struct TradePublisher{
    virtual ~TradePublisher()=default;
    virtual void publish(const TradeEvent& trade)=0;
};

}


#endif