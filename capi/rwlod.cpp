// RWLOD (LIB_RW group lod, R4 2026-11-06): the RpLODAtomic plugin's per-atomic LOD switch of MFL.exe, written op for op from the disassembly:
//   RpLODAtomicSetGeometry 0x568490, RpLODAtomicSetRange 0x568600, the render hook LODAtomicCallBackRender 0x568140 and the default LOD selector
//   LODAtomicCallBackSelectLOD 0x568030 (camera distance along the view axis * 10 / range, truncated, clamped to 0..9).
// The extension lives in the exe's atomic (RpLODAtomicGlobals.atomicOffset), the free list is the exe's; the hook and the default selector are stored
// as the exe addresses (bound), so atomics set up by exe code (the stream read callback 0x567ef0) and by this code look the same.
// Exe-layout adapter: mflrw_lod_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c).
// Check: tools/delink/rw_lod_check.sh (the original code and these exports natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint32_t u32;
static inline float R(X x){ volatile float f = (float)x; return f; }
typedef u32 __attribute__((may_alias)) UA;
static inline u32 wd(const float *p){ return *(const volatile UA *)p; }
static inline void wset(float *p, u32 w){ *(volatile UA *)p = w; }

enum { LOD_MAX = 10 };
typedef int32_t (__cdecl *SelectFn)(u8 *atomic);
typedef u8 *(__cdecl *RenderFn)(u8 *atomic);
struct LODAtomic37 {
	int32_t hooked;			// the render callback is ours
	RenderFn savedRender;		// the atomic's render callback before the hook
	SelectFn select;		// LOD selector (default LODAtomicCallBackSelectLOD)
	float farRange, invRange;	// range and 10 / range
	int32_t current;		// LOD in use, -1 before the first render
	void *geometry[LOD_MAX];
};
static_assert(sizeof(LODAtomic37) == 0x40, "LOD extension layout");
struct RpLODAtomicGlobals37 { int32_t engineOffset, atomicOffset, unused; void *freeList; };
enum { ATOMIC_FRAME = 0x04, ATOMIC_GEOMETRY = 0x18, ATOMIC_RENDER = 0x48, FRAME_LTM_AT = 0x20, FRAME_LTM_POS = 0x30 };

typedef void *(__cdecl *FreeListAllocFn)(void *freeList, u32 hint);
typedef void *(__cdecl *PtrFn)(void *p);
typedef void *(__cdecl *SetGeometryFn)(void *atomic, void *geometry, int32_t assumeSameBounds);
static char **g_engine;
static RpLODAtomicGlobals37 *g;
static PtrFn g_geomAddRef, g_geomDestroy, g_frameLTM;
static SetGeometryFn g_atomicSetGeometry;
static SelectFn g_defaultSelect;		// exe 0x568030
static RenderFn g_hook;				// exe 0x568140

static inline LODAtomic37 *&ext(u8 *atomic){ return *(LODAtomic37 **)(atomic + g->atomicOffset); }
static void initExt(LODAtomic37 *e){
	e->farRange = e->invRange = -1.0f;
	e->hooked = 0; e->savedRender = 0; e->select = g_defaultSelect; e->current = -1;
	memset(e->geometry, 0, sizeof e->geometry);
}
// the extension of an atomic, created from the free list (taking the atomic's geometry as LOD 0) on first use
static LODAtomic37 *getExt(u8 *atomic){
	LODAtomic37 *e = ext(atomic);
	if(e) return e;
	e = (LODAtomic37 *)(*(FreeListAllocFn *)(*g_engine + 0x118))(g->freeList, 0x30112);
	if(e) initExt(e);
	ext(atomic) = e;
	if(!e) return 0;
	void *geom = *(void **)(atomic + ATOMIC_GEOMETRY);
	if(geom) g_geomAddRef(geom);
	e->geometry[0] = geom;
	return e;
}
static void hook(u8 *atomic, LODAtomic37 *e){
	if(e->hooked) return;
	e->savedRender = *(RenderFn *)(atomic + ATOMIC_RENDER);
	*(RenderFn *)(atomic + ATOMIC_RENDER) = g_hook;
	e->hooked = 1;
}

extern "C" {

void __cdecl mflrw_lod_bind(char **engine, void *globals, void *geomAddRef, void *geomDestroy, void *atomicSetGeometry, void *frameGetLTM, void *defaultSelect,
                            void *renderHook){
	g_engine = engine; g = (RpLODAtomicGlobals37 *)globals; g_geomAddRef = (PtrFn)geomAddRef; g_geomDestroy = (PtrFn)geomDestroy;
	g_atomicSetGeometry = (SetGeometryFn)atomicSetGeometry; g_frameLTM = (PtrFn)frameGetLTM; g_defaultSelect = (SelectFn)defaultSelect; g_hook = (RenderFn)renderHook;
}

// 0x568490: geometry for one LOD (a reference is taken, the old one released) and the render hook in place
u8 *__cdecl rw37_RpLODAtomicSetGeometry(u8 *atomic, int32_t lod, void *geometry){
	LODAtomic37 *e = getExt(atomic);
	if(!e) return 0;
	if(geometry) g_geomAddRef(geometry);
	if(e->geometry[lod]) g_geomDestroy(e->geometry[lod]);
	e->geometry[lod] = geometry;
	hook(atomic, e);
	return atomic;
}

// 0x568600: the far range (LOD 9 from there on) and its reciprocal * 10
u8 *__cdecl rw37_RpLODAtomicSetRange(u8 *atomic, u32 farRangeWord){
	LODAtomic37 *e = getExt(atomic);
	if(!e) return 0;
	float farRange; wset(&farRange, farRangeWord);
	wset(&e->farRange, farRangeWord);
	e->invRange = R((X)10.0f / farRange);
	hook(atomic, e);
	return atomic;
}

// 0x568030: (atomic position - camera position) . camera at * invRange, truncated; -1 without an extension, 0 below 0, 9 from 10 on
int32_t __cdecl rw37_LODAtomicCallBackSelectLOD(u8 *atomic){
	u8 *camera = *(u8 **)*g_engine;
	LODAtomic37 *e = ext(atomic);
	if(!e) return -1;
	float a[3], c[3], at[3];
	const float *p = (const float *)((u8 *)g_frameLTM(*(void **)(atomic + ATOMIC_FRAME)) + FRAME_LTM_POS);
	for(int k = 0; k < 3; k++) wset(&a[k], wd(&p[k]));
	p = (const float *)((u8 *)g_frameLTM(*(void **)(camera + 4)) + FRAME_LTM_POS);
	for(int k = 0; k < 3; k++) wset(&c[k], wd(&p[k]));
	p = (const float *)((u8 *)g_frameLTM(*(void **)(camera + 4)) + FRAME_LTM_AT);
	X dx = (X)a[0] - c[0], dy = (X)a[1] - c[1], dz = (X)a[2] - c[2];
	for(int k = 0; k < 3; k++) wset(&at[k], wd(&p[k]));
	float t = R(((X)at[1] * dy + (X)at[0] * dx + (X)at[2] * dz) * e->invRange);
#ifndef MFL_FIXTURE_LOD_LRINTF
	int32_t lod = (int32_t)t;		// fistp under a chop control word, as the original
#else
	int32_t lod = (int32_t)__builtin_lrintf(t);	// negative fixture only (tools/delink/rw_lod_check.sh fixture build): round to nearest must DIFF
#endif
	if(lod < 0) return 0;
	return lod < LOD_MAX ? lod : LOD_MAX - 1;
}

// 0x568140: the render hook: select the LOD, switch the atomic's geometry when it changes, then the saved render callback (not called and NULL
// returned when that LOD has no geometry)
u8 *__cdecl rw37_LODAtomicCallBackRender(u8 *atomic){
	LODAtomic37 *e = ext(atomic);
	if(!e) return 0;
	int32_t lod = e->select(atomic);
	if(lod < 0) lod = e->current;
	else if(lod >= LOD_MAX) lod = LOD_MAX - 1;
	void *geom = e->geometry[lod];
	if(geom != *(void **)(atomic + ATOMIC_GEOMETRY)) g_atomicSetGeometry(atomic, geom, 0);
	e->current = lod;
	if(!geom) return 0;
	return e->savedRender(atomic);
}

}
