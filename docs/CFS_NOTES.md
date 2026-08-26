# CFS_NOTES.md

What the simplified CFS in this project does, what real Linux CFS does, and where the two part company.

---

## 1. The idea

Every other scheduler here picks by an external property: arrival order, burst length, a priority number. CFS picks by how much CPU a process has already had.

Each process carries a **virtual runtime**. Every tick it runs, its vruntime goes up. The process with the smallest vruntime runs next. That is the entire selection rule.

The clever part is that vruntime does not advance at the same rate for everyone. It advances in inverse proportion to the process's weight:

```
vruntime += delta_exec * NICE_0_LOAD / weight
```

A process with a large weight sees its vruntime creep up slowly, so it stays at the front of the queue longer and gets more CPU. A process with a small weight burns through vruntime quickly and yields sooner.

Weight comes from the nice value. Nice ranges from -20 to +19 in Linux; this project supports -5 to +5, which is enough to show the effect.

| nice | -5 | -4 | -3 | -2 | -1 | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| weight | 3121 | 2501 | 1991 | 1586 | 1277 | 1024 | 820 | 655 | 526 | 423 | 335 |

These are the real values from Linux's `sched_prio_to_weight` table. Each step is roughly a factor of 1.25, which was chosen so that one nice level shifts about 10% of the CPU between two competing processes.

Note what this means: **fair does not mean equal.** CFS divides CPU in proportion to weight. Equal shares are what you get when everyone has the same nice value, not what CFS aims for in general.

---

## 2. Does it work?

Two checks, both reproducible.

**Equal nice, equal shares.** Three identical processes, nice 0, burst 20 each:

```
printf '1 0 20 0 0 0\n2 0 20 0 0 0\n3 0 20 0 0 0\n' > traces/tmp.txt
./scheduler --algo cfs --trace traces/tmp.txt
```

Completion times come out at 58, 59, and 60. They finish within two ticks of each other after sixty ticks of competition.

The same effect shows in the trace suite. On `t1_basic`, where every process has nice 0, CFS has the lowest spread of waiting times of all nine algorithms: standard deviation 3.35, against 7.16 for FCFS and 5.34 for SJF.

**Different nice, proportional shares.** Nice 0 against nice +5, burst 30 each:

```
printf '1 0 30 0 0 0\n2 0 30 0 5 0\n' > traces/tmp.txt
./scheduler --algo cfs --trace traces/tmp.txt
```

The timeline reads `1 2 1 1 1 2 1 1 1 2 ...`. P1 gets three ticks for every one P2 gets. The weight ratio is 1024 to 335, which is 3.06 to 1. The scheduler was never told to do this. It falls out of the vruntime arithmetic.

---

## 3. What this implementation leaves out

Four simplifications, all deliberate.

### 3.1 No minimum granularity

Real CFS will not preempt a process that has run for less than `sched_min_granularity_ns`, typically a few milliseconds. Without that floor, a scheduler on a busy machine would switch constantly and spend more time switching than working.

This version has no floor, so a process can be preempted after a single tick. The cost is visible in the numbers: on `t5_mixed`, CFS makes **82 context switches** where FCFS makes 19. That is 4.3 times the switching for the same work.

In this simulator switches are free, so it costs nothing. On real hardware, where a switch means saving registers, swapping page tables, and losing cache lines, it would be a serious problem.

### 3.2 No sleeper fairness

Real CFS gives a process waking from sleep a small vruntime credit, so an interactive process that blocks on I/O is not punished for the time it spent waiting.

This project has no I/O at all: every process is CPU-bound from arrival to completion, so there is nothing to wake from. The mechanism has no meaning here.

### 3.3 A linear scan instead of a red-black tree

Real CFS keeps runnable processes in a red-black tree keyed by vruntime, and caches a pointer to the leftmost node so that picking the next process is O(1) and inserting is O(log n).

This version scans an array, which is O(n) per tick. With at most 20 processes that is 20 comparisons, and the cost is invisible. On a server with thousands of runnable threads it would not be.

The tree is a scalability optimisation, not a behavioural one. Both approaches pick the same process.

### 3.4 Integer division that truncates

This one is not a matter of scale. It changes the answer.

---

## 4. The truncation flaw

The update is:

```c
vruntime += NICE_0_LOAD / weight_of(nice);
```

with integer arithmetic. `delta_exec` is always 1 tick, so the increment is `1024 / weight`.

Work through what that gives:

| nice | weight | 1024 / weight | truncated |
|---|---|---|---|
| +5 | 335 | 3.06 | **3** |
| +2 | 655 | 1.56 | **1** |
| 0 | 1024 | 1.00 | **1** |
| -1 | 1277 | 0.80 | **0** |
| -5 | 3121 | 0.33 | **0** |

Every weight above 1024 truncates to **zero**. A process with a negative nice value accumulates no virtual runtime at all. Its vruntime stays at whatever it started at, forever, and it never yields the CPU.

Worse, nice +2 and nice 0 both give an increment of 1, so they are treated identically despite a 1.56x weight difference. The whole range from nice -1 to nice +2 collapses into two distinct behaviours.

### Reproducing it

```
printf '1 0 10 0 -5 0\n2 0 10 0 0 0\n' > traces/tmp.txt
./scheduler --algo cfs --trace traces/tmp.txt
```

Timeline:

```
1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2
```

One context switch in the entire run. P1 takes all ten of its ticks before P2 gets any. This is not proportional sharing. It is FCFS with extra steps.

### How real Linux avoids it

Linux does not divide. It multiplies by a precomputed reciprocal and shifts:

```c
vruntime += (delta_exec * NICE_0_LOAD * inv_weight) >> 32;
```

`inv_weight` comes from `sched_prio_to_wmult`, a second table holding `2^32 / weight` for each nice level. Because the reciprocal is scaled up by 2^32 before rounding, the fractional part survives, and the shift at the end recovers it.

The two-table approach also avoids a division instruction in the scheduler's hot path, which mattered a great deal more on the hardware CFS was written for.

The lesson generalises past scheduling: when you replace real arithmetic with integers, the error is not evenly distributed. It concentrates wherever the true value is small, and here that is exactly the high-priority processes you least want to get wrong.

---

## 5. Where CFS ranks in this project

Reading the results across all six traces, CFS is not a general-purpose winner. It is a specialist.

**Response time is where it dominates.** On `t6_interactive`, CFS has an average response of **3.00 ticks** against FCFS at 26.13, roughly nine times better. On `t5_mixed` it is **3.70** against FCFS at 37.90, more than ten times better. It gets every process onto the CPU almost immediately.

**Turnaround is where it pays for that.** On the same `t5_mixed`, CFS has an average waiting time of **45.80**, second worst of the nine, where SRTF manages 24.05. Constant interleaving means nothing finishes early.

That trade is not a defect. It is the design. An interactive desktop wants every process to feel responsive; a batch system wants jobs to finish. CFS was written for the desktop.

**One result worth explaining.** CFS has the lowest waiting-time spread on `t1_basic` at 3.35 but the **highest** on `t5_mixed` at 29.03. That looks like a contradiction for a scheduler with "fair" in its name.

It is not. Every process in `t1_basic` has nice 0, so equal shares are the fair outcome and CFS delivers them. `t5_mixed` uses varied nice values, so unequal shares are the fair outcome, and a high spread is CFS doing its job correctly. A low spread there would mean it was ignoring nice entirely.

---

## 6. CFS is no longer Linux's scheduler

Worth knowing, since the name is still in wide use.

CFS was merged in Linux 2.6.23 in October 2007, written by Ingo Molnár, replacing the O(1) scheduler. It ran as the default for sixteen years.

In Linux 6.6, released October 2023, it was replaced by **EEVDF**, Earliest Eligible Virtual Deadline First, written by Peter Zijlstra. EEVDF keeps the weight and virtual time machinery but adds two things CFS lacked:

**Lag**, a per-process measure of how much CPU it is owed relative to its fair share. A process with positive lag is behind and is eligible to run; one with negative lag has had more than its share and waits.

**Virtual deadlines**, which let a process request a shorter slice in exchange for running sooner. CFS had no way to express latency requirements, and EEVDF does.

The vruntime concept, the weight table, and the nice semantics all carried over. What changed is the selection rule: EEVDF picks the eligible process with the earliest virtual deadline rather than simply the smallest vruntime.

---

## 7. Summary

| Aspect | This project | Real CFS |
|---|---|---|
| Selection | smallest vruntime, linear scan | smallest vruntime, red-black tree |
| vruntime update | `1024 / weight`, truncating | multiply by inverse weight, shift by 32 |
| Minimum slice | none | `sched_min_granularity_ns` |
| Sleeper credit | none, no I/O exists | yes |
| Nice range | -5 to +5 | -20 to +19 |
| Status upstream | n/a | replaced by EEVDF in 6.6 |

The first three rows are the ones that change behaviour. The truncating division is the only one that makes this implementation give a wrong answer rather than a slower or less scalable one, and it is included deliberately, because working out why nice -5 monopolises the CPU teaches more than a correct implementation would.
