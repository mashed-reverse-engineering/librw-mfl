// CAMSYNC (LIB_RW group camsync; output/rw_next_family_2026-11-05.md row 10): the camera's frame sync callback of MFL.exe (0x4d1f70, set by
// RwCameraCreate at camera +0x10; unsplit by Ghidra: fnsizes row CameraSync), written op for op from its disassembly like rwcolltree.cpp
// (X = a value the original keeps on the x87 stack, R() = its float store).  Perspective: the view matrix from the frame's LTM, view window
// and view offset, then RwMatrixOptimize and CameraBuildPerspClipPlanes; parallel: CameraBuildParallelViewMatrix / ParallelClipPlanes;
// then RwBBoxCalculate over the 8 frustum corners.
// Exe-layout adapter: mflrw_camsync_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over RwMatrixOptimize 0x4d3c00,
// CameraBuildPerspClipPlanes 0x4d2110, CameraBuildParallelViewMatrix 0x4d2bc0, CameraBuildParallelClipPlanes 0x4d27b0, RwBBoxCalculate 0x4d18d0.
// Check: tools/delink/rw_camsync_check.sh (the original code and this export natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint32_t u32;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
#define F(p, off)  (*(float *)((u8 *)(p) + (off)))
#define W(p, off)  (*(u32 *)((u8 *)(p) + (off)))

typedef void *(__cdecl *CamFn)(void *cam);
typedef void *(__cdecl *OptimizeFn)(void *m, const void *tol);
typedef void *(__cdecl *BBoxCalcFn)(void *bbox, const void *verts, int n);

static OptimizeFn g_optimize; static CamFn g_perspClip, g_parView, g_parClip; static BBoxCalcFn g_bboxCalc;

// the original's float constants: 0x5de6a8 -0.5, 0x5dd340 0.5
static const float kMinusHalf = -0.5f, kHalf = 0.5f;

extern "C" {

void __cdecl mflrw_camsync_bind(void *optimize, void *perspClip, void *parView, void *parClip, void *bboxCalc){
	g_optimize = (OptimizeFn)optimize; g_perspClip = (CamFn)perspClip; g_parView = (CamFn)parView; g_parClip = (CamFn)parClip; g_bboxCalc = (BBoxCalcFn)bboxCalc;
}

// camera: +4 frame (LTM at +0x50: right 0, up 0x10, at 0x20, pos 0x30), +0x14 projection (1 perspective), +0x20 view matrix,
// +0x70 / +0x74 recip view window, +0x78 / +0x7c view offset, +0x10c bbox, +0x124 frustum corners
u8 *__cdecl rw37_CameraSync(u8 *cam){
	if(!g_optimize){ fprintf(stderr, "[mflrw] camsync called before mflrw_camsync_bind\n"); abort(); }
	if(W(cam, 0x14) == 1){
		const u8 *L = *(u8 **)(cam + 4) + 0x50;
		u8 *V = cam + 0x20;
		// row 0 from right and at
		X a = (X)F(cam, 0x70) * kMinusHalf;
		X rx = a * F(L, 0);
		float t1 = R(a * F(L, 4)), t2 = R(a * F(L, 8));
		float s = R(kHalf - F(cam, 0x78) * a);
		X m0 = (X)F(L, 0x20) * s + rx;
		X m1 = (X)F(L, 0x24) * s + t1;
		X m2 = (X)F(L, 0x28) * s + t2;
		F(V, 0) = R(m0); F(V, 0x10) = R(m1); F(V, 0x20) = R(m2);
		F(V, 0x30) = R(kHalf - (((X)F(L, 0x34) * m1 + (X)F(L, 0x30) * m0 + (X)F(L, 0x38) * m2) + s));
		// row 1 from up and at
		X b = (X)F(cam, 0x74) * kMinusHalf;
		X ux = b * F(L, 0x10);
		t1 = R(b * F(L, 0x14)); t2 = R(b * F(L, 0x18));
		s = R(F(cam, 0x7c) * b + kHalf);
		m0 = (X)F(L, 0x20) * s + ux;
		m1 = (X)F(L, 0x24) * s + t1;
		m2 = (X)F(L, 0x28) * s + t2;
		F(V, 4) = R(m0); F(V, 0x14) = R(m1); F(V, 0x24) = R(m2);
		F(V, 0x34) = R(kHalf - (((X)F(L, 0x34) * m1 + (X)F(L, 0x30) * m0 + (X)F(L, 0x38) * m2) + s));
		// row 2 = at, -at.pos
		W(V, 8) = W(L, 0x20); W(V, 0x18) = W(L, 0x24); W(V, 0x28) = W(L, 0x28);
		F(V, 0x38) = R(-((X)F(L, 0x24) * F(L, 0x34) + (X)F(L, 0x30) * F(L, 0x20) + (X)F(L, 0x38) * F(L, 0x28)));
		g_optimize(V, 0);
		g_perspClip(cam);
	}else{
		g_parView(cam);
		g_parClip(cam);
	}
	g_bboxCalc(cam + 0x10c, cam + 0x124, 8);
	return cam;
}

} // extern "C"
