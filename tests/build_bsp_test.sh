#!/bin/sh
# Builds tests/bsp_test with plain g++ (no cmake, no GPU, no platform backend).
# usage: tests/build_bsp_test.sh [outdir]   (default /tmp/librw_bsp_test)
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:-/tmp/librw_bsp_test}
mkdir -p "$O"
for f in "$R"/src/*.cpp; do
	b=$(basename "$f" .cpp)
	[ "$b" = charset ] && continue	# needs a render backend
	g++ -std=c++11 -O1 -w -I"$R" -I"$R/src" -c "$f" -o "$O/$b.o" &
done
g++ -std=c++11 -O1 -w -c "$R/src/lodepng/lodepng.cpp" -o "$O/lodepng.o" &
wait
# platform backends (d3d/gl/ps2/xbox) are not built; their symbols are only reached by native
# textures/geometry, which BSP loading never touches, hence the unresolved-symbols flag.
g++ -std=c++11 -O1 -w -I"$R" -I"$R/src" "$R/tests/bsp_test.cpp" "$O"/*.o -Wl,--unresolved-symbols=ignore-all -o "$O/bsp_test"
echo "built $O/bsp_test"
