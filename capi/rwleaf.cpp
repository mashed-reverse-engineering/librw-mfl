// RWLEAF (LIB_RW group rwleaf, rw6): stateless RenderWare leafs of MFL.exe written op for op from their disassembly (x87 like rwcolltree.cpp:
// X = a value the original keeps on the x87 stack, R() = its float store):
//   _rx_rxRadixExchangeSort 0x4f3280 (pipeline cluster sort), _rwD3D9MeshGetNumVerticesMinIndex 0x4fa700, FUN_004fa760 (triangle list sort
//   through the exe's CRT qsort, with its comparator 0x4fa790 here), _rwD3D9ConvertToTriList 0x4fa820, D3D9MatrixInvertOrthoNormalized 0x50c110.
// Exe-layout adapter: mflrw_rwleaf_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over qsort 0x5d38b0.
// Check: tools/delink/rw_rwleaf_check.sh (the original code and these exports natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint16_t u16;
typedef uint32_t u32;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
// addresses as integers (the sort walks one element below the segment start, as the original)
#define U32(a)  (*(u32 *)(uintptr_t)(a))
#define F(p, off)  (*(float *)((char *)(p) + (off)))

typedef int (__cdecl *CmpFn)(const void *, const void *);
typedef void (__cdecl *QsortFn)(void *base, u32 n, u32 size, CmpFn cmp);
static QsortFn g_qsort;

static inline void swapEl(u32 a, u32 b, u32 size){
	for(u32 k = size >> 2; k; k--, a += 4, b += 4){ u32 t = U32(b); U32(b) = U32(a); U32(a) = t; }
}

extern "C" {

void __cdecl mflrw_rwleaf_bind(void *qsortFn){ g_qsort = (QsortFn)qsortFn; }

// 0x4f3280: sort n elements of `size` bytes by the u32 key at `off`: radix exchange on the key bits from the top bit of maxKey down while a
// segment has 6+ elements (explicit stack), then the minimum of the first five to the front as a sentinel and insertion sort.  Parameter
// checks as the original (no base, key beyond the element, minKey >= maxKey: nothing).  minKey is only checked.  The return value is not kept.
void __cdecl rw37__rx_rxRadixExchangeSort(void *base_, u32 n, u32 size, u32 off, u32 minKey, u32 maxKey){
	u32 base = (u32)(uintptr_t)base_;
	if(!base || off + 4 > size || minKey >= maxKey) return;
	if(n > 5){
		u32 bit;
		if(maxKey){ u32 cl = 0; for(u32 m = maxKey >> 1; m; m >>= 1) cl++; bit = 1u << cl; }
		else bit = 0x80000000u;
		struct { u32 lo, hi, bit; } st[34];
		int sp = 0;
		st[0].lo = base; st[0].hi = base + (n - 1)*size; st[0].bit = bit; sp = 1;
		do{
			sp--;
			u32 lo = st[sp].lo, hi = st[sp].hi;
			bit = st[sp].bit;
			for(;;){
				u32 i = lo, j = hi;
				for(;;){
					if(!(U32(i + off) & bit)){ do{ i += size; if(i > j) goto parted; }while(!(U32(i + off) & bit)); }
					if(U32(j + off) & bit){ do{ j -= size; if(i > j) goto parted; }while(U32(j + off) & bit); }
					swapEl(i, j, size);
					i += size; j -= size;
					if(i > j) break;
				}
			parted:
				bit >>= 1;
				if(!bit) break;
				j += size;
				if(hi >= j + 5*size){ st[sp].lo = j; st[sp].hi = hi; st[sp].bit = bit; sp++; }
				j = i - size;
				if(j < lo + 5*size) break;
				hi = j;
			}
		}while(sp != 0);
	}
	if(n <= 1) return;
	u32 m = n - 1, k = m < 4 ? m : 4;
	u32 minIdx = k, mk = U32(base + k*size + off);
	for(u32 idx = k; idx-- > 0; ){
		u32 v = U32(base + idx*size + off);
		if(v < mk){ mk = v; minIdx = idx; }
	}
	if(minIdx) swapEl(base, base + minIdx*size, size);
	u32 prev = base;
	for(u32 c = m; c; c--, prev += size){
		u32 ck = U32(prev + size + off);
		for(u32 e = prev; U32(e + off) > ck; e -= size) swapEl(e, e + size, size);
	}
}

// 0x4fa700: index range of n u16 indices: *numVerts = max - min + 1, *minIndex = min (both 0 for n = 0); returns numVerts (as the original's eax)
u32 *__cdecl rw37__rwD3D9MeshGetNumVerticesMinIndex(const u16 *idx, u32 n, u32 *numVerts, u32 *minIndex){
	if(!n){ *numVerts = 0; *minIndex = 0; return numVerts; }
	u32 lo = 0xffffffffu, hi = 0;
	do{
		u32 v = *idx++;
		if(lo >= v) lo = v;
		if(hi <= v) hi = v;
	}while(--n);
	*numVerts = hi - lo + 1;
	*minIndex = lo;
	return numVerts;
}

// 0x4fa790: triangle order for FUN_004fa760: each triangle's three indices sorted ascending, then min, mid, max compared
static int __cdecl cmpTri(const void *pa, const void *pb){
	const u16 *a = (const u16 *)pa, *b = (const u16 *)pb;
	u32 a0 = a[0], a1 = a[1], a2 = a[2], t;
	if(a0 > a1){ t = a0; a0 = a1; a1 = t; }
	if(a1 > a2){ t = a1; a1 = a2; a2 = t; if(a0 > a1){ t = a0; a0 = a1; a1 = t; } }
	u32 b0 = b[0], b1 = b[1], b2 = b[2];
	if(b0 > b1){ t = b0; b0 = b1; b1 = t; }
	if(b1 > b2){ t = b1; b1 = b2; b2 = t; if(b0 > b1){ t = b0; b0 = b1; b1 = t; } }
	if(a0 != b0) return (int)(a0 - b0);
	if(a1 != b1) return (int)(a1 - b1);
	return (int)(a2 - b2);
}

// FUN_004fa760: sort the numIndices / 3 triangles of a u16 triangle list (exe CRT qsort, same algorithm as the original call)
void __cdecl rw37_D3D9SortTriList(u16 *tris, u32 numIndices){
	if(!g_qsort){ fprintf(stderr, "[mflrw] rwleaf called before mflrw_rwleaf_bind\n"); abort(); }
	g_qsort(tris, numIndices / 3, 6, cmpTri);
}

// 0x4fa820: a u16 tri-strip as a triangle list, minIndex subtracted, degenerate triangles dropped, odd triangles reversed; returns the
// number of indices written.  numIndices - 2 = 0: nothing (the loop runs numIndices - 2 times unsigned, as the original)
u32 __cdecl rw37__rwD3D9ConvertToTriList(u16 *dst, const u16 *strip, u32 numIndices, u32 minIndex){
	u32 n = numIndices - 2, count = 0;
	if(!n) return 0;
	for(u32 i = 0; i < n; i++, strip++){
		u16 s0 = strip[0], s1 = strip[1];
		if(s0 == s1) continue;
		u16 s2 = strip[2];
		if(s0 == s2 || s1 == s2) continue;
		u16 c = (u16)(s0 - minIndex);
		if(i & 1){
			dst[2] = c; dst[1] = (u16)(strip[1] - minIndex); dst[0] = (u16)(strip[2] - minIndex);
		}else{
			dst[0] = c; dst[1] = (u16)(strip[1] - minIndex); dst[2] = (u16)(strip[2] - minIndex);
		}
		dst += 3; count += 3;
	}
	return count;
}

// 0x50c110: inverse of an orthonormal D3D 4x4 (transpose of the 3x3, translation -p.r / -p.u / -p.a); returns src[10]'s bits (the
// original's eax); dst == src works as in the original (same load / store order)
static inline void cp(float *d, const float *s){ memcpy(d, s, 4); }		// word copy (memcpy: no type-based alias assumption, dst may be src)
u32 __cdecl rw37_D3D9MatrixInvertOrthoNormalized(float *dst, const float *src){
	F(dst, 0x30) = R(-(((X)F(src, 0x34) * F(src, 4) + (X)F(src, 0x30) * F(src, 0)) + (X)F(src, 0x38) * F(src, 8)));
	cp(dst + 0, src + 0); cp(dst + 4, src + 1); cp(dst + 8, src + 2);
	F(dst, 0x34) = R(-(((X)F(src, 0x30) * F(src, 0x10) + (X)F(src, 0x14) * F(src, 0x34)) + (X)F(src, 0x38) * F(src, 0x18)));
	cp(dst + 1, src + 4); cp(dst + 5, src + 5); cp(dst + 9, src + 6);
	F(dst, 0x38) = R(-(((X)F(src, 0x30) * F(src, 0x20) + (X)F(src, 0x24) * F(src, 0x34)) + (X)F(src, 0x38) * F(src, 0x28)));
	cp(dst + 2, src + 8); cp(dst + 6, src + 9);
	u32 r; memcpy(&r, src + 10, 4); memcpy(dst + 10, &r, 4);
	return r;
}

} // extern "C"
