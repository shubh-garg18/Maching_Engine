#pragma once

#include<queue>
#include<mutex>
#include<condition_variable>
#include "core/Event.hpp"

namespace MatchEngine{
    
class EventQueue{
public:// public API
    void push(const EngineEvent& event);

    bool pop(EngineEvent& event);

private:// private implementation
    std::queue<EngineEvent> q;
    std::mutex mtx;
    std::condition_variable cv;


};// private EventQueue

}// namespace MatchEngine