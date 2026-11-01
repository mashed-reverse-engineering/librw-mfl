#!/bin/sh
# B9: builds tests/rwreplay.cpp as a Wine exe linked statically with the d3d9 objects tools/build_mflrw.sh left in <dir>/d3d9 (-DRW_D3D9).
# usage: tests/build_rwreplay.sh <dir given to tools/build_mflrw.sh> ; writes <dir>/rwreplay.exe.  Run under Wine + Xvfb (software GL), see rwreplay.cpp.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:?dir given to build_mflrw.sh}
i686-w64-mingw32-g++ -std=c++11 -O2 -mfpmath=387 -ffp-contract=off -fno-exceptions -fno-rtti -w -DRW_D3D9 -DLODEPNG_NO_COMPILE_CPP -I"$R" -I"$R/src" -static-libgcc -static-libstdc++ \
	"$R/tests/rwreplay.cpp" "$O"/d3d9/*.o -ld3d9 -lgdi32 -luser32 -o "$O/rwreplay.exe"
echo "built $O/rwreplay.exe"
