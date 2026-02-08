#include "io/PrintBBO.hpp"

namespace MatchEngine {

std::ostream& operator<<(std::ostream& os, const BBO& bbo) {
    os << "BBO:\n";

    if (bbo.has_bid)
        os << "Bid: " << bbo.bid_price << " x " << bbo.bid_quantity << "\n";
    else
        os << "Bid: none\n";

    if (bbo.has_ask)
        os << "Ask: " << bbo.ask_price << " x " << bbo.ask_quantity << "\n";
    else
        os << "Ask: none\n";

    return os;
}

}
