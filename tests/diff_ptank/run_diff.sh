#!/bin/sh
# Rebuild the fork objects + dump program and compare against the original-behaviour model (ptankreview, 18450 cases).
# usage: sh tests/diff_ptank/run_diff.sh [reps] [outdir]   (g++/python3 only)
H=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$H/../.." && pwd)
D=${2:-/tmp/librw_ptank_diff}
mkdir -p "$D"
sh $R/tests/build_ptank_test.sh $D/obj >/dev/null 2>&1 || exit 1
g++ -std=c++11 -O1 -g -w -I$R -I$R/src $H/ptankdump.cpp $D/obj/*.o -Wl,--unresolved-symbols=ignore-all -o $D/ptankdump 2>/dev/null || exit 1
$D/ptankdump ${1:-1} > $D/fork.txt || exit 1
python3 -I $H/orig_model.py $D/fork.txt
