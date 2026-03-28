/*
 Invariants:
1. Monotonic for ordering :- Within a single process, event timestamps must not go backward.
Required for:
    FIFO tie-breaking
    Trade sequencing
    Deterministic replay
2. High resolution :- Microseconds minimum. Nanoseconds preferred internally.
3. Time-zone handling
    Internally: always UTC.
    API layer: output ISO-8601 in Z format.
    If business insists on IST → convert at serialization only.
4. Format: YYYY-MM-DDTHH:MM:SS.ssssssZ
*/

#pragma once

#include<chrono>
#include<string>
#include "utils/Types.hpp"

namespace MatchEngine::TimeUtils{
    
using EngineClock=std::chrono::steady_clock; // Monotonic time (for ordering)
using WallClock=std::chrono::system_clock; // UTC time (for logging)
using Timestamp=uint64_t;

// Get current time in UTC
Timestamp now_ns();

uint64_t wall_time_ns();   // system_clock nanoseconds since epoch

std::string to_iso8601(Timestamp timestamp_ns);// expects WALL clock timestamp

class OrderIdGenerator {
private:
    uint64_t counter = 0;
public:
    std::string next() {
        return std::to_string(counter++);
    }
};


} // namespace MatchEngine
