#include <stddef.h>

#include "sched.h"

/* Multi-Level Feedback Queue.

   Same three-queue shape as MLQ, but the level is earned rather than
   assigned. The trace file's queue column is ignored: everyone enters at
   Q0 and moves down by using up slices.

     Q0  Round Robin, quantum 2
     Q1  Round Robin, quantum 4
     Q2  Round Robin, quantum 8   (not FCFS)

   Three rules from SPEC.md section 6, and the difference between them is
   what invariant 9 exists to police:

     Demotion happens on quantum exhaustion ONLY. Drop one level, tail of
     the new queue, fresh quantum. A process already in Q2 stays in Q2.

     Preemption by a higher-queue arrival never demotes. The process keeps
     its level and its remaining quantum and goes to the HEAD of its own
     queue.

     A boost at every tick that is a positive multiple of 50 moves every
     unfinished process to Q0 with a fresh Q0 quantum, the running one
     included, in arrival-then-pid order. The boost resets both level and
     quantum, so it overrides the keep-quantum rule above. */

#define NLEVELS 3
#define BOOST_INTERVAL 50

static const int LEVEL_QUANTUM[NLEVELS] = { 2, 4, 8 };

typedef struct mlfq_state {
    int queue[NLEVELS][MAX_PROCESSES];
    int head[NLEVELS];
    int count[NLEVELS];
    int saved[NLEVELS];   /* quantum left for that queue's head, 0 if fresh */

    const process_t *procs;
    int n;

    int level_of[MAX_PROCESSES + 1];  /* indexed by pid */

    int running;
    int level;
    int left;
} mlfq_state_t;

static mlfq_state_t mlfq_state;

static void push_tail(mlfq_state_t *s, int lv, int pid)
{
    s->queue[lv][(s->head[lv] + s->count[lv]) % MAX_PROCESSES] = pid;
    s->count[lv]++;
    s->level_of[pid] = lv;
}

static void push_head(mlfq_state_t *s, int lv, int pid, int left)
{
    s->head[lv] = (s->head[lv] + MAX_PROCESSES - 1) % MAX_PROCESSES;
    s->queue[lv][s->head[lv]] = pid;
    s->count[lv]++;
    s->saved[lv] = left;
    s->level_of[pid] = lv;
}

static int pop_head(mlfq_state_t *s, int lv, int *left)
{
    int pid = s->queue[lv][s->head[lv]];

    s->head[lv] = (s->head[lv] + 1) % MAX_PROCESSES;
    s->count[lv]--;

    *left = (s->saved[lv] > 0) ? s->saved[lv] : LEVEL_QUANTUM[lv];
    s->saved[lv] = 0;
    return pid;
}

static int highest(const mlfq_state_t *s)
{
    int lv;

    for (lv = 0; lv < NLEVELS; lv++) {
        if (s->count[lv] > 0) {
            return lv;
        }
    }
    return -1;
}

/* Everything unfinished back to Q0. The queues are rebuilt from the
   process table rather than merged, because the table is already sorted
   by arrival then pid and that is exactly the order the boost requires. */
static void boost(mlfq_state_t *s, int tick)
{
    int lv, i;

    for (lv = 0; lv < NLEVELS; lv++) {
        s->head[lv] = 0;
        s->count[lv] = 0;
        s->saved[lv] = 0;
    }

    for (i = 0; i < s->n; i++) {
        const process_t *p = &s->procs[i];

        if (p->arrival > tick || p->remaining <= 0) {
            continue;
        }
        if (p->pid == s->running) {
            continue;         /* re-queued below, after the rest */
        }
        push_tail(s, 0, p->pid);
    }

    /* The running process is boosted too. It goes to the head of Q0 with
       a fresh quantum: it held the CPU, so it keeps it unless the tick's
       selection says otherwise. */
    if (s->running != IDLE) {
        push_head(s, 0, s->running, LEVEL_QUANTUM[0]);
        s->running = IDLE;
        s->level = -1;
        s->left = 0;
    }
}

static void mlfq_init(void *state, const config_t *cfg,
                      const process_t *procs, int n)
{
    mlfq_state_t *s = (mlfq_state_t *)state;
    int lv, i;

    (void)cfg;   /* quanta and boost interval are fixed */

    for (lv = 0; lv < NLEVELS; lv++) {
        s->head[lv] = 0;
        s->count[lv] = 0;
        s->saved[lv] = 0;
    }
    for (i = 0; i <= MAX_PROCESSES; i++) {
        s->level_of[i] = 0;
    }
    s->procs = procs;
    s->n = n;
    s->running = IDLE;
    s->level = -1;
    s->left = 0;
}

static void mlfq_on_arrival(void *state, const process_t *p, int tick)
{
    mlfq_state_t *s = (mlfq_state_t *)state;

    (void)tick;

    /* Entry is always Q0. The trace's queue column belongs to MLQ. */
    push_tail(s, 0, p->pid);
}

static int mlfq_pick_next(void *state, int current_pid, int tick)
{
    mlfq_state_t *s = (mlfq_state_t *)state;
    int top;

    (void)current_pid;

    if (tick > 0 && tick % BOOST_INTERVAL == 0) {
        boost(s, tick);
    }

    if (s->running != IDLE) {
        top = highest(s);

        /* A strictly higher queue preempts. Level and slice are kept. */
        if (top >= 0 && top < s->level) {
            push_head(s, s->level, s->running, s->left);
            s->running = IDLE;
        } else if (s->left > 0) {
            return s->running;
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

static void mlfq_on_tick_end(void *state, int running_pid, int tick)
{
    mlfq_state_t *s = (mlfq_state_t *)state;

    (void)tick;

    if (running_pid == IDLE || s->running == IDLE) {
        return;
    }

    s->left--;

    /* Exhausted slice: down one level, tail, fresh quantum. Q2 is the
       floor, so a process there stays and simply re-queues. */
    if (s->left <= 0) {
        int next = (s->level + 1 < NLEVELS) ? s->level + 1 : s->level;

        push_tail(s, next, s->running);
        s->running = IDLE;
    }
}

static void mlfq_on_complete(void *state, const process_t *p, int tick)
{
    mlfq_state_t *s = (mlfq_state_t *)state;

    (void)p;
    (void)tick;

    s->running = IDLE;
    s->level = -1;
    s->left = 0;
}

scheduler_t mlfq_scheduler = {
    "mlfq",
    mlfq_init,
    mlfq_on_arrival,
    mlfq_pick_next,
    mlfq_on_tick_end,
    mlfq_on_complete,
    NULL,
    &mlfq_state
};
