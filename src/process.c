#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "process.h"

#define LINE_MAX_LEN 256

/* True if the line holds nothing but whitespace, or begins with '#'
   after any leading whitespace. Such lines are skipped by the loader. */
static int is_skippable(const char *line)
{
    while (*line != '\0' && isspace((unsigned char)*line)) {
        line++;
    }
    return (*line == '\0' || *line == '#');
}

/* Order for qsort: arrival ascending, then pid ascending. This is the
   global tie-break from SPEC.md section 2, applied once at load time so
   that array order alone resolves ties everywhere else. The comparator
   is total because pids are unique, so stability does not matter. */
static int by_arrival_then_pid(const void *a, const void *b)
{
    const process_t *pa = (const process_t *)a;
    const process_t *pb = (const process_t *)b;

    if (pa->arrival != pb->arrival) {
        return (pa->arrival < pb->arrival) ? -1 : 1;
    }
    if (pa->pid != pb->pid) {
        return (pa->pid < pb->pid) ? -1 : 1;
    }
    return 0;
}

/* Validate one parsed record against SPEC.md section 4. Returns 1 if the
   record is good, 0 after printing the reason to stderr. */
static int check_fields(const process_t *p, const char *path, int lineno)
{
    const char *problem = NULL;

    if (p->pid < 1) {
        problem = "pid must be 1 or greater";
    } else if (p->arrival < 0) {
        problem = "arrival must be 0 or greater";
    } else if (p->burst < 1) {
        problem = "burst must be 1 or greater";
    } else if (p->priority < PRIORITY_MIN || p->priority > PRIORITY_MAX) {
        problem = "priority must be 0 to 9";
    } else if (p->nice < NICE_MIN || p->nice > NICE_MAX) {
        problem = "nice must be -5 to +5";
    } else if (p->queue < QUEUE_MIN || p->queue > QUEUE_MAX) {
        problem = "queue must be 0 to 2";
    }

    if (problem != NULL) {
        fprintf(stderr, "%s:%d: %s\n", path, lineno, problem);
        return 0;
    }
    return 1;
}

int load_trace(const char *path, process_t *procs, int max)
{
    char line[LINE_MAX_LEN];
    char trailing[LINE_MAX_LEN];
    FILE *f;
    int lineno = 0;
    int count = 0;
    int i;

    f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "cannot open trace file: %s\n", path);
        return -1;
    }

    while (fgets(line, (int)sizeof(line), f) != NULL) {
        process_t p;
        int fields;

        lineno++;

        if (is_skippable(line)) {
            continue;
        }

        if (count >= max) {
            fprintf(stderr, "%s:%d: more than %d processes\n",
                    path, lineno, max);
            fclose(f);
            return -1;
        }

        /* Six integers, then a %s that must find nothing. sscanf returning
           6 alone would accept a seventh field silently, so the trailing
           conversion is what enforces an exact field count. %s skips
           leading whitespace and stops at the next, so it matches only if
           real content follows the sixth integer. */
        fields = sscanf(line, "%d %d %d %d %d %d %255s",
                        &p.pid, &p.arrival, &p.burst,
                        &p.priority, &p.nice, &p.queue, trailing);

        if (fields < 6) {
            fprintf(stderr, "%s:%d: expected six integers\n", path, lineno);
            fclose(f);
            return -1;
        }
        if (fields > 6) {
            fprintf(stderr, "%s:%d: more than six fields\n", path, lineno);
            fclose(f);
            return -1;
        }

        if (!check_fields(&p, path, lineno)) {
            fclose(f);
            return -1;
        }

        for (i = 0; i < count; i++) {
            if (procs[i].pid == p.pid) {
                fprintf(stderr, "%s:%d: duplicate pid %d\n",
                        path, lineno, p.pid);
                fclose(f);
                return -1;
            }
        }

        p.remaining  = p.burst;
        p.start_tick = NOT_STARTED;
        p.completion = NOT_STARTED;

        procs[count] = p;
        count++;
    }

    fclose(f);

    if (count == 0) {
        fprintf(stderr, "%s: no processes found\n", path);
        return -1;
    }

    qsort(procs, (size_t)count, sizeof(process_t), by_arrival_then_pid);

    return count;
}
