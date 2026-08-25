#ifndef ENGINE_H
#define ENGINE_H

#include "process.h"
#include "sched/sched.h"

/* Upper bound on ticks in a run. Total ticks can never exceed the last
   arrival plus the sum of all bursts, which for the trace suite is well
   under a hundred. This cap leaves room for far larger traces and lets
   the timeline be a fixed array. */
#define MAX_TICKS 8192

/* Everything the run produces that does not live in the process table.
   Per-process results (start_tick, completion) are written back into the
   process array itself. */
typedef struct run_result {
    int timeline[MAX_TICKS];  /* pid running at each tick, or IDLE */
    int total_ticks;          /* number of ticks simulated */
    int context_switches;     /* per SPEC.md section 5 */
} run_result_t;

/* Run one algorithm over one trace. procs is modified in place: the
   engine maintains remaining, start_tick, and completion. Returns 0 on
   success, -1 if the run exceeded MAX_TICKS, which means a scheduler bug
   rather than a legitimate workload. */
int engine_run(scheduler_t *sched, process_t *procs, int n,
               run_result_t *out);

#endif /* ENGINE_H */
