// RWSPLOPEN (LIB_RW group splopen, rw7): the spline plugin's engine open / close callbacks of MFL.exe and the basis matrix builder they use,
// written op for op from the disassembly (x87 like rwleaf.cpp: X = a value the original keeps on the x87 stack, R() = its float store):
//   SplineOpen 0x556bf0 (first open: the 12 uniform B-spline basis tables of 257 floats in one block, the basis matrices of order 4..10),
//   SplineClose 0x556b90 (last close: frees them), FUN_00556ec0 (order n basis matrix, here SplineBasisMatrixCreate).
// Exe-layout adapter: mflrw_splopen_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance (engine malloc +0x108 /
// free +0x10c), the open counter 0x7da7b8, rapGTable 0x7da7bc (matrix slots 4..10, the 12 basis table pointers at +0x2c) and RwErrorSet.
// Check: tools/delink/rw_splopen_check.sh (the original code and these exports natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint32_t u32;
typedef u32 __attribute__((may_alias)) UA;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot

static const float kInv256 = 0.00390625f, kSixth = 0.16666667f, kHalf = 0.5f;	// 0x5dfdb4, 0x5dd9c8, 0x5dd340
static const float kOne = 1.0f, kTwo = 2.0f, kThree = 3.0f, kFour = 4.0f, kSix = 6.0f;	// 0x5dd330, 0x5dd634, 0x5dd32c, 0x5dd33c, 0x5dd814

typedef void *(__cdecl *MallocFn)(u32 size, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *ErrorSetFn)(void *error);
static char **g_engine;
static int32_t *g_count;
static void **g_rapG;		// [4..10] basis matrices, [11..22] the basis tables (block pointer first)
static ErrorSetFn g_errorSet;

static void need(){ if(!g_engine){ fprintf(stderr, "[mflrw] splopen called before mflrw_splopen_bind\n"); abort(); } }
static void *emalloc(u32 n, u32 hint){ return (*(MallocFn *)(*g_engine + 0x108))(n, hint); }
static void efree(void *p){ (*(FreeFn *)(*g_engine + 0x10c))(p); }
static void err(int32_t code){ int32_t e[2] = { 0x102, code }; g_errorSet(e); }

extern "C" {

void __cdecl mflrw_splopen_bind(char **engine, int32_t *openCount, void *rapGTable, void *errorSet){
	g_engine = engine; g_count = openCount; g_rapG = (void **)rapGTable; g_errorSet = (ErrorSetFn)errorSet;
}

// 0x556ec0: n x n float matrix (engine malloc, hint 0x30102): row r = B[n-1-r] * (1, B[c] * sign for c = 1..r) / A[n-2] with the integer
// recurrences A = 1, 4, 4a - a' (n-1 terms) and B = 1, 2, 4b - b' (n terms, every second one from the top negated), sign = 2 for odd n
// else -2; then M[n*n-1] = M[0] and the upper triangle copied from the lower one walking backwards.  NULL (RwError 0x102 / 0x80000013)
// when an allocation fails.
float *__cdecl rw37_SplineBasisMatrixCreate(int32_t n){
	need();
	int32_t nn = n * n;
	u32 bytes = (u32)nn * 4;
	float *M = (float *)emalloc(bytes, 0x30102);
	if(!M){ err((int32_t)0x80000013); return 0; }
	memset(M, 0, bytes);
	int32_t *A = (int32_t *)emalloc((u32)(n - 1) * 4, 0x30102);
	if(!A) goto fail;
	A[0] = 1; A[1] = 4;
	for(int32_t k = 0; k < n - 3; k++) A[k + 2] = A[k + 1] * 4 - A[k];
	{
		int32_t den = A[n - 2];
		efree(A);
		int32_t *B = (int32_t *)emalloc((u32)n * 4, 0x30102);
		if(!B) goto fail;
		B[0] = 1; B[1] = 2;
		for(int32_t k = 0; k < n - 2; k++) B[k + 2] = B[k + 1] * 4 - B[k];
		for(int32_t c = n - 2; c >= 0; c -= 2) B[c] = -B[c];
		int32_t sgn = (n & 1) ? 2 : -2;
		if(n > 0){
			X d = den;
			float *row = M;
			for(int32_t r = 0; r < n; r++, row += n){
				int32_t br = B[n - 1 - r];
				row[0] = R((X)br / d);
				for(int32_t c = 1; c <= r; c++){ int32_t v = (int32_t)((u32)B[c] * (u32)br * (u32)sgn); row[c] = R((X)v / d); }
			}
		}
		UA *W = (UA *)M;		// word copies of the float matrix (may_alias: they must stay behind the float stores)
		W[nn - 1] = W[0];
		for(int32_t r = 0, base = 0, top = nn; r < n; r++, base += n, top -= n){
			int32_t c = n - 1;
			if(c <= r) continue;
			UA *dst = W + base + c, *src = W + top - c - 1;
			for(int32_t k = c - r; k; k--) *dst-- = *src++;
		}
		efree(B);
		return M;
	}
fail:
	err((int32_t)0x80000013);
	efree(M);
	return 0;
}

// 0x556bf0: first open builds the tables; any failure: RwError 0x102 / 3 and NULL (the open count stays, the table block is not freed)
void *__cdecl rw37_SplineOpen(void *instance, int32_t offset, int32_t size){
	(void)offset; (void)size;
	need();
	if(*g_count == 0){
		char *blk = (char *)emalloc(0x3030, 0x40102);
		g_rapG[11] = blk;
		if(!blk) err((int32_t)0x80000013);
		else{
			for(int k = 1; k < 12; k++) g_rapG[11 + k] = blk + 0x404 * k;
			for(int32_t i = 0; i <= 0x100; i++){
				volatile int32_t iv = i;		// the original filds its counter from a stack slot
				X t = (X)iv * kInv256;
				X t2 = t * t;
				X t3 = t2 * t;
				((float *)g_rapG[11])[i] = R(t3 * kSixth);
				X t3x = t * kThree;
				((float *)g_rapG[12])[i] = R(((((X)kThree - t3x) * t + kThree) * t + kOne) * kSixth);
				((float *)g_rapG[13])[i] = R(((t3x - kSix) * t * t + kFour) * kSixth);
				((float *)g_rapG[14])[i] = R(((((X)kThree - t) * t - kThree) * t + kOne) * kSixth);
				((float *)g_rapG[15])[i] = R(t2 * kHalf);
				((float *)g_rapG[16])[i] = R((((X)kTwo - t3x) * t + kOne) * kHalf);
				((float *)g_rapG[17])[i] = R((t3x - kFour) * t * kHalf);
				((float *)g_rapG[18])[i] = R((((X)kTwo - t) * t - kOne) * kHalf);
				((float *)g_rapG[19])[i] = R(t);
				((float *)g_rapG[20])[i] = R((X)kOne - t3x);
				((float *)g_rapG[21])[i] = R(t3x - kTwo);
				((float *)g_rapG[22])[i] = R((X)kOne - t);
			}
		}
		if(!g_rapG[11]){ err(3); return 0; }
		for(int32_t i = 4; i <= 10; i++){
			void *m = rw37_SplineBasisMatrixCreate(i);
			g_rapG[i] = m;
			if(!m){
				for(int32_t j = i - 1; j >= 4; j--){ efree(g_rapG[j]); g_rapG[j] = 0; }
				err(3);
				return 0;
			}
		}
	}
	++*g_count;
	return instance;
}

// 0x556b90: last close frees the matrices 10..4 and the table block (NULL slots too) and clears the slots
void *__cdecl rw37_SplineClose(void *instance, int32_t offset, int32_t size){
	(void)offset; (void)size;
	need();
	if(--*g_count == 0){
		for(int32_t i = 10; i >= 4; i--){ efree(g_rapG[i]); g_rapG[i] = 0; }
		efree(g_rapG[11]);
		g_rapG[11] = 0;
	}
	return instance;
}

} // extern "C"
