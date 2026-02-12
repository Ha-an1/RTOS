/*
 * scheduler.c - Core Scheduler and Ready Queue
 *
 * Implements priority-sorted ready queue, context switching, and
 * Rate Monotonic Scheduling analysis.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "scheduler.h"
#include "timeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

/* ── Idle task function ───────────────────────────────────────────── */

static void idle_task_func(void *arg)
{
    (void)arg;
    /* Idle loop — runs when nothing else can */
}

/* ── Initialization ───────────────────────────────────────────────── */

void scheduler_init(Scheduler *sched, SchedPolicy policy,
                    bool priority_inheritance_enabled)
{
    if (!sched) return;

    memset(sched, 0, sizeof(Scheduler));
    sched->policy = policy;
    sched->priority_inheritance_enabled = priority_inheritance_enabled;
    sched->current_task   = NULL;
    sched->idle_task      = NULL;
    sched->ready_count    = 0;
    sched->task_count     = 0;
    sched->system_ticks   = 0;
    sched->context_switches = 0;
    sched->next_id        = 0;

    /* Create timeline */
    sched->timeline = timeline_create();

    /* Create idle task (lowest priority) */
    sched->idle_task = task_create(sched, "Idle", idle_task_func,
                                   NULL, PRIORITY_IDLE, 0, 0, 0);
    if (sched->idle_task) {
        /* Remove idle from ready queue — it's special-cased */
        ready_queue_remove(sched, sched->idle_task);
        sched->idle_task->remaining_work = UINT64_MAX;
    }
}

void scheduler_destroy(Scheduler *sched)
{
    if (!sched) return;

    for (int i = 0; i < sched->task_count; i++) {
        if (sched->all_tasks[i]) {
            task_destroy(sched->all_tasks[i]);
            free(sched->all_tasks[i]);
            sched->all_tasks[i] = NULL;
        }
    }

    if (sched->timeline) {
        timeline_destroy(sched->timeline);
        sched->timeline = NULL;
    }
}

/* ── Ready Queue (priority-sorted array) ──────────────────────────── */

void ready_queue_insert(Scheduler *sched, TaskControlBlock *task)
{
    if (!sched || !task) return;
    if (sched->ready_count >= MAX_READY_TASKS) {
        fprintf(stderr, "ready_queue_insert: queue full\n");
        return;
    }

    /* Find insertion point (sorted ascending by priority number,
       i.e. index 0 = highest priority = lowest number).
       For equal priority, insert AFTER existing (FIFO tie-break). */
    int pos = sched->ready_count;
    for (int i = 0; i < sched->ready_count; i++) {
        if (task->priority < sched->ready_queue[i]->priority) {
            pos = i;
            break;
        }
    }

    /* Shift elements to make room */
    for (int i = sched->ready_count; i > pos; i--) {
        sched->ready_queue[i] = sched->ready_queue[i - 1];
    }
    sched->ready_queue[pos] = task;
    sched->ready_count++;
}

bool ready_queue_remove(Scheduler *sched, TaskControlBlock *task)
{
    if (!sched || !task) return false;

    for (int i = 0; i < sched->ready_count; i++) {
        if (sched->ready_queue[i] == task) {
            for (int j = i; j < sched->ready_count - 1; j++) {
                sched->ready_queue[j] = sched->ready_queue[j + 1];
            }
            sched->ready_count--;
            return true;
        }
    }
    return false;
}

TaskControlBlock *ready_queue_peek(Scheduler *sched)
{
    if (!sched || sched->ready_count == 0) return NULL;
    return sched->ready_queue[0];
}

TaskControlBlock *ready_queue_pop(Scheduler *sched)
{
    if (!sched || sched->ready_count == 0) return NULL;
    TaskControlBlock *task = sched->ready_queue[0];
    for (int i = 0; i < sched->ready_count - 1; i++) {
        sched->ready_queue[i] = sched->ready_queue[i + 1];
    }
    sched->ready_count--;
    return task;
}

bool ready_queue_empty(const Scheduler *sched)
{
    return (!sched || sched->ready_count == 0);
}

/* ── Core Scheduling ──────────────────────────────────────────────── */

TaskControlBlock *scheduler_get_next_task(Scheduler *sched)
{
    TaskControlBlock *next = ready_queue_peek(sched);
    return next ? next : sched->idle_task;
}

void scheduler_context_switch(Scheduler *sched,
                              TaskControlBlock *from,
                              TaskControlBlock *to)
{
    if (!sched || !to) return;
    if (from == to) return;

    /* Transition outgoing task */
    if (from && from->state == TASK_RUNNING) {
        from->state = TASK_READY;
        from->ready_since = sched->system_ticks;
        ready_queue_insert(sched, from);
        from->preemptions++;

        if (sched->timeline) {
            timeline_record_state_change(sched->timeline,
                                         sched->system_ticks,
                                         from, VIS_READY);
        }
    }

    /* Transition incoming task */
    ready_queue_remove(sched, to);
    to->state = TASK_RUNNING;
    sched->current_task = to;
    sched->context_switches++;

    if (sched->timeline) {
        timeline_record_state_change(sched->timeline,
                                     sched->system_ticks,
                                     to, VIS_RUNNING);
    }
}

void scheduler_schedule(Scheduler *sched)
{
    if (!sched) return;

    TaskControlBlock *next = scheduler_get_next_task(sched);
    TaskControlBlock *curr = sched->current_task;

    if (next == curr) return;

    /* Only preempt if next has strictly higher priority */
    if (curr && curr->state == TASK_RUNNING) {
        if (next->priority >= curr->priority) {
            return;   /* Current still wins (lower number = higher pri) */
        }
        /* Record preemption event */
        if (sched->timeline) {
            timeline_record_preemption(sched->timeline,
                                       sched->system_ticks,
                                       curr, next);
        }
    }

    scheduler_context_switch(sched, curr, next);
}

bool scheduler_needs_preemption(Scheduler *sched)
{
    if (!sched || !sched->current_task) return true;
    TaskControlBlock *next = ready_queue_peek(sched);
    if (!next) return false;
    return (next->priority < sched->current_task->priority);
}

/* ── Accessors ────────────────────────────────────────────────────── */

TaskControlBlock *scheduler_current_task(const Scheduler *sched)
{
    return sched ? sched->current_task : NULL;
}

uint64_t scheduler_get_ticks(const Scheduler *sched)
{
    return sched ? sched->system_ticks : 0;
}

/* ── Rate Monotonic Scheduling ────────────────────────────────────── */

/* Comparison for qsort: sort tasks by period ascending */
static int cmp_period(const void *a, const void *b)
{
    const TaskControlBlock *ta = *(const TaskControlBlock **)a;
    const TaskControlBlock *tb = *(const TaskControlBlock **)b;
    if (ta->period == 0 && tb->period == 0) return 0;
    if (ta->period == 0) return 1;   /* Aperiodic goes last */
    if (tb->period == 0) return -1;
    if (ta->period < tb->period) return -1;
    if (ta->period > tb->period) return 1;
    return 0;
}

void rms_recalculate_priorities(Scheduler *sched)
{
    if (!sched) return;

    /* Collect periodic tasks */
    TaskControlBlock *periodic[MAX_ALL_TASKS];
    int n = 0;
    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (t && t->period > 0 && t->state != TASK_TERMINATED &&
            t != sched->idle_task) {
            periodic[n++] = t;
        }
    }

    /* Sort by period */
    qsort(periodic, (size_t)n, sizeof(TaskControlBlock *), cmp_period);

    /* Assign priority = rank (0 = shortest period) */
    for (int i = 0; i < n; i++) {
        periodic[i]->priority          = i;
        periodic[i]->original_priority = i;
    }

    /* Re-sort ready queue */
    sched->ready_count = 0;
    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (t && t->state == TASK_READY && t != sched->idle_task) {
            ready_queue_insert(sched, t);
        }
    }
}

double rms_utilization(const Scheduler *sched)
{
    if (!sched) return 0.0;
    double u = 0.0;
    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (t && t->period > 0 && t != sched->idle_task) {
            u += (double)t->remaining_work / (double)t->period;
        }
    }
    return u;
}

void rms_schedulability_test(const Scheduler *sched)
{
    if (!sched) return;

    int n = 0;
    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (t && t->period > 0 && t != sched->idle_task) {
            n++;
        }
    }
    if (n == 0) {
        printf("  No periodic tasks to analyze.\n");
        return;
    }

    double u = rms_utilization(sched);
    double bound = (double)n * (pow(2.0, 1.0 / (double)n) - 1.0);

    printf("  Number of periodic tasks : %d\n", n);
    printf("  Total utilization (U)    : %.3f\n", u);
    printf("  RMS bound n(2^(1/n)-1)   : %.3f\n", bound);

    if (u <= bound) {
        printf("  Verdict: SCHEDULABLE (U <= bound, guaranteed)\n");
    } else if (u <= 1.0) {
        printf("  Verdict: POSSIBLY schedulable (bound < U <= 1.0)\n");
        printf("           Run simulation to verify.\n");
    } else {
        printf("  Verdict: NOT SCHEDULABLE (U > 1.0)\n");
    }
}

void rms_print_report(const Scheduler *sched)
{
    if (!sched) return;

    printf("\n");
    printf("================================================================\n");
    printf("         RATE MONOTONIC SCHEDULING ANALYSIS\n");
    printf("================================================================\n\n");

    printf("  %-15s %8s %8s %8s %10s\n",
           "Task", "Period", "WCET", "Priority", "Util");
    printf("  %-15s %8s %8s %8s %10s\n",
           "----", "------", "----", "--------", "----");

    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (t && t->period > 0 && t != sched->idle_task) {
            double util = (double)t->remaining_work / (double)t->period;
            printf("  %-15s %8" PRIu64 " %8" PRIu64 " %8d %9.3f\n",
                   t->name, t->period, t->remaining_work,
                   t->priority, util);
        }
    }

    printf("\n");
    rms_schedulability_test(sched);
    printf("\n");
}
