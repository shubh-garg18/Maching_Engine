#include "tests/test_orderbook.hpp"

int main() {  

    /*limit_order_test();
    market_order_test();
    ioc_order_test();
    fok_order_test();
    status_state_machine_test();
    cancel_partial_fill_test();
    global_invariant_test();*/
    fee_tier_test();

    return 0;
}

/*
Commands to run
$ g++ -std=c++17 -g -Wall -Wextra -Wshadow -Wconversion -Wpedantic \
-Iinclude \
$(find src -name '*.cpp') \
-o test_engine



*/