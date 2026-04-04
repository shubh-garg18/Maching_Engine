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
    event_queue_engine_test();

    std::cout << "All tests passed" << std::endl;

    return 0;
}

/*
rm -rf build
mkdir build && cd build
cmake ..
make
*/