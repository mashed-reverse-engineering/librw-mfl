#!/bin/sh
# Builds tests/extra_test with plain g++ (no cmake, no GPU, no platform backend).
# usage: [SAN=1] tests/build_extra_test.sh [outdir]   (default /tmp/librw_extra_test; SAN=1 adds ASan+UBSan)
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:-/tmp/librw_extra_test}
F="-std=c++11 -O1 -g -w"
[ -n "$SAN" ] && F="$F -fsanitize=address,undefined -fno-sanitize-recover=undefined"
mkdir -p "$O"
for f in "$R"/src/*.cpp; do
	b=$(basename "$f" .cpp)
	[ "$b" = charset ] && continue	# needs a render backend
	g++ $F -I"$R" -I"$R/src" -c "$f" -o "$O/$b.o" &
done
g++ $F -c "$R/src/lodepng/lodepng.cpp" -o "$O/lodepng.o" &
wait
g++ $F -I"$R" -I"$R/src" "$R/tests/extra_test.cpp" "$O"/*.o -Wl,--unresolved-symbols=ignore-all -o "$O/extra_test"
echo "built $O/extra_test"
