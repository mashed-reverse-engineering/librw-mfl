#!/bin/sh
# Builds the RW 3.7 C-ABI facade DLL of librw-mfl with mingw (no cmake): mflrw.dll (D3D9 backend) and mflrw_null.dll (no backend).
# usage: tools/build_mflrw.sh [outdir]      (default /tmp/librw_mflrw; env GEN=<ghidra repo dir> regenerates capi/rwmfl.h|def|stubs_gen.cpp first)
# Flags follow plan_librw_integration section 2.1: x87 math, no exceptions/rtti, static runtime; the DLL never touches the FPU control word.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:-/tmp/librw_mflrw}
CXX=${CXX:-i686-w64-mingw32-g++}
if [ -n "$GEN" ]; then
	python3 "$R/capi/gen_facade.py" "$GEN/output/rwsurf/api_used.csv" "$GEN/../references/sdk/rwsdk-3.7.0.2-full/Library_D3D9_Hdr_Files" "$GEN/output/librw_facade_coverage.csv"
fi
F="-std=c++11 -O2 -mfpmath=387 -ffp-contract=off -fno-exceptions -fno-rtti -w -DLODEPNG_NO_COMPILE_CPP -I$R -I$R/src"
for P in d3d9 null; do
	D=$O/$P; mkdir -p "$D"
	PF="$F"; [ $P = d3d9 ] && PF="$F -DRW_D3D9"
	SRC=$(ls "$R"/src/*.cpp "$R"/src/gl/*.cpp "$R"/src/ps2/*.cpp "$R"/src/d3d/*.cpp "$R"/src/lodepng/lodepng.cpp "$R"/capi/*.cpp)
	for f in $SRC; do
		b=$(basename "$f" .cpp)
		[ $P = null ] && [ "$b" = charset ] && continue		# needs a render backend
		[ ! -e "$D/$b.o" ] || [ "$f" -nt "$D/$b.o" ] || [ "$R/src/rwobjects.h" -nt "$D/$b.o" ] || [ "$R/src/rwplugins.h" -nt "$D/$b.o" ] || [ "$R/capi/rwmfl.h" -nt "$D/$b.o" ] || continue
		$CXX $PF -c "$f" -o "$D/$b.o" &
	done
	wait
	LIBS=""; DLL=mflrw_null.dll
	[ $P = d3d9 ] && { LIBS="-ld3d9 -lgdi32 -luser32"; DLL=mflrw.dll; }
	$CXX -shared -O2 -static-libgcc -static-libstdc++ -o "$O/$DLL" "$R/capi/rwmfl.def" "$D"/*.o $LIBS -Wl,--out-implib,"$O/${DLL%.dll}.dll.a"
done
echo "built $O/mflrw.dll $O/mflrw_null.dll"
# test exe for the Wine run: tools/build_mflrw.sh also builds $O/test_g0.exe; run  docker run --rm -v $O:/t tianon/wine:latest wine /t/test_g0.exe /t/mflrw.dll
i686-w64-mingw32-gcc -O1 -mfpmath=387 -w -I"$R/capi" "$R/capi/test_g0.c" -o "$O/test_g0.exe"
# S2-c test: engine + stream + file system manager through the C ABI; run: docker run --rm -v $O:/t tianon/wine:latest wine /t/test_s2c.exe /t/mflrw_null.dll
i686-w64-mingw32-gcc -O1 -mfpmath=387 -w -I"$R/capi" "$R/capi/test_s2c.c" -o "$O/test_s2c.exe"
