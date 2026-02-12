/*
 * timeline.h - ASCII Timeline Visualization
 *
 * Records scheduling events and renders them as an ASCII Gantt chart
 * with an events log and analysis section.
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#ifndef TIMELINE_H
#define TIMELINE_H

#include <stdint.h>
#include "task.h"

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct Mutex Mutex;

/* ── Constants ────────────────────────────────────────────────────── */
#define ANNOTATION_MAX  256
#define TIMELINE_INITIAL_CAP 1024

/* ── Visual state for rendering ───────────────────────────────────── */
typedef enum {
    VIS_RUNNING,
    VIS_READY,
    VIS_BLOCKED,
    VIS_SUSPENDED,
    VIS_NONE        /* Pure annotation, no state change */
} VisualState;

/* ── Single timeline entry ────────────────────────────────────────── */
typedef struct {
    uint64_t          tick;
    TaskControlBlock *task;
    VisualState       state;
    char              annotation[ANNOTATION_MAX];
} TimelineEntry;

/* ── Timeline container ───────────────────────────────────────────── */
typedef struct Timeline {
    TimelineEntry *entries;
    int            count;
    int            capacity;
    uint64_t       start_time;
    uint64_t       end_time;
} Timeline;

/* ── Public API ───────────────────────────────────────────────────── */

/** Allocate and initialize a timeline. */
Timeline *timeline_create(void);

/** Free all timeline memory. */
void timeline_destroy(Timeline *tl);

/** Record a generic event. */
void timeline_record(Timeline *tl, uint64_t tick,
                     TaskControlBlock *task,
                     VisualState state,
                     const char *annotation);

/* ── Convenience recorders ────────────────────────────────────────── */

void timeline_record_state_change(Timeline *tl, uint64_t tick,
                                  TaskControlBlock *task,
                                  VisualState state);

void timeline_record_priority_inherit(Timeline *tl, uint64_t tick,
                                      TaskControlBlock *low_task,
                                      TaskControlBlock *high_task,
                                      Mutex *mtx);

void timeline_record_priority_restore(Timeline *tl, uint64_t tick,
                                      TaskControlBlock *task,
                                      int old_pri, int new_pri);

void timeline_record_mutex_op(Timeline *tl, uint64_t tick,
                              TaskControlBlock *task,
                              Mutex *mtx,
                              const char *action);

void timeline_record_deadline_miss(Timeline *tl, uint64_t tick,
                                   TaskControlBlock *task,
                                   uint64_t deadline,
                                   uint64_t actual);

void timeline_record_preemption(Timeline *tl, uint64_t tick,
                                TaskControlBlock *preempted,
                                TaskControlBlock *preemptor);

/* ── Rendering ────────────────────────────────────────────────────── */

/** Render the full ASCII timeline to stdout. */
void timeline_render(const Timeline *tl,
                     TaskControlBlock **all_tasks,
                     int task_count);

#endif /* TIMELINE_H */
