# High-Performance Order Matching Engine

A low-latency, price-time priority matching engine built in C++ for limit order book simulations and exchange system prototyping.

## Features

- **Price-Time Priority**: Standard FIFO matching at each price level
- **Multiple Order Types**: LIMIT, MARKET, IOC (Immediate-or-Cancel), FOK (Fill-or-Kill)
- **Constant-Time Cancellation**: O(1) order cancellation via order ID indexing
- **Market Data**: Real-time BBO (Best Bid/Offer) and L2 depth snapshots
- **Fee Engine**: Maker-taker fee model with configurable rates
- **Trade Publishing**: Event-driven trade notification system

---

## Architecture Overview

The system follows a layered architecture separating concerns between order management, matching logic, and market data distribution.

```text
┌─────────────────────────────────────────────────────────────┐
│                      MatchingEngine                         │
│  • Order validation and routing                            │
│  • Symbol-to-OrderBook mapping                             │
│  • Trade event coordination                                │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ├──► OrderBook (per symbol)
                       │     ├─ Bid side (std::map<price>)
                       │     ├─ Ask side (std::map<price>)
                       │     ├─ Order index (std::unordered_map)
                       │     └─ Matching loop (price-time priority)
                       │
                       ├──► TradePublisher
                       │     └─ Broadcasts fills to subscribers
                       │
                       └──► Market Data Layer
                             ├─ BBO (Best Bid/Offer)
                             ├─ L2Snapshot (Depth aggregation)
                             └─ TradeEvent (Fill notifications)
```

### Core Components

#### OrderBook

- Maintains separate bid and ask sides using sorted price levels
- Each price level contains a FIFO queue of orders
- Provides O(1) order lookup via hash map indexing
- Enforces matching logic and generates trade events

#### MatchingEngine

- Routes orders to the appropriate OrderBook by symbol
- Validates order parameters (price, quantity, type)
- Coordinates between multiple order books
- Acts as the central orchestrator for the system

#### TradePublisher

- Event-driven notification system for trade execution
- Decouples matching logic from downstream systems
- Enables real-time feed handlers, risk systems, and analytics

#### Market Data Layer

- **BBO**: Real-time best bid/ask with sizes
- **L2Snapshot**: Aggregated depth at each price level
- **TradeEvent**: Immutable records of executed trades with timestamps

#### FeeCalculator

- Implements maker-taker fee structure
- Differentiates between liquidity providers (makers) and takers
- Supports configurable fee rates per order

---

## Complexity Guarantees

The engine provides provable time complexity bounds for all critical operations:

| Operation          | Complexity | Notes                              |
| ------------------ | ---------- | ---------------------------------- |
| Limit Insert       | O(log P)   | P = number of active price levels  |
| Market / IOC Match | O(L + K)   | L = crossed levels, K = fills      |
| FOK                | O(L + K)   | Includes liquidity pre-scan        |
| Cancel             | O(1)       | Direct hash lookup                 |
| BBO                | O(1)       | Cached best prices                 |
| L2 Snapshot        | O(D)       | D = requested depth                |

**Key Properties:**

- No empty price levels maintained
- No allocations inside matching loop
- Sequential memory access during matching (cache-friendly)
- Minimal branching in hot paths

---

## Performance Audit

For detailed analysis of algorithmic complexity, memory behavior, hot paths, and scalability considerations:

👉 **[See Performance Audit](docs/Performance_Audit.md)**

This document includes:

- Detailed time complexity proofs
- Hot path walk-through with memory access patterns
- Hidden costs and edge case analysis
- Performance-critical invariants
- Future optimization opportunities

---

## Project Structure

```text
.
├── README.md
├── docs
│   └── Performance_Audit.md          # Detailed complexity analysis
├── include
│   ├── core                           # Order book and matching engine
│   │   ├── MatchingEngine.hpp
│   │   ├── Order.hpp
│   │   ├── OrderBook.hpp
│   │   └── PriceLevel.hpp
│   ├── market_data                    # Market data structures
│   │   ├── BBO.hpp
│   │   ├── L2Snapshot.hpp
│   │   └── TradeEvent.hpp
│   ├── publisher                      # Event distribution
│   │   └── TradePublisher.hpp
│   ├── FeeCalculator                  # Fee engine
│   │   └── FeeCalculator.hpp
│   ├── io                             # Display utilities
│   │   ├── PrintBBO.hpp
│   │   └── PrintL2Snapshot.hpp
│   └── utils
│       └── Types.hpp
├── src
│   ├── core
│   ├── market_data
│   ├── publisher
│   ├── FeeCalculator
│   ├── io
│   ├── tests
│   └── main.cpp
└── test_engine
```

---

## Building the Project

```bash
# Compile (adjust flags as needed)
g++ -std=c++17 -O3 -I./include src/**/*.cpp -o matching_engine

# Run
./matching_engine
```

---

## Example Usage

```cpp
#include "core/MatchingEngine.hpp"

int main() {
    MatchingEngine engine;
    
    // Submit limit orders
    engine.submit_order(Order{
        .id = 1,
        .symbol = "AAPL",
        .side = Side::BUY,
        .type = OrderType::LIMIT,
        .price = 150.0,
        .quantity = 100
    });
    
    engine.submit_order(Order{
        .id = 2,
        .symbol = "AAPL",
        .side = Side::SELL,
        .type = OrderType::LIMIT,
        .price = 151.0,
        .quantity = 50
    });
    
    // Get market data
    auto bbo = engine.get_bbo("AAPL");
    auto l2 = engine.get_l2_snapshot("AAPL", 5);
    
    // Cancel order
    engine.cancel_order("AAPL", 1);
    
    return 0;
}
```

---

## Design Decisions

### Why `std::map` for Price Levels?

- Maintains sorted order for best price retrieval
- Enables O(log P) insertion where P is number of levels (not orders)
- Acceptable trade-off for current scale

### Why Double for Prices?

- Simplifies simulation and testing
- Production systems should use fixed-point integer ticks (int64_t)
- Avoids floating-point precision issues

### Why FOK Pre-Scan Instead of Rollback?

- Simpler implementation with no state mutation on failure
- Standard approach in production engines
- Approximately 2× traversal cost is acceptable for correctness

### Why Store All Trades?

- Simplifies testing and audit trails
- Production systems should stream trades externally
- Memory can be managed via periodic truncation

---

## Future Enhancements

Optimizations intentionally deferred to preserve correctness:

- [ ] Replace `std::map` with cache-friendly price level structure
- [ ] Memory pooling for order allocation
- [ ] Integer-based tick pricing (fixed-point arithmetic)
- [ ] Trade streaming instead of in-memory storage
- [ ] Cache best bid/ask iterators to eliminate map lookups
- [ ] Lock-free data structures for multi-threaded matching

---

## Author

[Shubh Garg](https://github.com/shubh-garg18)
