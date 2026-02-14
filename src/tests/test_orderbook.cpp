#include "core/Order.hpp"
#include "core/OrderBook.hpp"
#include "core/MatchingEngine.hpp"
#include "FeeCalculator/FeeCalculator.hpp"
#include "io/PrintBBO.hpp"
#include "io/PrintL2Snapshot.hpp"
#include "../publisher/TradePublisher.cpp"

#include <iostream>
#include <cassert>

using namespace MatchEngine;

/*
 * Test Fixture for OrderBook + MatchingEngine
 * Owns the book and engine for each test case
 */
class OrderBookTest {
public:
    OrderBookTest()
        : book(), engine(book, fee_calculator) {}

    void run_test1();
    void run_test2();
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


private:
    OrderBook book;
    MatchingEngine engine;
    FeeCalculator fee_calculator;
};

// ------------------ TEST 1 ------------------

void OrderBookTest::run_test1() {
    std::cout << "=== TEST 1: Partial + Multi-level BUY ===\n";

    Order s1("S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    Order s2("S2", Side::SELL, OrderType::LIMIT, 102.0, 5, 2);
    Order b1("B1", Side::BUY,  OrderType::LIMIT,  99.0, 5, 3);

    book.insert_limit(&s1);
    book.insert_limit(&s2);
    book.insert_limit(&b1);

    std::cout << "Initial BBO:\n";
    auto bbo = book.get_bbo();
    std::cout << bbo;

    Order b2("B2", Side::BUY, OrderType::LIMIT, 101.0, 3, 4);
    engine.process_limit_order(&b2);

    assert(s1.remaining_quantity() == 2);
    assert(b2.is_filled());

    std::cout << "After BUY 3 @ 101:\n";
    bbo = book.get_bbo();
    std::cout << bbo;

    Order b3("B3", Side::BUY, OrderType::LIMIT, 103.0, 6, 5);
    engine.process_limit_order(&b3);

    assert(s1.is_filled());
    assert(s2.remaining_quantity() == 1);

    std::cout << "After BUY 6 @ 103:\n";
    bbo = book.get_bbo();
    std::cout << bbo;

    book.cancel_order("B1");

    std::cout << "After cancel B1:\n";
    bbo = book.get_bbo();
    std::cout << bbo;
    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ TEST 2 ------------------

void OrderBookTest::run_test2() {
    std::cout << "=== TEST 2: Sweep multiple ask levels ===\n";

    Order o1("O1", Side::SELL, OrderType::LIMIT, 101.0, 2, 1);
    Order o2("O2", Side::SELL, OrderType::LIMIT, 102.0, 3, 2);
    Order o3("O3", Side::SELL, OrderType::LIMIT, 103.0, 5, 3);

    book.insert_limit(&o1);
    book.insert_limit(&o2);
    book.insert_limit(&o3);

    std::cout << "Initial BBO:\n";
    auto bbo = book.get_bbo();
    std::cout << bbo;

    Order o4("O4", Side::BUY, OrderType::LIMIT, 103.0, 8, 4);
    engine.process_limit_order(&o4);

    assert(o1.is_filled());
    assert(o4.remaining_quantity() == 0);

    std::cout << "After BUY 8 @ 103:\n";
    bbo = book.get_bbo();
    std::cout << bbo;
    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ LIMIT ORDER TEST ------------------

void OrderBookTest::run_limit_order_test() {
    std::cout << "=== LIMIT ORDER TEST ===\n";

    Order o1("Rohit","O1", Side::SELL, OrderType::LIMIT, 101.0, 2, 1);
    Order o2("Rahul","O2", Side::SELL, OrderType::LIMIT, 102.0, 3, 2);
    Order o3("Virat","O3", Side::SELL, OrderType::LIMIT, 103.0, 5, 3);

    book.insert_limit(&o1);
    book.insert_limit(&o2);
    book.insert_limit(&o3);

    std::cout << "Initial BBO:\n";
    auto bbo = book.get_bbo();
    std::cout << bbo;

    Order o4("O4", Side::BUY, OrderType::LIMIT, 103.0, 8, 4);
    engine.process_order(&o4);

    std::cout << "After BUY 8 @ 103:\n";
    assert(engine.trades.size() == 3);
    assert(engine.trades[0].price == 101.0);
    assert(engine.trades[1].price == 102.0);
    assert(engine.trades[2].price == 103.0);
    assert(engine.trades[0].quantity == 2);
    assert(engine.trades[1].quantity == 3);
    assert(engine.trades[2].quantity == 3);

    std::cout<<"Number of Trades:"<<engine.trades.size()<<"\n";

    if(engine.trades.size()) std::cout << "Trades:\n";
    for (auto& trade : engine.trades) {
        std::cout << trade.user_id << " "<<trade.buy_order_id << " " << trade.sell_order_id << " " << trade.price << " " << trade.quantity << " " << trade.timestamp << "\n";
    }
    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ MARKET ORDER TEST ------------------

void OrderBookTest::run_market_order_test() {
    std::cout << "=== MARKET ORDER TEST ===\n";

    // Resting asks
    Order s1("Rohit","S1", Side::SELL, OrderType::LIMIT, 101.0, 2, 1);
    Order s2("Rahul","S2", Side::SELL, OrderType::LIMIT, 102.0, 3, 2);
    Order s3("Virat","S3", Side::SELL, OrderType::LIMIT, 103.0, 5, 3);

    book.insert_limit(&s1);
    book.insert_limit(&s2);
    book.insert_limit(&s3);

    std::cout << "Initial BBO:\n";
    auto bbo = book.get_bbo();
    std::cout << bbo;

    // Market BUY for more than total ask liquidity (2+3+5 = 10)
    Order m1("M1", Side::BUY, OrderType::MARKET, 12, 4);
    engine.process_market_order(&m1);

    // ---------- Assertions ----------

    // All asks consumed
    assert(s1.is_filled());
    assert(s2.is_filled());
    assert(s3.is_filled());

    // Market order partially filled, remainder cancelled
    assert(m1.filled_quantity == 10);
    assert(m1.remaining_quantity() == 2);
    assert(m1.status == OrderStatus::PARTIALLY_FILLED);

    // Market order must NOT rest
    assert(m1.price_level == nullptr);

    // Trades generated correctly
    assert(engine.trades.size() == 3);

    assert(engine.trades[0].price == 101.0);
    assert(engine.trades[0].quantity == 2);

    assert(engine.trades[1].price == 102.0);
    assert(engine.trades[1].quantity == 3);

    assert(engine.trades[2].price == 103.0);
    assert(engine.trades[2].quantity == 5);

    std::cout << "After MARKET BUY 12:\n";
    bbo = book.get_bbo();
    std::cout << bbo;

    std::cout<<"Number of Trades:"<<engine.trades.size()<<"\n";

    if(engine.trades.size()) std::cout << "Trades:\n";
    for (auto& t : engine.trades) {
        std::cout << t.user_id << " "
                  << t.buy_order_id << " "
                  << t.sell_order_id << " "
                  << t.price << " "
                  << t.quantity << " "
                  << t.timestamp << "\n";
    }

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ IOC ORDER TEST ---------------------

void OrderBookTest::run_ioc_order_test() {
    std::cout << "=== IOC ORDER TEST ===\n";

    Order s1("Rohit","S1",Side::SELL, OrderType::LIMIT,101.0,3,1);
    Order s2("Virat","S2",Side::SELL, OrderType::LIMIT,103.0,5,2);

    book.insert_limit(&s1);
    book.insert_limit(&s2);

    std::cout<<"Initial BBO:\n";
    auto bbo = book.get_bbo();
    std::cout << bbo;

    Order ioc("IOC1",Side::BUY,OrderType::IOC,102.0,10,3);
    engine.process_order(&ioc);

    // ---------- Assertions ----------

    assert(s1.is_filled());                 // 3 filled
    assert(s2.remaining_quantity() == 5);  // untouched

    assert(ioc.filled_quantity == 3);
    assert(ioc.remaining_quantity() == 7);
    assert(ioc.status == OrderStatus::PARTIALLY_FILLED);

    // IOC must not rest
    assert(ioc.price_level == nullptr);

    // Trades
    assert(engine.trades.size() == 1);
    assert(engine.trades[0].price == 101.0);
    assert(engine.trades[0].quantity == 3);

    std::cout<<"After IOC BUY 10 @ 102:\n";
    bbo = book.get_bbo();
    std::cout << bbo;

    std::cout<<"Number of Trades:"<<engine.trades.size()<<"\n";

    if(engine.trades.size()) std::cout << "Trades:\n";
    for (auto& t : engine.trades) {
        std::cout << t.user_id << " "
                  << t.buy_order_id << " "
                  << t.sell_order_id << " "
                  << t.price << " "
                  << t.quantity << " "
                  << t.timestamp << "\n";
    }
    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ FOK ORDER TEST ---------------------

void OrderBookTest::run_fok_order_test() {
    std::cout << "=== FOK ORDER TEST ===\n";

    Order s1("Rohit","S1", Side::SELL, OrderType::LIMIT, 101.0, 3, 1);
    Order s2("Virat","S2", Side::SELL, OrderType::LIMIT, 102.0, 2, 2);

    book.insert_limit(&s1);
    book.insert_limit(&s2);

    std::cout << "Initial BBO:\n";
    auto bbo = book.get_bbo();
    std::cout << bbo;

    Order fok("FOK1", Side::BUY, OrderType::FOK, 103.0, 6, 3);
    engine.process_order(&fok);

    // ---------- Assertions ----------

    // Book unchanged
    assert(s1.remaining_quantity() == 3);
    assert(s2.remaining_quantity() == 2);

    // No trades
    assert(engine.trades.empty());

    // Order cancelled
    assert(fok.filled_quantity == 0);
    assert(fok.status == OrderStatus::CANCELLED);

    std::cout << "After FOK BUY 6 @ 103:\n";
    bbo = book.get_bbo();
    std::cout << bbo;

    std::cout<<"Number of Trades:"<<engine.trades.size()<<"\n";

    if(engine.trades.size()) std::cout << "Trades:\n";
    for (auto& t : engine.trades) {
        std::cout << t.user_id << " "
                  << t.buy_order_id << " "
                  << t.sell_order_id << " "
                  << t.price << " "
                  << t.quantity << " "
                  << t.timestamp << "\n";
    }
    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ STATUS STATE MACHINE TEST ------------------

void OrderBookTest::run_status_state_machine_test() {
    std::cout << "=== STATUS STATE MACHINE TEST ===\n";

    Order s1("Virat","S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    book.insert_limit(&s1);

    assert(s1.status == OrderStatus::OPEN);

    Order b1("B1", Side::BUY, OrderType::LIMIT, 101.0, 3, 2);
    engine.process_limit_order(&b1);

    assert(b1.status == OrderStatus::COMPLETED);
    assert(s1.status == OrderStatus::PARTIALLY_FILLED);

    book.cancel_order("S1");
    assert(s1.status == OrderStatus::CANCELLED);

    // Terminal states must not change
    assert(s1.remaining_quantity() == 2);

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ CANCEL SAFETY TEST ------------------

void OrderBookTest::run_cancel_partial_fill_test() {
    std::cout << "=== CANCEL PARTIAL FILL TEST ===\n";

    Order s1("Virat","S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    book.insert_limit(&s1);

    Order b1("B1", Side::BUY, OrderType::LIMIT, 101.0, 3, 2);
    engine.process_limit_order(&b1);

    // s1 partially filled: remaining = 2
    assert(s1.status == OrderStatus::PARTIALLY_FILLED);
    assert(s1.remaining_quantity() == 2);

    bool cancelled = book.cancel_order("S1");
    assert(cancelled);
    assert(s1.status == OrderStatus::CANCELLED);

    // Book must be empty on ask side
    assert(book.get_best_ask() == nullptr);

    // Cancel again must fail safely
    assert(book.cancel_order("S1") == false);

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ GLOBAL INVARIANT TEST ------------------

void OrderBookTest::run_global_invariant_test() {
    std::cout << "=== GLOBAL INVARIANT TEST ===\n";
    Order s1("Virat","S1", Side::SELL, OrderType::LIMIT, 101.0, 5, 1);
    Order b1("B1", Side::BUY,  OrderType::LIMIT, 101.0, 5, 2);

    book.insert_limit(&s1);
    engine.process_limit_order(&b1);

    assert(book.get_best_bid() == nullptr);
    assert(book.get_best_ask() == nullptr);
    assert(engine.trades.size() == 1);

    const Trade& t = engine.trades[0];
    assert(t.price == 101.0);
    assert(t.quantity == 5);

    std::cout<<"Number of Trades:"<<engine.trades.size()<<"\n";

    if(engine.trades.size()) std::cout << "Trades:\n";
    for (auto& tr : engine.trades) {
        std::cout << tr.user_id << " "
                  << tr.buy_order_id << " "
                  << tr.sell_order_id << " "
                  << tr.price << " "
                  << tr.quantity << " "
                  << tr.timestamp << "\n";
    }

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ FEE TIER PROMOTION TEST ------------------

void OrderBookTest::run_fee_tier_test() {
    std::cout << "=== FEE TIER TEST ===\n";

    Order s1("Virat","S1", Side::SELL, OrderType::LIMIT, 100.0, 2000, 1);
    book.insert_limit(&s1);

    Order b1("B1", Side::BUY, OrderType::MARKET, 2000, 2);
    engine.process_market_order(&b1);

    const Trade& t = engine.trades[0];

    std::cout<<"Number of Trades:"<<engine.trades.size()<<"\n";

    if(engine.trades.size()) std::cout << "Trades:\n";
    for (auto& tr : engine.trades) {
        std::cout << tr.user_id << " "
                  << tr.buy_order_id << " "
                  << tr.sell_order_id << " "
                  << tr.price << " "
                  << tr.quantity << " "
                  << tr.timestamp << "\n";
    }

    // notional = 200,000 → Tier 1
    assert(t.maker_fee == 200000.0 * -0.0001);
    assert(t.taker_fee == 200000.0 *  0.0004);

    std::cout << "Maker fee: " << t.maker_fee << "\n";
    std::cout << "Taker fee: " << t.taker_fee << "\n";

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ MARKET DATA TEST ------------------

void OrderBookTest::run_market_data_test() {
    std::cout << "=== MARKET DATA TEST ===\n";

    Order b1("B1", Side::BUY,  OrderType::LIMIT, 99.0, 5, 1);
    Order b2("B2", Side::BUY,  OrderType::LIMIT, 98.0, 3, 2);
    Order s1("S1", Side::SELL, OrderType::LIMIT, 101.0, 4, 3);
    Order s2("S2", Side::SELL, OrderType::LIMIT, 102.0, 6, 4);

    book.insert_limit(&b1);
    book.insert_limit(&b2);
    book.insert_limit(&s1);
    book.insert_limit(&s2);

    BBO bbo = book.get_bbo();
    assert(bbo.has_bid && bbo.bid_price == 99.0 && bbo.bid_quantity == 5);
    assert(bbo.has_ask && bbo.ask_price == 101.0 && bbo.ask_quantity == 4);

    L2Snapshot snap = book.get_l2_snapshot(2);
    assert(snap.bids.size() == 2);
    assert(snap.asks.size() == 2);

    assert(snap.bids[0].price == 99.0);
    assert(snap.bids[1].price == 98.0);

    assert(snap.asks[0].price == 101.0);
    assert(snap.asks[1].price == 102.0);

    std::cout << bbo;
    std::cout << snap;

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ TRADE STREAM TEST ------------------

void OrderBookTest::run_trade_stream_test() {
    std::cout << "=== TRADE STREAM TEST ===\n";

    InMemoryTradePublisher publisher;
    engine.set_trade_publisher(&publisher);

    Order s1("S1", Side::SELL, OrderType::LIMIT, 100.0, 5, 1);
    book.insert_limit(&s1);

    Order b1("B1", Side::BUY, OrderType::MARKET, 5, 2);
    engine.process_market_order(&b1);

    assert(publisher.events.size() == 1);

    const TradeEvent& ev = publisher.events[0];
    assert(ev.price == 100.0);
    assert(ev.quantity == 5);
    assert(ev.buy_order_id == "B1");
    assert(ev.sell_order_id == "S1");

    std::cout << "Trade published at price "
              << ev.price << " qty "
              << ev.quantity << "\n";

    std::cout<<"\n TEST CASE PASSED\n\n\n\n";
}

// ------------------ STOP LOSS TEST ------------------

void OrderBookTest::run_stop_loss_test() {
    std::cout << "=== STOP ORDERS EDGE CASE TESTS ===\n";

    // ------------------ Setup Liquidity ------------------
    Order s1("S1", Side::SELL, OrderType::LIMIT, 100.0, 5, 1);
    Order s2("S2", Side::SELL, OrderType::LIMIT, 101.0, 5, 2);
    Order s3("S3", Side::SELL, OrderType::LIMIT, 102.0, 5, 3);

    book.insert_limit(&s1);
    book.insert_limit(&s2);
    book.insert_limit(&s3);

    // ------------------ 1️⃣ Multiple stops triggered at same price (FIFO) ------------------
    Order stop1("STOP1", Side::BUY, OrderType::STOP_LOSS, 0.0, 2, 4);
    stop1.stop_price = 100.0;

    Order stop2("STOP2", Side::BUY, OrderType::STOP_LOSS, 0.0, 2, 5);
    stop2.stop_price = 100.0;

    engine.process_stop_order(&stop1);
    engine.process_stop_order(&stop2);

    Order b1("B1", Side::BUY, OrderType::MARKET, 5, 6);
    engine.process_market_order(&b1);

    assert(stop1.is_triggered);
    assert(stop2.is_triggered);

    // FIFO guarantee: stop1 executes before stop2
    assert(engine.trades[1].buy_order_id == "STOP1");
    assert(engine.trades[2].buy_order_id == "STOP2");

    std::cout << "✔ FIFO triggering passed\n";

    // ------------------ 2️⃣ Stop-limit may not fully fill ------------------
    Order stop_limit("STOP_LIMIT1", Side::BUY, OrderType::STOP_LIMIT, 101.0, 10, 7);
    stop_limit.stop_price = 101.0;

    engine.process_stop_order(&stop_limit);

    Order b2("B2", Side::BUY, OrderType::MARKET, 5, 8);
    engine.process_market_order(&b2);

    assert(stop_limit.is_triggered);
    assert(stop_limit.status == OrderStatus::PARTIALLY_FILLED ||
           stop_limit.status == OrderStatus::OPEN);

    std::cout << "✔ Stop-limit partial fill behavior passed\n";

    // ------------------ 3️⃣ Stop triggers cascade (no infinite recursion) ------------------
    Order stop3("STOP3", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 9);
    stop3.stop_price = 101.0;

    Order stop4("STOP4", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 10);
    stop4.stop_price = 102.0;

    engine.process_stop_order(&stop3);
    engine.process_stop_order(&stop4);

    Order b3("B3", Side::BUY, OrderType::MARKET, 10, 11);
    engine.process_market_order(&b3);

    assert(stop3.is_triggered);
    assert(stop4.is_triggered);

    std::cout << "✔ Cascading stop triggers passed\n";

    // ------------------ 4️⃣ Triggering must not skip other stops ------------------
    Order s10("S10", Side::SELL, OrderType::LIMIT, 101.0, 5, 100);
    book.insert_limit(&s10);
    
    Order stop5("STOP5", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 12);
    stop5.stop_price = 101.0;

    Order stop6("STOP6", Side::BUY, OrderType::STOP_LOSS, 0.0, 1, 13);
    stop6.stop_price = 101.0;

    engine.process_stop_order(&stop5);
    engine.process_stop_order(&stop6);

    Order b4("B4", Side::BUY, OrderType::MARKET, 5, 14);
    engine.process_market_order(&b4);

    assert(stop5.is_triggered);
    assert(stop6.is_triggered);

    std::cout << "✔ No stop-skipping passed\n";

    std::cout << "\nALL STOP ORDER EDGE CASES PASSED ✅\n\n";
}

// ------------------ PUBLIC ENTRY POINTS ------------------

void test1() {
    OrderBookTest test;
    test.run_test1();
}

void test2() {
    OrderBookTest test;
    test.run_test2();
}

void limit_order_test() {
    OrderBookTest test;
    test.run_limit_order_test();
}

void market_order_test() {
    OrderBookTest test;
    test.run_market_order_test();
}

void ioc_order_test() {
    OrderBookTest test;
    test.run_ioc_order_test();
}

void fok_order_test() {
    OrderBookTest test;
    test.run_fok_order_test();
}

void status_state_machine_test() {
    OrderBookTest test;
    test.run_status_state_machine_test();
}

void cancel_partial_fill_test() {
    OrderBookTest test;
    test.run_cancel_partial_fill_test();
}

void global_invariant_test() {
    OrderBookTest test;
    test.run_global_invariant_test();
}

void fee_tier_test() {
    OrderBookTest test;
    test.run_fee_tier_test();
}

void market_data_test() {
    OrderBookTest test;
    test.run_market_data_test();
}

void trade_stream_test() {
    OrderBookTest test;
    test.run_trade_stream_test();
}

void stop_loss_test() {
    OrderBookTest test;
    test.run_stop_loss_test();
}