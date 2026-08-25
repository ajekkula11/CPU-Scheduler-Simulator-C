#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"
#include "engine.h"
#include "metrics.h"
#include "process.h"
#include "sched/sched.h"

#define DEFAULT_QUANTUM 4
#define OUTPUT_DIR "output"

static void usage(void)
{
    int i;
    const char *name;

    fprintf(stderr,
        "usage:\n"
        "  scheduler --algo <name> --trace <file> [--quantum N]\n"
        "\n"
        "algorithms:");

    for (i = 0; (name = scheduler_names(i)) != NULL; i++) {
        fprintf(stderr, " %s", name);
    }
    fprintf(stderr, "\n\n--quantum applies to rr only (default %d).\n",
            DEFAULT_QUANTUM);
}

/* Parse an integer argument strictly: the whole string must be a number. */
static int parse_int(const char *s, int *out)
{
    char *end;
    long v;

    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 100000) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

int main(int argc, char **argv)
{
    const char *algo_name = NULL;
    const char *trace_path = NULL;
    int quantum = DEFAULT_QUANTUM;
    int quantum_given = 0;
    int i;

    process_t procs[MAX_PROCESSES];
    run_result_t result;
    metrics_t m;
    scheduler_t *sched;
    config_t cfg;
    char token[PATH_MAX_LEN];
    int n;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--algo") == 0 && i + 1 < argc) {
            algo_name = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (strcmp(argv[i], "--quantum") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &quantum) != 0) {
                fprintf(stderr, "invalid quantum: %s\n", argv[i]);
                return 1;
            }
            quantum_given = 1;
        } else {
            fprintf(stderr, "unrecognised argument: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (algo_name == NULL || trace_path == NULL) {
        usage();
        return 1;
    }

    sched = scheduler_lookup(algo_name);
    if (sched == NULL) {
        fprintf(stderr, "unknown algorithm: %s\n", algo_name);
        usage();
        return 1;
    }

    /* SPEC.md section 10: --quantum with anything but rr is an error,
       not a silent no-op. */
    if (quantum_given && strcmp(algo_name, "rr") != 0) {
        fprintf(stderr, "--quantum applies to rr only, not %s\n", algo_name);
        return 1;
    }

    n = load_trace(trace_path, procs, MAX_PROCESSES);
    if (n < 0) {
        return 1;
    }

    cfg.quantum = quantum;
    sched->init(sched->state, &cfg, procs, n);

    if (engine_run(sched, procs, n, &result) != 0) {
        return 1;
    }

    metrics_compute(procs, n, &result, &m);

    if (metrics_check_invariants(procs, n, &result, &m) != 0) {
        fprintf(stderr, "invariant check failed for %s on %s\n",
                algo_name, trace_path);
        return 1;
    }

    csv_trace_token(trace_path, token, sizeof(token));

    if (csv_write_run(OUTPUT_DIR, algo_name, token,
                      (strcmp(algo_name, "rr") == 0) ? quantum : 0,
                      procs, n, &result, &m) != 0) {
        return 1;
    }

    printf("%s on %s: %d ticks, avg wait %.2f, %d switches\n",
           algo_name, token, m.total_ticks, m.avg_waiting,
           m.context_switches);

    return 0;
}
