#include <stddef.h>
#include <stdint.h>

#include "sched.h"

/* Completely Fair Scheduler, simplified.

   No queues. Each process carries a virtual runtime that advances in
   proportion to how much CPU it has used, scaled by its weight. The
   process with the smallest vruntime runs. A low nice value means a large
   weight, which means vruntime advances slowly, which means more CPU.

   Selection is a scan for the minimum vruntime among arrived and
   unfinished processes, the running one included, with strict improvement
   on ties: the same shape as SRTF, with vruntime in place of remaining.

   Simplifications against the real thing, all deliberate and all recorded
   in docs/CFS_NOTES.md:

     No min_granularity, so a process can be preempted after a single tick
     where Linux would guarantee it a minimum slice.

     No wakeup granularity or sleeper fairness.

     Integer arithmetic with truncating division. This one has teeth: see
     the note on the update below. */

#define NICE_0_LOAD 1024

/* Real Linux weights for the nice range SPEC.md section 6 supports.
   Index 0 is nice -5, index 10 is nice +5. */
static const int WEIGHT[11] = {
    3121, 2501, 1991, 1586, 1277,
    1024,
    820, 655, 526, 423, 335
};

typedef struct cfs_state {
    const process_t *procs;
    int n;
    uint64_t vruntime[MAX_PROCESSES + 1];   /* indexed by pid */
    int running;
} cfs_state_t;

static cfs_state_t cfs_state;

static int weight_of(int nice)
{
    int idx = nice - NICE_MIN;

    if (idx < 0) {
        idx = 0;
    }
    if (idx > 10) {
        idx = 10;
    }
    return WEIGHT[idx];
}

static const process_t *cfs_find(const cfs_state_t *s, int pid)
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

static void cfs_init(void *state, const config_t *cfg,
                     const process_t *procs, int n)
{
    cfs_state_t *s = (cfs_state_t *)state;
    int i;

    (void)cfg;   /* CFS has no quantum */

    s->procs = procs;
    s->n = n;
    s->running = IDLE;
    for (i = 0; i <= MAX_PROCESSES; i++) {
        s->vruntime[i] = 0;
    }
}

static void cfs_on_arrival(void *state, const process_t *p, int tick)
{
    cfs_state_t *s = (cfs_state_t *)state;
    uint64_t min = 0;
    int found = 0;
    int i;

    /* A new process starts at the minimum vruntime currently in play, not
       at zero. Starting at zero would let a late arrival monopolise the
       CPU until it caught up with everyone else's accumulated runtime.
       The running process counts, per SPEC.md section 6. */
    for (i = 0; i < s->n; i++) {
        const process_t *q = &s->procs[i];

        if (q->pid == p->pid || q->arrival > tick || q->remaining <= 0) {
            continue;
        }
        if (!found || s->vruntime[q->pid] < min) {
            min = s->vruntime[q->pid];
            found = 1;
        }
    }

    s->vruntime[p->pid] = found ? min : 0;
}

static int cfs_pick_next(void *state, int current_pid, int tick)
{
    cfs_state_t *s = (cfs_state_t *)state;
    const process_t *best = NULL;
    const process_t *incumbent;
    int i;

    (void)current_pid;

    for (i = 0; i < s->n; i++) {
        const process_t *p = &s->procs[i];

        if (p->arrival > tick || p->remaining <= 0) {
            continue;
        }
        if (best == NULL || s->vruntime[p->pid] < s->vruntime[best->pid]) {
            best = p;
        }
        /* Equal vruntimes need no comparison: the table is sorted by
           arrival then pid, so the strict < leaves the tie-break winner
           in place. */
    }

    if (best == NULL) {
        s->running = IDLE;
        return IDLE;
    }

    /* Strict improvement: a tie leaves the incumbent running. */
    incumbent = cfs_find(s, s->running);
    if (incumbent != NULL && incumbent->remaining > 0 &&
        s->vruntime[best->pid] == s->vruntime[incumbent->pid]) {
        best = incumbent;
    }

    s->running = best->pid;
    return s->running;
}

static void cfs_on_tick_end(void *state, int running_pid, int tick)
{
    cfs_state_t *s = (cfs_state_t *)state;
    const process_t *p;

    (void)tick;

    if (running_pid == IDLE) {
        return;
    }

    p = cfs_find(s, running_pid);
    if (p == NULL) {
        return;
    }

    /* delta_exec is always 1 tick, so the update is NICE_0_LOAD / weight.
       Truncating division means any weight above 1024 gives an increment
       of zero: a nice -5 process accumulates no vruntime at all and never
       yields. Real CFS avoids this with an inverse-weight table and a
       multiply-shift, which is precisely the rounding problem this
       simplification exposes. */
    s->vruntime[running_pid] += (uint64_t)(NICE_0_LOAD / weight_of(p->nice));
}

static void cfs_on_complete(void *state, const process_t *p, int tick)
{
    cfs_state_t *s = (cfs_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
}

scheduler_t cfs_scheduler = {
    "cfs",
    cfs_init,
    cfs_on_arrival,
    cfs_pick_next,
    cfs_on_tick_end,
    cfs_on_complete,
    NULL,
    &cfs_state
};
