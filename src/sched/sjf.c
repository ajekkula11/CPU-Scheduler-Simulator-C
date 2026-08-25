#include <stddef.h>

#include "sched.h"

/* Shortest Job First, non-preemptive.

   Selection key is burst, not remaining. Under a non-preemptive policy
   the two are identical at dispatch, since a process runs from start to
   finish without interruption, but naming the key as burst keeps the
   contrast with SRTF explicit.

   Ready processes are found by scanning the table for anything that has
   arrived and has work left, rather than by maintaining a separate list.
   The process count is small, and a scan cannot drift out of sync with
   the table the way a duplicated list can. */

typedef struct sjf_state {
    const process_t *procs;
    int n;
    int running;      /* pid on the CPU, or IDLE */
} sjf_state_t;

static sjf_state_t sjf_state;

static void sjf_init(void *state, const config_t *cfg,
                     const process_t *procs, int n)
{
    sjf_state_t *s = (sjf_state_t *)state;

    (void)cfg;   /* SJF has no quantum */

    s->procs = procs;
    s->n = n;
    s->running = IDLE;
}

static void sjf_on_arrival(void *state, const process_t *p, int tick)
{
    (void)state;
    (void)p;
    (void)tick;
    /* Nothing to record: pick_next finds eligible processes by scanning
       the table for arrival <= tick and remaining > 0. */
}

static int sjf_pick_next(void *state, int current_pid, int tick)
{
    sjf_state_t *s = (sjf_state_t *)state;
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
        if (best == NULL || p->burst < best->burst) {
            best = p;
        }
        /* Equal bursts need no comparison here. The table is sorted by
           arrival then pid, so the first process encountered with a given
           burst is already the tie-break winner, and the strict < above
           leaves it in place. */
    }

    s->running = (best == NULL) ? IDLE : best->pid;
    return s->running;
}

static void sjf_on_tick_end(void *state, int running_pid, int tick)
{
    (void)state;
    (void)running_pid;
    (void)tick;
    /* No quantum to decrement, nothing to requeue. */
}

static void sjf_on_complete(void *state, const process_t *p, int tick)
{
    sjf_state_t *s = (sjf_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
}

scheduler_t sjf_scheduler = {
    "sjf",
    sjf_init,
    sjf_on_arrival,
    sjf_pick_next,
    sjf_on_tick_end,
    sjf_on_complete,
    NULL,
    &sjf_state
};
