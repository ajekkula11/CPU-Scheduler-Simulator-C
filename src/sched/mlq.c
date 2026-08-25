#include <stddef.h>

#include "sched.h"

/* Multi-Level Queue.

   Three queues, highest first. A process's queue comes from the trace file
   and never changes: there is no migration, which is the whole difference
   between this and MLFQ.

     Q0  Round Robin, quantum 2
     Q1  Round Robin, quantum 4
     Q2  FCFS, no quantum

   Higher queues win outright. An arrival into a queue above the running
   process takes the CPU immediately, and the displaced process goes back
   to the HEAD of its own queue keeping its remaining quantum, per SPEC.md
   section 3. Only quantum exhaustion sends a process to the tail.

   Because at most one process runs at a time, at most one process per
   level can be sitting on a partial quantum, and it is always that
   queue's head. So one saved value per level is enough. */

#define NLEVELS 3

static const int LEVEL_QUANTUM[NLEVELS] = { 2, 4, 0 };  /* 0 means FCFS */

typedef struct mlq_state {
    int queue[NLEVELS][MAX_PROCESSES];
    int head[NLEVELS];
    int count[NLEVELS];
    int saved[NLEVELS];   /* quantum left for that queue's head, 0 if fresh */

    const process_t *procs;
    int n;

    int running;          /* pid on the CPU, or IDLE */
    int level;            /* queue the running process came from */
    int left;             /* ticks left in its slice, unused for Q2 */
} mlq_state_t;

static mlq_state_t mlq_state;

static void push_tail(mlq_state_t *s, int lv, int pid)
{
    s->queue[lv][(s->head[lv] + s->count[lv]) % MAX_PROCESSES] = pid;
    s->count[lv]++;
}

static void push_head(mlq_state_t *s, int lv, int pid, int left)
{
    s->head[lv] = (s->head[lv] + MAX_PROCESSES - 1) % MAX_PROCESSES;
    s->queue[lv][s->head[lv]] = pid;
    s->count[lv]++;
    s->saved[lv] = left;
}

static int pop_head(mlq_state_t *s, int lv, int *left)
{
    int pid = s->queue[lv][s->head[lv]];

    s->head[lv] = (s->head[lv] + 1) % MAX_PROCESSES;
    s->count[lv]--;

    *left = (s->saved[lv] > 0) ? s->saved[lv] : LEVEL_QUANTUM[lv];
    s->saved[lv] = 0;
    return pid;
}

/* Highest level with anything queued, or -1 if all are empty. */
static int highest(const mlq_state_t *s)
{
    int lv;

    for (lv = 0; lv < NLEVELS; lv++) {
        if (s->count[lv] > 0) {
            return lv;
        }
    }
    return -1;
}

static void mlq_init(void *state, const config_t *cfg,
                     const process_t *procs, int n)
{
    mlq_state_t *s = (mlq_state_t *)state;
    int lv;

    (void)cfg;   /* quanta are fixed per level, not configurable */

    for (lv = 0; lv < NLEVELS; lv++) {
        s->head[lv] = 0;
        s->count[lv] = 0;
        s->saved[lv] = 0;
    }
    s->procs = procs;
    s->n = n;
    s->running = IDLE;
    s->level = -1;
    s->left = 0;
}

static void mlq_on_arrival(void *state, const process_t *p, int tick)
{
    mlq_state_t *s = (mlq_state_t *)state;

    (void)tick;

    push_tail(s, p->queue, p->pid);
}

static int mlq_pick_next(void *state, int current_pid, int tick)
{
    mlq_state_t *s = (mlq_state_t *)state;
    int top;

    (void)current_pid;
    (void)tick;

    if (s->running != IDLE) {
        top = highest(s);

        /* A strictly higher queue with work preempts. The displaced
           process keeps its place and its slice. */
        if (top >= 0 && top < s->level) {
            push_head(s, s->level, s->running, s->left);
            s->running = IDLE;
        } else {
            /* Q2 is FCFS: it runs to completion unless preempted above. */
            if (LEVEL_QUANTUM[s->level] == 0 || s->left > 0) {
                return s->running;
            }
        }
    }

    top = highest(s);
    if (top < 0) {
        s->running = IDLE;
        s->level = -1;
        return IDLE;
    }

    s->running = pop_head(s, top, &s->left);
    s->level = top;
    return s->running;
}

static void mlq_on_tick_end(void *state, int running_pid, int tick)
{
    mlq_state_t *s = (mlq_state_t *)state;

    (void)tick;

    if (running_pid == IDLE || s->running == IDLE) {
        return;
    }
    if (LEVEL_QUANTUM[s->level] == 0) {
        return;                       /* Q2 has no slice to spend */
    }

    s->left--;

    /* Exhausted slice: tail of the same queue, no demotion ever. */
    if (s->left <= 0) {
        push_tail(s, s->level, s->running);
        s->running = IDLE;
    }
}

static void mlq_on_complete(void *state, const process_t *p, int tick)
{
    mlq_state_t *s = (mlq_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
    s->level = -1;
    s->left = 0;
}

scheduler_t mlq_scheduler = {
    "mlq",
    mlq_init,
    mlq_on_arrival,
    mlq_pick_next,
    mlq_on_tick_end,
    mlq_on_complete,
    NULL,
    &mlq_state
};
