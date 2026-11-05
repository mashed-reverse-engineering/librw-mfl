// UVANIM (LIB_RW group uvanim; output/rw_next_family_2026-11-05.md row 9): the RpUVAnim material update of MFL.exe, written op for op from
// its disassembly like rwcolltree.cpp (X = a value the original keeps on the x87 stack, R() = its float store):
//   _rpUVAnimCreateConsistantMatrices 0x562020, RpMaterialUVAnimApplyUpdate 0x562310,
//   RpUVAnimParamKeyFrameInterpolate 0x5626c0 and RpUVAnimLinearKeyFrameInterpolate 0x562ae0 (interpolator callbacks, unsplit by Ghidra: fnsizes rows).
// The update compares the interpolator's apply callback with the exe's linear / param apply functions (0x562a90 / 0x562620); the DLL reads
// those two addresses from the exe's interpolator info blocks (+0xc keyFrameApplyCB), the values the original has as immediates.
// Exe-layout adapter: mflrw_uvanim_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RpUVAnimMaterialGlobals 0x947b58,
// _rpUVAnimLinearInterpolatorInfo 0x63a3c0, _rpUVAnimParamInterpolatorInfo 0x63a3f0, rpUVAnimPivot 0x5f5d8c, rpUVAnimInvPivot 0x5f5d98,
// RwMatrixCreate 0x4d4e70, RwMatrixDestroy 0x4d4e40, RwMatrixUpdate 0x4d3cc0, RwMatrixTranslate 0x4d4870, RwMatrixRotate 0x4d43f0,
// RwMatrixTransform 0x4d49c0, RpMatFXMaterialSetUVTransformMatrices 0x550860.
// Check: tools/delink/rw_uvanim_check.sh (the original code and these exports natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint32_t u32;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
#define F(p, off)  (*(float *)((u8 *)(p) + (off)))
#define W(p, off)  (*(u32 *)((u8 *)(p) + (off)))

typedef void *(__cdecl *MatCreateFn)(void);
typedef int (__cdecl *MatDestroyFn)(void *m);
typedef void *(__cdecl *MatUpdateFn)(void *m);
typedef void *(__cdecl *MatTranslateFn)(void *m, const void *v, int op);
typedef void *(__cdecl *MatRotateFn)(void *m, const void *axis, float angle, int op);
typedef void *(__cdecl *MatTransformFn)(void *m, const void *t, int op);
typedef void *(__cdecl *SetUVFn)(void *mat, void *m0, void *m1);
typedef void (__cdecl *ApplyFn)(void *m, void *kf);

// ---- adapter
static int32_t *g_globals;		// RpUVAnimMaterialGlobals 0x947b58: material plugin offset
static const u8 *g_linInfo, *g_parInfo;	// interpolator info blocks 0x63a3c0 / 0x63a3f0
static const void *g_pivot, *g_invPivot;	// 0x5f5d8c / 0x5f5d98
static MatCreateFn g_create; static MatDestroyFn g_destroy; static MatUpdateFn g_update; static MatTranslateFn g_translate;
static MatRotateFn g_rotate; static MatTransformFn g_transform; static SetUVFn g_setUV;
static void unbound(void){ fprintf(stderr, "[mflrw] uvanim called before mflrw_uvanim_bind\n"); abort(); }

// the original's float constants: 0x5de274 180/pi, 0x5f5da4 -pi, 0x5de1f8 pi, 0x5de430 2pi (bit patterns)
static const union { u32 u; float f; } kRadToDeg = {0x42652ee0u}, kMinusPi = {0xc0490fdbu}, kPi = {0x40490fdbu}, kTwoPi = {0x40c90fdbu};

extern "C" {

void __cdecl mflrw_uvanim_bind(int32_t *globals, const void *linInfo, const void *parInfo, const void *pivot, const void *invPivot,
                               void *create, void *destroy, void *update, void *translate, void *rotate, void *transform, void *setUV){
	g_globals = globals; g_linInfo = (const u8 *)linInfo; g_parInfo = (const u8 *)parInfo; g_pivot = pivot; g_invPivot = invPivot;
	g_create = (MatCreateFn)create; g_destroy = (MatDestroyFn)destroy; g_update = (MatUpdateFn)update; g_translate = (MatTranslateFn)translate;
	g_rotate = (MatRotateFn)rotate; g_transform = (MatTransformFn)transform; g_setUV = (SetUVFn)setUV;
}

// ext: {RwMatrix *uv[2]; RtAnimInterpolator *interp[8]}; interp: +0 current anim (-> +0x14 custom data, +0x20 u32 node -> channel map),
// +0x24 interpolated keyframe size, +0x2c node count, +0x3c apply callback, +0x4c first interpolated keyframe

// 0x562020: a matrix for each UV channel (0 / 1) some node of some interpolator maps to, the others destroyed.  The node scan is a
// do-while: node 0 is compared before the count is checked (as the original).  NULL when RwMatrixCreate fails.
u8 *__cdecl rw37__rpUVAnimCreateConsistantMatrices(u8 *ext){
	if(!g_create) unbound();
	for(u32 ch = 0; ch < 2; ch++){
		void **slot = (void **)ext + ch;
		u32 node = 0, i = 0;
		int found = 0;
		while(i < 8){
			u8 *ip = *(u8 **)(ext + 8 + 4*i);
			if(ip){
				const u32 *map = (const u32 *)(*(u8 **)(*(u8 **)ip + 0x14) + 0x20);
				if(map[node] == ch){ found = 1; break; }
				node++;
				if(node < W(ip, 0x2c)) continue;
				node = 0;
			}
			i++;
		}
		if(found){
			if(*slot == 0){
				void *m = g_create();
				*slot = m;
				if(!m) return 0;
			}
		}else if(*slot){
			g_destroy(*slot);
			*slot = 0;
		}
	}
	return ext;
}

// 0x562310: both UV matrices to identity, then every node of every interpolator that maps to an existing channel matrix
// transforms it (preconcat) by its keyframe's matrix: built inline for the exe's linear / param apply callbacks, else by the
// callback.  The keyframe pointer only advances on a node that was applied (as the original).  The local matrix's flags and pad
// words are never written (as the original; RwMatrixUpdate clears the flags' identity bits).
u8 *__cdecl rw37_RpMaterialUVAnimApplyUpdate(u8 *mat){
	if(!g_globals) unbound();
	u8 *ext = mat + *g_globals;
	float M[16];
	float axis[3];
	for(u32 c = 0; c < 2; c++){
		u8 *m = *(u8 **)(ext + 4*c);
		if(!m) continue;
		u32 fl = W(m, 0xc);
		F(m, 0x28) = 1.0f; F(m, 0x14) = 1.0f; F(m, 0) = 1.0f;
		W(m, 0x10) = 0; W(m, 8) = 0; W(m, 4) = 0; W(m, 0x24) = 0; W(m, 0x20) = 0; W(m, 0x18) = 0; W(m, 0x38) = 0; W(m, 0x34) = 0; W(m, 0x30) = 0;
		W(m, 0xc) = fl | 0x20003;
	}
	void *linApply = *(void *const *)(g_linInfo + 0xc), *parApply = *(void *const *)(g_parInfo + 0xc);
	for(u32 i = 0; i < 8; i++){
		u8 *ip = *(u8 **)(ext + 8 + 4*i);
		if(!ip) continue;
		u32 stride = W(ip, 0x24);
		ApplyFn cb = *(ApplyFn *)(ip + 0x3c);
		const u32 *map = (const u32 *)(*(u8 **)(*(u8 **)ip + 0x14) + 0x20);
		u8 *kf = ip + 0x4c;
		for(int32_t n = 0; n < (int32_t)W(ip, 0x2c); n++){
			u32 ch = map[n];
			if(ch >= 2) continue;
			void *dst = *(void **)(ext + 4*ch);
			if(!dst) continue;
			if((void *)cb == linApply){
				memcpy(&M[0], kf + 8, 4); memcpy(&M[1], kf + 0xc, 4); memcpy(&M[4], kf + 0x10, 4); memcpy(&M[5], kf + 0x14, 4);
				M[2] = 0; M[6] = 0; M[8] = 0; M[9] = 0; M[10] = 0;
				memcpy(&M[12], kf + 0x18, 4); memcpy(&M[13], kf + 0x1c, 4); M[14] = 0;
				g_update(M);
			}else if((void *)cb == parApply){
				memcpy(&M[0], kf + 0xc, 4); memcpy(&M[1], kf + 0x14, 4); memcpy(&M[5], kf + 0x10, 4);
				axis[0] = 0; axis[1] = 0; axis[2] = 1.0f;
				M[2] = 0; M[4] = 0; M[6] = 0; M[8] = 0; M[9] = 0; M[10] = 0;
				memcpy(&M[12], kf + 0x18, 4); memcpy(&M[13], kf + 0x1c, 4); M[14] = 0;
				g_update(M);
				g_translate(M, g_pivot, 2);
				g_rotate(M, axis, R((X)F(kf, 8) * kRadToDeg.f), 2);
				g_translate(M, g_invPivot, 2);
			}else
				cb(M, kf);
			g_transform(dst, M, 1);
			kf += stride;
		}
	}
	g_setUV(mat, *(void **)ext, *(void **)(ext + 4));
	return mat;
}

// keyframe {prev, time, ...}: linear +8..+0x1c the six matrix floats; param +8 angle (radians, wrapped into [-pi, pi] on the way), +0xc..+0x1c
static inline X kfT(const u8 *a, const u8 *b, float time){ return ((X)time - F(a, 4)) / ((X)F(b, 4) - F(a, 4)); }

// 0x5626c0: out = a + (b - a) * t, the angle difference wrapped by 2pi first (a NaN difference is left as it is)
void __cdecl rw37_RpUVAnimParamKeyFrameInterpolate(u8 *out, const u8 *a, const u8 *b, float time){
	X t = kfT(a, b, time);
	X d = (X)F(b, 8) - F(a, 8);
	if(d < kMinusPi.f) d += kTwoPi.f;
	else if(d > kPi.f) d -= kTwoPi.f;
	F(out, 8) = R(d * t + F(a, 8));
	for(u32 o = 0xc; o <= 0x1c; o += 4) F(out, o) = R(((X)F(b, o) - F(a, o)) * t + F(a, o));
}

// 0x562ae0: out = a + (b - a) * t for the six floats
void __cdecl rw37_RpUVAnimLinearKeyFrameInterpolate(u8 *out, const u8 *a, const u8 *b, float time){
	X t = kfT(a, b, time);
	for(u32 o = 8; o <= 0x1c; o += 4) F(out, o) = R(((X)F(b, o) - F(a, o)) * t + F(a, o));
}

} // extern "C"
