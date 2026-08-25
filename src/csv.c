#include <stdio.h>
#include <string.h>

#include "csv.h"

/* The summary header, shared by the per-run summary file and by the sweep
   file, which uses the same columns. */
#define SUMMARY_HEADER \
    "algo,trace,quantum,n_processes,total_ticks,busy_ticks,idle_ticks," \
    "avg_turnaround,avg_waiting,avg_response,throughput," \
    "cpu_utilisation,context_switches,max_waiting,stddev_waiting\n"

void csv_trace_token(const char *path, char *out, size_t size)
{
    const char *base = path;
    const char *p;
    size_t len;

    for (p = path; *p != '\0'; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }

    len = strlen(base);
    if (len > 4 && strcmp(base + len - 4, ".txt") == 0) {
        len -= 4;
    }
    if (len >= size) {
        len = size - 1;
    }

    memcpy(out, base, len);
    out[len] = '\0';
}

/* Build "<dir>/<algo>__<trace>__<kind>.csv". */
static void build_path(char *buf, size_t size, const char *dir,
                       const char *algo, const char *trace, const char *kind)
{
    snprintf(buf, size, "%s/%s__%s__%s.csv", dir, algo, trace, kind);
}

static int write_timeline(const char *path, const run_result_t *r)
{
    FILE *f = fopen(path, "w");
    int i;

    if (f == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return -1;
    }

    fputs("tick,pid\n", f);
    for (i = 0; i < r->total_ticks; i++) {
        fprintf(f, "%d,%d\n", i, r->timeline[i]);
    }

    fclose(f);
    return 0;
}

static int write_processes(const char *path, const process_t *procs, int n)
{
    FILE *f;
    int order[MAX_PROCESSES];
    int i, j;

    /* Rows go out ordered by pid, which is not the table's order: the
       loader sorts by arrival then pid. Sorting an index array leaves the
       process table itself untouched. Insertion sort is stable and n is
       tiny. */
    for (i = 0; i < n; i++) {
        order[i] = i;
    }
    for (i = 1; i < n; i++) {
        int key = order[i];
        j = i - 1;
        while (j >= 0 && procs[order[j]].pid > procs[key].pid) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return -1;
    }

    fputs("pid,arrival,burst,priority,nice,queue,start,completion,"
          "turnaround,waiting,response\n", f);

    for (i = 0; i < n; i++) {
        const process_t *p = &procs[order[i]];
        int tat = p->completion - p->arrival;
        int wt  = tat - p->burst;
        int rt  = p->start_tick - p->arrival;

        fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                p->pid, p->arrival, p->burst, p->priority, p->nice, p->queue,
                p->start_tick, p->completion, tat, wt, rt);
    }

    fclose(f);
    return 0;
}

static int write_summary(const char *path, const char *algo,
                         const char *trace, int quantum, const metrics_t *m)
{
    FILE *f = fopen(path, "w");

    if (f == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return -1;
    }

    fputs(SUMMARY_HEADER, f);
    fprintf(f, "%s,%s,%d,%d,%d,%d,%d,"
               "%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f\n",
            algo, trace, quantum,
            m->n_processes, m->total_ticks, m->busy_ticks, m->idle_ticks,
            m->avg_turnaround, m->avg_waiting, m->avg_response,
            m->throughput, m->cpu_utilisation,
            m->context_switches, m->max_waiting, m->stddev_waiting);

    fclose(f);
    return 0;
}

int csv_write_run(const char *dir, const char *algo, const char *trace_token,
                  int quantum, const process_t *procs, int n,
                  const run_result_t *r, const metrics_t *m)
{
    char path[PATH_MAX_LEN];

    build_path(path, sizeof(path), dir, algo, trace_token, "timeline");
    if (write_timeline(path, r) != 0) {
        return -1;
    }

    build_path(path, sizeof(path), dir, algo, trace_token, "processes");
    if (write_processes(path, procs, n) != 0) {
        return -1;
    }

    build_path(path, sizeof(path), dir, algo, trace_token, "summary");
    if (write_summary(path, algo, trace_token, quantum, m) != 0) {
        return -1;
    }

    return 0;
}
