#include <stddef.h>

#include "sched.h"

/* Round Robin.

   The ready set is a FIFO queue and each dispatched process gets a fixed
   slice. When the slice runs out and the process is unfinished it goes to
   the tail and receives a fresh quantum on its next turn. Quantum expiry
   is the only source of preemption here, so the rule in SPEC.md section
   2.4 about keeping a remaining quantum never fires for this algorithm.

   The queue is a ring buffer, unlike the plain array FCFS uses: a process
   re-enters on every expiry, so head and tail wrap. A separate count
   distinguishes empty from full, which head == tail alone cannot.

   Ordering rule from SPEC.md section 6: a process arriving at tick t is
   enqueued before a process whose quantum expires at the end of tick t.
   That needs no special handling, because the engine admits arrivals at
   step 2 of the tick and calls on_tick_end at step 7. Moving the requeue
   into pick_next would silently break it. */

typedef struct rr_state {
    int queue[MAX_PROCESSES];
    int head;         /* index of the next process to dispatch */
    int count;        /* processes currently queued */
    int running;      /* pid on the CPU, or IDLE */
    int quantum;      /* slice length, from config */
    int left;         /* ticks left in the running process's slice */
} rr_state_t;

static rr_state_t rr_state;

static void rr_push(rr_state_t *s, int pid)
{
    s->queue[(s->head + s->count) % MAX_PROCESSES] = pid;
    s->count++;
}

static int rr_pop(rr_state_t *s)
{
    int pid = s->queue[s->head];

    s->head = (s->head + 1) % MAX_PROCESSES;
    s->count--;
    return pid;
}

static void rr_init(void *state, const config_t *cfg,
                    const process_t *procs, int n)
{
    rr_state_t *s = (rr_state_t *)state;

    (void)procs;   /* selection never inspects process fields */
    (void)n;

    s->head = 0;
    s->count = 0;
    s->running = IDLE;
    s->quantum = (cfg != NULL && cfg->quantum > 0) ? cfg->quantum : 4;
    s->left = 0;
}

static void rr_on_arrival(void *state, const process_t *p, int tick)
{
    rr_state_t *s = (rr_state_t *)state;

    (void)tick;

    rr_push(s, p->pid);
}

static int rr_pick_next(void *state, int current_pid, int tick)
{
    rr_state_t *s = (rr_state_t *)state;

    (void)current_pid;
    (void)tick;

    /* Mid-slice: the running process keeps the CPU. */
    if (s->running != IDLE && s->left > 0) {
        return s->running;
    }

    if (s->count > 0) {
        s->running = rr_pop(s);
        s->left = s->quantum;
        return s->running;
    }

    s->running = IDLE;
    return IDLE;
}

static void rr_on_tick_end(void *state, int running_pid, int tick)
{
    rr_state_t *s = (rr_state_t *)state;

    (void)tick;

    if (running_pid == IDLE) {
        return;
    }

    s->left--;

    /* A finished process was already cleared by on_complete, which the
       engine calls first. Anything still running here has work left, so
       an exhausted slice sends it to the tail. */
    if (s->running != IDLE && s->left <= 0) {
        rr_push(s, s->running);
        s->running = IDLE;
    }
}

static void rr_on_complete(void *state, const process_t *p, int tick)
{
    rr_state_t *s = (rr_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
    s->left = 0;
}

scheduler_t rr_scheduler = {
    "rr",
    rr_init,
    rr_on_arrival,
    rr_pick_next,
    rr_on_tick_end,
    rr_on_complete,
    NULL,
    &rr_state
};
