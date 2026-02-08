#pragma once
#include <vector>
#include <cstdint>

namespace MatchEngine {

struct L2Level {
    double price;
    uint64_t quantity;
};

struct L2Snapshot {
    std::vector<L2Level> bids;
    std::vector<L2Level> asks;
};

}
