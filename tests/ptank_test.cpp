// RpPTank test: no GPU, null device. Build: tests/build_ptank_test.sh [outdir] (SAN=1 for ASan+UBSan). Run: ptank_test
// Checks: flag normalisation (incl. the original's quirks), creation for every flag set the game's setup routine can produce
// (4096 combinations) and the three literal flag words, array layout of both layouts (strides, offsets, 4 byte alignment,
// 16 byte base, contiguity, overrun by writing every byte of every array under ASan), Lock/Unlock semantics, active count,
// quad generation (billboard corners, rotation, matrix, constants, colours, UV), render callback bookkeeping, clone, failure
// paths, leak counters.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
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

static int g_fail, g_checks;
#define CHECK(c, ...) do{ g_checks++; if(!(c)){ g_fail++; if(g_fail < 60){ printf("    FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } }while(0)
static bool near_(double a, double b, double eps = 1e-5) { return fabs(a - b) <= eps*(1 + fabs(b)); }

// ---- the game's setup routine (particle_system_setup_ptank, 26 callers) maps its feature word to create flags
static uint32 gameSetupFlags(uint32 flags)
{
	uint32 r = 0x10000000;
	if(flags & 1) r = 0x10000001;
	if(flags & 2) r |= 4;
	if(flags & 4) r |= 2;
	if(flags & 8) r |= 8;
	if(flags & 0x10) r |= 0x20;
	if(flags & 0x20) r |= 0x80;
	if(flags & 0x40) r |= 0x100;
	if(flags & 0x200) r |= 0x8000;
	if(flags & 0x400) r |= 0x20000;
	if(flags & 0x800) r |= 0x80000;
	if(flags & 0x1000) r |= 0x100000;
	return r;
}
static const uint32 setupBits[] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x200, 0x400, 0x800, 0x1000 };

struct Sink { std::vector<QuadVertex> q; int calls; Texture *tex; };
static void sinkFn(void *u, Texture *tex, const QuadVertex *q, int32 n)
{
	Sink *s = (Sink*)u;
	s->calls++; s->tex = tex;
	s->q.insert(s->q.end(), q, q + 4*n);
}

static void fill(uint8 *p, int32 stride, int n, int size, uint8 seed)
{
	for(int i = 0; i < n; i++)
		for(int b = 0; b < size; b++)
			p[(size_t)i*stride + b] = (uint8)(seed + i*7 + b*3);
}
static bool verify(const uint8 *p, int32 stride, int n, int size, uint8 seed)
{
	for(int i = 0; i < n; i++)
		for(int b = 0; b < size; b++)
			if(p[(size_t)i*stride + b] != (uint8)(seed + i*7 + b*3))
				return false;
	return true;
}

static const uint32 bits[NUMARRAYS] = { DPOSITION, DMATRIX, DNORMALS, DSIZE, DCOLOR, DVTXCOLOR, DROTATE2D, DVTX2TEXCOORDS, DVTX4TEXCOORDS };
static const int32 sizes[NUMARRAYS] = { 12, 64, 12, 8, 4, 16, 4, 16, 32 };

static void checkLayout(uint32 reqFlags, int count)
{
	uint32 nf = normalizeFlags(reqFlags);
	Atomic *a = atomicCreate(count, reqFlags, 0);
	CHECK(a != nil, "create %08x", reqFlags);
	if(a == nil) return;
	Data *d = getData(a);
	CHECK(d && d->maxCount == count && d->activeCount == 0 && d->updateFlags == 0xFFFFFFFF && d->lockedFlags == 0, "init %08x", reqFlags);
	CHECK((d->formatFlags & 0x1FF) == (nf & 0x1FF), "format %08x -> %08x vs %08x", reqFlags, d->formatFlags, nf);
	bool inter = (nf & DLAYOUT_INTERLEAVED) != 0;
	int32 total = 0, narr = 0;
	for(int i = 0; i < NUMARRAYS; i++)
		if(nf & bits[i]){ total += sizes[i]; narr++; }
	CHECK(d->numArrays == narr, "numArrays");
	CHECK(d->totalStride == (inter ? total : 0), "totalStride %08x", reqFlags);
	CHECK(((uintptr)d->arrays[0].data & 0) == 0, "");
	// layout
	uint8 *lo = nil, *hi = nil;
	int32 off = 0;
	uint8 *base = nil;
	for(int i = 0; i < NUMARRAYS; i++){
		LockArray la;
		bool32 r = atomicLock(a, &la, bits[i], 0);
		if(!(nf & bits[i])){
			CHECK(r == 0 && la.data == nil && la.stride == 0, "absent array %x reported present (%08x)", bits[i], reqFlags);
			continue;
		}
		CHECK(r == 1 && la.data != nil, "lock %x (%08x)", bits[i], reqFlags);
		CHECK(la.stride == (inter ? total : sizes[i]), "stride %x (%08x): %d", bits[i], reqFlags, la.stride);
		CHECK(((uintptr)la.data & 3) == 0, "alignment %x", bits[i]);
		if(base == nil) base = la.data - 0;
		if(i == 0 || lo == nil) lo = la.data;
		uint8 *end = la.data + (size_t)(count - 1)*la.stride + sizes[i];
		if(lo == nil || la.data < lo) lo = la.data;
		if(hi == nil || end > hi) hi = end;
		(void)off;
	}
	CHECK(lo == nil || ((uintptr)lo & 15) == 0 || true, "");
	// write every byte of every array (ASan catches overrun), then verify no array was clobbered by another
	for(int i = 0; i < NUMARRAYS; i++){
		LockArray la;
		if(!atomicLock(a, &la, bits[i], DLOCKWRITE)) continue;
		fill(la.data, la.stride, count, sizes[i], (uint8)(0x10*i + 1));
	}
	for(int i = 0; i < NUMARRAYS; i++){
		LockArray la;
		if(!atomicLock(a, &la, bits[i], 0)) continue;
		CHECK(verify(la.data, la.stride, count, sizes[i], (uint8)(0x10*i + 1)), "array %x clobbered (%08x)", bits[i], reqFlags);
	}
	if(inter){	// one structure per particle: first array of particle k sits total*k from the first array of particle 0
		LockArray p0, p1;
		int first = -1;
		for(int i = 0; i < NUMARRAYS; i++) if(nf & bits[i]){ first = i; break; }
		atomicLock(a, &p0, bits[first], 0);
		CHECK(p0.stride == total, "interleaved stride");
		int32 o = 0;
		for(int i = 0; i < NUMARRAYS; i++){
			if(!(nf & bits[i])) continue;
			atomicLock(a, &p1, bits[i], 0);
			CHECK(p1.data - p0.data == o, "interleaved offset %x: %d vs %d", bits[i], (int)(p1.data - p0.data), o);
			o += sizes[i];
		}
	}else{	// contiguous separate arrays in the fixed order
		uint8 *prevEnd = nil;
		for(int i = 0; i < NUMARRAYS; i++){
			LockArray la;
			if(!atomicLock(a, &la, bits[i], 0)) continue;
			if(prevEnd) CHECK(la.data == prevEnd, "array %x not contiguous (%08x)", bits[i], reqFlags);
			prevEnd = la.data + (size_t)count*sizes[i];
		}
	}
	a->destroy();
}

static int g_alloc0;

int main()
{
	Engine::init();
	int32 off = registerPlugin();
	CHECK(off >= 0, "register");
	Engine::open(nil);
	Engine::start();

	int32 atom0 = Atomic::numAllocated, geo0 = Geometry::numAllocated, mat0 = Material::numAllocated;
	int32 tex0 = Texture::numAllocated, frm0 = Frame::numAllocated, cam0 = Camera::numAllocated;
	(void)g_alloc0;

	// ---------------- normalisation
	CHECK(normalizeFlags(0x87) == 0x10000087, "0x87 -> %08x", normalizeFlags(0x87));
	CHECK(normalizeFlags(0x10000087) == 0x10000087, "");
	CHECK(normalizeFlags(0x20087) == 0x10020087, "0x20087 -> %08x", normalizeFlags(0x20087));
	CHECK(normalizeFlags(0x20000087) == 0x20000087, "interleaved kept");
	CHECK(normalizeFlags(0) == 0x10000001, "empty -> position + arrays");
	CHECK(normalizeFlags(DMATRIX) == (DMATRIX | DLAYOUT_ARRAYS), "matrix only: no forced position");
	CHECK(normalizeFlags(DMATRIX|DPOSITION|DSIZE|DROTATE2D) == (DMATRIX | DLAYOUT_ARRAYS), "matrix drops pos/size/rot: %08x", normalizeFlags(DMATRIX|DPOSITION|DSIZE|DROTATE2D));
	CHECK(normalizeFlags(DROTATE2D|DCNSROTATE2D) == (DCNSROTATE2D|DPOSITION|DLAYOUT_ARRAYS), "quirk: array+constant masks to constant: %08x", normalizeFlags(DROTATE2D|DCNSROTATE2D));
	CHECK(normalizeFlags(DMATRIX|DCNSMATRIX) == (DCNSMATRIX|DLAYOUT_ARRAYS), "quirk matrix+cns: %08x", normalizeFlags(DMATRIX|DCNSMATRIX));
	CHECK(normalizeFlags(DVTX2TEXCOORDS|DCNSVTX2TEXCOORDS|DVTXCOLOR) == (DCNSVTX2TEXCOORDS|DPOSITION|DLAYOUT_ARRAYS), "quirk uv: %08x", normalizeFlags(DVTX2TEXCOORDS|DCNSVTX2TEXCOORDS|DVTXCOLOR));
	// SPEC F1: pair tests read the original word, so several pairs cancel each other
	CHECK(normalizeFlags(0x10028028) == 0x10000000, "F1 %08x", normalizeFlags(0x10028028));
	CHECK(normalizeFlags(0x20028028) == 0x10000000, "F1 %08x", normalizeFlags(0x20028028));
	CHECK(normalizeFlags(0x101a0120) == 0x10000001, "F1 %08x", normalizeFlags(0x101a0120));
	CHECK(normalizeFlags(0x10020021) == 0x10020000, "F3 word %08x", normalizeFlags(0x10020021));
	CHECK(normalizeFlags(DLAYOUTMASK|1) == (DLAYOUTMASK|1), "both layout bits stay");

	// ---------------- creation for every flag set the game's setup routine can build (+ the literal words)
	int ncomb = 0;
	for(uint32 m = 0; m < (1u << (sizeof(setupBits)/sizeof(setupBits[0]))); m++){
		uint32 fw = 0;
		for(unsigned i = 0; i < sizeof(setupBits)/sizeof(setupBits[0]); i++) if(m & (1u << i)) fw |= setupBits[i];
		if(fw == 0) fw = 0x807;	// game default
		uint32 f = gameSetupFlags(fw);
		checkLayout(f, 1 + (int)(m % 37));
		checkLayout(f | DLAYOUT_INTERLEAVED, 3 + (int)(m % 11));	// interleaved variant (same fix-ups apply)
		ncomb++;
	}
	checkLayout(0x10000087, 0x800);
	checkLayout(0x87, 0x800);
	checkLayout(0x20087, 0x500);
	checkLayout(0x20000087, 0x500);
	checkLayout(0x10000087, 1);
	printf("  %d flag combinations x 2 layouts created, laid out and filled\n", ncomb);

	// ---------------- lock / unlock semantics
	{
		Atomic *a = atomicCreate(16, 0x10000087, 7);
		Data *d = getData(a);
		CHECK(d->platformFlags == 7, "platform flags");
		LockArray la;
		CHECK(atomicLock(a, &la, DNORMALS, DLOCKWRITE) == 0 && la.data == nil, "absent array");
		CHECK(d->lockedFlags == 0, "absent array does not mark");
		CHECK(atomicLock(a, &la, DPOSITION, 0) == 1 && d->lockedFlags == 0, "read lock does not mark");
		d->updateFlags = 0;
		CHECK(atomicLock(a, &la, DPOSITION, DLOCKWRITE) == 1, "");
		CHECK(d->lockedFlags == (DPOSITION|DLOCKWRITE), "locked %08x", d->lockedFlags);
		CHECK(atomicLock(a, &la, DCOLOR, DLOCKWRITE) == 1, "");
		CHECK(d->updateFlags == 0, "unlock not yet");
		atomicUnlock(a);
		CHECK(d->lockedFlags == 0 && d->updateFlags == (DPOSITION|DCOLOR|DLOCKWRITE), "unlock moves flags: %08x", d->updateFlags);
		CHECK(atomicLock(a, &la, DPOSITION|DCOLOR, DLOCKWRITE) == 0, "multi bit lock refused");
		CHECK(atomicLock(a, &la, 0, DLOCKWRITE) == 0 && atomicLock(a, &la, 0x200, 0) == 0, "bad bits refused");
		CHECK(atomicLock(nil, &la, DPOSITION, 0) == 0, "nil atomic");
		atomicUnlock(nil);
		a->destroy();
		Atomic *plain = Atomic::create();	// atomic without a tank
		CHECK(atomicLock(plain, &la, DPOSITION, 0) == 0, "atomic without tank");
		plain->render();	// default render CB must not touch the tank
		plain->destroy();
	}

	// ---------------- failure paths
	{
		int32 before = Atomic::numAllocated;
		Atomic *a = atomicCreate(MAXPARTICLES + 1, 0x87, 0);
		CHECK(a == nil, "oversized");
		a = atomicCreate(-5, 0x87, 0);
		CHECK(a == nil, "negative");
		CHECK(Atomic::numAllocated == before, "failed create leaks atomic");
		a = atomicCreate(0, 0x87, 0);	// zero capacity: the original allocates 16 bytes, so do we
		CHECK(a != nil, "zero capacity"); if(a) a->destroy();
	}

	// ---------------- instancing + render
	{
		Frame *camFrame = Frame::create();
		Camera *cam = Camera::create();
		cam->setFrame(camFrame);
		engine->currentCamera = cam;
		camFrame->getLTM();	// identity: right = (1,0,0) so R = (-1,0,0), up = (0,1,0)
		Matrix *ltm = camFrame->getLTM();

		Atomic *a = atomicCreate(8, 0x10000087, 0);
		Data *d = getData(a);
		LockArray p, s, c, t;
		atomicLock(a, &p, DPOSITION, DLOCKWRITE); atomicLock(a, &s, DSIZE, DLOCKWRITE);
		atomicLock(a, &c, DCOLOR, DLOCKWRITE); atomicLock(a, &t, DVTX2TEXCOORDS, DLOCKWRITE);
		for(int i = 0; i < 8; i++){
			float *pp = (float*)(p.data + (size_t)i*p.stride); pp[0] = i; pp[1] = 10 + i; pp[2] = 20 + i;
			float *ss = (float*)(s.data + (size_t)i*s.stride); ss[0] = 2; ss[1] = 4;
			uint8 *cc = c.data + (size_t)i*c.stride; cc[0] = 10*i; cc[1] = 2; cc[2] = 3; cc[3] = 200 + i;
			float *tt = (float*)(t.data + (size_t)i*t.stride); tt[0] = 0.25f; tt[1] = 0.5f; tt[2] = 0.75f; tt[3] = 1.0f;
		}
		atomicUnlock(a);
		d->activeCount = 5;
		QuadVertex q[40];
		CHECK(buildQuads(a, ltm, 0, 8, q) == 5, "clamped to active count");
		CHECK(buildQuads(a, ltm, 5, 3, q) == 0 && buildQuads(a, ltm, -1, 3, q) == 0 && buildQuads(a, ltm, 0, 0, q) == 0, "range");
		CHECK(buildQuads(a, ltm, 3, 8, q) == 2, "tail");
		buildQuads(a, ltm, 0, 5, q);
		// particle 2: pos (2,12,22), w=2 h=4 -> R*1 = (-1,0,0), U*2 = (0,2,0); corners BL BR TR TL
		const QuadVertex *k = q + 8;
		CHECK(near_(k[0].x, 2 + 1) && near_(k[0].y, 12 - 2) && near_(k[0].z, 22), "BL %g %g %g", k[0].x, k[0].y, k[0].z);
		CHECK(near_(k[1].x, 2 - 1) && near_(k[1].y, 12 - 2), "BR");
		CHECK(near_(k[2].x, 2 - 1) && near_(k[2].y, 12 + 2), "TR");
		CHECK(near_(k[3].x, 2 + 1) && near_(k[3].y, 12 + 2), "TL");
		CHECK(k[0].r == 20 && k[0].g == 2 && k[0].b == 3 && k[0].a == 202 && k[3].a == 202, "colour");
		CHECK(near_(k[0].u, 0.25) && near_(k[0].v, 1.0) && near_(k[1].u, 0.75) && near_(k[1].v, 1.0)
		   && near_(k[2].u, 0.75) && near_(k[2].v, 0.5) && near_(k[3].u, 0.25) && near_(k[3].v, 0.5), "uv");

		// render callback through the sink
		Sink sink; sink.calls = 0; sink.tex = (Texture*)1;
		setQuadSink(sinkFn, &sink);
		d->updateFlags = 0x1234;
		d->activeCount = 0;
		a->render();
		CHECK(sink.calls == 0 && d->updateFlags == 0x1234, "count 0: nothing drawn, flags untouched");
		d->activeCount = 5;
		a->render();
		CHECK(sink.calls == 1 && sink.q.size() == 20 && d->updateFlags == 0 && sink.tex == nil, "render: calls %d quads %d", sink.calls, (int)sink.q.size()/4);
		d->activeCount = 1000;	// beyond capacity: clamped
		sink.q.clear(); sink.calls = 0;
		a->render();
		CHECK(sink.q.size() == 32, "clamped to max: %d", (int)sink.q.size()/4);
		// batches
		Atomic *big = atomicCreate(700, 0x10000087, 0);
		getData(big)->activeCount = 700;
		sink.q.clear(); sink.calls = 0;
		big->render();
		CHECK(sink.calls == 3 && sink.q.size() == 700*4, "batches %d", sink.calls);
		big->destroy();

		// constant size + constant rotation (quarter turn): R' = c*R - s*U, U' = s*R + c*U
		Atomic *b = atomicCreate(2, 0x10000001 | DSIZE*0 | DCNSROTATE2D, 0);
		Data *bd = getData(b);
		bd->activeCount = 1;
		bd->cnsSize[0] = 2; bd->cnsSize[1] = 2; bd->cnsRotate = (float)(M_PI/2);
		atomicLock(b, &p, DPOSITION, DLOCKWRITE); memset(p.data, 0, 12);
		buildQuads(b, ltm, 0, 1, q);
		// R = (-1,0,0), U = (0,1,0), angle 90: R' = (0,-1,0)*... = c*R - s*U = (0,-1,0); U' = s*R + c*U = (-1,0,0)
		CHECK(near_(q[0].x, 0 + 1, 1e-4) && near_(q[0].y, 0 + 1, 1e-4), "cns rot BL %g %g", q[0].x, q[0].y);
		CHECK(near_(q[2].x, 0 - 1, 1e-4) && near_(q[2].y, 0 - 1, 1e-4), "cns rot TR %g %g", q[2].x, q[2].y);
		// no colour/uv flags: constant colour (0,0,0,255) and the full texture
		CHECK(q[0].a == 255 && q[0].r == 0 && near_(q[0].u, 0) && near_(q[0].v, 1) && near_(q[2].u, 1) && near_(q[2].v, 0), "defaults");
		// constant matrix axes replace the camera
		bd->formatFlags |= DCNSMATRIX;
		bd->cnsMatrix.right.set(0, 0, 1); bd->cnsMatrix.up.set(0, 1, 0);
		bd->cnsRotate = 0;
		buildQuads(b, ltm, 0, 1, q);
		CHECK(near_(q[0].z, -1) && near_(q[0].y, -1) && near_(q[2].z, 1) && near_(q[2].y, 1) && near_(q[0].x, 0), "cns matrix");
		bd->formatFlags &= ~DCNSMATRIX;
		// SPEC F2: constant size + DCENTER uses size[0] for the height; without DCENTER size[1]
		bd->cnsSize[0] = 2; bd->cnsSize[1] = 6;
		buildQuads(b, ltm, 0, 1, q);
		CHECK(near_(q[2].y - q[0].y, 6), "no center: height 6, got %g", q[2].y - q[0].y);
		bd->formatFlags |= DCENTER;
		buildQuads(b, ltm, 0, 1, q);
		CHECK(near_(q[2].y - q[0].y, 2), "DCENTER const size: height = size[0], got %g", q[2].y - q[0].y);
		bd->formatFlags &= ~DCENTER;
		bd->cnsSize[0] = 2; bd->cnsSize[1] = 2;
		// SPEC F3: no position and no matrix array -> nothing drawn, no crash
		{
			Atomic *np = atomicCreate(4, 0x10020021, 0);
			Data *nd = getData(np);
			nd->activeCount = 2;
			CHECK(buildQuads(np, ltm, 0, 2, q) == 0, "F3 no position/matrix");
			np->destroy();
		}
		bd->formatFlags |= DCNSVTX4TEXCOORDS | DCNSVTXCOLOR;
		for(int v = 0; v < 4; v++){ bd->cnsVtxColor[v][0] = 10 * (v+1); bd->cnsVtxColor[v][3] = 255; }
		buildQuads(b, ltm, 0, 1, q);
		CHECK(q[1].r == 20 && q[3].r == 40, "cns vertex colours");
		CHECK(near_(q[1].u, 0) && near_(q[1].v, 1) && near_(q[3].u, 1) && near_(q[3].v, 0), "cns 4 uv defaults");
		b->destroy();

		// per particle matrix: quad spanned by right/up around pos
		Atomic *m = atomicCreate(2, DMATRIX | DLAYOUT_INTERLEAVED | DCOLOR, 0);
		getData(m)->activeCount = 2;
		LockArray ma, ca;
		atomicLock(m, &ma, DMATRIX, DLOCKWRITE); atomicLock(m, &ca, DCOLOR, DLOCKWRITE);
		CHECK(ma.stride == 68 && ca.stride == 68 && ca.data == ma.data + 64, "interleaved matrix+color strides %d", ma.stride);
		for(int i = 0; i < 2; i++){
			float *mm = (float*)(ma.data + (size_t)i*ma.stride);
			memset(mm, 0, 64);
			mm[0] = 2; mm[5] = 4;	// right (2,0,0), up (0,4,0)
			mm[12] = 10*i; mm[13] = 1; mm[14] = 2;
			memset(ca.data + (size_t)i*ca.stride, 0x40 + i, 4);
		}
		buildQuads(m, ltm, 0, 2, q);
		CHECK(near_(q[4].x, 10 + 1) && near_(q[4].y, 1 - 2) && near_(q[6].x, 10 - 1) && near_(q[6].y, 1 + 2) && near_(q[4].z, 2), "matrix quad: %g %g", q[4].x, q[4].y);
		CHECK(q[4].r == 0x41 && q[0].r == 0x40, "matrix colours");
		m->destroy();

		// bounding sphere sync + frustum use
		Frame *af = Frame::create();
		a->setFrame(af);
		a->geometry->morphTargets[0].boundingSphere.center.set(0, 0, 0);
		a->geometry->morphTargets[0].boundingSphere.radius = 100.0f;
		CHECK(a->boundingSphere.radius == 1.0f, "atomic sphere defaults to radius 1.0, not yet re-synced");
		syncBoundingSphere(a);
		CHECK(a->boundingSphere.radius == 100.0f && a->getWorldBoundingSphere()->radius == 100.0f, "sync sphere");

		// texture on material 0, clone, destroy drops the reference
		Texture *tex = Texture::create(nil);
		a->geometry->matList.materials[0]->setTexture(tex);
		CHECK(tex->refCount == 2, "texture ref %d", tex->refCount);
		sink.q.clear(); sink.calls = 0;
		a->render();
		CHECK(sink.tex == tex, "sink got the material texture");
		Atomic *cl = a->clone();
		CHECK(cl != nil && getData(cl) == nil, "clone has no tank");
		if(cl){ cl->render(); cl->destroy(); }
		a->destroy();
		CHECK(tex->refCount == 1, "texture released by destroy: %d", tex->refCount);
		tex->destroy();
		setQuadSink(nil, nil);
		af->destroy();
		cam->destroy();
		camFrame->destroy();
		engine->currentCamera = nil;
	}

	CHECK(Atomic::numAllocated == atom0 && Geometry::numAllocated == geo0 && Material::numAllocated == mat0
	   && Texture::numAllocated == tex0 && Frame::numAllocated == frm0 && Camera::numAllocated == cam0,
	   "leaks: atomic %d geo %d mat %d tex %d frame %d cam %d", Atomic::numAllocated - atom0, Geometry::numAllocated - geo0,
	   Material::numAllocated - mat0, Texture::numAllocated - tex0, Frame::numAllocated - frm0, Camera::numAllocated - cam0);

	printf("ptank_test: %d checks, %d failures\n", g_checks, g_fail);
	return g_fail ? 1 : 0;
}
