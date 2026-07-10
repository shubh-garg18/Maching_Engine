# UML Diagrams

Mermaid diagrams for the matching engine. They mirror the code in `include/` and
`src/` — class structure, the order lifecycle, order dispatch, and the runtime
flows for matching, stop triggering, and async submission.

For a single high-level overview, see the diagram in the
[README](../README.md#architecture-at-a-glance). This file holds the detailed set.

## Contents

- [Class Diagram — Core Matching](#class-diagram--core-matching)
- [Class Diagram — Fees & Publishing](#class-diagram--fees--publishing)
- [Class Diagram — Async & Events](#class-diagram--async--events)
- [Class Diagram — Market Data](#class-diagram--market-data)
- [Enumerations](#enumerations)
- [State Diagram — Order Lifecycle](#state-diagram--order-lifecycle)
- [Flowchart — Order Dispatch](#flowchart--order-dispatch)
- [Sequence — Aggressive Order Matching](#sequence--aggressive-order-matching)
- [Sequence — Stop-Order Triggering](#sequence--stop-order-triggering)
- [Sequence — Async Event Queue](#sequence--async-event-queue)

---

## Class Diagram — Core Matching

`MatchingEngine` owns references to one `OrderBook` and one `FeeCalculator`,
runs the shared matching loop, and records every fill as a `Trade`. `OrderBook`
owns its `PriceLevel`s (heap-allocated, deleted on emptying); each `PriceLevel`
holds an intrusive FIFO list of `Order`s. `OrderBook` also references orders via
its id index and its `pending_stops` list, but does not own them.

```mermaid
classDiagram
    class MatchingEngine {
        +OrderBook& order_book
        +FeeCalculator& fees_calculator
        +double last_trade_price
        +vector~Trade~ trades
        +TradePublisher* trade_publisher
        +bool running
        +process_order(Order*)
        +matching_loop(Order*)
        +process_limit_order(Order*)
        +process_market_order(Order*)
        +process_ioc_order(Order*)
        +process_fok_order(Order*)
        +process_stop_order(Order*)
        +check_stop_orders()
        +run(EventQueue&)
        +process_event(EngineEvent)
        +cross(Order*, PriceLevel*) bool
        -generate_trades(qty, incoming, resting) Trade
    }

    class OrderBook {
        +PriceLevel* best_bid
        +PriceLevel* best_ask
        +map~double, PriceLevel*~ bids
        +map~double, PriceLevel*~ asks
        +unordered_map~string, Order*~ orders
        +vector~Order*~ pending_stops
        +insert_limit(Order*)
        +cancel_order(id) bool
        +get_best_opposite(side) PriceLevel*
        +remove_price_level(side, level)
        +can_fully_fill(Order*) bool
        +get_bbo() BBO
        +get_l2_snapshot(depth) L2Snapshot
    }

    class PriceLevel {
        +double price
        +uint64_t total_quantity
        +uint64_t order_count
        +Order* head
        +Order* tail
        +add_order(Order*)
        +remove_order(Order*)
        +reduce_quantity(qty)
        +get_head_order() Order*
        +is_empty() bool
    }

    class Order {
        +string user_id
        +string order_id
        +Side side
        +OrderType type
        +double price
        +uint64_t original_quantity
        +uint64_t filled_quantity
        +double stop_price
        +bool is_triggered
        +OrderStatus status
        +Timestamp timestamp_ns
        +Timestamp wall_timestamp_ns
        +Order* next
        +Order* prev
        +PriceLevel* price_level
        +remaining_quantity() uint64_t
        +fill_quantity(qty)
        +is_filled() bool
    }

    class Trade {
        +string user_id
        +string buy_order_id
        +string sell_order_id
        +double price
        +uint64_t quantity
        +Timestamp engine_ts
        +Timestamp wall_ts
        +double maker_fee
        +double taker_fee
    }

    MatchingEngine o-- OrderBook : reference
    MatchingEngine o-- FeeCalculator : reference
    MatchingEngine *-- Trade : records
    MatchingEngine ..> TradePublisher : publishes
    OrderBook *-- PriceLevel : owns
    OrderBook o-- Order : id index + pending stops
    PriceLevel o-- Order : intrusive FIFO
    Order ..> PriceLevel : back-reference
```

---

## Class Diagram — Fees & Publishing

`FeeCalculator` tracks per-user rolling volume and selects a `FeeTier` at trade
time (maker rebates kick in at higher tiers). Fills are emitted as immutable
`TradeEvent`s through the `TradePublisher` interface; `InMemoryTradePublisher`
is the test collector.

```mermaid
classDiagram
    class FeeCalculator {
        +vector~FeeTier~ tiers
        +unordered_map~string, UserFeeState~ users
        +maker_fee(user_id, price, qty) double
        +taker_fee(user_id, price, qty) double
        +update_volume(user_id, notional)
        +tier_for(user_id) FeeTier&
    }

    class FeeTier {
        +double min_volume
        +double maker_fee_rate
        +double taker_fee_rate
    }

    class UserFeeState {
        +double rolling_volume
        +size_t tier_index
    }

    class TradeEvent {
        +string user_id
        +string buy_order_id
        +string sell_order_id
        +double price
        +uint64_t quantity
        +Timestamp engine_ts
        +Timestamp wall_ts
        +double maker_fee
        +double taker_fee
    }

    class TradePublisher {
        <<interface>>
        +publish(TradeEvent)*
    }

    class InMemoryTradePublisher {
        +vector~TradeEvent~ events
        +publish(TradeEvent)
    }

    FeeCalculator *-- FeeTier : tier table
    FeeCalculator *-- UserFeeState : per user
    TradePublisher <|-- InMemoryTradePublisher : realizes
    InMemoryTradePublisher *-- TradeEvent : collects
    TradePublisher ..> TradeEvent : publishes
```

---

## Class Diagram — Async & Events

The async layer is a single-consumer command queue. Producers push
`EngineEvent`s; the worker thread running `MatchingEngine::run` blocks on `pop`
until an event arrives.

```mermaid
classDiagram
    class EventQueue {
        -queue~EngineEvent~ q
        -mutex mtx
        -condition_variable cv
        +push(EngineEvent)
        +pop(EngineEvent&) bool
    }

    class EngineEvent {
        +EventType type
        +Order* order
        +string order_id
        +New(Order*) EngineEvent
        +Cancel(id) EngineEvent
        +Stop() EngineEvent
    }

    EventQueue o-- EngineEvent : buffers
    EngineEvent ..> Order : references
    MatchingEngine ..> EventQueue : run() consumes
    MatchingEngine ..> EngineEvent : process_event
```

---

## Class Diagram — Market Data

Snapshot value types produced by `OrderBook`. `BBO` is an O(1) read off the
cached best pointers; `L2Snapshot` walks up to `depth` levels per side.

```mermaid
classDiagram
    class BBO {
        +bool has_bid
        +double bid_price
        +uint64_t bid_quantity
        +bool has_ask
        +double ask_price
        +uint64_t ask_quantity
    }

    class L2Snapshot {
        +vector~L2Level~ bids
        +vector~L2Level~ asks
    }

    class L2Level {
        +double price
        +uint64_t quantity
    }

    L2Snapshot *-- L2Level : depth levels
    OrderBook ..> BBO : get_bbo()
    OrderBook ..> L2Snapshot : get_l2_snapshot()
```

---

## Enumerations

```mermaid
classDiagram
    class Side {
        <<enumeration>>
        BUY
        SELL
    }
    class OrderType {
        <<enumeration>>
        LIMIT
        MARKET
        IOC
        FOK
        STOP_LOSS
        STOP_LIMIT
    }
    class OrderStatus {
        <<enumeration>>
        CREATED
        OPEN
        PARTIALLY_FILLED
        COMPLETED
        CANCELLED
    }
    class EventType {
        <<enumeration>>
        NEW_ORDER
        CANCEL_ORDER
        STOP
    }
```

---

## State Diagram — Order Lifecycle

`OrderStatus` transitions. An order starts `CREATED`; where it lands depends on
its type and available liquidity. `COMPLETED` and `CANCELLED` are terminal.

```mermaid
stateDiagram-v2
    [*] --> CREATED
    CREATED --> OPEN : limit rests (no fill) / stop queued
    CREATED --> PARTIALLY_FILLED : partial fill on arrival (limit rests)
    CREATED --> COMPLETED : full fill on arrival
    CREATED --> CANCELLED : no liquidity (market/IOC) / FOK pre-scan fails
    OPEN --> PARTIALLY_FILLED : resting order partially filled
    OPEN --> COMPLETED : resting order fully filled
    PARTIALLY_FILLED --> COMPLETED : remainder filled
    OPEN --> CANCELLED : explicit cancel
    PARTIALLY_FILLED --> CANCELLED : explicit cancel
    COMPLETED --> [*]
    CANCELLED --> [*]
```

> Note: a pending (untriggered) stop sits in `OPEN` and has no cancel path yet —
> see the known limitation in [OrderTypes.md](OrderTypes.md).

---

## Flowchart — Order Dispatch

`process_order` routes by `OrderType`. LIMIT / MARKET / IOC go straight into the
matching loop; FOK pre-scans first; stops are parked in `pending_stops`.

```mermaid
flowchart TD
    A[process_order] --> B{OrderType}
    B -->|LIMIT| L[process_limit_order]
    B -->|MARKET| M[process_market_order]
    B -->|IOC| I[process_ioc_order]
    B -->|FOK| K[process_fok_order]
    B -->|STOP_LOSS / STOP_LIMIT| S[process_stop_order]

    K --> PS{can_fully_fill?}
    PS -->|no| KC[status = CANCELLED, book untouched]
    PS -->|yes| ML

    L --> ML[matching_loop]
    M --> ML
    I --> ML

    ML --> RQ{remainder > 0?}
    RQ -->|LIMIT| INS[insert_limit -> rest on book]
    RQ -->|MARKET / IOC| DROP[drop remainder<br/>PARTIALLY_FILLED or CANCELLED]
    RQ -->|fully filled| DONE[status = COMPLETED]

    S --> SP[push to pending_stops<br/>status = OPEN]
```

---

## Sequence — Aggressive Order Matching

The hot path for a crossing order (e.g. a marketable limit buy). Each iteration
fills against the head of the best opposite level, prices the fill, publishes it,
and removes emptied levels. Stops are checked once, after the loop.

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant E as MatchingEngine
    participant B as OrderBook
    participant L as PriceLevel
    participant F as FeeCalculator
    participant P as TradePublisher

    C->>E: process_order(order)
    E->>E: dispatch by type

    loop while remaining_qty > 0
        E->>B: get_best_opposite(side)
        B-->>E: best PriceLevel (or null)
        alt no level, or price does not cross
            Note over E: break
        else crosses
            E->>L: get_head_order()
            L-->>E: resting order
            E->>E: fill_quantity() on both sides
            E->>L: reduce_quantity(trade_qty)
            E->>F: update_volume() + maker_fee/taker_fee
            F-->>E: fees
            E->>P: publish(TradeEvent)
            E->>E: trades.push_back(), last_trade_price = price
            opt resting fully filled
                E->>L: remove_order(resting)
                opt level now empty
                    E->>B: remove_price_level() -> refresh BBO
                end
            end
        end
    end

    opt at least one fill (any_trade)
        E->>E: check_stop_orders()
    end
    opt LIMIT with unfilled remainder
        E->>B: insert_limit(order) -> rests
    end
```

---

## Sequence — Stop-Order Triggering

`check_stop_orders` runs once at the tail of a matching pass. It marks stops
whose trigger condition is met against the latest `last_trade_price`, then
converts each to a MARKET (STOP_LOSS) or LIMIT (STOP_LIMIT) order — which
re-enters the matching loop and may itself trigger further stops (cascade).

```mermaid
sequenceDiagram
    autonumber
    participant E as MatchingEngine
    participant B as OrderBook

    Note over E: entered from matching_loop when any_trade == true
    E->>B: read pending_stops
    loop each untriggered stop
        alt BUY and last_trade_price >= stop_price
            E->>E: mark is_triggered
        else SELL and last_trade_price <= stop_price
            E->>E: mark is_triggered
        end
    end

    loop each triggered stop (FIFO)
        E->>B: erase from pending_stops
        alt STOP_LOSS
            E->>E: type = MARKET -> process_market_order()
        else STOP_LIMIT
            E->>E: type = LIMIT -> process_limit_order()
        end
        Note over E,B: re-enters matching_loop, may cascade
    end
```

---

## Sequence — Async Event Queue

Producers submit `EngineEvent`s from any thread; a single worker thread drains
them. `pop` blocks on a condition variable until an event is available. A `STOP`
event ends the loop.

```mermaid
sequenceDiagram
    autonumber
    participant Prod as Producer thread
    participant Q as EventQueue
    participant W as Worker thread (engine.run)
    participant E as MatchingEngine

    W->>Q: pop(event)
    Note over W,Q: blocks on condition_variable while empty
    Prod->>Q: push(EngineEvent::New / Cancel / Stop)
    Q-->>W: event (notify_one wakes worker)
    W->>E: process_event(event)
    alt NEW_ORDER
        E->>E: process_order(order)
    else CANCEL_ORDER
        E->>E: order_book.cancel_order(id)
    else STOP
        E->>W: running = false -> loop exits
    end
```
