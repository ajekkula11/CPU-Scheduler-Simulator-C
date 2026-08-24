**CPU Scheduler Simulator**

Complete Trace Suite \+ Hand-Calculation Golden Results

This document contains: (1) all six workload traces, (2) every scheduling rule and assumption used by the project, and (3) full hand-calculated results for the three hand-verified traces under FCFS, SJF-NP, SRTF, Priority-NP, Priority-P and Round Robin (q=4). These numbers are the golden reference for correctness testing.

# **1\. Locked Rules & Assumptions (Cross-Verification Checklist)**

Every calculation in this document obeys these rules exactly. When you compare simulator output, any deviation means either a bug in the simulator or a difference in interpretation of a rule.

## **1.1 Engine Rules**

1. Single CPU, discrete integer ticks starting at t \= 0\.  
2. A process that arrives at tick t is eligible to run during tick t.  
3. Preemption is evaluated only at tick boundaries (never mid-tick).  
4. Context-switch cost \= 0 ticks (switches are counted but never charged).  
5. All processes are CPU-bound (no I/O, no blocking).

## **1.2 Priority & Tie-Breaking**

6. Lower priority number \= higher priority (0 is most urgent).  
7. Global tie-break: earlier arrival wins; if arrival times equal, lower PID wins.  
8. Preemption requires strict improvement only. A tie leaves the incumbent running.

## **1.3 Round-Robin Specific**

9. Default quantum \= 4 ticks.  
10. Arrivals at tick t are enqueued before a process whose quantum expires at tick t.  
11. Quantum expiry → process goes to the tail with a fresh quantum.

## **1.4 Metric Definitions**

12. Response time \= first\_run\_tick − arrival\_time (may be 0).  
13. Turnaround \= completion − arrival.  
14. Waiting \= turnaround − burst.  
15. Completion time is the tick after the last execution tick (process finishes at the end of its last tick).

# **2\. The Six Workload Traces**

## **2.1 t1\_basic.txt (5 processes) — Hand-verified**

All arrive at t=0, distinct bursts. Baseline sanity check.

\# pid  arrival  burst  priority  nice  queue

1 0 6 3 0 0

2 0 2 1 0 0

3 0 8 2 0 1

4 0 4 0 0 0

5 0 3 4 0 2

## **2.2 t2\_convoy.txt (6 processes) — Hand-verified**

One long job first, short jobs behind it. Exposes FCFS convoy effect.

\# pid  arrival  burst  priority  nice  queue

1 0 12 2 0 0

2 1 3 1 0 0

3 2 2 3 0 1

4 3 4 0 0 0

5 4 1 2 0 0

6 5 2 1 0 2

## **2.3 t3\_starvation.txt (12 processes)**

Steady stream of short jobs \+ one long job. Exposes SJF/SRTF starvation.

\# pid  arrival  burst  priority  nice  queue

1 0 20 5 0 2

2 1 2 1 0 0

3 2 2 1 0 0

4 4 3 2 0 0

5 5 2 1 0 0

6 7 2 1 0 0

7 8 3 2 0 1

8 10 2 1 0 0

9 11 2 1 0 0

10 13 3 2 0 0

11 14 2 1 0 0

12 16 2 1 0 0

## **2.4 t4\_priority\_skew.txt (10 processes) — Hand-verified**

High-priority jobs arrive continuously; one low-priority job. Exposes priority starvation.

\# pid  arrival  burst  priority  nice  queue

1 0 5 5 0 2

2 1 2 0 0 0

3 3 2 0 0 0

4 5 3 1 0 0

5 6 2 0 0 0

6 8 2 0 0 0

7 9 3 1 0 1

8 11 2 0 0 0

9 12 2 0 0 0

10 14 2 1 0 0

## **2.5 t5\_mixed.txt (20 processes)**

Main comparison workload and Round-Robin quantum-sweep target.

\# pid  arrival  burst  priority  nice  queue

1 0 8 3 0 0

2 1 4 1 \-1 0

3 2 6 2 1 1

4 3 3 0 0 0

5 5 10 4 2 2

6 6 2 1 \-2 0

7 7 5 3 0 1

8 8 7 2 1 0

9 10 3 0 \-1 0

10 11 4 5 3 2

11 12 6 1 0 0

12 14 2 2 0 1

13 15 9 4 2 2

14 16 3 0 \-2 0

15 18 5 3 1 1

16 19 4 1 0 0

17 21 7 2 2 0

18 22 2 0 \-1 0

19 24 6 5 3 2

20 25 3 1 0 1

## **2.6 t6\_interactive.txt (15 processes)**

Many short interactive bursts \+ a few long ones. Shows MLFQ and CFS response-time advantages.

\# pid  arrival  burst  priority  nice  queue

1 0 2 1 0 0

2 0 12 4 2 2

3 1 1 0 \-1 0

4 2 2 1 0 0

5 3 15 5 3 2

6 4 1 0 \-2 0

7 5 2 2 0 1

8 6 1 0 0 0

9 8 3 1 1 0

10 9 2 0 \-1 0

11 10 14 4 2 2

12 11 1 0 0 0

13 12 2 1 0 0

14 14 1 0 \-2 0

15 15 3 2 1 1

# **3\. Hand-Calculated Golden Results**

These are the authoritative expected values for the three hand-verified traces. Your simulator must match them exactly (zero discrepancies) for success criterion 1\.

## **3.1 Trace t1\_basic.txt**

### **FCFS**

Order follows PID order because all arrive at 0 and the global tie-break uses lower PID.

Timeline (pid per tick): 1 1 1 1 1 1 | 2 2 | 3 3 3 3 3 3 3 3 | 4 4 4 4 | 5 5 5

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 6 | 0 | 6 | 6 | 0 | 0 |
| 2 | 0 | 2 | 6 | 8 | 8 | 6 | 6 |
| 3 | 0 | 8 | 8 | 16 | 16 | 8 | 8 |
| 4 | 0 | 4 | 16 | 20 | 20 | 16 | 16 |
| 5 | 0 | 3 | 20 | 23 | 23 | 20 | 20 |

Averages: TAT (Comp \- Arr )  \= 14.60   WT(TAT-Burst) \= 10.00   RT (Response time, Start \-Arr) \= 10.00   

Context switches \= 4   Max WT \= 20

### **SJF Non-Preemptive**

All ready at t=0 → pick shortest burst first (ties broken by arrival then PID). Order: 2(2), 5(3), 4(4), 1(6), 3(8).

Timeline: 2 2 | 5 5 5 | 4 4 4 4 | 1 1 1 1 1 1 | 3 3 3 3 3 3 3 3

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 6 | 9 | 15 | 15 | 9 | 9 |
| 2 | 0 | 2 | 0 | 2 | 2 | 0 | 0 |
| 3 | 0 | 8 | 15 | 23 | 23 | 15 | 15 |
| 4 | 0 | 4 | 5 | 9 | 9 | 5 | 5 |
| 5 | 0 | 3 | 2 | 5 | 5 | 2 | 2 |

Averages: TAT \= 10.80   WT \= 6.20   RT \= 6.20   Context switches \= 4   Max WT \= 15

Note: SJF WT ≤ FCFS WT (optimal for all-arrival-at-0 case)  structural check passes.

### **SRTF (preemptive SJF)**

Because every process arrives at t=0, SRTF produces the identical schedule to SJF-NP (no later arrival can preempt).

Timeline & metrics identical to SJF-NP above. Context switches \= 4 

### **Priority Non-Preemptive**

Lower number \= higher priority. Order by priority then tie-break: 4(p0), 2(p1), 3(p2), 1(p3), 5(p4).

Timeline: 4 4 4 4 | 2 2 | 3 3 3 3 3 3 3 3 | 1 1 1 1 1 1 | 5 5 5

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 6 | 14 | 20 | 20 | 14 | 14 |
| 2 | 0 | 2 | 4 | 6 | 6 | 4 | 4 |
| 3 | 0 | 8 | 6 | 14 | 14 | 6 | 6 |
| 4 | 0 | 4 | 0 | 4 | 4 | 0 | 0 |
| 5 | 0 | 3 | 20 | 23 | 23 | 20 | 20 |

Averages: TAT \= 13.40   WT \= 8.80   RT \= 8.80   Context switches \= 4   Max WT \= 20

### **Priority Preemptive**

Same order and timeline as Priority-NP because all arrive at t=0 (no later higher-priority arrival exists to preempt).

### **Round Robin (quantum \= 4\)**

Ready queue starts \[1,2,3,4,5\] (arrival order / PID order). Each gets up to 4 ticks.

Timeline: 1 1 1 1 | 2 2 | 3 3 3 3 | 4 4 4 4 | 5 5 5 | 1 1 | 3 3 3 3

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 6 | 0 | 19 | 19 | 13 | 0 |
| 2 | 0 | 2 | 4 | 6 | 6 | 4 | 4 |
| 3 | 0 | 8 | 6 | 23 | 23 | 15 | 6 |
| 4 | 0 | 4 | 10 | 14 | 14 | 10 | 10 |
| 5 | 0 | 3 | 14 | 17 | 17 | 14 | 14 |

Averages: TAT \= 15.80   WT \= 11.20   RT \= 6.80   Context switches \= 6

## **3.2 Trace t2\_convoy.txt**

### **FCFS — Demonstrates the convoy effect**

P1 (burst 12\) runs to completion first; all short jobs wait behind it.

Timeline: 1×12 | 2 2 2 | 3 3 | 4 4 4 4 | 5 | 6 6

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 12 | 0 | 12 | 12 | 0 | 0 |
| 2 | 1 | 3 | 12 | 15 | 14 | 11 | 11 |
| 3 | 2 | 2 | 15 | 17 | 15 | 13 | 13 |
| 4 | 3 | 4 | 17 | 21 | 18 | 14 | 14 |
| 5 | 4 | 1 | 21 | 22 | 18 | 17 | 17 |
| 6 | 5 | 2 | 22 | 24 | 19 | 17 | 17 |

Averages: TAT \= 16.00   WT \= 12.00   RT \= 12.00   Max WT \= 17

Observation: short jobs suffer large waiting times purely because a long job arrived first — classic convoy effect.

### **SJF Non-Preemptive**

P1 still runs first (only ready process at t=0). After it finishes, shortest remaining ready jobs run: 5(1), 3(2), 6(2), 2(3), 4(4).

Timeline: 1×12 | 5 | 3 3 | 6 6 | 2 2 2 | 4 4 4 4

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 12 | 0 | 12 | 12 | 0 | 0 |
| 2 | 1 | 3 | 17 | 20 | 19 | 16 | 16 |
| 3 | 2 | 2 | 13 | 15 | 13 | 11 | 11 |
| 4 | 3 | 4 | 20 | 24 | 21 | 17 | 17 |
| 5 | 4 | 1 | 12 | 13 | 9 | 8 | 8 |
| 6 | 5 | 2 | 15 | 17 | 12 | 10 | 10 |

Averages: TAT \= 14.33   WT \= 10.33   RT \= 10.33

### **SRTF (preemptive)**

P1 starts, but as soon as shorter jobs arrive they preempt it. P1 finishes last.

Timeline: 1 | 2 2 2 | 5 | 3 3 | 6 6 | 4 4 4 4 | 1×11

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 12 | 0 | 24 | 24 | 12 | 0 |
| 2 | 1 | 3 | 1 | 4 | 3 | 0 | 0 |
| 3 | 2 | 2 | 5 | 7 | 5 | 3 | 3 |
| 4 | 3 | 4 | 9 | 13 | 10 | 6 | 6 |
| 5 | 4 | 1 | 4 | 5 | 1 | 0 | 0 |
| 6 | 5 | 2 | 7 | 9 | 4 | 2 | 2 |

Averages: TAT \= 7.83   WT \= 3.83   RT \= 1.83   Max WT \= 12 (the long job)

Observation: short jobs finish quickly; the long job is starved until the end — SRTF starvation of long jobs.

### **Priority Non-Preemptive**

P1 (prio 2\) runs first. After it: 4(prio 0), 2(prio 1), 6(prio 1), 5(prio 2), 3(prio 3).  
Timeline: 1×12 | 4 4 4 4 | 2 2 2 | 6 6 | 5 | 3 3

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 12 | 0 | 12 | 12 | 0 | 0 |
| 2 | 1 | 3 | 16 | 19 | 18 | 15 | 15 |
| 3 | 2 | 2 | 22 | 24 | 22 | 20 | 20 |
| 4 | 3 | 4 | 12 | 16 | 13 | 9 | 9 |
| 5 | 4 | 1 | 21 | 22 | 18 | 17 | 17 |
| 6 | 5 | 2 | 19 | 21 | 16 | 14 | 14 |

Averages: TAT \= 16.50   WT \= 12.50   RT \= 12.50

### 

### **Priority Preemptive**

Higher-priority arrivals preempt P1. Final order reflects continuous priority evaluation.

Timeline: 1 | 2 2 | 4 4 4 4 | 2 | 6 6 | 1×11 | 5 | 3 3

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 12 | 0 | 21 | 21 | 9 | 0 |
| 2 | 1 | 3 | 1 | 8 | 7 | 4 | 0 |
| 3 | 2 | 2 | 22 | 24 | 22 | 20 | 20 |
| 4 | 3 | 4 | 3 | 7 | 4 | 0 | 0 |
| 5 | 4 | 1 | 21 | 22 | 18 | 17 | 17 |
| 6 | 5 | 2 | 8 | 10 | 5 | 3 | 3 |

Averages: TAT \= 12.83   WT \= 8.83  RT=6.67

### **Round Robin (q=4)** Timeline: 1 1 1 1 | 2 2 2 | 3 3 | 4 4 4 4 | 1 1 1 1 | 5 | 6 6 | 1 1 1 1

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 12 | 0 | 24 | 24 | 12 | 0 |
| 2 | 1 | 3 | 4 | 7 | 6 | 3 | 3 |
| 3 | 2 | 2 | 7 | 9 | 7 | 5 | 5 |
| 4 | 3 | 4 | 9 | 13 | 10 | 6 | 6 |
| 5 | 4 | 1 | 17 | 18 | 14 | 13 | 13 |
| 6 | 5 | 2 | 18 | 20 | 15 | 13 | 13 |

Averages: TAT \= 12.67   WT \= 8.67  RT \= 6.67

## **3.3 Trace t4\_priority\_skew.txt**

### **FCFS**

Timeline: 1 1 1 1 1 | 2 2 | 3 3 | 4 4 4 | 5 5 | 6 6 | 7 7 7 | 8 8 | 9 9 | 10 10

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 5 | 0 | 5 | 5 | 0 | 0 |
| 2 | 1 | 2 | 5 | 7 | 6 | 4 | 4 |
| 3 | 3 | 2 | 7 | 9 | 6 | 4 | 4 |
| 4 | 5 | 3 | 9 | 12 | 7 | 4 | 4 |
| 5 | 6 | 2 | 12 | 14 | 8 | 6 | 6 |
| 6 | 8 | 2 | 14 | 16 | 8 | 6 | 6 |
| 7 | 9 | 3 | 16 | 19 | 10 | 7 | 7 |
| 8 | 11 | 2 | 19 | 21 | 10 | 8 | 8 |
| 9 | 12 | 2 | 21 | 23 | 11 | 9 | 9 |
| 10 | 14 | 2 | 23 | 25 | 11 | 9 | 9 |

Averages: TAT \= 8.20   WT \= 5.70

### **SJF-NP**

Timeline: 1 1 1 1 1 | 2 2 | 3 3 | 5 5 | 6 6 | 8 8 | 9 9 | 10 10 | 4 4 4 | 7 7 7

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 5 | 0 | 5 | 5 | 0 | 0 |
| 2 | 1 | 2 | 5 | 7 | 6 | 4 | 4 |
| 3 | 3 | 2 | 7 | 9 | 6 | 4 | 4 |
| 4 | 5 | 3 | 19 | 22 | 17 | 14 | 14 |
| 5 | 6 | 2 | 9 | 11 | 5 | 3 | 3 |
| 6 | 8 | 2 | 11 | 13 | 5 | 3 | 3 |
| 7 | 9 | 3 | 22 | 25 | 16 | 13 | 13 |
| 8 | 11 | 2 | 13 | 15 | 4 | 2 | 2 |
| 9 | 12 | 2 | 15 | 17 | 5 | 3 | 3 |
| 10 | 14 | 2 | 17 | 19 | 5 | 3 | 3 |

Averages: TAT \= 7.40  WT \= 4.90, RT \= 4.90  Context Switches 9  Max WT \= 14\. 

### **SRTF** 

Timeline: 1 | 2 2 | 3 3 | 4 4 4 | 5 5 | 6 6 | 8 8 | 9 9 | 10 10 | 7 7 7 | 1 1 1 1

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 5 | 0 | 25 | 25 | 20 | 0 |
| 2 | 1 | 2 | 1 | 3 | 2 | 0 | 0 |
| 3 | 3 | 2 | 3 | 5 | 2 | 0 | 0 |
| 4 | 5 | 3 | 5 | 8 | 3 | 0 | 0 |
| 5 | 6 | 2 | 8 | 10 | 4 | 2 | 2 |
| 6 | 8 | 2 | 10 | 12 | 4 | 2 | 2 |
| 7 | 9 | 3 | 18 | 21 | 12 | 9 | 9 |
| 8 | 11 | 2 | 12 | 14 | 3 | 1 | 1 |
| 9 | 12 | 2 | 14 | 16 | 4 | 2 | 2 |
| 10 | 14 | 2 | 16 | 18 | 4 | 2 | 2 |

Averages: TAT 6.30  WT 3.80  RT 1.80  Context Switches 10  Max WT \= 20\. 

### **Priority Non-Preemptive — Shows priority starvation risk**

P1 (prio 5, lowest) starts first. High-priority jobs (prio 0\) that arrive later still wait until P1 finishes because the algorithm is non-preemptive

Timeline: 1 1 1 1 1 | 2 2 | 3 3 | 5 5 | 6 6 | 8 8 | 9 9 | 4 4 4 | 7 7 7 | 10 10\.

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 5 | 0 | 5 | 5 | 0 | 0 |
| 2 | 1 | 2 | 5 | 7 | 6 | 4 | 4 |
| 3 | 3 | 2 | 7 | 9 | 6 | 4 | 4 |
| 4 | 5 | 3 | 17 | 20 | 15 | 12 | 12 |
| 5 | 6 | 2 | 9 | 11 | 5 | 3 | 3 |
| 6 | 8 | 2 | 11 | 13 | 5 | 3 | 3 |
| 7 | 9 | 3 | 20 | 23 | 14 | 11 | 11 |
| 8 | 11 | 2 | 13 | 15 | 4 | 2 | 2 |
| 9 | 12 | 2 | 15 | 17 | 5 | 3 | 3 |
| 10 | 14 | 2 | 23 | 25 | 11 | 9 | 9 |

Averages: TAT \= 7.60   WT \= 5.10   Max WT \= 12

### **Priority Preemptive — Clear starvation of the low-priority job**

P1 (prio 5\) is continuously preempted by every higher-priority arrival. It only finishes after all higher-priority work is done.

Timeline: 1 | 2 2 | 3 3 | 4 | 5 5 | 6 6 | 4 | 8 8 | 9 9 | 4 | 7 7 7 | 10 10 | 1 1 1 1

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 5 | 0 | 25 | 25 | 20 | 0 |
| 2 | 1 | 2 | 1 | 3 | 2 | 0 | 0 |
| 3 | 3 | 2 | 3 | 5 | 2 | 0 | 0 |
| 4 | 5 | 3 | 5 | 16 | 11 | 8 | 0 |
| 5 | 6 | 2 | 6 | 8 | 2 | 0 | 0 |
| 6 | 8 | 2 | 8 | 10 | 2 | 0 | 0 |
| 7 | 9 | 3 | 16 | 19 | 10 | 7 | 7 |
| 8 | 11 | 2 | 11 | 13 | 2 | 0 | 0 |
| 9 | 12 | 2 | 13 | 15 | 3 | 1 | 1 |
| 10 | 14 | 2 | 19 | 21 | 7 | 5 | 5 |

Averages: TAT \= 6.60   WT \= 4.10  RT \= 1.3   Max WT \= 20 (the low-priority process)

Observation: P1 waits 20 ticks — classic priority starvation. This is exactly the phenomenon the analysis document must explain.

### **Round Robin** 

Timeline: 1 1 1 1 | 2 2 | 3 3 | 1 | 4 4 4 | 5 5 | 6 6 | 7 7 7 | 8 8 | 9 9 | 10 10

| PID | Arr | Burst | Start | Comp | TAT | WT | RT |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 | 0 | 5 | 0 | 9 | 9 | 4 | 0 |
| 2 | 1 | 2 | 4 | 6 | 5 | 3 | 3 |
| 3 | 3 | 2 | 6 | 8 | 5 | 3 | 3 |
| 4 | 5 | 3 | 9 | 12 | 7 | 4 | 4 |
| 5 | 6 | 2 | 12 | 14 | 8 | 6 | 6 |
| 6 | 8 | 2 | 14 | 16 | 8 | 6 | 6 |
| 7 | 9 | 3 | 16 | 19 | 10 | 7 | 7 |
| 8 | 11 | 2 | 19 | 21 | 10 | 8 | 8 |
| 9 | 12 | 2 | 21 | 23 | 11 | 9 | 9 |
| 10 | 14 | 2 | 23 | 25 | 11 | 9 | 9 |

Averages: TAT \= 8.40 WT \= 5.90 RT \= 5.50

# **4\. How to Use These Results**

* Copy the six .txt files from the traces/ directory into your project.  
* Implement FCFS first. Run it on t1\_basic and t2\_convoy. Diff the per-process metrics against the tables above. They must match exactly.  
* Once FCFS is golden, add SJF-NP and repeat. The structural check “SJF WT ≤ FCFS WT on t1” must hold.  
* Continue with SRTF, Priority-NP, Priority-P, RR. Each time verify against the corresponding table.  
* After the classic algorithms are solid, implement MLQ / MLFQ / CFS. Use the same traces; the hand calculations for those three algorithms are intentionally left to the simulator once the engine is proven correct on the simpler algorithms.  
* Commit the three hand-verified expected CSV files under tests/expected/ so the regression harness can catch any future regression.

# **5\. Notes on Larger Traces & Advanced Algorithms**

t3, t5 and t6 contain more processes and are intended for comparative analysis and quantum sweeps, not for exhaustive hand calculation. Once the engine and the six classic algorithms match the golden tables above, the simulator itself becomes the trusted source for generating expected results on the larger traces and for MLQ/MLFQ/CFS.

The rules in Section 1 remain binding for every algorithm. In particular, MLFQ demotion occurs only on quantum exhaustion (not on preemption), a preempted process returns to the head of its queue with remaining quantum, and CFS uses the exact weight table and per-tick vruntime update given in the project specification.

*End of verification document. All numbers were generated under the locked project rules and can be used directly as golden references.*
