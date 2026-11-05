// RWMATH (output/rw_next_family_2026-11-05.md rows 1-4): frame transforms, camera / bbox tests, the parallel view matrix, the vector module
// (SSE and x87 transform routines, x87 matrix product) and RpMorphTargetCalcBoundingSphere of MFL.exe, written op for op from its disassembly
// like hanimcam.cpp (X = a value the original keeps on the x87 stack, float = a value it stores; valid at the game's x87 CW 0x7f; the SSE
// routines use the same SSE operations in the same order, so they follow the caller's MXCSR as the original does).
//   RwFrameTranslate 0x4d1460, RwFrameScale 0x4d1500, RwFrameTransform 0x4d15a0, RwFrameRotate 0x4d1640, RwFrameSetIdentity 0x4d16e0,
//   RwFrameOrthoNormalize 0x4d17a0, RwBBoxContainsPoint 0x4d1a50, CameraUpdateZShiftScale 0x4d1c00, RwCameraFrustumTestSphere 0x4d1cd0,
//   RwD3D9CameraIsSphereFullyInsideFrustum 0x4dbcb0, RwD3D9CameraIsBBoxFullyInsideFrustum 0x4dbd10, CameraBuildParallelViewMatrix 0x4d2bc0,
//   _rwIntelSSE[P4]V3dTransform{Points,Vectors,Point,Vector} 0x4ec280..0x4ec650, _rwIntelx86AsmV3dTransform{Points,Vectors,Point,Vector}
//   0x5dc000..0x5dc160, _rwIntelx86AsmMatrixMultiply 0x5dc2a0, RpMorphTargetCalcBoundingSphere 0x4f8800.
// Objects in place (RW 3.7 layouts): RwFrame {+3 privateFlags, +8 inDirtyListLink, +0x10 modelling, +0xa0 root}; RwMatrix {right, flags, up,
// pad, at, pad, pos, pad}; RwCamera {+4 frame, +0x14 projection, +0x20 viewMatrix, +0x70 recipViewWindow, +0x78 viewOffset, +0x80 near,
// +0x84 far, +0x8c zScale, +0x90 zShift, +0x94 RwFrustumPlane[6] {normal, distance, closestX/Y/Z, pad}}; RwBBox {sup, inf}; RwSphere
// {center, radius}; RpMorphTarget {+0 parentGeom (+0x14 numVertices), +0x14 verts}.
// Exe-layout adapter (thrown away at the librw flip): mflrw_rwmath_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over
// &RwEngineInstance 0x7d2128 (dirty frame list +0xbc, z buffer range +0x18 / +0x1c) and RwBBoxCalculate 0x4d18d0 (recomp C in the link).
// Callees of the same DLL: RwMatrixTranslate / Scale / Transform / Rotate / OrthoNormalize / Optimize (g1), _rwSqrt (g0).
// Check: tools/delink/rw_rwmath_check.sh (the original code and these exports natively under Wine on the same random inputs).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <xmmintrin.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint32_t u32;

typedef u8 *__attribute__((may_alias)) PA;
typedef float __attribute__((may_alias)) FA;
typedef int32_t __attribute__((may_alias)) IA;
typedef u32 __attribute__((may_alias)) UA;
#define P(p, off)  (*(PA *)((u8 *)(p) + (off)))
#define F(p, off)  (*(FA *)((u8 *)(p) + (off)))
#define I(p, off)  (*(IA *)((u8 *)(p) + (off)))
#define U(p, off)  (*(UA *)((u8 *)(p) + (off)))
#define B(p, off)  (*((u8 *)(p) + (off)))

typedef void *(__cdecl *BBoxCalcFn)(u8 *bbox, const u8 *verts, int32_t n);

// ---- adapter
static u8 **g_engine;			// &RwEngineInstance 0x7d2128
static BBoxCalcFn g_bboxCalc;		// RwBBoxCalculate 0x4d18d0
static void unbound(void){ fprintf(stderr, "[mflrw] rwmath called before mflrw_rwmath_bind\n"); abort(); }
// the original's float constants (0x5de19c, 0x5ea2f0): float objects, because a float literal is evaluated in long double here (FLT_EVAL_METHOD 2)
static const float k1em4 = 9.99999974737875163555145263671875e-5f, k1001 = 1.00100004673004150390625f;

// the frame's root joins the engine's dirty frame list unless it is in it already (privateFlags & 3), then root |= 3 (from the byte read
// before the insert), frame |= 0xc: the inlined rwFrameInternalDirty tail every RwFrame transform shares
static inline u8 *frameDirty(u8 *f){
	if(!g_engine) unbound();
	u8 fl = B(P(f, 0xa0), 3);
	if(!(fl & 3)){
		P(P(f, 0xa0), 8) = P(*g_engine, 0xbc);
		P(P(f, 0xa0), 0xc) = *g_engine + 0xbc;
		P(P(*g_engine, 0xbc), 4) = P(f, 0xa0) + 8;
		P(*g_engine, 0xbc) = P(f, 0xa0) + 8;
	}
	B(P(f, 0xa0), 3) = fl | 3;
	B(f, 3) |= 0xc;
	return f;
}

extern "C" {

void __cdecl mflrw_rwmath_bind(u8 **engineVar, void *bboxCalculate){ g_engine = engineVar; g_bboxCalc = (BBoxCalcFn)bboxCalculate; }

// ---- frames
u8 *__cdecl rw37_RwFrameTranslate(u8 *f, const RwV3d *v, RwOpCombineType op){ RwMatrixTranslate((RwMatrix *)(f + 0x10), v, op); return frameDirty(f); }
u8 *__cdecl rw37_RwFrameScale(u8 *f, const RwV3d *v, RwOpCombineType op){ RwMatrixScale((RwMatrix *)(f + 0x10), v, op); return frameDirty(f); }
u8 *__cdecl rw37_RwFrameTransform(u8 *f, const RwMatrix *m, RwOpCombineType op){ RwMatrixTransform((RwMatrix *)(f + 0x10), m, op); return frameDirty(f); }
u8 *__cdecl rw37_RwFrameRotate(u8 *f, const RwV3d *axis, RwReal angle, RwOpCombineType op){ RwMatrixRotate((RwMatrix *)(f + 0x10), axis, angle, op); return frameDirty(f); }
u8 *__cdecl rw37_RwFrameOrthoNormalize(u8 *f){ RwMatrixOrthoNormalize((RwMatrix *)(f + 0x10), (RwMatrix *)(f + 0x10)); return frameDirty(f); }
u8 *__cdecl rw37_RwFrameSetIdentity(u8 *f){
	static const int one[3] = {0x38, 0x24, 0x10}, zero[9] = {0x20, 0x18, 0x14, 0x34, 0x30, 0x28, 0x48, 0x44, 0x40};
	for(int k = 0; k < 3; k++) U(f, one[k]) = 0x3f800000;
	for(int k = 0; k < 9; k++) U(f, zero[k]) = 0;
	U(f, 0x1c) |= 0x20003;
	return frameDirty(f);
}

// ---- bounding boxes / camera tests
// 1 when inf <= p <= sup per axis (a NaN compare passes, as the original's flag tests do)
int32_t __cdecl rw37_RwBBoxContainsPoint(const u8 *bb, const u8 *p){
	if(F(bb, 0xc) > F(p, 0)) return 0;
	if(F(bb, 0x10) > F(p, 4)) return 0;
	if(F(bb, 0x14) > F(p, 8)) return 0;
	if(F(bb, 0) < F(p, 0)) return 0;
	if(F(bb, 4) < F(p, 4)) return 0;
	if(F(bb, 8) < F(p, 8)) return 0;
	return 1;
}

// zScale / zShift from the near / far planes (1/x unless the projection is parallel) and the engine's z buffer range
void __cdecl rw37_CameraUpdateZShiftScale(u8 *cam){
	if(!g_engine) unbound();
	float zn = (X)F(*g_engine, 0x18), zf = (X)F(*g_engine, 0x1c);
	X n, f = F(cam, 0x84);
	if(I(cam, 0x14) != 2){ f = (X)1.0f/f; n = (X)1.0f/F(cam, 0x80); }
	else n = F(cam, 0x80);
	X dk = ((X)zf - zn)*(X)k1em4;
	zf = (X)zf - dk;
	X c = dk + zn;
	X s = ((X)zf - c)/(f - n);
	F(cam, 0x8c) = s;
	X t = (X)zf + c;
	X r = t - (n + f)*s;
	F(cam, 0x90) = r*(X)0.5f;
}

// 0 outside a plane, 1 on a boundary, 2 inside all six
int32_t __cdecl rw37_RwCameraFrustumTestSphere(const u8 *cam, const u8 *s){
	int32_t res = 2;
	const u8 *pl = cam + 0x94;
	for(int i = 6; i; i--, pl += 0x14){
		X d = (X)F(pl, 4)*F(s, 4) + (X)F(pl, 0)*F(s, 0);
		d = d + (X)F(pl, 8)*F(s, 8);
		d = d - F(pl, 0xc);
		if(d > F(s, 0xc)) return 0;
		if(d > -(X)F(s, 0xc)) res = 1;
	}
	return res;
}

int32_t __cdecl rw37_RwD3D9CameraIsSphereFullyInsideFrustum(const u8 *cam, const u8 *s){
	float nr = -(X)F(s, 0xc);
	const u8 *pl = cam + 0x94;
	for(int i = 6; i; i--, pl += 0x14){
		X d = (X)F(pl, 4)*F(s, 4) + (X)F(pl, 0)*F(s, 0);
		d = d + (X)F(pl, 8)*F(s, 8);
		d = d - F(pl, 0xc);
		if(d > nr) return 0;
	}
	return 1;
}

// the plane's closest corner (bytes +0x10..0x12: 0 = inf, 1 = sup per axis) against the plane distance
int32_t __cdecl rw37_RwD3D9CameraIsBBoxFullyInsideFrustum(const u8 *cam, const u8 *bb){
	const u8 *pl = cam + 0x94;
	for(int i = 6; i; i--, pl += 0x14){
		X x = F(bb + 0xc - 12*B(pl, 0x10), 0), y = F(bb + 0x10 - 12*B(pl, 0x11), 0), z = F(bb + 0x14 - 12*B(pl, 0x12), 0);
		X d = (X)F(pl, 4)*y + x*F(pl, 0);
		d = d + (X)F(pl, 8)*z;
		if(d > F(pl, 0xc)) return 0;
	}
	return 1;
}

// view matrix of a parallel camera from the frame's LTM, the view window reciprocals and the view offset; RwMatrixOptimize(vm, NULL)
u8 *__cdecl rw37_CameraBuildParallelViewMatrix(u8 *cam){
	const u8 *m = P(cam, 4) + 0x50;
	u8 *vm = cam + 0x20;
	{	X a = (X)F(cam, 0x70)*(X)-0.5f;
		X a0 = a*F(m, 0);
		float t8 = (X)F(m, 4)*a, tc = (X)F(m, 8)*a, t14 = -((X)F(cam, 0x78)*a);
		X rx = (X)F(m, 0x20)*t14 + a0, ry = (X)F(m, 0x24)*t14 + t8, rz = (X)F(m, 0x28)*t14 + tc;
		F(vm, 0) = rx; F(vm, 0x10) = ry; F(vm, 0x20) = rz;
		X d = (X)F(m, 0x30)*rx + (X)F(m, 0x34)*ry; d = d + (X)F(m, 0x38)*rz;
		F(vm, 0x30) = (X)0.5f - (d + t14); }
	{	X b = (X)F(cam, 0x74)*(X)-0.5f;
		X b0 = (X)F(m, 0x10)*b;
		float t8 = (X)F(m, 0x14)*b, tc = (X)F(m, 0x18)*b, t14 = (X)F(cam, 0x7c)*b;
		X rx = (X)F(m, 0x20)*t14 + b0, ry = (X)F(m, 0x24)*t14 + t8, rz = (X)F(m, 0x28)*t14 + tc;
		F(vm, 4) = rx; F(vm, 0x14) = ry; F(vm, 0x24) = rz;
		X d = (X)F(m, 0x30)*rx + (X)F(m, 0x34)*ry; d = d + (X)F(m, 0x38)*rz;
		F(vm, 0x34) = (X)0.5f - (d + t14); }
	U(vm, 8) = U(m, 0x20); U(vm, 0x18) = U(m, 0x24); U(vm, 0x28) = U(m, 0x28);
	{ X d = (X)F(m, 0x30)*F(m, 0x20) + (X)F(m, 0x24)*F(m, 0x34); d = d + (X)F(m, 0x38)*F(m, 0x28); F(vm, 0x38) = -d; }
	RwMatrixOptimize((RwMatrix *)vm, NULL);
	return cam;
}

// ---- vector module, SSE: rows shuffled 0xa4 (lane 3 = lane 2, never stored), x / y / z broadcast; points (z*at + y*up) + (x*right + pos),
// vectors z*at + (y*up + x*right); n is a do-while count as in the original (n = 0 runs 2^32 times there)
__attribute__((target("sse"))) static inline void sseLoad(const u8 *m, __m128 *r, int rows){
	for(int k = 0; k < rows; k++){ __m128 v = _mm_loadu_ps((const float *)(m + 16*k)); r[k] = _mm_shuffle_ps(v, v, 0xa4); }
}
__attribute__((target("sse"))) static inline void sseOne(u8 *out, const u8 *in, const __m128 *r, int pos){
	__m128 x = _mm_load_ss((const float *)in), y = _mm_load_ss((const float *)(in + 4)), z = _mm_load_ss((const float *)(in + 8));	// movss + shufps 0, no x87 load
	x = _mm_shuffle_ps(x, x, 0); y = _mm_shuffle_ps(y, y, 0); z = _mm_shuffle_ps(z, z, 0);
	x = _mm_mul_ps(x, r[0]); y = _mm_mul_ps(y, r[1]); z = _mm_mul_ps(z, r[2]);
	if(pos){ x = _mm_add_ps(x, r[3]); z = _mm_add_ps(z, y); z = _mm_add_ps(z, x); }
	else{ y = _mm_add_ps(y, x); z = _mm_add_ps(z, y); }
	float o[4]; _mm_storeu_ps(o, z);
	memcpy(out, o, 12);
}
// force_align_arg_pointer: the exe calls the exports with a 4-aligned stack, gcc keeps r[] in aligned slots (mulps/addps (%esp))
#define SSEFN __attribute__((target("sse"), force_align_arg_pointer))
SSEFN static u8 *sseMany(u8 *out, const u8 *in, int32_t n, const u8 *m, int pos){
	__m128 r[4]; sseLoad(m, r, pos ? 4 : 3);
	u8 *o = out;
	do{ sseOne(o, in, r, pos); in += 12; o += 12; }while(--n);
	return out;
}
SSEFN static u8 *sseSingle(u8 *out, const u8 *in, const u8 *m, int pos){
	__m128 r[4]; sseLoad(m, r, pos ? 4 : 3);
	sseOne(out, in, r, pos);
	return out;
}
u8 *__cdecl rw37__rwIntelSSEV3dTransformPoints(u8 *out, const u8 *in, int32_t n, const u8 *m){ return sseMany(out, in, n, m, 1); }
u8 *__cdecl rw37__rwIntelSSEV3dTransformVectors(u8 *out, const u8 *in, int32_t n, const u8 *m){ return sseMany(out, in, n, m, 0); }
u8 *__cdecl rw37__rwIntelSSEV3dTransformPoint(u8 *out, const u8 *in, const u8 *m){ return sseSingle(out, in, m, 1); }
u8 *__cdecl rw37__rwIntelSSEV3dTransformVector(u8 *out, const u8 *in, const u8 *m){ return sseSingle(out, in, m, 0); }
u8 *__cdecl rw37__rwIntelSSEP4V3dTransformPoints(u8 *out, const u8 *in, int32_t n, const u8 *m){ return sseMany(out, in, n, m, 1); }
u8 *__cdecl rw37__rwIntelSSEP4V3dTransformVectors(u8 *out, const u8 *in, int32_t n, const u8 *m){ return sseMany(out, in, n, m, 0); }
u8 *__cdecl rw37__rwIntelSSEP4V3dTransformPoint(u8 *out, const u8 *in, const u8 *m){ return sseSingle(out, in, m, 1); }
u8 *__cdecl rw37__rwIntelSSEP4V3dTransformVector(u8 *out, const u8 *in, const u8 *m){ return sseSingle(out, in, m, 0); }

// ---- vector module, x87: ((x*m0 + y*m1) + z*m2) [+ pos] per column, the three sums on the stack; returns the INPUT pointer (the
// original's epilogue reloads [esp+8] after popping ebp)
static inline void x87One(u8 *out, const u8 *in, const u8 *m, int pos){
	X x = F(in, 0), y = F(in, 4), z = F(in, 8);
	X ox = x*F(m, 0), oy = x*F(m, 4), oz = x*F(m, 8);
	ox = ox + y*F(m, 0x10); oy = oy + y*F(m, 0x14); oz = oz + y*F(m, 0x18);
	ox = ox + z*F(m, 0x20); if(pos) ox = ox + F(m, 0x30);
	oy = oy + z*F(m, 0x24); if(pos) oy = oy + F(m, 0x34);
	oz = oz + z*F(m, 0x28); if(pos) oz = oz + F(m, 0x38);
	F(out, 0) = ox; F(out, 4) = oy; F(out, 8) = oz;
}
static u8 *x87Many(u8 *out, const u8 *in, int32_t n, const u8 *m, int pos){
	const u8 *r = in;
	do{ x87One(out, in, m, pos); in += 12; out += 12; }while(--n);
	return (u8 *)r;
}
u8 *__cdecl rw37__rwIntelx86AsmV3dTransformPoints(u8 *out, const u8 *in, int32_t n, const u8 *m){ return x87Many(out, in, n, m, 1); }
u8 *__cdecl rw37__rwIntelx86AsmV3dTransformVectors(u8 *out, const u8 *in, int32_t n, const u8 *m){ return x87Many(out, in, n, m, 0); }
u8 *__cdecl rw37__rwIntelx86AsmV3dTransformPoint(u8 *out, const u8 *in, const u8 *m){ x87One(out, in, m, 1); return (u8 *)in; }
u8 *__cdecl rw37__rwIntelx86AsmV3dTransformVector(u8 *out, const u8 *in, const u8 *m){ x87One(out, in, m, 0); return (u8 *)in; }

// o = a * b row by row (a row loaded first; b read from memory per element, o stored per element), pos row adds b's pos; the original
// returns the caller's eax, nothing here
void __cdecl rw37__rwIntelx86AsmMatrixMultiply(u8 *o, const u8 *a, const u8 *b){
	for(int r = 0; r < 4; r++){
		X x = F(a, 16*r), y = F(a, 16*r + 4), z = F(a, 16*r + 8), t;
		t = (X)F(b, 0)*x + (X)F(b, 0x10)*y; t = t + (X)F(b, 0x20)*z; if(r == 3) t = t + F(b, 0x30); F(o, 16*r) = t;
		X t2 = (X)F(b, 8)*x;
		t = (X)F(b, 4)*x + (X)F(b, 0x14)*y; t = t + (X)F(b, 0x24)*z; if(r == 3) t = t + F(b, 0x34); F(o, 16*r + 4) = t;
		t = t2 + (X)F(b, 0x18)*y; t = t + (X)F(b, 0x28)*z; if(r == 3) t = t + F(b, 0x38); F(o, 16*r + 8) = t;
	}
}

// ---- geometry
// center = the bbox middle, radius = 1.001 * the largest vertex distance (sqrt only when > 0)
u8 *__cdecl rw37_RpMorphTargetCalcBoundingSphere(u8 *mt, u8 *sphere){
	if(!g_bboxCalc) unbound();
	float bb[6], mx = 0;
	int32_t n = I(P(mt, 0), 0x14);
	g_bboxCalc((u8 *)bb, P(mt, 0x14), n);
	float sz = (X)bb[2] + bb[5];		// the z sum is stored first, x / y stay on the stack
	float cx = ((X)bb[0] + bb[3])*(X)0.5f, cy = ((X)bb[1] + bb[4])*(X)0.5f, cz = (X)sz*(X)0.5f;
	if(n){
		const u8 *v = P(mt, 0x14);
		for(int32_t i = n; i; i--, v += 12){
			X dx = (X)F(v, 0) - cx, dy = (X)F(v, 4) - cy, dz = (X)F(v, 8) - cz;
			X d = dy*dy + dx*dx; d = d + dz*dz;
			if(d > mx) mx = d;
		}
		if(mx > 0.0f){ volatile float sq = _rwSqrt(mx); mx = sq; }	// _rwSqrt leaves an unrounded value in st0; the original stores it (fstp) before the multiply
	}
	float r = (X)mx*(X)k1001;
	F(sphere, 0) = cx; F(sphere, 4) = cy; F(sphere, 8) = cz; F(sphere, 0xc) = r;
	return mt;
}

} // extern "C"
