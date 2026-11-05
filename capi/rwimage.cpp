// IMAGE (LIB_RW group image; output/rw_next_family_2026-11-05.md row 6, without the resampler): the RwImage pixel leafs of MFL.exe, written
// op for op from its disassembly (integer only):
//   RwImageMakeMask 0x4dd700, RwImageApplyMask 0x4dd7d0, ImageStraightCopy 0x4de3a0, ImageConvertDepth 0x4de450, RwImageCopy 0x4de560,
//   RwImageGammaCorrect 0x4de880.
// RwImage {flags (1 owns pixels, 2 gamma corrected), width, height, depth, stride, cpPixels, palette}.  Depth conversions: 4 / 8 -> 8 copy the
// index bytes, 4 / 8 -> 32 expand through the palette; the gamma table is the image module's (engine + imageModule + 0xc).
// Exe-layout adapter: mflrw_image_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance 0x7d2128,
// &imageModule 0x7d2724 (image free list, gamma table), imageTKList 0x62e848, _rwPluginRegistryInitObject 0x4e9ce0 / DeInitObject 0x4e9d40,
// RwImageAllocatePixels 0x4dd610, RwErrorSet 0x4ea160.
// Check: tools/delink/rw_image_check.sh (the original code and these exports natively under Wine, same harness heap).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint32_t u32;

struct Image { int32_t flags, width, height, depth, stride; u8 *pixels, *palette; };

typedef void *(__cdecl *FlAllocFn)(void *fl, u32 hint);
typedef void *(__cdecl *FlFreeFn)(void *fl, void *p);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *RegFn)(void *registry, void *object);
typedef Image *(__cdecl *AllocPixFn)(Image *image);
typedef void *(__cdecl *ErrorSetFn)(int32_t *err);

// ---- adapter
static u8 **g_engine;			// &RwEngineInstance 0x7d2128
static int32_t *g_imageModule;		// 0x7d2724
static void *g_imageTK;			// imageTKList 0x62e848
static RegFn g_initObject, g_deInitObject;	// _rwPluginRegistryInitObject 0x4e9ce0, _rwPluginRegistryDeInitObject 0x4e9d40
static AllocPixFn g_allocPixels;	// RwImageAllocatePixels 0x4dd610
static ErrorSetFn g_errorSet;		// RwErrorSet 0x4ea160
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] image called before mflrw_image_bind\n"); abort(); }
	return *g_engine;
}
static void err(int32_t code){ int32_t e[2] = {1, code}; g_errorSet(e); }	// _rwerror 0x53eac0 returns its first argument
static void *&imageFL(void){ return *(void **)(eng() + *g_imageModule); }
static inline u8 max3(u8 r, u8 g, u8 b){ u8 m = g > r ? g : r; return b > m ? b : m; }
// flags bit 2 (gamma corrected) taken over from src: two stores like the original (dst == src loses the bit)
static inline void takeGamma(Image *dst, const Image *src){ int32_t f = dst->flags & ~2; dst->flags = f; dst->flags = (src->flags & 2) | f; }
// palette (when both have one and src->depth <= 8) and the rows of dst's size
static void copyRows(Image *dst, const Image *src){
	if(dst->palette && src->palette && src->depth <= 8) memcpy(dst->palette, src->palette, (1u << (src->depth & 31)) << 2);
	u32 row = (u32)(((dst->depth + 7) >> 3) * dst->width);
	const u8 *s = src->pixels;
	u8 *d = dst->pixels;
	for(int32_t y = 0; y < dst->height; y++){ memcpy(d, s, row); d += dst->stride; s += src->stride; }
}
// 4 / 8 -> 8: index bytes; 4 / 8 -> 32: palette expansion; same depth: nothing; returns 0 after the error for any other pair
static int convert(u8 *dp, const Image *dst, const u8 *sp, const Image *src, const u32 *pal, int32_t w, int32_t h){
	switch((u32)src->depth << 8 | (u32)dst->depth){
	case 0x404: case 0x808: case 0x2020: return 1;
	case 0x408:
		for(; h > 0; h--){ memcpy(dp, sp, (u32)w); sp += src->stride; dp += dst->stride; }
		return 1;
	case 0x420: case 0x820:
		for(; h > 0; h--){
			for(int32_t x = 0; x < w; x++) ((u32 *)dp)[x] = pal[sp[x]];
			sp += src->stride; dp += dst->stride;
		}
		return 1;
	}
	err((int32_t)0x80000009);
	return 0;
}

extern "C" {

void __cdecl mflrw_image_bind(char **engine, int32_t *imageModule, void *imageTK, void *initObject, void *deInitObject, void *allocPixels, void *errorSet){
	g_engine = (u8 **)engine; g_imageModule = imageModule; g_imageTK = imageTK; g_initObject = (RegFn)initObject; g_deInitObject = (RegFn)deInitObject;
	g_allocPixels = (AllocPixFn)allocPixels; g_errorSet = (ErrorSetFn)errorSet;
}

int32_t __cdecl rw37_ImageStraightCopy(Image *dst, const Image *src){ copyRows(dst, src); return 1; }

int32_t __cdecl rw37_ImageConvertDepth(Image *dst, const Image *src){
	return convert(dst->pixels, dst, src->pixels, src, (const u32 *)src->palette, dst->width, dst->height);
}

Image *__cdecl rw37_RwImageCopy(Image *dst, const Image *src){
	if(dst->depth == src->depth) copyRows(dst, src);
	else if(!convert(dst->pixels, dst, src->pixels, src, (const u32 *)src->palette, dst->width, dst->height)){
		Image *volatile none = 0;		// the original goes on with dst = NULL (a crash, as there)
		dst = none;
	}
	takeGamma(dst, src);
	return dst;
}

// alpha = max(r, g, b) of every pixel (32) or palette entry (4 / 8)
Image *__cdecl rw37_RwImageMakeMask(Image *img){
	if(img->depth == 4 || img->depth == 8){
		u8 *p = img->palette;
		for(int32_t n = 1 << (img->depth & 31); n > 0; n--, p += 4) p[3] = max3(p[0], p[1], p[2]);
	}else if(img->depth == 32){
		u8 *row = img->pixels;
		for(int32_t y = 0; y < img->height; y++, row += img->stride)
			for(int32_t x = 0; x < img->width; x++) row[4*x + 3] = max3(row[4*x], row[4*x + 1], row[4*x + 2]);
	}
	return img;
}

// the mask's alpha (through its palette for 4 / 8) into the image's alpha; a 4 / 8 bit image becomes 32 bit first
Image *__cdecl rw37_RwImageApplyMask(Image *img, const Image *mask){
	int32_t w = img->width;
	if(w != mask->width || img->height != mask->height){ err((int32_t)0x8000000a); return 0; }
	int32_t h = img->height, depth = img->depth;
	if(depth == 4 || depth == 8){
		Image *tmp = (Image *)(*(FlAllocFn *)(eng() + 0x118))(imageFL(), 0x30018);
		if(!tmp) return 0;
		tmp->width = w; tmp->height = h; tmp->depth = depth;
		tmp->pixels = 0; tmp->palette = 0; tmp->flags = 0;
		g_initObject(g_imageTK, tmp);
		if(!g_allocPixels(tmp)){
			if(tmp->flags & 1){ (*(FreeFn *)(eng() + 0x10c))(tmp->pixels); tmp->pixels = 0; tmp->palette = 0; tmp->flags &= ~1; }
			g_deInitObject(g_imageTK, tmp);
			(*(FlFreeFn *)(eng() + 0x11c))(imageFL(), tmp);
			return 0;
		}
		Image *t = tmp;
		if(!(tmp->depth == img->depth ? rw37_ImageStraightCopy(tmp, img) : rw37_ImageConvertDepth(tmp, img))){ Image *volatile none = 0; t = none; }
		takeGamma(t, img);
		if(img->flags & 1){ (*(FreeFn *)(eng() + 0x10c))(img->pixels); img->pixels = 0; img->palette = 0; img->flags &= ~1; }
		img->depth = 32;
		g_allocPixels(img);
		Image *res = img;
		if(img->depth == tmp->depth) copyRows(img, tmp);
		else if(!rw37_ImageConvertDepth(img, tmp)){ Image *volatile none = 0; res = none; }
		takeGamma(res, tmp);
		(*(FreeFn *)(eng() + 0x10c))(tmp->pixels);
		tmp->pixels = 0; tmp->palette = 0; tmp->flags &= ~1;
		g_deInitObject(g_imageTK, tmp);
		(*(FlFreeFn *)(eng() + 0x11c))(imageFL(), tmp);
	}else if(depth != 32){ err((int32_t)0x80000009); return 0; }
	const u8 *pal = mask->palette, *mp = mask->pixels;
	u8 *ip = img->pixels;
	for(int32_t y = 0; y < img->height; y++){
		if(mask->depth == 4 || mask->depth == 8) for(int32_t x = 0; x < img->width; x++) ip[4*x + 3] = pal[4*mp[x] + 3];
		else if(mask->depth == 32) for(int32_t x = 0; x < img->width; x++) ip[4*x + 3] = mp[4*x + 3];
		mp += mask->stride; ip += img->stride;
	}
	return img;
}

// palette entries (4 / 8) or pixels (32) through the gamma table, then flags |= 2
Image *__cdecl rw37_RwImageGammaCorrect(Image *img){
	if(img->depth == 4 || img->depth == 8){
		u8 *p = img->palette;
		int32_t n = 1 << (img->depth & 31);
		if(!p){ err((int32_t)0x80000016); return 0; }
		const u8 *tab = eng() + *g_imageModule + 0xc;
		for(; n; n--, p += 4){ p[0] = tab[p[0]]; p[1] = tab[p[1]]; p[2] = tab[p[2]]; }
	}else if(img->depth == 32){
		u8 *row = img->pixels;
		int32_t w = img->width, h = img->height;
		if(!row){ err((int32_t)0x80000016); return 0; }
		for(; h > 0; h--, row += img->stride){
			const u8 *tab = eng() + *g_imageModule + 0xc;
			u8 *p = row;
			if(w) for(u32 k = (u32)w; k; k--, p += 4){ p[0] = tab[p[0]]; p[1] = tab[p[1]]; p[2] = tab[p[2]]; }
		}
	}else{ err((int32_t)0x80000008); return 0; }
	img->flags |= 2;
	return img;
}

} // extern "C"
