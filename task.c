/*
 * task.c - Task Management Implementation
 *
 * Creates, destroys, and manipulates Task Control Blocks (TCBs).
 * Handles state transitions and resource tracking for mutexes.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "task.h"
#include "scheduler.h"
#include "timeline.h"
#include "mutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────── */

static VisualState state_to_vis(TaskState s)
{
    switch (s) {
        case TASK_RUNNING:    return VIS_RUNNING;
        case TASK_READY:      return VIS_READY;
        case TASK_BLOCKED:    return VIS_BLOCKED;
        case TASK_SUSPENDED:  return VIS_SUSPENDED;
        case TASK_TERMINATED: return VIS_SUSPENDED;
        default:              return VIS_NONE;
    }
}

/* ── Task Creation ────────────────────────────────────────────────── */

TaskControlBlock *task_create(Scheduler *sched,
                              const char *name,
                              TaskFunc    func,
                              void       *arg,
                              int         priority,
                              uint64_t    period,
                              uint64_t    deadline,
                              uint64_t    wcet)
{
    if (!sched || sched->task_count >= MAX_ALL_TASKS) {
        fprintf(stderr, "task_create: scheduler full or NULL\n");
        return NULL;
    }

    TaskControlBlock *task = calloc(1, sizeof(TaskControlBlock));
    if (!task) {
        fprintf(stderr, "task_create: out of memory\n");
        return NULL;
    }

    /* Identity */
    task->id = sched->next_id++;
    snprintf(task->name, TASK_NAME_MAX, "%s", name);
    task->state = TASK_READY;

    /* Execution context */
    task->func = func;
    task->arg  = arg;
    task->stack = NULL;     /* Simulated — no real stack allocation */
    task->stack_size = 0;

    /* Priority */
    task->priority          = priority;
    task->original_priority = priority;
    task->priority_inherited = false;

    /* Timing */
    task->period            = period;
    task->relative_deadline = (deadline > 0) ? deadline : period;
    task->next_release      = sched->system_ticks + period;
    task->absolute_deadline = sched->system_ticks +
                              ((deadline > 0) ? deadline : period);
    task->exec_time         = 0;
    task->wcet_observed     = 0;
    task->total_exec_time   = 0;
    task->remaining_work    = wcet;

    /* Statistics */
    task->invocations    = 1;
    task->deadline_misses = 0;
    task->preemptions    = 0;
    task->priority_boosts = 0;

    /* Resource tracking */
    task->held_mutexes    = calloc(TASK_INITIAL_MUTEX_CAP,
                                   sizeof(Mutex *));
    task->held_mutex_count = 0;
    task->held_mutex_cap   = TASK_INITIAL_MUTEX_CAP;
    task->blocked_on       = NULL;

    /* Linkage */
    task->next = NULL;
    task->prev = NULL;
    task->scheduler = sched;
    task->ready_since = sched->system_ticks;

    /* RMS auto-priority: shorter period → higher priority */
    if (sched->policy == SCHED_RATE_MONOTONIC && period > 0) {
        task->priority          = (int)period;
        task->original_priority = (int)period;
    }

    /* Register with scheduler */
    sched->all_tasks[sched->task_count++] = task;

    /* Add to ready queue */
    ready_queue_insert(sched, task);

    /* Record to timeline */
    if (sched->timeline) {
        char buf[ANNOTATION_MAX];
        snprintf(buf, sizeof(buf), "%s created (P%d)", name, task->priority);
        timeline_record(sched->timeline, sched->system_ticks,
                        task, VIS_READY, buf);
    }

    return task;
}

/* ── State Transitions ────────────────────────────────────────────── */

void task_set_state(TaskControlBlock *task, TaskState new_state)
{
    if (!task) return;
    TaskState old = task->state;
    if (old == new_state) return;

    Scheduler *sched = task->scheduler;
    task->state = new_state;

    /* Queue bookkeeping */
    if (old == TASK_READY && new_state != TASK_READY) {
        ready_queue_remove(sched, task);
    }
    if (new_state == TASK_READY && old != TASK_READY) {
        task->ready_since = sched->system_ticks;
        ready_queue_insert(sched, task);
    }

    /* Timeline */
    if (sched->timeline) {
        timeline_record_state_change(sched->timeline,
                                     sched->system_ticks,
                                     task, state_to_vis(new_state));
    }
}

void task_suspend(TaskControlBlock *task)
{
    if (!task || task->state == TASK_TERMINATED) return;
    task_set_state(task, TASK_SUSPENDED);
}

void task_resume(TaskControlBlock *task)
{
    if (!task || task->state != TASK_SUSPENDED) return;
    task_set_state(task, TASK_READY);
}

void task_terminate(TaskControlBlock *task)
{
    if (!task) return;
    task_set_state(task, TASK_TERMINATED);
}

/* ── Priority ─────────────────────────────────────────────────────── */

void task_set_priority(TaskControlBlock *task, int new_priority)
{
    if (!task) return;
    Scheduler *sched = task->scheduler;
    int old = task->priority;
    task->priority = new_priority;

    /* Re-sort ready queue if task is in it */
    if (task->state == TASK_READY) {
        ready_queue_remove(sched, task);
        ready_queue_insert(sched, task);
    }

    (void)old;
}

int task_get_priority(const TaskControlBlock *task)
{
    return task ? task->priority : PRIORITY_IDLE;
}

/* ── Resource Tracking ────────────────────────────────────────────── */

void task_add_held_mutex(TaskControlBlock *task, Mutex *m)
{
    if (!task || !m) return;

    /* Grow array if needed */
    if (task->held_mutex_count >= task->held_mutex_cap) {
        int new_cap = task->held_mutex_cap * 2;
        Mutex **tmp = realloc(task->held_mutexes,
                              (size_t)new_cap * sizeof(Mutex *));
        if (!tmp) {
            fprintf(stderr, "task_add_held_mutex: realloc failed\n");
            return;
        }
        task->held_mutexes   = tmp;
        task->held_mutex_cap = new_cap;
    }
    task->held_mutexes[task->held_mutex_count++] = m;
}

void task_remove_held_mutex(TaskControlBlock *task, Mutex *m)
{
    if (!task || !m) return;
    for (int i = 0; i < task->held_mutex_count; i++) {
        if (task->held_mutexes[i] == m) {
            /* Shift remaining entries */
            for (int j = i; j < task->held_mutex_count - 1; j++) {
                task->held_mutexes[j] = task->held_mutexes[j + 1];
            }
            task->held_mutex_count--;
            return;
        }
    }
}

/* ── Cleanup ──────────────────────────────────────────────────────── */

void task_destroy(TaskControlBlock *task)
{
    if (!task) return;
    free(task->held_mutexes);
    free(task->stack);
    /* Don't free the task itself — scheduler owns that memory */
}
