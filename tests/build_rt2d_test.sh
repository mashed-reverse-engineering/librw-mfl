#!/bin/sh
# Builds tests/rt2d_test with plain g++ (no cmake, no GPU, no platform backend).
# usage: [SAN=1] tests/build_rt2d_test.sh [outdir]   (default /tmp/librw_rt2d_test; SAN=1 adds ASan+UBSan)
# Sources that fail to compile (other work in progress under src/) are skipped with a warning; rt2d.cpp must build.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:-/tmp/librw_rt2d_test}
F="-std=c++11 -O1 -g -w"
[ -n "$SAN" ] && F="$F -fsanitize=address,undefined -fno-sanitize-recover=undefined"
mkdir -p "$O"
rm -f "$O"/*.o "$O"/*.fail
for f in "$R"/src/*.cpp; do
	b=$(basename "$f" .cpp)
	[ "$b" = charset ] && continue	# needs a render backend
	( g++ $F -I"$R" -I"$R/src" -c "$f" -o "$O/$b.o" 2>"$O/$b.log" || touch "$O/$b.fail" ) &
done
g++ $F -c "$R/src/lodepng/lodepng.cpp" -o "$O/lodepng.o" &
wait
[ -f "$O/rt2d.fail" ] && { cat "$O/rt2d.log"; exit 1; }
for x in "$O"/*.fail; do [ -f "$x" ] && echo "warning: $(basename "$x" .fail).cpp did not compile, skipped"; done
g++ $F -I"$R" -I"$R/src" "$R/tests/rt2d_test.cpp" "$O"/*.o -Wl,--unresolved-symbols=ignore-all -o "$O/rt2d_test"
echo "built $O/rt2d_test"
