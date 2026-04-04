#pragma once
#include "core/Order.hpp"
#include "utils/Types.hpp"

namespace MatchEngine{

struct EngineEvent{
    EventType type;
    Order* order=nullptr;
    std::string order_id;

    static EngineEvent New(Order* order){
        return EngineEvent{EventType::NEW_ORDER,order,""};
    }

    static EngineEvent Cancel(const std::string& id){
        return EngineEvent{EventType::CANCEL_ORDER,nullptr,id};
    }

    static EngineEvent Stop(){
        return EngineEvent{EventType::STOP,nullptr,""};
    }
};

}// namespace MatchEngine
