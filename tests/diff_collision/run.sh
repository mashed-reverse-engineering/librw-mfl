#!/bin/sh
# Differential harness for the collision component (review of patch 0014, fixes in 0017): the fork's queries / Rt
# functions against a float32 port of the original MFL.exe routines (colldiff.cpp), and the fork's tree parse against
# a reader following the original control flow (orig_coll.py). g++ and python3 only.
# usage: tests/diff_collision/run.sh [bsplist] [outdir]   (default list: game_bsps.txt next to this script, 126 BSPs)
# Expected after 0017: PARSE IDENTICAL; per type setEqual = seqEqual = firstEqual = queries, onlyFork = onlyOrig = 0,
# distBad 0, point!=v0 0; Rt: only fork / only orig / dist differs / weights differ 0.
set -e
H=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$H/../.." && pwd)
L=${1:-$H/game_bsps.txt}
O=${2:-/tmp/librw_diff_collision}
mkdir -p "$O/obj"
for f in "$R"/src/*.cpp; do b=$(basename "$f" .cpp); [ "$b" = charset ] && continue; g++ -std=c++11 -O2 -g -w -I"$R" -I"$R/src" -c "$f" -o "$O/obj/$b.o" & done
g++ -std=c++11 -O2 -g -w -c "$R/src/lodepng/lodepng.cpp" -o "$O/obj/lodepng.o" & wait
g++ -std=c++11 -O2 -g -w -I"$R" -I"$R/src" "$H/colldiff.cpp" "$O"/obj/*.o -Wl,--unresolved-symbols=ignore-all -o "$O/colldiff"
# (1) parse: fork vs original control flow reader
"$O/colldiff" dump $(cat "$L") > "$O/fork_dump.txt"
python3 -I "$H/orig_coll.py" $(cat "$L") > "$O/orig_dump.txt"
cmp "$O/fork_dump.txt" "$O/orig_dump.txt" && echo PARSE IDENTICAL
# (2) differential world queries (collision BSPs with trees; graphics BSPs without trees)
grep -v -E "/(GRAPH|GRAPHICS|MAIN|TREES)\.BSP$" "$L" > "$O/coll_bsps.txt"
grep -E "/(GRAPH|GRAPHICS|MAIN|TREES)\.BSP$" "$L" > "$O/gfx_bsps.txt"
"$O/colldiff" diff -n 6000 $(cat "$O/coll_bsps.txt") > "$O/diff_coll.txt"
"$O/colldiff" diff -n 1500 $(cat "$O/gfx_bsps.txt") > "$O/diff_gfx.txt"
# (3) Rt maths on random inputs
"$O/colldiff" rt -n 1000000 > "$O/diff_rt.txt"
cat "$O/diff_coll.txt" "$O/diff_gfx.txt" "$O/diff_rt.txt"
