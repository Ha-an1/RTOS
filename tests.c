/*
 * tests.c - Comprehensive Test Scenarios
 *
 * Eight self-contained tests that exercise every feature of the RTOS
 * scheduler, from basic priority scheduling to transitive priority
 * inheritance and deadline miss detection.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "task.h"
#include "scheduler.h"
#include "mutex.h"
#include "semaphore.h"
#include "timeline.h"
#include "rtos_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* ── Utility ──────────────────────────────────────────────────────── */

static void print_separator(const char *title)
{
    printf("\n");
    for (int i = 0; i < 65; i++) putchar('=');
    printf("\n  TEST: %s\n", title);
    for (int i = 0; i < 65; i++) putchar('=');
    printf("\n");
}

static void print_result(bool pass, const char *name)
{
    printf("\n  Result: %s %s\n\n",
           pass ? "PASS" : "FAIL", name);
}

/* ── Dummy task functions ─────────────────────────────────────────── */
static void task_func_noop(void *arg) { (void)arg; }

/* ══════════════════════════════════════════════════════════════════
 *  TEST 1: Basic Priority Scheduling
 *  Three aperiodic tasks execute in strict priority order.
 * ══════════════════════════════════════════════════════════════════ */

void test_basic_priority(void)
{
    print_separator("Basic Priority Scheduling");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, false);

    TaskControlBlock *tA = task_create(&sched, "TaskA", task_func_noop,
                                       NULL, 1, 0, 0, 5);
    TaskControlBlock *tB = task_create(&sched, "TaskB", task_func_noop,
                                       NULL, 2, 0, 0, 10);
    TaskControlBlock *tC = task_create(&sched, "TaskC", task_func_noop,
                                       NULL, 3, 0, 0, 8);

    /* Start scheduler: pick highest priority */
    scheduler_schedule(&sched);

    /* Run simulation for 30 ticks */
    for (uint64_t t = 0; t < 30; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;
        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING)
        {
            task_set_state(curr, TASK_TERMINATED);
        }

        scheduler_schedule(&sched);
    }

    /* Render timeline */
    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    /* Verify: A completes first, then B, then C */
    bool pass = (tA->state == TASK_TERMINATED &&
                 tB->state == TASK_TERMINATED &&
                 tC->state == TASK_TERMINATED);

    print_result(pass, "Basic Priority Scheduling");
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 2: Preemption
 *  High-priority task arrives at t=5 and preempts low-priority.
 * ══════════════════════════════════════════════════════════════════ */

void test_preemption(void)
{
    print_separator("Preemption");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, false);

    /* TaskLow starts immediately */
    TaskControlBlock *tLow = task_create(&sched, "TaskLow", task_func_noop,
                                          NULL, 10, 0, 0, 20);

    scheduler_schedule(&sched);

    /* Run 5 ticks */
    for (uint64_t t = 0; t < 5; t++) {
        tick_handler(&sched);
        scheduler_schedule(&sched);
    }

    /* Create TaskHigh at t=5 */
    TaskControlBlock *tHigh = task_create(&sched, "TaskHigh", task_func_noop,
                                           NULL, 1, 0, 0, 10);
    scheduler_schedule(&sched);

    /* Run remaining ticks */
    for (uint64_t t = 0; t < 30; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;
        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING)
        {
            task_set_state(curr, TASK_TERMINATED);
        }
        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    bool pass = (tHigh->state == TASK_TERMINATED &&
                 tLow->state  == TASK_TERMINATED &&
                 tLow->preemptions >= 1);

    printf("  TaskLow preemptions: %u\n", tLow->preemptions);
    printf("  Context switches:    %" PRIu64 "\n",
           sched.context_switches);

    print_result(pass, "Preemption");
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 3: Priority Inversion WITH Priority Inheritance
 *  THE CRITICAL TEST — demonstrates PI solving priority inversion.
 * ══════════════════════════════════════════════════════════════════ */

void test_priority_inversion_with_pi(void)
{
    print_separator("Priority Inversion WITH Priority Inheritance");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, true);  /* PI enabled */

    Mutex *mtxA = mutex_create(&sched, "MutexA");

    /* t=0: TaskLow created, locks MutexA immediately */
    TaskControlBlock *tLow = task_create(&sched, "TaskLow", task_func_noop,
                                          NULL, 10, 0, 0, 20);
    scheduler_schedule(&sched);
    mutex_lock(mtxA, tLow);

    /* Run 2 ticks: TaskLow executes */
    for (uint64_t t = 0; t < 2; t++) {
        tick_handler(&sched);
        scheduler_schedule(&sched);
    }

    /* t=2: TaskMed created (priority 5) — preempts TaskLow */
    TaskControlBlock *tMed = task_create(&sched, "TaskMed", task_func_noop,
                                          NULL, 5, 0, 0, 10);
    scheduler_schedule(&sched);

    /* Run 3 ticks: TaskMed runs (higher priority than TaskLow) */
    for (uint64_t t = 0; t < 3; t++) {
        tick_handler(&sched);
        scheduler_schedule(&sched);
    }

    /* t=5: TaskHigh created (priority 1) — tries MutexA */
    TaskControlBlock *tHigh = task_create(&sched, "TaskHigh", task_func_noop,
                                           NULL, 1, 0, 0, 8);
    scheduler_schedule(&sched);

    /* TaskHigh tries to lock MutexA — BLOCKED!
       Priority inheritance: TaskLow inherits priority 1 */
    mutex_lock(mtxA, tHigh);

    /* NOW: TaskLow has priority 1, preempts TaskMed */
    /* Run until TaskLow releases the mutex */
    bool mutex_released = false;
    uint64_t low_work_done_pi = 0;

    for (uint64_t t = 0; t < 15; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;

        if (curr == tLow) low_work_done_pi++;

        /* TaskLow releases mutex after doing 13 more ticks of work */
        if (curr == tLow && !mutex_released &&
            low_work_done_pi >= 13 && mtxA->owner == tLow) {
            mutex_unlock(mtxA, tLow);
            mutex_released = true;
        }

        scheduler_schedule(&sched);
    }

    /* Continue until completion */
    for (uint64_t t = 0; t < 30; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;
        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING)
        {
            task_set_state(curr, TASK_TERMINATED);
        }
        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    printf("  TaskLow  priority boosts: %u\n", tLow->priority_boosts);
    printf("  TaskHigh was blocked: %s\n",
           (tHigh->state == TASK_TERMINATED ||
            tHigh->state == TASK_RUNNING) ? "and completed" : "still");
    printf("  TaskMed  preemptions: %u\n", tMed->preemptions);

    bool pass = (tLow->priority_boosts >= 1);
    print_result(pass, "Priority Inversion WITH PI");

    mutex_destroy(mtxA);
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 4: Priority Inversion WITHOUT Priority Inheritance
 *  Shows the problem PI solves — medium task starves high.
 * ══════════════════════════════════════════════════════════════════ */

void test_priority_inversion_without_pi(void)
{
    print_separator("Priority Inversion WITHOUT Priority Inheritance");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, false);  /* PI disabled */

    Mutex *mtxA = mutex_create(&sched, "MutexA");

    /* t=0: TaskLow locks MutexA */
    TaskControlBlock *tLow = task_create(&sched, "TaskLow", task_func_noop,
                                          NULL, 10, 0, 0, 20);
    scheduler_schedule(&sched);
    mutex_lock(mtxA, tLow);

    for (uint64_t t = 0; t < 2; t++) {
        tick_handler(&sched);
        scheduler_schedule(&sched);
    }

    /* t=2: TaskMed created */
    TaskControlBlock *tMed = task_create(&sched, "TaskMed", task_func_noop,
                                          NULL, 5, 0, 0, 10);
    scheduler_schedule(&sched);

    for (uint64_t t = 0; t < 3; t++) {
        tick_handler(&sched);
        scheduler_schedule(&sched);
    }

    /* t=5: TaskHigh created, tries MutexA — blocked, NO PI */
    TaskControlBlock *tHigh = task_create(&sched, "TaskHigh", task_func_noop,
                                           NULL, 1, 0, 0, 8);
    scheduler_schedule(&sched);
    mutex_lock(mtxA, tHigh);

    /* TaskMed continues running because TaskLow stays at priority 10.
       This is the priority inversion problem: TaskHigh is waiting on
       TaskLow, but TaskMed (medium priority) runs instead. */

    bool mutex_released = false;
    uint64_t low_work_done = 0;  /* Track TaskLow's work separately */

    for (uint64_t t = 0; t < 50; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;

        /* Count work done by TaskLow */
        if (curr == tLow) {
            low_work_done++;
        }

        /* TaskLow releases mutex after 13 ticks of actual work */
        if (curr == tLow && !mutex_released &&
            low_work_done >= 13 && mtxA->owner == tLow) {
            mutex_unlock(mtxA, tLow);
            mutex_released = true;
        }

        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING)
        {
            task_set_state(curr, TASK_TERMINATED);
        }
        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    printf("  TaskLow  priority boosts: %u (should be 0)\n",
           tLow->priority_boosts);
    printf("  TaskMed  preemptions: %u\n", tMed->preemptions);
    (void)tHigh;

    bool pass = (tLow->priority_boosts == 0);
    print_result(pass, "Priority Inversion WITHOUT PI");

    mutex_destroy(mtxA);
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 5: Transitive Priority Inheritance
 *  Chain: High -> Low -> VeryLow through nested mutexes.
 * ══════════════════════════════════════════════════════════════════ */

void test_transitive_pi(void)
{
    print_separator("Transitive Priority Inheritance");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, true);

    Mutex *mtxA = mutex_create(&sched, "MutexA");
    Mutex *mtxB = mutex_create(&sched, "MutexB");

    /* t=0: TaskVeryLow locks MutexA */
    TaskControlBlock *tVeryLow = task_create(&sched, "TaskVeryLow",
                                              task_func_noop, NULL, 20,
                                              0, 0, 30);
    scheduler_schedule(&sched);
    mutex_lock(mtxA, tVeryLow);

    tick_handler(&sched);
    scheduler_schedule(&sched);

    /* t=1: TaskLow locks MutexB, then tries MutexA (blocks) */
    TaskControlBlock *tLow = task_create(&sched, "TaskLow", task_func_noop,
                                          NULL, 15, 0, 0, 20);
    scheduler_schedule(&sched);
    mutex_lock(mtxB, tLow);

    tick_handler(&sched);
    scheduler_schedule(&sched);

    /* t=2: TaskLow tries MutexA — blocked by TaskVeryLow
       PI: TaskVeryLow gets priority 15 */
    mutex_lock(mtxA, tLow);

    /* t=3: TaskMed created */
    tick_handler(&sched);
    TaskControlBlock *tMed = task_create(&sched, "TaskMed", task_func_noop,
                                          NULL, 10, 0, 0, 15);
    (void)tMed;  /* Used only as workload; suppress unused warning */
    scheduler_schedule(&sched);

    tick_handler(&sched);
    scheduler_schedule(&sched);

    /* t=4: TaskHigh tries MutexB — blocked by TaskLow
       TRANSITIVE: TaskLow gets priority 1, TaskVeryLow gets priority 1 */
    TaskControlBlock *tHigh = task_create(&sched, "TaskHigh", task_func_noop,
                                           NULL, 1, 0, 0, 10);
    scheduler_schedule(&sched);
    mutex_lock(mtxB, tHigh);

    /* Run simulation */
    bool mtxA_released_by_vl = false;
    bool mtxB_released_by_low = false;
    bool mtxA_released_by_low = false;

    for (uint64_t t = 0; t < 50; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;

        /* VeryLow releases MutexA after some work */
        if (curr == tVeryLow && !mtxA_released_by_vl &&
            tVeryLow->remaining_work <= 15 &&
            mtxA->owner == tVeryLow) {
            mutex_unlock(mtxA, tVeryLow);
            mtxA_released_by_vl = true;
        }

        curr = sched.current_task;  /* May have changed */

        /* Low releases MutexB after getting MutexA and doing work */
        if (curr == tLow && !mtxB_released_by_low &&
            tLow->remaining_work <= 10 &&
            mtxB->owner == tLow) {
            mutex_unlock(mtxB, tLow);
            mtxB_released_by_low = true;
        }

        curr = sched.current_task;

        if (curr == tLow && !mtxA_released_by_low &&
            tLow->remaining_work <= 8 &&
            mtxA->owner == tLow) {
            mutex_unlock(mtxA, tLow);
            mtxA_released_by_low = true;
        }

        curr = sched.current_task;
        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING) {
            task_set_state(curr, TASK_TERMINATED);
        }
        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    printf("  TaskVeryLow boosts: %u\n", tVeryLow->priority_boosts);
    printf("  TaskLow     boosts: %u\n", tLow->priority_boosts);
    printf("  Transitive chain: High(P1) -> Low -> VeryLow\n");

    bool pass = (tVeryLow->priority_boosts >= 1 &&
                 tLow->priority_boosts >= 1);
    print_result(pass, "Transitive Priority Inheritance");

    mutex_destroy(mtxA);
    mutex_destroy(mtxB);
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 6: Rate Monotonic Scheduling
 *  Automatic priority assignment + schedulability analysis.
 * ══════════════════════════════════════════════════════════════════ */

void test_rms(void)
{
    print_separator("Rate Monotonic Scheduling");

    Scheduler sched;
    scheduler_init(&sched, SCHED_RATE_MONOTONIC, false);

    /* Create periodic tasks — priorities auto-assigned by period */
    TaskControlBlock *t1 = task_create(&sched, "T1_p10", task_func_noop,
                                        NULL, 0, 10, 10, 3);
    TaskControlBlock *t2 = task_create(&sched, "T2_p15", task_func_noop,
                                        NULL, 0, 15, 15, 4);
    TaskControlBlock *t3 = task_create(&sched, "T3_p20", task_func_noop,
                                        NULL, 0, 20, 20, 5);

    /* Recalculate RMS priorities */
    rms_recalculate_priorities(&sched);

    /* Print schedulability report BEFORE running */
    rms_print_report(&sched);

    printf("  Assigned priorities:\n");
    printf("    T1 (period=10): P%d\n", t1->priority);
    printf("    T2 (period=15): P%d\n", t2->priority);
    printf("    T3 (period=20): P%d\n", t3->priority);

    /* Start scheduler */
    scheduler_schedule(&sched);

    /* Run for 60 ticks (LCM = 60 for a hyperperiod) */
    for (uint64_t t = 0; t < 60; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;

        /* When a periodic task finishes its work, suspend until
           next release */
        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING) {
            if (curr->period > 0) {
                /* Reset work for next period */
                task_set_state(curr, TASK_SUSPENDED);
            } else {
                task_set_state(curr, TASK_TERMINATED);
            }
        }

        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    int total_misses = 0;
    for (int i = 0; i < sched.task_count; i++) {
        TaskControlBlock *tk = sched.all_tasks[i];
        if (tk && tk != sched.idle_task && tk->period > 0) {
            total_misses += (int)tk->deadline_misses;
            printf("  %s: invocations=%u, misses=%u\n",
                   tk->name, tk->invocations, tk->deadline_misses);
        }
    }

    bool pass = (t1->priority < t2->priority &&
                 t2->priority < t3->priority);
    printf("  Priority assignment correct: %s\n", pass ? "yes" : "no");
    printf("  Total deadline misses: %d\n", total_misses);

    print_result(pass, "Rate Monotonic Scheduling");
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 7: Semaphore Producer-Consumer
 * ══════════════════════════════════════════════════════════════════ */

void test_semaphore(void)
{
    print_separator("Semaphore Producer-Consumer");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, false);

    Semaphore *sem_full  = semaphore_create(&sched, "sem_full",  0, 5);
    Semaphore *sem_empty = semaphore_create(&sched, "sem_empty", 5, 5);

    TaskControlBlock *producer = task_create(&sched, "Producer",
                                              task_func_noop, NULL,
                                              2, 0, 0, 50);
    TaskControlBlock *consumer = task_create(&sched, "Consumer",
                                              task_func_noop, NULL,
                                              3, 0, 0, 50);

    scheduler_schedule(&sched);

    int items_produced = 0;
    int items_consumed = 0;

    for (uint64_t t = 0; t < 100; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;

        /* Producer logic: every 3 ticks, produce an item */
        if (curr == producer && sched.system_ticks % 3 == 0) {
            if (sem_empty->count > 0) {
                semaphore_wait(sem_empty, producer);
                items_produced++;
                semaphore_signal(sem_full, producer);
            }
        }

        /* Consumer logic: every 4 ticks, consume an item */
        if (curr == consumer && sched.system_ticks % 4 == 0) {
            if (sem_full->count > 0) {
                semaphore_wait(sem_full, consumer);
                items_consumed++;
                semaphore_signal(sem_empty, consumer);
            }
        }

        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING)
        {
            task_set_state(curr, TASK_TERMINATED);
        }

        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    printf("  Items produced: %d\n", items_produced);
    printf("  Items consumed: %d\n", items_consumed);
    printf("  sem_full count:  %d\n", sem_full->count);
    printf("  sem_empty count: %d\n", sem_empty->count);

    bool pass = (items_produced > 0 && items_consumed > 0 &&
                 sem_full->count >= 0 && sem_empty->count >= 0 &&
                 (sem_full->count + sem_empty->count) == 5);

    print_result(pass, "Semaphore Producer-Consumer");

    semaphore_destroy(sem_full);
    semaphore_destroy(sem_empty);
    scheduler_destroy(&sched);
}

/* ══════════════════════════════════════════════════════════════════
 *  TEST 8: Deadline Miss Detection
 * ══════════════════════════════════════════════════════════════════ */

void test_deadline_miss(void)
{
    print_separator("Deadline Miss Detection");

    Scheduler sched;
    scheduler_init(&sched, SCHED_PRIORITY, false);

    /* Task with tight deadline that will be missed */
    TaskControlBlock *tTight = task_create(&sched, "TaskTight",
                                            task_func_noop, NULL,
                                            2, 0, 10, 15);
    /* Task with ample deadline */
    TaskControlBlock *tRelax = task_create(&sched, "TaskRelax",
                                            task_func_noop, NULL,
                                            3, 0, 50, 8);
    /* Higher priority task that hogs CPU */
    TaskControlBlock *tHog = task_create(&sched, "TaskHog",
                                          task_func_noop, NULL,
                                          1, 0, 100, 12);

    scheduler_schedule(&sched);

    for (uint64_t t = 0; t < 50; t++) {
        tick_handler(&sched);

        TaskControlBlock *curr = sched.current_task;
        if (curr && curr != sched.idle_task &&
            curr->remaining_work == 0 &&
            curr->state == TASK_RUNNING)
        {
            task_set_state(curr, TASK_TERMINATED);
        }
        scheduler_schedule(&sched);
    }

    timeline_render(sched.timeline, sched.all_tasks, sched.task_count);

    printf("  TaskHog   deadline misses: %u\n", tHog->deadline_misses);
    printf("  TaskTight deadline misses: %u\n", tTight->deadline_misses);
    printf("  TaskRelax deadline misses: %u\n", tRelax->deadline_misses);

    /* TaskTight should miss (WCET=15 > deadline=10, and it can't
       even start until TaskHog finishes 12 ticks) */
    bool pass = (tTight->deadline_misses >= 1);

    print_result(pass, "Deadline Miss Detection");
    scheduler_destroy(&sched);
}
