# RTOS Scheduler — Design Document

## Architecture Overview

```
┌──────────────────────────────────────────────────────┐
│                     main.c (CLI)                     │
├──────────────────────────────────────────────────────┤
│                   tests.c (8 Scenarios)              │
├──────────┬───────────┬───────────┬───────────────────┤
│ mutex.c  │semaphore.c│rtos_time.c│   timeline.c      │
│ (PI)     │ (P/V)     │ (ticks)   │   (rendering)     │
├──────────┴───────────┴───────────┴───────────────────┤
│              scheduler.c (ready queue, RMS)          │
├──────────────────────────────────────────────────────┤
│              task.c (TCB lifecycle)                   │
└──────────────────────────────────────────────────────┘
```

**Data flow:** Tests create tasks via `task_create()` → added to scheduler's ready queue → `scheduler_schedule()` picks highest priority → `tick_handler()` advances time → `timeline_record()` logs every state change → `timeline_render()` produces the ASCII visualization.

## Core Algorithms

### Scheduling Algorithm

Priority-based preemptive scheduling using a **sorted array** as the ready queue.

1. `scheduler_schedule()` calls `ready_queue_peek()` to get the highest-priority ready task
2. If it differs from `current_task`, calls `scheduler_context_switch()`
3. Context switch: outgoing task → READY, incoming task → RUNNING
4. Called after every event that could change scheduling: tick, task creation, mutex release, priority change

**Complexity:** O(n) insert, O(1) peek/pop where n = ready queue size.

### Priority Inheritance Protocol (Step-by-Step)

```
1. Task H tries to lock Mutex M (held by Task L)
2. If H.priority < L.priority (H is higher):
   a. Save L.original_priority if not already inherited
   b. Set L.priority = H.priority  (BOOST)
   c. Re-sort L in ready queue
   d. TRANSITIVE: if L is blocked on Mutex M2 (held by Task X):
      → Recursively call priority_inherit(X, H.priority)
3. Block H on M's wait queue
4. Reschedule (L now runs with boosted priority)
```

**Restoration on unlock:**
```
1. Task L unlocks Mutex M
2. Scan all mutexes L still holds
3. Find highest-priority waiter across those mutexes
4. Set L.priority = max(original_priority, highest_waiter_priority)
5. If no more inheritance needed, clear inherited flag
6. Wake highest-priority waiter on M → that waiter becomes new owner
7. Reschedule
```

### Rate Monotonic Priority Assignment

1. Collect all periodic tasks
2. Sort by period ascending (shortest first)
3. Assign priority = rank (0 = shortest period = highest priority)

**Schedulability test (Liu & Layland):**
- U = Σ(Cᵢ/Tᵢ) where Cᵢ = WCET, Tᵢ = period
- Bound = n × (2^(1/n) − 1)
- U ≤ bound → **guaranteed schedulable**
- bound < U ≤ 1 → possibly schedulable (simulate to verify)
- U > 1 → **not schedulable**

### Deadline Checking

Each tick: iterate all RUNNING/READY tasks. If `current_time > task.absolute_deadline` and work remains, record a deadline miss. Deadline is then set to `UINT64_MAX` to avoid re-triggering.

## Data Structure Design

### Task Control Block (TCB)
- **`held_mutexes` array**: dynamic with doubling growth. Needed for priority restoration — must scan all held mutexes to find the max waiter priority.
- **`blocked_on` pointer**: critical for transitive inheritance — lets the algorithm follow the chain of blocked tasks.
- **`remaining_work`**: simulation counter, decremented each tick while RUNNING.

### Ready Queue (Sorted Array)
- **Why array over linked list:** O(1) indexed access, good cache locality, simple implementation. The 64-task cap is adequate for an RTOS demo.
- **Why sorted over heap:** insertion is O(n, but n is small. Sorted order gives O(1) peek and maintains FIFO ordering for equal priorities.

### Timeline (Dynamic Array)
- Starts at 1024 entries, doubles when full. One entry per event (state change, mutex op, PI trigger, deadline miss).

## Design Decisions

| Decision | Rationale |
|---|---|
| Fixed-size ready queue (64) | Typical RTOS systems have tens, not thousands, of tasks |
| Priority 0 = highest | Standard RTOS convention (matches POSIX, FreeRTOS, VxWorks) |
| Separate original/effective priority | Clean PI implementation; original never changes |
| Transitive PI via recursion | Follows blocked_on chains naturally; limited depth in practice |
| No real stack/context save | This is a simulation; real RTOS would save registers |
| `simulate_work()` uses tick counting | Deterministic behavior for reproducible tests |

## Limitations

1. **Single-core only** — no SMP support
2. **No real context save** — simulated execution, not real register save/restore
3. **No priority ceiling protocol** — only priority inheritance implemented
4. **Bounded arrays** — 64 tasks max, 16 waiters per mutex
5. **Cooperative exit** — tasks "complete" by checking `remaining_work`, not real function return
