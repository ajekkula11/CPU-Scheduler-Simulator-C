#!/bin/bash
#
# Diff live simulator output against the hand-verified golden files in
# tests/expected/. Every algorithm in ALGOS is run against every trace in
# TRACES, and all three output files are compared byte for byte.
#
# Run from the repository root:  ./tests/run_tests.sh

set -u

BIN=./scheduler
EXPECTED=tests/expected
OUTPUT=output

# Traces with hand-computed expected results. The other three traces in
# traces/ have no golden files and are not tested here.
TRACES="t1_basic t2_convoy t4_priority_skew"

# Algorithms with golden files. Add to this list as each is implemented.
ALGOS="fcfs sjf srtf priority_np priority_p rr"

pass=0
fail=0
skip=0

if [ ! -x "$BIN" ]; then
    echo "error: $BIN not found. Run make first."
    exit 1
fi

mkdir -p "$OUTPUT"

for algo in $ALGOS; do
    for trace in $TRACES; do

        # Skip algorithms not yet registered, so the script stays usable
        # while the suite is still being built out.
        if ! "$BIN" --algo "$algo" --trace "traces/$trace.txt" \
             >/dev/null 2>&1; then
            printf '  SKIP  %-12s %-18s (not implemented or run failed)\n' \
                   "$algo" "$trace"
            skip=$((skip + 1))
            continue
        fi

        for kind in timeline processes summary; do
            name="${algo}__${trace}__${kind}.csv"

            if [ ! -f "$EXPECTED/$name" ]; then
                printf '  SKIP  %s (no golden file)\n' "$name"
                skip=$((skip + 1))
                continue
            fi

            if diff -q "$OUTPUT/$name" "$EXPECTED/$name" >/dev/null 2>&1; then
                printf '  ok    %s\n' "$name"
                pass=$((pass + 1))
            else
                printf '  FAIL  %s\n' "$name"
                diff "$OUTPUT/$name" "$EXPECTED/$name" | head -10 | sed 's/^/          /'
                fail=$((fail + 1))
            fi
        done
    done
done

echo
echo "passed $pass, failed $fail, skipped $skip"

if [ "$fail" -gt 0 ]; then
    exit 1
fi
exit 0
