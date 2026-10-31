#!/bin/sh
# S2-c: builds tests/asset_dump.cpp as a Wine exe that loads everything through the C ABI of mflrw_null.dll (-DVIA_DLL), for the asset-parity oracle through the facade.
# usage: tests/build_asset_dump_dll.sh <dir with mflrw_null.dll(.a), built by tools/build_mflrw.sh> ; writes <dir>/asset_dump_dll.exe
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
O=${1:?dir with mflrw_null.dll}
i686-w64-mingw32-g++ -std=c++11 -O2 -mfpmath=387 -ffp-contract=off -fno-exceptions -fno-rtti -w -DVIA_DLL -I"$R" -I"$R/src" -static-libgcc -static-libstdc++ \
	"$R/tests/asset_dump.cpp" "$O/mflrw_null.dll.a" -o "$O/asset_dump_dll.exe"
echo "built $O/asset_dump_dll.exe"
