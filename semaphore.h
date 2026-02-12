/*
 * semaphore.h - Counting Semaphore
 *
 * Classic P/V counting semaphore for signaling and counting.
 * Does NOT use priority inheritance (that's for mutexes only).
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "task.h"

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct Scheduler Scheduler;

/* ── Constants ────────────────────────────────────────────────────── */
#define SEM_NAME_MAX        32
#define SEM_WAIT_QUEUE_CAP  16

/* ── Semaphore structure ──────────────────────────────────────────── */
typedef struct Semaphore {
    int               count;
    int               max_count;

    TaskControlBlock *wait_queue[SEM_WAIT_QUEUE_CAP];
    int               wait_count;

    char              name[SEM_NAME_MAX];
    Scheduler        *scheduler;
} Semaphore;

/* ── Public API ───────────────────────────────────────────────────── */

/** Create a semaphore with initial and maximum count. */
Semaphore *semaphore_create(Scheduler *sched, const char *name,
                            int initial, int max_count);

/** Destroy a semaphore. */
void semaphore_destroy(Semaphore *sem);

/** Wait (P operation). Blocks if count == 0. */
void semaphore_wait(Semaphore *sem, TaskControlBlock *task);

/** Signal (V operation). Wakes one waiter or increments count. */
void semaphore_signal(Semaphore *sem, TaskControlBlock *task);

#endif /* SEMAPHORE_H */
