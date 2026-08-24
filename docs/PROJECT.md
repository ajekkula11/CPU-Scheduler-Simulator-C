# CPU Scheduler Simulator (C)

---

## 1\. Project Crux / Introduction

This project is a command-line CPU scheduler simulator written in C. It models a single CPU core executing a fixed set of processes under nine different scheduling algorithms, records exactly what ran at every time unit, and computes standard scheduling metrics for each run. It emits machine-readable CSV, and a small Python script turns that CSV into Gantt charts and comparison plots. A written analysis accompanies the output and explains why the algorithms differ, where each one breaks down, and which classic anomalies show up in the data.

**Core problem it solves.** CPU scheduling is normally taught as a set of hand-worked examples on small process tables. Those examples are too small to show the interesting behaviour: convoy effects, starvation, quantum sensitivity, and fairness under load. This project builds a reproducible harness that runs the same workloads through every algorithm so the differences become measurable.

**What this project is for.** This is a proof of understanding of CPU scheduling theory. The measure of success is whether the implementation is theoretically correct and whether the resulting behaviour can be explained from first principles.

**Elevator pitch.** A C simulator that runs nine CPU scheduling algorithms over identical workloads and produces the metrics, charts, and written analysis needed to explain how and why they diverge.

---

## 2\. Goals & Success Criteria

### Primary goal (measurable)

Simulate all nine algorithms (FCFS, SJF non-preemptive, SRTF, Round Robin, Priority non-preemptive, Priority preemptive, MLQ, MLFQ, simplified CFS) against a fixed suite of at least six hand-written workload traces, and produce for every algorithm-trace pair: a complete execution timeline, a per-process metrics table, and an aggregate metrics row, all written to CSV.

### Success criteria

1. **Correctness against hand calculation.** For the three designated traces, simulator output matches results computed by hand, process by process, with zero discrepancies. Correctness is a criterion for "done," not only a testing activity.  
     
2. **Determinism.** Running the full suite twice produces byte-identical CSV output. Tie-breaking rules are documented, not accidental.  
     
3. **One-command reproduction.** A stranger can clone the repo, run `make && ./scheduler --all` and then `python3 tools/plot.py`, and get every CSV and every chart without editing the source.  
     
4. **Explained anomalies.** The analysis document identifies and explains, with reference to specific numbers in the generated CSVs, at least six named phenomena: the FCFS convoy effect, starvation under SJF/SRTF, starvation under strict priority, Round Robin quantum sensitivity, MLFQ starvation of low-priority queues, and CFS fairness convergence across unequal weights.  
     
5. **Comparative output.** At least four comparison charts exist: average waiting time by algorithm, average turnaround time by algorithm, average response time by algorithm, and Round Robin metrics as a function of quantum.

### Non-goals

- This is not a kernel, a kernel module, or a patch to a real scheduler.  
- This is not a performance-optimised simulator. Clarity of code beats speed.  
- This is not a GUI application or a web application.  
- This is not a real-time scheduling study. EDF, rate-monotonic, and deadline algorithms are excluded.  
- This is not multicore. One CPU, always.  
- This is not a presentation artifact. Visual polish is not a goal in itself. Charts are included only where they carry an argument that a table cannot.

---

## 3\. Scope Definition

### In-scope

**Simulation engine**

- Single CPU core, discrete integer time units ("ticks"), starting at t=0.  
- Processes defined by: PID, arrival time, CPU burst length, priority, nice value (for CFS), and queue assignment (for MLQ).  
- Preemption decisions evaluated at tick boundaries only.  
- Full execution timeline recorded: which PID held the CPU during each tick, and idle ticks recorded explicitly.

**Algorithms (all nine)**

1. FCFS  
2. SJF, non-preemptive  
3. SRTF (shortest remaining time first, preemptive SJF)  
4. Round Robin, with configurable quantum  
5. Priority, non-preemptive  
6. Priority, preemptive  
7. MLQ (multilevel queue, explicit queue assignment from the trace file, fixed per-queue policy)  
8. MLFQ (multilevel feedback queue, demotion on quantum exhaustion, periodic boost)  
9. CFS, simplified: vruntime accounting with weights, ready set held in a sorted array

**Metrics, per process and aggregate**

- Completion time, turnaround time, waiting time, response time  
- Averages for turnaround, waiting, and response  
- Throughput (processes completed per unit time)  
- CPU utilisation (busy ticks over total elapsed ticks)  
- Context switch count  
- Fairness indicators: max waiting time, and standard deviation of waiting time across processes

**Input and output**

- Workload traces as plain text files in the repo, hand-written and version-controlled.  
- Three CSV outputs per run: timeline, per-process metrics, aggregate metrics.  
- A Python script that reads the CSVs and renders Gantt charts and comparison plots to PNG.

**Documentation**

- README with build instructions, run instructions, and trace file format.  
- ANALYSIS.md containing the written interpretation, anomaly explanations, and embedded charts.  
- Documented tie-breaking, preemption, and priority-ordering rules.

### Out-of-scope

The following will not be built. If any of these are wanted later, they are a new phase and a new scope decision.

- Multicore or SMP scheduling, load balancing, CPU affinity  
- I/O bursts, blocking, waiting queues, device simulation  
- Context switch cost modelling. Switches are counted but cost zero ticks.  
- Aging or priority boosting in the Priority algorithms (MLFQ boost is in scope; Priority aging is not)  
- Full CFS fidelity: red-black tree, the real `sched_prio_to_weight` table, min\_granularity, sched\_latency tuning, group scheduling. This is specified in full in Section 11 as proposed work. It is not part of the locked scope.  
- Real-time algorithms: EDF, rate-monotonic, deadline scheduling  
- Lottery, stride, or fair-share algorithms beyond CFS  
- Interactive or step-through debugging modes  
- A GUI, a TUI, a web front end, or animation  
- Random workload generation (traces are hand-written and fixed)  
- Executing real processes, threads, or measuring real hardware  
- Memory management, paging, or virtual memory interaction  
- Cross-platform installers or packaging

### Assumptions

Numeric values for everything below are fixed in **Section 4**, not chosen at implementation time.

1. Single CPU core.  
2. All processes are CPU-bound. No process ever blocks.  
3. All values in a trace file are non-negative integers. So are the quantum and the boost interval.  
4. A process that arrives at tick `t` is eligible to run during tick `t`.  
5. Preemption is evaluated only at tick boundaries, never mid-tick.  
6. **Priority convention:** a lower priority number means higher priority. Priority 0 is the most urgent. This applies to both Priority algorithms and to any MLQ or MLFQ tie-break that consults priority.  
7. **Tie-breaking rule (global):** when two processes are equally eligible, the one with the earlier arrival time wins; if arrival times are equal, the lower PID wins. This rule is applied uniformly and documented in the README, because it is the usual reason simulator output disagrees with a textbook.  
8. **Preemption on strict improvement only:** a running process is preempted only when the challenger is strictly better on the algorithm's own key, never when it merely ties. A tie leaves the incumbent running. This is the standard convention and it is also what stops CFS and SRTF from switching every tick between equal candidates for no benefit.  
9. **Round Robin ordering rule:** on a quantum expiry at tick `t`, a process arriving at tick `t` is enqueued before the preempted process is re-enqueued. This is documented explicitly, since the opposite convention changes results.  
10. **Preemption never costs a process its place or its quantum.** A process removed from the CPU by something other than its own quantum expiry (a higher-priority arrival, a higher queue becoming non-empty, an MLFQ boost) returns to the **head** of its queue and resumes with its **remaining quantum**, not a fresh one. Losing the CPU is not the same as using up a turn, and treating it as one would penalise a process for someone else's arrival. Quantum expiry is the only thing that costs a full turn.  
11. **Context switch cost:** switches cost zero ticks. They are counted for reporting only. This is the single largest departure from reality in the model, and the analysis must say so rather than let the reader assume the numbers include overhead.  
12. MLQ performs no migration between queues. Queue assignment is static and comes from the trace file. Migration is MLFQ's job and MLFQ's alone.  
13. Development on Windows, building through WSL2 with gcc. Everything must also compile with plain gcc on Linux.  
14. This is a proof-of-understanding exercise in operating systems theory. Theoretical correctness and explainability outrank performance, polish, and presentation in every design trade-off.

---

## 4\. Parameters & Fixed Constants

Every tunable number and every ambiguous rule in the project is fixed here. Nothing in this section is decided during implementation. If one of these values needs to change, it changes here first and the change is noted, because these values are baked into the golden test files.

### 4.1 Engine

| Parameter | Value | Notes |
| :---- | :---- | :---- |
| Tick | 1 abstract time unit | No mapping to real milliseconds. Consistency matters, realism does not. |
| Simulation start | t \= 0 |  |
| Context switch cost | **0 ticks** | Counted, never charged. Not configurable in this version. |
| Priority ordering | Lower number \= higher priority | Valid range 0 to 9, where 0 is most urgent. |
| Preemption threshold | Strict improvement only | Ties leave the incumbent running (assumption 8). |
| Response time | `first_run_tick - arrival_time` | May be 0\. Measured on first dispatch only, never on resumption. |
| Throughput | `processes_completed / total_elapsed_ticks` |  |
| CPU utilisation | `busy_ticks / total_elapsed_ticks` | With zero switch cost, this is `1 - idle_fraction`. |

### 4.2 Round Robin

| Parameter | Value |
| :---- | :---- |
| Default quantum | 4 ticks |
| Quantum sweep range | 1 to 12 inclusive, step 1 |
| Sweep trace | `t5_mixed.txt` only. One sweep, on the main comparison workload. |
| Requeue order | Arrivals at tick `t` enqueue before the process preempted at tick `t` |
| Quantum on resumption | A process that exhausts its quantum goes to the tail with a fresh quantum. Round Robin has no other preemption source, so assumption 10 never fires here. |

### 4.3 MLQ (multilevel queue)

| Parameter | Value |
| :---- | :---- |
| Number of queues | 3: Q0 (highest), Q1, Q2 (lowest) |
| Queue assignment | Explicit `queue` column in the trace file, values 0 to 2 |
| Q0 policy | Round Robin, quantum 2 |
| Q1 policy | Round Robin, quantum 4 |
| Q2 policy | FCFS |
| Between queues | Strict priority, preemptive. An arrival into a higher queue preempts a running process from a lower queue. |
| **Resumption after preemption** | The preempted process returns to the **head** of its own queue with its **remaining quantum**. It resumes the moment its queue is the highest non-empty one again. This is the standard convention: preemption by a higher queue is not a turn taken. |
| Migration | None. Assignment is static for the life of the process. |

### 4.4 MLFQ (multilevel feedback queue)

| Parameter | Value |
| :---- | :---- |
| Number of queues | 3: Q0 (highest), Q1, Q2 (lowest) |
| Entry queue | All processes enter at Q0 on arrival |
| Q0 quantum | 2 ticks |
| Q1 quantum | 4 ticks |
| Q2 quantum | 8 ticks (Round Robin at the floor, not FCFS) |
| Demotion rule | On quantum **exhaustion**, drop one level and receive a fresh quantum at the new level. A process already in Q2 stays in Q2. |
| **Preemption rule** | A process preempted by a higher-queue arrival or by a boost keeps its level and its **remaining quantum**, and returns to the **head** of its queue. Only exhaustion demotes. |
| Boost interval `B` | Every **50 ticks**, all processes move to Q0 and receive a fresh Q0 quantum. Chosen over 30 so that a process can reach Q2 and stay there long enough for starvation to be visible. |
| Between queues | Strict priority, preemptive. Boost and arrival both trigger re-evaluation. |

### 4.5 CFS (simplified)

| Parameter | Value |
| :---- | :---- |
| Nice range supported | \-5 to \+5 |
| Weight table | Real Linux values for that subrange: \-5:3121, \-4:2501, \-3:1991, \-2:1586, \-1:1277, 0:1024, 1:820, 2:655, 3:526, 4:423, 5:335 |
| `NICE_0_LOAD` | 1024 |
| **vruntime accounting** | Updated **per tick of execution**, at the end of each tick the process ran: `vruntime += delta_exec * 1024 / weight` with `delta_exec = 1`. This is the standard accounting point and the simplest: one update, one place, no reconciliation on preemption. |
| vruntime type | 64-bit unsigned integer, truncating division |
| New arrival placement | vruntime set to the current minimum vruntime in the ready set. If the ready set is empty, 0\. |
| Ready structure | Array kept sorted by vruntime, ties broken by the global rule (arrival, then PID) |
| Re-evaluation | Every tick, subject to assumption 8: the lowest-vruntime process preempts only if strictly lower. No `min_granularity` in the simplified version. |

The truncating division is a known source of drift and must be named as a simplification in `docs/CFS_NOTES.md`, since removing exactly that rounding error is the reason the real implementation uses an inverse-weight table.

### 4.6 Trace file format

Whitespace-separated, one process per line. Lines beginning with `#` are comments. All six fields are always present, even for algorithms that ignore some of them. This keeps one trace usable by all nine algorithms.

\# pid  arrival  burst  priority  nice  queue

  1    0        7      2         0     0

  2    2        4      0        \-2     0

  3    4        9      5         3     2

| Field | Used by |
| :---- | :---- |
| `pid` | All. Also the final tie-break. |
| `arrival` | All |
| `burst` | All |
| `priority` | Priority-NP, Priority-P |
| `nice` | CFS |
| `queue` | MLQ only |

### 4.7 Trace suite

| Trace | Size | Purpose |
| :---- | :---- | :---- |
| `t1_basic.txt` | 5 processes | All arrive at 0, distinct bursts. Baseline sanity. **Hand-verified.** |
| `t2_convoy.txt` | 6 processes | One long burst arriving first, several short bursts behind it. Exposes the FCFS convoy effect. **Hand-verified.** |
| `t3_starvation.txt` | 12 processes | Steady stream of short jobs plus one long job. Exposes SJF and SRTF starvation. |
| `t4_priority_skew.txt` | 10 processes | High-priority jobs arriving continuously, one low-priority job. Exposes priority starvation. **Hand-verified.** |
| `t5_mixed.txt` | **20 processes** | Staggered arrivals, mixed bursts, priorities, nice values, and queue assignments. Main comparison workload and the quantum sweep target. |
| `t6_interactive.txt` | 15 processes | Many very short bursts plus a few long ones. Exposes response time differences and shows MLFQ and CFS at their best. |

The three marked traces are the ones hand-computed in Phase 0 and checked by success criterion 1\.

### 4.8 Output files and formats

Written to `output/`. One directory, flat, predictable names, so `plot.py` can glob rather than be configured.

| File | Name pattern | Columns |
| :---- | :---- | :---- |
| Timeline | `<algo>__<trace>__timeline.csv` | `tick, pid` where `pid = -1` means idle |
| Per-process | `<algo>__<trace>__processes.csv` | `pid, arrival, burst, priority, nice, queue, start, completion, turnaround, waiting, response` |
| Aggregate | `<algo>__<trace>__summary.csv` | `algo, trace, quantum, n_processes, total_ticks, busy_ticks, idle_ticks, avg_turnaround, avg_waiting, avg_response, throughput, cpu_utilisation, context_switches, max_waiting, stddev_waiting` |
| Sweep | `sweep__t5_mixed.csv` | one row per quantum, same columns as the aggregate file |

One row per tick in the timeline file. Verbose, but it makes the Gantt chart trivial to draw and the file trivial to diff, which is what the golden tests need.

---

## 5\. Recommended Tech Stack

### Preferred stack

| Layer | Choice | Rationale |
| :---- | :---- | :---- |
| Simulator | C (C99), no third-party libraries | The point of the project is to demonstrate OS understanding in C. Standard library only keeps the build trivial and the code auditable. |
| Build | GNU Make | One Makefile, one command. No CMake, because there is nothing to configure. |
| Charts | Python 3 \+ matplotlib | C has no reasonable plotting story. Keeping plotting out of C keeps the C code focused on scheduling. |
| Data interchange | CSV | Human-readable, diffable in git, trivially parsed on both sides. |
| Build environment | WSL2 (Ubuntu) on the existing Windows machine | Real gcc, real make, matches what a reviewer will use. |
| Version control | Git and GitHub | Already decided. |

Python dependency footprint is deliberately one package: matplotlib. CSV reading uses the standard library `csv` module. No pandas, no numpy unless a chart genuinely needs it.

**Verdict:** Python. The Gantt chart is the most important visual in the project.

### Required tools

Minimal set, nothing else: gcc, make, git, python3, matplotlib. Optional during development, not required to run: valgrind (leak checking), clang-format (consistent style).

---

## 6\. Phased Implementation Plan

Effort estimates assume focused solo work and are given in working hours.

### Phase 0: Setup and Specification

**Goal.** Fix the toolchain and lock down the definitions before writing scheduling logic, so no algorithm has to be rewritten because a metric or a tie-break rule changed.

**Deliverables**

- WSL2 \+ gcc \+ make \+ python3 \+ matplotlib installed and verified  
- Git repo initialised, pushed to GitHub, with README stub and `.gitignore`  
- `docs/SPEC.md`: the trace file format and every rule and constant from Sections 3 and 4, restated as implementation specification  
- Six workload traces written by hand in `traces/`, matching the sizes and purposes in Section 4.7  
- The three marked traces worked out by hand for FCFS and SJF, saved as `tests/expected/`

**Estimated effort.** 6 to 8 hours **Dependencies.** None **Exit criteria.** `docs/SPEC.md` defines every metric formula, every tie-break rule, the priority convention, the preemption and resumption rules, and every constant in Section 4\. All six traces exist in the fixed six-field format, and three have hand-computed expected results committed.

---

### Phase 1: Core Foundation

**Goal.** A working simulation engine with one algorithm end to end, proving the architecture.

**Deliverables**

- Trace file parser reading the six-field format into a process table  
- Simulation engine: the tick loop, timeline recording, and completion detection  
- Scheduler interface (a struct of function pointers) that every algorithm will implement  
- FCFS implemented against that interface  
- Metrics calculator producing per-process and aggregate values  
- CSV writer producing all three output files in the Section 4.8 formats  
- Working `Makefile`

**Estimated effort.** 12 to 16 hours **Dependencies.** Phase 0 **Exit criteria.** `./scheduler --algo fcfs --trace traces/t1_basic.txt` produces three CSVs whose numbers match the Phase 0 hand calculation exactly. The scheduler interface is stable enough that adding an algorithm requires no engine changes.

---

### Phase 2: Classic Algorithms

**Goal.** Five more algorithms, including the first preemptive ones.

**Deliverables**

- SJF non-preemptive  
- SRTF  
- Round Robin with `--quantum` flag  
- Priority non-preemptive  
- Priority preemptive  
- Preemption support in the engine if Phase 1 did not already cover it  
- Context switch counting (count only, no cost)

**Estimated effort.** 14 to 18 hours **Dependencies.** Phase 1 **Exit criteria.** All six algorithms run on all six traces. Two structural checks pass: Round Robin with a quantum larger than the longest burst produces the same timeline as FCFS, and SJF produces an average waiting time no higher than FCFS on `t1_basic` where all processes arrive at 0\.

---

### Phase 3: Multilevel and CFS

**Goal.** The three harder algorithms, which are the ones that actually distinguish this project from a typical coursework submission.

**Deliverables**

- MLQ: three queues per Section 4.3, static assignment read from the `queue` column, per-queue policy as specified, strict preemptive priority between queues, head-of-queue resumption with remaining quantum  
- MLFQ: three queues per Section 4.4, entry at Q0, demotion on quantum exhaustion only, remaining-quantum resumption on preemption, boost every 50 ticks  
- CFS (simplified): per Section 4.5, vruntime updated per executed tick  
- `docs/CFS_NOTES.md` stating clearly what was simplified versus real Linux CFS and why

**Estimated effort.** 18 to 24 hours **Dependencies.** Phase 2 **Exit criteria.** All nine algorithms run on all six traces without crashes. On `t6_interactive`, CFS shows visibly lower waiting-time standard deviation than FCFS. MLFQ demotion and boost events are visible in the timeline CSV, and a preempted process is observably not demoted. MLQ shows a low-queue process being preempted by a high-queue arrival and later resuming at the head of its queue. The CFS simplification note is written.

---

### Phase 4: Visualization

**Goal.** Turn the CSVs into the charts that make the differences legible.

**Deliverables**

- `tools/plot.py` reading the CSV outputs  
- Gantt chart per algorithm-trace pair, with idle ticks visually distinct from running ticks  
- Comparison bar charts: average waiting, average turnaround, average response, all nine algorithms side by side  
- Round Robin quantum sweep chart on `t5_mixed`, quanta 1 to 12, plotting average waiting, turnaround, and response time, plus context switch count on a secondary axis. The switch count is what carries the tradeoff argument, since with zero switch cost the timing curves alone will not show it.  
- All charts written to `output/charts/` as PNG

**Estimated effort.** 8 to 12 hours **Dependencies.** Phase 3 **Exit criteria.** `python3 tools/plot.py` regenerates every chart from scratch with no manual steps and no hardcoded paths outside a config block at the top of the file.

---

### Phase 5: Testing, Analysis, and Documentation

**Goal.** Make it correct, make it reproducible, make it explainable.

**Deliverables**

- Test script comparing current output against committed golden files  
- Invariant checks (see Section 9\)  
- `ANALYSIS.md`: the written interpretation, covering all six required phenomena, referencing specific generated numbers and embedding the charts  
- `README.md`: what it is, how to build, how to run, trace format, results summary, limitations  
- Repo cleanup, consistent formatting, final push

**Estimated effort.** 12 to 16 hours **Dependencies.** Phase 4 **Exit criteria.** All Section 2 success criteria are met **and** the Section 9 definition of "working correctly" holds in full. A clean clone builds and runs without edits.

---

**Total estimated effort:** 70 to 94 hours.

---

## 7\. Feature Prioritization (MoSCoW)

Only **Must** items are in the locked initial scope.

### Must (initial scope)

- Tick-based single-core simulation engine  
- Trace file parsing, six-field format  
- All nine algorithms: FCFS, SJF-NP, SRTF, RR, Priority-NP, Priority-P, MLQ, MLFQ, simplified CFS  
- Per-process metrics: completion, turnaround, waiting, response  
- Aggregate metrics: averages, throughput, CPU utilisation, context switch count, max waiting time, waiting time standard deviation  
- Timeline recording and CSV output in the Section 4.8 formats  
- Six hand-written traces per Section 4.7  
- Gantt charts and comparison charts via Python  
- Round Robin quantum sweep, quanta 1 to 12, on `t5_mixed`  
- Golden-file regression tests and invariant checks  
- README, SPEC, CFS simplification notes, ANALYSIS

### Should (only after all Must items are complete)

- A summary table of all algorithms across all traces in one CSV

### Could

Deferred items are recorded in Section 11\. Nothing there is approved.

### Won't (this version)

The out-of-scope list in Section 3 is the authoritative Won't list and is not duplicated here.

---

## 8\. High-Level Design / Architecture

### Component overview

traces/\*.txt

     |

     v

\[ Trace Loader \] \---\> Process Table (array of struct process)

                              |

                              v

                     \[ Simulation Engine \]  \<----\>  \[ Scheduler \]

                        (the tick loop)              (one of nine,

                              |                    behind one interface)

                              v

                     \[ Timeline Recorder \]

                        (RUN / IDLE)

                              |

                              v

                     \[ Metrics Calculator \]

                              |

                              v

                      \[ CSV Writer \]  \---\>  output/\*.csv

                                                  |

                                                  v

                                       tools/plot.py  \---\>  output/charts/\*.png

### Key design decision: one scheduler interface

Every algorithm implements the same small struct of function pointers. The engine never knows which algorithm it is running. This is what keeps nine algorithms from becoming nine tangled special cases inside one loop.

typedef struct scheduler {

    const char \*name;

    void (\*init)(void \*state, config\_t \*cfg);

    void (\*on\_arrival)(void \*state, process\_t \*p, int tick);

    int  (\*pick\_next)(void \*state, int current\_pid, int tick);

    void (\*on\_tick\_end)(void \*state, int running\_pid, int tick);

    void (\*on\_complete)(void \*state, process\_t \*p, int tick);

    void (\*destroy)(void \*state);

    void \*state;

} scheduler\_t;

`pick_next` returns the PID that should run during the next tick, or `IDLE` if nothing is ready. Preemption falls out naturally: a preemptive scheduler simply returns a different PID than the one currently running.

**Context switches are counted in the engine, not in the schedulers.** The engine observes that `pick_next` returned a different real PID and increments the counter. No algorithm implementation is aware of switching as a concept. This keeps the counter consistent across all nine, and means that if switch cost is ever promoted from Section 11, it is one change in one file.

**Quantum bookkeeping lives inside each scheduler's state, not in the engine.** RR, MLQ, and MLFQ each track remaining quantum for the running process. This is what makes assumption 10 implementable: the scheduler knows the difference between "I was preempted" and "I exhausted my quantum," and the engine does not need to.

### The tick loop

tick \= 0

current\_pid \= IDLE

while (completed \< total\_processes):

    admit all processes with arrival\_time \== tick   \-\> scheduler.on\_arrival()

    next\_pid \= scheduler.pick\_next(current\_pid, tick)

    if next\_pid \!= current\_pid and next\_pid \!= IDLE and current\_pid \!= IDLE:

        context\_switches \+= 1

    record timeline\[tick\] \= next\_pid

    if next\_pid \!= IDLE:

        if first time running: response\_time \= tick \- arrival\_time

        remaining\[next\_pid\] \-= 1

        if remaining\[next\_pid\] \== 0:

            completion\_time \= tick \+ 1

            scheduler.on\_complete()

    scheduler.on\_tick\_end(next\_pid, tick)

    current\_pid \= next\_pid

    tick \+= 1

`on_tick_end` is where a scheduler decrements the running process's quantum, updates its vruntime, or fires a boost. Keeping all per-tick bookkeeping in one callback is what stops the engine from growing algorithm-specific branches.

### Data structures per algorithm

| Algorithm | Ready structure | Selection rule |
| :---- | :---- | :---- |
| FCFS | FIFO queue | Head of queue, never preempt |
| SJF-NP | Array scan | Min burst among arrived, never preempt |
| SRTF | Array scan | Min remaining time, re-evaluated every tick, strict improvement to preempt |
| RR | FIFO queue \+ quantum counter | Head; requeue at tail on quantum expiry |
| Priority-NP | Array scan | Lowest priority number among arrived, never preempt |
| Priority-P | Array scan | Lowest priority number, re-evaluated every tick, strict improvement to preempt |
| MLQ | 3 FIFO queues | Highest non-empty queue, that queue's own policy, no migration, head-of-queue resumption |
| MLFQ | 3 FIFO queues \+ boost timer | Highest non-empty queue; demote on exhaustion only; boost all every 50 ticks |
| CFS | Sorted array by vruntime | Lowest vruntime; `vruntime += delta * 1024 / weight` per executed tick |

Linear array scans are used deliberately. Process counts are small, and an O(n) scan that is obviously correct is worth more here than a heap that is not.

### Repository layout

cpu-scheduler-sim/

  Makefile

  README.md

  src/

    main.c            CLI parsing, orchestration

    process.h/.c      process struct, trace loader

    engine.h/.c       tick loop, timeline recorder

    metrics.h/.c      metric computation

    csv.h/.c          output writers

    sched/

      sched.h         the scheduler interface

      fcfs.c  sjf.c  srtf.c  rr.c  priority.c

      mlq.c   mlfq.c  cfs.c

  traces/             six trace files

  tests/

    expected/         golden CSV files

    run\_tests.sh

  tools/

    plot.py

  docs/

    SPEC.md

    CFS\_NOTES.md

    ANALYSIS.md

  output/             generated, gitignored except final charts

### Command-line interface

./scheduler \--algo \<name\> \--trace \<file\> \[--quantum N\]

./scheduler \--all                          \# every algorithm, every trace

./scheduler \--sweep \--trace \<file\>         \# RR quantum sweep, 1 to 12

---

## 9\. Testing & Validation Strategy

### Layer 1: Hand-verified golden files

The three traces marked in Section 4.7 are computed by hand for every applicable algorithm, committed under `tests/expected/`, and diffed against live output by `run_tests.sh`. Any difference fails the test. This is the primary correctness gate and it maps directly to success criterion 1\.

### Layer 2: Invariant checks (asserted at the end of every run)

1. Sum of executed ticks across all processes equals the sum of all burst times.  
2. `total_elapsed_ticks = busy_ticks + idle_ticks`, with no tick counted twice.  
3. No process is ever scheduled before its arrival time.  
4. No two processes hold the CPU in the same tick.  
5. For every process: `completion_time >= arrival_time + burst_time`.  
6. For every process: `turnaround_time = completion_time - arrival_time` and `waiting_time = turnaround_time - burst_time`.  
7. `0 <= response_time <= waiting_time` for every process.  
8. Every process completes. No process is left unscheduled at the end of the run.  
9. For MLFQ: a process's level never drops except on a tick where its quantum reached zero. This is assumption 10 as an executable assertion, and it is the one most likely to be violated by accident.

### Layer 3: Structural cross-checks between algorithms

These verify that the algorithms relate to each other the way theory says they must.

- Round Robin with quantum \>= max burst produces the same timeline as FCFS.  
- On a trace where all processes arrive at t=0, SJF's average waiting time is less than or equal to FCFS's. SJF is provably optimal for this case, so a violation means a bug.  
- SRTF's average waiting time is less than or equal to SJF's on any trace with staggered arrivals.  
- MLFQ with a single queue and a fixed quantum behaves identically to Round Robin.  
- MLQ with all processes assigned to one queue behaves identically to that queue's policy.  
- CFS with all nice values at 0 converges toward equal CPU shares over a long trace.

### Layer 4: Determinism check

Run the full suite twice into separate directories and diff. Any difference means an unspecified tie-break is leaking through, which must be found and documented rather than papered over.

### Layer 5: Manual scenario review

For each of the six required phenomena, the corresponding trace is run, the chart is inspected, and the explanation in `ANALYSIS.md` is checked against the actual numbers rather than against expectation.

### Definition of "working correctly"

The simulator is working correctly when: all golden-file tests pass, all invariants hold for all nine algorithms on all six traces, all structural cross-checks pass, output is byte-identical across repeated runs, and every claim in `ANALYSIS.md` is traceable to a specific value in a generated CSV.

---

## 10\. Final Deliverables

1. **Source code.** A C99 codebase implementing the engine and all nine algorithms, building with a single `make`.  
2. **Workload traces.** Six committed, documented, hand-written trace files in the six-field format.  
3. **Generated data.** Timeline, per-process metrics, and aggregate metrics CSVs for every algorithm-trace pair.  
4. **Charts.** Gantt charts per run, three comparison bar charts, and the Round Robin quantum sweep, all as committed PNGs.  
5. **Plotting tool.** `tools/plot.py`, regenerating every chart from the CSVs.  
6. **Tests.** Golden expected outputs and `run_tests.sh`.  
7. **`docs/SPEC.md`.** Trace format, metric definitions, tie-break, priority, preemption, and resumption rules.  
8. **`docs/CFS_NOTES.md`.** What the simplified CFS does and how it differs from Linux CFS.  
9. **`docs/ANALYSIS.md`.** The written interpretation: algorithm comparison, the six required phenomena, and what the data shows.  
10. **`README.md`.** Build, run, and reproduce instructions, results summary, and stated limitations.  
11. **Public GitHub repository** containing all of the above.

**Repository:** `<GITHUB_LINK_TO_BE_ADDED>`

---

## 11\. Proposed Log (Deferred, Not Locked)

Everything in this section is **proposed, not approved**. Nothing here is built until the locked scope in Section 12 is complete and the item is explicitly promoted. This section exists so that ideas are recorded rather than smuggled into the current phases.

### P1. Full-fidelity CFS

**Status.** Proposed. Not in scope. Directly conflicts with Section 3, which locks CFS at the simplified level.

**Conflict note and proposed resolution.** The locked scope specifies a simplified CFS (vruntime, weights, sorted array). This item replaces that with the real thing. The minimal resolution is that the simplified CFS ships first as `cfs_simple`, the full version is added later as a **separate ninth-and-a-half algorithm** called `cfs_full`, and both remain in the codebase. That is deliberate: running both against the same trace and diffing the timelines is itself a strong proof of understanding, because it makes every simplification visible as a concrete behavioural difference. Replacing the simple version instead of adding alongside it would destroy that comparison and is not recommended.

**Goal.** Implement CFS as Linux actually implements it, close enough that the design decisions in `kernel/sched/fair.c` can be explained rather than approximated.

**Deliverables**

1. **Real weight table.** The full `sched_prio_to_weight[40]` mapping for nice \-20 through \+19, from 88761 down to 15, with `NICE_0_LOAD = 1024` at nice 0\. Plus the companion `sched_prio_to_wmult[40]` inverse-weight table used to turn division into a multiply-and-shift. This supersedes the 11-entry subrange table in Section 4.5.  
     
2. **Correct vruntime arithmetic.** `vruntime += delta_exec * NICE_0_LOAD / weight` implemented with 64-bit fixed-point using the inverse-weight table and a shift, mirroring `calc_delta_fair()` and `__calc_delta()`. Truncating integer division is not acceptable here, because the rounding error is exactly what the real implementation goes out of its way to avoid, and it is exactly what Section 4.5 accepts.  
     
3. **Red-black tree runqueue.** A self-balancing red-black tree keyed on vruntime, with the leftmost node cached so that picking the next task is O(1) rather than O(log n). This includes the insert, delete, and rebalance logic written by hand, since using a library tree defeats the purpose of the exercise.  
     
4. **min\_vruntime tracking.** A monotonically non-decreasing `min_vruntime` on the runqueue, advanced as tasks run and complete, used as the reference point for all vruntime normalisation.  
     
5. **place\_entity for arriving and waking tasks.** New tasks are placed relative to `min_vruntime`, not at zero, so that a newly arrived task cannot monopolise the CPU by virtue of having a low vruntime. Includes the start-of-life penalty for forks (`START_DEBIT`) and the sleeper credit path, with `GENTLE_FAIR_SLEEPERS` behaviour documented even if the sleeper path is inert under the no-I/O assumption.  
     
6. **Scheduling period and granularity.**  
     
   - `sched_latency` (target latency, real default 6ms scaled to simulator ticks)  
   - `min_granularity` (real default 0.75ms scaled to ticks)  
   - Period computation: `period = max(sched_latency, nr_running * min_granularity)`  
   - Ideal slice per task: `slice = period * weight / total_runqueue_weight`  
   - Preemption when `delta_exec > slice`, mirroring `check_preempt_tick()`

   

7. **Runqueue load tracking.** Running total of the weights of all runnable tasks, maintained on enqueue and dequeue, since every slice computation depends on it.  
     
8. **Wakeup preemption.** `check_preempt_wakeup()` semantics with `wakeup_granularity`: a waking task preempts the current task only if its vruntime advantage exceeds the granularity threshold. Under the no-I/O assumption this applies to arrivals rather than wakeups, and that substitution must be stated explicitly in the notes.  
     
9. **`docs/CFS_FULL_NOTES.md`.** A side-by-side account of `cfs_simple` versus `cfs_full`: what each simplification cost, and which specific traces expose the difference.

**Explicitly still excluded, even in this item**

- Group scheduling and `task_group` hierarchies (cgroups)  
- Multicore load balancing, `select_task_rq_fair()`, and NUMA balancing  
- PELT load tracking (`load_avg`, `runnable_avg`, `util_avg`)  
- `SCHED_IDLE` and `SCHED_BATCH` policies  
- Bandwidth control (`cfs_bandwidth`, quota and period throttling)  
- EEVDF, which replaced the classic CFS placement logic in newer kernels. If the intent later becomes "model what Linux does today rather than the textbook CFS," that is a separate item and a separate conversation.

**Estimated effort.** 22 to 30 hours. The red-black tree alone is 8 to 12 of those, and it is where the bugs will be.

**Dependencies.** Phase 5 complete. Specifically, the golden-file harness must already exist, because a hand-written red-black tree without regression tests is not a good use of time.

**Exit criteria**

1. Red-black tree invariants (root black, no red node with a red child, equal black-height on all paths) are asserted after every insert and delete across the full trace suite.  
2. With all tasks at nice 0, long-run CPU shares converge to equal within 2 percent.  
3. With one task at nice 0 (weight 1024\) and one at nice 5 (weight 335), the measured CPU share ratio matches 1024:335 within 5 percent over a long trace.  
4. `cfs_full` and `cfs_simple` are run on every trace and the timeline differences are catalogued and explained, not merely observed.  
5. Selecting the next task is demonstrably O(1) via the cached leftmost pointer, verified by instrumentation rather than by claim.

### P2. Aging in the Priority algorithms

Proposed. Adds a configurable aging increment so that a starving process eventually rises in priority, turning the starvation demonstration into a starvation-and-remedy demonstration. Estimated 4 to 6 hours. Low risk.

### P3. Non-zero context switch cost

**Status.** Proposed. Considered for promotion and deliberately left deferred.

Makes switches consume ticks, so that small quanta become genuinely costly rather than merely equal. This is the single most theoretically interesting deferred item after P1, because without it the Round Robin quantum sweep cannot show a real optimum: with zero cost, smaller quanta never cost anything, so the tradeoff has to be argued from the switch-count column rather than demonstrated from the timing curves. The analysis must be explicit that it is making that argument, not showing it. Design note for whenever this is picked up: the cost belongs in the engine, not in the nine scheduler implementations. Estimated 4 to 6 hours.

### P4. I/O bursts and a blocked state

Proposed. High impact and high cost: it invalidates the "all processes are CPU-bound" assumption, changes every algorithm's ready-queue handling, and makes MLFQ and CFS behave the way they were actually designed to. Estimated 20 to 25 hours plus rework of existing algorithms. Recommend only after P1.

---

## 12\. Current Locked Scope Summary

This project is a proof of understanding of CPU scheduling theory. The locked scope is a single-core, tick-based CPU scheduler simulator written in C99 with no third-party C libraries, implementing nine algorithms (FCFS, SJF non-preemptive, SRTF, Round Robin, Priority non-preemptive, Priority preemptive, MLQ, MLFQ, and a simplified vruntime-and-weights CFS backed by a sorted array rather than a red-black tree), run against six hand-written and version-controlled workload traces in a fixed six-field format, with a 20-process trace as the main comparison workload and the quantum sweep target. All processes are CPU-bound with no I/O and no blocking; lower priority numbers mean higher priority; preemption requires strict improvement, never a tie; a process preempted by anything other than its own quantum expiry returns to the head of its queue with its remaining quantum and, in MLFQ, keeps its level; MLQ queue assignment is an explicit column in the trace file with no migration; MLFQ boosts every 50 ticks; CFS updates vruntime once per executed tick; tie-breaking follows a single documented rule (earlier arrival, then lower PID); and context switches are counted but cost zero ticks. Every tunable constant and every ambiguous rule, including per-queue quanta, the boost interval, the CFS weight table, the quantum sweep range, and the CSV output formats, is fixed in Section 4 rather than chosen during implementation. The simulator emits timeline, per-process, and aggregate metrics as CSV; a single Python script using matplotlib renders Gantt charts, three comparison bar charts, and a Round Robin quantum sweep across quanta 1 to 12\. Correctness is a success criterion in its own right and is established by hand-verified golden files on three designated traces, runtime invariants, cross-algorithm structural checks, and a determinism check. Deliverables are the source, the traces, the generated CSVs and PNGs, the test harness, a specification document, a CFS simplification note, a written analysis explaining six named scheduling phenomena, and a README, all published in one public GitHub repository. Estimated effort is 70 to 94 hours across six phases. Multicore, I/O bursts, context switch cost, aging in the Priority algorithms, real-time algorithms, random workload generation, and any graphical interface are explicitly out of scope. Full-fidelity CFS, aging, non-zero switch cost, and I/O bursts are recorded in Section 11 as proposed work and are not approved.  
