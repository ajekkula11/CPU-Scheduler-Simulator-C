#ifndef CSV_H
#define CSV_H

#include <stddef.h>

#include "process.h"
#include "engine.h"
#include "metrics.h"

/* Longest output path this program builds. */
#define PATH_MAX_LEN 512

/* Strip a trace path down to the token used in output filenames: the
   basename with any .txt extension removed, so "traces/t1_basic.txt"
   becomes "t1_basic". Writes at most size bytes including the
   terminator. */
void csv_trace_token(const char *path, char *out, size_t size);

/* Write the three output files for one run into dir, named per SPEC.md
   section 9. Returns 0 on success, -1 after printing to stderr. */
int csv_write_run(const char *dir, const char *algo, const char *trace_token,
                  int quantum, const process_t *procs, int n,
                  const run_result_t *r, const metrics_t *m);

#endif /* CSV_H */
