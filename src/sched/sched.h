#ifndef SCHED_H
#define SCHED_H

#include "../process.h"

/* No process on the CPU this tick. Also the value written to the pid
   column of the timeline CSV for an idle tick. */
#define IDLE (-1)

/* Per-run configuration. Only values that can differ between runs of the
   same algorithm belong here. Everything a scheduler needs that is fixed
   by SPEC.md section 7 is a #define in that scheduler's own .c file. */
typedef struct config {
    int quantum;   /* Round Robin only; ignored by every other scheduler. */
} config_t;

/* The interface every algorithm implements. The engine calls only through
   these pointers and contains no algorithm-specific branch.

   The process table is const throughout: schedulers read it to make
   decisions but never write it. remaining, start_tick, and completion are
   the engine's to maintain. Scheduler-owned data, such as CFS vruntime or
   an MLFQ queue level, lives in the scheduler's own state keyed by pid.

   Call order within a tick follows SPEC.md section 8:
     on_arrival for each process arriving this tick, then pick_next,
     then the engine executes, then on_complete if it finished,
     then on_tick_end. */
typedef struct scheduler {
    const char *name;

    /* Called once before the run. procs is the loaded table, sorted by
       arrival then pid, and stays valid for the whole run. */
    void (*init)(void *state, const config_t *cfg,
                 const process_t *procs, int n);

    /* A process has arrived and is now eligible. Called at the start of
       the tick matching its arrival field. */
    void (*on_arrival)(void *state, const process_t *p, int tick);

    /* Return the pid to run during this tick, or IDLE. Preemption needs
       no special case: returning a pid other than current_pid is a
       preemption. */
    int (*pick_next)(void *state, int current_pid, int tick);

    /* Called after the tick has been executed. Quantum decrement,
       vruntime update, and requeue-on-exhaustion happen here. running_pid
       may be IDLE. */
    void (*on_tick_end)(void *state, int running_pid, int tick);

    /* The process finished during this tick. Its remaining is 0 and its
       completion has been set. */
    void (*on_complete)(void *state, const process_t *p, int tick);

    /* Release anything init allocated. May be NULL. */
    void (*destroy)(void *state);

    /* Opaque per-algorithm state, passed back to every callback. */
    void *state;
} scheduler_t;

/* Look up a scheduler by the canonical identifier from SPEC.md section 9
   ("fcfs", "sjf", "srtf", "rr", "priority_np", "priority_p", "mlq",
   "mlfq", "cfs"). Returns NULL if the name is not recognised. */
scheduler_t *scheduler_lookup(const char *name);

#endif /* SCHED_H */
