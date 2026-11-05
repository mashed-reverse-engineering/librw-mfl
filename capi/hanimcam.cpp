// HANIMCAM (plan_librw_engine_switch_2026-11-03_v2 "HAnim UpdateMatrices" + the camera clip planes): RpHAnimHierarchyUpdateMatrices and the two static
// frustum builders of the camera module of MFL.exe, written op for op from its disassembly like splanim.cpp / bezpat.cpp (X = a value the
// original keeps on the x87 stack, float = a value it stores; valid at the game's x87 CW 0x7f).
//   CameraBuildPerspClipPlanes 0x4d2110, CameraBuildParallelClipPlanes 0x4d27b0, RpHAnimHierarchyUpdateMatrices 0x544450.
// Objects in place (RW 3.7 layouts): RwCamera {+4 frame, +0x68 viewWindow, +0x78 viewOffset, +0x80 nearPlane, +0x84 farPlane,
// +0x94 RwFrustumPlane frustumPlanes[6] {RwPlane {normal, distance}, closestX/Y/Z, pad}, +0x124 RwV3d frustumCorners[8]}; RwFrame {+3 privateFlags,
// +4 parent, +8 inDirtyListLink, +0x10 modelling, +0x50 ltm, +0xa0 root}; RwMatrix {right, flags, up, pad, at, pad, pos, pad};
// RpHAnimHierarchy {flags, numNodes, pMatrixArray, pMatrixArrayUnaligned, pNodeInfo {nodeID, nodeIndex, flags, pFrame}, parentFrame,
// parentHierarchy, rootParentOffset, currentAnim}; RtAnimInterpolator {+0x24 interpKeyFrameSize, +0x3c keyFrameApplyCB, +0x4c the
// interpolated frames {prev, next, quat (x, y, z, w), trans}}.
// Exe-layout adapter (thrown away at the librw flip): mflrw_hanimcam_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over
// &RwEngineInstance 0x7d2128 (dirty frame list +0xbc), RpHAnimKeyFrameApply 0x542fe0 (the interpolator's apply callback the original
// inlines), RwFrameDirty 0x4d0c30, RwFrameGetLTM 0x4d0ff0, RwFrameUpdateObjects 0x4d0f70 and RwV3dTransformPoints 0x4d5670 (the engine's
// vector module dispatch).  Callees of the same DLL: _rwInvSqrt (g0), RwMatrixMultiply (g1).
// Not reproduced: the original reads a few never-written stack words (the flags word of its identity parent matrices, the pad words of
// its local matrix that RwMatrixMultiply / the modelling copy carry on); here they are 0 (only pad / garbage flag bits can differ).
// Check: tools/delink/rw_hanimcam_check.sh (the original code and these exports natively under Wine on the same random inputs, objects
// and callback logs compared word for word).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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

typedef int32_t (__cdecl *FrameFn)(u8 *frame);
typedef u8 *(__cdecl *FrameLtmFn)(u8 *frame);
typedef void *(__cdecl *ApplyFn)(u8 *matrix, u8 *interpFrame);
typedef void *(__cdecl *TransformPointsFn)(u8 *out, const u8 *in, int32_t n, const u8 *matrix);

// ---- adapter
static u8 **g_engine;			// &RwEngineInstance 0x7d2128
static void *g_kfApply;			// RpHAnimKeyFrameApply 0x542fe0
static FrameFn g_frameDirty;		// RwFrameDirty 0x4d0c30
static FrameLtmFn g_frameGetLTM;	// RwFrameGetLTM 0x4d0ff0
static FrameLtmFn g_frameUpdateObjects;	// RwFrameUpdateObjects 0x4d0f70
static TransformPointsFn g_v3dTransformPoints;	// RwV3dTransformPoints 0x4d5670
static void unbound(void){ fprintf(stderr, "[mflrw] hanimcam called before mflrw_hanimcam_bind\n"); abort(); }

static inline u8 sgn1(const void *p, int off){ return (u8)((I(p, off) >> 31) + 1); }	// sar 31 / inc: 0 for a negative float, 1 otherwise
static inline void closest(u8 *pl){ B(pl, 0x10) = sgn1(pl, 0); B(pl, 0x11) = sgn1(pl, 4); B(pl, 0x12) = sgn1(pl, 8); }
static inline void cp(void *d, const void *s, size_t n){ memcpy(d, s, n); }		// the original's integer moves / rep movs

// one frustum side: normal = (a - b) x (c - b) as the original orders it (e = a - b stored, g = c - b on the stack), normalised with
// _rwInvSqrt, distance against corner p; writes plane pl (normal, distance, closest bytes)
static inline void sidePlane(u8 *pl, const float *e, X g0, X g1, X g2, int form, const float *p){
	float A, Bv; X C;
	if(form == 0){			// e.y*g.z - g.y*e.z, g.x*e.z - g.z*e.x, g.y*e.x - g.x*e.y
		A = (X)e[1]*g2 - g1*e[2];
		Bv = g0*e[2] - g2*e[0];
		C = g1*e[0] - g0*e[1];
	}else{				// g.y*e.z - g.z*e.y, g.z*e.x - g.x*e.z, g.x*e.y - g.y*e.x
		A = g1*e[2] - g2*e[1];
		Bv = g2*e[0] - g0*e[2];
		C = g0*e[1] - g1*e[0];
	}
	F(pl, 0) = A; F(pl, 4) = Bv; F(pl, 8) = C;
	X s = (X)A*A + (X)Bv*Bv; s = s + C*C;
	X r = _rwInvSqrt((float)s);
	X nx = F(pl, 0)*r; F(pl, 0) = nx;
	X ny = F(pl, 4)*r; F(pl, 4) = ny;
	X nz = F(pl, 8)*r; F(pl, 8) = nz;
	X d = (X)p[1]*ny + (X)p[0]*nx; d = d + (X)p[2]*nz;
	F(pl, 0xc) = d;
	closest(pl);
}

extern "C" {

void __cdecl mflrw_hanimcam_bind(u8 **engineVar, void *keyFrameApply, void *frameDirty, void *frameGetLTM, void *frameUpdateObjects, void *v3dTransformPoints){
	g_engine = engineVar; g_kfApply = keyFrameApply; g_frameDirty = (FrameFn)frameDirty; g_frameGetLTM = (FrameLtmFn)frameGetLTM;
	g_frameUpdateObjects = (FrameLtmFn)frameUpdateObjects; g_v3dTransformPoints = (TransformPointsFn)v3dTransformPoints;
}

// CameraBuildPerspClipPlanes 0x4d2110: the 8 frustum corners in world space from the frame's LTM, view window / offset and the near / far
// planes, then the 6 planes (at, -at, and the four sides)
void __cdecl rw37_CameraBuildPerspClipPlanes(u8 *cam){
	const u8 *m = P(cam, 4) + 0x50;
	float *c = (float *)(cam + 0x124);
	u8 *pl = cam + 0x94;
	// view offset in world space (o.x / o.y stay on the stack, o.z is stored)
	X A = -(X)F(cam, 0x78);
	X ax = A*F(m, 0);
	float t30 = (X)F(m, 4)*A, t34 = (X)F(m, 8)*A;
	X ox = (X)F(cam, 0x7c)*F(m, 0x10) + ax;
	X oy = (X)F(cam, 0x7c)*F(m, 0x14) + t30;
	float oz = (X)F(cam, 0x7c)*F(m, 0x18) + t34;
	// corner 0 at distance 1 (at + window), then 1..3 around the window
	X a = (X)F(cam, 0x68)*F(m, 0), b = (X)F(cam, 0x68)*F(m, 4);
	float t40 = (X)F(cam, 0x68)*F(m, 8);
	X px = (X)F(m, 0x20) + a, py = (X)F(m, 0x24) + b;
	float t1c = (X)F(m, 0x28) + t40;
	float t20 = (X)F(cam, 0x6c)*F(m, 0x10), t24 = (X)F(cam, 0x6c)*F(m, 0x14), t28 = (X)F(cam, 0x6c)*F(m, 0x18);
	float cx = t20 + px, cy = t24 + py, cz = (X)t28 + t1c;
	c[0] = cx; c[1] = cy; c[2] = cz;
	float t38 = a + a;
	X b2 = b + b, w2 = (X)t40 + t40, u2 = (X)t20 + t20, v2 = (X)t24 + t24;
	float z2 = (X)t28 + t28;
	cx = (X)cx - t38; cy = cy - b2; cz = cz - w2;
	c[3] = cx; c[4] = cy; c[5] = cz;
	cx = cx - u2; cy = cy - v2; cz = (X)cz - z2;
	c[6] = cx; c[7] = cy; c[8] = cz;
	cx = (X)cx + t38; cy = cy + b2; cz = cz + w2;
	c[9] = cx; c[10] = cy; c[11] = cz;
	// near (0..3) and far (4..7) corners: pos + o + plane * (corner - o)
	float *k = c;
	for(int i = 4; i; i--, k += 3){
		X dx = k[0] - ox, dy = k[1] - oy;
		float dz = (X)k[2] - oz;
		k[0] = F(m, 0x30) + ox; k[1] = F(m, 0x34) + oy; k[2] = (X)F(m, 0x38) + oz;
		k[0] = (X)F(cam, 0x80)*dx + k[0]; k[1] = (X)F(cam, 0x80)*dy + k[1]; k[2] = (X)F(cam, 0x80)*dz + k[2];
		k[12] = F(m, 0x30) + ox; k[13] = F(m, 0x34) + oy; k[14] = (X)F(m, 0x38) + oz;
		k[12] = (X)F(cam, 0x84)*dx + k[12]; k[13] = (X)F(cam, 0x84)*dy + k[13]; k[14] = (X)F(cam, 0x84)*dz + k[14];
	}
	// plane 0: far (normal at, through corner 4); plane 1: near (-at, through corner 0)
	cp(pl, m + 0x20, 12);
	{ X d = (X)F(m, 0x20)*c[12] + (X)c[13]*F(m, 0x24); d = d + (X)c[14]*F(m, 0x28); F(pl, 0xc) = d; }
	closest(pl);
	{ X nx = -(X)F(pl, 0), ny = -(X)F(pl, 4), nz = -(X)F(pl, 8);
	  F(pl, 0x14) = nx; F(pl, 0x18) = ny; F(pl, 0x1c) = nz;
	  X d = (X)c[1]*ny + nx*c[0]; d = d + (X)c[2]*nz; F(pl, 0x20) = d; }
	closest(pl + 0x14);
	// sides: corners 1 / 5 / 6 (plane 2), 1 / 5 / 4 (plane 3), 3 / 7 / 4 (plane 4), 3 / 7 / 6 (plane 5)
	float e[3];
	e[0] = c[3] - c[15]; e[1] = c[4] - c[16]; e[2] = c[5] - c[17];
	sidePlane(pl + 0x28, e, (X)c[18] - c[15], (X)c[19] - c[16], (X)c[20] - c[17], 0, c + 3);
	sidePlane(pl + 0x3c, e, (X)c[12] - c[15], (X)c[13] - c[16], (X)c[14] - c[17], 1, c + 3);
	e[0] = c[9] - c[21]; e[1] = c[10] - c[22]; e[2] = c[11] - c[23];
	sidePlane(pl + 0x50, e, (X)c[12] - c[21], (X)c[13] - c[22], (X)c[14] - c[23], 0, c + 9);
	sidePlane(pl + 0x64, e, (X)c[18] - c[21], (X)c[19] - c[22], (X)c[20] - c[23], 1, c + 9);
}

// CameraBuildParallelClipPlanes 0x4d27b0: the 8 corners in camera space (window shifted by the view offset scaled with 1 - plane), moved to
// world space with RwV3dTransformPoints, then the planes; the left / bottom planes are the negated right / top planes
void __cdecl rw37_CameraBuildParallelClipPlanes(u8 *cam){
	if(!g_v3dTransformPoints) unbound();
	u8 *frame = P(cam, 4);
	float *c = (float *)(cam + 0x124);
	u8 *pl = cam + 0x94;
	X vwx = F(cam, 0x68), nr = F(cam, 0x80), fr = F(cam, 0x84);
	float far_ = fr, vox = -(X)F(cam, 0x78), voy = F(cam, 0x7c), vwy = F(cam, 0x6c);
	c[11] = nr; c[8] = nr; c[5] = nr; c[2] = nr;
	c[23] = fr; c[20] = fr; c[17] = fr; c[14] = fr;
	float n1 = (X)1.0f - nr;
	X p = (X)n1*vox;
	c[9] = c[0] = p + vwx;
	c[6] = c[3] = p - vwx;
	X f1 = (X)1.0f - far_;
	float f1s = f1;
	X q = f1*vox;
	c[21] = c[12] = q + vwx;
	c[18] = c[15] = q - vwx;
	X r = (X)n1*voy;
	c[4] = c[1] = vwy + r;
	c[10] = c[7] = r - vwy;
	X s = (X)f1s*voy;
	c[16] = c[13] = vwy + s;
	c[22] = c[19] = s - vwy;
	g_v3dTransformPoints((u8 *)c, (u8 *)c, 8, frame + 0x50);
	// plane 0: far (normal = the LTM's at, through corner 4); plane 1: near (-at, through corner 0)
	cp(pl, frame + 0x70, 12);
	{ X d = (X)F(pl, 4)*c[13] + (X)c[12]*F(pl, 0); d = d + (X)c[14]*F(pl, 8); F(pl, 0xc) = d; }
	closest(pl);
	{ X nx = -(X)F(pl, 0), ny = -(X)F(pl, 4), nz = -(X)F(pl, 8);
	  F(pl, 0x14) = nx; F(pl, 0x18) = ny; F(pl, 0x1c) = nz;
	  X d = (X)c[1]*ny + nx*c[0]; d = d + (X)c[2]*nz; F(pl, 0x20) = d; }
	closest(pl + 0x14);
	// plane 2 (corners 1 / 5 / 6) as in the perspective case
	float e[3];
	e[0] = c[3] - c[15]; e[1] = c[4] - c[16]; e[2] = c[5] - c[17];
	sidePlane(pl + 0x28, e, (X)c[18] - c[15], (X)c[19] - c[16], (X)c[20] - c[17], 0, c + 3);
	// plane 3 (corners 1 / 5 / 4): ny / nz are stored before the distance, nx stays on the stack
	{ X g0 = (X)c[12] - c[15], g1 = (X)c[13] - c[16], g2 = (X)c[14] - c[17];
	  u8 *p3 = pl + 0x3c;
	  float A = g1*e[2] - g2*e[1], Bv = g2*e[0] - g0*e[2]; X C = g0*e[1] - g1*e[0];
	  F(p3, 0) = A; F(p3, 4) = Bv; F(p3, 8) = C;
	  X s2 = (X)A*A + (X)Bv*Bv; s2 = s2 + C*C;
	  X rr = _rwInvSqrt((float)s2);
	  X nx = F(p3, 0)*rr; F(p3, 0) = nx;
	  float ny = F(p3, 4)*rr; F(p3, 4) = ny;
	  float nz = F(p3, 8)*rr; F(p3, 8) = nz;
	  X d = (X)c[4]*ny + (X)c[3]*nx; d = d + (X)c[5]*nz; F(p3, 0xc) = d;
	  closest(p3);
	  // plane 4 = -plane 2 through corner 3, plane 5 = -plane 3 through corner 3
	  u8 *p4 = pl + 0x50, *p2 = pl + 0x28;
	  X mx = -(X)F(p2, 0), my = -(X)F(p2, 4), mz = -(X)F(p2, 8);
	  F(p4, 0) = mx; F(p4, 4) = my; F(p4, 8) = mz;
	  X d4 = (X)c[9]*mx + (X)c[10]*my; d4 = d4 + (X)c[11]*mz; F(p4, 0xc) = d4;
	  X lx = -nx, ly = -(X)ny, lz = -(X)nz;
	  u8 *p5 = pl + 0x64;
	  F(p5, 0) = lx; F(p5, 4) = ly; F(p5, 8) = lz;
	  closest(p4);
	  X d5 = (X)c[9]*lx + (X)c[10]*ly; d5 = d5 + (X)c[11]*lz; F(p5, 0xc) = d5;
	  closest(p5); }
}

} // extern "C"

// ---- HAnim
// RpHAnimKeyFrameApply as the original inlines it: interpolated frame (quat x, y, z, w at +8, trans at +0x18) -> matrix m (flags 3; pads untouched)
static inline void quatToMatrix(u8 *m, const u8 *f){
	X x = F(f, 8), y = F(f, 0xc);
	float z = F(f, 0x10), w = F(f, 0x14);
	float xx = x*x, yy = y*y, zz = (X)z*z, zy = (X)z*y, zx = (X)z*x, yx = y*x, wx = (X)w*x;
	X wy = (X)w*y, wz = (X)w*z;
	X t;
	t = (X)zz + yy; t = t + t; F(m, 0) = (X)1.0f - t;
	t = yx + wz; F(m, 4) = t + t;
	t = zx - wy; F(m, 8) = t + t;
	t = yx - wz; F(m, 0x10) = t + t;
	t = (X)zz + xx; t = t + t; F(m, 0x14) = (X)1.0f - t;
	t = (X)wx + zy; F(m, 0x18) = t + t;
	t = wy + zx; F(m, 0x20) = t + t;
	U(m, 0xc) = 3;
	t = (X)zy - wx; F(m, 0x24) = t + t;
	t = (X)yy + xx; t = t + t; F(m, 0x28) = (X)1.0f - t;
	cp(m + 0x30, f + 0x18, 12);
}

static inline void setIdentity(u8 *m){
	static const float id[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
	u32 fl = U(m, 0xc);
	cp(m, id, 64);
	U(m, 0xc) = fl | 0x20003;
}

// the parent matrix of the hierarchy: LTM of frame pf (clean), or the product of the modelling matrices up the chain (dirty), or identity
static inline void parentLTM(u8 *out, u8 *pf, u8 *tmp){
	if(!pf){ setIdentity(out); return; }
	if(!g_frameDirty(pf)){ cp(out, g_frameGetLTM(pf), 64); return; }
	cp(out, pf + 0x10, 64);
	for(u8 *f = P(pf, 4); f; f = P(f, 4)){
		cp(tmp, out, 64);
		RwMatrixMultiply((RwMatrix *)out, (RwMatrix *)tmp, (RwMatrix *)(f + 0x10));
	}
}

// the root frame of the hierarchy's parent frame joins the engine's dirty frame list (inlined rwLinkListAddLLLink + rwFRAMEPRIVATEHIERARCHYSYNCLTM)
static inline void markRootDirty(u8 *h){
	u8 *root = P(P(h, 0x14), 0xa0);
	if(B(root, 3) & 3) return;
	P(root, 8) = P(*g_engine, 0xbc);
	P(P(P(h, 0x14), 0xa0), 0xc) = *g_engine + 0xbc;
	P(P(*g_engine, 0xbc), 4) = P(P(h, 0x14), 0xa0) + 8;
	P(*g_engine, 0xbc) = P(P(h, 0x14), 0xa0) + 8;
	B(P(P(h, 0x14), 0xa0), 3) |= 2;
}

extern "C" {

// RpHAnimHierarchyUpdateMatrices 0x544450: returns 1
int32_t __cdecl rw37_RpHAnimHierarchyUpdateMatrices(u8 *h){
	if(!g_engine) unbound();
	u32 flags = U(h, 0);
	u8 local[64], tmp[64], walk[64];
	memset(local, 0, sizeof local); memset(tmp, 0, sizeof tmp); memset(walk, 0, sizeof walk);
	if(flags & 2){
		// no matrix array: the frames only, with a stack of whole matrices for the push / pop node flags
		u8 parent[64], mstack[31][64];
		memset(parent, 0, sizeof parent);
		parentLTM(parent, P(P(h, 0x14), 4), walk);
		u32 ltm = flags & 0x2000;
		if(ltm) markRootDirty(h);
		u8 *ip = P(h, 0x20), *kf = ip + 0x4c, *sp = mstack[0];
		int32_t ks = I(ip, 0x24);
		void *apply = P(ip, 0x3c);
		if(I(h, 4) <= 0) return 1;
		u8 *ni = P(h, 0x10) + 8;
		for(int32_t i = 0; i < I(h, 4); i++, kf += ks, ni += 0x10){
			if(apply == g_kfApply) quatToMatrix(local, kf);
			else ((ApplyFn)apply)(local, kf);
			RwMatrixMultiply((RwMatrix *)tmp, (RwMatrix *)local, (RwMatrix *)parent);
			u8 *fr = P(ni, 4);
			if(fr){
				if(flags & 0x1000){
					cp(fr + 0x10, local, 64);
					if(!ltm) g_frameUpdateObjects(fr);
					else goto setLtm;
				}else if(ltm){
				setLtm:
					cp(fr + 0x50, tmp, 64);
					B(fr, 3) = (B(fr, 3) & 0xfb) | 8;
				}
			}
			switch(U(ni, 0) & 3){
			case 2: cp(sp, parent, 64); sp += 64;	// push, then fall through
			case 0: cp(parent, tmp, 64); break;
			case 1: sp -= 64; cp(parent, sp, 64); break;
			}
		}
		return 1;
	}
	// matrix array: pMatrixArray[i] = local * parent, the parent a pointer (stack of pointers for push / pop)
	u8 ident[64], pltm[64], *pstack[(0xa14 - 0x1a8)/4];
	memset(ident, 0, sizeof ident); memset(pltm, 0, sizeof pltm);
	u8 *par = NULL, *pf = NULL;
	int32_t off;
	int fetch;
	if((flags & 1) && (off = I(h, 0x1c)) != -1){
		par = P(P(h, 0x18), 8) + ((u32)off << 6);
		fetch = (flags & 0x2000) && (flags & 0x4000);
		if(fetch) pf = P(P(h, 0x18), 0x14);
	}else if(flags & 0x4000){
		setIdentity(ident);
		par = ident;
		fetch = (flags & 0x2000) != 0;
		if(fetch) pf = P(h, 0x14);
	}else{
		par = pltm;
		fetch = 1;
		pf = P(h, 0x14);
	}
	if(fetch) parentLTM(pltm, pf ? P(pf, 4) : NULL, walk);
	u32 ltm = flags & 0x2000;
	if(ltm) markRootDirty(h);
	u8 *ip = P(h, 0x20), *kf = ip + 0x4c, *mp = P(h, 8), **sp = pstack;
	int32_t ks = I(ip, 0x24);
	void *apply = P(ip, 0x3c);
	if(I(h, 4) <= 0) return 1;
	u8 *ni = P(h, 0x10) + 8;
	for(int32_t i = 0; i < I(h, 4); i++, kf += ks, mp += 64, ni += 0x10){
		if(apply == g_kfApply) quatToMatrix(local, kf);
		else ((ApplyFn)apply)(local, kf);
		RwMatrixMultiply((RwMatrix *)mp, (RwMatrix *)local, (RwMatrix *)par);
		u8 *fr = P(ni, 4);
		if(fr){
			if(flags & 0x1000){
				cp(fr + 0x10, local, 64);
				if(!ltm){ g_frameUpdateObjects(fr); goto next; }
			}else if(!ltm) goto next;
			if(flags & 0x4000) RwMatrixMultiply((RwMatrix *)(fr + 0x50), (RwMatrix *)mp, (RwMatrix *)pltm);
			else cp(fr + 0x50, mp, 64);
			B(fr, 3) = (B(fr, 3) & 0xfb) | 8;
		}
	next:
		switch(U(ni, 0) & 3){
		case 2: *sp++ = par;		// push, then fall through
		case 0: par = mp; break;
		case 1: par = *--sp; break;
		}
	}
	return 1;
}

} // extern "C"
