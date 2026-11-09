// D3D9VDTAN (LIB_RW group d3d9vdtan, D6 of plan_engine_switch_window3_2026-11-09): the tangent instancer FUN_00508600 of d3d9vertexdeclaration.obj (rpworld) of
// MFL.exe, written op for op from the disassembly (x87 as in d3dlights.cpp: X = a value the original keeps on the x87 stack, R() = its float store).
//   Per triangle of the locked index buffer (lists, strips; degenerate strip triangles skipped): the texture-space tangent subtracted from its 3 vertices, per axis
//   only when |det| > eps; then vertices at the same position (|d| <= eps per axis) with close normals (dot >= 0.9999) and close tangents (cos > threshold) are merged
//   into the first one, the rest normalised (_rwInvSqrt) or copied from their representative; _rpD3D9VertexDeclarationInstV3d writes the result.  The original name is
//   unknown (no symbol): exported as rw37__rpD3D9VertexDeclarationInstTangent.
// Exe-layout adapter: mflrw_d3d9vdtan_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612_d3d9vdtan.inc), table layout = enum B below.
// Check: ghidra tools/delink/rw_d3d9vdtan_check.sh (the original code and this export natively under Wine, x87 CW 0x7f and 0x27f).
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef long double X;

enum B { B_ENGINEVAR, B_EPSDET, B_ONE, B_EPSPOS, B_INV128, B_NRMDOT, B_TANCOS, B_V3DLENGTH, B_INVSQRT, B_INSTV3D, B_N };
static void *const *g;

typedef u32 __attribute__((may_alias)) UA;
typedef u16 __attribute__((may_alias)) UA16;
typedef float __attribute__((may_alias)) FA;
#define W32(a) (*(UA *)(uintptr_t)(a))
#define W16(a) (*(UA16 *)(uintptr_t)(a))
#define FL(a) (*(FA *)(uintptr_t)(a))
#define KF(i) (*(const FA *)g[i])

static inline float R(X x){ volatile float f = (float)x; return f; }
static inline u32 ENG(void){ return *(UA *)g[B_ENGINEVAR]; }
static inline u32 rwMalloc(u32 n){ return (*(u32 (__cdecl **)(u32, u32))(uintptr_t)(ENG() + 0x108))(n, 0x1050d); }
static inline void rwFree(u32 p){ (*(void (__cdecl **)(u32))(uintptr_t)(ENG() + 0x10c))(p); }
static inline X v3dLength(u32 v){ return ((X (__cdecl *)(u32))g[B_V3DLENGTH])(v); }		// st0 as returned (not rounded)
static inline X invSqrt(float x){ return ((X (__cdecl *)(float))g[B_INVSQRT])(x); }

extern "C" {

i32 __cdecl mflrw_d3d9vdtan_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

u32 __cdecl rw37__rpD3D9VertexDeclarationInstTangent(u32 a0, u32 a1, u32 pos, u32 nrm, u32 cnrm, u32 uv, u32 hdr, u32 a7){
	u32 nv = W32(hdr + 0x3c), tan = rwMalloc(nv * 12), ip = 0;
	{ void *d = (void *)(uintptr_t)tan; u32 c = nv * 12; __asm__ volatile("rep stosb" : "+D"(d), "+c"(c) : "a"(0) : "memory"); }
	{ u32 ib = W32(hdr + 8); ((i32 (__stdcall *)(u32, u32, u32, u32 *, u32))(*(void ***)(uintptr_t)ib)[11])(ib, 0, W32(hdr + 0x38) * 2, &ip, 0); }
	for (u32 m = W32(hdr + 4), inst = hdr + 0x40; m; m--, inst += 0x24) {
		u32 base = ip + W32(inst + 0x1c) * 2, n = W32(inst);
		if (W32(hdr + 0xc) == 5) n -= 2;
		for (u32 i = 0; i < n; ) {
			u32 v0, v1, v2, mv = W32(inst + 4);
			if (W32(hdr + 0xc) == 4) {
				v0 = W16(base + 2 * i) + mv; v1 = W16(base + 2 * i + 2) + mv; v2 = W16(base + 2 * i + 4) + mv;
				i += 3;
			} else {
				if (i & 1) { v0 = W16(base + 2 * i + 4) + mv; v1 = W16(base + 2 * i + 2) + mv; v2 = W16(base + 2 * i) + mv; }
				else { v0 = W16(base + 2 * i) + mv; v1 = W16(base + 2 * i + 2) + mv; v2 = W16(base + 2 * i + 4) + mv; }
				i++;
				if (v0 == v1 || v0 == v2 || v1 == v2) continue;
			}
			u32 p0 = pos + v0 * 12, p1 = pos + v1 * 12, p2 = pos + v2 * 12, t0 = tan + v0 * 12, t1 = tan + v1 * 12, t2 = tan + v2 * 12;
			float e1x = R((X)FL(p1) - FL(p0));
			X du1 = (X)FL(uv + 8 * v1) - FL(uv + 8 * v0);
			float dv1 = R((X)FL(uv + 8 * v1 + 4) - FL(uv + 8 * v0 + 4));
			float e2x = R((X)FL(p2) - FL(p0));
			X du2 = (X)FL(uv + 8 * v2) - FL(uv + 8 * v0);
			X dv2x = (X)FL(uv + 8 * v2 + 4) - FL(uv + 8 * v0 + 4);
			float dv2 = R(dv2x);
			X det = du1 * dv2x - du2 * dv1;
			float tx = R((X)e2x * dv1 - (X)dv2 * e1x);
			if (__builtin_fabsl(det) > (X)KF(B_EPSDET)) {
				X r = (X)KF(B_ONE) / det * tx;
				FL(t0) = R(FL(t0) - r); FL(t1) = R(FL(t1) - r); FL(t2) = R(FL(t2) - r);
			}
			X e1y = (X)FL(p1 + 4) - FL(p0 + 4), e2y = (X)FL(p2 + 4) - FL(p0 + 4);
			float detf = R(det);
			X ty = e2y * dv1 - (X)dv2 * e1y;
			if (__builtin_fabsl((X)detf) > (X)KF(B_EPSDET)) {
				X r = ty * ((X)KF(B_ONE) / detf);
				FL(t0 + 4) = R(FL(t0 + 4) - r); FL(t1 + 4) = R(FL(t1 + 4) - r); FL(t2 + 4) = R(FL(t2 + 4) - r);
			}
			X e1z = (X)FL(p1 + 8) - FL(p0 + 8), e2z = (X)FL(p2 + 8) - FL(p0 + 8);
			X tz = e2z * dv1 - (X)dv2 * e1z;
			if (__builtin_fabsl((X)detf) > (X)KF(B_EPSDET)) {
				X r = tz * ((X)KF(B_ONE) / detf);
				FL(t0 + 8) = R(FL(t0 + 8) - r); FL(t1 + 8) = R(FL(t1 + 8) - r); FL(t2 + 8) = R(FL(t2 + 8) - r);
			}
		}
	}
	{ u32 ib = W32(hdr + 8); ((i32 (__stdcall *)(u32))(*(void ***)(uintptr_t)ib)[12])(ib); }
	u32 remap = rwMalloc(nv * 4);
	for (u32 i = 0; i < nv; i++) W32(remap + 4 * i) = i;
	for (u32 i = 0; i < nv; i++) {
		u32 pi = pos + 12 * i, ti = tan + 12 * i;
		for (u32 j = i + 1; j < nv; j++) {
			u32 pj = pos + 12 * j, tj = tan + 12 * j;
			if (W32(remap + 4 * j) <= i) continue;
			const X eps = *(const double *)g[B_EPSPOS];
			if (!(__builtin_fabsl((X)FL(pi) - FL(pj)) <= eps)) continue;
			if (!(__builtin_fabsl((X)FL(pi + 4) - FL(pj + 4)) <= eps)) continue;
			if (!(__builtin_fabsl((X)FL(pi + 8) - FL(pj + 8)) <= eps)) continue;
			X dot;
			if (nrm) {
				u32 ni = nrm + 12 * i, nj = nrm + 12 * j;
				dot = ((X)FL(nj + 4) * FL(ni + 4) + (X)FL(nj) * FL(ni)) + (X)FL(nj + 8) * FL(ni + 8);
			} else {
				const i8 *ci = (const i8 *)(uintptr_t)(cnrm + 4 * i), *cj = (const i8 *)(uintptr_t)(cnrm + 4 * j);
				X k = KF(B_INV128);
				X xi = (X)ci[0] * k, yi = (X)ci[1] * k, zi = (X)ci[2] * k, xj = (X)cj[0] * k, yj = (X)cj[1] * k, zj = (X)cj[2] * k;
				dot = (yj * yi + xj * xi) + zj * zi;
			}
			if (!(dot >= (X)KF(B_NRMDOT))) continue;
			float d = R(((X)FL(tj + 4) * FL(ti + 4) + (X)FL(tj) * FL(ti)) + (X)FL(tj + 8) * FL(ti + 8));
			float d2 = R((X)d / v3dLength(ti));
			if (!((X)d2 / v3dLength(tj) > (X)KF(B_TANCOS))) continue;
			FL(ti) = R((X)FL(tj) + FL(ti)); FL(ti + 4) = R((X)FL(tj + 4) + FL(ti + 4)); FL(ti + 8) = R((X)FL(tj + 8) + FL(ti + 8));
			W32(remap + 4 * j) = i;
		}
	}
	for (u32 k = 0; k < nv; k++) {
		u32 t = tan + 12 * k, r = W32(remap + 4 * k);
		if (r == k) {
			X x = FL(t), y = FL(t + 4), z = FL(t + 8);
			X inv = invSqrt(R((x * x + y * y) + z * z));
			FL(t) = R(FL(t) * inv); FL(t + 4) = R(FL(t + 4) * inv); FL(t + 8) = R(FL(t + 8) * inv);
		} else {
			u32 s = tan + 12 * r;
			W32(t) = W32(s); W32(t + 4) = W32(s + 4); W32(t + 8) = W32(s + 8);
		}
	}
	rwFree(remap);
	u32 res = ((u32 (__cdecl *)(u32, u32, u32, u32, u32))g[B_INSTV3D])(a0, a1, tan, nv, a7);
	rwFree(tan);
	return res;
}

}
