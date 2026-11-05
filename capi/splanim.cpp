// SPLANIM (plan_librw_engine_switch_2026-11-03_v2 "spline FindPosition/FindMatrix" + "HAnim KeyFrameInterpolate, RtAnim Add/SubAnimTime"): leaf
// kernels of the RpSpline plugin, the HAnim keyframe interpolator and the RtAnim interpolator clock of MFL.exe, written op for op from its
// disassembly like collis.cpp (X = a value the original keeps on the x87 stack, float = a value it stores; comparisons keep the original's
// unordered (NaN) outcome; valid at the game's x87 CW 0x7f).
//   _rwV3dNormalize 0x4d51f0, _splCreateSpline 0x555970, RpSplineFindPosition 0x555ff0, RpSplineFindMatrix 0x556360,
//   RpHAnimKeyFrameInterpolate 0x543100, RtAnimInterpolatorSetCurrentAnim 0x53e3c0, RtAnimInterpolatorSubAnimTime 0x53e4d0,
//   RtAnimInterpolatorAddAnimTime 0x53e640.
// Objects in place (RW 3.7 layouts): RpSpline {+8 numCtrlPoints, +0xc type (1 open, 2 closed), +0x38 RwV3d vPts[]}; RwMatrix {right, flags,
// up, at, pos}; RpHAnimKeyFrame {prev, time, RtQuat q (imag, real), RwV3d t}; RtAnimAnimation {interpInfo, numFrames, flags, duration,
// pFrames, customData}; RtAnimInterpolatorInfo {+4 interpKeyFrameSize, +8 animKeyFrameSize, +0xc apply, blend, interpolate, add};
// RtAnimInterpolator {anim, currentTime, pNextFrame, animCB, animCBData, animCBTime, loopCB, loopCBData, +0x24 interpKeyFrameSize,
// +0x28 animKeyFrameSize, +0x2c numNodes, ..., +0x3c..+0x48 the scheme's four callbacks, +0x4c the interpolated frames}.
// Exe-layout adapter (thrown away at the librw flip): mflrw_splanim_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over
// &RwEngineInstance 0x7d2128 (memory functions +0x108 / +0x10c), the spline plugin's table block 0x7da7bc (rapGTable[11], then the 12
// basis tables position0..3, velocity0..3, acceleration0..3 that SplineOpen 0x556bf0 builds) and RwErrorSet 0x4ea160.
// Callees of the same DLL: _rwSqrt / _rwInvSqrt / RwV3dNormalize (g0), RwMatrixOptimize (g1).
// Check: tools/delink/rw_splanim_check.sh (the original code and these exports natively under Wine on the same random inputs, outputs,
// callback logs and error reports compared word for word).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint32_t u32;

// field access with may_alias types: the same words are read as float and written as integers (the original's integer moves), which
// strict aliasing at -O2 would otherwise let the compiler reorder
typedef u8 *__attribute__((may_alias)) PA;
typedef float __attribute__((may_alias)) FA;
typedef int32_t __attribute__((may_alias)) IA;
typedef u32 __attribute__((may_alias)) UA;
#define P(p, off)  (*(PA *)((u8 *)(p) + (off)))
#define F(p, off)  (*(FA *)((u8 *)(p) + (off)))
#define I(p, off)  (*(IA *)((u8 *)(p) + (off)))
#define U(p, off)  (*(UA *)((u8 *)(p) + (off)))

typedef void *(__cdecl *MallocFn)(u32 size, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *ErrorSetFn)(const int32_t *err);
typedef void *(__cdecl *KfInterpFn)(void *out, void *in1, void *in2, float time, void *data);
typedef void *(__cdecl *InterpCBFn)(void *interp, void *data);

// ---- adapter
static u8 **g_engine;		// &RwEngineInstance 0x7d2128
static float **g_spl;		// 0x7da7bc: rapGTable[0..10], then [11..14] position0..3, [15..18] velocity0..3, [19..22] acceleration0..3
static ErrorSetFn g_errorSet;	// RwErrorSet 0x4ea160
static void unbound(void){ fprintf(stderr, "[mflrw] splanim called before mflrw_splanim_bind\n"); abort(); }
static inline void *rwMalloc(u32 n, u32 hint){ if(!g_engine) unbound(); return (*(MallocFn *)(*g_engine + 0x108))(n, hint); }
static inline void rwFree(void *p){ if(!g_engine) unbound(); (*(FreeFn *)(*g_engine + 0x10c))(p); }
// RWERROR((code)) as compiled: _rwerror 0x53eac0 returns its first argument, RwErrorSet gets {pluginID 0x102, code}
static inline void splError(int32_t code){ int32_t e[2] = {0x102, code}; if(!g_errorSet) unbound(); g_errorSet(e); }
static inline float *tab(int k){ if(!g_spl) unbound(); return g_spl[k]; }

static inline float CF(u32 b){ float f; memcpy(&f, &b, 4); return f; }
static inline u32 FB(float f){ u32 b; memcpy(&b, &f, 4); return b; }
static inline void cp3(void *d, const void *s){ memcpy(d, s, 12); }		// the original copies vectors with integer moves
// fistp at the current rounding mode (nearest at CW 0x7f)
static inline int32_t fistpN(X v){ int32_t r; __asm__ volatile("fistpl %0" : "=m"(r) : "t"(v) : "st"); return r; }

extern "C" {

void __cdecl mflrw_splanim_bind(u8 **engineVar, float **splineTables, void *errorSet){
	g_engine = engineVar; g_spl = splineTables; g_errorSet = (ErrorSetFn)errorSet;
}

// _rwV3dNormalize 0x4d51f0: out = in * invSqrt(|in|^2), returns the reciprocal length (0 for a zero vector)
float __cdecl rw37__rwV3dNormalize(float *out, const float *in){
	X x = in[0], y = in[1], z = in[2];
	X s = x*x + y*y; s = s + z*z;
	float r = _rwInvSqrt((float)s);
	out[0] = (X)r*in[0];
	out[1] = (X)in[1]*r;
	out[2] = (X)in[2]*r;
	return r;
}

} // extern "C"

// ---- spline: the "where" preprocessing shared by FindPosition / FindMatrix (abs, closed-loop wrap, path).  0 = invalid path.
static inline int splWhere(u8 *spline, int32_t path, float &where, X &w){
	w = where;
	if(!(where >= 0.0f)){ w = -w; where = (float)w; }
	if(I(spline, 0xc) == 2 && w > 1.0f){
		u32 n = (u32)fistpN(where);			// RwFastRealToUInt32: fistp, then fild of the zero-extended qword
		w = (X)where - (X)(int64_t)n;
	}
	if(path == 10) return 1;
	if(path != 11) return 0;
	if(!(w > 0.5f || w != w)) w = (w + w)*w;		// test ah,0x41 / jp: <= 0.5
	else{ X a = w*w*(X)-2.0f; w = a + w*(X)4.0f; w = w - 1.0f; }
	return 1;
}
// index nJ (control point), nI (table row) and the subsample; the general case keeps the subsample on the x87 stack (FindPosition) or
// stores it (FindMatrix): both get the X value here, FindMatrix rounds it
static inline void splParam(u8 *spline, X w, int32_t &nJ, int32_t &nI, X &sub){
	int32_t n = I(spline, 8);
	if(w == 1.0f){ nJ = n - 4; nI = 0xff; sub = 1.0f; return; }
	float p = (X)(n - 3)*w;
	nJ = (int32_t)p;
	float q = ((X)p - (X)nJ)*(X)256.0f;
	nI = (int32_t)q;
	sub = (X)q - (X)nI;
}
static inline X lerpT(int k, int32_t i, X s){ const float *t = tab(k); return ((X)t[i + 1] - t[i])*s + t[i]; }

extern "C" {

// RpSplineFindPosition 0x555ff0
float *__cdecl rw37_RpSplineFindPosition(u8 *spline, int32_t path, float where, float *pos, float *tangent){
	X w;
	if(!splWhere(spline, path, where, w)){ splError(1); return NULL; }
	int32_t nJ, nI; X s;
	splParam(spline, w, nJ, nI, s);
	const float *v0 = (float *)(spline + 0x38) + 3*nJ, *v1 = v0 + 3, *v2 = v0 + 6, *v3 = v0 + 9;
	X w3 = lerpT(14, nI, s), w2 = lerpT(13, nI, s);
	float w1 = lerpT(12, nI, s), w0 = lerpT(11, nI, s);
	pos[0] = w3*v0[0]; pos[1] = (X)v0[1]*w3; pos[2] = (X)v0[2]*w3;
	pos[0] = w2*v1[0] + pos[0]; pos[1] = w2*v1[1] + pos[1]; pos[2] = w2*v1[2] + pos[2];
	pos[0] = (X)w1*v2[0] + pos[0]; pos[1] = (X)w1*v2[1] + pos[1]; pos[2] = (X)w1*v2[2] + pos[2];
	pos[0] = (X)w0*v3[0] + pos[0]; pos[1] = (X)w0*v3[1] + pos[1]; pos[2] = (X)w0*v3[2] + pos[2];
	if(!tangent) return pos;
	float t3 = lerpT(18, nI, s), t2 = lerpT(17, nI, s), t1 = lerpT(16, nI, s);
	X t0 = lerpT(15, nI, s);
	tangent[0] = (X)t3*v0[0]; tangent[1] = (X)v0[1]*t3; tangent[2] = (X)v0[2]*t3;
	tangent[0] = (X)t2*v1[0] + tangent[0]; tangent[1] = (X)t2*v1[1] + tangent[1]; tangent[2] = (X)t2*v1[2] + tangent[2];
	tangent[0] = (X)t1*v2[0] + tangent[0]; tangent[1] = (X)t1*v2[1] + tangent[1]; tangent[2] = (X)t1*v2[2] + tangent[2];
	tangent[0] = t0*v3[0] + tangent[0]; tangent[1] = t0*v3[1] + tangent[1]; tangent[2] = t0*v3[2] + tangent[2];
	if(rw37__rwV3dNormalize(tangent, tangent) == 0.0f){ U(tangent, 0) = 0; U(tangent, 4) = 0x3f800000; U(tangent, 8) = 0; }
	return pos;
}

// RpSplineFindMatrix 0x556360: Frenet frame; returns the length of at x acceleration (the original's "curvature"), -1 on a bad path
float __cdecl rw37_RpSplineFindMatrix(u8 *spline, int32_t path, float where, float *up, float *m){
	X w;
	if(!splWhere(spline, path, where, w)){ splError(1); return -1.0f; }
	int32_t nJ, nI; X sx;
	splParam(spline, w, nJ, nI, sx);
	float s = sx;
	const float *v0 = (float *)(spline + 0x38) + 3*nJ, *v1 = v0 + 3, *v2 = v0 + 6, *v3 = v0 + 9;
	RwMatrixOptimize((RwMatrix *)m, NULL);
	float *right = m, *mup = m + 4, *at = m + 8, *pos = m + 12;
	// position
	{	X w3 = lerpT(14, nI, s), w2 = lerpT(13, nI, s);
		float w1 = lerpT(12, nI, s), w0 = lerpT(11, nI, s);
		pos[0] = w3*v0[0]; pos[1] = (X)v0[1]*w3; pos[2] = (X)v0[2]*w3;
		pos[0] = w2*v1[0] + pos[0]; pos[1] = w2*v1[1] + pos[1]; pos[2] = w2*v1[2] + pos[2];
		pos[0] = (X)w1*v2[0] + pos[0]; pos[1] = (X)w1*v2[1] + pos[1]; pos[2] = (X)w1*v2[2] + pos[2];
		pos[0] = (X)w0*v3[0] + pos[0]; pos[1] = (X)w0*v3[1] + pos[1]; pos[2] = (X)w0*v3[2] + pos[2]; }
	// velocity (into at) and acceleration
	float vel[3], acc[3];
	{	X w3 = lerpT(18, nI, s), w2 = lerpT(17, nI, s);
		float w1 = lerpT(16, nI, s), w0 = lerpT(15, nI, s);
		X a = w3*v0[0], b = (X)v0[1]*w3;
		float cz = (X)v0[2]*w3;
		vel[0] = w2*v1[0] + a;
		vel[1] = w2*v1[1] + b;
		X vz = w2*v1[2] + cz;
		X xx = (X)w1*v2[0] + vel[0];
		vel[1] = (X)w1*v2[1] + vel[1];
		vel[2] = (X)w1*v2[2] + vz;
		vel[0] = (X)w0*v3[0] + xx;
		vel[1] = (X)w0*v3[1] + vel[1];
		vel[2] = (X)w0*v3[2] + vel[2]; }
	{	X w3 = lerpT(22, nI, s), w2 = lerpT(21, nI, s);
		float w1 = lerpT(20, nI, s), w0 = lerpT(19, nI, s);
		X a = w3*v0[0], b = (X)v0[1]*w3;
		acc[2] = (X)v0[2]*w3;
		acc[0] = w2*v1[0] + a;
		acc[1] = w2*v1[1] + b;
		X az = w2*v1[2] + acc[2];
		X xx = (X)w1*v2[0] + acc[0];
		acc[1] = (X)w1*v2[1] + acc[1];
		acc[2] = (X)w1*v2[2] + az;
		acc[0] = (X)w0*v3[0] + xx;
		acc[1] = (X)w0*v3[1] + acc[1];
		acc[2] = (X)w0*v3[2] + acc[2]; }
	cp3(at, vel);
	if(rw37__rwV3dNormalize(at, at) == 0.0f){ U(at, 0) = 0; U(at, 4) = 0; U(at, 8) = 0x3f800000; }
	float bn[3];
	bn[0] = (X)at[2]*acc[1] - (X)at[1]*acc[2];
	bn[1] = (X)acc[2]*at[0] - (X)at[2]*acc[0];
	bn[2] = (X)at[1]*acc[0] - (X)acc[1]*at[0];
	float len = RwV3dNormalize((RwV3d *)bn, (RwV3d *)bn);
	if(up){
		cp3(mup, up);
		if(rw37__rwV3dNormalize(mup, mup) == 0.0f) U(mup, 4) = 0x3f800000;
	}else{
		cp3(mup, bn);
		if(len == 0.0f) U(mup, 4) = 0x3f800000;
	}
	right[0] = (X)mup[1]*at[2] - (X)at[1]*mup[2];
	right[1] = (X)mup[2]*at[0] - (X)at[2]*mup[0];
	right[2] = (X)at[1]*mup[0] - (X)mup[1]*at[0];
	if(rw37__rwV3dNormalize(right, right) == 0.0f) U(right, 0) = 0x3f800000;
	if(up){
		mup[0] = (X)at[1]*right[2] - (X)at[2]*right[1];
		mup[1] = (X)at[2]*right[0] - (X)right[2]*at[0];
		mup[2] = (X)right[1]*at[0] - (X)at[1]*right[0];
		if(rw37__rwV3dNormalize(mup, mup) == 0.0f) U(mup, 4) = 0x3f800000;
	}
	return len;
}

// _splCreateSpline 0x555970: the phantom control points vPts from the points (open: rapGTable coefficient rows; closed: the series
// filter).  RpSplineGetNumControlPoints is inlined (with its own error report for a bad type).
u8 *__cdecl rw37__splCreateSpline(u8 *spline, const float *pts){
	int32_t type = I(spline, 0xc), nNum;
	if(type == 1) nNum = I(spline, 8) - 2;
	else if(type == 2) nNum = I(spline, 8) - 3;
	else { splError(1); nNum = 0; }
	float *vp = (float *)(spline + 0x38);
	if(type == 1){
		int32_t nSig = nNum < 10 ? nNum : 10;
		const float *co = tab(nSig);
		if(!co){ splError(0x80000016); return NULL; }
		int32_t half = nSig/2;
		for(int32_t i = 0; i < nNum; i++){
			X x = 0.0f, y = 0.0f, z = 0.0f;
			const float *row, *p;
			if(i < half){ row = co + i*nSig; p = pts; }
			else if(i < nNum - half){ row = co + half*nSig; p = pts + 3*(i - half); }
			else { row = co + (nSig - nNum + i)*nSig; p = pts + 3*(nNum - nSig); }
			for(int32_t j = 0; j < nSig; j++){
				x = x + (X)row[j]*p[3*j];
				y = y + (X)row[j]*p[3*j + 1];
				z = z + (X)row[j]*p[3*j + 2];
			}
			float r[3] = {(float)x, (float)y, (float)z};
			cp3(vp + 3*(i + 1), r);
		}
		cp3(vp, vp + 6);
		cp3(vp + 3*(nNum + 1), vp + 3*(nNum - 1));
		return spline;
	}
	if(type != 2){ splError(1); return NULL; }
	int32_t nSig = nNum < 10 ? nNum : 10, half = nSig/2, len;
	int32_t *s;
	float recip;
	if(nSig % 2){
		len = half + 1;
		s = (int32_t *)rwMalloc(len*4, 0x30102);
		if(!s){ splError(0x80000013); return NULL; }
		s[0] = 1; s[1] = 3;
		for(int32_t k = 2; k < len; k++) s[k] = 4*s[k - 1] - s[k - 2];
		recip = (X)1.0f/(X)s[half];
		rwFree(s);
		s = (int32_t *)rwMalloc(len*4, 0x30102);
		if(!s){ splError(0x80000013); return NULL; }
		s[0] = 1; s[1] = 5;
		for(int32_t k = 2; k < len; k++) s[k] = 4*s[k - 1] - s[k - 2];
	}else{
		s = (int32_t *)rwMalloc(half*4, 0x30102);
		if(!s){ splError(0x80000013); return NULL; }
		s[0] = 1; s[1] = 4;
		for(int32_t k = 2; k < half; k++) s[k] = 4*s[k - 1] - s[k - 2];
		recip = (X)1.0f/(X)s[half - 1];
		rwFree(s);
		len = half + 1;
		s = (int32_t *)rwMalloc(len*4, 0x30102);
		if(!s){ splError(0x80000013); return NULL; }
		s[0] = 1; s[1] = 2;
		for(int32_t k = 2; k < len; k++) s[k] = 4*s[k - 1] - s[k - 2];
	}
	for(int32_t k = half - 1; k >= 0; k -= 2) s[k] = -s[k];
	int32_t h2 = (nSig + 1)/2;
	for(int32_t i = 0; i < nNum; i++){
		float sh = (X)s[half];
		const float *p = pts + 3*(i % nNum);
		X x = (X)p[0]*sh, y = (X)p[1]*sh, z = (X)p[2]*sh;
		float zf = z;
		for(int32_t j = 1; j < len; j++){
			X c = (X)s[half - j]; const float *q = pts + 3*((j + i) % nNum);
			x = x + c*q[0]; y = y + c*q[1]; z = z + c*q[2];
		}
		zf = z;
		for(int32_t j = 1; j < h2; j++){
			X c = (X)s[half - j]; const float *q = pts + 3*((nNum + i - j) % nNum);
			x = x + c*q[0]; y = y + c*q[1]; z = z + c*q[2];
		}
		if(h2 > 1) zf = z;
		float *d = vp + 3*(i + 1);
		d[0] = (X)recip*x;
		d[1] = (X)recip*y;
		d[2] = (X)recip*zf;
	}
	cp3(vp, vp + 3*nNum);
	cp3(vp + 3*(nNum + 1), vp + 3);
	cp3(vp + 3*(nNum + 2), vp + 6);
	rwFree(s);
	return spline;
}

} // extern "C"

// ---- HAnim: float acos (fdlibm's acosf as compiled, with the 12 bit _rwSqrt) and the sin polynomial, both inlined in the interpolator
static const float AC_PS0 = CF(0x3e2aaaab), AC_PS1 = CF(0x3ea6b090), AC_PS2 = CF(0x3e4e0aa8), AC_PS3 = CF(0x3d241146), AC_PS4 = CF(0x3a4f7f04),
	AC_PS5 = CF(0x3811ef08), AC_QS1 = CF(0x4019d139), AC_QS2 = CF(0x4001572d), AC_QS3 = CF(0x3f303361), AC_QS4 = CF(0x3d9dc62e),
	PIO2_LO = CF(0x33a22168), PIO2_HI = CF(0x3fc90fda), PI_HI = CF(0x40490fda), PI_F = CF(0x40490fdb), PIO2_F = CF(0x3fc90fdb),
	S1 = CF(0x3e2aaaab), S2 = CF(0x3c088889), S3 = CF(0x39500d01), S4 = CF(0x3638ef1b), S5 = CF(0x32d72f34), S6 = CF(0x2f2ec9d3);
template<class T> static inline X acP(T z){ X p = (X)AC_PS5*z; p = p + AC_PS4; p = p*z; p = p - AC_PS3; p = p*z; p = p + AC_PS2; p = p*z; p = p - AC_PS1; p = p*z; p = p + AC_PS0; return p*z; }
template<class T> static inline X acQ(T z){ X q = (X)z*AC_QS4; q = q - AC_QS3; q = q*z; q = q + AC_QS2; q = q*z; q = q - AC_QS1; q = q*z; return q + 1.0f; }
static inline X sinP(X t){ X z = t*t; X p = (X)S6*z; p = p - S5; p = p*z; p = p + S4; p = p*z; p = p - S3; p = p*z; p = p + S2; p = p*z; p = p - S1; return p*(z*t) + t; }

extern "C" {

// RpHAnimKeyFrameInterpolate 0x543100: slerp (acos / sin polynomials) above dot 0.999, else lerp; negates in2's quaternion in place when
// the dot product is negative (as the original does)
void __cdecl rw37_RpHAnimKeyFrameInterpolate(float *out, float *in1, float *in2, float time, void *data){
	(void)data;
	X dot = (X)in2[2]*in1[2] + (X)in1[3]*in2[3]; dot = dot + (X)in2[4]*in1[4]; dot = dot + (X)in1[5]*in2[5];
	float f = ((X)time - in1[1])/((X)in2[1] - in1[1]);
	out[6] = ((X)in2[6] - in1[6])*f + in1[6];
	out[7] = ((X)in2[7] - in1[7])*f + in1[7];
	out[8] = ((X)in2[8] - in1[8])*f + in1[8];
	if(dot < 0.0f){ dot = -dot; in2[2] = -in2[2]; in2[3] = -in2[3]; in2[4] = -in2[4]; in2[5] = -in2[5]; }
	float g = (X)1.0f - f;
	if(!(dot >= CF(0x3f7fbe77))){		// C0: dot < 0.999 or NaN -> slerp
		float df = dot; u32 b = FB(df), ab = b & 0x7fffffff;
		X th;
		if((int32_t)ab >= 0x3f800000) th = (int32_t)b > 0 ? 0.0f : PI_F;
		else if((int32_t)ab < 0x3f000000){
			if((int32_t)ab <= 0x23000000) th = PIO2_F;
			else{ X x = dot, z = x*x; X r = acP(z)/acQ(z); th = (X)PIO2_HI - (x - ((X)PIO2_LO - r*x)); }
		}else if((int32_t)b < 0){
			float zf = (dot + (X)1.0f)*(X)0.5f;
			X sq = _rwSqrt(zf);
			X r = acP(zf)/acQ(zf);
			X w = r*sq - PIO2_LO;
			X t = sq + w; t = t + t;
			th = (X)PI_HI - t;
		}else{
			float zf = ((X)1.0f - dot)*(X)0.5f;
			X sq = _rwSqrt(zf);
			float sf = sq; float dfl = CF(FB(sf) & 0xfffff000);
			X r = acP(zf)/acQ(zf);
			X w = r*sq;
			X c = ((X)zf - (X)dfl*dfl)/((X)dfl + sq);
			X t = w + c; t = t + dfl; th = t + t;
		}
		X inv = (X)1.0f/sinP(th);
		g = sinP(th*g)*inv;
		f = sinP(th*f)*inv;
	}
	out[2] = (X)f*in2[2] + (X)g*in1[2];
	out[3] = (X)g*in1[3] + (X)f*in2[3];
	out[4] = (X)f*in2[4] + (X)g*in1[4];
	out[5] = (X)g*in1[5] + (X)f*in2[5];
}

// ---- RtAnim interpolator clock
// RtAnimInterpolatorSetCurrentAnim 0x53e3c0
int32_t __cdecl rw37_RtAnimInterpolatorSetCurrentAnim(u8 *ip, u8 *anim){
	P(ip, 0) = anim; U(ip, 4) = 0;
	u8 *info = P(anim, 0);
	U(ip, 0x24) = U(info, 4); U(ip, 0x28) = U(info, 8);
	U(ip, 0x3c) = U(info, 0xc); U(ip, 0x40) = U(info, 0x10); U(ip, 0x44) = U(info, 0x14); U(ip, 0x48) = U(info, 0x18);
	for(int32_t i = 0; i < I(ip, 0x2c); i++){
		int32_t ks = I(ip, 0x28); u8 *fr = P(anim, 0x10);
		(*(KfInterpFn *)(ip + 0x44))(ip + 0x4c + I(ip, 0x24)*i, fr + ks*i, fr + ks*(I(ip, 0x2c) + i), 0.0f, P(anim, 0x14));
	}
	int32_t n = I(ip, 0x2c);
	u8 *k1 = P(anim, 0x10), *k2 = k1 + I(ip, 0x28)*n, *f = ip + 0x4c;
	for(int32_t i = 0; i < n; i++){
		P(f, 0) = k1; P(f, 4) = k2;
		f += I(ip, 0x24); k1 += I(ip, 0x28); k2 += I(ip, 0x28);
		n = I(ip, 0x2c);
	}
	P(ip, 8) = P(anim, 0x10) + I(ip, 0x2c)*I(ip, 0x28)*2;
	return 1;
}

// RtAnimInterpolatorAddAnimTime 0x53e640
int32_t __cdecl rw37_RtAnimInterpolatorAddAnimTime(u8 *ip, float time){
	if(!(time > 0.0f)) return 1;
	u8 *anim = P(ip, 0);
	float cb = F(ip, 0x14), dur = F(anim, 0xc);
	int32_t nodes = I(ip, 0x2c), looped = 0, fire;
	if(F(ip, 4) < cb) fire = !((X)time + F(ip, 4) < cb || (X)time + F(ip, 4) != (X)time + F(ip, 4));
	else fire = 0;
	X t = (X)time + F(ip, 4);
	F(ip, 4) = t;
	if(t > dur){
		looped = 1;
		do{
			t = t - dur;
			fire |= !(t < cb || t != t);
		}while(t > dur);
		F(ip, 4) = t;
		rw37_RtAnimInterpolatorSetCurrentAnim(ip, anim);
		F(ip, 4) = t;
	}
	if(!P(ip, 8)){
		X c = F(ip, 4);
		rw37_RtAnimInterpolatorSetCurrentAnim(ip, anim);
		F(ip, 4) = c;
	}
	u8 *end = P(anim, 0x10) + I(anim, 4)*I(ip, 0x28);
	int32_t node = 0;
	while(P(ip, 8) < end){
		u8 *prev = P(P(ip, 8), 0);
		if(F(prev, 4) > F(ip, 4)) break;
		u8 *f;
		do{
			f = ip + 0x4c + I(ip, 0x24)*node;
			if(++node >= nodes) node = 0;
		}while(P(f, 4) != prev);
		P(f, 0) = P(f, 4);
		P(f, 4) = P(ip, 8);
		P(ip, 8) = P(ip, 8) + I(ip, 0x28);
	}
	u8 *f = ip + 0x4c;
	for(int32_t i = nodes; i > 0; i--){
		u8 *a = P(ip, 0);
		(*(KfInterpFn *)(ip + 0x44))(f, P(f, 0), P(f, 4), F(ip, 4), P(a, 0x14));
		f += I(ip, 0x24);
	}
	if(fire && P(ip, 0xc)){
		if(!(*(InterpCBFn *)(ip + 0xc))(ip, P(ip, 0x10))) P(ip, 0xc) = NULL;
	}
	if(looped && P(ip, 0x18)){
		if(!(*(InterpCBFn *)(ip + 0x18))(ip, P(ip, 0x1c))) P(ip, 0x18) = NULL;
	}
	return 1;
}

// RtAnimInterpolatorSubAnimTime 0x53e4d0
int32_t __cdecl rw37_RtAnimInterpolatorSubAnimTime(u8 *ip, float time){
	int32_t looped = 0, fire;
	if(F(ip, 4) > F(ip, 0x14)){
		X d = (X)F(ip, 4) - time;
		fire = !(d > F(ip, 0x14) || d != d);
	}else fire = 0;
	u8 *anim = P(ip, 0);
	X t = (X)F(ip, 4) - time;
	F(ip, 4) = t;
	if(t < 0.0f){
		float dur = F(anim, 0xc);
		X c = F(ip, 4);
		do c = c + dur; while(c < 0.0f);
		float tf = c;
		F(ip, 4) = c;
		rw37_RtAnimInterpolatorSetCurrentAnim(ip, anim);
		rw37_RtAnimInterpolatorAddAnimTime(ip, tf);
		fire |= F(ip, 4) < F(ip, 0x14);
		looped = 1;
	}else{
		u8 *f = ip + 0x4c;
		for(int32_t i = I(ip, 0x2c); i != 0; i--){
			if(F(ip, 4) < F(P(f, 0), 4)){
				do{
					u8 *k = P(f, 0);
					P(f, 4) = k;
					P(f, 0) = P(k, 0);
				}while(F(ip, 4) < F(P(f, 0), 4));
			}
			(*(KfInterpFn *)(ip + 0x44))(f, P(f, 0), P(f, 4), F(ip, 4), P(P(ip, 0), 0x14));
			f += I(ip, 0x24);
		}
		P(ip, 8) = NULL;
	}
	if(fire && P(ip, 0xc)){
		if(!(*(InterpCBFn *)(ip + 0xc))(ip, P(ip, 0x10))) P(ip, 0xc) = NULL;
	}
	if(looped && P(ip, 0x18)){
		if(!(*(InterpCBFn *)(ip + 0x18))(ip, P(ip, 0x1c))) P(ip, 0x18) = NULL;
	}
	return 1;
}

} // extern "C"
