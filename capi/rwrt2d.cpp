// RWRT2D (LIB_RW group rt2d, R2 2026-11-06): the Rt2d (2D toolkit) kernels of MFL.exe that the game runs and that are not tier1 C yet, written op for op
// from their disassembly (x87 as in rwleaf2.cpp: X = a value the original keeps on the x87 stack, R() = its float store, raw word copies stay word copies):
//   _rt2dCTMGet 0x563410 (the current transform: CTM stack top * metric matrix, layer depth / view window scale, cached until a CTM change),
//   path build: Rt2dPathEmpty 0x563570, Rt2dPathMoveto 0x563600, Rt2dPathLineto 0x5636c0, Rt2dPathClose 0x5639a0, Rt2dPathDestroy 0x564450,
//   path bounds: _rt2dFlatSubPathExtendBBox 0x564220, Rt2dPathGetBBox 0x5642c0,
//   brush state: Rt2dBrushCreate 0x5649f0.
// The DLL keeps no state of its own: every field lives in the exe's Rt2dGlobals (bound below) and no render state is touched.
// Exe-layout adapter: mflrw_rt2d_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance (free +0x10c, free list
// alloc +0x118 / free +0x11c), &Rt2dGlobals.brushFreeList (0x948060, the block up to viewCamera 0x94826c), the SList functions that allocate
// (_rwSListGetNewEntry 0x5d5e90, _rwSListDestroy 0x5d6030), Rt2dPathLock 0x564510, Rt2dBrushSetUV 0x564d90, RwTextureDestroy 0x4d5bf0, _rwSqrt 0x4d5410,
// _rwInvSqrt 0x4d5470, RwMatrixMultiply 0x4d3cd0, RwMatrixScale 0x4d46e0, RwMatrixTranslate 0x4d4870, RwMatrixInvert 0x4d4490, RwFrameGetLTM 0x4d0ff0.
// Not here: path flattening (_rt2dSubPathFlatten is never run and int3-filled in the standalone link: Rt2dPathCurveto, the only producer of a
// curved sub path, is trapped too).  Rt2dPathGetBBox stops loudly on a curved sub path instead of calling it.
// Check: tools/delink/rw_rt2d_check.sh (the original code and these exports natively under Wine, x87 CW 0x7f).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint32_t u32;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
static inline float bf(u32 b){ float f; memcpy(&f, &b, 4); return f; }
// raw word copies (mov, never fld / fstp: an sNaN stays signalling as in the original); volatile, else gcc turns a copy next to a float compare into fstp
typedef u32 __attribute__((may_alias)) UA;
static inline u32 wd(const float *p){ return *(const volatile UA *)p; }
static inline void wset(float *p, u32 w){ *(volatile UA *)p = w; }
// fld + fstp of a float word (an sNaN comes out quiet, as in the original's fst); fld, fchs, fstp
static inline u32 fq(const float *p){ u32 r; __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(*p)); return r; }
static inline u32 fneg(const float *p){ u32 r; __asm__ volatile("flds %1\n\tfchs\n\tfstps %0" : "=m"(r) : "m"(*p)); return r; }

struct RwMatrix37 { float right[3]; u32 flags; float up[3]; u32 pad1; float at[3]; u32 pad2; float pos[3]; u32 pad3; };
struct RwV3d37 { float x, y, z; };
struct RwSList37 { u8 *entries; int32_t numEntries; int32_t numAlloc; int32_t entrySize; };
enum { NODE_MOVE = 0, NODE_LINE = 1 };
struct Rt2dPathNode37 { int32_t type; float x, y, nx, ny, dist; };		// position, the (smoothed) normal, the distance along the sub path
struct Rt2dPath37 {
	void *segments;			// RwSList37 * while locked (numSegment == -1), else a node array
	int32_t numSegment;
	int32_t closed, flat;
	float inset;
	Rt2dPath37 *next;		// next sub path
	Rt2dPath37 *curr;		// sub path being built (the head only)
};
// Rt2dGlobals of the exe from brushFreeList (0x948060) on; the fields before it (index tables) are not used here
struct Rt2dGlobals37 {
	void *brushFreeList;		// 0x948060
	void *brushVertexCache;
	void *fontFreeList;
	char *fontPath;
	u32 fontPathSize;
	void *fontDictNodeFreeList;
	void *fontDictionary;
	void *fontTexDictionary;
	void *currFontTexDictionary;
	void *pathFreeList;		// 0x948084
	Rt2dPath37 *scratch;		// 0x948088
	void *objectFreeList;
	void *fillFreeList;
	void *triVertFreeList;
	void *triPolyFreeList;
	u32 triVertIndex[2];
	RwMatrix37 metricMtx;		// 0x9480a4
	RwMatrix37 *ctm[32];		// 0x9480e4: the CTM stack
	int32_t mtos;			// 0x948164: its top
	float vw[2];			// 0x948168: view window (camera view window * 2)
	float vp[2];
	RwMatrix37 CTM2d, iCTM2d, CTM;	// 0x948178, 0x9481b8, 0x9481f8
	int32_t CTMValid;		// 0x948238
	u32 transformFlags;
	float layerDepth;		// 0x948240
	float tolerance, sqTolerance;
	int32_t sqToleranceValid;
	int32_t flatDepth, extensionOffset;
	float deviceX, deviceY, deviceW, deviceH;
	int32_t pixelSpace;
	u8 *viewCamera;			// 0x94826c: RwCamera (+0x04 frame, +0x14 projection)
};
static_assert(offsetof(Rt2dGlobals37, pathFreeList) == 0x24, "Rt2dGlobals layout");
static_assert(offsetof(Rt2dGlobals37, metricMtx) == 0x44, "Rt2dGlobals layout");
static_assert(offsetof(Rt2dGlobals37, mtos) == 0x104, "Rt2dGlobals layout");
static_assert(offsetof(Rt2dGlobals37, CTMValid) == 0x1d8, "Rt2dGlobals layout");
static_assert(offsetof(Rt2dGlobals37, viewCamera) == 0x20c, "Rt2dGlobals layout");
static_assert(sizeof(Rt2dPathNode37) == 0x18 && sizeof(Rt2dPath37) == 0x1c, "Rt2d path layout");

enum { rwCOMBINEPOSTCONCAT = 2, rwPARALLEL = 2 };

typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *FreeListAllocFn)(void *freeList, u32 hint);
typedef void (__cdecl *FreeListFreeFn)(void *freeList, void *p);
typedef void *(__cdecl *SListNewEntryFn)(RwSList37 *list, u32 hint);
typedef int32_t (__cdecl *SListDestroyFn)(RwSList37 *list);
typedef void *(__cdecl *PtrFn)(void *p);
typedef void *(__cdecl *BrushSetUVFn)(void *brush, const void *uv0, const void *uv1, const void *uv2, const void *uv3);
typedef X (__cdecl *SqrtFn)(u32 x);			// returns st0 as the callee leaves it
typedef void *(__cdecl *Mat3Fn)(void *dst, const void *a, const void *b);
typedef void *(__cdecl *MatV3Fn)(void *m, const void *v, int32_t combine);
typedef void *(__cdecl *Mat2Fn)(void *dst, const void *src);

static char **g_engine;
static Rt2dGlobals37 *g;
static SListNewEntryFn g_newEntry;
static SListDestroyFn g_slistDestroy;
static PtrFn g_pathLock, g_textureDestroy, g_frameGetLTM;
static BrushSetUVFn g_brushSetUV;
static SqrtFn g_sqrt, g_invSqrt;
static Mat3Fn g_mul;
static MatV3Fn g_scale, g_translate;
static Mat2Fn g_invert;

extern "C" void *__cdecl rw37_Rt2dBrushSetRGBA(u8 *b, const u8 *c0, const u8 *c1, const u8 *c2, const u8 *c3);	// rwleaf2.cpp

static void efree(void *p){ (*(FreeFn *)(*g_engine + 0x10c))(p); }
static void *flAlloc(void *fl, u32 hint){ return (*(FreeListAllocFn *)(*g_engine + 0x118))(fl, hint); }
static void flFree(void *fl, void *p){ (*(FreeListFreeFn *)(*g_engine + 0x11c))(fl, p); }
static inline Rt2dPathNode37 *slistArray(RwSList37 *l){ return l->numEntries ? (Rt2dPathNode37 *)l->entries : 0; }	// _rwSListGetArray
static inline Rt2dPathNode37 *slistEntry(RwSList37 *l, int32_t i){ return (Rt2dPathNode37 *)(l->entries + l->entrySize * i); }

// the node storage of a sub path goes (an SList while locked, else a plain block) and the sub path itself back to the free list
static void subPathFree(Rt2dPath37 *p){
	if(p->segments){
		if(p->numSegment == -1) g_slistDestroy((RwSList37 *)p->segments);
		else efree(p->segments);
	}
	p->segments = 0;
	flFree(g->pathFreeList, p);
}

// appends a line node ending at (x, y) (raw words) to the sub path's node list; the previous node's normal becomes the normalised sum of both
// normals (the join), or just the new one when the previous node starts the sub path.  Shared by Rt2dPathLineto and Rt2dPathClose.
static void addLineNode(RwSList37 *segs, u32 xw, u32 yw){
	int32_t before = segs->numEntries;
	Rt2dPathNode37 *node = (Rt2dPathNode37 *)g_newEntry(segs, 0x30190), *prev = node - 1;
	float dy = R((X)bf(yw) - prev->y);
	node->type = NODE_LINE; wset(&node->x, xw); wset(&node->y, yw);
	X t = (X)prev->x - bf(xw);		// the original squares the register value times its float store (fst + fmul [slot])
	float mdx = R(t);
	float lenSq = R(t * mdx + (X)dy * dy);
	float len = R(g_sqrt(wd(&lenSq)));
	X inv = g_invSqrt(wd(&lenSq));
	float nx = R(dy * inv);
	node->nx = nx;
	float ny = R(inv * mdx);
	node->ny = ny;
	node->dist = R((X)len + prev->dist);
	if(before > 1){
		X sx = (X)prev->nx + nx, sy = (X)prev->ny + ny;
		X k = (X)1.0f / ((X)prev->nx * sx + (X)prev->ny * sy);
		prev->nx = R(k * sx);
		prev->ny = R(k * sy);
	}else{
		wset(&prev->nx, wd(&node->nx)); wset(&prev->ny, wd(&node->ny));
	}
}

extern "C" {

void __cdecl mflrw_rt2d_bind(char **engine, void *globals, void *slistNewEntry, void *slistDestroy, void *pathLock, void *brushSetUV, void *textureDestroy,
                             void *sqrtFn, void *invSqrtFn, void *matMultiply, void *matScale, void *matTranslate, void *matInvert, void *frameGetLTM){
	g_engine = engine; g = (Rt2dGlobals37 *)globals; g_newEntry = (SListNewEntryFn)slistNewEntry; g_slistDestroy = (SListDestroyFn)slistDestroy;
	g_pathLock = (PtrFn)pathLock; g_brushSetUV = (BrushSetUVFn)brushSetUV; g_textureDestroy = (PtrFn)textureDestroy; g_sqrt = (SqrtFn)sqrtFn;
	g_invSqrt = (SqrtFn)invSqrtFn; g_mul = (Mat3Fn)matMultiply; g_scale = (MatV3Fn)matScale; g_translate = (MatV3Fn)matTranslate;
	g_invert = (Mat2Fn)matInvert; g_frameGetLTM = (PtrFn)frameGetLTM;
}

// 0x563410: the current transform for Im3D: when the cache is stale, CTM2d = ctm[top] * metric, scaled by the layer depth (1 for a parallel camera),
// by (-min(vw), min(vw)) and moved by (vw.x / 2, -vw.y / 2) * depth; iCTM2d its inverse.  Every call: CTM = CTM2d * camera frame LTM.
RwMatrix37 *__cdecl rw37__rt2dCTMGet(void){
	if(!g->CTMValid){
		u8 *cam = g->viewCamera;
		float depth; wset(&depth, *(int32_t *)(cam + 0x14) == rwPARALLEL ? 0x3f800000u : wd(&g->layerDepth));
		RwMatrix37 temp;
		g_mul(&temp, g->ctm[g->mtos], &g->metricMtx);
		float vw[2]; wset(&vw[0], wd(&g->vw[0])); wset(&vw[1], wd(&g->vw[1]));
		RwV3d37 v;
		wset(&v.x, wd(&depth)); wset(&v.y, wd(&depth)); v.z = 1.0f;
		g_scale(&temp, &v, rwCOMBINEPOSTCONCAT);
		const float *mindim = vw[0] < vw[1] ? &vw[0] : &vw[1];
		wset(&v.x, fneg(mindim)); wset(&v.y, fq(mindim)); v.z = 1.0f;
		g_scale(&temp, &v, rwCOMBINEPOSTCONCAT);
		v.x = R((X)vw[0] * depth * 0.5f);
		v.y = R(-((X)vw[1] * depth * 0.5f));
		v.z = 0.0f;
		g_translate(&temp, &v, rwCOMBINEPOSTCONCAT);
		memcpy(&g->CTM2d, &temp, sizeof temp);
		g_invert(&g->iCTM2d, &temp);
		g->CTMValid = 1;
	}
	g_mul(&g->CTM, &g->CTM2d, g_frameGetLTM(*(void **)(g->viewCamera + 4)));
	return &g->CTM;
}

// 0x564450: the sub path chain back to the free list (recursively along next); FALSE for NULL
int32_t __cdecl rw37_Rt2dPathDestroy(Rt2dPath37 *path){
	int32_t ok = path != 0;
	if(ok){
		Rt2dPath37 *next = path->next;
		subPathFree(path);
		rw37_Rt2dPathDestroy(next);
	}
	return ok;
}

// 0x563570: drops the sub paths behind the first and empties the first (node list kept, closed FALSE, flat TRUE)
Rt2dPath37 *__cdecl rw37_Rt2dPathEmpty(Rt2dPath37 *path){
	Rt2dPath37 *sub = path->next;
	if(sub){
		Rt2dPath37 *next = sub->next;
		subPathFree(sub);
		rw37_Rt2dPathDestroy(next);
	}
	path->next = 0;
	((RwSList37 *)path->segments)->numEntries = 0;
	path->closed = 0;
	path->flat = 1;
	path->curr = path;
	return path;
}

// 0x563600: a move node starts the current sub path; when it has nodes already, a new locked sub path is chained behind it (Rt2dPathLock is
// called on it twice, as in the original) and takes the move
Rt2dPath37 *__cdecl rw37_Rt2dPathMoveto(Rt2dPath37 *path, u32 xw, u32 yw){
	RwSList37 *segs = (RwSList37 *)path->curr->segments;
	if(segs->numEntries == 0){
		Rt2dPathNode37 *node = (Rt2dPathNode37 *)g_newEntry(segs, 0x30190);
		wset(&node->x, xw); wset(&node->y, yw);
		node->type = NODE_MOVE; wset(&node->nx, 0); wset(&node->ny, 0); wset(&node->dist, 0);
		return path;
	}
	Rt2dPath37 *sub = (Rt2dPath37 *)flAlloc(g->pathFreeList, 0x301a1);
	if(sub){
		sub->segments = 0; sub->numSegment = 0; sub->closed = 0; sub->flat = 1; sub->next = 0; sub->curr = sub; wset(&sub->inset, 0);
		g_pathLock(sub);
	}
	path->curr->next = sub;
	g_pathLock(path->curr->next);
	path->curr = path->curr->next;
	rw37_Rt2dPathMoveto(path, xw, yw);
	return path;
}

// 0x5636c0: a line node to (x, y) on the current sub path
Rt2dPath37 *__cdecl rw37_Rt2dPathLineto(Rt2dPath37 *path, u32 xw, u32 yw){
	addLineNode((RwSList37 *)path->curr->segments, xw, yw);
	return path;
}

// 0x5639a0: closes the current sub path: a line back to the first node unless the last one is already there, then the join normal at the
// closing point goes into both the last and the first node
Rt2dPath37 *__cdecl rw37_Rt2dPathClose(Rt2dPath37 *path){
	Rt2dPath37 *cur = path->curr;
	RwSList37 *segs = (RwSList37 *)cur->segments;
	int32_t n = segs->numEntries;
	Rt2dPathNode37 *first = slistArray(segs), *last = slistEntry(segs, n - 1);
	float fx, fy; wset(&fx, wd(&first->x)); wset(&fy, wd(&first->y));
	X dy = (X)fy - last->y;
	float dx = R((X)last->x - fx);
	if(!(dy == 0.0f) || !(dx == 0.0f)){
		addLineNode((RwSList37 *)path->curr->segments, wd(&fx), wd(&fy));
		first = slistArray(segs); last = slistEntry(segs, n);
	}
	X sx = (X)last->nx + first->nx, sy = (X)last->ny + first->ny;
	X k = (X)1.0f / ((X)first->ny * sy + (X)first->nx * sx);
	last->nx = R(k * sx);
	last->ny = R(k * sy);
	wset(&first->nx, wd(&last->nx)); wset(&first->ny, wd(&last->ny));
	cur->closed = 1;
	return path;
}

// 0x564220: grows bbox {x0, y0, x1, y1} (corners, not yet width / height) by the node positions of one flat sub path (word copies)
float *__cdecl rw37__rt2dFlatSubPathExtendBBox(Rt2dPath37 *path, float *bbox){
	int32_t locked = path->numSegment == -1;
	Rt2dPathNode37 *node = locked ? slistArray((RwSList37 *)path->segments) : (Rt2dPathNode37 *)path->segments;
	int32_t n = locked ? ((RwSList37 *)path->segments)->numEntries : path->numSegment;
	for(; n > 0; n--, node++){
		if(node->x < bbox[0]) wset(&bbox[0], wd(&node->x));
		else if(node->x > bbox[2]) wset(&bbox[2], wd(&node->x));
		if(node->y < bbox[1]) wset(&bbox[1], wd(&node->y));
		else if(node->y > bbox[3]) wset(&bbox[3], wd(&node->y));
	}
	return bbox;
}

// 0x5642c0: bounding box {x, y, w, h} of all sub paths, starting from the first node
Rt2dPath37 *__cdecl rw37_Rt2dPathGetBBox(Rt2dPath37 *path, float *bbox){
	Rt2dPathNode37 *first = path->numSegment == -1 ? slistArray((RwSList37 *)path->segments) : (Rt2dPathNode37 *)path->segments;
	u32 x = fq(&first->x), y = fq(&first->y);
	wset(&bbox[2], x); wset(&bbox[0], x);
	wset(&bbox[3], y); wset(&bbox[1], y);
	for(Rt2dPath37 *p = path; p; p = p->next){
		if(!p->flat){		// the original flattens it into Rt2dGlobals.scratch first (_rt2dSubPathFlatten): never run, trapped in the standalone link
			fprintf(stderr, "[mflrw] Rt2dPathGetBBox: curved sub path, _rt2dSubPathFlatten is not in the DLL\n");
			abort();
		}
		rw37__rt2dFlatSubPathExtendBBox(p, bbox);
	}
	bbox[2] = R((X)bbox[2] - bbox[0]);
	bbox[3] = R((X)bbox[3] - bbox[1]);
	return path;
}

// 0x5649f0: a brush from the brush free list, zeroed, white in all four corners, UV 0 everywhere, no texture, half width 0.5, flag 1 at +0x74
u8 *__cdecl rw37_Rt2dBrushCreate(void){
	u8 *b = (u8 *)flAlloc(g->brushFreeList, 0x301a2);
	if(!b) return 0;
	memset(b, 0, 0x78);
	u8 white[4] = {0xff, 0xff, 0xff, 0xff};
	rw37_Rt2dBrushSetRGBA(b, white, white, white, white);
	float uv[2]; wset(&uv[0], 0); wset(&uv[1], 0);
	g_brushSetUV(b, uv, uv, uv, uv);
	*(u32 *)(b + 0x64) &= ~2u;
	if(*(void **)(b + 0x68)) g_textureDestroy(*(void **)(b + 0x68));
	*(void **)(b + 0x68) = 0;
	*(u32 *)(b + 0x70) = 0x3f000000;
	*(u32 *)(b + 0x74) = 1;
	return b;
}

}
