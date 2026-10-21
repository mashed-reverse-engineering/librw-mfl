#!/bin/sh
# Differential of the librw-mfl Rt2d against a Python model of the game's routines (orig_reader.py).
# usage: tests/diff_rt2d/run_diff.sh [workdir]   -> SAME/DIFF per font file (files.txt), outputs in workdir/d
# Pass = cmp.py reports {} (everything exact, screen positions within 0.01 px). Needs g++ and python3 only. files.txt holds absolute paths of the 13 shipped RWF files.
set -e
H=$(cd "$(dirname "$0")" && pwd); R=$(cd "$H/../.." && pwd)
W=${1:-/tmp/librw_rt2d_diff}
mkdir -p "$W/d"
sh "$R/tests/build_rt2d_test.sh" "$W/obj" >/dev/null 2>&1
g++ -std=c++11 -O1 -w -I"$R" -I"$R/src" "$H/rt2ddump.cpp" $(ls "$W"/obj/*.o | grep -v '/rt2d_test') -Wl,--unresolved-symbols=ignore-all -o "$W/rt2ddump" 2>/dev/null
i=0; bad=0
while read f; do
  i=$((i+1))
  "$W/rt2ddump" "$H/strings.txt" "$f" > "$W/d/$i.lib" 2>&1
  python3 -I "$H/orig_reader.py" "$H/strings.txt" "$f" > "$W/d/$i.orig" 2>"$W/d/$i.err"
  if [ "$(python3 -I "$H/cmp.py" "$W/d/$i.lib" "$W/d/$i.orig" | head -1)" = "{}" ]; then echo "SAME $i $f"; else bad=$((bad+1)); echo "DIFF $i $f"; python3 -I "$H/cmp.py" "$W/d/$i.lib" "$W/d/$i.orig" | head -3; fi
done < "$H/files.txt"
echo "diffs: $bad"
[ $bad -eq 0 ]
