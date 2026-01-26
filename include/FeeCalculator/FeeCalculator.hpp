#ifndef FEE_CALCULATOR_HPP
#define FEE_CALCULATOR_HPP

#include<vector>
#include<string>
#include<unordered_map>
#include<cstdint>

namespace MatchEngine{

struct FeeTier {
    double min_volume;
    double maker_fee_rate;
    double taker_fee_rate;
};

struct UserFeeState {
    double rolling_volume = 0.0;
    size_t tier_index = 0;
};

struct FeeCalculator{
    std::vector<FeeTier> tiers;
    std::unordered_map<std::string, UserFeeState> users;

    FeeCalculator();

    double maker_fee(const std::string& user_id,double price, uint64_t qty) const;
    double taker_fee(const std::string& user_id,double price, uint64_t qty) const;

    void update_volume(const std::string& user_id, double notional);

    const FeeTier& tier_for(const std::string& user_id) const;

    
};

}// namespace MatchEngine

#endif// FEE_CALCULATOR