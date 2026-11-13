// D3D9VSU (group d3d9vsu, lane K3 window 7): the vertex shader utilities of MFL.exe (rpworld d3d9vertexshaderutils), written op for op from the disassembly,
// on the exe's own world matrix / inverse caches and callees:
//   _rwD3D9VSGetNormalInLocalSpace 0x50c1a0, _rwD3D9VSGetPointInLocalSpace 0x50c2e0, _rwD3D9VSGetRadiusInLocalSpace 0x50c3d0
// They bring a world space normal / point / radius into the local space of the current world matrix (D3D9ActiveTransform 0x7d7a30), caching the inverse
// (D3D9ActiveInverseTransform 0x7d7a34, buffer D3D9InverseTransform 0x62ff90) and its orthonormalized copy for normals (D3D9ActiveInverseNormalizedTransform
// 0x7d7a38, buffer D3D9InverseNormalizedTransform 0x62ffd0); D3D9IdentityMatrix 0x5ec460 = the "no transform" marker.
// Exe binding: mflrw_d3d9vsu_bind(table), table order = enum U below.  Unbound: abort.  Check: tools/delink/rw_d3d9vsu_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

enum U { U_WORLD, U_INV, U_INVT, U_INVBUF, U_TBUF, U_IDENT, U_ORTHOINV, U_INVERT, U_ORTHONORM, U_XFNORMAL, U_XFCOORD, U_INVSQRT, U_N };
static void *g_u[U_N];
static void *fn(int i){
	if(!g_u[U_N - 1]){ fprintf(stderr, "[mflrw] d3d9vsu called before mflrw_d3d9vsu_bind\n"); abort(); }
	return g_u[i];
}
#define VAR(i) (*(u32 *)fn(i))
#define P(p) ((u32)(uintptr_t)(p))
typedef void (__cdecl *Mat2Fn)(u32 dst, u32 src);					// D3D9MatrixInvertOrthoNormalized 0x50c110 / RwMatrixInvert / RwMatrixOrthoNormalize 0x4d3d40
typedef u32 (__stdcall *XformFn)(const void *out, const void *in, u32 m);	// the D3DX thunks 0x4fbc73 (TransformNormal) / 0x4fbc49 (TransformCoord)
typedef float (__cdecl *InvSqrtFn)(u32 bits);					// _rwInvSqrt 0x4d5470

// the D3D matrix pads of an RwMatrix buffer (+0xc / +0x1c / +0x2c = 0, +0x3c = 1.0f)
static void pads(u32 m){ *(u32 *)(uintptr_t)(m + 0xc) = 0; *(u32 *)(uintptr_t)(m + 0x1c) = 0; *(u32 *)(uintptr_t)(m + 0x2c) = 0; *(u32 *)(uintptr_t)(m + 0x3c) = 0x3f800000; }
static void copy3(const u32 *in, u32 *out){ out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; }
static int isIdentity(u32 world){ return (*(u32 *)(uintptr_t)(world + 0xc) & 0x20000) == 0x20000; }
static int isOrtho(u32 world){ return (*(u32 *)(uintptr_t)(world + 0xc) & 3) == 3; }

extern "C" {

void __cdecl mflrw_d3d9vsu_bind(void *const *tbl){
	for(int i = 0; i < U_N; i++) g_u[i] = tbl[i];
}

// _rwD3D9VSGetNormalInLocalSpace 0x50c1a0 (in, out): out = in through the inverse transpose of the world matrix.  Builds the caches on first use: an orthonormal
// matrix (flags & 3 == 3) uses its fast inverse for both; else RwMatrixInvert + pads, then RwMatrixOrthoNormalize of it + pads.  No / identity world: copy.
void __cdecl rw37__rwD3D9VSGetNormalInLocalSpace(const u32 *in, u32 *out){
	u32 m = VAR(U_INVT);
	if(!m){
		u32 world = VAR(U_WORLD);
		if(!world || isIdentity(world)){ VAR(U_INV) = P(fn(U_IDENT)); VAR(U_INVT) = P(fn(U_IDENT)); copy3(in, out); return; }
		m = VAR(U_INV);
		if(!isOrtho(world)){
			if(!m){
				((Mat2Fn)fn(U_INVERT))(P(fn(U_INVBUF)), world);
				m = P(fn(U_INVBUF)); pads(m);
				VAR(U_INV) = m;
			}
			((Mat2Fn)fn(U_ORTHONORM))(P(fn(U_TBUF)), m);
			u32 t = P(fn(U_TBUF));
			*(u32 *)(uintptr_t)(t + 0xc) = 0; VAR(U_INVT) = t;				// store order of the original (50c273..50c29c)
			*(u32 *)(uintptr_t)(t + 0x1c) = 0; *(u32 *)(uintptr_t)(t + 0x2c) = 0; *(u32 *)(uintptr_t)(t + 0x3c) = 0x3f800000;
			((XformFn)fn(U_XFNORMAL))(out, in, t);
			return;
		}
		if(!m){ ((Mat2Fn)fn(U_ORTHOINV))(P(fn(U_INVBUF)), world); m = P(fn(U_INVBUF)); VAR(U_INV) = m; }
		VAR(U_INVT) = m;
	}
	if(m == P(fn(U_IDENT))){ copy3(in, out); return; }
	((XformFn)fn(U_XFNORMAL))(out, in, m);
}

// _rwD3D9VSGetPointInLocalSpace 0x50c2e0 (in, out): out = in through the inverse of the world matrix (cache 0x7d7a34 only).
void __cdecl rw37__rwD3D9VSGetPointInLocalSpace(const u32 *in, u32 *out){
	u32 m = VAR(U_INV);
	if(!m){
		u32 world = VAR(U_WORLD);
		if(!world || isIdentity(world)){ VAR(U_INV) = P(fn(U_IDENT)); copy3(in, out); return; }
		if(isOrtho(world)){
			((Mat2Fn)fn(U_ORTHOINV))(P(fn(U_INVBUF)), world);
			m = P(fn(U_INVBUF)); VAR(U_INV) = m;
		}else{
			((Mat2Fn)fn(U_INVERT))(P(fn(U_INVBUF)), world);
			m = P(fn(U_INVBUF));
			*(u32 *)(uintptr_t)(m + 0xc) = 0; VAR(U_INV) = m;				// store order of the original (50c354..50c37d)
			*(u32 *)(uintptr_t)(m + 0x1c) = 0; *(u32 *)(uintptr_t)(m + 0x2c) = 0; *(u32 *)(uintptr_t)(m + 0x3c) = 0x3f800000;
		}
		((XformFn)fn(U_XFCOORD))(out, in, m);
		return;
	}
	if(m == P(fn(U_IDENT))){ copy3(in, out); return; }
	((XformFn)fn(U_XFCOORD))(out, in, m);
}

// _rwD3D9VSGetRadiusInLocalSpace 0x50c3d0 (radius, out): no world matrix or an orthonormal one: out = radius; else the vector (1/sqrt 3) x 3 through the
// inverse (built on first use, as above; the identity marker is not tested here), out = radius * _rwInvSqrt(|v|^2).
void __cdecl rw37__rwD3D9VSGetRadiusInLocalSpace(float radius, float *out){
	u32 world = VAR(U_WORLD);
	if(!world || isOrtho(world)){
		__asm__ volatile("flds %1\n\tfstps %0" : "=m"(*out) : "m"(radius));		// fld / fstp: a signalling NaN comes out quiet
		return;
	}
	u32 m = VAR(U_INV);
	if(!m){
		((Mat2Fn)fn(U_INVERT))(P(fn(U_INVBUF)), world);
		m = P(fn(U_INVBUF)); pads(m);
		VAR(U_INV) = m;
	}
	volatile u32 v[3] = {0x3f13cd3a, 0x3f13cd3a, 0x3f13cd3a};
	((XformFn)fn(U_XFNORMAL))((const void *)v, (const void *)v, m);
	u32 len2;
	__asm__ volatile("flds 4(%1)\n\tfmuls 4(%1)\n\tflds (%1)\n\tfmuls (%1)\n\tfaddp\n\tflds 8(%1)\n\tfmuls 8(%1)\n\tfaddp\n\tfstps %0"
	                 : "=m"(len2) : "r"(v) : "memory", "st");
	float inv = ((InvSqrtFn)fn(U_INVSQRT))(len2);
	__asm__ volatile("fmuls %1\n\tfstps %0" : "=m"(*out) : "m"(radius), "t"(inv) : "st");
}

}
