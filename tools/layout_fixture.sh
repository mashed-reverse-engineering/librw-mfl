#!/bin/sh
# Negative fixture for -DLAYOUT_STRICT: the strict layout_check.cpp must compile, and must FAIL with one planted wrong row (LAYOUT_PLANT).
# usage: tools/layout_fixture.sh        prints PASS / FAIL, exit 0 on PASS
R=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-i686-w64-mingw32-g++}
F="-std=c++11 -fsyntax-only -w -DLAYOUT_STRICT -I$R -I$R/src $R/capi/layout_check.cpp"
$CXX $F || { echo "FAIL: clean strict build does not compile"; exit 1; }
if $CXX $F -DLAYOUT_PLANT 2>&1 | grep -q Planted_Matrix_pos; then echo "PASS: clean compiles, planted row fails"; else echo "FAIL: planted wrong offset was not caught"; exit 1; fi
