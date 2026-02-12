/*
 * timeline.c - ASCII Timeline Visualization
 *
 * Records scheduling events and renders a rich ASCII Gantt chart with
 * an events log and quantitative analysis section.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include "timeline.h"
#include "mutex.h"
#include "scheduler.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* ── Creation / Destruction ───────────────────────────────────────── */

Timeline *timeline_create(void)
{
    Timeline *tl = calloc(1, sizeof(Timeline));
    if (!tl) return NULL;

    tl->entries  = calloc(TIMELINE_INITIAL_CAP, sizeof(TimelineEntry));
    if (!tl->entries) { free(tl); return NULL; }

    tl->capacity   = TIMELINE_INITIAL_CAP;
    tl->count      = 0;
    tl->start_time = UINT64_MAX;
    tl->end_time   = 0;
    return tl;
}

void timeline_destroy(Timeline *tl)
{
    if (!tl) return;
    free(tl->entries);
    free(tl);
}

/* ── Generic recorder ─────────────────────────────────────────────── */

void timeline_record(Timeline *tl, uint64_t tick,
                     TaskControlBlock *task,
                     VisualState state,
                     const char *annotation)
{
    if (!tl) return;

    /* Grow if needed */
    if (tl->count >= tl->capacity) {
        int new_cap = tl->capacity * 2;
        TimelineEntry *tmp = realloc(tl->entries,
                                     (size_t)new_cap * sizeof(TimelineEntry));
        if (!tmp) {
            fprintf(stderr, "timeline_record: realloc failed\n");
            return;
        }
        tl->entries  = tmp;
        tl->capacity = new_cap;
    }

    TimelineEntry *e = &tl->entries[tl->count++];
    e->tick  = tick;
    e->task  = task;
    e->state = state;

    if (annotation) {
        snprintf(e->annotation, ANNOTATION_MAX, "%s", annotation);
    } else {
        e->annotation[0] = '\0';
    }

    if (tick < tl->start_time) tl->start_time = tick;
    if (tick > tl->end_time)   tl->end_time   = tick;
}

/* ── Convenience recorders ────────────────────────────────────────── */

void timeline_record_state_change(Timeline *tl, uint64_t tick,
                                  TaskControlBlock *task,
                                  VisualState state)
{
    timeline_record(tl, tick, task, state, NULL);
}

void timeline_record_priority_inherit(Timeline *tl, uint64_t tick,
                                      TaskControlBlock *low_task,
                                      TaskControlBlock *high_task,
                                      Mutex *mtx)
{
    char buf[ANNOTATION_MAX];
    snprintf(buf, sizeof(buf),
             "PRIORITY INHERITANCE: %s (P%d) inherits from %s (P%d) via %s",
             low_task->name, low_task->original_priority,
             high_task->name, high_task->priority,
             mtx->name);
    timeline_record(tl, tick, low_task, VIS_NONE, buf);
}

void timeline_record_priority_restore(Timeline *tl, uint64_t tick,
                                      TaskControlBlock *task,
                                      int old_pri, int new_pri)
{
    char buf[ANNOTATION_MAX];
    snprintf(buf, sizeof(buf),
             "PRIORITY RESTORED: %s (P%d -> P%d)",
             task->name, old_pri, new_pri);
    timeline_record(tl, tick, task, VIS_NONE, buf);
}

void timeline_record_mutex_op(Timeline *tl, uint64_t tick,
                              TaskControlBlock *task,
                              Mutex *mtx,
                              const char *action)
{
    char buf[ANNOTATION_MAX];
    snprintf(buf, sizeof(buf), "%s %s %s",
             task->name, action, mtx->name);
    timeline_record(tl, tick, task, VIS_NONE, buf);
}

void timeline_record_deadline_miss(Timeline *tl, uint64_t tick,
                                   TaskControlBlock *task,
                                   uint64_t deadline,
                                   uint64_t actual)
{
    char buf[ANNOTATION_MAX];
    snprintf(buf, sizeof(buf),
             "DEADLINE MISS: %s deadline=%" PRIu64 " actual=%" PRIu64 " late=%" PRIu64,
             task->name, deadline, actual, actual - deadline);
    timeline_record(tl, tick, task, VIS_NONE, buf);
}

void timeline_record_preemption(Timeline *tl, uint64_t tick,
                                TaskControlBlock *preempted,
                                TaskControlBlock *preemptor)
{
    char buf[ANNOTATION_MAX];
    snprintf(buf, sizeof(buf),
             "%s preempted by %s (P%d > P%d)",
             preempted->name, preemptor->name,
             preemptor->priority, preempted->priority);
    timeline_record(tl, tick, preempted, VIS_NONE, buf);
}

/* ── ASCII Rendering ──────────────────────────────────────────────── */

void timeline_render(const Timeline *tl,
                     TaskControlBlock **all_tasks,
                     int task_count)
{
    if (!tl || tl->count == 0) {
        printf("  (no timeline data)\n");
        return;
    }

    uint64_t t_start = tl->start_time;
    uint64_t t_end   = tl->end_time + 1;
    int      span    = (int)(t_end - t_start);

    if (span <= 0 || span > 500) {
        /* Clamp to avoid absurd output */
        if (span > 500) span = 500;
        if (span <= 0)  span = 1;
        t_end = t_start + (uint64_t)span;
    }

    /* ── Header ───────────────────────────────────────────────────── */
    printf("\n");
    for (int i = 0; i < 65; i++) putchar('=');
    printf("\n");
    printf("           RTOS SCHEDULER TIMELINE VISUALIZATION\n");
    for (int i = 0; i < 65; i++) putchar('=');
    printf("\n\n");

    /* ── Time axis ────────────────────────────────────────────────── */
    printf("Time (ticks): ");
    for (int t = 0; t < span; t++) {
        if (((t_start + (uint64_t)t) % 5) == 0) {
            char num[8];
            snprintf(num, sizeof(num), "%-4" PRIu64,
                     t_start + (uint64_t)t);
            int len = (int)strlen(num);
            printf("%s", num);
            t += (len - 1);
        } else {
            putchar(' ');
        }
    }
    printf("\n");

    printf("              ");
    for (int t = 0; t < span; t++) {
        if (((t_start + (uint64_t)t) % 5) == 0) {
            putchar('|');
        } else {
            putchar(' ');
        }
    }
    printf("\n\n");

    /* ── Task rows ────────────────────────────────────────────────── */
    for (int ti = 0; ti < task_count; ti++) {
        TaskControlBlock *task = all_tasks[ti];
        if (!task) continue;
        /* Skip idle task in visualization */
        if (task->priority == PRIORITY_IDLE) continue;

        /* Print task label */
        printf("%-11s(P%-3d) ", task->name, task->original_priority);

        /* Build state array */
        char *row = malloc((size_t)span + 1);
        if (!row) continue;
        memset(row, '_', (size_t)span);
        row[span] = '\0';

        /* Walk timeline entries chronologically for this task */
        VisualState cur_state = VIS_SUSPENDED;
        int         cur_pos   = -1;

        for (int e = 0; e < tl->count; e++) {
            const TimelineEntry *ent = &tl->entries[e];
            if (ent->task != task) continue;
            if (ent->state == VIS_NONE) continue;  /* annotation only */

            int pos = (int)(ent->tick - t_start);
            if (pos < 0 || pos >= span) continue;

            /* Fill previous state up to this point */
            if (cur_pos >= 0 && cur_pos < span) {
                char ch = '_';
                switch (cur_state) {
                    case VIS_RUNNING:   ch = '#'; break;
                    case VIS_READY:     ch = '-'; break;
                    case VIS_BLOCKED:   ch = '.'; break;
                    case VIS_SUSPENDED: ch = '_'; break;
                    default:            ch = '_'; break;
                }
                for (int p = cur_pos; p < pos && p < span; p++) {
                    row[p] = ch;
                }
            }

            cur_state = ent->state;
            cur_pos   = pos;
        }

        /* Fill remaining with last state */
        if (cur_pos >= 0 && cur_pos < span) {
            char ch = '_';
            switch (cur_state) {
                case VIS_RUNNING:   ch = '#'; break;
                case VIS_READY:     ch = '-'; break;
                case VIS_BLOCKED:   ch = '.'; break;
                case VIS_SUSPENDED: ch = '_'; break;
                default:            ch = '_'; break;
            }
            for (int p = cur_pos; p < span; p++) {
                row[p] = ch;
            }
        }

        printf("%s\n", row);
        free(row);
    }

    /* ── Legend ────────────────────────────────────────────────────── */
    printf("\nLegend: # = RUNNING  - = READY  . = BLOCKED  _ = SUSPENDED/NOT_RELEASED\n");

    /* ── Events Log ───────────────────────────────────────────────── */
    printf("\nEvents Log:\n");
    for (int e = 0; e < tl->count; e++) {
        const TimelineEntry *ent = &tl->entries[e];
        if (ent->annotation[0] != '\0') {
            printf("  [t=%-4" PRIu64 "] %s\n",
                   ent->tick, ent->annotation);
        }
    }

    /* ── Analysis ─────────────────────────────────────────────────── */
    int pi_count  = 0;
    int dl_misses = 0;
    for (int e = 0; e < tl->count; e++) {
        const TimelineEntry *ent = &tl->entries[e];
        if (strstr(ent->annotation, "PRIORITY INHERITANCE")) pi_count++;
        if (strstr(ent->annotation, "DEADLINE MISS"))        dl_misses++;
    }

    printf("\nAnalysis:\n");
    if (pi_count > 0) {
        printf("  * Priority inheritance triggered: %d time(s)\n", pi_count);
    } else {
        printf("  * No priority inheritance events\n");
    }
    if (dl_misses > 0) {
        printf("  * Deadline misses detected: %d\n", dl_misses);
    } else {
        printf("  * No deadline misses\n");
    }

    /* Context switches — get from first task's scheduler */
    if (task_count > 0 && all_tasks[0]) {
        Scheduler *s = all_tasks[0]->scheduler;
        if (s) {
            printf("  * Context switches: %" PRIu64 "\n",
                   s->context_switches);
        }
    }

    printf("\n");
}
