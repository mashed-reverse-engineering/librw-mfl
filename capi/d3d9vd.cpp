// D3D9VD: the D3D9 vertex-declaration instancing converters of MFL.exe (rpworld d3d9vertexdeclaration.obj), written from the disassembly
// like bezpat.cpp (valid at the game's x87 CW 0x7f; a float variable = a value the original stores, X = a value it keeps on the x87 stack).
//   _rpD3D9VertexDeclarationInstV3d 0x5063f0, InstV2d 0x507e00, InstV3dMorph 0x507480, InstWeights 0x505c40, InstColor 0x508400,
//   InstIndicesRemap 0x508460, UnInstV3d 0x508cb0, UnInstV2d 0x5090a0.
// type = D3DDECLTYPE 0..14 (FLOAT1..4, D3DCOLOR, UBYTE4, SHORT2/4, UBYTE4N, SHORT2/4N, USHORT2/4N, UDEC3, DEC3N); the Inst* functions write
// numVerts elements at a byte stride and return the element size (the original reads its size table with the type as index: types > 16
// read past the table there, here they return 0); other types write nothing.  Float to int is fistp at the current rounding mode.
// Not here: FUN_00508600 (tangent instancing: engine allocator, D3D9 index buffer Lock/Unlock, RwV3dLength / _rwInvSqrt) and
// InstV3dComp 0x506ad0 (no caller, no symbol).
// No exe state, no adapter: the kernels only touch their arguments.
// Check: tools/delink/rw_d3d9vd_check.sh (the original code and these exports natively under Wine on the same random inputs, outputs and the
// untouched bytes around them compared word for word).
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef float __attribute__((may_alias)) FA;
typedef u32 __attribute__((may_alias)) UA;
typedef u16 __attribute__((may_alias)) WA;
#define F(p, off)  (*(FA *)((u8 *)(p) + (off)))
#define U(p, off)  (*(UA *)((u8 *)(p) + (off)))
#define W(p, off)  (*(WA *)((u8 *)(p) + (off)))
#define B(p, off)  (((u8 *)(p))[off])

// fistp at the current rounding mode (the game's: nearest), out of range = 0x80000000 like the hardware
static inline int32_t fist(X v){
	int32_t r;
	__asm__ volatile("fistpl %0" : "=m"(r) : "t"(v) : "st");
	return r;
}
static inline float kf(u32 b){ float f; memcpy(&f, &b, 4); return f; }
#define K255    kf(0x437f0000)
#define K32767  kf(0x46fffe00)
#define K65535  kf(0x477fff00)
#define K511    kf(0x43ff8000)
#define KONE    kf(0x3f800000)
#define KI255   kf(0x3b808081)	// 1/255
#define KI32767 kf(0x38000100)	// 1/32767
#define KI65535 kf(0x37800080)	// 1/65535
#define KI511   kf(0x3b004020)	// 1/511

static const u32 declSize[17] = { 4, 8, 12, 16, 4, 4, 4, 8, 4, 4, 8, 4, 8, 4, 4, 4, 8 };	// 0x5ec388
static inline u32 sizeOf(u32 type){ return type < 17 ? declSize[type] : 0; }
static inline u32 pack3(int32_t x, int32_t y, int32_t z){ return (((u32)z & 0x3ff) << 10 | ((u32)y & 0x3ff)) << 10 | ((u32)x & 0x3ff); }

// component sources of the shared encoder: bits(c) = the float bits of component c (copy types), ival(c, k) = component c (times k when k != 0)
// rounded to float and converted to int32.  Components past the source's count are 0 (InstV2d's z).
struct SrcVec {
	const u8 *p; int n, step;
	u32 bits(int c) const { return c < n ? U(p, 4*c) : 0; }
	int32_t ival(int c, float k) const {
		if(c >= n) return 0;
		if(k == 0) return fist(F(p, 4*c));
		float t = F(p, 4*c) * k;
		return fist(t);
	}
	void next(){ p += step; }
};
// InstV3dMorph: copy and plain int types take lerp = (s2 - s1) * scale + s1; the normalised types a * s1 + b * s2 with a = (1 - scale) * k,
// b = scale * k (both stored)
struct SrcMorph {
	const u8 *p1, *p2; float sc;
	float lerp(int c) const { float t = (F(p2, 4*c) - F(p1, 4*c)) * sc + F(p1, 4*c); return t; }
	u32 bits(int c) const { float t = lerp(c); u32 b; memcpy(&b, &t, 4); return b; }
	int32_t ival(int c, float k) const {
		if(k == 0) return fist(lerp(c));
		float a = (KONE - sc) * k, b = sc * k;
		float t = a * F(p1, 4*c) + b * F(p2, 4*c);
		return fist(t);
	}
	void next(){ p1 += 12; p2 += 12; }
};

template<class S> static u32 encode(u32 type, u8 *m, S s, int32_t n, u32 stride){
	if(type > 14 || n <= 0) return sizeOf(type);
	for(; n > 0; n--, m += stride, s.next()){
		switch(type){
		case 0: U(m, 0) = s.bits(0); break;
		case 1: U(m, 0) = s.bits(0); U(m, 4) = s.bits(1); break;
		case 2: U(m, 0) = s.bits(0); U(m, 4) = s.bits(1); U(m, 8) = s.bits(2); break;
		case 3: U(m, 0) = s.bits(0); U(m, 4) = s.bits(1); U(m, 8) = s.bits(2); U(m, 12) = 0x3f800000; break;
		case 4: B(m, 2) = (u8)s.ival(0, K255); B(m, 1) = (u8)s.ival(1, K255); B(m, 0) = (u8)s.ival(2, K255); B(m, 3) = 0xff; break;
		case 5: B(m, 0) = (u8)s.ival(0, 0); B(m, 1) = (u8)s.ival(1, 0); B(m, 2) = (u8)s.ival(2, 0); B(m, 3) = 1; break;
		case 6: W(m, 0) = (u16)s.ival(0, 0); W(m, 2) = (u16)s.ival(1, 0); break;
		case 7: W(m, 0) = (u16)s.ival(0, 0); W(m, 2) = (u16)s.ival(1, 0); W(m, 4) = (u16)s.ival(2, 0); W(m, 6) = 1; break;
		case 8: B(m, 0) = (u8)s.ival(0, K255); B(m, 1) = (u8)s.ival(1, K255); B(m, 2) = (u8)s.ival(2, K255); B(m, 3) = 0xff; break;
		case 9: W(m, 0) = (u16)s.ival(0, K32767); W(m, 2) = (u16)s.ival(1, K32767); break;
		case 10: W(m, 0) = (u16)s.ival(0, K32767); W(m, 2) = (u16)s.ival(1, K32767); W(m, 4) = (u16)s.ival(2, K32767); W(m, 6) = 0x7fff; break;
		case 11: W(m, 0) = (u16)s.ival(0, K65535); W(m, 2) = (u16)s.ival(1, K65535); break;
		case 12: W(m, 0) = (u16)s.ival(0, K65535); W(m, 2) = (u16)s.ival(1, K65535); W(m, 4) = (u16)s.ival(2, K65535); W(m, 6) = 0xffff; break;
		case 13: U(m, 0) = pack3(s.ival(0, 0), s.ival(1, 0), s.ival(2, 0)); break;
		case 14: U(m, 0) = pack3(s.ival(0, K511), s.ival(1, K511), s.ival(2, K511)); break;
		}
	}
	return sizeOf(type);
}

extern "C" {

// 0x5063f0: RwV3d (12-byte) source; FLOAT3 at stride 12 is one block copy of numVerts * 12 bytes (no numVerts check, as the original)
u32 __cdecl rw37__rpD3D9VertexDeclarationInstV3d(u32 type, u8 *mem, const u8 *src, int32_t numVerts, u32 stride){
	if(type == 2 && stride == 12){ memcpy(mem, src, (u32)numVerts * 12); return 12; }
	SrcVec s = { src, 3, 12 };
	return encode(type, mem, s, numVerts, stride);
}

// 0x507e00: RwV2d (8-byte) source, z = 0; FLOAT2 at stride 8 is one block copy
u32 __cdecl rw37__rpD3D9VertexDeclarationInstV2d(u32 type, u8 *mem, const u8 *src, int32_t numVerts, u32 stride){
	if(type == 1 && stride == 8){ memcpy(mem, src, (u32)numVerts * 8); return 8; }
	SrcVec s = { src, 2, 8 };
	return encode(type, mem, s, numVerts, stride);
}

// 0x507480: the morph target blend of two RwV3d arrays (see SrcMorph)
u32 __cdecl rw37__rpD3D9VertexDeclarationInstV3dMorph(u32 type, u8 *mem, const u8 *src1, const u8 *src2, float scale, int32_t numVerts, u32 stride){
	SrcMorph s = { src1, src2, scale };
	return encode(type, mem, s, numVerts, stride);
}

// 0x505c40: skin weights (RwV4d source, 16 bytes).  The normalised byte / short types store weights 1-3 and derive weight 0 so the sum is the
// maximum (D3DCOLOR keeps the D3DCOLOR byte order); a first weight >= 1 stores (max, 0, 0, 0).  DEC3N: below 1/511 for weight 2 only weight 0
// is stored (its complement in the y field), otherwise weights 1 and 2 plus the complement as the low field (not masked, as in the original).
u32 __cdecl rw37__rpD3D9VertexDeclarationInstWeights(u32 type, u8 *mem, const u8 *src, int32_t n, u32 stride){
	SrcVec s = { src, 4, 16 };
	if(type > 14 || n <= 0) return sizeOf(type);
	for(; n > 0; n--, mem += stride, s.next()){
		const u8 *p = s.p;
		switch(type){
		case 0: U(mem, 0) = U(p, 0); break;
		case 1: U(mem, 0) = U(p, 0); U(mem, 4) = U(p, 4); break;
		case 2: case 3: U(mem, 0) = U(p, 0); U(mem, 4) = U(p, 4); U(mem, 8) = U(p, 8); U(mem, 12) = U(p, 12); break;	// FLOAT3 writes 16 bytes too (original)
		case 4:
			if(F(p, 0) >= KONE){ B(mem, 0) = 0; B(mem, 1) = 0; B(mem, 2) = 0xff; B(mem, 3) = 0; break; }
			B(mem, 1) = (u8)s.ival(1, K255); B(mem, 0) = (u8)s.ival(2, K255); B(mem, 3) = (u8)s.ival(3, K255);
			B(mem, 2) = (u8)(0xff - B(mem, 3) - B(mem, 1) - B(mem, 0));
			break;
		case 5: B(mem, 0) = (u8)s.ival(0, 0); B(mem, 1) = (u8)s.ival(1, 0); B(mem, 2) = (u8)s.ival(2, 0); B(mem, 3) = (u8)s.ival(3, 0); break;
		case 6: W(mem, 0) = (u16)s.ival(0, 0); W(mem, 2) = (u16)s.ival(1, 0); break;
		case 7: W(mem, 0) = (u16)s.ival(0, 0); W(mem, 2) = (u16)s.ival(1, 0); W(mem, 4) = (u16)s.ival(2, 0); W(mem, 6) = (u16)s.ival(3, 0); break;
		case 8:
			if(F(p, 0) >= KONE){ B(mem, 0) = 0xff; B(mem, 1) = 0; B(mem, 2) = 0; B(mem, 3) = 0; break; }
			B(mem, 1) = (u8)s.ival(1, K255); B(mem, 2) = (u8)s.ival(2, K255); B(mem, 3) = (u8)s.ival(3, K255);
			B(mem, 0) = (u8)(0xff - B(mem, 2) - B(mem, 3) - B(mem, 1));
			break;
		case 9: case 11: {
			u32 mx = type == 9 ? 0x7fff : 0xffff;
			int32_t i0 = s.ival(0, type == 9 ? K32767 : K65535);
			W(mem, 0) = (u16)i0; W(mem, 2) = (u16)(mx - (u32)i0);
			break;
		}
		case 10: case 12: {
			u32 mx = type == 10 ? 0x7fff : 0xffff; float k = type == 10 ? K32767 : K65535;
			int32_t i1 = s.ival(1, k), i2 = s.ival(2, k), i3 = s.ival(3, k);
			W(mem, 2) = (u16)i1; W(mem, 4) = (u16)i2; W(mem, 6) = (u16)i3;
			W(mem, 0) = (u16)(mx - (u32)i3 - (u32)i2 - (u32)i1);
			break;
		}
		case 13: U(mem, 0) = pack3(s.ival(0, 0), s.ival(1, 0), s.ival(2, 0)); break;
		case 14:
			if(F(p, 8) < KI511){
				u32 a = (u32)s.ival(0, K511) & 0x3ff;
				U(mem, 0) = (0x7fc00 - (a << 10)) | a;
			}else{
				u32 b = (u32)s.ival(1, K511) & 0x3ff, c = (u32)s.ival(2, K511) & 0x3ff;
				U(mem, 0) = ((c << 10 | b) << 10) | (0x1ff - c - b);
			}
			break;
		}
	}
	return sizeOf(type);
}

// 0x508400: RwRGBA -> D3DCOLOR (ARGB); returns 1 when some alpha is not 255
RwBool __cdecl rw37__rpD3D9VertexDeclarationInstColor(u8 *mem, const u8 *color, int32_t n, u32 stride){
	u8 a = 0xff;
	for(; n > 0; n--, mem += stride, color += 4){
		U(mem, 0) = (u32)color[3] << 24 | (u32)color[0] << 16 | (u32)color[1] << 8 | color[2];
		a &= color[3];
	}
	return a != 0xff;
}

// 0x508460: skin bone indices (4 bytes per vertex) through the remap table; D3DCOLOR (byte order 2 1 0 3), UBYTE4, SHORT2, SHORT4
u32 __cdecl rw37__rpD3D9VertexDeclarationInstIndicesRemap(u32 type, u8 *mem, const u8 *idx, const u8 *remap, int32_t n, u32 stride){
	for(; n > 0 && type - 4 < 4; n--, mem += stride, idx += 4){
		switch(type){
		case 4: B(mem, 0) = remap[idx[2]]; B(mem, 1) = remap[idx[1]]; B(mem, 2) = remap[idx[0]]; B(mem, 3) = remap[idx[3]]; break;
		case 5: B(mem, 0) = remap[idx[0]]; B(mem, 1) = remap[idx[1]]; B(mem, 2) = remap[idx[2]]; B(mem, 3) = remap[idx[3]]; break;
		case 6: W(mem, 0) = remap[idx[0]]; W(mem, 2) = remap[idx[1]]; break;
		case 7: W(mem, 0) = remap[idx[0]]; W(mem, 2) = remap[idx[1]]; W(mem, 4) = remap[idx[2]]; W(mem, 6) = remap[idx[3]]; break;
		}
	}
	return sizeOf(type);
}

static inline int32_t sx10(u32 v){ v &= 0x3ff; return (int32_t)(v & 0x200 ? v | 0xfffffc00 : v); }

// 0x508cb0 / 0x5090a0: one element back to RwV3d / RwV2d (int -> float through fild, times the reciprocal; USHORT*N read as signed shorts,
// as the original does).  Types > 14 give 0.  n = 2 (V2d) or 3 (V3d).
static void uninst(u32 type, u8 *dst, const u8 *src, int n){
	float r[3] = { 0, 0, 0 };
	u32 rb[3] = { 0, 0, 0 };
	bool bits = false, z = n == 3;	// V2d reads two components only
	switch(type){
	case 0: rb[0] = U(src, 0); bits = true; break;
	case 1: rb[0] = U(src, 0); rb[1] = U(src, 4); bits = true; break;
	case 2: case 3: rb[0] = U(src, 0); rb[1] = U(src, 4); if(z) rb[2] = U(src, 8); bits = true; break;
	case 4: r[0] = (X)src[2] * KI255; r[1] = (X)src[1] * KI255; if(z) r[2] = (X)src[0] * KI255; break;
	case 5: r[0] = src[0]; r[1] = src[1]; if(z) r[2] = src[2]; break;
	case 6: r[0] = (int16_t)W(src, 0); r[1] = (int16_t)W(src, 2); break;
	case 7: r[0] = (int16_t)W(src, 0); r[1] = (int16_t)W(src, 2); if(z) r[2] = (int16_t)W(src, 4); break;
	case 8: r[0] = (X)src[0] * KI255; r[1] = (X)src[1] * KI255; if(z) r[2] = (X)src[2] * KI255; break;
	case 9: r[0] = (X)(int16_t)W(src, 0) * KI32767; r[1] = (X)(int16_t)W(src, 2) * KI32767; break;
	case 10: r[0] = (X)(int16_t)W(src, 0) * KI32767; r[1] = (X)(int16_t)W(src, 2) * KI32767; if(z) r[2] = (X)(int16_t)W(src, 4) * KI32767; break;
	case 11: r[0] = (X)(int16_t)W(src, 0) * KI65535; r[1] = (X)(int16_t)W(src, 2) * KI65535; break;
	case 12: r[0] = (X)(int16_t)W(src, 0) * KI65535; r[1] = (X)(int16_t)W(src, 2) * KI65535; if(z) r[2] = (X)(int16_t)W(src, 4) * KI65535; break;
	case 13: { u32 v = U(src, 0); r[0] = (X)(v & 0x3ff); r[1] = (X)(v >> 10 & 0x3ff); r[2] = (X)(v >> 20 & 0x3ff); break; }
	case 14: { u32 v = U(src, 0); r[0] = (X)sx10(v) * KI511; r[1] = (X)sx10(v >> 10) * KI511; r[2] = (X)sx10(v >> 20) * KI511; break; }
	default: bits = true; break;
	}
	for(int c = 0; c < n; c++){
		if(bits) U(dst, 4*c) = rb[c];
		else F(dst, 4*c) = r[c];
	}
}

void __cdecl rw37__rpD3D9VertexDeclarationUnInstV3d(u32 type, u8 *dst, const u8 *src){ uninst(type, dst, src, 3); }
void __cdecl rw37__rpD3D9VertexDeclarationUnInstV2d(u32 type, u8 *dst, const u8 *src){ uninst(type, dst, src, 2); }

} // extern "C"
