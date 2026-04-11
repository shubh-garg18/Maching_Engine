#pragma once

#include "core/OrderBook.hpp"
#include "core/MatchingEngine.hpp"
#include "FeeCalculator/FeeCalculator.hpp"
#include "publisher/TradePublisher.hpp"
#include "core/EventQueue.hpp"

#include "core/Order.hpp"
#include "io/PrintBBO.hpp"
#include "io/PrintL2Snapshot.hpp"

#include <chrono>

/*
 * OrderBookTest — one fresh fixture instance per test case.
 *
 * Member declaration order matches initialization order:
 *   fee_calculator -> book -> engine  (engine depends on both).
 */
class OrderBookTest {
public:
    OrderBookTest() : engine(book, fee_calculator) {}

    void run_limit_order_test();
    void run_market_order_test();
    void run_ioc_order_test();
    void run_fok_order_test();
    void run_status_state_machine_test();
    void run_cancel_partial_fill_test();
    void run_global_invariant_test();
    void run_fee_tier_test();
    void run_market_data_test();
    void run_trade_stream_test();
    void run_stop_loss_test();
    void run_timestamp_test();
    void run_order_timestamp_test();
    void run_event_queue_engine_test();

private:
    MatchEngine::FeeCalculator          fee_calculator;  // must precede engine
    MatchEngine::OrderBook              book;
    MatchEngine::MatchingEngine         engine;
    MatchEngine::InMemoryTradePublisher publisher;
    MatchEngine::EventQueue             queue;

    // Spin until publisher holds `expected` events. Asserts on timeout.
    void wait_for_trades(size_t expected,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
};

inline void run_all_tests() {
    OrderBookTest{}.run_limit_order_test();
    OrderBookTest{}.run_market_order_test();
    OrderBookTest{}.run_ioc_order_test();
    OrderBookTest{}.run_fok_order_test();
    OrderBookTest{}.run_status_state_machine_test();
    OrderBookTest{}.run_cancel_partial_fill_test();
    OrderBookTest{}.run_global_invariant_test();
    OrderBookTest{}.run_fee_tier_test();
    OrderBookTest{}.run_market_data_test();
    OrderBookTest{}.run_trade_stream_test();
    OrderBookTest{}.run_stop_loss_test();
    OrderBookTest{}.run_timestamp_test();
    OrderBookTest{}.run_order_timestamp_test();
    OrderBookTest{}.run_event_queue_engine_test();
}
