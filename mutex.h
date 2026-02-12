/*
 * mutex.h - Mutex with Priority Inheritance Protocol
 *
 * Implements mutual exclusion with optional priority inheritance
 * to solve the classic priority inversion problem.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>
#include "task.h"

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct Scheduler Scheduler;

/* ── Constants ────────────────────────────────────────────────────── */
#define MUTEX_NAME_MAX      32
#define MUTEX_WAIT_QUEUE_CAP 16

/* ── Mutex structure ──────────────────────────────────────────────── */
struct Mutex {
    bool              locked;
    TaskControlBlock *owner;

    /* Priority-ordered wait queue */
    TaskControlBlock *wait_queue[MUTEX_WAIT_QUEUE_CAP];
    int               wait_count;

    char              name[MUTEX_NAME_MAX];
    Scheduler        *scheduler;   /* Back-pointer for PI operations */
};

/* ── Public API ───────────────────────────────────────────────────── */

/** Create and initialize a mutex. */
Mutex *mutex_create(Scheduler *sched, const char *name);

/** Destroy a mutex (must be unlocked). */
void mutex_destroy(Mutex *mtx);

/** Lock the mutex. Blocks if already held. Triggers PI if enabled. */
void mutex_lock(Mutex *mtx, TaskControlBlock *task);

/** Unlock the mutex. Restores priority. Wakes highest-prio waiter. */
void mutex_unlock(Mutex *mtx, TaskControlBlock *task);

/* ── Priority Inheritance helpers ─────────────────────────────────── */

/**
 * Boost `task` to `new_priority`.
 * Handles transitive inheritance recursively.
 */
void priority_inherit(TaskControlBlock *task, int new_priority);

/**
 * Restore `task` priority after releasing a mutex.
 * Recalculates needed priority from remaining held mutexes.
 */
void priority_restore(TaskControlBlock *task);

#endif /* MUTEX_H */
