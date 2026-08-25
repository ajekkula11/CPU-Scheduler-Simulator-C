#include <stddef.h>

#include "sched.h"

/* Priority scheduling, both variants.

   Selection key is the priority field, where a lower number means more
   urgent. The two schedulers differ in exactly one respect: whether a
   running process can be displaced.

   priority_np dispatches only when the CPU is free, so a high-priority
   arrival waits for the current process to finish.

   priority_p re-evaluates every tick with the running process as one more
   candidate, so a high-priority arrival takes the CPU immediately. A tie
   leaves the incumbent in place, per the strict improvement rule in
   SPEC.md section 3.

   Both scan the process table rather than maintaining a ready list, and
   both rely on the table being sorted by arrival then pid so that the
   strict < in the scan applies the global tie-break for free. */

/* ------------------------------------------------------------------ */
/* Non-preemptive                                                      */
/* ------------------------------------------------------------------ */

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

    (void)cfg;   /* priority_np has no quantum */

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
        /* Equal priorities need no comparison here. The table is sorted
           by arrival then pid, so the first process encountered at a
           given priority is already the tie-break winner, and the strict
           < above leaves it in place. */
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

/* ------------------------------------------------------------------ */
/* Preemptive                                                          */
/* ------------------------------------------------------------------ */

typedef struct priority_p_state {
    const process_t *procs;
    int n;
    int running;      /* pid that ran last tick, or IDLE */
} priority_p_state_t;

static priority_p_state_t priority_p_state;

/* Table lookup by pid. Returns NULL when the pid is IDLE or absent. */
static const process_t *pp_find(const priority_p_state_t *s, int pid)
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

static void priority_p_init(void *state, const config_t *cfg,
                            const process_t *procs, int n)
{
    priority_p_state_t *s = (priority_p_state_t *)state;

    (void)cfg;   /* priority_p has no quantum */

    s->procs = procs;
    s->n = n;
    s->running = IDLE;
}

static void priority_p_on_arrival(void *state, const process_t *p, int tick)
{
    (void)state;
    (void)p;
    (void)tick;
    /* Nothing to record: pick_next scans the table each tick. */
}

static int priority_p_pick_next(void *state, int current_pid, int tick)
{
    priority_p_state_t *s = (priority_p_state_t *)state;
    const process_t *best = NULL;
    const process_t *incumbent;
    int i;

    (void)current_pid;

    /* No early return: the running process competes like any other, and
       the only filter is arrived and unfinished. */
    for (i = 0; i < s->n; i++) {
        const process_t *p = &s->procs[i];

        if (p->arrival > tick || p->remaining <= 0) {
            continue;
        }
        if (best == NULL || p->priority < best->priority) {
            best = p;
        }
    }

    if (best == NULL) {
        s->running = IDLE;
        return IDLE;
    }

    /* Strict improvement: a tie leaves the incumbent in place. Without
       this the scan would hand the CPU to whichever tied process sorts
       first, switching every tick between equals. */
    incumbent = pp_find(s, s->running);
    if (incumbent != NULL && incumbent->remaining > 0 &&
        best->priority == incumbent->priority) {
        best = incumbent;
    }

    s->running = best->pid;
    return s->running;
}

static void priority_p_on_tick_end(void *state, int running_pid, int tick)
{
    (void)state;
    (void)running_pid;
    (void)tick;
    /* No quantum to decrement, nothing to requeue. */
}

static void priority_p_on_complete(void *state, const process_t *p, int tick)
{
    priority_p_state_t *s = (priority_p_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
}

scheduler_t priority_p_scheduler = {
    "priority_p",
    priority_p_init,
    priority_p_on_arrival,
    priority_p_pick_next,
    priority_p_on_tick_end,
    priority_p_on_complete,
    NULL,
    &priority_p_state
};
