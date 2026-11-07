// D3DCONV (plan_engine_switch_2026-11-07_v2 X1): the still-original image <-> raster pixel conversion functions of d3d9convrt.obj in MFL.exe,
// written op for op from the disassembly, on the exe's own RwImage / RwRaster objects (RW 3.7 layout, rw37.h) and D3D9 raster extension:
//   _rwD3D9CheckRasterSize 0x4eddd0, _rwD3D9ImageGetFromRaster 0x4ee2d0, D3D9Image32GetFromRaster 0x4ee4b0, D3D9Image32GetFromD3D9Raster 0x4ee920,
//   D3D9ConvA1R5G5B5ToR8G8B8A8 0x4eecd0, D3D9ConvA4R4G4B4ToR8G8B8A8 0x4eed10, _rwD3D9ImageFindFormat 0x4eef60, _rwD3D9ImageFindRasterFormat 0x4ef150
//   (exe label LAB_004ef150), _rwD3D9RasterSetFromImage 0x4ef240, D3D9SelectConvertFn 0x4ef870, D3D9RasterPalletizedSetFromImage 0x4efce0,
//   D3D9Raster24_32LinearSetFromImage 0x4f03f0.
// No device call: the raster lock / palette lock / unlock, the format checks, the image create / resample / destroy and the palette quantiser are
// exe functions; the per-pixel converters are called through the exe's own addresses (the original's function pointer tables).
// Exe-layout adapter: mflrw_d3dconv_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612.c), table layout = enum B below.
// Behaviour kept from the original: width 0 rows loop 2^32 times in Image32GetFrom*Raster (do-while), an unknown D3D format calls a null converter,
// a 4 bit image reads the uninitialised lookup entries 16..255 for pixel values above 15, Raster24_32 / Palletized return nothing.
// Check: tools/delink/rw_d3dconv_check.sh (the original code and these exports natively under Wine on the same random images and rasters).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rw37.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

typedef RwRaster37 Ras;
typedef RwImage37 Img;
typedef RwD3D9RasterExt37 Ext;
typedef void *(__cdecl *ErrorSetFn)(i32 *err);
typedef i32 (__cdecl *ErrCodeFn)(i32 code, ...);
typedef u8 *(__cdecl *LockFn)(Ras *r, i32 level, i32 mode);
typedef u8 *(__cdecl *LockPalFn)(Ras *r, i32 mode);
typedef Ras *(__cdecl *UnlockFn)(Ras *r);
typedef i32 (__cdecl *ValidFmtFn)(i32 d3dFormat);
typedef i32 (__cdecl *RasFmtFn)(Ras *r, u32 flags);
typedef Img *(__cdecl *ResampleFn)(Img *img, i32 w, i32 h);
typedef Img *(__cdecl *ImgCreateFn)(i32 w, i32 h, i32 depth);
typedef Img *(__cdecl *ImgFn)(Img *img);
typedef i32 (__cdecl *PqInitFn)(void *pq);
typedef void (__cdecl *PqAddFn)(void *pq, Img *img, float weight);
typedef i32 (__cdecl *PqResolveFn)(u8 *palette, i32 n, void *pq);
typedef void (__cdecl *PqMatchFn)(u8 *pixels, i32 stride, i32 depth, i32 dither, void *pq, Img *img);
typedef void (__cdecl *PqTermFn)(void *pq);
typedef u32 (__cdecl *ConvFn)(const u8 *rgba);
typedef void (__cdecl *UnconvFn)(u8 *rgba, u32 pixel);

// bind table layout (an_hk_612.c builds it in this order)
enum B { B_ERRORSET, B_ERRCODE, B_LOCK, B_LOCKPAL, B_UNLOCK, B_UNLOCKPAL, B_VALIDFMT, B_RASFMT, B_RESAMPLE, B_IMGCREATE, B_IMGALLOC, B_IMGDESTROY,
         B_PQINIT, B_PQADD, B_PQRESOLVE, B_PQMATCH, B_PQTERM, B_EXTOFF, B_PIXFMTINFO, B_CONVTABLE, B_DISPDEPTH, B_CAPS,
         B_STR_PAL4, B_STR_NONPAL, B_STR_FROMCOMP, B_STR_TOCOMP, B_TOX, B_TORGBA = B_TOX + 15, B_N = B_TORGBA + 18 };
static void *const *g;

static inline void *fn(int i){
	if(!g){ fprintf(stderr, "[mflrw] d3dconv called before mflrw_d3dconv_bind\n"); abort(); }
	return g[i];
}
#define CALL(i, T) ((T)fn(i))
static inline i32 extOff(void){ return *(i32 *)fn(B_EXTOFF); }
static inline Ext *EXT(Ras *r){ return (Ext *)((u8 *)r + extOff()); }
static inline u8 *caps(void){ return (u8 *)fn(B_CAPS); }	// _RwD3D9DeviceCaps (D3DCAPS9): TextureCaps 0x3c, MaxTextureWidth / Height 0x58 / 0x5c
static inline i32 dispDepth(void){ return *(i32 *)fn(B_DISPDEPTH); }
static inline u8 fmtAlpha(u32 d3dFormat){ return ((u8 *)fn(B_PIXFMTINFO))[d3dFormat*4]; }	// _rwD3D9PixelFormatInfo[fmt].alpha
static void error(i32 code){ i32 e[2] = {1, CALL(B_ERRCODE, ErrCodeFn)(code)}; CALL(B_ERRORSET, ErrorSetFn)(e); }
static void error2(int str){ i32 e[2] = {1, CALL(B_ERRCODE, ErrCodeFn)(2, fn(str))}; CALL(B_ERRORSET, ErrorSetFn)(e); }

// D3D9SelectConvertFn's switch (also inlined in D3D9Raster24_32LinearSetFromImage): converter index into the B_TOX block (address order 0x4ef990..0x4efc80)
static int toxIndex(u32 f){
	switch(f){
	case 0x15: return 9; case 0x16: return 7; case 0x17: return 1; case 0x18: return 0; case 0x19: return 2; case 0x1a: return 3; case 0x1d: return 4;
	case 0x1e: return 5; case 0x32: return 12; case 0x33: return 13; case 0x51: return 14; case 0x3c: case 0x75: return 11; case 0x3d: return 6;
	case 0x20: case 0x3f: return 10; case 0x21: case 0x3e: return 8;
	}
	return -1;
}
static ConvFn selectConv(u32 f){ int i = toxIndex(f); return i < 0 ? 0 : (ConvFn)fn(B_TOX + i); }
// D3D9Image32GetFromD3D9Raster's switch: index into the B_TORGBA block (address order 0x4eec40..0x4eef40)
static UnconvFn selectUnconv(u32 f){
	int i;
	switch(f){
	case 0x18: i = 0; break; case 0x17: i = 1; break; case 0x19: i = 3; break; case 0x1a: i = 4; break; case 0x32: i = 13; break; case 0x1c: i = 14; break;
	case 0x16: i = 8; break; case 0x15: i = 11; break; case 0x1d: i = 5; break; case 0x1b: i = 6; break; case 0x1e: i = 7; break; case 0x34: i = 15; break;
	case 0x33: i = 16; break; case 0x51: i = 17; break; case 0x3c: case 0x75: i = 10; break; case 0x3d: i = 2; break; case 0x20: case 0x3f: i = 12; break;
	case 0x21: case 0x3e: i = 9; break;
	default: return 0;
	}
	return (UnconvFn)fn(B_TORGBA + i);
}
// _rwD3D9FindMSB inlined: 1 << msb (x86 shift count & 31, so 0 gives 0x80000000; a negative value never ends, as in the original)
static inline i32 pow2floor(i32 v){ i32 c = -1; while(v){ c++; v >>= 1; } return (i32)(1u << (c & 31)); }
static inline i32 lum100(const u8 *p){ return (30*p[0] + 59*p[1] + 11*p[2])/100; }

extern "C" {

void __cdecl mflrw_d3dconv_bind(i32 n, void *const *table){
	if(n != B_N){ fprintf(stderr, "[mflrw] mflrw_d3dconv_bind: %d entries, expected %d\n", n, B_N); abort(); }
	g = table;
}

void __cdecl rw37_D3D9ConvA1R5G5B5ToR8G8B8A8(u8 *o, u32 p){
	o[3] = p & 0x8000 ? 0xff : 0; o[0] = (p >> 7) & 0xf8; o[1] = (p >> 2) & 0xf8; o[2] = (u8)(p << 3);
}
void __cdecl rw37_D3D9ConvA4R4G4B4ToR8G8B8A8(u8 *o, u32 p){
	o[3] = (p & 0xf000) == 0xf000 ? 0xff : (p >> 8) & 0xf0; o[0] = (p >> 4) & 0xf0; o[1] = p & 0xf0; o[2] = (u8)(p << 4);
}

void __cdecl rw37__rwD3D9CheckRasterSize(i32 *w, i32 *h, u32 flags){
	if(*w > *(i32 *)(caps() + 0x58)) *w = *(i32 *)(caps() + 0x58);
	if(*h > *(i32 *)(caps() + 0x5c)) *h = *(i32 *)(caps() + 0x5c);
	if((flags & 7) && (flags & 0x8000)){
		if(!(caps()[0x3c] & 2)) goto square;
	}else{
		u32 tc = *(u32 *)(caps() + 0x3c);
		if(!(tc & 2) || (tc & 0x100)) goto square;
	}
	*w = pow2floor(*w);
	*h = pow2floor(*h);
square:
	if(caps()[0x3c] & 0x20){ i32 a = *w, b = *h; if(a < b) *h = a; else *w = b; }
}

u32 __cdecl rw37__rwD3D9ImageFindFormat(Img *img){
	i32 depth = img->depth;
	if(depth == 4 || depth == 8){
		i32 w = img->width, h = img->height, n = depth == 4 ? 16 : 256, i;
		u8 *pal = img->palette, *px = img->cpPixels;
		for(i = 0; i < n; i++) if(pal[4*i + 3] != 0xff) goto pixels;
		return depth == 4 ? 0x4600 : 0x2600;
	pixels:
		for(i32 y = 0; y < h; y++){
			for(i32 x = 0; x < w; x++) if(pal[4*px[x] + 3] != 0xff) return depth == 4 ? 0x4500 : 0x2500;
			px += img->stride;
		}
		return depth == 4 ? 0x4600 : 0x2600;
	}
	i32 w = img->width, a1 = 0;
	u8 *px = img->cpPixels;
	for(i32 y = 0; y < img->height; y++){
		const u8 *p = px + 3;
		for(i32 x = 0; x < w; x++, p += 4){ u8 a = *p; if(a < 0xff){ if(a > 0xf) goto full; a1 = 1; } }
		px += img->stride;
	}
	if(!a1){
		if(dispDepth() > 0x10) return CALL(B_VALIDFMT, ValidFmtFn)(0x16) ? 0x600 : 0x200;
		return 0x200;
	}
	if(dispDepth() > 0x10) return CALL(B_VALIDFMT, ValidFmtFn)(0x15) ? 0x500 : 0x100;
	return 0x100;
full:
	if(dispDepth() > 0x10 && CALL(B_VALIDFMT, ValidFmtFn)(0x15)) return 0x500;
	return CALL(B_VALIDFMT, ValidFmtFn)(0x1a) ? 0x300 : 0x100;
}

i32 __cdecl rw37__rwD3D9ImageFindRasterFormat(Ras *ras, Img *img, u32 flags){
	ras->width = img->width; ras->depth = 0; ras->height = img->height;
	u32 t = flags & 7;
	switch(t){
	case 0:
		rw37__rwD3D9CheckRasterSize(&ras->width, &ras->height, flags);
		return CALL(B_RASFMT, RasFmtFn)(ras, rw37__rwD3D9ImageFindFormat(img) | (flags & ~0x9000u));
	case 1: case 2:
		return CALL(B_RASFMT, RasFmtFn)(ras, flags);
	case 4: case 5:
		rw37__rwD3D9CheckRasterSize(&ras->width, &ras->height, flags);
		return CALL(B_RASFMT, RasFmtFn)(ras, rw37__rwD3D9ImageFindFormat(img) | (flags & 0x9000) | t);
	}
	error((i32)0x8000000d);
	return 0;
}

ConvFn __cdecl rw37_D3D9SelectConvertFn(u32 d3dFormat){ return selectConv(d3dFormat); }

void __cdecl rw37_D3D9Image32GetFromD3D9Raster(Img *img, Ras *ras){
	UnconvFn conv = selectUnconv(EXT(ras->parent)->d3dFormat);
	switch(ras->depth){
	case 4: case 8:
		for(i32 y = 0; y < ras->height; y++){
			const u8 *s = ras->cpPixels + ras->stride*y; u8 *d = img->cpPixels + img->stride*y; u32 n = ras->width;
			do { conv(d, *s); s++; d += 4; } while(--n);
		}
		return;
	case 16:
		for(i32 y = 0; y < ras->height; y++){
			const u16 *s = (const u16 *)(ras->cpPixels + ras->stride*y); u8 *d = img->cpPixels + img->stride*y; u32 n = ras->width;
			do { conv(d, *s); s++; d += 4; } while(--n);
		}
		return;
	case 32:
		for(i32 y = 0; y < ras->height; y++){
			const u32 *s = (const u32 *)(ras->cpPixels + ras->stride*y); u8 *d = img->cpPixels + img->stride*y; u32 n = ras->width;
			do { conv(d, *s); s++; d += 4; } while(--n);
		}
		return;
	}
	error((i32)0x8000000c);
}

void __cdecl rw37_D3D9Image32GetFromRaster(Img *img, Ras *ras){
	u32 fmt = (ras->cFormat & 0xf) << 8;
	if(!fmt){ rw37_D3D9Image32GetFromD3D9Raster(img, ras); return; }
	switch(ras->depth){
	case 4: case 8: {
		const u32 *pal = (const u32 *)ras->palette;
		for(i32 y = 0; y < ras->height; y++){
			const u8 *s = ras->cpPixels + ras->stride*y; u32 *d = (u32 *)(img->cpPixels + img->stride*y); u32 n = ras->width;
			if(pal) do { *d++ = pal[*s++]; } while(--n);
			else do { u32 b = *s++; *d++ = 0xff000000u | b << 16 | b << 8 | b; } while(--n);
		}
		return;
	}
	case 16:
		if(fmt != 0x100 && fmt != 0x200 && fmt != 0x300 && fmt != 0xa00) return;
		for(i32 y = 0; y < ras->height; y++){
			const u16 *s = (const u16 *)(ras->cpPixels + ras->stride*y); u8 *d = img->cpPixels + img->stride*y; u32 n = ras->width;
			do {
				u32 p = *s++;
				switch(fmt){
				case 0x200: d[3] = 0xff; d[0] = (p >> 8) & 0xf8; d[1] = (p >> 3) & 0xfc; d[2] = (u8)(p << 3); break;
				case 0x100: d[0] = (p >> 7) & 0xf8; d[1] = (p >> 2) & 0xf8; d[2] = (u8)(p << 3); d[3] = p & 0x8000 ? 0xff : 0; break;
				case 0x300: d[0] = (p >> 4) & 0xf0; d[1] = p & 0xf0; d[2] = (u8)(p << 4); d[3] = (p & 0xf000) == 0xf000 ? 0xff : (p >> 8) & 0xf0; break;
				default: d[3] = 0xff; d[0] = (p >> 7) & 0xf8; d[1] = (p >> 2) & 0xf8; d[2] = (u8)(p << 3); break;	// 0xa00
				}
				d += 4;
			} while(--n);
		}
		return;
	case 24: case 32: {
		u32 f = EXT(ras->parent)->d3dFormat;
		if(f != 0x16 && f != 0x15 && f != 0x23){ rw37_D3D9Image32GetFromD3D9Raster(img, ras); return; }
		for(i32 y = 0; y < ras->height; y++){
			const u32 *s = (const u32 *)(ras->cpPixels + ras->stride*y); u8 *d = img->cpPixels + img->stride*y; u32 n = ras->width;
			do {
				u32 p = *s++;
				if(f == 0x16){ d[3] = 0xff; d[2] = (u8)p; d[0] = (u8)(p >> 16); d[1] = (u8)(p >> 8); }
				else if(f == 0x15){ d[2] = (u8)p; d[0] = (u8)(p >> 16); d[1] = (u8)(p >> 8); d[3] = (u8)(p >> 24); }
				else { d[0] = (u8)(p >> 22); d[1] = (u8)(p >> 12); d[2] = (u8)(p >> 2); d[3] = (u8)((p >> 30)*0x55); }	// A2R10G10B10
				d += 4;
			} while(--n);
		}
		return;
	}
	}
	error((i32)0x8000000c);
}

i32 __cdecl rw37__rwD3D9ImageGetFromRaster(Img *img, Ras *ras){
	if(EXT(ras)->autoMipCompressed & 0xf0){ error2(B_STR_FROMCOMP); return 0; }
	i32 locked = 0, palLocked = 0;
	if(!(ras->privateFlags & 2)){ CALL(B_LOCK, LockFn)(ras, 0, 2); locked = 1; }
	if((ras->cFormat & 0x60) && !(ras->privateFlags & 8)){ CALL(B_LOCKPAL, LockPalFn)(ras, 2); palLocked = 1; }
	if(img->depth == 32) rw37_D3D9Image32GetFromRaster(img, ras);
	else if(img->depth != 8) error2(B_STR_PAL4);
	else if(ras->depth != 8 || !(ras->cFormat & 0xf)) error2(B_STR_NONPAL);
	else {
		if(img->stride == ras->stride && ras->width == ras->stride) memcpy(img->cpPixels, ras->cpPixels, (u32)(ras->height*ras->width));
		else for(i32 y = 0; y < ras->height; y++) memcpy(img->cpPixels + img->stride*y, ras->cpPixels + ras->stride*y, (u32)ras->width);
		if(ras->palette) memcpy(img->palette, ras->palette, 0x400);
		else for(int i = 0; i < 256; i++){ img->palette[4*i] = (u8)i; img->palette[4*i + 1] = (u8)i; img->palette[4*i + 2] = (u8)i; img->palette[4*i + 3] = 0xff; }
	}
	if(palLocked == 1) CALL(B_UNLOCKPAL, UnlockFn)(ras);
	if(locked == 1) CALL(B_UNLOCK, UnlockFn)(ras);
	return 1;
}

void __cdecl rw37_D3D9RasterPalletizedSetFromImage(Ras *ras, Img *img){
	Ext *ext = EXT(ras);
	u8 lut[256];
	if((ras->cFormat & 0xf) == 4){	// LUM8: one byte per pixel from the image's colours
		u32 f = ext->d3dFormat;
		int kind = f == 0x1b ? 0 : f == 0x34 ? 1 : f == 0x1c ? 2 : 3;	// R3G3B2, A4L4, A8, else luminance * alpha
		if(img->depth == 4 || img->depth == 8){
			i32 n = 1 << img->depth;
			for(i32 i = 0; i < n; i++){
				const u8 *p = img->palette + 4*i;
				lut[i] = kind == 0 ? (u8)((((p[1] & 0xe7) | (p[2] >> 3)) >> 3) | (p[0] & 0xe0)) : kind == 1 ? (u8)((u8)(lum100(p) >> 4) | (p[3] & 0xf0))
				       : kind == 2 ? p[3] : (u8)((u32)(lum100(p)*p[3])/255u);
			}
			for(i32 y = 0; y < ras->height; y++){
				const u8 *s = img->cpPixels + img->stride*y; u8 *d = ras->cpPixels + ras->stride*y;
				for(i32 x = 0; x < ras->width; x++) d[x] = lut[s[x]];
			}
			return;
		}
		for(i32 y = 0; y < ras->height; y++){
			const u8 *p = img->cpPixels + img->stride*y; u8 *d = ras->cpPixels + ras->stride*y;
			for(i32 x = 0; x < ras->width; x++, p += 4)
				d[x] = kind == 0 ? (u8)((((p[1] & 0xe7) | (p[2] >> 3)) >> 3) | (p[0] & 0xe0)) : kind == 1 ? (u8)((u8)(lum100(p) >> 4) | (p[3] & 0xf0))
				     : kind == 2 ? p[3] : (u8)((u32)(lum100(p)*p[3])/255u);
		}
		return;
	}
	if(img->depth != 4 && img->depth != 8){	// quantise into a new palettised image of the raster's depth, then copy that
		alignas(4) u8 pq[0x3ff8];
		i32 depth = ras->depth;
		Img *pi = CALL(B_IMGCREATE, ImgCreateFn)(img->width, img->height, depth);
		if(!pi) return;
		CALL(B_IMGALLOC, ImgFn)(pi);
		if(!CALL(B_PQINIT, PqInitFn)(pq)) return;
		CALL(B_PQADD, PqAddFn)(pq, img, 1.0f);
		CALL(B_PQRESOLVE, PqResolveFn)(pi->palette, 1 << depth, pq);
		CALL(B_PQMATCH, PqMatchFn)(pi->cpPixels, pi->stride, pi->depth, 0, pq, img);
		CALL(B_PQTERM, PqTermFn)(pq);
		if(ext->alpha && (rw37__rwD3D9ImageFindFormat(pi) & 0xf00) == 0x600){ ras->cFormat = (ras->cFormat & 0xfe) | 6; ext->alpha = 0; }
		rw37_D3D9RasterPalletizedSetFromImage(ras, pi);
		CALL(B_IMGDESTROY, ImgFn)(pi);
		return;
	}
	if(img->stride == ras->stride && ras->width == ras->stride) memcpy(ras->cpPixels, img->cpPixels, (u32)(ras->height*ras->width));
	else for(i32 y = 0; y < ras->height; y++) memcpy(ras->cpPixels + ras->stride*y, img->cpPixels + img->stride*y, (u32)ras->width);
	if(ext->lockedMipLevel) return;
	if(!ras->palette) return;
	memcpy(ras->palette, img->palette, (u32)(4 << img->depth));
}

void __cdecl rw37_D3D9Raster24_32LinearSetFromImage(Ras *ras, Img *img){
	Ext *ext = EXT(ras);
	u32 fmt = (ras->cFormat & 0xf) << 8;
	u32 lut[256];
	u8 alpha;
	if(img->depth == 4 || img->depth == 8){
		if(fmt == 0x600){
			alpha = 0xff;
			for(i32 i = 0, n = 1 << img->depth; i < n; i++){ const u8 *p = img->palette + 4*i; lut[i] = 0xff000000u | (u32)p[0] << 16 | (u32)p[1] << 8 | p[2]; }
		}else if(fmt == 0x500){
			alpha = 0;
			for(i32 i = 0, n = 1 << img->depth; i < n; i++){ const u8 *p = img->palette + 4*i; lut[i] = (u32)p[3] << 24 | (u32)p[0] << 16 | (u32)p[1] << 8 | p[2]; }
		}else{
			u32 f = ext->d3dFormat;
			ConvFn conv = selectConv(f);
			alpha = fmtAlpha(f);
			for(i32 i = 0; i < 1 << img->depth; i++) lut[i] = conv(img->palette + 4*i);
		}
		for(i32 y = 0; y < ras->height; y++){
			const u8 *s = img->cpPixels + img->stride*y; u32 *d = (u32 *)(ras->cpPixels + ras->stride*y);
			for(i32 x = 0; x < ras->width; x++) d[x] = lut[s[x]];
		}
		if(alpha >= 0xff){ ext->alpha = 0; return; }
	}else if(img->depth == 32){
		if(fmt == 0x600){
			for(i32 y = 0; y < ras->height; y++){
				const u8 *p = img->cpPixels + img->stride*y; u32 *d = (u32 *)(ras->cpPixels + ras->stride*y);
				for(i32 x = 0; x < ras->width; x++, p += 4) d[x] = 0xff000000u | (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
			}
			ext->alpha = 0;
			return;
		}
		if(fmt == 0x500){
			for(i32 y = 0; y < ras->height; y++){
				const u8 *p = img->cpPixels + img->stride*y; u32 *d = (u32 *)(ras->cpPixels + ras->stride*y);
				for(i32 x = 0; x < ras->width; x++, p += 4) d[x] = (u32)p[3] << 24 | (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
			}
		}else{
			u32 f = ext->d3dFormat;
			ConvFn conv = selectConv(f);
			alpha = fmtAlpha(f);
			for(i32 y = 0; y < ras->height; y++){
				const u8 *p = img->cpPixels + img->stride*y; u32 *d = (u32 *)(ras->cpPixels + ras->stride*y);
				for(i32 x = 0; x < ras->width; x++, p += 4) d[x] = conv(p);
			}
			if(alpha >= 0xff){ ext->alpha = 0; return; }
		}
	}
	ext->alpha = 1;
	if(ras != ras->parent) EXT(ras->parent)->alpha = 1;
}

i32 __cdecl rw37__rwD3D9RasterSetFromImage(Ras *ras, Img *image, i32 unused){
	Ext *ext = EXT(ras);
	i32 rasterLocked = 0, paletteLocked = 0;
	if(ext->autoMipCompressed & 0xf0){ error2(B_STR_TOCOMP); return 0; }
	u32 format = (u32)ras->cFormat << 8;
	if(ras->cType == 0) ext->alpha = ((u8 *)fn(B_CONVTABLE))[(((i32)rw37__rwD3D9ImageFindFormat(image) >> 8) & 0xf)*8 + 5];	// _rwRasterConvTable[].alpha
	else if(ext->d3dFormat == 0x29) ext->alpha = (format & 0xf00) == 0x500 ? 1 : 0;	// D3DFMT_P8
	else ext->alpha = fmtAlpha(ext->d3dFormat);
	if(ext->alpha && ras != ras->parent) EXT(ras->parent)->alpha = 1;
	Img *img = image;
	if(ras->width != image->width || ras->height != image->height) img = CALL(B_RESAMPLE, ResampleFn)(image, ras->width, ras->height);
	if(ras->privateFlags & 4) rasterLocked = 1;
	else if(!CALL(B_LOCK, LockFn)(ras, 0, 5)) return 0;
	u32 pal = format & 0x6000;
	if(pal){
		if(ras->privateFlags & 0x10) paletteLocked = 1;
		else if(!CALL(B_LOCKPAL, LockPalFn)(ras, 5)) return 0;
	}
	switch(ras->depth){
	case 4: case 8:
		rw37_D3D9RasterPalletizedSetFromImage(ras, img);
		break;
	case 16:
		if(img->depth == 4 || img->depth == 8){
			ConvFn conv = selectConv(EXT(ras)->d3dFormat);
			u16 lut[256];
			if(img->depth == 4) for(int i = 0; i < 16; i++) lut[i] = (u16)conv(img->palette + 4*i);
			else for(int i = 0; i < 256; i++) lut[i] = (u16)conv(img->palette + 4*i);
			for(i32 y = 0; y < ras->height; y++){
				const u8 *s = img->cpPixels + img->stride*y; u16 *d = (u16 *)(ras->cpPixels + ras->stride*y);
				for(i32 x = 0; x < ras->width; x++) d[x] = lut[s[x]];
			}
		}else if(img->depth == 32){
			u32 f = EXT(ras)->d3dFormat;
			ConvFn conv = f >= 0x17 && f <= 0x1a ? 0 : rw37_D3D9SelectConvertFn(f);
			for(i32 y = 0; y < ras->height; y++){
				const u8 *p = img->cpPixels + img->stride*y; u16 *d = (u16 *)(ras->cpPixels + ras->stride*y);
				for(i32 x = 0; x < ras->width; x++, p += 4){
					switch(f){
					case 0x18: d[x] = (u16)(((p[0] & 0xf8) << 5 | (p[1] & 0xf8)) << 2 | p[2] >> 3); break;	// X1R5G5B5
					case 0x17: d[x] = (u16)(((p[0] & 0xf8) << 5 | (p[1] & 0xfc)) << 3 | p[2] >> 3); break;	// R5G6B5
					case 0x19: d[x] = (u16)((((p[3] & 0x80) << 1 | (p[0] & 0xf8)) << 5 | (p[1] & 0xf8)) << 2 | p[2] >> 3); break;	// A1R5G5B5
					case 0x1a: d[x] = (u16)(((p[3] & 0xf0) << 4 | (p[0] & 0xf0)) << 4 | (p[1] & 0xf0) | p[2] >> 4); break;	// A4R4G4B4
					default: d[x] = (u16)conv(p);
					}
				}
			}
		}
		break;
	case 24: case 32:
		rw37_D3D9Raster24_32LinearSetFromImage(ras, img);
		break;
	case 0x40: case 0x80:
		break;
	default:
		error((i32)0x8000000c);
	}
	if(pal && !paletteLocked) CALL(B_UNLOCKPAL, UnlockFn)(ras);
	if(!rasterLocked) CALL(B_UNLOCK, UnlockFn)(ras);
	if(img != image) CALL(B_IMGDESTROY, ImgFn)(img);
	return 1;
}

} // extern "C"
