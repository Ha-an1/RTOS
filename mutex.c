/*
 * mutex.c - Mutex with Priority Inheritance Protocol
 *
 * Implements mutual exclusion with optional priority inheritance to
 * solve priority inversion. Supports transitive inheritance chains.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "mutex.h"
#include "scheduler.h"
#include "timeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Wait-queue helpers (priority-ordered) ────────────────────────── */

static void wait_queue_insert(Mutex *mtx, TaskControlBlock *task)
{
    if (mtx->wait_count >= MUTEX_WAIT_QUEUE_CAP) {
        fprintf(stderr, "mutex wait queue full for %s\n", mtx->name);
        return;
    }

    /* Insert in priority order (lowest number = highest priority first) */
    int pos = mtx->wait_count;
    for (int i = 0; i < mtx->wait_count; i++) {
        if (task->priority < mtx->wait_queue[i]->priority) {
            pos = i;
            break;
        }
    }
    for (int i = mtx->wait_count; i > pos; i--) {
        mtx->wait_queue[i] = mtx->wait_queue[i - 1];
    }
    mtx->wait_queue[pos] = task;
    mtx->wait_count++;
}

static TaskControlBlock *wait_queue_pop(Mutex *mtx)
{
    if (mtx->wait_count == 0) return NULL;
    TaskControlBlock *task = mtx->wait_queue[0];
    for (int i = 0; i < mtx->wait_count - 1; i++) {
        mtx->wait_queue[i] = mtx->wait_queue[i + 1];
    }
    mtx->wait_count--;
    return task;
}

/* ── Creation / Destruction ───────────────────────────────────────── */

Mutex *mutex_create(Scheduler *sched, const char *name)
{
    Mutex *mtx = calloc(1, sizeof(Mutex));
    if (!mtx) return NULL;

    mtx->locked     = false;
    mtx->owner      = NULL;
    mtx->wait_count = 0;
    mtx->scheduler  = sched;
    snprintf(mtx->name, MUTEX_NAME_MAX, "%s", name);
    return mtx;
}

void mutex_destroy(Mutex *mtx)
{
    if (!mtx) return;
    /* Force-release if still locked (test cleanup) */
    if (mtx->locked && mtx->owner) {
        task_remove_held_mutex(mtx->owner, mtx);
        mtx->locked = false;
        mtx->owner  = NULL;
    }
    free(mtx);
}

/* ── Priority Inheritance ─────────────────────────────────────────── */

void priority_inherit(TaskControlBlock *task, int new_priority)
{
    if (!task) return;

    /* Only boost if new_priority is strictly higher (lower number) */
    if (new_priority >= task->priority) return;

    Scheduler *sched = task->scheduler;
    int old_priority = task->priority;

    /* Save original if not already inherited */
    if (!task->priority_inherited) {
        task->original_priority = task->priority;
        task->priority_inherited = true;
    }

    task->priority = new_priority;
    task->priority_boosts++;

    /* Log */
    if (sched && sched->timeline) {
        char buf[ANNOTATION_MAX];
        snprintf(buf, sizeof(buf),
                 "%s priority boosted: P%d -> P%d (inherited)",
                 task->name, old_priority, new_priority);
        timeline_record(sched->timeline, sched->system_ticks,
                        task, VIS_NONE, buf);
    }

    /* Re-sort if in ready queue */
    if (task->state == TASK_READY && sched) {
        ready_queue_remove(sched, task);
        ready_queue_insert(sched, task);
    }

    /* TRANSITIVE INHERITANCE:
       If this task is blocked on another mutex, propagate
       the boost to that mutex's owner. */
    if (task->blocked_on && task->blocked_on->owner) {
        priority_inherit(task->blocked_on->owner, new_priority);
    }
}

void priority_restore(TaskControlBlock *task)
{
    if (!task || !task->priority_inherited) return;

    Scheduler *sched = task->scheduler;
    int old_priority = task->priority;

    /* Calculate highest priority needed from remaining held mutexes */
    int needed = task->original_priority;
    for (int i = 0; i < task->held_mutex_count; i++) {
        Mutex *m = task->held_mutexes[i];
        if (!m) continue;
        for (int w = 0; w < m->wait_count; w++) {
            if (m->wait_queue[w]->priority < needed) {
                needed = m->wait_queue[w]->priority;
            }
        }
    }

    task->priority = needed;
    if (task->priority == task->original_priority) {
        task->priority_inherited = false;
    }

    /* Log restoration */
    if (sched && sched->timeline) {
        timeline_record_priority_restore(sched->timeline,
                                         sched->system_ticks,
                                         task, old_priority,
                                         task->priority);
    }

    /* Re-sort if in ready queue */
    if (task->state == TASK_READY && sched) {
        ready_queue_remove(sched, task);
        ready_queue_insert(sched, task);
    }
}

/* ── Lock / Unlock ────────────────────────────────────────────────── */

void mutex_lock(Mutex *mtx, TaskControlBlock *task)
{
    if (!mtx || !task) return;
    Scheduler *sched = mtx->scheduler;

    if (!mtx->locked) {
        /* Acquire immediately */
        mtx->locked = true;
        mtx->owner  = task;
        task_add_held_mutex(task, mtx);

        if (sched && sched->timeline) {
            timeline_record_mutex_op(sched->timeline,
                                     sched->system_ticks,
                                     task, mtx, "locks");
        }
        return;
    }

    /* Already locked — contention */
    if (sched && sched->timeline) {
        char buf[ANNOTATION_MAX];
        snprintf(buf, sizeof(buf),
                 "%s tries to lock %s (blocked by %s)",
                 task->name, mtx->name, mtx->owner->name);
        timeline_record(sched->timeline, sched->system_ticks,
                        task, VIS_NONE, buf);
    }

    /* Priority inheritance: boost owner if requester has higher pri */
    if (sched && sched->priority_inheritance_enabled) {
        if (task->priority < mtx->owner->priority) {
            if (sched->timeline) {
                timeline_record_priority_inherit(sched->timeline,
                                                 sched->system_ticks,
                                                 mtx->owner, task, mtx);
            }
            priority_inherit(mtx->owner, task->priority);
        }
    }

    /* Block the requesting task */
    task->blocked_on = mtx;
    task_set_state(task, TASK_BLOCKED);
    wait_queue_insert(mtx, task);

    /* Reschedule */
    scheduler_schedule(sched);
}

void mutex_unlock(Mutex *mtx, TaskControlBlock *task)
{
    if (!mtx || !task) return;

    Scheduler *sched = mtx->scheduler;

    if (mtx->owner != task) {
        fprintf(stderr, "mutex_unlock: %s is not owner of %s\n",
                task->name, mtx->name);
        return;
    }

    /* Record event */
    if (sched && sched->timeline) {
        timeline_record_mutex_op(sched->timeline,
                                 sched->system_ticks,
                                 task, mtx, "unlocks");
    }

    /* Remove from held list */
    task_remove_held_mutex(task, mtx);

    /* Restore priority BEFORE handing off the mutex */
    if (sched && sched->priority_inheritance_enabled) {
        priority_restore(task);
    }

    /* Wake highest-priority waiter */
    if (mtx->wait_count > 0) {
        TaskControlBlock *waiter = wait_queue_pop(mtx);
        waiter->blocked_on = NULL;

        /* Transfer ownership */
        mtx->owner = waiter;
        task_add_held_mutex(waiter, mtx);

        /* Waiter becomes READY */
        task_set_state(waiter, TASK_READY);

        if (sched && sched->timeline) {
            char buf[ANNOTATION_MAX];
            snprintf(buf, sizeof(buf),
                     "%s acquires %s (was waiting)",
                     waiter->name, mtx->name);
            timeline_record(sched->timeline, sched->system_ticks,
                            waiter, VIS_NONE, buf);
        }
    } else {
        mtx->locked = false;
        mtx->owner  = NULL;
    }

    /* Reschedule — newly woken task may preempt */
    scheduler_schedule(sched);
}
