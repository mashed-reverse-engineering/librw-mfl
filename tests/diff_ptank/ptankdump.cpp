// Differential dump: links the fork's objects. For each create flag word: normalised flags, layout, then random inputs
// (written through Lock) and the fork's buildQuads output. orig_model.py recomputes everything from the inputs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rw.h"
#include "rwptank.h"
using namespace rw;
using namespace rw::ptank;
namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}
static uint32 rs = 12345;
static uint32 rnd(void) { rs = rs*1103515245u + 12345u; return rs >> 8; }
static float rf(float lo, float hi) { return lo + (hi-lo)*(rnd() & 0xFFFF)/65535.0f; }
static uint32 fb(float f) { uint32 u; memcpy(&u, &f, 4); return u; }
static const uint32 bits[9] = { 1, 8, 0x10, 4, 2, 0x40, 0x20, 0x80, 0x100 };
static const int sz[9] = { 12, 64, 12, 8, 4, 16, 4, 16, 32 };

static uint32 setupFlags(uint32 f)
{
	uint32 r = 0x10000000;
	if(f & 1) r = 0x10000001;
	if(f & 2) r |= 4; if(f & 4) r |= 2; if(f & 8) r |= 8; if(f & 0x10) r |= 0x20; if(f & 0x20) r |= 0x80;
	if(f & 0x40) r |= 0x100; if(f & 0x200) r |= 0x8000; if(f & 0x400) r |= 0x20000; if(f & 0x800) r |= 0x80000;
	if(f & 0x1000) r |= 0x100000;
	return r;
}
static const uint32 sbits[11] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x200, 0x400, 0x800, 0x1000 };

static void one(uint32 flags)
{
	int n = 1 + rnd()%6;
	Atomic *a = atomicCreate(n, flags, 0);
	printf("C %08x %08x", flags, normalizeFlags(flags));
	if(a == nil){ printf(" nil\n"); return; }
	Data *d = getData(a);
	uint8 *base = (uint8*)(((uintptr)d->block + 15) & ~(uintptr)15);
	printf(" %08x %d %d %d\n", d->formatFlags, d->numArrays, d->totalStride, n);
	for(int i = 0; i < 9; i++){
		LockArray l;
		bool32 r = atomicLock(a, &l, bits[i], DLOCKWRITE);
		printf("L %x %d %ld %d\n", bits[i], r, l.data ? (long)(l.data - base) : -1L, l.stride);
		if(!r || !l.data) continue;
		for(int k = 0; k < n; k++){
			uint8 *p = l.data + k*l.stride;
			printf("D %x %d", bits[i], k);
			for(int w = 0; w < sz[i]/4; w++){
				uint32 u;
				if(bits[i] == 2 || bits[i] == 0x40) u = rnd() ^ (rnd() << 16);
				else if(bits[i] == 0x20) u = fb(rf(-4, 4));
				else if(bits[i] == 4) u = fb(rf(0, 3));
				else if(bits[i] == 0x80 || bits[i] == 0x100) u = fb(rf(0, 1));
				else u = fb(rf(-10, 10));
				memcpy(p + 4*w, &u, 4);
				printf(" %08x", u);
			}
			printf("\n");
		}
	}
	printf("U %08x %08x\n", d->lockedFlags, d->updateFlags);
	atomicUnlock(a);
	printf("U %08x %08x\n", d->lockedFlags, d->updateFlags);
	// constants
	d->cnsCenter[0] = rf(-2, 2); d->cnsCenter[1] = rf(-2, 2);
	d->cnsSize[0] = rf(0, 3); d->cnsSize[1] = rf(0, 3);
	d->cnsRotate = rf(-4, 4);
	for(int i = 0; i < 4; i++) d->cnsColor[i] = rnd();
	for(int i = 0; i < 16; i++) ((uint8*)d->cnsVtxColor)[i] = rnd();
	for(int i = 0; i < 8; i++) d->cnsTexCoords[i] = rf(0, 1);
	float *m = (float*)&d->cnsMatrix;
	for(int i = 0; i < 16; i++) if(i != 3 && i != 7 && i != 11 && i != 15) m[i] = rf(-2, 2);
	int act = rnd() % (n + 1);
	d->activeCount = act;
	printf("K");
	for(int i = 0; i < 2; i++) printf(" %08x", fb(d->cnsCenter[i]));
	for(int i = 0; i < 2; i++) printf(" %08x", fb(d->cnsSize[i]));
	printf(" %08x", fb(d->cnsRotate));
	uint32 c; memcpy(&c, d->cnsColor, 4); printf(" %08x", c);
	for(int i = 0; i < 4; i++){ memcpy(&c, d->cnsVtxColor[i], 4); printf(" %08x", c); }
	for(int i = 0; i < 8; i++) printf(" %08x", fb(d->cnsTexCoords[i]));
	for(int i = 0; i < 16; i++) printf(" %08x", fb(m[i]));
	printf(" %d\n", act);
	Matrix cam; cam.setIdentity();
	float *cm = (float*)&cam;
	for(int i = 0; i < 16; i++) if(i != 3 && i != 7 && i != 11 && i != 15) cm[i] = rf(-1, 1);
	printf("M");
	for(int i = 0; i < 16; i++) printf(" %08x", fb(cm[i]));
	printf("\n");
	if(!(d->formatFlags & 9)){ printf("Q skip\n"); a->destroy(); return; }	// fork crashes (nil position array)
	QuadVertex q[4*8];
	int got = buildQuads(a, &cam, 0, 8, q);
	printf("Q %d\n", got);
	for(int i = 0; i < 4*got; i++){
		QuadVertex *v = &q[i];
		printf("V %08x %08x %08x %08x %08x %08x %02x%02x%02x%02x %08x %08x\n", fb(v->x), fb(v->y), fb(v->z),
		       fb(v->nx), fb(v->ny), fb(v->nz), v->r, v->g, v->b, v->a, fb(v->u), fb(v->v));
	}
	a->destroy();
}

int main(int argc, char **argv)
{
	Engine::init();
	registerPlugin();
	Engine::open(nil);
	Engine::start();
	int reps = argc > 1 ? atoi(argv[1]) : 3;
	static const uint32 lit[] = { 0x10000087, 0x87, 0x20087 };
	for(int r = 0; r < reps; r++){
		for(uint32 l : lit){ one(l); one(l | 0x20000000); }
		for(int m = 0; m < 2048; m++){
			uint32 f = 0;
			for(int b = 0; b < 11; b++) if(m & (1<<b)) f |= sbits[b];
			uint32 cf = setupFlags(f);
			one(cf);
			one((cf & ~0x10000000u) | 0x20000000u);	// interleaved variant
			one(cf | 0x01000000u);			// center variant (game never sets it)
		}
	}
	return 0;
}
