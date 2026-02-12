/*
 * rtos_time.h - Time Management and Simulation
 *
 * Provides the system tick handler, periodic task release, deadline
 * checking, and a workload simulation function.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#ifndef RTOS_TIME_H
#define RTOS_TIME_H

#include "scheduler.h"
#include <stdint.h>

/* ── Public API ───────────────────────────────────────────────────── */

/** Process one system tick: update counters, releases, deadlines. */
void tick_handler(Scheduler *sched);

/** Check and release periodic tasks whose period boundary is reached. */
void check_periodic_releases(Scheduler *sched);

/** Detect and log deadline overruns for running/ready tasks. */
void check_deadlines(Scheduler *sched);

/** Advance time by `ticks` calls to tick_handler. */
void advance_time(Scheduler *sched, uint64_t ticks);

/**
 * Simulate a task doing `work_ticks` of computation.
 * Yields on preemption and resumes later.
 */
void simulate_work(Scheduler *sched, TaskControlBlock *task,
                   uint64_t work_ticks);

#endif /* RTOS_TIME_H */
