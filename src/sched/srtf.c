#include <stddef.h>

#include "sched.h"

/* Shortest Remaining Time First: preemptive SJF.

   Unlike the non-preemptive schedulers, this one re-evaluates every tick
   and the running process holds no special position. It is simply one
   more candidate, and it keeps the CPU only by still having the smallest
   remaining time.

   Two rules from SPEC.md sections 2 and 3 combine here, and the order
   matters:

     The scan uses a strict <, so on equal remaining times the process
     earlier in the table wins. The table is sorted by arrival then pid,
     so that is the global tie-break applied for free.

     Strict improvement then overrides the scan when the winner merely
     ties the incumbent. Without that second step a tie would hand the CPU
     to whichever process happens to sort first, switching every tick
     between equals and inflating the context switch count for no gain. */

typedef struct srtf_state {
    const process_t *procs;
    int n;
    int running;      /* pid that ran last tick, or IDLE */
} srtf_state_t;

static srtf_state_t srtf_state;

/* Table lookup by pid. Returns NULL if the pid is not present, which
   happens when running is IDLE or the process has completed. */
static const process_t *find(const srtf_state_t *s, int pid)
{
    int i;

    if (pid == IDLE) {
        return NULL;
    }
    for (i = 0; i < s->n; i++) {
        if (s->procs[i].pid == pid) {
            return &s->procs[i];
        }
    }
    return NULL;
}

static void srtf_init(void *state, const config_t *cfg,
                      const process_t *procs, int n)
{
    srtf_state_t *s = (srtf_state_t *)state;

    (void)cfg;   /* SRTF has no quantum */

    s->procs = procs;
    s->n = n;
    s->running = IDLE;
}

static void srtf_on_arrival(void *state, const process_t *p, int tick)
{
    (void)state;
    (void)p;
    (void)tick;
    /* Nothing to record: pick_next scans the table each tick. */
}

static int srtf_pick_next(void *state, int current_pid, int tick)
{
    srtf_state_t *s = (srtf_state_t *)state;
    const process_t *best = NULL;
    const process_t *incumbent;
    int i;

    (void)current_pid;

    /* The running process is included in this scan, not excluded from it:
       the only filter is arrived and unfinished. */
    for (i = 0; i < s->n; i++) {
        const process_t *p = &s->procs[i];

        if (p->arrival > tick || p->remaining <= 0) {
            continue;
        }
        if (best == NULL || p->remaining < best->remaining) {
            best = p;
        }
    }

    if (best == NULL) {
        s->running = IDLE;
        return IDLE;
    }

    /* Strict improvement: a tie leaves the incumbent in place. */
    incumbent = find(s, s->running);
    if (incumbent != NULL && incumbent->remaining > 0 &&
        best->remaining == incumbent->remaining) {
        best = incumbent;
    }

    s->running = best->pid;
    return s->running;
}

static void srtf_on_tick_end(void *state, int running_pid, int tick)
{
    (void)state;
    (void)running_pid;
    (void)tick;
    /* No quantum to decrement, nothing to requeue. */
}

static void srtf_on_complete(void *state, const process_t *p, int tick)
{
    srtf_state_t *s = (srtf_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
}

scheduler_t srtf_scheduler = {
    "srtf",
    srtf_init,
    srtf_on_arrival,
    srtf_pick_next,
    srtf_on_tick_end,
    srtf_on_complete,
    NULL,
    &srtf_state
};
