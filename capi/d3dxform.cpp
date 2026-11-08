// D3DXFORM (LIB_RW group d3dxform, D4 of plan_engine_switch_window2_2026-11-08): the transform / stream / declaration caches of the RW 3.7 D3D9 driver
// (d3d9device.obj of MFL.exe), written op for op from the disassembly ("kernel on exe state": the caches stay in the exe's variables).
//   D3D9ClearCacheShaders 0x4d87e0, D3D9ClearCacheMatrix 0x4d8820, _rwD3D9SetVertexDeclaration 0x4db1b0, RwD3D9SetStreamSource 0x4db270, _rwD3D9SetStreams 0x4db2d0,
//   RwD3D9SetTransform 0x4db470, RwD3D9GetTransform 0x4db5d0, RwD3D9SetTransformWorld 0x4db600.
// State: _rwD3D9LastFVFUsed / LastVertexDeclarationUsed / LastVertexShaderUsed / LastPixelShaderUsed / LastIndexBufferUsed 0x62e760..0x62e770, LastVertexStreamUsed
// 0x7d21f0 (4 x {vb, offset, stride, -}), LastWorldMatrixUsedIdentity 0x7d2284, LastMatrixUsed 0x7d2288 (0x104 D3DMATRIX pointers, world = 0x100), MatrixFreeList
// 0x7d2698, IdentityMatrix 0x5ea090, the world D3DMATRIX scratch 0x62e7f0.  The device is the exe's (*0x7d2240): SetTransform 44, SetVertexDeclaration 87,
// SetStreamSource 100.  Matrices come from the exe's free list (RwEngineInstance +0x118 alloc, +0x11c free, RwFreeListCreate / Purge); no CRT heap.
// Exe-layout adapter: mflrw_d3dxform_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612.c), table layout = enum B below.
// Check: ghidra tools/delink/rw_d3dxform_check.sh (fake device tools/delink/fakedev.c; both sides run the same op sequences natively under Wine).
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;

// bind table layout (an_hk_612.c builds it in this order)
enum B { B_DEVVAR, B_ENGINEVAR, B_LASTFVF, B_LASTDECL, B_LASTVS, B_LASTPS, B_LASTIB, B_STREAMS, B_WORLDIDENT, B_MATRICES, B_MATFL, B_IDENTITY, B_WORLDD3D,
         B_FLCREATE, B_FLPURGE, B_N };
static void *const *g;

typedef u32 __attribute__((may_alias)) UA;
#define V(i) (*(UA *)g[i])
#define STREAM(s, f) (((UA *)g[B_STREAMS])[4 * (s) + (f)])	// {vb, offset, stride, -}
#define SLOT(i) (((UA *)g[B_MATRICES])[i])
#define IDENT ((const u8 *)g[B_IDENTITY])
#define WD ((UA *)g[B_WORLDD3D])
enum { WORLD = 0x100, HINT = 0x30411 };

static inline u8 *DEV(void){ return *(u8 **)g[B_DEVVAR]; }
static inline void **VT(void *o){ return *(void ***)o; }
static inline i32 devSetTransform(u32 st, const void *m){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, const void *))VT(d)[44])(d, st, m); }
static inline i32 devSetDecl(u32 decl){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32))VT(d)[87])(d, decl); }
static inline i32 devSetStream(u32 s, u32 vb, u32 off, u32 stride){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32))VT(d)[100])(d, s, vb, off, stride); }
static inline u8 *ENG(void){ return *(u8 **)g[B_ENGINEVAR]; }
static inline u32 flAlloc(u32 list, u32 hint){ return (u32)(uintptr_t)(*(void *(__cdecl **)(u32, u32))(ENG() + 0x118))(list, hint); }
static inline void flFree(u32 list, u32 p){ (*(void (__cdecl **)(u32, u32))(ENG() + 0x11c))(list, p); }
static inline bool same16(const void *a, const void *b){	// the original's word loop (no CRT memcmp: M1)
	for (int i = 0; i < 16; i++) if (((const UA *)a)[i] != ((const UA *)b)[i]) return false;
	return true;
}

extern "C" {

i32 __cdecl mflrw_d3dxform_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// D3D9ClearCacheShaders 0x4d87e0: FVF, declaration, shaders, index buffer unknown; the 4 stream caches {-1, 0, 0}
void __cdecl rw37_D3D9ClearCacheShaders(void){
	V(B_LASTFVF) = 0xffffffffu; V(B_LASTDECL) = 0xffffffffu; V(B_LASTVS) = 0xffffffffu; V(B_LASTPS) = 0xffffffffu; V(B_LASTIB) = 0xffffffffu;
	for (int s = 0; s < 4; s++) { STREAM(s, 0) = 0xffffffffu; STREAM(s, 1) = 0; STREAM(s, 2) = 0; }
}

// D3D9ClearCacheMatrix 0x4d8820: every cached matrix back to the free list (then purged), or the free list created; the world slot gets the identity
void __cdecl rw37_D3D9ClearCacheMatrix(void){
	V(B_WORLDIDENT) = 0;
	if (V(B_MATFL)) {
		for (int i = 0; i < 0x104; i++) if (SLOT(i)) { flFree(V(B_MATFL), SLOT(i)); SLOT(i) = 0; }
		((void (__cdecl *)(u32))g[B_FLPURGE])(V(B_MATFL));
	} else {
		V(B_MATFL) = ((u32 (__cdecl *)(u32, u32, u32, u32))g[B_FLCREATE])(0x40, 0xf, 0x10, HINT);
		for (int i = 0; i < 0x104; i++) SLOT(i) = 0;
	}
	SLOT(WORLD) = flAlloc(V(B_MATFL), HINT);
	memcpy((void *)(uintptr_t)SLOT(WORLD), IDENT, 64);
}

// _rwD3D9SetVertexDeclaration 0x4db1b0
void __cdecl rw37__rwD3D9SetVertexDeclaration(u32 decl){
	if (V(B_LASTDECL) == decl) return;
	i32 hr = devSetDecl(decl);
	V(B_LASTDECL) = 0xffffffffu;
	if (hr >= 0) V(B_LASTDECL) = decl;
	V(B_LASTFVF) = 0xffffffffu;
}

// RwD3D9SetStreamSource 0x4db270
void __cdecl rw37_RwD3D9SetStreamSource(u32 s, u32 vb, u32 off, u32 stride){
	if (STREAM(s, 0) == vb && STREAM(s, 1) == off && STREAM(s, 2) == stride) return;
	STREAM(s, 0) = vb; STREAM(s, 1) = off; STREAM(s, 2) = stride;
	devSetStream(s, vb, off, stride);
}

// _rwD3D9SetStreams 0x4db2d0: the 2 streams {vb, offset, stride, -} of an instanced mesh, offsets used or forced to 0; a null buffer clears a set stream
void __cdecl rw37__rwD3D9SetStreams(const u32 *streams, i32 useOffsets){
	for (u32 s = 0; s < 2; s++) {
		const u32 *st = streams + 4 * s;
		u32 vb = st[0], off = useOffsets ? st[1] : 0;
		if (vb) {
			if (STREAM(s, 0) == vb && STREAM(s, 1) == off && STREAM(s, 2) == st[2]) continue;
			STREAM(s, 0) = vb; STREAM(s, 1) = off; STREAM(s, 2) = st[2];
			devSetStream(s, vb, off, st[2]);
		} else if (STREAM(s, 0) != 0) {
			STREAM(s, 0) = 0; STREAM(s, 1) = 0; STREAM(s, 2) = 0;
			devSetStream(s, 0, 0, 0);
		}
	}
}

// RwD3D9SetTransform 0x4db470: a matrix equal to the cached one is not sent; null = identity; the world state also keeps the identity flag
i32 __cdecl rw37_RwD3D9SetTransform(u32 state, const u32 *m){
	const u8 *src = m ? (const u8 *)m : IDENT;
	u32 cur = SLOT(state);
	if (cur && same16((const void *)(uintptr_t)cur, src)) return 1;
	if (!cur) SLOT(state) = flAlloc(V(B_MATFL), HINT);
	memcpy((void *)(uintptr_t)SLOT(state), src, 64);
	i32 hr = devSetTransform(state, src);
	if (state == WORLD) V(B_WORLDIDENT) = m ? 0 : 1;
	return hr >= 0;
}

// RwD3D9GetTransform 0x4db5d0
void __cdecl rw37_RwD3D9GetTransform(u32 state, u32 *out){
	if (!out) return;
	const void *src = SLOT(state) ? (const void *)(uintptr_t)SLOT(state) : (const void *)IDENT;
	memcpy(out, src, 64);
}

// RwD3D9SetTransformWorld 0x4db600: the LTM as a D3DMATRIX in the scratch 0x62e7f0 (4th column kept); identity (null or rwMATRIXINTERNALIDENTITY) by the flag
i32 __cdecl rw37_RwD3D9SetTransformWorld(const u32 *ltm){
	if (ltm && !(ltm[3] & 0x20000)) {
		WD[0] = ltm[0]; WD[1] = ltm[1]; WD[2] = ltm[2];
		WD[4] = ltm[4]; WD[5] = ltm[5]; WD[6] = ltm[6];
		WD[8] = ltm[8]; WD[9] = ltm[9]; WD[10] = ltm[10];
		WD[12] = ltm[12]; WD[13] = ltm[13]; WD[14] = ltm[14];
		void *slot = (void *)(uintptr_t)SLOT(WORLD);
		if (!V(B_WORLDIDENT) && same16(slot, WD)) return 1;
		memcpy(slot, WD, 64);
		i32 hr = devSetTransform(WORLD, WD);
		V(B_WORLDIDENT) = 0;
		return hr >= 0;
	}
	if (V(B_WORLDIDENT)) return 1;
	memcpy((void *)(uintptr_t)SLOT(WORLD), IDENT, 64);
	i32 hr = devSetTransform(WORLD, IDENT);
	V(B_WORLDIDENT) = 1;
	return hr >= 0;
}

}
