#!/bin/sh
# E3: builds tests/rwreplay_f1.cpp as a Wine exe that does everything through the C ABI of mflrw.dll (D3D9 build).  Also builds the native
# format-choice check tests/rasfmt_check.cpp (host g++, src/d3d/d3drasfmt.cpp alone).
# usage: tests/build_rwreplay_f1.sh <dir with mflrw.dll(.a), built by tools/build_mflrw.sh> ; writes <dir>/rwreplay_f1.exe and <dir>/rasfmt_check
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:?dir with mflrw.dll}
i686-w64-mingw32-g++ -std=c++11 -O2 -mfpmath=387 -ffp-contract=off -fno-exceptions -fno-rtti -w -DVIA_DLL -DRW_D3D9 -I"$R" -I"$R/src" -static-libgcc -static-libstdc++ \
	"$R/tests/rwreplay_f1.cpp" "$O/mflrw.dll.a" -lgdi32 -luser32 -o "$O/rwreplay_f1.exe"
g++ -O1 -I"$R/src" "$R/tests/rasfmt_check.cpp" "$R/src/d3d/d3drasfmt.cpp" -o "$O/rasfmt_check"
echo "built $O/rwreplay_f1.exe $O/rasfmt_check"
