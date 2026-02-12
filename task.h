/*
 * task.h - Task Control Block and Task Management
 *
 * Defines the core TCB structure for the RTOS scheduler and provides
 * functions for creating, destroying, and manipulating tasks.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct Mutex    Mutex;
typedef struct Scheduler Scheduler;
typedef struct Timeline  Timeline;

/* ── Constants ────────────────────────────────────────────────────── */
#define TASK_NAME_MAX        32
#define TASK_INITIAL_MUTEX_CAP 4
#define PRIORITY_IDLE        255   /* Lowest possible priority        */
#define PRIORITY_HIGHEST     0     /* Numerically lowest = highest    */

/* ── Task States ──────────────────────────────────────────────────── */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_TERMINATED
} TaskState;

/* ── Task function signature ──────────────────────────────────────── */
typedef void (*TaskFunc)(void *arg);

/* ── Task Control Block ───────────────────────────────────────────── */
typedef struct TaskControlBlock {
    /* Identity */
    int              id;
    char             name[TASK_NAME_MAX];
    TaskState        state;

    /* Execution context */
    TaskFunc         func;
    void            *arg;
    uint8_t         *stack;
    size_t           stack_size;

    /* Priority (critical for priority inheritance) */
    int              priority;           /* Current effective priority */
    int              original_priority;  /* Saved when inherited       */
    bool             priority_inherited; /* True if boosted            */

    /* Timing */
    uint64_t         period;             /* 0 = aperiodic              */
    uint64_t         relative_deadline;
    uint64_t         next_release;
    uint64_t         absolute_deadline;
    uint64_t         exec_time;          /* Accumulated this period    */
    uint64_t         wcet_observed;      /* Worst-case observed        */
    uint64_t         total_exec_time;    /* Across all invocations     */

    /* Statistics */
    uint32_t         invocations;
    uint32_t         deadline_misses;
    uint32_t         preemptions;
    uint32_t         priority_boosts;

    /* Resource tracking (for priority inheritance) */
    Mutex          **held_mutexes;
    int              held_mutex_count;
    int              held_mutex_cap;
    Mutex           *blocked_on;         /* Mutex we're waiting for    */

    /* Queue linkage */
    struct TaskControlBlock *next;
    struct TaskControlBlock *prev;

    /* Back-pointer to owning scheduler */
    Scheduler       *scheduler;

    /* Simulation bookkeeping */
    uint64_t         remaining_work;     /* Ticks of work left         */
    uint64_t         ready_since;        /* Tick when last became READY*/
} TaskControlBlock;

/* ── Public API ───────────────────────────────────────────────────── */

/**
 * Create a new task and register it with the scheduler.
 * Returns NULL on failure.
 */
TaskControlBlock *task_create(Scheduler *sched,
                              const char *name,
                              TaskFunc    func,
                              void       *arg,
                              int         priority,
                              uint64_t    period,
                              uint64_t    deadline,
                              uint64_t    wcet);

/** Change a task's state and update scheduler queues accordingly. */
void task_set_state(TaskControlBlock *task, TaskState new_state);

/** Suspend a task (remove from ready queue). */
void task_suspend(TaskControlBlock *task);

/** Resume a suspended task (add back to ready queue). */
void task_resume(TaskControlBlock *task);

/** Terminate a task permanently. */
void task_terminate(TaskControlBlock *task);

/** Set a task's effective priority and re-sort queues. */
void task_set_priority(TaskControlBlock *task, int new_priority);

/** Get a task's current effective priority. */
int  task_get_priority(const TaskControlBlock *task);

/** Track a mutex held by this task. */
void task_add_held_mutex(TaskControlBlock *task, Mutex *m);

/** Remove a mutex from this task's held list. */
void task_remove_held_mutex(TaskControlBlock *task, Mutex *m);

/** Free a task's internal resources. */
void task_destroy(TaskControlBlock *task);

#endif /* TASK_H */
