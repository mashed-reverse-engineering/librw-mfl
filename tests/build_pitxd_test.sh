#!/bin/sh
# Builds tests/pitxd_test with plain g++ (no cmake, no GPU, no platform backend). Needs zlib (test only).
# usage: [SAN=1] tests/build_pitxd_test.sh [outdir]   (default /tmp/librw_pitxd_test; SAN=1 adds ASan+UBSan)
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:-/tmp/librw_pitxd_test}
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
# platform backends (d3d/gl/ps2/xbox) are not built; their symbols are only reached by native textures/geometry
g++ $F -I"$R" -I"$R/src" "$R/tests/pitxd_test.cpp" "$O"/*.o -lz -Wl,--unresolved-symbols=ignore-all -o "$O/pitxd_test"
echo "built $O/pitxd_test"
