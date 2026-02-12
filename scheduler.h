/*
 * scheduler.h - Core Scheduler and Ready Queue
 *
 * Defines the scheduler state, ready queue operations, and the main
 * scheduling algorithm (priority-based preemptive scheduling).
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include <stdbool.h>

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct Timeline Timeline;

/* ── Constants ────────────────────────────────────────────────────── */
#define MAX_READY_TASKS   64
#define MAX_ALL_TASKS     64

/* ── Scheduling Policy ────────────────────────────────────────────── */
typedef enum {
    SCHED_PRIORITY,
    SCHED_RATE_MONOTONIC
} SchedPolicy;

/* ── Scheduler State ──────────────────────────────────────────────── */
struct Scheduler {
    SchedPolicy          policy;
    bool                 priority_inheritance_enabled;

    TaskControlBlock    *current_task;
    TaskControlBlock    *idle_task;

    /* Ready queue (sorted by priority, index 0 = highest) */
    TaskControlBlock    *ready_queue[MAX_READY_TASKS];
    int                  ready_count;

    /* All tasks in the system */
    TaskControlBlock    *all_tasks[MAX_ALL_TASKS];
    int                  task_count;

    /* Timing */
    uint64_t             system_ticks;
    uint64_t             context_switches;

    /* Visualization */
    Timeline            *timeline;

    /* Unique ID counter */
    int                  next_id;
};

/* ── Public API ───────────────────────────────────────────────────── */

/** Initialize a scheduler with the given policy. */
void scheduler_init(Scheduler *sched, SchedPolicy policy,
                    bool priority_inheritance_enabled);

/** Destroy scheduler and free all owned resources. */
void scheduler_destroy(Scheduler *sched);

/* ── Ready Queue ──────────────────────────────────────────────────── */

/** Insert a task into the ready queue (maintains priority order). */
void ready_queue_insert(Scheduler *sched, TaskControlBlock *task);

/** Remove a specific task from the ready queue. Returns true if found. */
bool ready_queue_remove(Scheduler *sched, TaskControlBlock *task);

/** Peek at the highest-priority task without removing it. */
TaskControlBlock *ready_queue_peek(Scheduler *sched);

/** Pop the highest-priority task from the ready queue. */
TaskControlBlock *ready_queue_pop(Scheduler *sched);

/** Check if the ready queue is empty. */
bool ready_queue_empty(const Scheduler *sched);

/* ── Core Scheduling ──────────────────────────────────────────────── */

/** Run the scheduling algorithm: pick highest-priority task, switch. */
void scheduler_schedule(Scheduler *sched);

/** Perform a context switch from `from` to `to`. */
void scheduler_context_switch(Scheduler *sched,
                              TaskControlBlock *from,
                              TaskControlBlock *to);

/** Returns true if a higher-priority task is ready than current. */
bool scheduler_needs_preemption(Scheduler *sched);

/** Get the next task to run (highest ready or idle). */
TaskControlBlock *scheduler_get_next_task(Scheduler *sched);

/* ── Accessors ────────────────────────────────────────────────────── */

TaskControlBlock  *scheduler_current_task(const Scheduler *sched);
uint64_t           scheduler_get_ticks(const Scheduler *sched);

/* ── Rate Monotonic Scheduling ────────────────────────────────────── */

/** Recalculate all task priorities based on period (short = high). */
void rms_recalculate_priorities(Scheduler *sched);

/** Calculate total CPU utilization Σ(Ci/Ti). */
double rms_utilization(const Scheduler *sched);

/** Perform Liu & Layland schedulability test. */
void rms_schedulability_test(const Scheduler *sched);

/** Print a detailed RMS analysis report. */
void rms_print_report(const Scheduler *sched);

#endif /* SCHEDULER_H */
