/*
 * rtos_time.c - Time Management and Simulation
 *
 * Implements the system tick handler, periodic task release, deadline
 * checking, and workload simulation with preemption support.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "rtos_time.h"
#include "timeline.h"

#include <stdio.h>
#include <inttypes.h>

/* ── Periodic Task Release ────────────────────────────────────────── */

void check_periodic_releases(Scheduler *sched)
{
    if (!sched) return;

    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (!t || t == sched->idle_task) continue;
        if (t->period == 0) continue;   /* Aperiodic */

        if (t->state == TASK_SUSPENDED &&
            sched->system_ticks >= t->next_release &&
            (sched->system_ticks - t->next_release) % t->period == 0 &&
            sched->system_ticks == t->next_release)
        {
            /* Release this periodic task */
            t->next_release     = sched->system_ticks + t->period;
            t->absolute_deadline = sched->system_ticks + t->relative_deadline;
            t->exec_time        = 0;
            t->invocations++;

            task_set_state(t, TASK_READY);

            if (sched->timeline) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "%s released (period=%" PRIu64 ", deadline=%" PRIu64 ")",
                         t->name, t->period, t->absolute_deadline);
                timeline_record(sched->timeline, sched->system_ticks,
                                t, VIS_NONE, buf);
            }
        }
    }
}

/* ── Deadline Checking ────────────────────────────────────────────── */

void check_deadlines(Scheduler *sched)
{
    if (!sched) return;

    for (int i = 0; i < sched->task_count; i++) {
        TaskControlBlock *t = sched->all_tasks[i];
        if (!t || t == sched->idle_task) continue;
        if (t->period == 0 && t->relative_deadline == 0) continue;

        if ((t->state == TASK_RUNNING || t->state == TASK_READY) &&
            t->absolute_deadline > 0 &&
            sched->system_ticks > t->absolute_deadline &&
            t->remaining_work > 0)
        {
            t->deadline_misses++;
            if (sched->timeline) {
                timeline_record_deadline_miss(sched->timeline,
                                              sched->system_ticks,
                                              t, t->absolute_deadline,
                                              sched->system_ticks);
            }
            /* Set deadline far in future to avoid re-triggering */
            t->absolute_deadline = UINT64_MAX;
        }
    }
}

/* ── Tick Handler ─────────────────────────────────────────────────── */

void tick_handler(Scheduler *sched)
{
    if (!sched) return;

    sched->system_ticks++;

    /* Update current task's execution counters */
    TaskControlBlock *curr = sched->current_task;
    if (curr && curr->state == TASK_RUNNING) {
        curr->exec_time++;
        curr->total_exec_time++;
        if (curr->remaining_work > 0) {
            curr->remaining_work--;
        }
        if (curr->exec_time > curr->wcet_observed) {
            curr->wcet_observed = curr->exec_time;
        }
    }

    /* Check for periodic releases */
    check_periodic_releases(sched);

    /* Check for deadline violations */
    check_deadlines(sched);
}

/* ── Time Advancement ─────────────────────────────────────────────── */

void advance_time(Scheduler *sched, uint64_t ticks)
{
    for (uint64_t i = 0; i < ticks; i++) {
        tick_handler(sched);
        scheduler_schedule(sched);
    }
}

/* ── Workload Simulation ──────────────────────────────────────────── */

void simulate_work(Scheduler *sched, TaskControlBlock *task,
                   uint64_t work_ticks)
{
    if (!sched || !task) return;

    task->remaining_work = work_ticks;

    for (uint64_t i = 0; i < work_ticks; i++) {
        /* If we were preempted, wait until we're running again */
        if (sched->current_task != task) return;

        tick_handler(sched);

        /* Check for preemption */
        if (scheduler_needs_preemption(sched)) {
            scheduler_schedule(sched);
            return;   /* We'll be resumed later by the scheduler */
        }
    }
}
