#include "tests/test_orderbook.hpp"
#include<iostream>

int main() {  

    limit_order_test();
    market_order_test();
    ioc_order_test();
    fok_order_test();
    status_state_machine_test();
    cancel_partial_fill_test();
    global_invariant_test();
    fee_tier_test();
    market_data_test();
    trade_stream_test();
    stop_loss_test();
    timestamp_test();
    order_timestamp_test();

    std::cout << "All tests passed" << std::endl;

    return 0;
}

/*

g++ -std=c++20 -g -Wall -Wextra -Wshadow -Wconversion -Wpedantic \
-Iinclude \
$(find src -name '*.cpp') \
-o test_engine
*/