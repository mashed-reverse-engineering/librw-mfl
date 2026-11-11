// MATFX2 (group matfx2, lane K window 5 batch 4): the two big effectpipesd3d9 rows of MFL.exe, written op for op from the disassembly.
//   _rpMatFXD3D9AtomicMatFXDualPassRender 0x550b50 (3339 B), CalculatePerturbedUVs 0x552850 (4672 B, x87).
// Exe binding: mflrw_matfx2_bind(&RwEngineInstance, table), table order = enum X below.  Unbound: abort.  Check: tools/delink/rw_matfx2_check.sh.
// Floating point: values the original keeps in x87 registers are double here, values it stores to a float stack slot are volatile float (so the check holds at control word 0x7f and 0x27f).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum X { X_MATOFF, X_MULTITEX, X_MAXSTAGES, X_MULADD, X_MOD2X, X_VALID2X, X_STATE55D0, X_SETTSS, X_SETRS, X_GETRS, X_CONVNONPAL, X_SETTRANSFORM, X_FLUSH, X_MATSCRATCH,
         X_DEVICE, X_LASTPS, X_LASTVS, X_4E5950, X_VALPHA, X_SRCBLEND, X_DESTBLEND, X_DEFRENDER,
         X_GETTRANSFORM, X_MATINVERT, X_GETLTM, X_V3DPOINT, X_V3DVECTOR, X_INVSQRT, X_UNINSTV3D, X_UNINSTV2D, X_N };
static u8 **g_engine;
static void *g_m[X_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] matfx2 called before mflrw_matfx2_bind\n"); abort(); }
	return *g_engine;
}
typedef void *(__cdecl *MallocFn)(u32 n, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void (__cdecl *V0Fn)(void);
typedef int (__cdecl *V0RFn)(void);
typedef void (__cdecl *V1Fn)(u32 a);
typedef void (__cdecl *V2Fn)(u32 a, u32 b);
typedef void (__cdecl *V3Fn)(u32 a, u32 b, u32 c);
typedef void (__cdecl *R4Fn)(void *a, void *b, u32 c, void *d);
typedef void (__cdecl *GetRSFn)(u32 state, u32 *out);
typedef void (__cdecl *SetXformFn)(u32 state, void *m);
typedef void (__cdecl *Conv1Fn)(void *raster);
typedef void (__cdecl *EngRSFn)(u32 id, u32 *p);
typedef void (__cdecl *EngRSSetFn)(u32 id, u32 v);
typedef long (__stdcall *DevSetFn)(void *dev, u32 v);
typedef long (__stdcall *DevValidateFn)(void *dev, u32 *passes);
typedef long (__stdcall *DevDrawIdxFn)(void *dev, u32 type, u32 base, u32 zero, u32 nv, u32 start, u32 count);
typedef long (__stdcall *DevDrawFn)(void *dev, u32 type, u32 start, u32 count);

#define T(a, b, c) ((V3Fn)g_m[X_SETTSS])((a), (b), (c))
#define S55(a, b) ((V2Fn)g_m[X_STATE55D0])((u32)(uintptr_t)(a), (b))
#define SETRS(a, b) ((V2Fn)g_m[X_SETRS])((a), (b))
#define GETRS(a, p) ((GetRSFn)g_m[X_GETRS])((a), (p))

extern "C" {

void __cdecl mflrw_matfx2_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < X_N; i++) g_m[i] = tbl[i];
}

static void dualDraw(u8 *hdr, u8 *mesh){
	if(*(u32 *)(hdr + 8)){
		((V0Fn)g_m[X_FLUSH])();
		void *dev = *(void **)g_m[X_DEVICE];
		(*(DevDrawIdxFn *)((u8 *)*(void **)dev + 0x148))(dev, *(u32 *)(hdr + 0xc), *(u32 *)(mesh + 0x14), 0, *(u32 *)(mesh + 0x18), *(u32 *)(mesh + 0x1c), *(u32 *)(mesh + 0x20));
	}else{
		((V0Fn)g_m[X_FLUSH])();
		void *dev = *(void **)g_m[X_DEVICE];
		(*(DevDrawFn *)((u8 *)*(void **)dev + 0x144))(dev, *(u32 *)(hdr + 0xc), *(u32 *)(mesh + 0x14), *(u32 *)(mesh + 0x20));
	}
}
// copies the 2D part of a texture matrix (words 0 1 4 5 12 13) into the exe's scratch matrix
static void loadMat(const u32 *s){
	u32 *g = (u32 *)g_m[X_MATSCRATCH];
	g[0] = s[0]; g[1] = s[1]; g[4] = s[4]; g[5] = s[5]; g[8] = s[12]; g[9] = s[13];
}

// _rpMatFXD3D9AtomicMatFXDualPassRender 0x550b50 (resentry header, mesh, flags, texture, second texture): two texture MatFX material (type 4: dual pass, 6: dual pass with texture
// transforms).  The material's blend pair (source, destination: p[1], p[2]) picks a single pass multitexture setup (stage 1 / 2 states for the card's blend stage count, validated once
// for modulate 2x) when the card can do it; otherwise (and when it cannot) the mesh is drawn with the base texture and a second pass blends the second texture on with the pair.
// Without a second texture raster this is the default render.
void __cdecl rw37_rpMatFXD3D9AtomicMatFXDualPassRender(u8 *hdr, u8 *mesh, u32 flags, void **tex, void **tex2){
	u8 *md, *mdv;
	u32 *p, *s, src, dst, ebx = 1, passes, sA, sB, s1, s2, s3 = 0, f80;
	void *dev;
	long hr;
	if(!tex2 || !*tex2){ ((R4Fn)g_m[X_DEFRENDER])(hdr, mesh, flags, tex); return; }
	md = *(u8 **)(*(int32_t *)g_m[X_MATOFF] + *(u8 **)(mesh + 8));
	if(*(int32_t *)(md + 0x30) == 6){ p = (u32 *)(md + 0x18); mdv = md; }
	else{ p = (u32 *)md; mdv = 0; }
	if(flags & 0x84){
		S55(tex, 0);
		T(0, 1, 4); T(0, 2, 2); T(0, 3, 0); T(0, 4, 4); T(0, 5, 2); T(0, 6, 0);
	}else{
		S55(0, 0);
		T(0, 1, 3); T(0, 3, 0); T(0, 4, 3); T(0, 6, 0);
	}
	if(mdv){
		s = *(u32 **)mdv;
		if(s && !(s[3] & 0x20000)){
			T(0, 0x18, 2);
			loadMat(s);
			((SetXformFn)g_m[X_SETTRANSFORM])(0x10, g_m[X_MATSCRATCH]);
		}
	}
	if(tex && (*(u8 **)tex)[0x23] & 0x20 && (*(u8 **)tex2)[0x23] & 0x20 && tex != tex2) ((Conv1Fn)g_m[X_CONVNONPAL])(*tex2);
	if(!*(u32 *)g_m[X_MULTITEX]) goto L253;
	src = p[1]; dst = p[2];
	if((src == 9 && dst == 1) || (src == 1 && dst == 3)){
		if(tex){
			S55(tex2, 1);
			if(*(u32 *)g_m[X_MAXSTAGES] == 3){
				T(0, 1, 2); T(0, 2, 2); T(0, 4, 2); T(0, 5, 2);
				T(1, 1, 4); T(1, 2, 2); T(1, 3, 1); T(1, 4, 3); T(1, 6, 1);
				T(2, 1, 4); T(2, 2, 0); T(2, 3, 1); T(2, 4, 4); T(2, 5, 0); T(2, 6, 1);
			}else{
				T(1, 1, 4); T(1, 2, 2); T(1, 3, 1); T(1, 4, 3); T(1, 6, 1);
			}
		}else{
			S55(tex2, 0);
			T(0, 1, 4); T(0, 2, 2); T(0, 3, 0); T(0, 4, 4); T(0, 5, 2); T(0, 6, 0);
		}
		ebx = 0; goto L5d8;
	}
	if(src == 5 && dst == 6){
		if(tex){
			if(*(u32 *)g_m[X_MAXSTAGES] < 3) goto L253;
			S55(tex2, 1);
			T(0, 1, 2); T(0, 2, 2); T(0, 4, 2); T(0, 5, 2);
			T(1, 1, 0xd); T(1, 2, 2); T(1, 3, 1); T(1, 4, 0xd); T(1, 5, 2); T(1, 6, 1);
			T(2, 1, 4); T(2, 2, 0); T(2, 3, 1); T(2, 4, 4); T(2, 5, 0); T(2, 6, 1);
		}else{
			SETRS(0x3c, 0xffffffffu);
			S55(tex2, 0);
			T(0, 1, 0xd); T(0, 2, 2); T(0, 3, 3); T(0, 4, 3); T(0, 5, 2); T(0, 6, 3);
			T(1, 1, 4); T(1, 2, 0); T(1, 3, 1); T(1, 4, 4); T(1, 5, 0); T(1, 6, 1);
		}
		ebx = 0; goto L5d8;
	}
	if(src == 9 && dst == 3){
		if(!*(u32 *)g_m[X_MOD2X]) goto L253;
		if(tex){
			S55(tex2, 1);
			T(1, 1, 5); T(1, 2, 2); T(1, 3, 1);
			if(*(u32 *)g_m[X_MAXSTAGES] == 3){
				T(0, 1, 2); T(0, 2, 2); T(0, 4, 2); T(0, 5, 2); T(1, 4, 3); T(1, 6, 1);
				T(2, 1, 4); T(2, 2, 0); T(2, 3, 1); T(2, 4, 4); T(2, 5, 0); T(2, 6, 1);
			}else{
				T(1, 4, 3); T(1, 6, 1);
			}
		}else{
			S55(tex2, 0);
			T(0, 1, 5); T(0, 4, 3); T(0, 6, 0);
		}
		if(!*(u32 *)g_m[X_VALID2X]){ ebx = 0; goto L5d8; }
		passes = 0;
		((V0Fn)g_m[X_FLUSH])();
		dev = *(void **)g_m[X_DEVICE];
		hr = (*(DevValidateFn *)((u8 *)*(void **)dev + 0x118))(dev, &passes);
		if(hr >= 0 && passes == 1) ebx = 0;
		else{
			*(u32 *)g_m[X_MOD2X] = 0;
			S55(tex, 0);
			T(0, 1, 4); T(0, 2, 2); T(0, 3, 0); T(0, 4, 4); T(0, 5, 2); T(0, 6, 0);
			T(1, 1, 1); T(1, 4, 1); T(2, 1, 1); T(2, 4, 1);
			S55(0, 1);
		}
		*(u32 *)g_m[X_VALID2X] = 0;
		if(ebx) goto L253;
		goto L5d8;
	}
	if(src == 2 && dst == 2){
		if(tex){
			if(!*(u32 *)g_m[X_MULADD]) goto L253;
			S55(tex2, 1);
			T(1, 1, 0x19); T(1, 0x1a, 1); T(1, 2, 2); T(1, 3, 0); T(1, 4, 3); T(1, 6, 1);
		}else{
			S55(tex2, 0);
			T(0, 4, 3); T(0, 6, 0);
			T(1, 1, 7); T(1, 2, 0); T(1, 3, 1); T(1, 4, 3); T(1, 6, 1);
		}
		ebx = 0; goto L5d8;
	}
	if(src == 1 && dst == 5){
		if(tex){
			S55(tex2, 1);
			if(*(u32 *)g_m[X_MAXSTAGES] == 3){
				T(0, 1, 2); T(0, 2, 2); T(0, 4, 2); T(0, 5, 2);
				T(1, 1, 4); T(1, 2, 0x22); T(1, 3, 1); T(1, 4, 3); T(1, 6, 1);
				T(2, 1, 4); T(2, 2, 0); T(2, 3, 1); T(2, 4, 4); T(2, 5, 0); T(2, 6, 1);
			}else{
				T(1, 1, 4); T(1, 2, 0x22); T(1, 3, 1); T(1, 4, 3); T(1, 6, 1);
			}
		}else{
			S55(tex2, 0);
			T(0, 1, 4); T(0, 2, 0x22); T(0, 3, 0); T(0, 4, 3); T(0, 6, 0);
		}
		ebx = 0; goto L5d8;
	}
	goto L253;
L5d8:
	if(!(flags & 0x80)) T(1, 0xb, 0);
	if(mdv){
		s = *(u32 **)(mdv + 4);
		if(s && !(s[3] & 0x20000)){
			T(1, 0x18, 2);
			loadMat(s);
			((SetXformFn)g_m[X_SETTRANSFORM])(0x11, g_m[X_MATSCRATCH]);
		}
	}
L253:
	if(*(u32 *)g_m[X_LASTPS]){
		dev = *(void **)g_m[X_DEVICE];
		*(u32 *)g_m[X_LASTPS] = 0;
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, 0);
	}
	if(*(u32 *)g_m[X_LASTVS] != *(u32 *)(mesh + 0x10)){
		*(u32 *)g_m[X_LASTVS] = *(u32 *)(mesh + 0x10);
		dev = *(void **)g_m[X_DEVICE];
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x170))(dev, *(u32 *)(mesh + 0x10));
	}
	dualDraw(hdr, mesh);
	if(ebx == 0){
		// single pass: back to the plain stage states
		S55(0, 1);
		T(0, 1, 4); T(0, 2, 2); T(0, 3, 0); T(0, 4, 4); T(0, 5, 2); T(0, 6, 0);
		T(1, 1, 1); T(1, 4, 1); T(2, 1, 1); T(2, 4, 1);
		if(!(flags & 0x80)) T(1, 0xb, 1);
	}else{
		// second pass: the second texture blended on with the material's blend pair
		S55(tex2, 0);
		if(mdv){
			s = *(u32 **)(mdv + 4);
			if(s && !(s[3] & 0x20000)){
				T(0, 0x18, 2);
				loadMat(s);
				((SetXformFn)g_m[X_SETTRANSFORM])(0x10, g_m[X_MATSCRATCH]);
			}
		}
		if(!((V0RFn)g_m[X_4E5950])()) ((V1Fn)g_m[X_VALPHA])(1);
		src = p[1]; dst = p[2];
		if((src == 9 && dst == 1) || (src == 1 && dst == 3) || (src == 9 && dst == 3)) SETRS(0xf, 0);
		(*(EngRSFn *)(eng() + 0x24))(0xa, &sA);
		(*(EngRSFn *)(eng() + 0x24))(0xb, &sB);
		((V1Fn)g_m[X_SRCBLEND])(p[1]);
		((V1Fn)g_m[X_DESTBLEND])(p[2]);
		GETRS(0xe, &s1);
		SETRS(0xe, 0);
		GETRS(0x1c, &s2);
		if(s2){
			GETRS(0x22, &s3);
			if(p[2] == 2) SETRS(0x22, 0);
			else if(p[1] == 9 || p[2] == 3) SETRS(0x22, 0xffffffffu);
		}
		f80 = flags & 0x80;
		if(f80) T(0, 0xb, 1);
		dualDraw(hdr, mesh);
		SETRS(0xe, s1);
		if(s2) SETRS(0x22, s3);
		if(f80) T(0, 0xb, 0);
		((V1Fn)g_m[X_VALPHA])(0);
		(*(EngRSSetFn *)(eng() + 0x20))(0xa, sA);
		(*(EngRSSetFn *)(eng() + 0x20))(0xb, sB);
	}
	if(mdv){
		T(0, 0x18, 0);
		T(1, 0x18, 0);
	}
}

// CalculatePerturbedUVs 0x552850 (resentry header, mesh, output vertices): for a bump mapped mesh builds the 0x2c byte dynamic vertex format {position, normal, colour, uv, perturbed uv}
// per vertex.  The light (the first light attached to the material's frame, or the camera frame; a directional light gives -at, anything else its position) is brought into object
// space with the inverse of the device's world matrix; per vertex the unit vector l from the vertex to it is dotted with the tangent frame: perturbed uv = uv + scale * (T.l, B.l)
// (scale = material float 4 * float 3).  With a tangent stream T is the vertex tangent and B = T x N; without one the tangent is derived per triangle (from the index buffer,
// or from the vertex order for triangle lists / strips) and the perturbed uv is uv - scale * (T.l, B.l).  The vertex buffers (and index buffer) are locked and unlocked here.
typedef double (__cdecl *InvSqrtFn)(float x);
typedef void (__cdecl *GetXformFn)(u32 state, float *m);
typedef void (__cdecl *Mat2Fn)(void *dst, void *src);
typedef void *(__cdecl *GetLtmFn)(void *frame);
typedef void (__cdecl *V3dXFn)(float *out, float *in, float *m);
typedef void (__cdecl *UnInstFn)(u32 type, float *out, const void *src);
typedef long (__stdcall *LockFn)(void *self, u32 off, u32 size, void **pp, u32 flags);
typedef long (__stdcall *UnlockFn)(void *self);
typedef long (__stdcall *GetDeclFn)(void *self, u8 *elems, u32 *num);

struct PUV {
	u8 *hdr, *mesh;
	u32 *out;
	float scale;
	float L[3];
	u8 *posBase, *normBase, *colBase, *uvBase;
	u32 posType, normType, uvType, posStride, normStride, colStride, uvStride;
};
static void ld3(const PUV &c, u32 type, u32 stride, u8 *base, u32 v, volatile float *dst){
	const u32 *src = (const u32 *)(base + stride * v);
	if(type == 2){ ((volatile u32 *)dst)[0] = src[0]; ((volatile u32 *)dst)[1] = src[1]; ((volatile u32 *)dst)[2] = src[2]; }
	else ((UnInstFn)g_m[X_UNINSTV3D])(type, (float *)dst, src);
}
static void ld2(const PUV &c, u32 v, volatile float *dst){
	const u32 *src = (const u32 *)(c.uvBase + c.uvStride * v);
	if(c.uvType == 1){ ((volatile u32 *)dst)[0] = src[0]; ((volatile u32 *)dst)[1] = src[1]; }
	else ((UnInstFn)g_m[X_UNINSTV2D])(c.uvType, (float *)dst, src);
}
static inline double sq3(volatile float *d){ return (double)d[1] * d[1] + (double)d[0] * d[0] + (double)d[2] * d[2]; }

// one vertex v0 of a triangle (v0, v1, v2) without a tangent stream: T from the position and uv edges (kept as the original: floats where it stores, doubles in registers)
static void triVertex(const PUV &c, u32 v0, u32 v1, u32 v2){
	volatile float P0[3], P1[3], P2[3], UV0[2], UV1[2], UV2[2], N[3], D[3], l[3], Tr[3], R[3];
	volatile float E1z, E2[3], dv2, e1xdv2;
	ld3(c, c.posType, c.posStride, c.posBase, v0, P0);
	ld3(c, c.posType, c.posStride, c.posBase, v1, P1);
	ld3(c, c.posType, c.posStride, c.posBase, v2, P2);
	ld2(c, v0, UV0); ld2(c, v1, UV1); ld2(c, v2, UV2);
	if(c.normBase) ld3(c, c.normType, c.normStride, c.normBase, v0, N);
	else{ ((volatile u32 *)N)[0] = 0; ((volatile u32 *)N)[1] = 0; ((volatile u32 *)N)[2] = 0; }
	D[0] = c.L[0] - P0[0]; D[1] = c.L[1] - P0[1]; D[2] = c.L[2] - P0[2];
	{
		float s = (float)sq3(D);
		double inv = ((InvSqrtFn)g_m[X_INVSQRT])(s);
		l[0] = (float)((double)D[0] * inv); l[1] = (float)((double)D[1] * inv); l[2] = (float)((double)D[2] * inv);
	}
	double E1x = (double)P1[0] - P0[0], E1y = (double)P1[1] - P0[1];
	E1z = P1[2] - P0[2];
	E2[0] = P2[0] - P0[0]; E2[1] = P2[1] - P0[1]; E2[2] = P2[2] - P0[2];
	dv2 = UV2[1] - UV0[1];
	e1xdv2 = (float)(E1x * dv2);
	double A = E1y * dv2;
	double E1zdv2 = (double)E1z * dv2;
	double dv1 = (double)UV1[1] - UV0[1];
	double E2xdv1 = (double)E2[0] * dv1;
	E2[1] = (float)((double)E2[1] * dv1);
	E2[2] = (float)((double)E2[2] * dv1);
	Tr[0] = (float)((double)e1xdv2 - E2xdv1);
	Tr[1] = (float)(A - E2[1]);
	Tr[2] = (float)(E1zdv2 - E2[2]);
	double inv2;
	{
		float s = (float)sq3(Tr);
		inv2 = ((InvSqrtFn)g_m[X_INVSQRT])(s);
	}
	double a = (double)Tr[0] * inv2, b = (double)Tr[1] * inv2, cc = (double)Tr[2] * inv2;
	R[0] = (float)(b * N[2] - cc * N[1]);
	R[1] = (float)((double)N[0] * cc - a * N[2]);
	R[2] = (float)(a * N[1] - b * N[0]);
	double Tdl = (b * l[1] + a * l[0]) + cc * l[2];
	double Bdl = ((double)R[1] * l[1] + (double)R[0] * l[0]) + (double)R[2] * l[2];
	u32 *o = c.out + 11 * v0;
	o[0] = ((volatile u32 *)P0)[0]; o[1] = ((volatile u32 *)P0)[1]; o[2] = ((volatile u32 *)P0)[2];
	if(c.normBase){ o[3] = ((volatile u32 *)N)[0]; o[4] = ((volatile u32 *)N)[1]; o[5] = ((volatile u32 *)N)[2]; }
	else{ o[3] = 0; o[4] = 0; o[5] = 0; }
	o[6] = c.colBase ? *(u32 *)(c.colBase + v0 * c.colStride) : 0xffffffffu;
	o[7] = ((volatile u32 *)UV0)[0]; o[8] = ((volatile u32 *)UV0)[1];
	*(float *)(o + 9) = (float)((double)UV0[0] - Tdl * c.scale);
	*(float *)(o + 10) = (float)((double)UV0[1] - Bdl * c.scale);
}

int32_t __cdecl rw37_CalculatePerturbedUVs(u8 *hdr, u8 *mesh, u32 *out){
	PUV c;
	u8 *md = *(u8 **)(*(int32_t *)g_m[X_MATOFF] + *(u8 **)(mesh + 8));
	volatile float scale;
	scale = *(float *)(md + 0x10) * *(float *)(md + 0xc);
	u8 *frame = *(u8 **)md;
	if(!frame) frame = *(u8 **)(*(u8 **)eng() + 4);
	float W[16], Wi[16];
	((GetXformFn)g_m[X_GETTRANSFORM])(0x100, W);
	((Mat2Fn)g_m[X_MATINVERT])(Wi, W);
	// the first light attached to the frame decides: directional (subtype 1 / 0x81 / 0x82) gives -at, everything else the frame position
	{
		u8 *head = frame + 0x90, *node = *(u8 **)head;
		int dir = 0;
		for(; node != head; node = *(u8 **)node){
			if(node[-8] == 3){
				u8 st = (node - 8)[1];
				dir = (st == 1 || st == 0x81 || st == 0x82);
				break;
			}
		}
		if(dir){
			float *ltm = (float *)((GetLtmFn)g_m[X_GETLTM])(frame);
			volatile float nv[3];
			nv[0] = -ltm[8]; nv[1] = -ltm[9]; nv[2] = -ltm[10];
			((V3dXFn)g_m[X_V3DVECTOR])(c.L, (float *)nv, Wi);
		}else{
			float *ltm = (float *)((GetLtmFn)g_m[X_GETLTM])(frame);
			((V3dXFn)g_m[X_V3DPOINT])(c.L, ltm + 12, Wi);
		}
	}
	u8 el[0xd0];
	u32 nelem;
	{
		void *decl = *(void **)(hdr + 0x34);
		(*(GetDeclFn *)((u8 *)*(void **)decl + 0x10))(decl, el, &nelem);
	}
	u8 *lockp[2];
	for(int j = 0; j < 2; j++){
		u8 *st = hdr + 0x10 + 16 * j;
		void *vb = *(void **)st;
		lockp[j] = 0;
		if(vb && (*(u16 *)(st + 0xc) & 0xff6)){
			u32 stride = *(u32 *)(st + 8);
			(*(LockFn *)((u8 *)*(void **)vb + 0x2c))(vb, *(u32 *)(mesh + 4) * stride + *(u32 *)(st + 4), *(u32 *)(mesh + 0x18) * stride, (void **)&lockp[j], 0x800);
		}
	}
	c.hdr = hdr; c.mesh = mesh; c.out = out; c.scale = scale;
#define EL(k) (el + 8 * (k))
#define BASE(k) (lockp[*(u16 *)EL(k)] + *(u16 *)(EL(k) + 2))
#define STRIDE(k) (*(u32 *)(hdr + ((u32)*(u16 *)EL(k) << 4) + 0x18))
	u32 k;
	for(k = 0; !(EL(k)[6] == 0 && EL(k)[7] == 0); k++);
	c.posBase = BASE(k); c.posType = EL(k)[4]; c.posStride = STRIDE(k);
	for(k = 0; !((EL(k)[6] == 3 && EL(k)[7] == 0) || EL(k)[4] == 0x11); k++);
	if(EL(k)[4] == 0x11) c.normBase = 0;
	else{ c.normBase = BASE(k); c.normType = EL(k)[4]; c.normStride = STRIDE(k); }
	for(k = 0; !((EL(k)[6] == 0xa && EL(k)[7] == 0) || EL(k)[4] == 0x11); k++);
	if(EL(k)[4] == 0x11) c.colBase = 0;
	else{ c.colBase = BASE(k); c.colStride = STRIDE(k); }
	for(k = 0; !(EL(k)[6] == 5 && EL(k)[7] == 0); k++);
	c.uvBase = BASE(k); c.uvStride = STRIDE(k); c.uvType = EL(k)[4];
	u8 *tanBase = 0;
	u32 tanType = 0, tanStride = 0;
	for(k = 0; !((EL(k)[6] == 6 && EL(k)[7] == 0) || EL(k)[4] == 0x11); k++);
	if(EL(k)[4] != 0x11){ tanType = EL(k)[4]; tanBase = BASE(k); tanStride = STRIDE(k); }
	int32_t nv = *(int32_t *)(mesh + 0x18);
	if(tanBase){
		// vertex tangents
		for(int32_t i = 0; i < nv; i++){
			volatile float P[3], UV[2], N[3], Tg[3], D[3];
			ld3(c, c.posType, c.posStride, c.posBase, i, P);
			ld2(c, i, UV);
			if(c.normBase) ld3(c, c.normType, c.normStride, c.normBase, i, N);
			else{ ((volatile u32 *)N)[0] = 0; ((volatile u32 *)N)[1] = 0; ((volatile u32 *)N)[2] = 0; }
			ld3(c, tanType, tanStride, tanBase, i, Tg);
			D[0] = c.L[0] - P[0]; D[1] = c.L[1] - P[1]; D[2] = c.L[2] - P[2];
			float s = (float)sq3(D);
			double inv = ((InvSqrtFn)g_m[X_INVSQRT])(s);
			double lx = (double)D[0] * inv, ly = inv * D[1], lz = inv * D[2];
			double X = (double)Tg[1] * N[2] - (double)Tg[2] * N[1];
			double Y = (double)Tg[2] * N[0] - (double)Tg[0] * N[2];
			volatile float Z;
			Z = (float)((double)Tg[0] * N[1] - (double)Tg[1] * N[0]);
			volatile float Tdl, Bdl;
			Tdl = (float)(((double)Tg[1] * ly + (double)Tg[0] * lx) + (double)Tg[2] * lz);
			Bdl = (float)((Y * ly + X * lx) + (double)Z * lz);
			u32 *o = out + 11 * i;
			o[0] = ((volatile u32 *)P)[0]; o[1] = ((volatile u32 *)P)[1]; o[2] = ((volatile u32 *)P)[2];
			if(c.normBase){ o[3] = ((volatile u32 *)N)[0]; o[4] = ((volatile u32 *)N)[1]; o[5] = ((volatile u32 *)N)[2]; }
			else{ o[3] = 0; o[4] = 0; o[5] = 0; }
			o[6] = c.colBase ? *(u32 *)(c.colBase + i * c.colStride) : 0xffffffffu;
			o[7] = ((volatile u32 *)UV)[0]; o[8] = ((volatile u32 *)UV)[1];
			*(float *)(o + 9) = (float)((double)Tdl * c.scale + UV[0]);
			*(float *)(o + 10) = (float)((double)Bdl * c.scale + UV[1]);
		}
	}else if(*(u32 *)(hdr + 8)){
		// no tangents, indexed: every vertex once, by the first triangle that reaches it
		u8 *ib = *(u8 **)(hdr + 8);
		u8 *done = (u8 *)(*(MallocFn *)(eng() + 0x108))((u32)nv, 0x30120);
		memset(done, 0, (u32)nv);
		u32 nidx = *(u32 *)mesh;
		u16 *idx;
		(*(LockFn *)((u8 *)*(void **)ib + 0x2c))(ib, 2 * *(u32 *)(mesh + 0x1c), 2 * nidx, (void **)&idx, 0x800);
		u32 pt = *(u32 *)(hdr + 0xc);
		u32 v1 = nidx, v2 = nidx, v0 = nidx;
		for(int32_t i = 0; i < (int32_t)nidx; i++){
			if(done[idx[i]]) continue;
			if(pt == 4){
				u32 base = 3 * ((u32)i / 3);
				v0 = idx[i]; v1 = idx[base + ((u32)i + 1) % 3]; v2 = idx[base + ((u32)i + 2) % 3];
			}else if(pt == 5){
				u32 a, b;
				if(i < 2){ if(i % 2 == 0){ a = i + 1; b = i + 2; } else{ a = i + 2; b = i + 1; } }
				else{ if(i % 2 == 0){ a = i - 2; b = i - 1; } else{ a = i - 1; b = i - 2; } }
				v0 = idx[i]; v1 = idx[a]; v2 = idx[b];
			}
			triVertex(c, v0, v1, v2);
			done[v0] = 1;
		}
		(*(UnlockFn *)((u8 *)*(void **)ib + 0x30))(ib);
		(*(FreeFn *)(eng() + 0x10c))(done);
	}else{
		// no tangents, not indexed: vertex order is the triangle order
		u32 pt = *(u32 *)(hdr + 0xc);
		u32 v0 = nv, v1 = nv, v2 = nv;
		for(int32_t i = 0; i < nv; i++){
			if(pt == 4){
				u32 base = 3 * ((u32)i / 3);
				v0 = i; v1 = base + ((u32)i + 1) % 3; v2 = base + ((u32)i + 2) % 3;
			}else if(pt == 5){
				v0 = i;
				if(i < 2){ if(i % 2 == 0){ v1 = i + 1; v2 = i + 2; } else{ v1 = i + 2; v2 = i + 1; } }
				else{ if(i % 2 == 0){ v1 = i - 2; v2 = i - 1; } else{ v1 = i - 1; v2 = i - 2; } }
			}
			triVertex(c, v0, v1, v2);
		}
	}
	for(int j = 0; j < 2; j++){
		if(lockp[j]){
			void *vb = *(void **)(hdr + 0x10 * (j + 1));
			(*(UnlockFn *)((u8 *)*(void **)vb + 0x30))(vb);
		}
	}
	return 1;
}

}
