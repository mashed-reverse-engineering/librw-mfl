// G1 (plan_librw_d3dx S4): more stateless RenderWare 3.7 functions, written op for op from the disassembly of MFL.exe (address in the
// comment) so that results are bit-identical at the game's x87 precision (CW 0x7f = 24 bit mantissa): every float operation below
// is one x87 operation of the original in the same order, and loads / stores through the pointer arguments happen in the original's
// order (results match even for aliasing pointers).  fsin/fcos/fpatan ignore the precision control, so they are inline asm together
// with the original's next rounding step.  Constants are given by their bits in MFL.exe.  Check: tools/delink/rw_g1_check.sh.
// Engine state the originals read and the game never changes after start-up is constant here: identity-flag mask 0x20000, matrix
// multiply = the x87 routine 0x5dc2a0 (installed by 0x4d85f3 on every CPU), RwMatrixOptimize default tolerances 0.01.
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

// X = a value the original keeps on the x87 stack (precision control already rounds it to 24 bits, but its exponent range is not
// clipped to float); float = a value the original stores to float memory.  (g++ -std=c++11 rounds every assignment to a float.)
typedef long double X;
#define MF(m) ((float *)(m))
#define CMF(m) ((const float *)(m))
static inline uint32_t FB(const volatile float *p){ float f = *p; uint32_t b; memcpy(&b, &f, 4); return b; }
static inline float BF(uint32_t b){ float f; memcpy(&f, &b, 4); return f; }
// whole words (the original moves them as integers); memcpy keeps their order against float accesses of the same words
static inline void W4(void *d, int di, const void *s, int si){ memcpy((char *)d + 4*di, (const char *)s + 4*si, 4); }
static inline uint32_t GETF(const void *m){ uint32_t f; memcpy(&f, (const char *)m + 12, 4); return f; }
static inline void SETF(void *m, uint32_t f){ memcpy((char *)m + 12, &f, 4); }
static inline void copy64(void *d, const void *s){ memmove(d, s, 64); }
static const uint32_t IDENT = 0x20000;

// matrix product 0x5dc2a0: o = a*b row by row (the a row is loaded first), the pos row adds b's pos
static void mult(float *o, const float *a, const float *b){
	for(int r = 0; r < 4; r++){
		float x = a[4*r], y = a[4*r+1], z = a[4*r+2]; X t;
		t = b[0]*x + b[4]*y; t = t + b[8]*z; if(r == 3) t = t + b[12]; o[4*r] = t;
		t = b[1]*x + b[5]*y; t = t + b[9]*z; if(r == 3) t = t + b[13]; o[4*r+1] = t;
		t = x*b[2] + y*b[6]; t = t + z*b[10]; if(r == 3) t = t + b[14]; o[4*r+2] = t;
	}
}
// the rotate / transform family multiplies into a frame local and copies it over m.
// ponytail: the original's local has uninitialised pad words (0x1c/0x2c/0x3c) that land in m; here m keeps its own pads.
static void combineInto(float *m, const float *a, const float *b, uint32_t flags){
	float t[16]; mult(t, a, b);
	for(int k = 0; k < 15; k++) if((k & 3) != 3) m[k] = t[k];
	SETF(m, flags);
}
static inline void copyRows(float *m, const float *s){ for(int k = 0; k < 15; k++) if((k & 3) != 3) m[k] = s[k]; }

extern "C" {

// L1:exact 0x4d38f0
RWAPI RwReal _rwMatrixDeterminant(const RwMatrix *mm){
	const float *m = CMF(mm);
	X c0 = m[10]*m[5] - m[9]*m[6], c1 = m[6]*m[8] - m[10]*m[4], c2 = m[9]*m[4] - m[5]*m[8];
	X s = m[1]*c1 + c0*m[0];
	return s + m[2]*c2;
}
// L1:exact 0x4d3940
RWAPI RwReal _rwMatrixOrthogonalError(const RwMatrix *mm){
	const float *m = CMF(mm);
	X d1 = m[9]*m[5] + m[8]*m[4]; d1 = d1 + m[10]*m[6];
	X d2 = m[9]*m[1] + m[8]*m[0]; d2 = d2 + m[10]*m[2];
	X d3 = m[5]*m[1] + m[4]*m[0]; d3 = d3 + m[6]*m[2];
	X s = d2*d2 + d1*d1;
	return s + d3*d3;
}
static inline X sqlen1(float x, float y, float z){ X s = y*y + x*x; s = s + z*z; return s - 1.0f; }
// L1:exact 0x4d39a0
RWAPI RwReal _rwMatrixNormalError(const RwMatrix *mm){
	const float *m = CMF(mm);
	X e1 = sqlen1(m[0], m[1], m[2]), e2 = sqlen1(m[4], m[5], m[6]), e3 = sqlen1(m[8], m[9], m[10]);
	X s = e2*e2 + e1*e1;
	return s + e3*e3;
}
// L1:exact 0x4d3a30
RWAPI RwReal _rwMatrixIdentityError(const RwMatrix *mm){
	const float *m = CMF(mm);
	X a = m[0] - 1.0f, b = m[5] - 1.0f, c = m[10] - 1.0f;
	X s = m[1]*m[1] + m[2]*m[2]; s = s + a*a;
	X t = m[12]*m[12] + m[13]*m[13]; t = t + m[14]*m[14]; s = s + t;
	t = m[8]*m[8] + m[9]*m[9]; t = t + c*c; s = s + t;
	t = m[4]*m[4] + m[6]*m[6]; t = t + b*b;
	return s + t;
}
// L1:exact 0x4d3c00; tolerance NULL = the engine default set by the matrix module open 0x4d3b40 (the game never calls RwMatrixSetTolerance)
RWAPI RwMatrix *RwMatrixOptimize(RwMatrix *m, const RwMatrixTolerance *tolerance){
	const float deftol[3] = {BF(0x3c23d70a), BF(0x3c23d70a), BF(0x3c23d70a)};
	const float *tol = tolerance ? (const float *)tolerance : deftol;
	int nrm = _rwMatrixNormalError(m) <= tol[0];
	int orth = _rwMatrixOrthogonalError(m) <= tol[1];
	int id = nrm && orth && _rwMatrixIdentityError(m) <= tol[2];
	uint32_t f = GETF(m);
	f = nrm ? f | 1 : f & ~1u;
	f = orth ? f | 2 : f & ~2u;
	SETF(m, id ? f | IDENT : f & ~IDENT);
	return m;
}
// L1:exact 0x4d3cd0
RWAPI RwMatrix *RwMatrixMultiply(RwMatrix *out, const RwMatrix *a, const RwMatrix *b){
	uint32_t fa = GETF(a), fb = GETF(b);
	if(fa & IDENT){ copy64(out, b); return out; }
	if(fb & IDENT){ copy64(out, a); return out; }
	mult(MF(out), CMF(a), CMF(b));
	SETF(out, fb & fa);
	return out;
}
// L1:exact 0x4d3d50 (via the jmp at 0x4d3d40)
RWAPI RwMatrix *RwMatrixOrthoNormalize(RwMatrix *out, const RwMatrix *in){
	const float *m = CMF(in);
	float r[3] = {m[0], m[1], m[2]}, u[3] = {m[4], m[5], m[6]}, a[3] = {m[8], m[9], m[10]};
	uint32_t pos[3]; memcpy(pos, m + 12, 12);
	X n = r[1]*r[1] + r[0]*r[0]; n = n + r[2]*r[2];
	volatile float ir = _rwInvSqrt(n);
	r[0] = ir*r[0]; r[1] = ir*r[1]; r[2] = ir*r[2];
	n = u[1]*u[1] + u[0]*u[0]; n = n + u[2]*u[2];
	volatile float iu = _rwInvSqrt(n);
	u[0] = iu*u[0]; u[1] = iu*u[1]; u[2] = iu*u[2];
	n = a[1]*a[1] + a[0]*a[0]; n = n + a[2]*a[2];
	X ia = _rwInvSqrt(n);
	a[0] = a[0]*ia; a[1] = a[1]*ia; a[2] = a[2]*ia;
	float *B, *S, *D;		// D = S x B is rebuilt first, then S = B x D
	if(!(ir > 0.0f)){ B = u; S = a; D = r; }
	else if(!(iu > 0.0f)){ B = a; S = r; D = u; }
	else if(!(ia > 0.0f)){ B = r; S = u; D = a; }
	else{
		X d = (a[1]*u[1] + a[0]*u[0]) + a[2]*u[2]; float dau = d; if(d < 0.0f) dau = -dau;		// sign tested on the register, negated in memory
		d = (a[1]*r[1] + a[0]*r[0]) + a[2]*r[2]; float dar = d; if(d < 0.0f) dar = -dar;
		X dur = (u[1]*r[1] + u[0]*r[0]) + u[2]*r[2]; if(dur < 0.0f) dur = -dur;
		if(dau < dar){
			if(dau < dur){ B = u; S = a; D = r; }
			else{ B = r; S = u; D = a; }
		}else if(dar < dur){ B = a; S = r; D = u; }
		else{ B = r; S = u; D = a; }
	}
	D[0] = S[2]*B[1] - S[1]*B[2];
	D[1] = B[2]*S[0] - S[2]*B[0];
	D[2] = S[1]*B[0] - B[1]*S[0];
	n = D[0]*D[0] + D[1]*D[1]; n = n + D[2]*D[2];
	X i = _rwInvSqrt(n);
	D[0] = i*D[0]; D[1] = D[1]*i; D[2] = D[2]*i;
	S[0] = B[2]*D[1] - B[1]*D[2];
	S[1] = D[2]*B[0] - B[2]*D[0];
	S[2] = B[1]*D[0] - D[1]*B[0];
	n = S[0]*S[0] + S[1]*S[1]; n = n + S[2]*S[2];
	i = _rwInvSqrt(n);
	S[0] = i*S[0]; S[1] = S[1]*i; S[2] = S[2]*i;
	float *o = MF(out);
	o[0] = r[0]; o[1] = r[1]; o[2] = r[2]; o[4] = u[0]; o[5] = u[1]; o[6] = u[2]; o[8] = a[0]; o[9] = a[1]; o[10] = a[2];
	memcpy(o + 12, pos, 12);
	SETF(out, (GETF(out) & ~IDENT) | 3);
	return out;
}

// L1:exact 0x4d4120; an invalid combine op returns NULL (the original reports RwError 0x80000003 and returns NULL too)
RWAPI RwMatrix *RwMatrixRotateOneMinusCosineSine(RwMatrix *mat, const RwV3d *axis, RwReal oneMinusCos, RwReal sine, RwOpCombineType op){
	const float *v = (const float *)axis;
	X A = 1.0f - v[0]*v[0], Bv = 1.0f - v[1]*v[1]; float C = 1.0f - v[2]*v[2];
	float t24 = A*oneMinusCos; X Bo = Bv*oneMinusCos, Co = C*oneMinusCos;
	X yz = v[1]*v[2], zx = v[2]*v[0]; float xy = v[1]*v[0];
	float yzo = yz*oneMinusCos; X zxo = zx*oneMinusCos, xyo = xy*oneMinusCos;
	float sx = sine*v[0], sy = v[1]*sine; X sz = v[2]*sine;
	float R[16];
	R[0] = 1.0f - t24; R[1] = sz + xyo; R[2] = zxo - sy;
	R[4] = xyo - sz; R[5] = 1.0f - Bo; R[6] = sx + yzo;
	R[8] = sy + zxo; R[9] = yzo - sx; R[10] = 1.0f - Co;
	R[12] = R[13] = R[14] = 0.0f;
	float *m = MF(mat);
	switch(op){
	case 0: copyRows(m, R); SETF(m, 3); return mat;
	case 1: if(GETF(m) & IDENT){ copyRows(m, R); SETF(m, 3); } else combineInto(m, R, m, GETF(m) & 3); return mat;
	case 2: if(GETF(m) & IDENT){ copyRows(m, R); SETF(m, 3); } else combineInto(m, m, R, GETF(m) & 3); return mat;
	default: return NULL;
	}
}

static const float ONE = 1.0f;
static inline float x87sin(float x){ float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static inline float x87omcos(float x){ float r; __asm__("fcos\n\tfsubrs %2" : "=t"(r) : "0"(x), "m"(ONE)); return r; }	// 1 - cos x, one 24 bit rounding

// L1:exact 0x4d43f0 (angle in degrees)
RWAPI RwMatrix *RwMatrixRotate(RwMatrix *m, const RwV3d *axis, RwReal angle, RwOpCombineType op){
	volatile float rad = angle*BF(0x3c8efa35);
	const float *v = (const float *)axis;
	X n = v[0]*v[0] + v[1]*v[1]; n = n + v[2]*v[2];
	X i = _rwInvSqrt(n);
	RwV3d u; u.x = i*v[0]; u.y = v[1]*i; u.z = v[2]*i;
	volatile float s = x87sin(rad), omc = x87omcos(rad);
	RwMatrixRotateOneMinusCosineSine(m, &u, omc, s, op);
	return m;
}

// L1:exact 0x4d4580: general inverse (adjugate / determinant)
static void invertGeneral(float *o, const float *m){
	o[0] = m[10]*m[5] - m[9]*m[6];
	o[1] = -(m[10]*m[1] - m[9]*m[2]);
	X o2 = m[6]*m[1] - m[5]*m[2]; o[2] = o2;
	X det = m[4]*o[1] + m[8]*o2; det = det + o[0]*m[0];
	volatile float detf = det;
	X inv = 1.0f; if(FB(&detf)) inv = inv/detf;
	X t0 = inv*o[0]; o[0] = t0;
	volatile float t1 = inv*o[1]; o[1] = t1;
	o[2] = inv*o[2];
	o[4] = -((m[10]*m[4] - m[8]*m[6])*inv);
	o[5] = (m[10]*m[0] - m[8]*m[2])*inv;
	o[6] = -((m[6]*m[0] - m[4]*m[2])*inv);
	o[8] = (m[9]*m[4] - m[8]*m[5])*inv;
	o[9] = -((m[9]*m[0] - m[8]*m[1])*inv);
	o[10] = (m[5]*m[0] - m[4]*m[1])*inv;
	X s = o[8]*m[14] + m[13]*o[4]; s = s + m[12]*t0; o[12] = -s;
	s = o[9]*m[14] + m[13]*o[5]; s = s + m[12]*t1; o[13] = -s;
	s = m[13]*o[6] + m[12]*o[2]; s = s + o[10]*m[14];
	SETF(o, 0);
	o[14] = -s;
}
// L1:exact 0x4d4490
RWAPI RwMatrix *RwMatrixInvert(RwMatrix *out, const RwMatrix *in){
	const float *m = CMF(in); float *o = MF(out); uint32_t f = GETF(in);
	if(f & IDENT){ copy64(out, in); return out; }
	if((f & 3) == 3){
		W4(o, 0, m, 0); W4(o, 1, m, 4); W4(o, 2, m, 8); W4(o, 4, m, 1); W4(o, 5, m, 5); W4(o, 6, m, 9); W4(o, 8, m, 2); W4(o, 9, m, 6); W4(o, 10, m, 10);
		X s = m[13]*m[1] + m[12]*m[0]; s = s + m[2]*m[14]; o[12] = -s;
		s = m[12]*m[4] + m[13]*m[5]; s = s + m[14]*m[6]; o[13] = -s;
		s = m[8]*m[12] + m[13]*m[9]; s = s + m[10]*m[14];
		SETF(o, 3);
		o[14] = -s;
		return out;
	}
	invertGeneral(o, m);
	return out;
}
// L1:exact 0x4d49c0; an invalid op returns NULL like the original (after its RwError)
RWAPI RwMatrix *RwMatrixTransform(RwMatrix *mat, const RwMatrix *t, RwOpCombineType op){
	float *m = MF(mat); uint32_t fm = GETF(mat), ft = GETF(t);
	switch(op){
	case 0: copy64(mat, t); return mat;
	case 1:
		if(ft & IDENT) return mat;
		if(fm & IDENT){ copy64(mat, t); return mat; }
		combineInto(m, CMF(t), m, fm & ft); return mat;
	case 2:
		if(fm & IDENT){ copy64(mat, t); return mat; }
		if(ft & IDENT) return mat;
		combineInto(m, m, CMF(t), ft & fm); return mat;
	default: return NULL;
	}
}

static const float RAD2DEG = BF(0x42652ee1);
static inline float x87atan2deg(float y, float x){ float r; __asm__("fpatan\n\tfmuls %3" : "=t"(r) : "0"(x), "u"(y), "m"(RAD2DEG) : "st(1)"); return r; }

// L1:exact 0x4d4d50: axis of a (nearly) 180 degree rotation from the largest diagonal element
static void queryRotateDegenerate(const float *m, float *axis){
	float v[3];
	if(m[0] > m[5] && m[0] > m[10]){ X t = m[0] + 1.0f; v[0] = t + t; v[1] = m[4] + m[1]; v[2] = m[8] + m[2]; }
	else if(!(m[0] > m[5]) && m[5] > m[10]){ X t = m[5] + 1.0f; v[1] = t + t; v[2] = m[9] + m[6]; v[0] = m[4] + m[1]; }
	else{ X t = m[10] + 1.0f; v[2] = t + t; v[0] = m[8] + m[2]; v[1] = m[9] + m[6]; }
	X n = v[1]*v[1] + v[0]*v[0]; n = n + v[2]*v[2];
	X i = _rwInvSqrt(n);
	axis[0] = v[0]*i; axis[1] = v[1]*i; axis[2] = i*v[2];
}
// L1:exact 0x4d4b40
RWAPI const RwMatrix *RwMatrixQueryRotate(const RwMatrix *mat, RwV3d *unitAxis, RwReal *angle, RwV3d *center){
	const float *m = CMF(mat); float *ax = (float *)unitAxis, *c = (float *)center;
	volatile float tr1 = ((m[0] + m[5]) + m[10]) - 1.0f;
	RwV3d v; v.x = m[6] - m[9]; v.y = m[8] - m[2]; v.z = m[1] - m[4];
	float len = RwV3dLength(&v);
	if(!(len > 0.0f)){ ax[0] = 0.0f; ax[1] = 0.0f; ax[2] = 0.0f; }
	else{ X r = 1.0f/len; ax[0] = v.x*r; ax[1] = v.y*r; ax[2] = r*v.z; }
	*angle = x87atan2deg(len, tr1);
	if(len <= BF(0x3c23d70a) && tr1 <= 0.0f) queryRotateDegenerate(m, ax);
	float L[16], I[16];
	L[0] = 1.0f - m[0]; L[1] = -m[1]; L[2] = -m[2];
	L[4] = -m[4]; L[5] = 1.0f - m[5]; L[6] = -m[6];
	L[8] = -m[8]; L[9] = -m[9]; L[10] = 1.0f - m[10];
	L[12] = L[13] = L[14] = 0.0f;
	invertGeneral(I, L);
	c[0] = I[12]; c[1] = I[13]; c[2] = I[14];
	c[0] = m[12]*I[0] + c[0]; c[1] = m[12]*I[1] + c[1]; c[2] = m[12]*I[2] + c[2];
	c[0] = m[13]*I[4] + c[0]; c[1] = m[13]*I[5] + c[1]; c[2] = m[13]*I[6] + c[2];
	c[0] = m[14]*I[8] + c[0]; c[1] = m[14]*I[9] + c[1]; c[2] = m[14]*I[10] + c[2];
	return mat;
}

// ---- RtQuat (x, y, z, w)
// L1:exact 0x557220 / 0x557280 / 0x5572e0: trace <= 0, the largest diagonal element k picks the formula
static void quatFromDiag(float *q, const float *m, int k){
	X t;
	if(k == 0) t = m[0] - (m[10] + m[5]);
	else if(k == 1) t = m[5] - (m[10] + m[0]);
	else t = m[10] - (m[5] + m[0]);
	float s = _rwSqrt(t + 1.0f);
	q[k] = 0.5f*s; X h = 0.5f/s;
	if(k == 0){ q[3] = (m[6] - m[9])*h; q[1] = (m[4] + m[1])*h; q[2] = (m[8] + m[2])*h; }
	else if(k == 1){ q[3] = (m[8] - m[2])*h; q[2] = (m[9] + m[6])*h; q[0] = (m[4] + m[1])*h; }
	else{ q[3] = (m[1] - m[4])*h; q[0] = (m[8] + m[2])*h; q[1] = (m[9] + m[6])*h; }
}
// L1:exact 0x557140
RWAPI RwBool RtQuatConvertFromMatrix(RtQuat * const qpQuat, const RwMatrix * const mpMatrix){
	if(!qpQuat || !mpMatrix) return 0;
	float *q = (float *)qpQuat; const float *m = CMF(mpMatrix);
	X tr = m[10] + m[0]; tr = tr + m[5];
	if(tr > 0.0f){
		float s = _rwSqrt(tr + 1.0f);
		q[3] = 0.5f*s; X h = 0.5f/s;
		q[0] = (m[6] - m[9])*h; q[1] = (m[8] - m[2])*h; q[2] = (m[1] - m[4])*h;
		return 1;
	}
	if(m[0] > m[5]) quatFromDiag(q, m, m[0] > m[10] ? 0 : 2);
	else quatFromDiag(q, m, m[5] > m[10] ? 1 : 2);
	return 1;
}
// L1:exact 0x557340: v' = (w^2 - |q|^2) v + 2 (q.v) q + 2 w (q x v)
RWAPI RwV3d *RtQuatTransformVectors(RwV3d *vectorsOut, const RwV3d *vectorsIn, const RwInt32 numPoints, const RtQuat *quat){
	const float *q = (const float *)quat;
	X d = q[0]*q[0] + q[1]*q[1]; d = d + q[2]*q[2]; volatile float D = q[3]*q[3] - d;
	float X2 = q[0] + q[0], Y2 = q[1] + q[1], Z2 = q[2] + q[2];
	float WX2 = q[3]*X2, WY2 = q[3]*Y2, WZ2 = q[3]*Z2;
	const float *in = (const float *)vectorsIn; float *out = (float *)vectorsOut;
	for(RwInt32 i = 0; i < numPoints; i++, in += 3, out += 3){
		X s = in[1]*Y2 + X2*in[0]; s = s + in[2]*Z2;
		float t0 = in[2]*WY2 - in[1]*WZ2;
		float t1 = WZ2*in[0] - in[2]*WX2;
		X a = in[1]*WX2, b = WY2*in[0];
		out[0] = t0; out[1] = t1; out[2] = a - b;
		out[0] = s*q[0] + out[0];
		out[1] = q[1]*s + out[1];
		out[2] = q[2]*s + out[2];
		out[0] = D*in[0] + out[0];
		out[1] = in[1]*D + out[1];
		out[2] = in[2]*D + out[2];
	}
	return vectorsOut;
}
// acos numerator / denominator polynomials (fdlibm acosf coefficients, inlined in the original)
static inline X acosP(X z){
	X p = z*BF(0x3811ef08) + BF(0x3a4f7f04); p = p*z; p = p - BF(0x3d241146); p = p*z; p = p + BF(0x3e4e0aa8); p = p*z;
	p = p - BF(0x3ea6b090); p = p*z; p = p + BF(0x3e2aaaab); return p*z;
}
static inline X acosQ(X z){
	X q = z*BF(0x3d9dc62e); q = q - BF(0x3f303361); q = q*z; q = q + BF(0x4001572d); q = q*z; q = q - BF(0x4019d139); q = q*z; return q + 1.0f;
}
// L1:exact 0x5574a0: cache = {from, to (negated for the shorter arc), omega, use-lerp}
RWAPI void RtQuatSetupSlerpCache(RtQuat *qpFrom, RtQuat *qpTo, RtQuatSlerpCache *sCache){
	const float *f = (const float *)qpFrom, *to = (const float *)qpTo; float *c = (float *)sCache;
	W4(c, 0, f, 0); W4(c, 1, f, 1); W4(c, 2, f, 2); W4(c, 3, f, 3);
	X dot = f[1]*to[1] + f[0]*to[0]; dot = dot + f[2]*to[2]; dot = dot + f[3]*to[3];
	volatile float cs = dot;
	if(!(dot < 0.0f)){
		if(cs > 1.0f) cs = 1.0f;
		W4(c, 4, to, 0); W4(c, 5, to, 1); W4(c, 6, to, 2); W4(c, 7, to, 3);
	}else{
		if(!(cs < -1.0f)) cs = -cs; else cs = 1.0f;
		c[7] = -to[3]; c[4] = -to[0]; c[5] = -to[1]; c[6] = -to[2];
	}
	float x = cs; int32_t ix = (int32_t)FB(&cs), ax = ix & 0x7fffffff;
	uint32_t om;
	if(ax >= 0x3f800000) om = ix > 0 ? 0 : 0x40490fdb;
	else if(ax < 0x3f000000){
		if(ax <= 0x23000000) om = 0x3fc90fdb;
		else{
			X z = x*x;
			X r = acosP(z)/acosQ(z); r = r*x;
			float o = BF(0x3fc90fda) - (x - (BF(0x33a22168) - r));
			memcpy(&om, &o, 4);
		}
	}else{	// x >= 0.5 (x < 0 is impossible here, the original's branch 0x557642 for it is left out)
		volatile float z = (1.0f - x)*0.5f;
		float s = _rwSqrt(z);
		volatile float sf = s; float df = BF(FB(&sf) & 0xfffff000);
		X r = acosP(z)/acosQ(z); r = r*s;
		X cc = z - df*df; cc = cc/(df + s);
		X w = r + cc; w = w + df;
		float o = w + w; memcpy(&om, &o, 4);
	}
	memcpy(c + 8, &om, 4);
	int lerp = cs >= BF(0x3f7fff58);
	memcpy(c + 9, &lerp, 4);
	if(!lerp){
		float o = c[8]; X o2 = o*o;
		X p = BF(0x2f2ec9d3)*o2; p = p - BF(0x32d72f34); p = p*o2; p = p + BF(0x3638ef1b); p = p*o2; p = p - BF(0x39500d01); p = p*o2;
		p = p + BF(0x3c088889); p = p*o2; p = p - BF(0x3e2aaaab);
		X k = 1.0f/(o + p*(o2*o));
		c[3] = k*c[3]; c[0] = k*c[0]; c[1] = c[1]*k; c[2] = c[2]*k;
		c[7] = k*c[7]; c[4] = c[4]*k; c[5] = c[5]*k; c[6] = c[6]*k;
	}
}

// L1:exact 0x557860: line (start, delta) against triangle v0 v1 v2, one sided (det > 1e-8); *distance = parameter along delta
RWAPI RwBool RtIntersectionLineTriangle(const RwV3d *lineStart, const RwV3d *lineDelta, const RwV3d *v0, const RwV3d *v1, const RwV3d *v2, RwReal *distance){
	const float *s = (const float *)lineStart, *d = (const float *)lineDelta, *a = (const float *)v0, *b = (const float *)v1, *c = (const float *)v2;
	float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	X px = d[1]*e2[2] - d[2]*e2[1], py = d[2]*e2[0] - e2[2]*d[0], pz = e2[1]*d[0] - d[1]*e2[0];
	X det = py*e1[1] + px*e1[0]; det = det + pz*e1[2];
	volatile float detf = det;
	if(!(det > BF(0x322bcc77))) return 0;
	volatile float lo = detf*BF(0xb727c5ac), hi = detf - lo;
	float t[3] = {s[0] - a[0], s[1] - a[1], s[2] - a[2]};
	X u = t[1]*py + t[0]*px; u = u + t[2]*pz;
	volatile float uf = u;
	if(uf < lo || !(uf <= hi)) return 0;
	float qx = t[1]*e1[2] - t[2]*e1[1], qy = t[2]*e1[0] - t[0]*e1[2], qz = t[0]*e1[1] - t[1]*e1[0];
	X v = d[1]*qy + qx*d[0]; v = v + d[2]*qz;
	if(v < lo) return 0;
	X uv = uf + v;
	if(!(uv <= hi)) return 0;
	X tt = qy*e2[1] + qx*e2[0]; tt = tt + qz*e2[2];
	volatile float ttf = tt; *distance = tt;
	if(ttf < lo || !(ttf <= hi)) return 0;
	*distance = ttf/detf;
	return 1;
}

// ---- RpRandom 0x544e80 / RpRandomSeed 0x544e00: additive lagged generator over 31 words (front 3 words ahead of rear).
// The state lives here.  The exe's plugin constructor 0x544fa0 stays original: it seeds ITS table with 0x9a319039 and then calls
// RpRandom 310 times (= this function after the swap), so this state starts seeded with 0x9a319039 but not warmed up.
// ponytail: a second RwEngineOpen would re-seed only the exe's unused table; the game opens the engine once.
static uint32_t rnd[31], *rndFront, *rndRear;
static void rndSeedTable(uint32_t seed){
	rnd[0] = seed;
	for(int i = 1; i < 31; i++) rnd[i] = rnd[i-1]*0x41c64e6du + 0x3039u;
	rndFront = rnd + 3; rndRear = rnd;
}
static struct RndInit { RndInit(void){ rndSeedTable(0x9a319039u); } } rndInit;
RWAPI RwUInt32 RpRandom(void){
	*rndFront += *rndRear;
	uint32_t r = *rndFront++;
	if(rndFront >= rnd + 31){ rndFront = rnd; rndRear++; }
	else if(++rndRear >= rnd + 31) rndRear = rnd;
	return r >> 1;
}
RWAPI void RpRandomSeed(RwUInt32 seed){
	rndSeedTable(seed);
	for(int i = 0; i < 310; i++) RpRandom();
}

}
