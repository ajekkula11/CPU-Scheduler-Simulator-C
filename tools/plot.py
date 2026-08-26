#!/usr/bin/env python3
"""Render charts from the CSV files the simulator writes into output/.

Reads only what is already there: run ./scheduler --all and
./scheduler --sweep --trace traces/t5_mixed.txt first. Nothing here
recomputes a metric, so a chart can never disagree with a CSV.

Produces four kinds of chart:

  gantt__<trace>.png      timeline per algorithm, stacked
  compare__<trace>.png    average turnaround, waiting, response per algorithm
  switches__<trace>.png   context switches per algorithm
  sweep__<trace>.png      Round Robin quantum against waiting and switches

Usage:  python3 tools/plot.py [output_dir]
"""

import csv
import os
import sys
from collections import OrderedDict

import matplotlib
matplotlib.use("Agg")          # no display needed, write straight to file
import matplotlib.pyplot as plt

# Canonical order, matching SPEC.md section 7. Charts read left to right in
# roughly increasing sophistication, which makes the comparisons legible.
ALGO_ORDER = ["fcfs", "sjf", "srtf", "rr", "priority_np", "priority_p",
              "mlq", "mlfq", "cfs"]

ALGO_LABEL = {
    "fcfs": "FCFS", "sjf": "SJF", "srtf": "SRTF", "rr": "RR",
    "priority_np": "Prio-NP", "priority_p": "Prio-P",
    "mlq": "MLQ", "mlfq": "MLFQ", "cfs": "CFS",
}

IDLE = -1


def read_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def discover(outdir):
    """Map trace name to the algorithms that have output for it."""
    found = {}
    for name in sorted(os.listdir(outdir)):
        if not name.endswith("__summary.csv") or name.startswith("sweep__"):
            continue
        algo, trace, _ = name.split("__")
        found.setdefault(trace, []).append(algo)
    return found


def ordered(algos):
    """Sort by ALGO_ORDER, keeping anything unrecognised at the end."""
    known = [a for a in ALGO_ORDER if a in algos]
    return known + sorted(set(algos) - set(known))


def plot_gantt(outdir, trace, algos):
    """One row per algorithm, coloured blocks showing which pid held the CPU."""
    fig, ax = plt.subplots(figsize=(12, 0.6 * len(algos) + 1.5))
    cmap = plt.get_cmap("tab20")
    pids = set()

    for row, algo in enumerate(algos):
        ticks = read_csv(os.path.join(
            outdir, "%s__%s__timeline.csv" % (algo, trace)))

        # Collapse consecutive ticks with the same pid into one bar, so a
        # 12-tick run draws as one block instead of twelve.
        start = 0
        for i in range(len(ticks) + 1):
            at_end = (i == len(ticks))
            changed = (not at_end and i > 0
                       and ticks[i]["pid"] != ticks[start]["pid"])
            if at_end or changed:
                pid = int(ticks[start]["pid"])
                if pid != IDLE:
                    pids.add(pid)
                    ax.broken_barh([(start, i - start)],
                                   (row - 0.35, 0.7),
                                   facecolors=cmap((pid - 1) % 20),
                                   edgecolor="white", linewidth=0.5)
                start = i

    ax.set_yticks(range(len(algos)))
    ax.set_yticklabels([ALGO_LABEL.get(a, a) for a in algos])
    ax.invert_yaxis()
    ax.set_xlabel("tick")
    ax.set_title("CPU timeline: %s" % trace)
    ax.grid(axis="x", alpha=0.3)


    handles = [plt.Rectangle((0, 0), 1, 1, color=cmap((p - 1) % 20))
               for p in sorted(pids)]
    ax.legend(handles, ["P%d" % p for p in sorted(pids)],
              ncol=min(len(pids), 10), fontsize="small",
              loc="upper center", bbox_to_anchor=(0.5, -0.25))

    fig.tight_layout()
    path = os.path.join(outdir, "gantt__%s.png" % trace)
    fig.savefig(path, dpi=120, bbox_inches="tight")
    plt.close(fig)
    return path


def plot_compare(outdir, trace, algos):
    """Grouped bars: the three averages side by side per algorithm."""
    metrics = OrderedDict([
        ("avg_turnaround", "turnaround"),
        ("avg_waiting", "waiting"),
        ("avg_response", "response"),
    ])
    values = {k: [] for k in metrics}

    for algo in algos:
        row = read_csv(os.path.join(
            outdir, "%s__%s__summary.csv" % (algo, trace)))[0]
        for key in metrics:
            values[key].append(float(row[key]))

    fig, ax = plt.subplots(figsize=(10, 5))
    width = 0.26
    xs = range(len(algos))

    for i, (key, label) in enumerate(metrics.items()):
        offset = (i - 1) * width
        ax.bar([x + offset for x in xs], values[key], width, label=label)

    ax.set_xticks(list(xs))
    ax.set_xticklabels([ALGO_LABEL.get(a, a) for a in algos])
    ax.set_ylabel("ticks")
    ax.set_title("Average metrics: %s" % trace)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)

    fig.tight_layout()
    path = os.path.join(outdir, "compare__%s.png" % trace)
    fig.savefig(path, dpi=120)
    plt.close(fig)
    return path


def plot_switches(outdir, trace, algos):
    """Context switches per algorithm: the cost side of the trade-off."""
    counts = []
    for algo in algos:
        row = read_csv(os.path.join(
            outdir, "%s__%s__summary.csv" % (algo, trace)))[0]
        counts.append(int(row["context_switches"]))

    fig, ax = plt.subplots(figsize=(10, 4))
    ax.bar([ALGO_LABEL.get(a, a) for a in algos], counts, color="#c44e52")
    ax.set_ylabel("context switches")
    ax.set_title("Context switches: %s" % trace)
    ax.grid(axis="y", alpha=0.3)

    fig.tight_layout()
    path = os.path.join(outdir, "switches__%s.png" % trace)
    fig.savefig(path, dpi=120)
    plt.close(fig)
    return path


def plot_sweep(outdir, path_in):
    """Quantum against waiting time and switch count, on twin axes.

    The two curves move in opposite directions, which is the whole point:
    a small quantum keeps waiting low and pays for it in switches.
    """
    rows = read_csv(path_in)
    trace = rows[0]["trace"]
    q = [int(r["quantum"]) for r in rows]
    wait = [float(r["avg_waiting"]) for r in rows]
    sw = [int(r["context_switches"]) for r in rows]

    fig, ax1 = plt.subplots(figsize=(9, 5))
    ax2 = ax1.twinx()

    ax1.plot(q, wait, marker="o", color="#4c72b0", label="avg waiting")
    ax2.plot(q, sw, marker="s", color="#c44e52", label="context switches")

    ax1.set_xlabel("quantum")
    ax1.set_ylabel("average waiting (ticks)", color="#4c72b0")
    ax2.set_ylabel("context switches", color="#c44e52")
    ax1.set_xticks(q)
    ax1.grid(alpha=0.3)
    ax1.set_title("Round Robin quantum sweep: %s" % trace)

    lines = ax1.get_lines() + ax2.get_lines()
    ax1.legend(lines, [l.get_label() for l in lines], loc="upper center")

    fig.tight_layout()
    out = os.path.join(outdir, "sweep__%s.png" % trace)
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "output"

    if not os.path.isdir(outdir):
        sys.exit("no such directory: %s\n"
                 "run ./scheduler --all first" % outdir)

    found = discover(outdir)
    if not found:
        sys.exit("no summary CSVs in %s/\n"
                 "run ./scheduler --all first" % outdir)

    written = []
    for trace in sorted(found):
        algos = ordered(found[trace])
        written.append(plot_gantt(outdir, trace, algos))
        written.append(plot_compare(outdir, trace, algos))
        written.append(plot_switches(outdir, trace, algos))

    for name in sorted(os.listdir(outdir)):
        if name.startswith("sweep__") and name.endswith(".csv"):
            written.append(plot_sweep(outdir, os.path.join(outdir, name)))

    for path in written:
        print("wrote %s" % path)


if __name__ == "__main__":
    main()
