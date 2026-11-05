// PIXFMT (output/rw_next_family_2026-11-05.md row 5): the D3D9 pixel format packers / unpackers of MFL.exe, written op for op from its
// disassembly (integer only):
//   _rwD3D9RGBToPixel 0x4ede80, _rwD3D9PixelToRGB 0x4ee0d0, D3D9ConvR8G8B8A8To{A1R5G5B5 0x4ef9f0, A4R4G4B4 0x4efa30, R4G4B4 0x4efaa0,
//   L8 0x4efbd0, A8L8 0x4efc30, L16 0x4efc80}.
// RwRGBA {r, g, b, a} bytes; raster format = format & 0xf00 (0x100 1555, 0x200 565, 0x300 4444, 0x400 LUM8, 0x500 8888, 0x600 888,
// 0xa00 555; PixelToRGB copies the word for PAL8 0x2000; RGBToPixel treats 0 as 8888).  An unknown format sets the error
// {1, 0x8000000d} (_rwerror 0x53eac0 is the identity) through the exe's RwErrorSet and returns 0.
// Exe-layout adapter: mflrw_pixfmt_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over RwErrorSet 0x4ea160.
// Check: tools/delink/rw_pixfmt_check.sh (the original code and these exports natively under Wine on the same random inputs).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef void *(__cdecl *ErrorSetFn)(int32_t *err);

static ErrorSetFn g_errorSet;		// RwErrorSet 0x4ea160

static int32_t badFormat(void){
	if(!g_errorSet){ fprintf(stderr, "[mflrw] pixfmt called before mflrw_pixfmt_bind\n"); abort(); }
	int32_t err[2] = {1, (int32_t)0x8000000d};
	g_errorSet(err);
	return 0;
}
// 30 r + 59 g + 11 b, then / 100 (signed, truncating)
static inline int32_t lum(const u8 *c){ return (30*c[0] + 59*c[1] + 11*c[2])/100; }
static inline u32 p1555(const u8 *c){ return (((((u32)c[3] & 0x80) << 1 | (c[0] & 0xf8)) << 5 | (c[1] & 0xf8)) << 2) | c[2] >> 3; }
static inline u32 p4444(const u8 *c){ return ((((u32)c[3] & 0xf0) << 4 | (c[0] & 0xf0)) << 4) | (c[1] & 0xf0) | c[2] >> 4; }

extern "C" {

void __cdecl mflrw_pixfmt_bind(void *errorSet){ g_errorSet = (ErrorSetFn)errorSet; }

u32 __cdecl rw37_D3D9ConvR8G8B8A8ToA1R5G5B5(const u8 *c){ return p1555(c); }
u32 __cdecl rw37_D3D9ConvR8G8B8A8ToA4R4G4B4(const u8 *c){ return p4444(c); }
u32 __cdecl rw37_D3D9ConvR8G8B8A8ToR4G4B4(const u8 *c){ return ((0xf00u | (c[0] & 0xf0)) << 4) | (c[1] & 0xf0) | c[2] >> 4; }
u32 __cdecl rw37_D3D9ConvR8G8B8A8ToL8(const u8 *c){ return (u32)lum(c)*c[3]/255u; }
u32 __cdecl rw37_D3D9ConvR8G8B8A8ToA8L8(const u8 *c){ return (u32)lum(c) | (u32)c[3] << 8; }
u32 __cdecl rw37_D3D9ConvR8G8B8A8ToL16(const u8 *c){ int32_t t = (30*c[0] + 59*c[1] + 11*c[2])*(int32_t)c[3]/25500; return (u32)t*255u; }

int32_t __cdecl rw37__rwD3D9RGBToPixel(u32 *out, const u8 *c, u32 format){
	switch(format & 0xf00){
	case 0: { u32 p; rw37__rwD3D9RGBToPixel(&p, c, 0x500); *out = p; return 1; }
	case 0x100: *out = p1555(c); return 1;
	case 0x200: *out = ((((u32)c[0] & 0xf8) << 5 | (c[1] & 0xfc)) << 3) | c[2] >> 3; return 1;
	case 0x300: *out = p4444(c); return 1;
	case 0x400: *out = (u32)lum(c)*c[3]/255u; return 1;
	case 0x500: *out = (((u32)c[3] << 8 | c[0]) << 8 | c[1]) << 8 | c[2]; return 1;
	case 0x600: *out = 0xff000000u | (u32)c[0] << 16 | (u32)c[1] << 8 | c[2]; return 1;
	case 0xa00: *out = ((((u32)c[0] & 0xf8) << 5 | (c[1] & 0xf8)) << 2) | c[2] >> 3; return 1;
	}
	return badFormat();
}

int32_t __cdecl rw37__rwD3D9PixelToRGB(u8 *o, const u32 *in, u32 format){
	if(format & 0x2000){ *(u32 *)o = *in; return 1; }
	u32 p;
	switch(format & 0xf00){
	case 0x100: p = *in; o[3] = p & 0x8000 ? 0xff : 0; o[0] = (p >> 7) & 0xf8; o[2] = (u8)(p << 3); o[1] = (p >> 2) & 0xf8; return 1;
	case 0x200: p = *in; o[3] = 0xff; o[0] = (p >> 8) & 0xf8; o[2] = (u8)(p << 3); o[1] = (p >> 3) & 0xfc; return 1;
	case 0x300: p = *in; o[3] = (p & 0xf000) == 0xf000 ? 0xff : (p >> 8) & 0xf0; o[0] = (p >> 4) & 0xf0; o[2] = (u8)(p << 4); o[1] = p & 0xf0; return 1;
	case 0x400: p = *in; o[3] = 0xff; o[0] = o[1] = o[2] = (u8)p; return 1;
	case 0x500: p = *in; o[2] = (u8)p; o[0] = (u8)(p >> 16); o[3] = (u8)(p >> 24); o[1] = (u8)(p >> 8); return 1;
	case 0x600: p = *in; o[3] = 0xff; o[2] = (u8)p; o[0] = (u8)(p >> 16); o[1] = (u8)(p >> 8); return 1;
	case 0xa00: p = *in; o[3] = 0xff; o[0] = (p >> 7) & 0xf8; o[2] = (u8)(p << 3); o[1] = (p >> 2) & 0xf8; return 1;
	}
	return badFormat();
}

} // extern "C"
