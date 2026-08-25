#include <stdio.h>
#include <string.h>

#include "engine.h"

int engine_run(scheduler_t *sched, process_t *procs, int n, run_result_t *out)
{
    int tick = 0;
    int completed = 0;
    int current = IDLE;
    int i;

    memset(out, 0, sizeof(*out));

    while (completed < n) {

        if (tick >= MAX_TICKS) {
            fprintf(stderr,
                    "engine: run exceeded %d ticks with %d of %d complete; "
                    "scheduler '%s' is not making progress\n",
                    MAX_TICKS, completed, n, sched->name);
            return -1;
        }

        /* Step 1 and 2 of SPEC.md section 8. The boost check belongs to
           MLFQ alone, so it happens inside that scheduler's on_tick_end
           and pick_next rather than here; the engine stays algorithm
           agnostic. Arrivals are admitted in table order, which is
           arrival then pid after the loader's sort. */
        for (i = 0; i < n; i++) {
            if (procs[i].arrival == tick) {
                sched->on_arrival(sched->state, &procs[i], tick);
            }
        }

        /* Step 3: ask the scheduler what runs. */
        {
            int next = sched->pick_next(sched->state, current, tick);

            /* Step 4: count the switch. Transitions to or from idle do
               not count, only process to different process. */
            if (next != current && next != IDLE && current != IDLE) {
                out->context_switches++;
            }

            /* Step 5: record the timeline. */
            out->timeline[tick] = next;

            /* Step 6: execute. */
            if (next != IDLE) {
                process_t *p = NULL;

                for (i = 0; i < n; i++) {
                    if (procs[i].pid == next) {
                        p = &procs[i];
                        break;
                    }
                }

                if (p == NULL) {
                    fprintf(stderr,
                            "engine: scheduler '%s' returned unknown pid %d "
                            "at tick %d\n", sched->name, next, tick);
                    return -1;
                }
                if (p->remaining <= 0) {
                    fprintf(stderr,
                            "engine: scheduler '%s' returned finished pid %d "
                            "at tick %d\n", sched->name, next, tick);
                    return -1;
                }
                if (p->arrival > tick) {
                    fprintf(stderr,
                            "engine: scheduler '%s' returned pid %d at tick %d "
                            "before its arrival at %d\n",
                            sched->name, next, tick, p->arrival);
                    return -1;
                }

                if (p->start_tick == NOT_STARTED) {
                    p->start_tick = tick;
                }

                p->remaining--;

                if (p->remaining == 0) {
                    p->completion = tick + 1;
                    completed++;
                    sched->on_complete(sched->state, p, tick);
                }
            }

            /* Step 7: end of tick bookkeeping. */
            sched->on_tick_end(sched->state, next, tick);

            current = next;
        }

        tick++;
    }

    out->total_ticks = tick;
    return 0;
}
