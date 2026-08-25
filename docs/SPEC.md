# SPEC.md

Implementation rules for the CPU Scheduler Simulator.

Everything here is fixed. Nothing gets decided while coding. If a rule changes, it changes 
here first, and the golden files in `tests/expected/` get recomputed.

---

## 1. How time works

- One CPU. Time moves in whole ticks, starting at tick 0. - A process that arrives at tick `t` 
can run during tick `t`. - Scheduling decisions happen only at tick boundaries, never in the 
middle of a tick. - One process at most runs during a tick. A tick with nothing running is 
idle. - A context switch costs 0 ticks. Switches are counted, never charged. - The run ends 
after the tick in which the last process finishes.

Total ticks counts idle ticks, including any before the first arrival.

---

## 2. Priority and tie-breaking

**Lower priority number means higher priority.** Priority 0 is the most urgent. Valid range is 
0 to 9.

**Tie-break rule.** When two processes are equally good under an algorithm's own rule:

1. Earlier arrival wins. 2. If arrivals are equal, lower PID wins.

Every algorithm uses this same rule. No algorithm invents its own.

---

## 3. Preemption rules

**Strict improvement only.** A running process is replaced only when a challenger is strictly 
better on the algorithm's key. A tie leaves the running process alone.

**Preemption does not cost a turn.** A process pushed off the CPU by anything other than its 
own quantum running out goes back to the **head** of its queue and keeps its **remaining 
quantum**.

**Running out of quantum does cost a turn.** The process goes to the **tail** with a fresh 
quantum. In MLFQ it also drops one level.

---

## 4. Trace file format

One process per line, six numbers separated by spaces. Lines starting with `#` are comments. 
Blank lines are skipped.

```
# pid arrival burst priority nice queue
  1 0 6 3 0 0 2 0 2 1 0 0
```

| Field | Range | Used by | ---|---|---|
| `pid` | 1 or higher, unique | all, and the final tie-break | `arrival` | 0 or higher | all | 
| `burst` | 1 or higher | all | `priority` | 0 to 9 | Priority-NP, Priority-P | `nice` | -5 to 
| +5 | CFS | `queue` | 0 to 2 | MLQ |

All six fields are always present, even for algorithms that ignore some of them, so one trace 
works for all nine algorithms.

The loader rejects the file and exits with an error if a line has the wrong number of fields, 
a value is out of range, a PID repeats, or the file has no processes. After loading, processes 
are sorted by arrival, then PID.

---

## 5. Metric formulas

Per process:

| Metric | Formula | ---|---|
| start | tick of first run, set once, not updated after a preemption | completion | the tick 
| after its last execution tick | turnaround | completion − arrival | waiting | turnaround − 
| burst | response | start − arrival |

Aggregate:

| Metric | Formula | ---|---|
| avg turnaround | sum of turnaround ÷ number of processes | avg waiting | sum of waiting ÷ 
| number of processes | avg response | sum of response ÷ number of processes | throughput | 
| number of processes ÷ total ticks | CPU utilisation | busy ticks ÷ total ticks | context 
| switches | see below | max waiting | largest waiting value | stddev waiting | population 
| standard deviation, divide by N |

Averages use floating point, never integer division.

**Counting context switches.** A switch is counted when the CPU goes from one process to a 
**different** process on the next tick. Going to or from idle does not count. This is not the 
same as counting preemptions: SJF on `t1_basic` has zero preemptions but four switches, 
because five processes run one after another.

The engine counts switches. No algorithm implementation knows switching exists.

---

## 6. The nine algorithms

| Algorithm | Picks | Preempts? | Quantum | ---|---|---|---|
| FCFS | head of the queue | never | none | SJF-NP | smallest burst among waiting | never | 
| none | SRTF | smallest remaining time, including the running process | every tick, strict 
| improvement | none | RR | head of the queue | only on quantum expiry | 4 by default | 
| Priority-NP | lowest priority number among waiting | never | none | Priority-P | lowest 
| priority number, including the running process | every tick, strict improvement | none | MLQ 
| | highest non-empty queue, then that queue's policy | yes, by higher-queue arrivals | Q0=2, 
| Q1=4, Q2 none | MLFQ | highest non-empty queue, head of it | yes, by higher-queue arrivals 
| and boosts | Q0=2, Q1=4, Q2=8 | CFS | lowest vruntime, including the running process | every 
| tick, strict improvement | none |

Selection is a linear scan over the process array for SJF, SRTF, and both Priority algorithms. 
Process counts are small, and an obvious scan beats a clever structure here.

### Round Robin

Quantum 4 by default, settable with `--quantum`. The sweep runs quanta 1 to 12 on 
`t5_mixed.txt`.

When the quantum runs out and the process is not finished, it goes to the tail and gets a 
fresh quantum next time.

**Queue order.** A process arriving at tick `t` is enqueued before a process whose quantum 
expires at the end of tick `t`. This falls out of the tick order in Section 8: arrivals are 
admitted at the start of a tick, requeues happen at the end.

### MLQ

Three queues. Q0 is highest. A process's queue comes from the trace file and never changes.

- Q0: Round Robin, quantum 2 - Q1: Round Robin, quantum 4 - Q2: FCFS

Higher queues win outright. An arrival into a higher queue immediately preempts a process 
running from a lower one, and that process goes to the head of its own queue keeping its 
remaining quantum.

### MLFQ

Three queues, same levels. Everyone enters at Q0. Quanta are Q0=2, Q1=4, Q2=8. Q2 is Round 
Robin, not FCFS.

- **Demotion happens only when a quantum runs out.** Drop one level, go to the tail, get a 
fresh quantum. A process already in Q2 stays in Q2. - **Preemption by a higher-queue arrival 
never demotes.** The process keeps its level and its remaining quantum, and goes to the head 
of its queue. - **Boost.** At every tick that is a multiple of 50 (so 50, 100, 150, not 0), 
every unfinished process moves to Q0 with a fresh Q0 quantum, including the one currently 
running. They enter Q0 in tie-break order: arrival, then PID.

A boost resets both level and quantum for everyone. The "keeps its remaining quantum" rule 
applies to preemption by a higher-queue arrival, not to a boost.

### CFS, simplified

Ready processes are kept in an array sorted by vruntime, ties broken by the usual rule.

Weights by nice value:

| nice | -5 | -4 | -3 | -2 | -1 | 0 | 1 | 2 | 3 | 4 | 5 | 
|---|---|---|---|---|---|---|---|---|---|---|---|
| weight | 3121 | 2501 | 1991 | 1586 | 1277 | 1024 | 820 | 655 | 526 | 423 | 335 |

At the end of every tick a process runs:

``` vruntime += 1024 / weight ```

64-bit unsigned, truncating division, updated in exactly one place and never adjusted on 
preemption.

A new arrival gets the lowest vruntime among all unfinished processes, including the one 
running. If there are none, it gets 0.

Truncating division means weights above 1024 give an increment of 0, so nice -5 never 
accumulates vruntime at all. That is a real limitation and it goes in `docs/CFS_NOTES.md`.

---

## 7. The scheduler interface

Every algorithm fills in the same struct. The process table is const and the engine owns the deriver fields.

```typedef struct scheduler {
    const char *name;

    void (*init)(void *state, const config_t *cfg,
                 const process_t *procs, int n);
   
    void (*on_arrival)(void *state, const process_t *p, int tick);
    
    int (*pick_next)(void *state, int current_pid, int tick);

    void (*on_tick_end)(void *state, int running_pid, int tick);

    void (*on_complete)(void *state, const process_t *p, int tick);
    void (*destroy)(void *state);
    void *state;
} scheduler_t;
```

A scheduler helps by mapping the canonical identifier from SPEC.md
---

## 8. Order of events in a tick

Fixed, because changing it changes the answers.

1. MLFQ boost, if this tick is a multiple of 50 and not tick 0. 2. Admit every process 
arriving this tick, in PID order. 3. Ask the scheduler which PID runs. 4. Count a context 
switch if the PID changed and neither side is idle. 5. Record the timeline entry. 6. Run it: 
set `start` if this is its first tick, subtract 1 from remaining, and if remaining hits 0 set 
completion to `tick + 1`. 7. End-of-tick bookkeeping: decrement quantum, update vruntime, 
requeue if the quantum ran out.

Repeat until every process has finished.

---

## 9. Output files

Everything goes into `output/`, flat, so `plot.py` can glob for files instead of being 
configured.

| File | Name | ---|---|
| Timeline | `<algo>__<trace>__timeline.csv` | Per process | `<algo>__<trace>__processes.csv` 
| | Summary | `<algo>__<trace>__summary.csv` | Sweep | `sweep__t5_mixed.csv` |

Separator is a double underscore. `<trace>` is the filename without `.txt`, so `t1_basic`. 
`<algo>` is one of:

`fcfs`, `sjf`, `srtf`, `rr`, `priority_np`, `priority_p`, `mlq`, `mlfq`, `cfs`

These same strings are used by `--algo` and in the `algo` column.

**Columns**

Timeline, one row per tick with no gaps. `-1` means idle:

``` tick,pid ```

Per process, one row per process, ordered by PID:

``` pid,arrival,burst,priority,nice,queue,start,completion,turnaround,waiting,response ```

Summary, exactly one row:

``` 
algo,trace,quantum,n_processes,total_ticks,busy_ticks,idle_ticks,avg_turnaround,avg_waiting,avg_response,throughput,cpu_utilisation,context_switches,max_waiting,stddev_waiting 
```

`quantum` holds the real quantum for `rr` and 0 for everything else. MLQ and MLFQ have 
per-queue quanta, so there is no single number to report.

Sweep uses the same columns as the summary, one row per quantum from 1 to 12.

**Formatting.** These exist so two runs produce byte-identical files.

- Header row on every file, comma separated, no spaces. - LF line endings, no trailing spaces, 
one newline at the end. - Integers with `%d`. - All floats with `%.6f`. Never scientific 
notation. - Never call `setlocale`, so the decimal point is always a dot.

---

## 10. Command line

``` ./scheduler --algo <name> --trace <file> [--quantum N] ./scheduler --all ./scheduler 
--sweep --trace <file> ```

- `--quantum` works with `rr` only. Using it with anything else is an error, not a silent 
no-op. - `--all` runs every algorithm on every trace, with Round Robin at quantum 4. - 
`--sweep` writes only the sweep summary. It does not write timeline or per-process files, 
because all twelve runs would collide on the same filename.

---

## 11. The traces

| Trace | Size | Purpose | Hand-verified | ---|---|---|---|
| `t1_basic.txt` | 5 | All arrive at 0, distinct bursts | yes | `t2_convoy.txt` | 6 | Long job 
| first, short jobs behind it | yes | `t3_starvation.txt` | 12 | Short jobs streaming past one 
| long job | no | `t4_priority_skew.txt` | 10 | Constant high-priority arrivals, one 
| low-priority job | yes | `t5_mixed.txt` | 20 | Main comparison workload, sweep target | no | 
| `t6_interactive.txt` | 15 | Many short bursts, a few long ones | no |

The three hand-verified traces have expected results committed under `tests/expected/`.

---

## 12. Checks

**Golden files.** The three hand-verified traces are diffed against live output by 
`tests/run_tests.sh`. Any difference is a failure.

**Invariants**, asserted at the end of every run:

1. Executed ticks across all processes equals the sum of all bursts. 2. Total ticks equals 
busy plus idle. 3. Nothing runs before it arrives. 4. Two processes never run in the same 
tick. 5. completion ≥ arrival + burst for everyone. 6. turnaround = completion − arrival, and 
waiting = turnaround − burst. 7. 0 ≤ response ≤ waiting for everyone. 8. Everything finishes. 
9. MLFQ only: a process never drops a level except on a tick where its quantum hit zero.

**Cross-checks between algorithms:**

- Round Robin with a quantum at least as large as the longest burst gives the same timeline as 
FCFS. - When everything arrives at tick 0, SJF's average waiting is no worse than FCFS's. SJF 
is provably optimal in that case, so a violation is a bug. - SRTF's average waiting is no 
worse than SJF's on any trace with staggered arrivals. - MLFQ with one queue and a fixed 
quantum behaves like Round Robin. - MLQ with everything in one queue behaves like that queue's 
policy. - CFS with all nice values at 0 converges toward equal shares.

**Determinism.** Running the whole suite twice into different directories and diffing produces 
nothing. If it does, an undocumented tie-break is leaking.

Nothing random, time-based, or address-based may affect a scheduling decision. `qsort` is not 
stable, so it must not be used to order the CFS ready set unless the comparator is made total 
by adding the tie-break.

---

## 13. Decisions this file makes

`PROJECT.md` did not pin these down. They are pinned here, and they affect golden file 
contents.

| Question | Answer | ---|---|
| How many decimals in float output? | six, fixed notation | Population or sample standard 
| deviation? | population, divide by N | Algorithm name strings | the nine in Section 9 | 
| `quantum` column for non-RR algorithms | 0 | Order of events inside a tick | Section 8 | 
| First MLFQ boost | tick 50, not tick 0 | Boost versus "keeps remaining quantum" | boost wins 
| and resets both | Does the running process compete against challengers? | yes, for SRTF, 
| Priority-P, and CFS | Does CFS arrival placement look at the running process? | yes | Two 
| processes arriving on the same tick | admitted in PID order | Idle ticks before the first 
| arrival | counted in total ticks | Does `--sweep` write timelines? | no | Smallest legal 
| burst | 1 | Idle marker | -1 |

