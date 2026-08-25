#include <stddef.h>

#include "sched.h"

/* First Come First Served.

   Ready processes sit in a FIFO queue. The head runs until it finishes.
   There is no preemption and no quantum, so pick_next returns the current
   process unchanged whenever one is still running.

   The loader sorts the process table by arrival then pid, and the engine
   admits arrivals in table order, so enqueueing at the tail here makes
   queue order equal to arrival order with the tie-break already applied.
   No comparison is needed anywhere in this file. */

typedef struct fcfs_state {
    int queue[MAX_PROCESSES];
    int head;            /* index of the next process to run */
    int tail;            /* one past the last queued process */
    int running;         /* pid on the CPU, or IDLE */
} fcfs_state_t;

static fcfs_state_t fcfs_state;

static void fcfs_init(void *state, const config_t *cfg,
                      const process_t *procs, int n)
{
    fcfs_state_t *s = (fcfs_state_t *)state;

    (void)cfg;    /* FCFS has no quantum */
    (void)procs;  /* selection never inspects process fields */
    (void)n;

    s->head = 0;
    s->tail = 0;
    s->running = IDLE;
}

static void fcfs_on_arrival(void *state, const process_t *p, int tick)
{
    fcfs_state_t *s = (fcfs_state_t *)state;

    (void)tick;

    s->queue[s->tail] = p->pid;
    s->tail++;
}

static int fcfs_pick_next(void *state, int current_pid, int tick)
{
    fcfs_state_t *s = (fcfs_state_t *)state;

    (void)current_pid;
    (void)tick;

    /* Whatever is running keeps running: no preemption, no quantum. */
    if (s->running != IDLE) {
        return s->running;
    }

    if (s->head < s->tail) {
        s->running = s->queue[s->head];
        s->head++;
        return s->running;
    }

    return IDLE;
}

static void fcfs_on_tick_end(void *state, int running_pid, int tick)
{
    (void)state;
    (void)running_pid;
    (void)tick;
    /* Nothing to do: no quantum to decrement, nothing to requeue. */
}

static void fcfs_on_complete(void *state, const process_t *p, int tick)
{
    fcfs_state_t *s = (fcfs_state_t *)state;

    (void)p;
    (void)tick;

    /* The CPU is free again; the next pick_next takes the queue head. */
    s->running = IDLE;
}

scheduler_t fcfs_scheduler = {
    "fcfs",
    fcfs_init,
    fcfs_on_arrival,
    fcfs_pick_next,
    fcfs_on_tick_end,
    fcfs_on_complete,
    NULL,              /* destroy: nothing allocated */
    &fcfs_state
};
