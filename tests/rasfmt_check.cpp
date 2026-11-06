// E3: src/d3d/d3drasfmt.cpp against the original _rwD3D9FindCorrectRasterFormat (ghidra E3 fmtoracle.py gen <vectors>: lines `CAPS tex rt ds disp z pal mode`
// and `F type flags result`, the result computed by the exe bytes under Unicorn).  Host build: g++ -I src tests/rasfmt_check.cpp src/d3d/d3drasfmt.cpp
// usage: rasfmt_check <vectors.txt> [--emit]   (--emit: print `F type flags ours` for each case instead, for fmtoracle.py check)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "d3d/d3drasfmt.h"
using namespace rw::d3d;
int main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage: rasfmt_check vectors.txt [--emit]\n"); return 2; }
	FILE *f = fopen(argv[1], "r"); if(!f){ perror(argv[1]); return 2; }
	bool emit = argc > 2 && !strcmp(argv[2], "--emit");
	RasFmtCaps c; memset(&c, 0, sizeof c);
	char l[256]; long n = 0, bad = 0;
	while(fgets(l, sizeof l, f)){
		unsigned tex, rt, ds, pal, mode, fl, res; int disp, z, t;
		if(sscanf(l, "CAPS %x %x %x %d %d %x %x", &tex, &rt, &ds, &disp, &z, &pal, &mode) == 7){
			c.tex = tex; c.rt = rt; c.ds = ds; c.displayDepth = disp; c.zDepth = z; c.textureCaps = pal; c.modeFormat = mode;
			if(emit) fputs(l, stdout);
			continue;
		}
		if(sscanf(l, "F %d %x %x", &t, &fl, &res) != 3) continue;
		unsigned o = (unsigned)findCorrectRasterFormat(t, (int)fl, &c); n++;
		if(emit){ printf("F %d %x %x\n", t, fl, o); continue; }
		if(o != res && bad++ < 10) printf("DIFF type %d flags %x: original %x, ours %x\n", t, fl, res, o);
	}
	if(!emit) printf(bad ? "RASFMT DIFF %ld of %ld\n" : "RASFMT EQUAL %ld\n", bad ? bad : n, n);
	return bad != 0;
}
