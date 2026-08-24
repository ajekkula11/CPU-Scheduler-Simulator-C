#ifndef PROCESS_H
#define PROCESS_H

/* Upper bound on processes in a single trace. The largest trace in the
   suite has 20, so this leaves generous headroom and lets the caller
   supply a fixed array instead of allocating. */
#define MAX_PROCESSES 64

/* Sentinel for start_tick before a process has ever been dispatched.
   Distinct from IDLE, which happens to share the value but means
   "no process on the CPU this tick". */
#define NOT_STARTED (-1)

/* Field bounds from SPEC.md section 4. */
#define PRIORITY_MIN 0
#define PRIORITY_MAX 9
#define NICE_MIN     (-5)
#define NICE_MAX      5
#define QUEUE_MIN     0
#define QUEUE_MAX     2

typedef struct process {
    /* The six fields read from the trace file. */
    int pid;
    int arrival;
    int burst;
    int priority;    /* 0..9, lower number is more urgent */
    int nice;        /* -5..+5, CFS only */
    int queue;       /* 0..2, MLQ only */

    /* Filled in by the engine during the run. */
    int remaining;   /* ticks of burst still to execute */
    int start_tick;  /* first dispatch, NOT_STARTED until then */
    int completion;  /* tick after the last execution tick */
} process_t;

/* Load a trace file into procs, which must hold at least max entries.

   Skips comment lines beginning with '#' and blank lines. Validates
   every field against SPEC.md section 4 and sorts the result by
   arrival, then pid.

   Returns the number of processes loaded, or -1 on any error, after
   printing a message naming the offending line to stderr. */
int load_trace(const char *path, process_t *procs, int max);

#endif /* PROCESS_H */
