#include "utils/TimeUtils.hpp"
#include <iomanip>
#include <sstream>
#include <cassert>
#include <ctime>

namespace MatchEngine::TimeUtils{

Timestamp now_ns(){
    static Timestamp last = 0;

    auto now=EngineClock::now();
    auto duration=now.time_since_epoch();

    Timestamp current = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    if(current <= last) current = last + 1; // Ensure monotonicity
    last = current;

    return current;
}

uint64_t wall_time_ns() {
    auto now = WallClock::now();
    auto duration = now.time_since_epoch();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

std::string to_iso8601(Timestamp timestamp_ns){

    using namespace std::chrono;

    nanoseconds total_ns(timestamp_ns);
    seconds sec=duration_cast<seconds>(total_ns);
    nanoseconds ns = total_ns - sec;

    std::time_t tt=sec.count();

    std::tm utc_tm{};
#if defined(_MSC_VER)
    gmtime_s(&utc_tm, &tt);
#else
    gmtime_r(&tt, &utc_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");

    // microseconds precision
    auto micros=duration_cast<microseconds>(ns).count();
    oss << "." << std::setw(6) << std::setfill('0') << micros; // microseconds

    oss << "Z"; // UTC indicator
    return oss.str();
}

}// namespace MatchEngine