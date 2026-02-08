#include "FeeCalculator/FeeCalculator.hpp"

namespace MatchEngine {

FeeCalculator::FeeCalculator() {
    tiers = {
        {0.0,       0.0000, 0.0005},//T0
        {100000.0, -0.0001, 0.0004},//T1
        {1000000.0,-0.0002, 0.0003},//T2
    };
}


void FeeCalculator::update_volume(const std::string& user_id, double notional) {
    auto& state = users[user_id];
    state.rolling_volume += notional;

    // Promote tier if needed (monotonic)
    while(state.tier_index + 1 < tiers.size() &&
          state.rolling_volume >= tiers[state.tier_index + 1].min_volume) {
        ++state.tier_index;
    }
}

const FeeTier& FeeCalculator::tier_for(const std::string& user_id) const {
    static FeeTier default_tier{0.0, 0.0000, 0.0005};

    auto it = users.find(user_id);
    if(it == users.end()) return default_tier;

    return tiers[it->second.tier_index];
}

double FeeCalculator::maker_fee(const std::string& user_id,double price,uint64_t qty) const {
    const FeeTier& t = tier_for(user_id);
    return price * (double)qty * t.maker_fee_rate;
}

double FeeCalculator::taker_fee(const std::string& user_id,double price,uint64_t qty) const {
    const FeeTier& t = tier_for(user_id);
    return price * (double)qty * t.taker_fee_rate;
}


}// namespace MatchEngine