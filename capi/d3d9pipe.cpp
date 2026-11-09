// D3D9PIPE (LIB_RW group d3d9pipe, D5 of plan_engine_switch_window3_2026-11-09): the still-original functions of the RW 3.7 D3D9 world pipeline (D3D9pipe.obj of
// rpworld.lib in MFL.exe), written op for op from the disassembly ("kernel on exe state": the resource entries, the device caches and the device are the exe's).
//   SortTriangles 0x4fa790 (the comparator, in rwleaf.cpp: rw37_SortTriangles), _rwD3D9ResourceEntryInstanceDataDestroy 0x4fa950, rxD3D9Instance 0x4faa20,
//   _rwD3D9EnableClippingIfNeeded 0x4fae50, rxD3D9DefaultRenderCallback 0x4faeb0.
// Resource entry: RwResEntry (0x18) + RxD3D9ResEntryHeader {serialNumber, numMeshes, indexBuffer, primType, vertexStream[2] {vb, offset, stride, u16 geometryFlags,
// u8 managed, u8 dynamicLock}, useOffsets, vertexDeclaration, totalNumIndex, totalNumVertex} (0x40) + numMeshes x RxD3D9InstanceData {numIndex, minVert, material,
// vertexAlpha, vertexShader, baseIndex, numVertices, startIndex, numPrimitives} (0x24).
// Device (*0x7d2240): DrawPrimitive 81, DrawIndexedPrimitive 82, SetVertexDeclaration 87, SetVertexShader 92, SetIndices 104, SetPixelShader 107; index buffer:
// Release 2, Lock 11, Unlock 12.  Cache variables _rwD3D9LastFVFUsed .. LastIndexBufferUsed 0x62e760..0x62e770.  Everything else through exe callees (enum B).
// Exe-layout adapter: mflrw_d3d9pipe_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612_d3d9pipe.inc), table layout = enum B below.
// Check: ghidra tools/delink/rw_d3d9pipe_check.sh (fake device tools/delink/fakedev.c; both sides run the same op sequences natively under Wine).
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

// bind table layout (an_hk_612_d3d9pipe.inc builds it in this order)
enum B { B_DEVVAR, B_ENGINEVAR, B_MESHMODULE, B_PRIMTAB, B_LASTFVF, B_LASTDECL, B_LASTVS, B_LASTPS, B_LASTIB,
         B_RESALLOC, B_RESFREE, B_IBCREATE, B_TOTRILIST, B_QSORT, B_DESTROYNOTIFY,
         B_REMOVEDYNGEOM, B_DELETEDECL, B_DESTROYVB, B_DYNVBDESTROY,
         B_ATOMICSPHERE, B_SPHEREINSIDE, B_BBOXINSIDE, B_SETRS, B_GETRS, B_SETTSS, B_SETTEXTURE, B_VAENABLE, B_VAENABLED, B_SETSURFPROPS,
         B_TEXHASALPHA, B_FLUSHCACHE, B_SETSTREAMS, B_N };
static void *const *g;

typedef u32 __attribute__((may_alias)) UA;
typedef u16 __attribute__((may_alias)) UA16;
typedef u8 __attribute__((may_alias)) UA8;
#define V(i) (*(UA *)g[i])
#define W32(a) (*(UA *)(uintptr_t)(a))
#define W16(a) (*(UA16 *)(uintptr_t)(a))
#define W8(a) (*(UA8 *)(uintptr_t)(a))

typedef int __cdecl CmpFn(const void *, const void *);
extern "C" CmpFn rw37_SortTriangles;		// 0x4fa790, rwleaf.cpp

static inline u32 DEV(void){ return V(B_DEVVAR); }
static inline void **VT(u32 o){ return *(void ***)(uintptr_t)o; }
static inline void dev1(int slot, u32 a){ u32 d = DEV(); ((i32 (__stdcall *)(u32, u32))VT(d)[slot])(d, a); }
static inline void devDraw(u32 type, u32 start, u32 count){ u32 d = DEV(); ((i32 (__stdcall *)(u32, u32, u32, u32))VT(d)[81])(d, type, start, count); }
static inline void devDrawIndexed(u32 type, u32 base, u32 minIdx, u32 nv, u32 start, u32 count){
	u32 d = DEV(); ((i32 (__stdcall *)(u32, u32, u32, u32, u32, u32, u32))VT(d)[82])(d, type, base, minIdx, nv, start, count);
}
static inline u32 ENG(void){ return V(B_ENGINEVAR); }

#define CALL(i, T) ((T)g[i])
typedef u32 (__cdecl *R0)(void);
typedef u32 (__cdecl *R1)(u32);
typedef u32 (__cdecl *R2)(u32, u32);
typedef u32 (__cdecl *R3)(u32, u32, u32);
typedef u32 (__cdecl *R4)(u32, u32, u32, u32);
static inline void setRS(u32 s, u32 v){ CALL(B_SETRS, R2)(s, v); }
static inline void setTSS(u32 st, u32 ty, u32 v){ CALL(B_SETTSS, R3)(st, ty, v); }
static inline void setTex(u32 t, u32 st){ CALL(B_SETTEXTURE, R2)(t, st); }

// the clipping test of _rwD3D9EnableClippingIfNeeded (also inlined by the render callback): D3DRS_CLIPPING off when the object is inside the current camera
static inline u32 clipInside(u32 object, u32 type){
	u32 cam = W32(ENG());
	if (type == 1) { u32 s = CALL(B_ATOMICSPHERE, R1)(object); return CALL(B_SPHEREINSIDE, R2)(cam, s); }
	return CALL(B_BBOXINSIDE, R2)(cam, object + 0x60);
}

extern "C" {

i32 __cdecl mflrw_d3d9pipe_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// _rwD3D9ResourceEntryInstanceDataDestroy 0x4fa950: dynamic geometry unregistered (owner type 8 = the geometry, 1 = the atomic's geometry) when a stream is not
// managed, index buffer released, declaration deleted, vertex buffers destroyed (managed) or dynamic ones released (offset 0, not dynamically locked); streams zeroed
void __cdecl rw37__rwD3D9ResourceEntryInstanceDataDestroy(u32 re){
	u32 owner = W32(re + 0xc);
	if (owner) {
		u8 t = W8(owner);
		if (t != 0xff) {
			u32 i;
			for (i = 0; i < 2; i++) if (W8(re + 0x36 + 0x10 * i) == 0) break;
			if (i < 2) {
				if (t == 8) CALL(B_REMOVEDYNGEOM, R1)(owner);
				else if (t == 1) CALL(B_REMOVEDYNGEOM, R1)(W32(owner + 0x18));
			}
		}
	}
	u32 ib = W32(re + 0x20);
	if (ib) { ((u32 (__stdcall *)(u32))VT(ib)[2])(ib); W32(re + 0x20) = 0; }
	if (W32(re + 0x4c)) { CALL(B_DELETEDECL, R1)(W32(re + 0x4c)); W32(re + 0x4c) = 0; }
	for (u32 s = re + 0x28; s < re + 0x48; s += 0x10) {
		u32 vb = W32(s);
		if (vb) {
			if (W8(s + 0xe)) CALL(B_DESTROYVB, R4)(W32(s + 8), W32(re + 0x54) * W32(s + 8), vb, W32(s + 4));
			else if (W32(s + 4) == 0 && W8(s + 0xf) == 0) CALL(B_DYNVBDESTROY, R1)(vb);
			W32(s) = 0;
		}
		W32(s + 4) = 0; W32(s + 8) = 0; W16(s + 0xc) = 0; W8(s + 0xe) = 0; W8(s + 0xf) = 0;
	}
}

// rxD3D9Instance 0x4faa20: allocate (engine malloc when allocateNative, else the resource arena) and fill the resource entry of a mesh header: index buffer
// (strips as triangle lists when the converted count fits 3 x the vertex count; triangle lists sorted), per-mesh instance data, then the instance callback
u32 __cdecl rw37_rxD3D9Instance(u32 object, u32 owner, u32 type, u32 resEntryPointer, u32 meshHeader, u32 instanceCallback, u32 allocateNative){
	u32 numMeshes = W16(meshHeader + 4), size = numMeshes * 36 + 0x40, re, toTri = 0, ip = 0;
	if (allocateNative) {
		re = (*(u32 (__cdecl **)(u32, u32))(uintptr_t)(ENG() + 0x108))(size + 0x18, 0x3050d);		// RwMalloc
		W32(resEntryPointer) = re;
		W32(re) = 0; W32(re + 4) = 0; W32(re + 8) = size; W32(re + 0xc) = 0; W32(re + 0x10) = 0; W32(re + 0x14) = (u32)(uintptr_t)g[B_DESTROYNOTIFY];
	} else {
		re = CALL(B_RESALLOC, R4)(owner, resEntryPointer, size, (u32)(uintptr_t)g[B_DESTROYNOTIFY]);
	}
	u32 h = re + 0x18;
	{ void *d = (void *)(uintptr_t)h; u32 c = size; __asm__ volatile("rep stosb" : "+D"(d), "+c"(c) : "a"(0) : "memory"); }
	W32(h) = W16(meshHeader + 6);
	W32(h + 4) = W16(meshHeader + 4);
	W32(h + 8) = 0;
	W32(h + 0x38) = 0;
	u32 mmb = W8(ENG() + V(B_MESHMODULE) + (W32(meshHeader) & 0xff) + 8);
	int primSet = 0;
	if (!(W32(meshHeader) & 0x100)) {
		for (u32 k = W16(meshHeader + 4), m = meshHeader + 0x14; k; k--, m += 0xc) W32(h + 0x38) += W32(m);
		u32 tni = W32(h + 0x38);
		if (tni) {
			if (mmb == 4) {
				u32 nv = 0x55555555u;
				if ((u8)type == 0xff) nv = W16(object + 0x84);
				else if ((u8)type == 1) nv = W32(W32(object + 0x18) + 0x10);
				nv *= 3;
				if (tni > nv) { toTri = 1; W32(h + 0x38) = nv; }
			}
			if (CALL(B_IBCREATE, R2)(W32(h + 0x38), h + 8)) {
				u32 ib = W32(h + 8);
				((i32 (__stdcall *)(u32, u32, u32, u32 *, u32))VT(ib)[11])(ib, 0, 0, &ip, 0);
			}
			if (toTri) { W32(h + 0xc) = 4; primSet = 1; }
		}
	}
	if (!primSet) W32(h + 0xc) = ((const UA *)g[B_PRIMTAB])[W8(ENG() + V(B_MESHMODULE) + (W32(meshHeader) & 0xff) + 8)];
	for (u32 s = h + 0x10; s < h + 0x30; s += 0x10) { W32(s) = 0; W32(s + 4) = 0; W32(s + 8) = 0; W16(s + 0xc) = 0; W8(s + 0xe) = 0; W8(s + 0xf) = 0; }
	u32 vcount = 0, start = 0;
	W32(h + 0x34) = 0;
	u32 inst = h + 0x40, mesh = meshHeader + 0x10;
	for (u32 k = W16(meshHeader + 4); k; k--, inst += 0x24, mesh += 0xc) {
		u32 n = W32(mesh + 4);
		if (W32(meshHeader) & 0x100) {
			W32(inst + 0x18) = n; W32(inst + 4) = vcount; vcount += n;
		} else if (n) {
			u32 lo = 0xffffffffu, hi = 0;
			for (u32 p = W32(mesh), c = n; c; c--, p += 2) { u32 v = W16(p); if (lo >= v) lo = v; if (hi <= v) hi = v; }
			W32(inst + 0x18) = hi - lo + 1; W32(inst + 4) = lo;
		} else {
			W32(inst + 0x18) = 0; W32(inst + 4) = 0;
		}
		W32(inst + 0x10) = 0; W32(inst + 8) = W32(mesh + 8); W32(inst + 0xc) = 0;
		if (ip) {
			W32(inst + 0x1c) = start;
			if (toTri) n = CALL(B_TOTRILIST, R4)(ip, W32(mesh), n, W32(inst + 4));
			else if (W32(inst + 4)) { u32 src = W32(mesh), dst = ip; for (u32 c = n; c; c--, src += 2, dst += 2) W16(dst) = (u16)(W16(src) - (u16)W32(inst + 4)); }
			else { void *d = (void *)(uintptr_t)ip, *sp = (void *)(uintptr_t)W32(mesh); u32 c = n * 2; __asm__ volatile("rep movsb" : "+D"(d), "+S"(sp), "+c"(c) : : "memory"); }
			W32(inst) = n;
			if (W32(re + 0x24) == 4) CALL(B_QSORT, R4)(ip, n / 3, 6, (u32)(uintptr_t)rw37_SortTriangles);
			ip += n * 2; start += n;
		} else {
			W32(inst + 0x1c) = 0; W32(inst) = 0;
		}
		u32 np;
		switch (W32(re + 0x24)) {
		case 2: np = n >> 1; break;
		case 3: np = n - 1; break;
		case 4: np = n / 3; break;
		case 5: case 6: np = n - 2; break;
		default: np = 0;
		}
		W32(inst + 0x20) = np;
		W32(inst + 0x14) = 0;
	}
	if (ip) { u32 ib = W32(h + 8); ((i32 (__stdcall *)(u32))VT(ib)[12])(ib); }
	if (instanceCallback && !((u32 (__cdecl *)(u32, u32, u32))(uintptr_t)instanceCallback)(object, h, 0)) {
		if (allocateNative) (*(void (__cdecl **)(u32))(uintptr_t)(ENG() + 0x10c))(re);
		else CALL(B_RESFREE, R1)(re);
		return 0;
	}
	return re;
}

// _rwD3D9EnableClippingIfNeeded 0x4fae50 (its RwD3D9SetRenderState is a tail jump: the result is the setter's eax)
u32 __cdecl rw37__rwD3D9EnableClippingIfNeeded(u32 object, u32 type){
	return CALL(B_SETRS, R2)(0x88, clipInside(object, type) == 0);
}

// rxD3D9DefaultRenderCallback 0x4faeb0: no pixel shader, clipping, indices / streams / declaration from the entry, then per mesh: unlit and not prelit = flat
// black through the texture factor (alpha from the texture when it has one), else material colour (surface properties when lit, texture factor when modulated),
// texture and the stage states it needs; vertex shader, then DrawIndexedPrimitive / DrawPrimitive after the render-state flush
void __cdecl rw37_rxD3D9DefaultRenderCallback(u32 re, u32 object, u32 type, u32 flags){
	u32 lighting, s1, s2;
	if (V(B_LASTPS)) { V(B_LASTPS) = 0; dev1(107, 0); }
	setRS(0x88, clipInside(object, (u8)type) == 0);
	if (W32(re + 0x20) && V(B_LASTIB) != W32(re + 0x20)) { V(B_LASTIB) = W32(re + 0x20); dev1(104, W32(re + 0x20)); }
	CALL(B_SETSTREAMS, R2)(re + 0x28, W32(re + 0x48));
	if (V(B_LASTDECL) != W32(re + 0x4c)) { V(B_LASTDECL) = W32(re + 0x4c); V(B_LASTFVF) = 0xffffffffu; dev1(87, W32(re + 0x4c)); }
	CALL(B_GETRS, R2)(0x89, (u32)(uintptr_t)&lighting);
	if (lighting == 0 && !(flags & 8)) {
		CALL(B_GETRS, R2)(0x1a, (u32)(uintptr_t)&s1);
		CALL(B_GETRS, R2)(9, (u32)(uintptr_t)&s2);
		CALL(B_VAENABLE, R1)(0);
		setRS(0x3c, 0xff000000u); setRS(0x1a, 0); setRS(9, 1);
		setTSS(0, 1, 3); setTSS(0, 2, 2); setTSS(0, 3, 3); setTSS(0, 4, 3); setTSS(0, 5, 2); setTSS(0, 6, 3);
		setTex(0, 0);
		u32 tex = flags & 0x84, alpha = 0;
		for (u32 k = W32(re + 0x1c), p = re + 0x68; k; k--, p += 0x24) {
			if (tex && W32(W32(p - 8)) && CALL(B_TEXHASALPHA, R1)(W32(W32(p - 8)))) {
				setTex(W32(W32(p - 8)), 0);
				if (!alpha) { alpha = 1; setTSS(0, 4, 4); }
			} else if (alpha) {
				alpha = 0; setTSS(0, 4, 3); setTex(0, 0);
			}
			if (V(B_LASTVS) != W32(p)) { V(B_LASTVS) = W32(p); dev1(92, W32(p)); }
			CALL(B_FLUSHCACHE, R0)();
			if (W32(re + 0x20)) devDrawIndexed(W32(re + 0x24), W32(p + 4), 0, W32(p + 8), W32(p + 0xc), W32(p + 0x10));
			else devDraw(W32(re + 0x24), W32(p + 4), W32(p + 0x10));
		}
		setRS(0x1a, s1); setRS(9, s2);
		return;
	}
	u32 va = CALL(B_VAENABLED, R0)(), last = 0x80000000u;
	u32 k = W32(re + 0x1c);
	if (!k) return;
	for (u32 p = re + 0x68; k; k--, p += 0x24) {
		u32 mat = W32(p - 8), col = mat + 4, st = 0;
		if (W8(mat + 7) == 0xff && W32(p - 4) == 0) { if (va) { va = 0; CALL(B_VAENABLE, R1)(0); } }
		else if (!va) { va = 1; CALL(B_VAENABLE, R1)(1); }
		if (lighting) CALL(B_SETSURFPROPS, R3)(mat + 0xc, col, flags);
		else if ((flags & 0x40) && W32(col) != 0xffffffffu) {
			st = 2;
			setRS(0x3c, (u32)W8(col + 3) << 24 | (u32)W8(col) << 16 | (u32)W8(col + 1) << 8 | W8(col + 2));
		}
		if (W32(mat) && (flags & 0x84)) { st |= 1; setTex(W32(mat), 0); }
		else setTex(0, 0);
		if (st != last) {
			int chk = 1;
			if (st & 1) {
				if (!(last & 1)) { setTSS(0, 1, 4); setTSS(0, 2, 2); setTSS(0, 3, 0); setTSS(0, 4, 4); setTSS(0, 5, 2); setTSS(0, 6, 0); }
				if (st & 2) { setTSS(1, 1, 4); setTSS(1, 2, 1); setTSS(1, 3, 3); setTSS(1, 4, 4); setTSS(1, 5, 1); setTSS(1, 6, 3); chk = 0; }
			} else if (st & 2) { setTSS(0, 1, 4); setTSS(0, 2, 0); setTSS(0, 3, 3); setTSS(0, 4, 4); setTSS(0, 5, 0); setTSS(0, 6, 3); }
			else { setTSS(0, 1, 3); setTSS(0, 3, 0); setTSS(0, 4, 3); setTSS(0, 6, 0); }
			if (chk && last == 3) { setTSS(1, 1, 1); setTSS(1, 4, 1); }
			last = st;
		}
		if (V(B_LASTVS) != W32(p)) { V(B_LASTVS) = W32(p); dev1(92, W32(p)); }
		CALL(B_FLUSHCACHE, R0)();
		if (W32(re + 0x20)) devDrawIndexed(W32(re + 0x24), W32(p + 4), 0, W32(p + 8), W32(p + 0xc), W32(p + 0x10));
		else devDraw(W32(re + 0x24), W32(p + 4), W32(p + 0x10));
	}
	if (last == 3) { setTSS(1, 1, 1); setTSS(1, 4, 1); }
}

}
