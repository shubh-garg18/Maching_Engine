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

// ─── Combined full-system test ────────────────────────────────────────────────
//
// Walks through every order type and subsystem in one coherent scenario.
// Each part builds on the shared fixture; order IDs use part-specific prefixes
// to prevent collisions, and leftover resting orders are cancelled before the
// next part begins.
//
// Parts:
//   1  Limit orders        — resting, partial fill, multi-level sweep, cancel
//   2  Market orders       — full sweep, partial fill, no-resting invariant
//   3  IOC orders          — partial fill + cancel remainder, no resting
//   4  FOK orders          — kill on insufficient liquidity, fill on exact match
//   5  Stop-loss           — FIFO triggering, cascading stops
//   6  Stop-limit          — triggered but may only partially fill
//   7  Status state machine — OPEN → PARTIALLY_FILLED → CANCELLED / COMPLETED
//   8  OrderBook invariants — book empty after full match, double-cancel safe
//   9  Market data         — BBO and L2 snapshot accuracy
//  10  Fee tiers           — Tier-1 notional, maker/taker rates
//  11  Trade publisher     — event fields, timestamps present
//  12  Timestamps          — monotonicity, uniqueness, ISO-8601 wall time
//  13  Event queue         — async engine thread, publisher receives events

void OrderBookTest::run_combined_test() {
    std::cout << "=== COMBINED FULL SYSTEM TEST ===\n\n";

    // ── Part 1: Limit orders ─────────────────────────────────────────────────
    std::cout << "-- Part 1: Limit Orders --\n";
    {
        Order s1("P1_S1", Side::SELL, OrderType::LIMIT, 100.0, 5, TimeUtils::now_ns());
        Order s2("P1_S2", Side::SELL, OrderType::LIMIT, 101.0, 3, TimeUtils::now_ns());
        Order s3("P1_S3", Side::SELL, OrderType::LIMIT, 102.0, 4, TimeUtils::now_ns());
        Order b1("P1_B1", Side::BUY,  OrderType::LIMIT,  99.0, 5, TimeUtils::now_ns());

        book.insert_limit(&s1);
        book.insert_limit(&s2);
        book.insert_limit(&s3);
        book.insert_limit(&b1);

        BBO bbo = book.get_bbo();
        assert(bbo.has_bid && bbo.bid_price == 99.0  && bbo.bid_quantity == 5);
        assert(bbo.has_ask && bbo.ask_price == 100.0 && bbo.ask_quantity == 5);

        L2Snapshot snap = book.get_l2_snapshot(3);
        assert(snap.bids.size() == 1 && snap.asks.size() == 3);
        assert(snap.asks[0].price == 100.0);
        assert(snap.asks[1].price == 101.0);
        assert(snap.asks[2].price == 102.0);

        // Partial fill on S1: BUY 3 @ 100
        size_t t0 = engine.trades.size();
        Order b2("P1_B2", Side::BUY, OrderType::LIMIT, 100.0, 3, TimeUtils::now_ns());
        engine.process_limit_order(&b2);

        assert(engine.trades.size() == t0 + 1);
        assert(engine.trades[t0].price == 100.0 && engine.trades[t0].quantity == 3);
        assert(b2.is_filled());
        assert(s1.remaining_quantity() == 2);
        assert(s1.status == OrderStatus::PARTIALLY_FILLED);

        // Multi-level sweep: BUY 8 @ 102 → fills S1(2) + S2(3) + S3(3)
        t0 = engine.trades.size();
        Order b3("P1_B3", Side::BUY, OrderType::LIMIT, 102.0, 8, TimeUtils::now_ns());
        engine.process_limit_order(&b3);

        assert(engine.trades.size() == t0 + 3);
        assert(s1.is_filled());
        assert(s2.is_filled());
        assert(s3.remaining_quantity() == 1);
        assert(b3.is_filled());

        // Cancel resting bid B1 and partially-filled ask S3
        assert(book.cancel_order("P1_B1") && b1.status == OrderStatus::CANCELLED);
        assert(book.cancel_order("P1_S3") && s3.status == OrderStatus::CANCELLED);
        assert(book.get_best_bid() == nullptr);
        assert(book.get_best_ask() == nullptr);

        std::cout << "PASS  Limit orders\n";
    }

    // ── Part 2: Market orders ────────────────────────────────────────────────
    std::cout << "-- Part 2: Market Orders --\n";
    {
        Order s1("P2_S1", Side::SELL, OrderType::LIMIT, 200.0, 10, TimeUtils::now_ns());
        Order s2("P2_S2", Side::SELL, OrderType::LIMIT, 201.0, 10, TimeUtils::now_ns());
        Order s3("P2_S3", Side::SELL, OrderType::LIMIT, 202.0, 10, TimeUtils::now_ns());
        book.insert_limit(&s1);
        book.insert_limit(&s2);
        book.insert_limit(&s3);

        // MARKET BUY 25 — exhausts S1(10) + S2(10) + partial S3(5)
        size_t t0 = engine.trades.size();
        Order m1("P2_M1", Side::BUY, OrderType::MARKET, 25, TimeUtils::now_ns());
        engine.process_market_order(&m1);

        assert(engine.trades.size() == t0 + 3);
        assert(s1.is_filled() && s2.is_filled());
        assert(s3.remaining_quantity() == 5);
        assert(m1.filled_quantity == 25 && m1.status == OrderStatus::COMPLETED);
        assert(m1.price_level == nullptr);       // market orders never rest

        // MARKET BUY 10 — only 5 remain → partially filled
        t0 = engine.trades.size();
        Order m2("P2_M2", Side::BUY, OrderType::MARKET, 10, TimeUtils::now_ns());
        engine.process_market_order(&m2);

        assert(engine.trades.size() == t0 + 1);
        assert(s3.is_filled());
        assert(m2.filled_quantity == 5 && m2.status == OrderStatus::PARTIALLY_FILLED);
        assert(m2.price_level == nullptr);
        assert(book.get_best_ask() == nullptr);

        std::cout << "PASS  Market orders\n";
    }

    // ── Part 3: IOC orders ───────────────────────────────────────────────────
    std::cout << "-- Part 3: IOC Orders --\n";
    {
        Order s1("P3_S1", Side::SELL, OrderType::LIMIT, 300.0, 5, TimeUtils::now_ns());
        Order s2("P3_S2", Side::SELL, OrderType::LIMIT, 305.0, 5, TimeUtils::now_ns());
        book.insert_limit(&s1);
        book.insert_limit(&s2);

        // IOC BUY @ 301, qty 10: crosses S1(300) but not S2(305 > 301)
        size_t t0 = engine.trades.size();
        Order ioc("P3_IOC", Side::BUY, OrderType::IOC, 301.0, 10, TimeUtils::now_ns());
        engine.process_order(&ioc);

        assert(engine.trades.size() == t0 + 1);
        assert(s1.is_filled());
        assert(s2.remaining_quantity() == 5);
        assert(ioc.filled_quantity == 5 && ioc.remaining_quantity() == 5);
        assert(ioc.status == OrderStatus::PARTIALLY_FILLED);
        assert(ioc.price_level == nullptr);      // IOC must not rest

        book.cancel_order("P3_S2");
        std::cout << "PASS  IOC orders\n";
    }

    // ── Part 4: FOK orders ───────────────────────────────────────────────────
    std::cout << "-- Part 4: FOK Orders --\n";
    {
        Order s1("P4_S1", Side::SELL, OrderType::LIMIT, 400.0, 3, TimeUtils::now_ns());
        Order s2("P4_S2", Side::SELL, OrderType::LIMIT, 401.0, 3, TimeUtils::now_ns());
        book.insert_limit(&s1);
        book.insert_limit(&s2);

        // FOK BUY @ 402 qty 8: total available = 6, need 8 → cancel
        size_t t0 = engine.trades.size();
        Order fok1("P4_FOK1", Side::BUY, OrderType::FOK, 402.0, 8, TimeUtils::now_ns());
        engine.process_order(&fok1);

        assert(engine.trades.size() == t0);      // no trades
        assert(s1.remaining_quantity() == 3 && s2.remaining_quantity() == 3);
        assert(fok1.status == OrderStatus::CANCELLED);

        // Add missing liquidity → total becomes 3+3+2 = 8 exactly
        Order s3("P4_S3", Side::SELL, OrderType::LIMIT, 402.0, 2, TimeUtils::now_ns());
        book.insert_limit(&s3);

        t0 = engine.trades.size();
        Order fok2("P4_FOK2", Side::BUY, OrderType::FOK, 402.0, 8, TimeUtils::now_ns());
        engine.process_order(&fok2);

        assert(engine.trades.size() == t0 + 3);
        assert(s1.is_filled() && s2.is_filled() && s3.is_filled());
        assert(fok2.is_filled());

        std::cout << "PASS  FOK orders\n";
    }

    // ── Part 5: Stop-loss orders ─────────────────────────────────────────────
    std::cout << "-- Part 5: Stop-Loss Orders --\n";
    {
        Order liq1("P5_LIQ1", Side::SELL, OrderType::LIMIT, 500.0, 10, TimeUtils::now_ns());
        Order liq2("P5_LIQ2", Side::SELL, OrderType::LIMIT, 501.0, 10, TimeUtils::now_ns());
        book.insert_limit(&liq1);
        book.insert_limit(&liq2);

        // Two stops at the same price — FIFO guarantee
        Order stop1("P5_STOP1", Side::BUY, OrderType::STOP_LOSS, 0.0, 3, 500.0, TimeUtils::now_ns());
        Order stop2("P5_STOP2", Side::BUY, OrderType::STOP_LOSS, 0.0, 3, 500.0, TimeUtils::now_ns());
        engine.process_stop_order(&stop1);
        engine.process_stop_order(&stop2);

        // Market BUY triggers trade @ 500 → both stops fire
        size_t t0 = engine.trades.size();
        Order trig("P5_TRIG", Side::BUY, OrderType::MARKET, 4, TimeUtils::now_ns());
        engine.process_market_order(&trig);

        assert(stop1.is_triggered && stop2.is_triggered);
        assert(engine.trades[t0 + 1].buy_order_id == "P5_STOP1");  // FIFO
        assert(engine.trades[t0 + 2].buy_order_id == "P5_STOP2");

        book.cancel_order("P5_LIQ2");           // clean up untouched liquidity
        std::cout << "PASS  Stop-loss orders\n";
    }

    // ── Part 6: Stop-limit orders ────────────────────────────────────────────
    std::cout << "-- Part 6: Stop-Limit Orders --\n";
    {
        // 5 units at 600; stop-limit wants 10 → will only partially fill
        Order liq("P6_LIQ", Side::SELL, OrderType::LIMIT, 600.0, 5, TimeUtils::now_ns());
        book.insert_limit(&liq);

        Order sl("P6_SL", Side::BUY, OrderType::STOP_LIMIT, 601.0, 10, 600.0, TimeUtils::now_ns());
        engine.process_stop_order(&sl);

        // Trade at 600 triggers the stop-limit
        Order trig("P6_TRIG", Side::BUY, OrderType::MARKET, 2, TimeUtils::now_ns());
        engine.process_market_order(&trig);

        assert(sl.is_triggered);
        // Filled remaining 3 from liq; 7 unfilled → rests on book at 601
        assert(sl.status == OrderStatus::PARTIALLY_FILLED ||
               sl.status == OrderStatus::OPEN);
        assert(sl.remaining_quantity() > 0);

        book.cancel_order("P6_SL");
        std::cout << "PASS  Stop-limit orders\n";
    }

    // ── Part 7: Order status state machine ───────────────────────────────────
    std::cout << "-- Part 7: Order Status State Machine --\n";
    {
        Order s1("P7_S1", Side::SELL, OrderType::LIMIT, 700.0, 5, TimeUtils::now_ns());
        book.insert_limit(&s1);
        assert(s1.status == OrderStatus::OPEN);

        // Partial fill → PARTIALLY_FILLED
        Order b1("P7_B1", Side::BUY, OrderType::LIMIT, 700.0, 3, TimeUtils::now_ns());
        engine.process_limit_order(&b1);
        assert(b1.status == OrderStatus::COMPLETED);
        assert(s1.status == OrderStatus::PARTIALLY_FILLED);
        assert(s1.remaining_quantity() == 2);

        // Cancel partially-filled → CANCELLED; remaining qty is preserved
        assert(book.cancel_order("P7_S1"));
        assert(s1.status == OrderStatus::CANCELLED);
        assert(s1.remaining_quantity() == 2);

        // Full fill → both sides COMPLETED
        Order s2("P7_S2", Side::SELL, OrderType::LIMIT, 700.0, 3, TimeUtils::now_ns());
        book.insert_limit(&s2);
        Order b2("P7_B2", Side::BUY, OrderType::LIMIT, 700.0, 3, TimeUtils::now_ns());
        engine.process_limit_order(&b2);
        assert(s2.status == OrderStatus::COMPLETED);
        assert(b2.status == OrderStatus::COMPLETED);

        std::cout << "PASS  Order status state machine\n";
    }

    // ── Part 8: OrderBook invariants ─────────────────────────────────────────
    std::cout << "-- Part 8: OrderBook Invariants --\n";
    {
        Order s1("P8_S1", Side::SELL, OrderType::LIMIT, 800.0, 5, TimeUtils::now_ns());
        Order b1("P8_B1", Side::BUY,  OrderType::LIMIT, 800.0, 5, TimeUtils::now_ns());
        book.insert_limit(&s1);
        engine.process_limit_order(&b1);

        // Full match: both sides empty, both orders completed
        assert(book.get_best_bid() == nullptr);
        assert(book.get_best_ask() == nullptr);
        assert(s1.status == OrderStatus::COMPLETED);
        assert(b1.status == OrderStatus::COMPLETED);

        // Double-cancel on an already-cancelled order returns false
        Order s2("P8_S2", Side::SELL, OrderType::LIMIT, 800.0, 2, TimeUtils::now_ns());
        book.insert_limit(&s2);
        assert(book.cancel_order("P8_S2"));
        assert(!book.cancel_order("P8_S2"));     // not in map → safe

        std::cout << "PASS  OrderBook invariants\n";
    }

    // ── Part 9: Market data ───────────────────────────────────────────────────
    std::cout << "-- Part 9: Market Data (BBO + L2 Snapshot) --\n";
    {
        Order b1("P9_B1", Side::BUY,  OrderType::LIMIT, 900.0, 5, TimeUtils::now_ns());
        Order b2("P9_B2", Side::BUY,  OrderType::LIMIT, 899.0, 3, TimeUtils::now_ns());
        Order s1("P9_S1", Side::SELL, OrderType::LIMIT, 902.0, 4, TimeUtils::now_ns());
        Order s2("P9_S2", Side::SELL, OrderType::LIMIT, 903.0, 6, TimeUtils::now_ns());
        book.insert_limit(&b1);
        book.insert_limit(&b2);
        book.insert_limit(&s1);
        book.insert_limit(&s2);

        BBO bbo = book.get_bbo();
        assert(bbo.has_bid && bbo.bid_price == 900.0 && bbo.bid_quantity == 5);
        assert(bbo.has_ask && bbo.ask_price == 902.0 && bbo.ask_quantity == 4);

        L2Snapshot snap = book.get_l2_snapshot(2);
        assert(snap.bids.size() == 2 && snap.asks.size() == 2);
        assert(snap.bids[0].price == 900.0 && snap.bids[1].price == 899.0);
        assert(snap.asks[0].price == 902.0 && snap.asks[1].price == 903.0);

        book.cancel_order("P9_B1");
        book.cancel_order("P9_B2");
        book.cancel_order("P9_S1");
        book.cancel_order("P9_S2");

        std::cout << "PASS  Market data\n";
    }

    // ── Part 10: Fee tiers ────────────────────────────────────────────────────
    std::cout << "-- Part 10: Fee Tiers --\n";
    {
        // notional = 100.0 * 2000 = 200,000 → Tier 1
        // maker (resting) must have a different user_id than taker so
        // FeeCalculator::update_volume is called and the tier can be promoted.
        Order s1("Virat", "P10_S1", Side::SELL, OrderType::LIMIT, 100.0, 2000, TimeUtils::now_ns());
        book.insert_limit(&s1);

        Order b1("P10_B1", Side::BUY, OrderType::MARKET, 2000, TimeUtils::now_ns());
        engine.process_market_order(&b1);

        const Trade& t = engine.trades.back();
        assert(t.maker_fee == 200000.0 * -0.0001);
        assert(t.taker_fee == 200000.0 *  0.0004);

        std::cout << "  maker_fee=" << t.maker_fee
                  << " taker_fee=" << t.taker_fee << '\n';
        std::cout << "PASS  Fee tiers\n";
    }

    // ── Part 11: Trade publisher ──────────────────────────────────────────────
    std::cout << "-- Part 11: Trade Publisher --\n";
    {
        engine.set_trade_publisher(&publisher);
        const size_t ev0 = publisher.events.size();

        Order s1("P11_S1", Side::SELL, OrderType::LIMIT, 1000.0, 5, TimeUtils::now_ns());
        book.insert_limit(&s1);

        Order b1("P11_B1", Side::BUY, OrderType::MARKET, 5, TimeUtils::now_ns());
        engine.process_market_order(&b1);

        assert(publisher.events.size() == ev0 + 1);
        const TradeEvent& ev = publisher.events.back();
        assert(ev.price          == 1000.0);
        assert(ev.quantity       == 5);
        assert(ev.buy_order_id   == "P11_B1");
        assert(ev.sell_order_id  == "P11_S1");
        assert(ev.engine_ts      > 0);
        assert(ev.wall_ts        > 0);

        std::cout << "PASS  Trade publisher\n";
    }

    // ── Part 12: Timestamps ───────────────────────────────────────────────────
    std::cout << "-- Part 12: Timestamps --\n";
    {
        using namespace TimeUtils;
        constexpr int N = 10'000;

        // Monotonicity
        Timestamp prev = now_ns();
        for (int i = 0; i < N; ++i) {
            Timestamp curr = now_ns();
            assert(curr > prev);
            prev = curr;
        }

        // Uniqueness
        std::unordered_set<Timestamp> seen;
        seen.reserve(N);
        for (int i = 0; i < N; ++i)
            assert(seen.insert(now_ns()).second);

        // Wall-clock ISO-8601 validity
        const std::string iso = to_iso8601(wall_time_ns());
        assert(!iso.empty() && iso.back() == 'Z');

        // Order timestamps respect FIFO ordering
        Order o1("P12_O1", Side::BUY, OrderType::LIMIT, 1.0, 1, now_ns());
        Order o2("P12_O2", Side::BUY, OrderType::LIMIT, 1.0, 1, now_ns());
        assert(o2.timestamp_ns > o1.timestamp_ns);

        std::cout << "PASS  Timestamps\n";
    }

    // ── Part 13: Event queue (async engine) ───────────────────────────────────
    std::cout << "-- Part 13: Event Queue (Async Engine) --\n";
    {
        // publisher already set in Part 11
        const size_t ev0 = publisher.events.size();

        std::thread engine_thread([this] { engine.run(queue); });

        Order s1("P13_S1", Side::SELL, OrderType::LIMIT, 1100.0, 5, TimeUtils::now_ns());
        Order s2("P13_S2", Side::SELL, OrderType::LIMIT, 1100.0, 5, TimeUtils::now_ns());
        queue.push(EngineEvent::New(&s1));
        queue.push(EngineEvent::New(&s2));

        Order b1("P13_B1", Side::BUY, OrderType::MARKET, 7, TimeUtils::now_ns());
        queue.push(EngineEvent::New(&b1));

        wait_for_trades(ev0 + 2);

        assert(s1.is_filled());
        assert(s2.remaining_quantity() == 3);
        assert(b1.filled_quantity == 7);
        assert(publisher.events.size() == ev0 + 2);

        queue.push(EngineEvent::Stop());
        engine_thread.join();

        std::cout << "PASS  Event queue\n";
    }

    std::cout << "\n=== COMBINED FULL SYSTEM TEST PASSED ===\n\n";
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
