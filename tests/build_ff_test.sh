#!/bin/sh
# S3 piece 2: builds tests/ff_test.cpp as a Wine exe linked statically with the d3d9 objects tools/build_mflrw.sh left in <dir>/d3d9 (-DRW_D3D9).
# usage: tests/build_ff_test.sh <dir given to tools/build_mflrw.sh> ; writes <dir>/ff_test.exe.  Run under Wine + Xvfb (software GL), see ff_test.cpp.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:?dir given to build_mflrw.sh}
i686-w64-mingw32-g++ -std=c++11 -O2 -mfpmath=387 -ffp-contract=off -fno-exceptions -fno-rtti -w -DRW_D3D9 -DLODEPNG_NO_COMPILE_CPP -I"$R" -I"$R/src" -static-libgcc -static-libstdc++ \
	"$R/tests/ff_test.cpp" "$O"/d3d9/*.o -ld3d9 -lgdi32 -luser32 -o "$O/ff_test.exe"
echo "built $O/ff_test.exe"
