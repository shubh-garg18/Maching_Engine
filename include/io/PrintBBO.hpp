#pragma once
#include <ostream>
#include "../market_data/BBO.hpp"

namespace MatchEngine {

std::ostream& operator<<(std::ostream& os, const BBO& bbo);

}
