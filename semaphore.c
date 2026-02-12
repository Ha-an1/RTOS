/*
 * semaphore.c - Counting Semaphore Implementation
 *
 * Classic P/V counting semaphore. Does NOT use priority inheritance.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "semaphore.h"
#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Wait-queue helpers ───────────────────────────────────────────── */

static void sem_wait_queue_insert(Semaphore *sem, TaskControlBlock *task)
{
    if (sem->wait_count >= SEM_WAIT_QUEUE_CAP) {
        fprintf(stderr, "semaphore wait queue full for %s\n", sem->name);
        return;
    }
    /* Priority-ordered insertion */
    int pos = sem->wait_count;
    for (int i = 0; i < sem->wait_count; i++) {
        if (task->priority < sem->wait_queue[i]->priority) {
            pos = i;
            break;
        }
    }
    for (int i = sem->wait_count; i > pos; i--) {
        sem->wait_queue[i] = sem->wait_queue[i - 1];
    }
    sem->wait_queue[pos] = task;
    sem->wait_count++;
}

static TaskControlBlock *sem_wait_queue_pop(Semaphore *sem)
{
    if (sem->wait_count == 0) return NULL;
    TaskControlBlock *task = sem->wait_queue[0];
    for (int i = 0; i < sem->wait_count - 1; i++) {
        sem->wait_queue[i] = sem->wait_queue[i + 1];
    }
    sem->wait_count--;
    return task;
}

/* ── Creation / Destruction ───────────────────────────────────────── */

Semaphore *semaphore_create(Scheduler *sched, const char *name,
                            int initial, int max_count)
{
    Semaphore *sem = calloc(1, sizeof(Semaphore));
    if (!sem) return NULL;

    sem->count      = initial;
    sem->max_count  = max_count;
    sem->wait_count = 0;
    sem->scheduler  = sched;
    snprintf(sem->name, SEM_NAME_MAX, "%s", name);
    return sem;
}

void semaphore_destroy(Semaphore *sem)
{
    if (!sem) return;
    free(sem);
}

/* ── P / V operations ─────────────────────────────────────────────── */

void semaphore_wait(Semaphore *sem, TaskControlBlock *task)
{
    if (!sem || !task) return;

    if (sem->count > 0) {
        sem->count--;
        return;
    }

    /* Block the task */
    task_set_state(task, TASK_BLOCKED);
    sem_wait_queue_insert(sem, task);
    scheduler_schedule(sem->scheduler);
}

void semaphore_signal(Semaphore *sem, TaskControlBlock *task)
{
    if (!sem) return;
    (void)task;   /* signaler identity not needed for semaphores */

    if (sem->wait_count > 0) {
        TaskControlBlock *waiter = sem_wait_queue_pop(sem);
        task_set_state(waiter, TASK_READY);
        scheduler_schedule(sem->scheduler);
    } else if (sem->count < sem->max_count) {
        sem->count++;
    }
}
