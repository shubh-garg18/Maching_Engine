#pragma once
#include <cstdint>

namespace MatchEngine {

struct BBO {
    bool has_bid = false;
    double bid_price = 0.0;
    uint64_t bid_quantity = 0;

    bool has_ask = false;
    double ask_price = 0.0;
    uint64_t ask_quantity = 0;
};

}
