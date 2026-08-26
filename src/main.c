/* opendir and readdir are POSIX, not C99, and -std=c99 hides them unless
   a feature test macro asks for them. */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"
#include "engine.h"
#include "metrics.h"
#include "process.h"
#include "sched/sched.h"

#define DEFAULT_QUANTUM 4
#define OUTPUT_DIR      "output"
#define TRACE_DIR       "traces"
#define SWEEP_MIN       1
#define SWEEP_MAX       12

static void usage(void)
{
    int i;
    const char *name;

    fprintf(stderr,
        "usage:\n"
        "  scheduler --algo <name> --trace <file> [--quantum N]\n"
        "  scheduler --all\n"
        "  scheduler --sweep --trace <file>\n"
        "\n"
        "algorithms:");

    for (i = 0; (name = scheduler_names(i)) != NULL; i++) {
        fprintf(stderr, " %s", name);
    }
    fprintf(stderr, "\n\n--quantum applies to rr only (default %d).\n"
                    "--all runs every algorithm over every trace in %s/.\n"
                    "--sweep runs rr over quanta %d to %d and writes only\n"
                    "the sweep summary.\n",
            DEFAULT_QUANTUM, TRACE_DIR, SWEEP_MIN, SWEEP_MAX);
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

/* Load, run, check invariants, and compute metrics for one pairing. The
   trace is reloaded on every call because the engine mutates the process
   table. Returns 0 on success. If m is non-NULL the metrics are stored
   there; if write is non-zero the three CSV files are produced. */
static int run_one(const char *algo_name, const char *trace_path,
                   int quantum, int write, metrics_t *m_out)
{
    process_t procs[MAX_PROCESSES];
    run_result_t result;
    metrics_t m;
    scheduler_t *sched;
    config_t cfg;
    char token[PATH_MAX_LEN];
    int n;

    sched = scheduler_lookup(algo_name);
    if (sched == NULL) {
        fprintf(stderr, "unknown algorithm: %s\n", algo_name);
        return -1;
    }

    n = load_trace(trace_path, procs, MAX_PROCESSES);
    if (n < 0) {
        return -1;
    }

    cfg.quantum = quantum;
    sched->init(sched->state, &cfg, procs, n);

    if (engine_run(sched, procs, n, &result) != 0) {
        return -1;
    }

    metrics_compute(procs, n, &result, &m);

    if (metrics_check_invariants(procs, n, &result, &m) != 0) {
        fprintf(stderr, "invariant check failed for %s on %s\n",
                algo_name, trace_path);
        return -1;
    }

    csv_trace_token(trace_path, token, sizeof(token));

    if (write) {
        if (csv_write_run(OUTPUT_DIR, algo_name, token,
                          (strcmp(algo_name, "rr") == 0) ? quantum : 0,
                          procs, n, &result, &m) != 0) {
            return -1;
        }
        printf("%-12s %-18s %3d ticks  wait %6.2f  %2d switches\n",
               algo_name, token, m.total_ticks, m.avg_waiting,
               m.context_switches);
    }

    if (m_out != NULL) {
        *m_out = m;
    }
    return 0;
}

/* Every algorithm over every .txt file in traces/, alphabetically. Round
   Robin uses the default quantum here, per SPEC.md section 8. */
static int run_all(void)
{
    char traces[MAX_PROCESSES][PATH_MAX_LEN];
    DIR *d;
    struct dirent *e;
    int count = 0;
    int i, j, k;
    const char *algo;

    d = opendir(TRACE_DIR);
    if (d == NULL) {
        fprintf(stderr, "cannot open %s/\n", TRACE_DIR);
        return -1;
    }

    while ((e = readdir(d)) != NULL && count < MAX_PROCESSES) {
        size_t len = strlen(e->d_name);

        if (len > 4 && strcmp(e->d_name + len - 4, ".txt") == 0) {
            snprintf(traces[count], PATH_MAX_LEN, "%s/%s",
                     TRACE_DIR, e->d_name);
            count++;
        }
    }
    closedir(d);

    if (count == 0) {
        fprintf(stderr, "no .txt traces found in %s/\n", TRACE_DIR);
        return -1;
    }

    /* readdir order is filesystem dependent, and output should not be.
       Insertion sort by path keeps runs reproducible. */
    for (i = 1; i < count; i++) {
        char key[PATH_MAX_LEN];

        strcpy(key, traces[i]);
        j = i - 1;
        while (j >= 0 && strcmp(traces[j], key) > 0) {
            strcpy(traces[j + 1], traces[j]);
            j--;
        }
        strcpy(traces[j + 1], key);
    }

    for (k = 0; (algo = scheduler_names(k)) != NULL; k++) {
        for (i = 0; i < count; i++) {
            if (run_one(algo, traces[i], DEFAULT_QUANTUM, 1, NULL) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

/* Round Robin over quanta SWEEP_MIN to SWEEP_MAX on one trace. Only the
   sweep summary is written: per-run files would all collide on the same
   name, since the naming pattern has no quantum in it. */
static int run_sweep(const char *trace_path)
{
    metrics_t rows[SWEEP_MAX - SWEEP_MIN + 1];
    int quanta[SWEEP_MAX - SWEEP_MIN + 1];
    char token[PATH_MAX_LEN];
    int q;
    int i = 0;

    for (q = SWEEP_MIN; q <= SWEEP_MAX; q++) {
        if (run_one("rr", trace_path, q, 0, &rows[i]) != 0) {
            return -1;
        }
        quanta[i] = q;
        printf("rr q=%-3d %-18s %3d ticks  wait %6.2f  %2d switches\n",
               q, trace_path, rows[i].total_ticks, rows[i].avg_waiting,
               rows[i].context_switches);
        i++;
    }

    csv_trace_token(trace_path, token, sizeof(token));
    return csv_write_sweep(OUTPUT_DIR, token, quanta, rows, i);
}

int main(int argc, char **argv)
{
    const char *algo_name = NULL;
    const char *trace_path = NULL;
    int quantum = DEFAULT_QUANTUM;
    int quantum_given = 0;
    int want_all = 0;
    int want_sweep = 0;
    int i;

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
        } else if (strcmp(argv[i], "--all") == 0) {
            want_all = 1;
        } else if (strcmp(argv[i], "--sweep") == 0) {
            want_sweep = 1;
        } else {
            fprintf(stderr, "unrecognised argument: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (want_all && want_sweep) {
        fprintf(stderr, "--all and --sweep are mutually exclusive\n");
        return 1;
    }

    if (want_all) {
        if (algo_name != NULL || trace_path != NULL || quantum_given) {
            fprintf(stderr, "--all takes no other arguments\n");
            return 1;
        }
        return (run_all() == 0) ? 0 : 1;
    }

    if (want_sweep) {
        if (trace_path == NULL) {
            fprintf(stderr, "--sweep needs --trace\n");
            return 1;
        }
        if (algo_name != NULL || quantum_given) {
            fprintf(stderr, "--sweep sets the algorithm and quantum itself\n");
            return 1;
        }
        return (run_sweep(trace_path) == 0) ? 0 : 1;
    }

    if (algo_name == NULL || trace_path == NULL) {
        usage();
        return 1;
    }

    /* SPEC.md section 8: --quantum with anything but rr is an error,
       not a silent no-op. */
    if (quantum_given && strcmp(algo_name, "rr") != 0) {
        fprintf(stderr, "--quantum applies to rr only, not %s\n", algo_name);
        return 1;
    }

    return (run_one(algo_name, trace_path, quantum, 1, NULL) == 0) ? 0 : 1;
}
