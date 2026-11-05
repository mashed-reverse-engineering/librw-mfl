// RWLEAF2 (LIB_RW group rwleaf2, rw7): RenderWare leafs of MFL.exe written op for op from their disassembly (x87 like rwleaf.cpp:
// X = a value the original keeps on the x87 stack, R() = its float store):
//   Rt2dBrushSetRGBA 0x564b10, BMPImageSetSpan 0x53d6c0, PVSCalculateSectorBounds 0x548920, RpLightGetConeAngle 0x4f4a70,
//   FUN_00542fe0 (the RpHAnim keyframe -> matrix callback, here HAnimKeyFrameToMatrix).
// Exe-layout adapter: mflrw_rwleaf2_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over RwErrorSet 0x4ea160, _rwSqrt 0x4d5410,
// RwV3dLength 0x4d53a0 and the PVS plugin offset 0x947ae8.
// Check: tools/delink/rw_rwleaf2_check.sh (the original code and these exports natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
// may_alias: the original mixes word copies and float loads of the same slots (PVS box), the order must stay
typedef float __attribute__((may_alias)) FA;
typedef u32 __attribute__((may_alias)) UA;
typedef int32_t __attribute__((may_alias)) IA;
#define F(p, off)  (*(FA *)((u8 *)(p) + (off)))
#define U(p, off)  (*(UA *)((u8 *)(p) + (off)))
#define I(p, off)  (*(IA *)((u8 *)(p) + (off)))
// fld + fstp of a float word (an sNaN comes out quiet, as in the original's fst; a C float copy may become an integer move)
static inline u32 fq(const void *p){ u32 r; __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(*(const float *)p)); return r; }
// fld, fchs, fstp
static inline u32 fneg(const void *p){ u32 r; __asm__ volatile("flds %1\n\tfchs\n\tfstps %0" : "=m"(r) : "m"(*(const float *)p)); return r; }
static inline float bf(u32 b){ float f; memcpy(&f, &b, 4); return f; }

static const float kZero = 0.0f, kOne = 1.0f, kHalf = 0.5f;
// RpLightGetConeAngle: the float acos of fdlibm (e_acosf.c) with the |x| > 1 case folded into 0 / pi
static const float kPi = 3.1415927f, kPio2 = 1.5707964f;		// 0x5de1f8, 0x5de27c
static const float kPiHi = 3.1415925f, kPio2Hi = 1.5707963f, kPio2Lo = 7.5497894e-08f;	// 0x5ea2c0, 0x5ea2c4, 0x5ea2c8
static const float kPS0 = 0.16666667f, kPS1 = 0.32556581f, kPS2 = 0.20121253f, kPS3 = 0.040055536f, kPS4 = 0.000791535f, kPS5 = 3.479331e-05f;
static const float kQS1 = 2.403395f, kQS2 = 2.0209458f, kQS3 = 0.688284f, kQS4 = 0.077038154f;

typedef void *(__cdecl *ErrorSetFn)(void *error);
typedef X (__cdecl *SqrtFn)(float x);			// returns st0 as the callee leaves it
typedef X (__cdecl *LengthFn)(const void *v);
static ErrorSetFn g_errorSet;
static SqrtFn g_sqrt;
static LengthFn g_length;
static int32_t *g_pvsOffset;

static void need(const void *p){ if(!p){ fprintf(stderr, "[mflrw] rwleaf2 called before mflrw_rwleaf2_bind\n"); abort(); } }

// (pS5 z + pS4) z - pS3 ... + pS0, times z; and the denominator (qS4 z - qS3) z + qS2 ... + 1 (z = an x87 value or a float slot)
static inline X acosP(X z){ return (((((kPS5 * z + kPS4) * z - kPS3) * z + kPS2) * z - kPS1) * z + kPS0) * z; }
static inline X acosQ(X z){ return (((z * kQS4 - kQS3) * z + kQS2) * z - kQS1) * z + kOne; }

extern "C" {

void __cdecl mflrw_rwleaf2_bind(void *errorSet, void *sqrtFn, void *v3dLength, int32_t *pvsOffset){
	g_errorSet = (ErrorSetFn)errorSet; g_sqrt = (SqrtFn)sqrtFn; g_length = (LengthFn)v3dLength; g_pvsOffset = pvsOffset;
}

// 0x564b10: brush colours of the four corners (c3 = top left; c2, c0, c1 relative to it as the original's layout), the raw c3 word at +0x60,
// flags bit 0 = the colours are not all equal (byte values: every float is exact)
void *__cdecl rw37_Rt2dBrushSetRGBA(u8 *b, const u8 *c0, const u8 *c1, const u8 *c2, const u8 *c3){
	for(int k = 0; k < 4; k++) F(b, 4*k) = c3[k];
	for(int k = 0; k < 4; k++) F(b, 0x18 + 4*k) = c2[k];
	float d[4];
	for(int k = 0; k < 4; k++){ d[k] = F(b, 0x18 + 4*k) - F(b, 4*k); F(b, 0x18 + 4*k) = d[k]; }
	for(int k = 0; k < 4; k++) F(b, 0x30 + 4*k) = c0[k];
	for(int k = 0; k < 4; k++) F(b, 0x48 + 4*k) = c1[k];
	float e[4];
	for(int k = 0; k < 4; k++){ e[k] = F(b, 0x48 + 4*k) - F(b, 0x30 + 4*k); F(b, 0x48 + 4*k) = e[k]; }
	U(b, 0x60) = U(c3, 0);
	bool vary = false;
	for(int k = 0; k < 4 && !vary; k++) vary = d[k] != kZero || e[k] != kZero || F(b, 4*k) != F(b, 0x30 + 4*k);
	if(vary) U(b, 0x64) |= 1; else U(b, 0x64) &= ~1u;
	return b;
}

// 0x53d6c0: one row of a BMP into RwImage row y {flags, width, height, depth, stride, pixels, palette}: key = image depth | bmp bpp << 8.
// 1 / 4 bit: palette index (32-bit image) or the index byte; 8 bit: index or copy; 15 / 16 bit: 555 / 565 to R G B 0xff; 24 bit: B G R
// (bgr = 0) or as stored; 32 bit: 3 bytes + 0xff.  The bit readers fetch the next source byte after the last pixel too.  Else RwError 0x19d / 0x80000008, 0.
RwBool __cdecl rw37_BMPImageSetSpan(const u8 *im, int32_t y, const u8 *src, int32_t bpp, int32_t bgr){
	u8 *d = (u8 *)(uintptr_t)(U(im, 0x14) + U(im, 0x10) * (u32)y);
	const u32 *pal = (const u32 *)(uintptr_t)U(im, 0x18);
	u32 w = U(im, 4), key = U(im, 0xc) | (u32)bpp << 8;
	switch(key){
	case 0x120: case 0x104: case 0x108: case 0x420: case 0x404: case 0x408: {
		int bits = key >> 8, top = 8 - bits, mask = (1 << bits) - 1, pos = top;
		u8 cur = *src++;
		if(!w) return 1;
		for(; w; w--){
			u32 v = (u32)(cur >> pos) & mask;
			if((key & 0xff) == 0x20){ U(d, 0) = pal[v]; d += 4; } else *d++ = (u8)v;
			pos -= bits;
			if(pos < 0){ cur = *src++; pos = top; }
		}
		return 1;
	}
	case 0x808: memcpy(d, src, w); return 1;
	case 0x820: for(; w; w--, d += 4) U(d, 0) = pal[*src++]; return 1;
	case 0xf20: case 0x1020:
		for(; w; w--, d += 4, src += 2){
			u32 v = *(const u16 *)src;
			d[3] = 0xff;
			d[0] = (u8)((key == 0xf20 ? v >> 7 : v >> 8) & 0xf8);
			d[1] = (u8)(key == 0xf20 ? v >> 2 & 0xf8 : v >> 3 & 0xfc);
			d[2] = (u8)(v << 3);
		}
		return 1;
	case 0x2020:
		for(; w; w--, d += 4, src += 4){ d[0] = src[0]; d[1] = src[1]; d[3] = 0xff; d[2] = src[2]; }
		return 1;
	case 0x1820:
		if(bgr) for(; w; w--, d += 4, src += 3){ d[0] = src[0]; d[1] = src[1]; d[3] = 0xff; d[2] = src[2]; }
		else for(; w; w--, d += 4, src += 3){ d[2] = src[0]; d[1] = src[1]; d[3] = 0xff; d[0] = src[2]; }
		return 1;
	}
	int32_t e[2] = { 0x19d, (int32_t)0x80000008 };
	need((void *)g_errorSet);
	g_errorSet(e);
	return 0;
}

// 0x548920: walk the world sector tree with the box of each node; an atomic sector (type < 0) gets the box (sup at PVS +0x1c, inf at +0x28),
// the sector's bbox words (+0x48 / +0x54 -> PVS +0x34 / +0x40) and the diagonal length RwV3dLength(sup - inf) at PVS +0x4c.  A plane
// sector (type 0 / 4 / 8 = x / y / z) splits the box at its value (left: sup, right: inf); other types pass the box unchanged.
void *__cdecl rw37_PVSCalculateSectorBounds(u8 *s, const u8 *bb){
	int32_t type = I(s, 0);
	if(type < 0){
		need(g_pvsOffset); need((void *)g_length);
		u8 *p = s + *g_pvsOffset;
		U(p, 0x28) = U(bb, 0xc); U(p, 0x2c) = U(bb, 0x10); U(p, 0x30) = U(bb, 0x14);
		U(p, 0x1c) = U(bb, 0); U(p, 0x20) = U(bb, 4); U(p, 0x24) = U(bb, 8);
		U(p, 0x40) = U(s, 0x54); U(p, 0x44) = U(s, 0x58); U(p, 0x48) = U(s, 0x5c);
		U(p, 0x34) = U(s, 0x48); U(p, 0x38) = U(s, 0x4c); U(p, 0x3c) = U(s, 0x50);
		float v[3];
		v[0] = R((X)F(p, 0x1c) - F(p, 0x28)); v[1] = R((X)F(p, 0x20) - F(p, 0x2c)); v[2] = R((X)F(p, 0x24) - F(p, 0x30));
		F(p, 0x4c) = (float)g_length(v);
		return s;
	}
	u32 lb[6], rb[6];
	for(int k = 0; k < 6; k++) lb[k] = rb[k] = U(bb, 4*k);
	if(type == 0 || type == 4 || type == 8){ u32 v = fq(s + 4); lb[type >> 2] = v; rb[3 + (type >> 2)] = v; }
	rw37_PVSCalculateSectorBounds((u8 *)(uintptr_t)U(s, 8), (const u8 *)lb);
	rw37_PVSCalculateSectorBounds((u8 *)(uintptr_t)U(s, 0xc), (const u8 *)rb);
	return s;
}

// 0x4f4a70: acos(-light->minusCosAngle) (fdlibm acosf: |x| >= 1 -> 0 for x > 0 else pi, tiny -> pi/2, |x| < 0.5 rational approximation,
// else through _rwSqrt).  The result is left in st0 unrounded, as the original.
X __cdecl rw37_RpLightGetConeAngle(const u8 *light){
	u32 ib = fneg(light + 0x28);
	int32_t ix = (int32_t)ib, hx = ix & 0x7fffffff;
	float x = bf(ib);
	if(hx >= 0x3f800000) return ix > 0 ? (X)kZero : (X)kPi;
	if(hx < 0x3f000000){
		if(hx <= 0x23000000) return kPio2;
		X z = (X)x * x;
		X r = acosP(z) / acosQ(z);
		return kPio2Hi - ((X)x - (kPio2Lo - r * x));
	}
	need((void *)g_sqrt);
	if(ix < 0){
		float z = R(((X)x + kOne) * kHalf);
		X s = g_sqrt(z);
		X r = acosP(z) / acosQ(z);
		X w = r * s - kPio2Lo;
		X t = s + w;
		return kPiHi - (t + t);
	}
	float z = R(((X)kOne - x) * kHalf);
	X s = g_sqrt(z);
	float sf = R(s);
	u32 sb; memcpy(&sb, &sf, 4);
	float df = bf(sb & 0xfffff000u);
	X r = acosP(z) / acosQ(z);
	X w = r * s;
	X c = ((X)z - (X)df * df) / ((X)df + s);
	X t = w + c + df;
	return t + t;
}

// FUN_00542fe0: keyframe {prev, time, quat x y z w, translation} -> matrix (rotation from the quaternion, flags 3, pos = translation);
// products stored as floats, w*y and w*z kept on the x87 stack
void __cdecl rw37_HAnimKeyFrameToMatrix(u8 *m, const u8 *kf){
	X x = F(kf, 8), y = F(kf, 0xc);
	float Z = F(kf, 0x10), W = F(kf, 0x14);
	float xx = R(x * x), yy = R(y * y), zz = R((X)Z * Z), yz = R((X)Z * y), xz = R((X)Z * x), xy = R(y * x), wx = R((X)W * x);
	X wy = (X)W * y, wz = (X)W * Z, t;
	t = (X)zz + yy; F(m, 0) = R(kOne - (t + t));
	t = (X)xy + wz; F(m, 4) = R(t + t);
	t = (X)xz - wy; F(m, 8) = R(t + t);
	t = (X)xy - wz; F(m, 0x10) = R(t + t);
	t = (X)zz + xx; F(m, 0x14) = R(kOne - (t + t));
	t = (X)wx + yz; F(m, 0x18) = R(t + t);
	t = wy + xz; F(m, 0x20) = R(t + t);
	t = (X)yz - wx; F(m, 0x24) = R(t + t);
	t = (X)yy + xx;
	U(m, 0xc) = 3; U(m, 0x30) = 0; U(m, 0x34) = 0;
	F(m, 0x28) = R(kOne - (t + t));
	U(m, 0x38) = 0;
	U(m, 0x30) = U(kf, 0x18); U(m, 0x34) = U(kf, 0x1c); U(m, 0x38) = U(kf, 0x20);
}

} // extern "C"
