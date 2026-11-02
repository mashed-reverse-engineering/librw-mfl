// D3DX math entry points of MFL.exe's statically linked D3DX (plan_librw_d3dx S1).  The exe fills a 0x47 slot table (0x62ec88) at the first call through D3DXCpuOptimizations (0x50501a);
// on this host class (CPUID SSE2, no 3DNow) the SSE2 installer 0x50c60e picks the entries below.  Each function is written op for op from that SSE2 variant (the exe is stdcall, ret 0xc / 0x8):
// same lane arithmetic, same association of the partial sums, same rcpps + Newton step; valid under any MXCSR the caller runs (the game sets FTZ|DAZ).  The DLL itself never touches MXCSR.
//   exe VA      table slot  D3DX9 name                      DLL export
//   0x510b45    0x0c        D3DXMatrixMultiply              D3DXMatrixMultiply            (sse2 variant of 0x4fc3e0; alias sse_D3DXMatrixMultiply)
//   0x50f872    0x10        D3DXMatrixMultiplyTranspose     D3DXMatrixMultiplyTranspose
//   0x50f5d9    0x18        D3DXVec3TransformNormal         D3DXVec3TransformNormal
//   0x511050    0x28        D3DXVec3TransformCoord          D3DXVec3TransformCoord        (rcpps: result is the host CPU's approximation, like the original)
//   0x4fc629    0x94        D3DXMatrixTranspose             D3DXMatrixTranspose           (plain C variant, no SSE override exists; fld/fstp copies: an sNaN would be quieted by the original, copied bit-exact here)
// Exports are listed in rwmfl.def by hand (gen_facade.py appends them); prototypes are not in rwmfl.h (stdcall, not the RW cdecl ABI).
#include <emmintrin.h>
#include <string.h>
#define S2 __attribute__((target("sse2"), force_align_arg_pointer))  // i386 stdcall entry: stack is only 4-aligned, spills use aligned mulps (%esp)
#define STD __attribute__((stdcall))
#define EXPORT(name) __asm__("_" #name)
extern "C" {
typedef struct { float m[16]; } M4;
typedef struct { float x, y, z; } V3;

static inline S2 __m128 row(const M4 *a, int r){ return _mm_loadu_ps(a->m + 4*r); }
static inline S2 __m128 bc(float f){ return _mm_set1_ps(f); }
static inline S2 void st4(M4 *o, int r, __m128 v){ _mm_storeu_ps(o->m + 4*r, v); }

S2 STD M4 *mfl_D3DXMatrixMultiply(M4 *out, const M4 *a, const M4 *b) EXPORT(D3DXMatrixMultiply);
S2 STD M4 *mfl_D3DXMatrixMultiply(M4 *out, const M4 *a, const M4 *b){		// 0x510b45
	__m128 b0 = row(b,0), b1 = row(b,1), b2 = row(b,2), b3 = row(b,3);
	__m128 r0 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(bc(a->m[0]),b0), _mm_mul_ps(bc(a->m[1]),b1)),
	                       _mm_add_ps(_mm_mul_ps(bc(a->m[2]),b2), _mm_mul_ps(bc(a->m[3]),b3)));
	__m128 r1 = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(bc(a->m[4]),b0), _mm_mul_ps(bc(a->m[5]),b1)), _mm_mul_ps(bc(a->m[6]),b2)), _mm_mul_ps(bc(a->m[7]),b3));
	__m128 r2 = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(bc(a->m[8]),b0), _mm_mul_ps(bc(a->m[9]),b1)), _mm_mul_ps(bc(a->m[10]),b2)), _mm_mul_ps(bc(a->m[11]),b3));
	__m128 r3 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(b0,bc(a->m[12])), _mm_mul_ps(b1,bc(a->m[13]))),
	                       _mm_add_ps(_mm_mul_ps(b2,bc(a->m[14])), _mm_mul_ps(b3,bc(a->m[15]))));
	st4(out,0,r0); st4(out,1,r1); st4(out,2,r2); st4(out,3,r3);
	return out;
}

S2 STD M4 *mfl_D3DXMatrixMultiplyTranspose(M4 *out, const M4 *a, const M4 *b) EXPORT(D3DXMatrixMultiplyTranspose);
S2 STD M4 *mfl_D3DXMatrixMultiplyTranspose(M4 *out, const M4 *a, const M4 *b){		// 0x50f872: left fold per row, stored transposed
	__m128 b0 = row(b,0), b1 = row(b,1), b2 = row(b,2), b3 = row(b,3), r[4];
	for(int i = 0; i < 4; i++)
		r[i] = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(bc(a->m[4*i]),b0), _mm_mul_ps(bc(a->m[4*i+1]),b1)), _mm_mul_ps(bc(a->m[4*i+2]),b2)), _mm_mul_ps(bc(a->m[4*i+3]),b3));
	_MM_TRANSPOSE4_PS(r[0], r[1], r[2], r[3]);
	for(int i = 0; i < 4; i++) st4(out,i,r[i]);
	return out;
}

S2 STD V3 *mfl_D3DXVec3TransformNormal(V3 *out, const V3 *v, const M4 *m) EXPORT(D3DXVec3TransformNormal);
S2 STD V3 *mfl_D3DXVec3TransformNormal(V3 *out, const V3 *v, const M4 *m){		// 0x50f5d9: (x*M0 + y*M1) + z*M2
	__m128 r = _mm_add_ps(_mm_add_ps(_mm_mul_ps(bc(v->x),row(m,0)), _mm_mul_ps(bc(v->y),row(m,1))), _mm_mul_ps(bc(v->z),row(m,2)));
	float t[4]; _mm_storeu_ps(t, r); out->x = t[0]; out->y = t[1]; out->z = t[2];
	return out;
}

S2 STD V3 *mfl_D3DXVec3TransformCoord(V3 *out, const V3 *v, const M4 *m) EXPORT(D3DXVec3TransformCoord);
S2 STD V3 *mfl_D3DXVec3TransformCoord(V3 *out, const V3 *v, const M4 *m){		// 0x511050: (x*M0 + y*M1) + (z*M2 + M3), then res * (2r - r*res*r) with r = rcpps(res), lane 3 broadcast
	__m128 res = _mm_add_ps(_mm_add_ps(_mm_mul_ps(row(m,0),bc(v->x)), _mm_mul_ps(row(m,1),bc(v->y))), _mm_add_ps(_mm_mul_ps(row(m,2),bc(v->z)), row(m,3)));
	__m128 r = _mm_rcp_ps(res);
	__m128 t = _mm_mul_ps(_mm_mul_ps(r, res), r);
	__m128 n = _mm_sub_ps(_mm_add_ps(r, r), t);
	__m128 o = _mm_mul_ps(res, _mm_shuffle_ps(n, n, 0xff));
	float f[4]; _mm_storeu_ps(f, o); out->x = f[0]; out->y = f[1]; out->z = f[2];
	return out;
}

STD M4 *mfl_D3DXMatrixTranspose(M4 *out, const M4 *in) EXPORT(D3DXMatrixTranspose);
STD M4 *mfl_D3DXMatrixTranspose(M4 *out, const M4 *in){		// 0x4fc629
	M4 t;
	for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++) t.m[4*i+j] = in->m[4*j+i];
	memcpy(out, &t, sizeof t);
	return out;
}

// D3DXAssembleShader (0x50cf85, 7 stdcall args): 0 calls in 16 scenarios (S2 measure, ROADMAP T-S2); RW's caller treats hr < 0 as "no shader" (skips CreateVertexShader, returns NULL).
STD long mfl_D3DXAssembleShader(const void *, unsigned, const void *, void *, unsigned, void **shader, void **err) EXPORT(D3DXAssembleShader);
STD long mfl_D3DXAssembleShader(const void *, unsigned, const void *, void *, unsigned, void **shader, void **err){
	if(shader) *shader = 0;
	if(err) *err = 0;
	return (long)0x8876086C;		// D3DERR_INVALIDCALL
}
}
