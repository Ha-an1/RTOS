# RTOS Task Scheduler

A priority-based preemptive Real-Time Operating System scheduler built from scratch in C, featuring **priority inheritance protocol** (the fix for the Mars Pathfinder 1997 priority inversion bug) and comprehensive **ASCII timeline visualization**.


## Features

| Feature | Description |
|---|---|
| **Priority-based preemptive scheduling** | Higher priority tasks immediately preempt lower priority |
| **Priority Inheritance Protocol** | Solves priority inversion with transitive chain support |
| **Rate Monotonic Scheduling** | Automatic priority assignment by period + Liu & Layland schedulability analysis |
| **Mutex synchronization** | With priority-ordered wait queues |
| **Counting semaphores** | Producer-consumer support |
| **Deadline tracking** | Detects and logs overruns |
| **ASCII timeline visualization** | Gantt chart + events log + analysis section |

## Build

**Requirements:** GCC (MinGW on Windows, or any C11-compatible compiler). No external dependencies — standard C library only.

```bash
# Compile
gcc -Wall -Wextra -std=c11 -O2 -o rtos_scheduler task.c scheduler.c rtos_time.c \
    mutex.c semaphore.c timeline.c tests.c main.c -lm

# Or use the Makefile
make
```

## Usage

```bash
./rtos_scheduler [scenario]
```

| Scenario | Description |
|---|---|
| `1` | Basic Priority Scheduling — 3 tasks in strict order |
| `2` | Preemption — high-priority arrival interrupts low |
| `3` | Priority Inversion **WITH** PI |
| `4` | Priority Inversion **WITHOUT** PI |
| `5` | Transitive Priority Inheritance — 3-task chain |
| `6` | Rate Monotonic Scheduling + schedulability analysis |
| `7` | Semaphore Producer-Consumer |
| `8` | Deadline Miss Detection |
| `all` | Run everything |

**Quick demo**:
```bash
./rtos_scheduler 3
```

## Timeline Visualization

Each task gets a row showing its state over time:

```
Time (ticks): 0    5    10   15   20   25   30
              |    |    |    |    |    |    |

TaskHigh   (P1 ) _____....########______________
TaskMed    (P5 ) __--------##########____________
TaskLow    (P10) ##__############________________

Legend: # = RUNNING  - = READY  . = BLOCKED  _ = SUSPENDED
```

- `#` — Task is **RUNNING** on the CPU
- `-` — Task is **READY** but preempted by a higher-priority task
- `.` — Task is **BLOCKED** waiting on a mutex/semaphore
- `_` — Task is **SUSPENDED** or not yet released

The Events Log below the chart shows mutex operations, priority inheritance triggers, deadline misses, and context switches.

## Test Scenarios

1. **Basic Priority** — Verifies strict priority ordering (P1 → P2 → P3)
2. **Preemption** — Late high-priority task interrupts running low-priority
3. **PI Demo** — TaskLow holds mutex, TaskHigh blocks, PI boosts TaskLow past TaskMed ★
4. **No PI** — Same setup without PI: TaskMed starves TaskHigh (the Mars Pathfinder bug)
5. **Transitive PI** — Chain: High → Low → VeryLow through nested mutexes
6. **RMS** — Auto-assigns priorities by period, prints schedulability analysis
7. **Semaphore** — Producer-consumer with bounded buffer
8. **Deadline Miss** — TaskLow delays TaskHigh past its deadline

## File Structure

```
task.h / task.c        — Task Control Block and lifecycle
scheduler.h / scheduler.c — Core scheduling, ready queue, RMS analysis
mutex.h / mutex.c      — Mutex with priority inheritance
semaphore.h / semaphore.c — Counting semaphore
timeline.h / timeline.c   — Event recording + ASCII rendering
rtos_time.h / rtos_time.c — Tick handler, periodic releases, deadlines
tests.c                — All 8 test scenarios
main.c                 — CLI entry point
Makefile               — Build automation
```

