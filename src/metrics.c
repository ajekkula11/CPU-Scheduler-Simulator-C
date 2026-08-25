#include <stdio.h>
#include <math.h>

#include "metrics.h"
#include "sched/sched.h"

void metrics_compute(const process_t *procs, int n,
                     const run_result_t *r, metrics_t *out)
{
    double sum_tat = 0.0;
    double sum_wt  = 0.0;
    double sum_rt  = 0.0;
    double mean_wt;
    double var = 0.0;
    int busy = 0;
    int max_wt = 0;
    int i;

    for (i = 0; i < r->total_ticks; i++) {
        if (r->timeline[i] != IDLE) {
            busy++;
        }
    }

    for (i = 0; i < n; i++) {
        int tat = procs[i].completion - procs[i].arrival;
        int wt  = tat - procs[i].burst;
        int rt  = procs[i].start_tick - procs[i].arrival;

        sum_tat += tat;
        sum_wt  += wt;
        sum_rt  += rt;

        if (wt > max_wt) {
            max_wt = wt;
        }
    }

    /* Population standard deviation: the process set is the whole
       population under study, not a sample, so divide by n. */
    mean_wt = sum_wt / n;
    for (i = 0; i < n; i++) {
        int wt = procs[i].completion - procs[i].arrival - procs[i].burst;
        double d = wt - mean_wt;
        var += d * d;
    }
    var /= n;

    out->n_processes      = n;
    out->total_ticks      = r->total_ticks;
    out->busy_ticks       = busy;
    out->idle_ticks       = r->total_ticks - busy;
    out->avg_turnaround   = sum_tat / n;
    out->avg_waiting      = mean_wt;
    out->avg_response     = sum_rt / n;
    out->throughput       = (double)n / r->total_ticks;
    out->cpu_utilisation  = (double)busy / r->total_ticks;
    out->context_switches = r->context_switches;
    out->max_waiting      = max_wt;
    out->stddev_waiting   = sqrt(var);
}

int metrics_check_invariants(const process_t *procs, int n,
                             const run_result_t *r, const metrics_t *m)
{
    int sum_burst = 0;
    int i;

    for (i = 0; i < n; i++) {
        sum_burst += procs[i].burst;
    }

    /* 1. Executed ticks equal the sum of all bursts. */
    if (m->busy_ticks != sum_burst) {
        fprintf(stderr, "invariant 1: busy ticks %d but bursts sum to %d\n",
                m->busy_ticks, sum_burst);
        return -1;
    }

    /* 2. Total ticks equal busy plus idle. */
    if (m->total_ticks != m->busy_ticks + m->idle_ticks) {
        fprintf(stderr, "invariant 2: %d ticks but %d busy plus %d idle\n",
                m->total_ticks, m->busy_ticks, m->idle_ticks);
        return -1;
    }

    for (i = 0; i < n; i++) {
        const process_t *p = &procs[i];
        int tat, wt, rt;

        /* 8. Everything finishes. */
        if (p->completion == NOT_STARTED || p->start_tick == NOT_STARTED) {
            fprintf(stderr, "invariant 8: pid %d never completed\n", p->pid);
            return -1;
        }

        /* 3. Nothing runs before it arrives. */
        if (p->start_tick < p->arrival) {
            fprintf(stderr, "invariant 3: pid %d started at %d but arrived "
                    "at %d\n", p->pid, p->start_tick, p->arrival);
            return -1;
        }

        /* 5. Completion is at least arrival plus burst. */
        if (p->completion < p->arrival + p->burst) {
            fprintf(stderr, "invariant 5: pid %d completed at %d, earlier "
                    "than arrival %d plus burst %d\n",
                    p->pid, p->completion, p->arrival, p->burst);
            return -1;
        }

        /* 6 and 7. Metric relationships hold. */
        tat = p->completion - p->arrival;
        wt  = tat - p->burst;
        rt  = p->start_tick - p->arrival;

        if (rt < 0 || rt > wt) {
            fprintf(stderr, "invariant 7: pid %d has response %d and "
                    "waiting %d\n", p->pid, rt, wt);
            return -1;
        }
    }

    /* 4. Two processes never run in the same tick. The timeline holds one
       pid per tick by construction, so this checks the weaker property
       that every recorded pid is a real one. */
    for (i = 0; i < r->total_ticks; i++) {
        int pid = r->timeline[i];
        int j;
        int found = 0;

        if (pid == IDLE) {
            continue;
        }
        for (j = 0; j < n; j++) {
            if (procs[j].pid == pid) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "invariant 4: tick %d records unknown pid %d\n",
                    i, pid);
            return -1;
        }
    }

    return 0;
}
