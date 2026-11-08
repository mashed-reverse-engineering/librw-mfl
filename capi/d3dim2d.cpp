// D3DIM2D (LIB_RW group d3dim2d, D2 of plan_engine_switch_2026-11-07_v2): the still-original functions of the RW 3.7 D3D9 driver's Im2D renderer
// (d3d92drend.obj) and vertex buffer manager (d3d9vertexbuffer.obj) of MFL.exe, written op for op from the disassembly ("kernel on exe state": the
// 2D vertex / index buffer state, the managed-VB stride lists, the dynamic-VB list and the 4-slot dynamic ring stay in the exe's bss 0x7d51c8..0x7d522c).
//   _rwD3D9Im2DRenderOpen 0x4eb500, _rwD3D9Im2DRenderClose 0x4eb670, rwD3D9Im2DRenderLine 0x4eb6d0 (exe LAB_004eb6d0), rxD3D9Im2DRenderFlush 0x4eb890,
//   rwD3D9Im2DRenderTriangle 0x4eb960 (LAB_004eb960), _rwD3D9Im2DRenderPrimitive 0x4ebb80, rwD3D9Im2DRenderIndexedPrimitive 0x4ebdb0 (LAB_004ebdb0),
//   RwD3D9CreateVertexBuffer 0x4ec6d0, RwD3D9DestroyVertexBuffer 0x4ec870, _rwD3D9DynamicVertexBufferManagerForceDiscard 0x4ec9c0 (FUN_004ec9c0),
//   _rwD3D9DynamicVertexBufferRelease 0x4eca00, _rwD3D9DynamicVertexBufferRestore 0x4eca90, D3D9DynamicVertexBufferManagerCreate 0x4ecb00,
//   RwD3D9DynamicVertexBufferCreate 0x4ecbc0, RwD3D9DynamicVertexBufferLock 0x4eccd0, _rwD3D9VertexBufferManagerOpen 0x4ece70, _rwD3D9VertexBufferManagerClose 0x4ecee0.
// Not here (C in the exe): RwD3D9DynamicVertexBufferDestroy 0x4ecca0 (the original inlines it, so it is inlined here too), RwD3D9DynamicVertexBufferUnlock 0x4ece50.
// The device is the exe's (*0x7d2240): GetAvailableTextureMem 4, CreateVertexBuffer 26, CreateIndexBuffer 27, DrawPrimitive 81, DrawIndexedPrimitive 82; buffers:
// Release 2, Lock 11, Unlock 12.  Entries come from the exe's free lists through RwEngineInstance (+0x118 alloc, +0x11c free); no CRT heap.
// Exe-layout adapter: mflrw_d3dim2d_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612.c), table layout = enum B below.
// Check: ghidra tools/delink/rw_d3dim2d_check.sh (fake device tools/delink/fakedev.c; both sides run the same op sequences natively under Wine).
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef long double X;

// bind table layout (an_hk_612.c builds it in this order)
enum B { B_DEVVAR, B_CAPS, B_ENGINEVAR, B_BSS, B_DEFVBSIZE, B_CAMERAVAR, B_PRIMTAB,
         B_GETCTX, B_SETDECL, B_SETVS, B_SETPS, B_SETSTREAM, B_SETINDICES, B_CREATEDECL, B_DELETEDECL,
         B_SETRS, B_SETTSS, B_FLUSHCACHE, B_GETRASTERSTAGE, B_FLCREATE, B_FLDESTROY, B_N };
static void *const *g;

typedef u32 __attribute__((may_alias)) UA;
#define A(va) (*(UA *)((u8 *)g[B_BSS] + ((va) - 0x7d51c8u)))
// d3d92drend
#define OWNVB A(0x7d51c8)	// UseOwnVertexBuffer
#define BASE A(0x7d51cc)	// CurrentBaseIndex
#define VB A(0x7d51d0)		// CurrentVertexBuffer
#define IBOFF A(0x7d51d4)	// IB2DOffset
#define IB A(0x7d51d8)		// IndexBuffer2D
#define DECL A(0x7d51dc)	// VertexDeclIm2D
// d3d9vertexbuffer
#define STRIDES A(0x7d51e0)
#define STRIDEFL A(0x7d51e4)
#define FREEVBFL A(0x7d51e8)
#define CREATEDFL A(0x7d51ec)
#define DYNLIST A(0x7d51f0)
#define DYNFL A(0x7d51f4)
#define CUR A(0x7d51f8)
#define OFF(n) A(0x7d51fc + 4 * (n))
#define SIZE(n) A(0x7d520c + 4 * (n))
#define MGR(n) A(0x7d521c + 4 * (n))
#define MGRP(n) (&MGR(n))

// list nodes: stride {stride, freelist, vblist, next}, free block {offset, size, vb, next, prev}, created {vb, next}, dynamic {size, used, vb, var, next}
#define F(p, i) (((UA *)(uintptr_t)(p))[i])
enum { S_STRIDE, S_FREE, S_VBS, S_NEXT };
enum { FB_OFF, FB_SIZE, FB_VB, FB_NEXT, FB_PREV };
enum { C_VB, C_NEXT };
enum { D_SIZE, D_USED, D_VB, D_VAR, D_NEXT };

static inline u32 hwtl(void){ return *(const u32 *)((const u8 *)g[B_CAPS] + 0x1c) & 0x10000; }	// D3DCAPS9.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT
static inline u8 *DEV(void){ return *(u8 **)g[B_DEVVAR]; }
static inline void **VT(void *o){ return *(void ***)o; }
static inline u32 devTexMem(void){ u8 *d = DEV(); return ((u32 (__stdcall *)(void *))VT(d)[4])(d); }
static inline i32 devCreateVB(u32 len, u32 usage, u32 fvf, u32 pool, UA *out){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, UA *, void *))VT(d)[26])(d, len, usage, fvf, pool, out, 0); }
static inline i32 devCreateIB(u32 len, u32 usage, u32 fmt, u32 pool, UA *out){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, UA *, void *))VT(d)[27])(d, len, usage, fmt, pool, out, 0); }
static inline i32 devDraw(u32 type, u32 start, u32 count){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32))VT(d)[81])(d, type, start, count); }
static inline i32 devDrawIndexed(u32 type, u32 base, u32 minIdx, u32 nv, u32 start, u32 count){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, u32))VT(d)[82])(d, type, base, minIdx, nv, start, count); }
static inline void bufRelease(u32 b){ void *o = (void *)(uintptr_t)b; ((u32 (__stdcall *)(void *))VT(o)[2])(o); }
static inline i32 bufLock(u32 b, u32 off, u32 size, void *pp, u32 flags){ void *o = (void *)(uintptr_t)b; return ((i32 (__stdcall *)(void *, u32, u32, void *, u32))VT(o)[11])(o, off, size, pp, flags); }
static inline void bufUnlock(u32 b){ void *o = (void *)(uintptr_t)b; ((i32 (__stdcall *)(void *))VT(o)[12])(o); }
// RwFreeListAlloc / Free through the engine instance (RwEngineInstance->memoryFuncs)
static inline u32 flAlloc(u32 list, u32 hint){ u8 *e = *(u8 **)g[B_ENGINEVAR]; return (u32)(uintptr_t)(*(void *(__cdecl **)(u32, u32))(e + 0x118))(list, hint); }
static inline void flFree(u32 list, u32 p){ u8 *e = *(u8 **)g[B_ENGINEVAR]; (*(void (__cdecl **)(u32, u32))(e + 0x11c))(list, p); }

#define CALL(i, T) ((T)g[i])
typedef void (__cdecl *Fn1)(u32);
typedef void (__cdecl *Fn2)(u32, u32);
typedef void (__cdecl *Fn3)(u32, u32, u32);
typedef void (__cdecl *Fn4)(u32, u32, u32, u32);
typedef u32 (__cdecl *FnR0)(void);
typedef u32 (__cdecl *FnR1)(u32);
typedef i32 (__cdecl *CreateDeclFn)(const void *elems, UA *out);
typedef u32 (__cdecl *FlCreateFn)(u32 size, u32 entries, u32 align, u32 hint);

static inline u8 *camRaster(void){ return *(u8 **)(*(u8 **)g[B_CAMERAVAR] + 0x60); }	// the current camera's frame buffer raster
static inline u32 primType(i32 t){ return ((const u32 *)g[B_PRIMTAB])[t]; }		// rwPRIMTYPE -> D3DPRIMITIVETYPE (0x5ea28c)

// RwD3D9DynamicVertexBufferDestroy 0x4ecca0, inlined by the original: mark the list entry of vb unused
static inline void dynDestroy(u32 vb){
	for (u32 e = DYNLIST; e; e = F(e, D_NEXT)) if (F(e, D_VB) == vb) { F(e, D_USED) = 0; F(e, D_VAR) = 0; return; }
}
// D3D9DynamicVertexBufferManagerDestroy, inlined by Release and Close
static inline void ringDestroy(void){
	CUR = 0;
	for (u32 n = 0; n < 4; n++) { OFF(n) = 0; SIZE(n) = 0; if (MGR(n)) { dynDestroy(MGR(n)); MGR(n) = 0; } }
}
static inline void unlinkFree(u32 s, u32 f){
	if (F(f, FB_PREV)) F(F(f, FB_PREV), FB_NEXT) = F(f, FB_NEXT);
	if (F(f, FB_NEXT)) F(F(f, FB_NEXT), FB_PREV) = F(f, FB_PREV);
	if (F(s, S_FREE) == f) F(s, S_FREE) = F(f, FB_NEXT);
	flFree(FREEVBFL, f);
}
// an Im2D vertex {x, y, z, rhw, colour, u, v} with the raster offset added to x / y (x87 at the caller's precision, as the original's fild / fadd / fstp)
static inline void put(u8 *o, const u8 *v, i16 ox, i16 oy){
	float *p = (float *)o; const float *s = (const float *)v;
	p[0] = (float)((X)ox + (X)s[0]);
	p[1] = (float)((X)oy + (X)s[1]);
	memcpy(o + 8, v + 8, 20);
}
static inline i16 rasOX(const u8 *r){ return *(const i16 *)(r + 0x1c); }
static inline i16 rasOY(const u8 *r){ return *(const i16 *)(r + 0x1e); }

extern "C" {

i32 __cdecl rw37_RwD3D9DynamicVertexBufferCreate(u32 size, UA *vb);
i32 __cdecl rw37_D3D9DynamicVertexBufferManagerCreate(void);
i32 __cdecl rw37_RwD3D9DynamicVertexBufferLock(u32 vs, u32 nv, UA *vbOut, void *dataOut, UA *baseOut);
void __cdecl rw37__rwD3D9VertexBufferManagerClose(void);

i32 __cdecl mflrw_d3dim2d_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// _rwD3D9Im2DRenderOpen 0x4eb500: own 2D vertex buffer (no hardware T&L and more than 8 MB texture memory), the index buffer, the vertex declaration
i32 __cdecl rw37__rwD3D9Im2DRenderOpen(void){
	OWNVB = 0;
	if (!hwtl() && devTexMem() > 0x800000u && devCreateVB(0x40000, 0x228, 0, 0, &VB) == 0) { OWNVB = 1; BASE = 0; }
	IBOFF = 0;
	if (devCreateIB(0x4e20, 0x228, 0x65, 0, &IB) == 0) {
		static const u8 tmpl[32] = { 0,0, 0,0, 3, 0, 9, 0,  0,0, 0x10,0, 4, 0, 0xa, 0,  0,0, 0x14,0, 1, 0, 5, 0,  0xff,0, 0,0, 0x11, 0, 0, 0 };
		u8 elems[32];
		memcpy(elems, tmpl, 32);
		CALL(B_CREATEDECL, CreateDeclFn)(elems, &DECL);
		return 1;
	}
	if (DECL) { CALL(B_DELETEDECL, Fn1)(DECL); DECL = 0; }
	if (OWNVB && VB) { bufRelease(VB); VB = 0; }
	if (IB) { bufRelease(IB); IB = 0; }
	return 0;
}

// _rwD3D9Im2DRenderClose 0x4eb670
void __cdecl rw37__rwD3D9Im2DRenderClose(void){
	if (DECL) { CALL(B_DELETEDECL, Fn1)(DECL); DECL = 0; }
	if (OWNVB && VB) { bufRelease(VB); VB = 0; }
	if (IB) { bufRelease(IB); IB = 0; }
}

// rxD3D9Im2DRenderFlush 0x4eb890: declaration, no shaders, lighting / clipping off, stage 0 colour / alpha ops by texture, stream 0, then the render
// state flush (tail call; its eax is the return)
u32 __cdecl rw37_rxD3D9Im2DRenderFlush(void){
	CALL(B_SETDECL, Fn1)(DECL);
	CALL(B_SETVS, Fn1)(0);
	CALL(B_SETPS, Fn1)(0);
	CALL(B_SETRS, Fn2)(0x88, 0);
	CALL(B_SETRS, Fn2)(0x89, 0);
	Fn3 tss = CALL(B_SETTSS, Fn3);
	if (CALL(B_GETRASTERSTAGE, FnR1)(0) == 0) { tss(0, 1, 3); tss(0, 3, 0); tss(0, 4, 3); tss(0, 6, 0); }
	else { tss(0, 1, 4); tss(0, 2, 2); tss(0, 3, 0); tss(0, 4, 4); tss(0, 5, 2); tss(0, 6, 0); }
	CALL(B_SETSTREAM, Fn4)(0, VB, 0, 0x1c);
	return CALL(B_FLUSHCACHE, FnR0)();
}

// the vertex lock shared by the renderers: n vertices of 0x1c bytes in the own buffer (discard at the 0x2492 vertex wrap) or the dynamic ring
static inline void lockVerts(u32 n, u8 **p){
	if (OWNVB) {
		u32 off = BASE;
		if (off + n > 0x2492u) { BASE = 0; bufLock(VB, 0, n * 0x1c, p, 0x2800); }
		else bufLock(VB, off * 0x1c, n * 0x1c, p, 0x1800);
	} else rw37_RwD3D9DynamicVertexBufferLock(0x1c, n, &VB, p, &BASE);
}

// rwD3D9Im2DRenderLine 0x4eb6d0
i32 __cdecl rw37_rwD3D9Im2DRenderLine(const u8 *verts, i32 numVerts, i32 v1, i32 v2){
	u8 *p = 0, *ras = camRaster();
	lockVerts(2, &p);
	if (!p) return 0;
	i16 ox = rasOX(ras), oy = rasOY(ras);
	put(p, verts + 0x1c * v1, ox, oy);
	put(p + 0x1c, verts + 0x1c * v2, ox, oy);
	bufUnlock(VB);
	rw37_rxD3D9Im2DRenderFlush();
	i32 hr = devDraw(2, BASE, 1);
	BASE += 2;
	return hr >= 0;
}

// rwD3D9Im2DRenderTriangle 0x4eb960
i32 __cdecl rw37_rwD3D9Im2DRenderTriangle(const u8 *verts, i32 numVerts, i32 v1, i32 v2, i32 v3){
	u8 *p = 0, *ras = camRaster();
	lockVerts(3, &p);
	if (!p) return 0;
	i16 ox = rasOX(ras), oy = rasOY(ras);
	put(p, verts + 0x1c * v1, ox, oy);
	put(p + 0x1c, verts + 0x1c * v2, ox, oy);
	put(p + 0x38, verts + 0x1c * v3, ox, oy);
	bufUnlock(VB);
	rw37_rxD3D9Im2DRenderFlush();
	i32 hr = devDraw(4, BASE, 1);
	BASE += 3;
	return hr >= 0;
}

// the primitive count of n vertices / indices (line list, poly line, triangle list, strip, fan; anything else 0)
static inline u32 primCount(i32 type, i32 n){
	switch (type) {
	case 1: return (u32)(n / 2);
	case 2: return (u32)(n - 1);
	case 3: return (u32)(n / 3);
	case 4: case 5: return (u32)(n - 2);
	default: return 0;
	}
}
// n vertices copied with the raster offset (memcpy when it is 0, as the original's rep movs)
static inline void putN(u8 *p, const u8 *verts, i32 n, const u8 *ras){
	i16 ox = rasOX(ras), oy = rasOY(ras);
	if (ox == 0 && oy == 0) { memcpy(p, verts, (u32)n * 0x1c); return; }
	for (i32 i = 0; i < n; i++) put(p + 0x1c * i, verts + 0x1c * i, ox, oy);
}

// _rwD3D9Im2DRenderPrimitive 0x4ebb80 (no camera: the current raster context)
i32 __cdecl rw37__rwD3D9Im2DRenderPrimitive(i32 type, const u8 *verts, i32 n){
	u8 *cam = *(u8 **)g[B_CAMERAVAR], *p = 0;
	u8 *ras = cam ? *(u8 **)(cam + 0x60) : (u8 *)(uintptr_t)CALL(B_GETCTX, FnR0)();
	u32 pc = primCount(type, n);
	lockVerts((u32)n, &p);
	if (!p) return 0;
	putN(p, verts, n, ras);
	bufUnlock(VB);
	rw37_rxD3D9Im2DRenderFlush();
	i32 hr = devDraw(primType(type), BASE, pc);
	BASE += n;
	return hr >= 0;
}

// rwD3D9Im2DRenderIndexedPrimitive 0x4ebdb0: at most as many indices as vertices are expanded into the vertex buffer (DrawPrimitive); more indices go
// through the 2D index buffer (10000 indices, discard at the wrap; DrawIndexedPrimitive)
i32 __cdecl rw37_rwD3D9Im2DRenderIndexedPrimitive(i32 type, const u8 *verts, i32 nv, const u16 *idx, i32 ni){
	u8 *p = 0, *ras = camRaster();
	u32 pc = primCount(type, ni);
	i32 hr;
	if (ni <= nv) {
		lockVerts((u32)ni, &p);
		if (!p) return 0;
		i16 ox = rasOX(ras), oy = rasOY(ras);
		if (ox == 0 && oy == 0) { for (i32 i = 0; i < ni; i++) memcpy(p + 0x1c * i, verts + 0x1c * idx[i], 0x1c); }
		else for (i32 i = 0; i < ni; i++) put(p + 0x1c * i, verts + 0x1c * idx[i], ox, oy);
		bufUnlock(VB);
		rw37_rxD3D9Im2DRenderFlush();
		hr = devDraw(primType(type), BASE, pc);
		BASE += ni;
		return hr >= 0;
	}
	lockVerts((u32)nv, &p);
	if (!p) return 0;
	putN(p, verts, nv, ras);
	bufUnlock(VB);
	if (!IB || ni > 0x2710) return 0;
	u32 ibo = IBOFF;
	if (ibo + ni > 0x2710u) { IBOFF = 0; hr = bufLock(IB, 0, ni * 2, &p, 0x2800); }
	else hr = bufLock(IB, ibo * 2, ni * 2, &p, 0x1800);
	if (hr >= 0) {
		memcpy(p, idx, (u32)ni * 2);
		bufUnlock(IB);
		rw37_rxD3D9Im2DRenderFlush();
		CALL(B_SETINDICES, Fn1)(IB);
		hr = devDrawIndexed(primType(type), BASE, 0, nv, IBOFF, pc);
		IBOFF += ni;
	}
	BASE += nv;
	return hr >= 0;
}

// RwD3D9CreateVertexBuffer 0x4ec6d0: a block of a shared managed vertex buffer of this stride (a new buffer of DefaultVBSize rounded to the stride,
// or size if larger, when no free block fits)
i32 __cdecl rw37_RwD3D9CreateVertexBuffer(u32 stride, u32 size, UA *vb, UA *offset){
	u32 s, f;
	for (s = STRIDES; s; s = F(s, S_NEXT)) if (F(s, S_STRIDE) == stride) break;
	if (!s) {
		s = flAlloc(STRIDEFL, 0x30411);
		F(s, S_STRIDE) = stride; F(s, S_FREE) = 0; F(s, S_VBS) = 0; F(s, S_NEXT) = STRIDES; STRIDES = s;
	}
	for (f = F(s, S_FREE); f; f = F(f, FB_NEXT)) if (F(f, FB_SIZE) >= size) break;
	if (!f) {
		f = flAlloc(FREEVBFL, 0x30411);
		F(f, FB_OFF) = 0;
		u32 def = ((*(const u32 *)g[B_DEFVBSIZE] + stride - 1) / stride) * stride;
		F(f, FB_SIZE) = def;
		if (size >= def) F(f, FB_SIZE) = size;
		if (devCreateVB(F(f, FB_SIZE), 8, 0, 1, &F(f, FB_VB)) != 0) { flFree(FREEVBFL, f); return 0; }
		F(f, FB_PREV) = 0; F(f, FB_NEXT) = F(s, S_FREE);
		if (F(s, S_FREE)) F(F(s, S_FREE), FB_PREV) = f;
		F(s, S_FREE) = f;
		u32 c = flAlloc(CREATEDFL, 0x30411);
		F(c, C_VB) = F(f, FB_VB); F(c, C_NEXT) = F(s, S_VBS); F(s, S_VBS) = c;
	}
	*vb = F(f, FB_VB);
	u32 o = F(f, FB_OFF);
	*offset = o;
	u32 no = o + size;
	F(f, FB_OFF) = no;
	F(f, FB_SIZE) += o - no;
	if (F(f, FB_SIZE) == 0) unlinkFree(s, f);
	return 1;
}

// RwD3D9DestroyVertexBuffer 0x4ec870: the block goes back to the free list, merged with the free blocks it touches
void __cdecl rw37_RwD3D9DestroyVertexBuffer(u32 stride, u32 size, u32 vb, u32 offset){
	u32 s, f;
	for (s = STRIDES; s; s = F(s, S_NEXT)) if (F(s, S_STRIDE) == stride) break;
	if (!s) return;
	for (f = F(s, S_FREE); f; f = F(f, FB_NEXT))
		if (F(f, FB_VB) == vb && (F(f, FB_OFF) + F(f, FB_SIZE) == offset || offset + size == F(f, FB_OFF))) break;
	if (!f) {
		f = flAlloc(FREEVBFL, 0x30411);
		F(f, FB_OFF) = offset; F(f, FB_SIZE) = size; F(f, FB_VB) = vb;
		F(f, FB_PREV) = 0; F(f, FB_NEXT) = F(s, S_FREE);
		if (F(s, S_FREE)) F(F(s, S_FREE), FB_PREV) = f;
		F(s, S_FREE) = f;
		return;
	}
	u32 no = offset, ns = size;
	for (;;) {
		u32 fs = F(f, FB_SIZE), fo = F(f, FB_OFF);
		if (fo + fs == no) F(f, FB_SIZE) = fs + ns;
		else if (ns + no == fo) { F(f, FB_OFF) = no; F(f, FB_SIZE) = fs + ns; }
		no = F(f, FB_OFF); ns = F(f, FB_SIZE);
		u32 old = f;
		for (f = F(s, S_FREE); f; f = F(f, FB_NEXT))
			if (F(f, FB_VB) == vb && (F(f, FB_OFF) + F(f, FB_SIZE) == no || ns + no == F(f, FB_OFF))) break;
		if (!f) return;
		unlinkFree(s, old);
	}
}

// _rwD3D9DynamicVertexBufferManagerForceDiscard 0x4ec9c0: every ring slot starts over, the next slot becomes current
void __cdecl rw37__rwD3D9DynamicVertexBufferManagerForceDiscard(void){
	OFF(0) = 0; OFF(1) = 0; OFF(2) = 0; OFF(3) = 0;
	u32 c = CUR + 1;
	CUR = c;
	if (c >= 4) CUR = 0;
}

// _rwD3D9DynamicVertexBufferRelease 0x4eca00 (before a device reset): the ring, then (hardware T&L: video memory) every dynamic buffer
void __cdecl rw37__rwD3D9DynamicVertexBufferRelease(void){
	ringDestroy();
	if (!hwtl()) return;
	for (u32 e = DYNLIST, next; e; e = next) {
		next = F(e, D_NEXT);
		if (F(e, D_VB)) { bufRelease(F(e, D_VB)); F(e, D_VB) = 0; if (F(e, D_VAR)) F(F(e, D_VAR), 0) = 0; }
	}
}

// _rwD3D9DynamicVertexBufferRestore 0x4eca90 (after a device reset): the released buffers again, then the ring (tail call)
i32 __cdecl rw37__rwD3D9DynamicVertexBufferRestore(void){
	i32 hr = 0;
	if (hwtl())
		for (u32 e = DYNLIST, next; e; e = next) {
			next = F(e, D_NEXT);
			if (!F(e, D_VB)) { hr = devCreateVB(F(e, D_SIZE), 0x208, 0, 0, &F(e, D_VB)); if (F(e, D_VAR)) F(F(e, D_VAR), 0) = F(e, D_VB); }
			if (hr < 0) return 0;
		}
	return rw37_D3D9DynamicVertexBufferManagerCreate();
}

// D3D9DynamicVertexBufferManagerCreate 0x4ecb00: four 256 KB ring slots with hardware T&L, one without
i32 __cdecl rw37_D3D9DynamicVertexBufferManagerCreate(void){
	CUR = 0;
	if (hwtl()) {
		for (u32 n = 0; n < 4; n++) { OFF(n) = 0; SIZE(n) = 0x40000; if (!rw37_RwD3D9DynamicVertexBufferCreate(0x40000, MGRP(n))) return 0; }
		return 1;
	}
	OFF(0) = 0; SIZE(0) = 0x40000;
	if (!rw37_RwD3D9DynamicVertexBufferCreate(0x40000, MGRP(0))) return 0;
	for (u32 n = 1; n < 4; n++) { OFF(n) = 0; MGR(n) = 0; SIZE(n) = 0; }
	return 1;
}

// RwD3D9DynamicVertexBufferCreate 0x4ecbc0: an unused buffer of this size, else the largest unused one is recreated, else a new list entry
i32 __cdecl rw37_RwD3D9DynamicVertexBufferCreate(u32 size, UA *vb){
	u32 e, fr = 0;
	for (e = DYNLIST; e; e = F(e, D_NEXT)) {
		if (F(e, D_USED)) continue;
		if (F(e, D_SIZE) == size) { F(e, D_USED) = 1; *vb = F(e, D_VB); F(e, D_VAR) = (u32)(uintptr_t)vb; return 1; }
		if (!fr || F(fr, D_SIZE) < F(e, D_SIZE)) fr = e;
	}
	if (fr) { bufRelease(F(fr, D_VB)); F(fr, D_VB) = 0; e = fr; }
	else { e = flAlloc(DYNFL, 0x40411); F(e, D_NEXT) = DYNLIST; DYNLIST = e; }
	if (devCreateVB(size, 0x208, 0, (~*(const u32 *)((const u8 *)g[B_CAPS] + 0x1c) >> 15) & 2, vb) != 0) return 0;	// DEFAULT with hw T&L, else SYSTEMMEM
	F(e, D_SIZE) = size; F(e, D_USED) = 1; F(e, D_VB) = *vb; F(e, D_VAR) = (u32)(uintptr_t)vb;
	return 1;
}

// RwD3D9DynamicVertexBufferLock 0x4eccd0: nv vertices of vs bytes in the current ring slot (aligned to vs), another slot with room, else the next slot
// from 0 (discard); a request larger than the slot recreates it
i32 __cdecl rw37_RwD3D9DynamicVertexBufferLock(u32 vs, u32 nv, UA *vbOut, void *dataOut, UA *baseOut){
	u32 size = vs * nv, cur = CUR, off, next;
	if (hwtl()) {
		off = (OFF(cur) + vs - 1) / vs * vs;
		next = off + size;
		if (next > SIZE(cur)) {
			u32 n;
			for (n = 0; n < 4; n++) {
				if (n == cur) continue;
				off = (OFF(n) + vs - 1) / vs * vs;
				next = off + size;
				if (next <= SIZE(n)) break;
			}
			if (n < 4) { cur = n; CUR = n; }
			else { cur++; CUR = cur; if (cur >= 4) { cur = 0; CUR = 0; } off = 0; next = size; }
		}
	} else { off = 0; next = size; }
	if (size > SIZE(cur)) {
		dynDestroy(MGR(cur));
		cur = CUR;
		SIZE(cur) = size;
		rw37_RwD3D9DynamicVertexBufferCreate(size, MGRP(cur));
		cur = CUR;
	}
	u32 vb = MGR(cur);
	i32 hr;
	if (off == 0) { hr = bufLock(vb, 0, size, dataOut, 0x2800); OFF(CUR) = size; *baseOut = 0; }
	else { hr = bufLock(vb, off, size, dataOut, 0x1800); OFF(CUR) = next; *baseOut = off / vs; }
	*vbOut = MGR(CUR);
	return hr >= 0;
}

// _rwD3D9VertexBufferManagerOpen 0x4ece70: the four free lists (stride entries, free blocks, created buffers, dynamic buffers), then the ring
i32 __cdecl rw37__rwD3D9VertexBufferManagerOpen(void){
	rw37__rwD3D9VertexBufferManagerClose();
	FlCreateFn cr = CALL(B_FLCREATE, FlCreateFn);
	STRIDEFL = cr(0x10, 0x10, 4, 0x40411);
	FREEVBFL = cr(0x14, 0x64, 4, 0x40411);
	CREATEDFL = cr(8, 0x64, 4, 0x40411);
	DYNFL = cr(0x14, 0x2a, 4, 0x40411);
	rw37_D3D9DynamicVertexBufferManagerCreate();
	return 1;
}

// _rwD3D9VertexBufferManagerClose 0x4ecee0: the ring, the dynamic buffer list, the stride lists with their buffers, the free lists
void __cdecl rw37__rwD3D9VertexBufferManagerClose(void){
	ringDestroy();
	while (DYNLIST) {
		u32 e = DYNLIST, next = F(e, D_NEXT);
		if (F(e, D_VB)) { bufRelease(F(e, D_VB)); F(e, D_VB) = 0; if (F(e, D_VAR)) F(F(e, D_VAR), 0) = 0; }
		flFree(DYNFL, e);
		DYNLIST = next;
	}
	if (DYNFL) { CALL(B_FLDESTROY, Fn1)(DYNFL); DYNFL = 0; }
	while (STRIDES) {
		u32 s = STRIDES, next = F(s, S_NEXT);
		while (F(s, S_FREE)) { u32 nf = F(F(s, S_FREE), FB_NEXT); flFree(FREEVBFL, F(s, S_FREE)); F(s, S_FREE) = nf; }
		while (F(s, S_VBS)) {
			u32 c = F(s, S_VBS), nc = F(c, C_NEXT);
			if (F(c, C_VB)) bufRelease(F(c, C_VB));
			flFree(CREATEDFL, c);
			F(s, S_VBS) = nc;
		}
		flFree(STRIDEFL, s);
		STRIDES = next;
	}
	if (CREATEDFL) { CALL(B_FLDESTROY, Fn1)(CREATEDFL); CREATEDFL = 0; }
	if (FREEVBFL) { CALL(B_FLDESTROY, Fn1)(FREEVBFL); FREEVBFL = 0; }
	if (STRIDEFL) { CALL(B_FLDESTROY, Fn1)(STRIDEFL); STRIDEFL = 0; }
}

}
