#!/bin/sh
# Builds tests/asset_dump (S2-b asset-parity oracle, librw side) with plain g++, no GPU, no backend.  MINGW=1: 32 bit mingw build (same flags as tools/build_mflrw.sh, layout = the game's) -> asset_dump.exe, run under Wine.
# usage: [MINGW=1] tests/build_asset_dump.sh [outdir]   (default /tmp/librw_asset_dump[_w32])
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
CXX=g++; F="-std=c++11 -O1 -w"; X=; O=${1:-/tmp/librw_asset_dump}
[ -n "$MINGW" ] && { CXX=i686-w64-mingw32-g++; F="-std=c++11 -O2 -mfpmath=387 -ffp-contract=off -fno-exceptions -fno-rtti -w -DLODEPNG_NO_COMPILE_CPP"; X=.exe; O=${1:-/tmp/librw_asset_dump_w32}; }
mkdir -p "$O"
SRC=$(ls "$R"/src/*.cpp)
[ -n "$MINGW" ] && { SRC=$(ls "$R"/src/*.cpp "$R"/src/gl/*.cpp "$R"/src/ps2/*.cpp "$R"/src/d3d/*.cpp); F="$F -DMFL_FULL_BACKENDS"; }	# same file set as tools/build_mflrw.sh (null variant, no RW_D3D9)
for f in $SRC; do
	b=$(basename "$f" .cpp)
	[ "$b" = charset ] && continue
	[ -e "$O/$b.o" ] && [ ! "$f" -nt "$O/$b.o" ] && [ ! "$R/src/rwobjects.h" -nt "$O/$b.o" ] && [ ! "$R/src/rwplugins.h" -nt "$O/$b.o" ] && continue
	$CXX $F -I"$R" -I"$R/src" -c "$f" -o "$O/$b.o" &
done
[ -e "$O/lodepng.o" ] || $CXX $F -c "$R/src/lodepng/lodepng.cpp" -o "$O/lodepng.o" &
wait
L=; LX="-Wl,--unresolved-symbols=ignore-all"; [ -n "$MINGW" ] && { L="-static"; LX=; }
$CXX $F $L -I"$R" -I"$R/src" "$R/tests/asset_dump.cpp" "$O"/*.o $LX -o "$O/asset_dump$X"
echo "built $O/asset_dump$X"
