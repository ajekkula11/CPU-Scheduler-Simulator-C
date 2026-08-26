# ANALYSIS.md

What nine scheduling algorithms did on six workloads, and what the numbers mean.

Every figure here comes from a CSV in `output/`, produced by `./scheduler --all`. Nothing is recomputed or estimated. Where a result contradicts what theory predicts, it is reported as it came out.

---

## How to read this

All six traces are CPU-bound with no I/O, one core, and zero-cost context switches. That last point matters more than it looks: because a switch costs no ticks, **total elapsed time is identical for every algorithm on a given trace**. `t5_mixed` takes 99 ticks under all nine. CPU utilisation is 100% everywhere.

So no algorithm here can win on throughput. What they compete on is *when* each process gets served, which shows up in waiting time, response time, and the spread between the luckiest and unluckiest process.

The metrics:

- **Turnaround**: completion minus arrival. Total time in the system.
- **Waiting**: turnaround minus burst. Time spent ready but not running.
- **Response**: first dispatch minus arrival. How long until the process first gets the CPU.
- **Max waiting**: the worst-off process. This is the starvation detector.
- **Context switches**: how often the CPU changed hands.

---

## 1. The convoy effect

`t2_convoy` puts one twelve-tick process at tick 0 and five short ones behind it, arriving at ticks 1 through 5.

| Algorithm | Avg waiting | Avg response | Max waiting |
|---|---|---|---|
| SRTF | **3.83** | 1.83 | 12 |
| CFS | 7.00 | **0.83** | 12 |
| MLFQ | 7.50 | 2.33 | 12 |
| RR (q=4) | 8.67 | 6.67 | 13 |
| Priority-P | 8.83 | 6.67 | 20 |
| MLQ | 10.00 | 7.33 | 18 |
| SJF | 10.33 | 10.33 | 17 |
| FCFS | 12.00 | 12.00 | 17 |
| Priority-NP | 12.50 | 12.50 | 20 |

FCFS is worst on waiting at 12.00. SRTF is best at 3.83, a factor of 3.1.

The cause is visible in `output/gantt__t2_convoy.png`. Under FCFS the first row is a solid twelve-tick block of P1, and every short process sits behind it. P5 has a burst of 1 and waits 17 ticks to run it. Under SRTF, P1 is preempted at tick 1 and does not resume until tick 13, by which point all five short processes have finished.

**The general principle.** A non-preemptive scheduler makes an irrevocable commitment at dispatch. If it commits to a long process while short ones are still arriving, everyone behind it pays. This is why FCFS survives only in batch systems where nobody is waiting for a response.

**Note that SJF barely helps.** At 10.33 it is only 1.67 ticks better than FCFS. The reason is that P1 is already running by the time anything else arrives, and SJF is non-preemptive, so it cannot act on knowing P1 is the long one. Knowing burst lengths is worth almost nothing without the ability to preempt.

---

## 2. Starvation under SJF

`t3_starvation` streams twelve processes past one long job. This is the trace where average and worst-case pull apart.

| Algorithm | Avg waiting | Max waiting |
|---|---|---|
| Priority-P | 5.17 | 25 |
| SRTF | 5.17 | 25 |
| MLQ | 5.75 | 25 |
| MLFQ | 9.25 | 25 |
| CFS | 10.58 | 25 |
| RR | 12.58 | 25 |
| Priority-NP | 20.17 | **32** |
| SJF | 20.17 | **32** |
| FCFS | 21.17 | 27 |

SJF's average waiting is 20.17 against FCFS's 21.17. A one-tick improvement, essentially nothing.

But look at max waiting. **SJF's worst-off process waits 32 ticks; FCFS's waits 27.** SJF is worse than FCFS for the process it treats worst, despite being the algorithm that provably minimises average waiting time.

That is starvation, quantified. SJF improves the average by repeatedly favouring short jobs, and a long job that keeps losing that comparison never runs. The average hides it; the maximum does not.

**Why report max waiting at all.** If this analysis only tracked averages, SJF would look mildly better than FCFS on this trace and the starvation would be invisible. Any scheduler evaluation that reports only means is missing the failure mode that matters most to the process experiencing it.

**Also note** that Priority-NP produces numerically identical results to SJF here (20.17 waiting, 32 max, 11 switches). That is a property of this trace, where priority ordering happens to coincide with burst ordering, not a general relationship.

---

## 3. Starvation under priority scheduling

`t4_priority_skew` sends a stream of high-priority arrivals past one low-priority process. P1 arrives at tick 0 with priority 5, the least urgent value in the trace.

| Algorithm | Avg waiting | Avg response | Max waiting |
|---|---|---|---|
| SRTF | 3.80 | 1.80 | 20 |
| MLQ | 3.90 | 1.70 | 20 |
| **Priority-P** | **4.10** | **1.30** | **20** |
| SJF | 4.90 | 4.90 | 14 |
| Priority-NP | 5.10 | 5.10 | **12** |
| FCFS | 5.70 | 5.70 | **9** |
| RR | 5.90 | 5.50 | 9 |
| MLFQ | 6.40 | 2.10 | 18 |
| CFS | 8.00 | 2.10 | 14 |

Preemptive priority has the best average response of all nine at 1.30 ticks. It also has the worst max waiting at 20.

Compare the two priority variants directly. Non-preemptive: average waiting 5.10, max 12. Preemptive: average waiting 4.10, max 20. Preemption improves the average by one tick and makes the worst case 67% worse.

The mechanism is in the timeline. Under Priority-P, P1 starts at tick 0, is preempted at tick 1 by the first priority-0 arrival, and does not complete until tick 25. It waits 20 of the 25 ticks in the run. Under Priority-NP it runs to completion immediately and waits zero.

**FCFS is the fairest algorithm on this trace**, with a max waiting of 9 and the lowest standard deviation at 2.65. It ignores priority entirely, which on a workload designed to punish low-priority processes turns out to be an advantage. Ignoring information is sometimes the fair thing to do.

**The fix in real systems** is aging: raise a process's priority the longer it waits. This project does not implement aging for the priority schedulers, but MLFQ's boost is the same idea applied to queue levels, which is why MLFQ keeps its max waiting at 18 rather than 20.

---

## 4. Response time against turnaround

`t6_interactive` has many short bursts and a few long ones, which is what a desktop workload looks like.

| Algorithm | Avg waiting | Avg response | Switches |
|---|---|---|---|
| SJF | **6.87** | 6.87 | 14 |
| SRTF | 6.87 | 6.87 | 14 |
| Priority-NP | 7.00 | 7.00 | 14 |
| Priority-P | 7.40 | 6.47 | 18 |
| MLQ | 7.67 | 6.93 | 16 |
| MLFQ | 13.67 | 5.20 | 24 |
| CFS | 13.93 | **3.00** | 49 |
| RR | 17.80 | 11.87 | 22 |
| FCFS | 26.13 | 26.13 | 14 |

Two different rankings depending on which column you read.

By waiting time, SJF wins at 6.87 and CFS is seventh at 13.93. By response time, CFS wins at 3.00 and SJF is fourth at 6.87. CFS gets every process onto the CPU more than twice as fast as SJF while taking twice as long to finish them.

This is the interactive versus batch trade-off in one table. A text editor that responds in 3 ticks and finishes in 14 feels better than one that waits 7 ticks to respond and finishes in 7. A compile job feels the opposite way.

**FCFS is the clearest case.** Its waiting and response are identical at 26.13, because under FCFS a process's first tick is also its only start: once dispatched it runs to completion. Every non-preemptive algorithm shows this equality, which is a useful sanity check on the data. SJF, Priority-NP, and FCFS all have waiting equal to response on every trace in the suite.

---

## 5. What preemption costs

Switches are free in this simulator, but the counts show what a real system would pay.

`t5_mixed`, twenty processes over 99 ticks:

| Algorithm | Switches | Avg response |
|---|---|---|
| FCFS | 19 | 37.90 |
| SJF | 19 | 25.70 |
| Priority-NP | 19 | 29.50 |
| SRTF | 21 | 21.05 |
| Priority-P | 22 | 25.05 |
| RR (q=4) | 31 | 26.60 |
| MLQ | 37 | 27.75 |
| MLFQ | 47 | 7.05 |
| CFS | **82** | **3.70** |

The three non-preemptive algorithms all sit at exactly 19, which is the minimum possible: twenty processes each dispatched once, with nineteen handovers between them.

CFS makes 82 switches, 4.3 times as many, and buys a response time ten times better than FCFS. On this simulator that is free. On real hardware each switch costs register saves, a possible TLB flush, and cold cache lines, so 82 switches over 99 ticks would mean a substantial fraction of the machine spent not doing work.

This is exactly what `sched_min_granularity_ns` exists to prevent in real CFS, and its absence here is documented in `docs/CFS_NOTES.md`.

---

## 6. The Round Robin quantum

`output/sweep__t5_mixed.png` plots quanta 1 through 12 against average response and context switches.

| Quantum | Avg response | Switches |
|---|---|---|
| 1 | 3.60 | 45 |
| 2 | 6.50 | 25 |
| 3 | 9.10 | 18 |
| 4 | 26.60 | 31 |
| 8 | (rising) | 10 |
| 12 | (flat) | 9 |

Response time climbs as the quantum grows, switches fall. That is the real trade-off: a small quantum cycles through every process quickly, so everyone gets on the CPU sooner, and the cost is constant switching.

**A correction worth recording.** An earlier version of the sweep chart plotted average *waiting* against switches and claimed the two moved in opposite directions. They do not. Both fall as the quantum grows. The reason is that switches cost zero ticks here, so a small quantum buys no throughput benefit to offset the interleaving, and interleaving pushes every completion later.

Waiting and switches only oppose each other when switching has a real cost. In this simulator it does not, so the honest pairing is response against switches.

**At the high end**, quanta of 10 and above produce identical results, because every burst in the trace fits inside one slice and Round Robin has become FCFS. This is one of the structural cross-checks in `SPEC.md` section 12, and it holds.

---

## 7. Verifying the results

Three properties that theory guarantees, checked against the data.

**SJF minimises average waiting when all processes arrive together.** On `t1_basic`, where everything arrives at tick 0, SJF gives 6.20 against FCFS's 10.00, and no algorithm in the suite beats 6.20. A violation would mean a bug.

**SRTF is never worse than SJF when arrivals are staggered.** Confirmed on all five staggered traces:

| Trace | SJF | SRTF |
|---|---|---|
| t2_convoy | 10.33 | **3.83** |
| t3_starvation | 20.17 | **5.17** |
| t4_priority_skew | 4.90 | **3.80** |
| t5_mixed | 25.70 | **24.05** |
| t6_interactive | 6.87 | 6.87 |

The equality on `t6_interactive` is not a failure. SJF and SRTF are byte-identical there across every metric, which means no preemption ever fired: no process arrived with a shorter remaining time than the one running. The same holds on `t1_basic`, where simultaneous arrival makes preemption impossible.

**Every run satisfies the invariants** in `SPEC.md` section 12, asserted at the end of each run. Busy ticks equal the sum of bursts on all 54 runs, no process is scheduled before arrival, and every process completes.

---

## 8. Overall

Ranking the nine by average waiting across all six traces, then by average response, gives two different orders. That is the substantive finding: **there is no best scheduler, only a best scheduler for a stated objective.**

- **Shortest average waiting**: SRTF, then SJF. Both need burst lengths in advance, which a real OS does not have.
- **Best response**: CFS, then MLFQ. Both pay for it in switches and turnaround.
- **Fairest worst case**: FCFS and Round Robin, which is to say the two algorithms that ignore the most information.
- **Least switching**: the non-preemptive three, at the minimum possible count.
- **Best compromise without foreknowledge**: MLFQ. It approximates SJF's favouring of short jobs by observing behaviour rather than being told, and its boost bounds the starvation that SJF cannot.

That last point is why MLFQ, not SJF, is the ancestor of the schedulers real systems use. SJF is optimal and unimplementable. MLFQ is neither, and it ships.
