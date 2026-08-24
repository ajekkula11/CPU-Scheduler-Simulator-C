# CPU Scheduler Simulator

A single-core, tick-based CPU scheduler simulator written in C99, implementing
nine scheduling algorithms over a fixed suite of workload traces. Outputs
timeline, per-process, and aggregate metrics as CSV, with charts rendered by a
Python script.

**Status:** in development (Phase 0 — setup and specification).

## Build

```
make
```

## Run

```
./scheduler --algo <name> --trace <file> [--quantum N]
./scheduler --all
./scheduler --sweep --trace <file>
```

## Requirements

gcc, make, python3, matplotlib
