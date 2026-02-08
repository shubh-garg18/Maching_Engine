#include "io/PrintL2Snapshot.hpp"

namespace MatchEngine {

std::ostream& operator<<(std::ostream& os, const L2Snapshot& snap) {
    os << "L2 Snapshot\n";

    os << "Bids:\n";
    if (snap.bids.empty()) {
        os << "  none\n";
    } else {
        for (const auto& level : snap.bids) {
            os << "  " << level.price
               << " x " << level.quantity << "\n";
        }
    }

    os << "Asks:\n";
    if (snap.asks.empty()) {
        os << "  none\n";
    } else {
        for (const auto& level : snap.asks) {
            os << "  " << level.price
               << " x " << level.quantity << "\n";
        }
    }

    return os;
}

} // namespace MatchEngine
