#include "tests/test_orderbook.hpp"

#include <cassert>
#include <iostream>
#include <thread>
#include <unordered_set>

using namespace MatchEngine;

// ─── Print helpers ────────────────────────────────────────────────────────────

static void print_trades(const std::vector<Trade>& trades) {
    std::cout << "Trades (" << trades.size() << "):\n";
    for (const auto& t : trades) {
        std::cout << "  user=" << t.user_id
                  << " buy="  << t.buy_order_id
                  << " sell=" << t.sell_order_id
                  << " price=" << t.price
                  << " qty="  << t.quantity
                  << " ts="   << t.engine_ts << '\n';
    }
}

static void print_events(const std::vector<TradeEvent>& events) {
    std::cout << "Events (" << events.size() << "):\n";
    for (const auto& e : events) {
        std::cout << "  buy="  << e.buy_order_id
                  << " sell=" << e.sell_order_id
                  << " price=" << e.price
                  << " qty="  << e.quantity
                  << " ts="   << e.engine_ts
                  << " wall=" << TimeUtils::to_iso8601(e.wall_ts) << '\n';
    }
}

void OrderBookTest::wait_for_trades(size_t expected, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (publisher.events.size() < expected) {
        assert(std::chrono::steady_clock::now() < deadline
               && "wait_for_trades: timed out");
        std::this_thread::yield();
    }
}

// ─── Limit order test ─────────────────────────────────────────────────────────

void OrderBookTest::run_limit_order_test() {
    std::cout << "=== LIMIT ORDER TEST ===\n";

    Order o1("Rohit", "O1", Side::SELL, OrderType::LIMIT, 101.0, 2, 1);
    Order o2("Rahul", "O2", Side::SELL, OrderType::LIMIT, 102.0, 3, 2);
    Order o3("Virat", "O3", Side::SELL, OrderType::LIMIT, 103.0, 5, 3);

    book.insert_limit(&o1);
    book.insert_limit(&o2);
    book.insert_limit(&o3);

    std::cout << "Initial BBO:\n" << book.get_bbo();

    Order o4("O4", Side::BUY, OrderType::LIMIT, 103.0, 8, 4);
    engine.process_order(&o4);

    assert(engine.trades.size() == 3);
    assert(engine.trades[0].price    == 101.0);
    assert(engine.trades[1].price    == 102.0);
    assert(engine.trades[2].price    == 103.0);
    assert(engine.trades[0].quantity == 2);
    assert(engine.trades[1].quantity == 3);
    assert(engine.trades[2].quantity == 3);

    std::cout << "After BUY 8 @ 103:\n";
    print_trades(engine.trades);
    std::cout << "PASS\n\n";
}

// ─── Market order test ────────────────────────────────────────────────────────

void OrderBookTest::run_market_order_test() {
    std::cout << "=== MARKET ORDER TEST ===\n";

    Order s1("Rohit", "S1", Side::SELL, OrderType::LIMIT, 101.0, 2, 1);
    Order s2("Rahul", "S2", Side::SELL, OrderType::LIMIT, 102.0, 3, 2);
    Order s3("Virat", "S3", Side::SELL, OrderType::LIMIT, 103.0, 5, 3);

    book.insert_limit(&s1);
    book.insert_limit(&s2);
    book.insert_limit(&s3);

    std::cout << "Initial BBO:\n" << book.get_bbo();

    Order m1("M1", Side::BUY, OrderType::MARKET, 12, 4);
    engine.process_market_order(&m1);

    assert(s1.is_filled());
    assert(s2.is_filled());
    assert(s3.is_filled());
    assert(m1.filled_quantity == 10);
    assert(m1.remaining_quantity() == 2);
    assert(m1.status == OrderStatus::PARTIALLY_FILLED);
    assert(m1.price_level == nullptr);          // market orders must not rest
    assert(engine.trades.size() == 3);
    assert(engine.trades[0].price == 101.0 && engine.trades[0].quantity == 2);
    assert(engine.trades[1].price == 102.0 && engine.trades[1].quantity == 3);
    assert(engine.trades[2].price == 103.0 && engine.trades[2].quantity == 5);

    std::cout << "After MARKET BUY 12:\n" << book.get_bbo();
    print_trades(engine.trades);
    std::cout << "PASS\n\n";
}

// ─── IOC order test ───────────────────────────────────────────────────────────

void OrderBookTest::run_ioc_order_test() {
    std::cout << "=== IOC ORDER TEST ===\n";

    Order s1("Rohit", "S1", Side::SELL, OrderType::LIMIT, 101.0, 3, 1);
    Order s2("Virat", "S2", Side::SELL, OrderType::LIMIT, 103.0, 5, 2);

    book.insert_limit(&s1);
    book.insert_limit(&s2);

    std::cout << "Initial BBO:\n" << book.get_bbo();

    Order ioc("IOC1", Side::BUY, OrderType::IOC, 102.0, 10, 3);
    engine.process_order(&ioc);

    assert(s1.is_filled());
    assert(s2.remaining_quantity() == 5);       // above limit price, untouched
    assert(ioc.filled_quantity == 3);
    assert(ioc.remaining_quantity() == 7);
    assert(ioc.status == OrderStatus::PARTIALLY_FILLED);
    assert(ioc.price_level == nullptr);          // IOC must not rest
    assert(engine.trades.size() == 1);
    assert(engine.trades[0].price == 101.0 && engine.trades[0].quantity == 3);

    std::cout << "After IOC BUY 10 @ 102:\n" << book.get_bbo();
    print_trades(engine.trades);
    std::cout << "PASS\n\n";
}

// ─── FOK order test ───────────────────────────────────────────────────────────

void OrderBookTest::run_fok_order_test() {
    std::cout << "=== FOK ORDER TEST ===\n";

    Order s1("Rohit", "S1", Side::SELL, OrderType::LIMIT, 101.0, 3, 1);
    Order s2("Virat", "S2", Side::SELL, OrderType::LIMIT, 102.0, 2, 2);

    book.insert_limit(&s1);
    book.insert_limit(&s2);

    std::cout << "Initial BBO:\n" << book.get_bbo();

    // Total ask liquidity = 5; order asks for 6 -> must cancel entirely
    Order fok("FOK1", Side::BUY, OrderType::FOK, 103.0, 6, 3);
    engine.process_order(&fok);

    assert(s1.remaining_quantity() == 3);   // book unchanged
    assert(s2.remaining_quantity() == 2);
    assert(engine.trades.empty());
    assert(fok.filled_quantity == 0);
    assert(fok.status == OrderStatus::CANCELLED);

    std::cout << "After FOK BUY 6 @ 103 (cancelled - insufficient liquidity):\n"
              << book.get_bbo();
    std::cout << "PASS\n\n";
}

// ─── Status state machine test ────────────────────────────────────────────────

void OrderBookTest::run_status_state_machine_test() {
    std::cout << "=== STATUS STATE MACHINE TEST ===\n";

    Order s1("Virat", "S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    book.insert_limit(&s1);

    assert(s1.status == OrderStatus::OPEN);

    Order b1("B1", Side::BUY, OrderType::LIMIT, 101.0, 3, 2);
    engine.process_limit_order(&b1);

    assert(b1.status == OrderStatus::COMPLETED);
    assert(s1.status == OrderStatus::PARTIALLY_FILLED);

    book.cancel_order("S1");
    assert(s1.status == OrderStatus::CANCELLED);
    assert(s1.remaining_quantity() == 2);   // terminal state preserved

    std::cout << "PASS\n\n";
}

// ─── Cancel partial-fill test ─────────────────────────────────────────────────

void OrderBookTest::run_cancel_partial_fill_test() {
    std::cout << "=== CANCEL PARTIAL FILL TEST ===\n";

    Order s1("Virat", "S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    book.insert_limit(&s1);

    Order b1("B1", Side::BUY, OrderType::LIMIT, 101.0, 3, 2);
    engine.process_limit_order(&b1);

    assert(s1.status == OrderStatus::PARTIALLY_FILLED);
    assert(s1.remaining_quantity() == 2);

    assert(book.cancel_order("S1"));
    assert(s1.status == OrderStatus::CANCELLED);
    assert(book.get_best_ask() == nullptr);     // ask side must be clear

    assert(!book.cancel_order("S1"));           // double-cancel must be safe

    std::cout << "PASS\n\n";
}

// ─── Global invariant test ────────────────────────────────────────────────────

void OrderBookTest::run_global_invariant_test() {
    std::cout << "=== GLOBAL INVARIANT TEST ===\n";

    Order s1("Virat", "S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    Order b1("B1",          Side::BUY,  OrderType::LIMIT, 101.0, 5, 2);

    book.insert_limit(&s1);
    engine.process_limit_order(&b1);

    assert(book.get_best_bid() == nullptr);
    assert(book.get_best_ask() == nullptr);
    assert(engine.trades.size() == 1);
    assert(engine.trades[0].price == 101.0 && engine.trades[0].quantity == 5);

    print_trades(engine.trades);
    std::cout << "PASS\n\n";
}

// ─── Fee tier test ────────────────────────────────────────────────────────────

void OrderBookTest::run_fee_tier_test() {
    std::cout << "=== FEE TIER TEST ===\n";

    Order s1("Virat", "S1", Side::SELL, OrderType::LIMIT, 100.0, 2000, 1);
    book.insert_limit(&s1);

    Order b1("B1", Side::BUY, OrderType::MARKET, 2000, 2);
    engine.process_market_order(&b1);

    const Trade& t = engine.trades[0];

    // notional = 100.0 * 2000 = 200,000 -> Tier 1
    assert(t.maker_fee == 200000.0 * -0.0001);
    assert(t.taker_fee == 200000.0 *  0.0004);

    print_trades(engine.trades);
    std::cout << "Maker fee: " << t.maker_fee << '\n';
    std::cout << "Taker fee: " << t.taker_fee << '\n';
    std::cout << "PASS\n\n";
}

// ─── Market data test ─────────────────────────────────────────────────────────

void OrderBookTest::run_market_data_test() {
    std::cout << "=== MARKET DATA TEST ===\n";

    Order b1("B1", Side::BUY,  OrderType::LIMIT,  99.0, 5, 1);
    Order b2("B2", Side::BUY,  OrderType::LIMIT,  98.0, 3, 2);
    Order s1("S1", Side::SELL, OrderType::LIMIT, 101.0, 4, 3);
    Order s2("S2", Side::SELL, OrderType::LIMIT, 102.0, 6, 4);

    book.insert_limit(&b1);
    book.insert_limit(&b2);
    book.insert_limit(&s1);
    book.insert_limit(&s2);

    BBO bbo = book.get_bbo();
    assert(bbo.has_bid && bbo.bid_price == 99.0  && bbo.bid_quantity == 5);
    assert(bbo.has_ask && bbo.ask_price == 101.0 && bbo.ask_quantity == 4);

    L2Snapshot snap = book.get_l2_snapshot(2);
    assert(snap.bids.size() == 2);
    assert(snap.asks.size() == 2);
    assert(snap.bids[0].price == 99.0);
    assert(snap.bids[1].price == 98.0);
    assert(snap.asks[0].price == 101.0);
    assert(snap.asks[1].price == 102.0);

    std::cout << bbo << snap;
    std::cout << "PASS\n\n";
}

// ─── Trade stream test ────────────────────────────────────────────────────────

void OrderBookTest::run_trade_stream_test() {
    std::cout << "=== TRADE STREAM TEST ===\n";

    engine.set_trade_publisher(&publisher);

    Order s1("S1", Side::SELL, OrderType::LIMIT, 100.0, 5, 1);
    book.insert_limit(&s1);

    Order b1("B1", Side::BUY, OrderType::MARKET, 5, 2);
    engine.process_market_order(&b1);

    assert(publisher.events.size() == 1);
    const TradeEvent& ev = publisher.events[0];
    assert(ev.price         == 100.0);
    assert(ev.quantity      == 5);
    assert(ev.buy_order_id  == "B1");
    assert(ev.sell_order_id == "S1");

    std::cout << "Trade published: price=" << ev.price
              << " qty=" << ev.quantity << '\n';
    std::cout << "PASS\n\n";
}

// ─── Stop orders test ─────────────────────────────────────────────────────────

void OrderBookTest::run_stop_loss_test() {
    std::cout << "=== STOP ORDERS EDGE CASE TESTS ===\n";

    // Resting liquidity used across sub-tests
    Order s1("S1", Side::SELL, OrderType::LIMIT, 100.0, 5, 1);
    Order s2("S2", Side::SELL, OrderType::LIMIT, 101.0, 5, 2);
    Order s3("S3", Side::SELL, OrderType::LIMIT, 102.0, 5, 3);
    book.insert_limit(&s1);
    book.insert_limit(&s2);
    book.insert_limit(&s3);

    // 1. Multiple stops at same price trigger in FIFO order
    Order stop1("STOP1", Side::BUY, OrderType::STOP_LOSS, 0.0, 2, 100.0, 4);
    Order stop2("STOP2", Side::BUY, OrderType::STOP_LOSS, 0.0, 2, 100.0, 5);
    engine.process_stop_order(&stop1);
    engine.process_stop_order(&stop2);

    Order b1("B1", Side::BUY, OrderType::MARKET, 5, 6);
    engine.process_market_order(&b1);

    assert(stop1.is_triggered);
    assert(stop2.is_triggered);
    assert(engine.trades[1].buy_order_id == "STOP1");
    assert(engine.trades[2].buy_order_id == "STOP2");
    std::cout << "PASS  FIFO triggering\n";

    // 2. Stop-limit may not fully fill
    Order stop_limit("STOP_LIMIT1", Side::BUY, OrderType::STOP_LIMIT, 101.0, 10, 101.0, 7);
    engine.process_stop_order(&stop_limit);

    Order b2("B2", Side::BUY, OrderType::MARKET, 5, 8);
    engine.process_market_order(&b2);

    assert(stop_limit.is_triggered);
    assert(stop_limit.status == OrderStatus::PARTIALLY_FILLED ||
           stop_limit.status == OrderStatus::OPEN);
    std::cout << "PASS  Stop-limit partial fill\n";

    // 3. Cascading stops produce no infinite recursion
    Order stop3("STOP3", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 101.0,  9);
    Order stop4("STOP4", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 102.0, 10);
    engine.process_stop_order(&stop3);
    engine.process_stop_order(&stop4);

    Order b3("B3", Side::BUY, OrderType::MARKET, 10, 11);
    engine.process_market_order(&b3);

    assert(stop3.is_triggered);
    assert(stop4.is_triggered);
    std::cout << "PASS  Cascading stop triggers\n";

    // 4. No stops are skipped when multiple fire at the same price
    Order s10("S10", Side::SELL, OrderType::LIMIT, 101.0, 5, 100);
    book.insert_limit(&s10);

    Order stop5("STOP5", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 101.0, 12);
    Order stop6("STOP6", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 101.0, 13);
    engine.process_stop_order(&stop5);
    engine.process_stop_order(&stop6);

    Order b4("B4", Side::BUY, OrderType::MARKET, 5, 14);
    engine.process_market_order(&b4);

    assert(stop5.is_triggered);
    assert(stop6.is_triggered);
    std::cout << "PASS  No stop-skipping\n";

    std::cout << "ALL STOP ORDER EDGE CASES PASSED\n\n";
}

// ─── Timestamp test ───────────────────────────────────────────────────────────

void OrderBookTest::run_timestamp_test() {
    std::cout << "=== TIMESTAMP TEST ===\n";

    using namespace TimeUtils;
    constexpr int N = 100'000;

    // 1. Monotonicity
    Timestamp prev = now_ns();
    for (int i = 0; i < N; ++i) {
        Timestamp curr = now_ns();
        assert(curr > prev);
        prev = curr;
    }
    std::cout << "PASS  Monotonicity\n";

    // 2. Uniqueness
    std::unordered_set<Timestamp> seen;
    seen.reserve(N);
    for (int i = 0; i < N; ++i)
        assert(seen.insert(now_ns()).second);
    std::cout << "PASS  Uniqueness\n";

    // 3. Wall-clock ISO-8601 formatting
    const std::string iso = to_iso8601(wall_time_ns());
    assert(!iso.empty() && iso.back() == 'Z');
    std::cout << "Wall time ISO: " << iso << '\n';
    std::cout << "PASS  ISO conversion\n\n";
}

// ─── Order timestamp test ─────────────────────────────────────────────────────

void OrderBookTest::run_order_timestamp_test() {
    std::cout << "=== ORDER TIMESTAMP TEST ===\n";

    Order o1("O1", Side::BUY, OrderType::LIMIT, 100.0, 1, TimeUtils::now_ns());
    Order o2("O2", Side::BUY, OrderType::LIMIT, 100.0, 1, TimeUtils::now_ns());

    assert(o2.timestamp_ns > o1.timestamp_ns);

    std::cout << "Order1 ts: " << o1.timestamp_ns << '\n';
    std::cout << "Order2 ts: " << o2.timestamp_ns << '\n';
    std::cout << "PASS  FIFO timestamp ordering\n\n";
}

// ─── Event queue engine test ──────────────────────────────────────────────────

void OrderBookTest::run_event_queue_engine_test() {
    std::cout << "=== EVENT QUEUE ENGINE TEST ===\n";

    engine.set_trade_publisher(&publisher);

    std::thread engine_thread([this] { engine.run(queue); });

    Order o1("O1", Side::SELL, OrderType::LIMIT, 100.0, 5, TimeUtils::now_ns());
    Order o2("O2", Side::SELL, OrderType::LIMIT, 100.0, 5, TimeUtils::now_ns());
    queue.push(EngineEvent::New(&o1));
    queue.push(EngineEvent::New(&o2));

    Order b1("B1", Side::BUY, OrderType::MARKET, 6, TimeUtils::now_ns());
    queue.push(EngineEvent::New(&b1));

    wait_for_trades(2);

    assert(o1.is_filled());
    assert(o2.remaining_quantity() == 4);
    assert(b1.filled_quantity == 6);
    assert(publisher.events.size() == 2);

    print_events(publisher.events);
    std::cout << "PASS  Event queue matching\n\n";

    queue.push(EngineEvent::Stop());
    engine_thread.join();
}
