#include "publisher/TradePublisher.hpp"

namespace MatchEngine{

struct InMemoryTradePublisher: public TradePublisher{
    std::vector<TradeEvent> events;

    void publish(const TradeEvent& trade) override{
        events.push_back(trade);
    }
};

}