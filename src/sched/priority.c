#include <stddef.h>

#include "sched.h"

/* Priority, non-preemptive.

   Selection key is the priority field, where a lower number means more
   urgent. Once dispatched a process runs to completion, so a
   high-priority arrival waits for the current process to finish. That is
   what separates this from priority_p. */

typedef struct priority_np_state {
    const process_t *procs;
    int n;
    int running;      /* pid on the CPU, or IDLE */
} priority_np_state_t;

static priority_np_state_t priority_np_state;

static void priority_np_init(void *state, const config_t *cfg,
                     const process_t *procs, int n)
{
    priority_np_state_t *s = (priority_np_state_t *)state;

    (void)cfg;   /* Priority_NP has no quantum */

    s->procs = procs;
    s->n = n;
    s->running = IDLE;
}

static void priority_np_on_arrival(void *state, const process_t *p, int tick)
{
    (void)state;
    (void)p;
    (void)tick;
    /* Nothing to record: pick_next finds eligible processes by scanning
       the table for arrival <= tick and remaining > 0. */
}

static int priority_np_pick_next(void *state, int current_pid, int tick)
{
    priority_np_state_t *s = (priority_np_state_t *)state;
    const process_t *best = NULL;
    int i;

    (void)current_pid;

    /* No preemption: a running process keeps the CPU until it finishes. */
    if (s->running != IDLE) {
        return s->running;
    }

    for (i = 0; i < s->n; i++) {
        const process_t *p = &s->procs[i];

        if (p->arrival > tick || p->remaining <= 0) {
            continue;
        }
        if (best == NULL || p->priority < best->priority) {
            best = p;
        }
        /* Equal priorities need no comparison here. The table is sorted by
           arrival then pid, so the first process encountered with a given
           burst is already the tie-break winner, and the strict < above
           leaves it in place. */
    }

    s->running = (best == NULL) ? IDLE : best->pid;
    return s->running;
}

static void priority_np_on_tick_end(void *state, int running_pid, int tick)
{
    (void)state;
    (void)running_pid;
    (void)tick;
    /* No quantum to decrement, nothing to requeue. */
}

static void priority_np_on_complete(void *state, const process_t *p, int tick)
{
    priority_np_state_t *s = (priority_np_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
}

scheduler_t priority_np_scheduler = {
    "priority_np",
    priority_np_init,
    priority_np_on_arrival,
    priority_np_pick_next,
    priority_np_on_tick_end,
    priority_np_on_complete,
    NULL,
    &priority_np_state
};
