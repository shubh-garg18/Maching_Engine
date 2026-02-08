#ifndef PRINT_L2_SNAPSHOT_HPP
#define PRINT_L2_SNAPSHOT_HPP

#include <ostream>
#include "../market_data/L2Snapshot.hpp"

namespace MatchEngine {

// Stream printer for L2Snapshot
std::ostream& operator<<(std::ostream& os, const L2Snapshot& snap);

} // namespace MatchEngine

#endif // PRINT_L2_SNAPSHOT_HPP
