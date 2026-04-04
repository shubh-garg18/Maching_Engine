#include "core/EventQueue.hpp"

namespace MatchEngine{

void EventQueue::push(const EngineEvent& event){
    {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(event);
    }
    cv.notify_one();
}

bool EventQueue::pop(EngineEvent& event){
    std::unique_lock<std::mutex> lock(mtx);

    cv.wait(lock, [&](){return !q.empty();});

    event=q.front();
    q.pop();

    return true;
}

}// namespace MatchEngine