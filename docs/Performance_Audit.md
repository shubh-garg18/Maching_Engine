# Matching Engine Performance & Complexity Audit

## Overview

This document analyzes the computational complexity, memory behavior, and scalability characteristics of the matching engine core.

The goal of this audit is to:

- Prove correctness of performance assumptions
- Identify hot paths and bottlenecks
- Establish guarantees for latency-sensitive environments
- Provide a foundation for future optimization work

This analysis focuses on the **engine core**, not I/O or persistence layers.

---

## C.1 Time Complexity (Per Operation)

### 1. Limit Order Insertion (Non-Crossing)

#### Execution Path

process_limit_order
→ matching_loop (0 iterations)
→ insert_limit

#### Cost Breakdown

- `std::map` price level lookup/insert: **O(log P)**
- FIFO append within level: **O(1)**

#### Total Complexity

O(log P)

Where:

- `P` = Number of active price levels (NOT number of orders)

#### Analysis

This is the optimal complexity for an ordered limit order book.

---

### 2. Matching Loop (Aggressive Orders)

#### Execution Path for Matching

matching_loop
→ iterate crossed price levels
→ consume FIFO orders within each level

Let:

- `L` = Number of crossed price levels
- `K` = Number of resting orders consumed

#### Cost

O(L + K)

#### Key Properties

- No scanning of irrelevant levels
- FIFO removal is O(1)
- No heap allocation inside the matching loop

#### Analysis of Matching Loop

This is the theoretical lower bound for a price-time priority matching engine.

---

### 3. Market / IOC / FOK Orders

| Order Type | Extra Cost       | Total Complexity |
| ---------- | ---------------- | ---------------- |
| MARKET     | None             | O(L + K)         |
| IOC        | None             | O(L + K)         |
| FOK        | Pre-scan + match | O(L + K)         |

#### Note on FOK Orders

FOK performs a full liquidity pre-check, resulting in approximately 2× traversal, but remains linear.

This design avoids rollback complexity and is standard in production engines.

---

### 4. Cancel Order

#### Execution Path for Cancel

unordered_map lookup
→ FIFO unlink
→ price level cleanup

#### Complexity of Cancel

O(1)

#### Analysis of Cancel Operation

Constant-time cancellation is achieved via order ID indexing.

---

### 5. Best Bid / Offer (BBO)

#### Operation

return best_bid / best_ask

#### Complexity of BBO

O(1)

---

### 6. L2 Snapshot (Depth = D)

#### Complexity of L2 Snapshot

O(D)

Where:

- `D` = Requested depth

---

## C.2 Hot Path Walk-Through

The most frequently executed path is:

matching_loop →
get_best_opposite →
cross check →
head order retrieval →
fill execution →
removal if fully filled

### Memory Access Pattern

- PriceLevel pointers remain stable
- FIFO list traversal is sequential (cache-friendly)
- No vector growth inside matching loop
- Trade append is amortized O(1)

#### Hot Path Conclusion

The matching loop is designed for strong cache locality and minimal branching.

---

## C.3 Hidden Costs & Edge Cases

### 1. `std::map` vs `unordered_map` for Price Levels

Current structure:

std::map<double, PriceLevel*>

#### Advantages

- Maintains sorted price ordering
- Enables efficient best price retrieval

#### Disadvantages

- Tree-based pointer chasing
- Reduced cache locality

#### Assessment of Price Level Storage

Acceptable for current scale. Likely first scalability bottleneck at very high throughput.

---

### 2. Double-Based Price Keys

Potential issues:

- Floating-point precision drift
- Comparison instability

#### Assessment of Double Keys

Acceptable for simulation.

#### Production Recommendation for Pricing

Use fixed-point integer ticks (`int64_t`).

---

### 3. FOK Pre-Scan

FOK performs duplicate traversal intentionally.

Alternative rollback-based designs introduce:

- Higher complexity
- State mutation risk

Current approach is correct and safe.

---

### 4. Trade Vector Growth

std::vector\<Trade\> trades

Potential issue:

- Unbounded memory growth during long sessions

#### Future Production Approach for Trades

- Stream trades externally
- Persist and truncate in memory

---

## C.4 Performance-Critical Invariants

The following invariants guarantee engine efficiency:

- No empty price levels
- FIFO ordering via intrusive pointers
- No rebalancing during matching
- No allocations inside `matching_loop`
- No book mutation on FOK failure
- Market data structures are read-only during matching

These constraints minimize latency variance.

---

## C.5 Complexity Summary Table

| Operation          | Complexity |
| ------------------ | ---------- |
| Limit Insert       | O(log P)   |
| Market / IOC Match | O(L + K)   |
| FOK                | O(L + K)   |
| Cancel             | O(1)       |
| BBO                | O(1)       |
| L2 Snapshot        | O(D)       |

Where:

- `P` = Number of price levels
- `L` = Crossed levels
- `K` = Matched resting orders
- `D` = Requested depth

---

## Future Optimization Opportunities

(Not implemented yet to preserve correctness and simplicity)

- Replace `std::map` with cache-friendly structure
- Introduce memory pooling for orders
- Convert floating price representation to integer ticks
- Stream trades instead of storing indefinitely
- Cache best bid/ask iterators

These are intentionally deferred until empirical benchmarking is completed.
