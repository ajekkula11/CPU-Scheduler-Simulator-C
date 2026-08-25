#ifndef METRICS_H
#define METRICS_H

#include "process.h"
#include "engine.h"

/* Aggregate results for one run, per SPEC.md section 5. Per-process
   metrics are derived directly from the process table and are not stored
   here; the CSV writer computes them inline. */
typedef struct metrics {
    int    n_processes;
    int    total_ticks;
    int    busy_ticks;
    int    idle_ticks;
    double avg_turnaround;
    double avg_waiting;
    double avg_response;
    double throughput;
    double cpu_utilisation;
    int    context_switches;
    int    max_waiting;
    double stddev_waiting;
} metrics_t;

/* Compute aggregates from a finished run. */
void metrics_compute(const process_t *procs, int n,
                     const run_result_t *r, metrics_t *out);

/* Check the invariants in SPEC.md section 12 that do not depend on a
   particular algorithm. Returns 0 if all hold, or -1 after printing the
   first violation to stderr. */
int metrics_check_invariants(const process_t *procs, int n,
                             const run_result_t *r, const metrics_t *m);

#endif /* METRICS_H */
