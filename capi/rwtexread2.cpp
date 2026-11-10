// TEXREAD batch 2 (lane K, window 5, round 2): rtpitexd readers, written op for op from the disassembly of MFL.exe.  Kept apart from rwtexread.cpp (batch 1 stays frozen).
//   rtpitexdTextureStreamReadPre 0x560760, RtPITexDictionaryStreamRead 0x55ff00.
// Exe binding: mflrw_texread2_bind(&RwEngineInstance, &textureModule 0x7d2184, table) (table order = enum B2 below).  Unbound: abort.
// Check: tools/delink/rw_texread2_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "capi/rw37.h"

typedef uint8_t u8;
typedef uint32_t u32;

enum B2 { C_ERRSET, C_FINDCHUNK, C_STREAMREAD, C_SKIPCH, C_STRFR, C_TEXTK, C_TEXCREATE, C_SETNAME, C_SETMASK, C_TDCREATE, C_LEGACY, C_IMGSTREAMREAD, C_GAMMA,
          C_FINDRASTERFMT, C_RASTERCREATE, C_RASTERDESTROY, C_NUMLEVELS, C_RESAMPLE, C_IMGDESTROY, C_RLOCK, C_SETFROMIMAGE, C_RUNLOCK, C_SETRASTER, C_READCH, C_TDADD, C_POW, C_IMGMOD, C_STREAMOPEN, C_STREAMWRITE, C_WRITEINT32, C_BMPWRITEROW, C_STREAMCLOSE,
          C_IMGCOUNT, C_TEXDICTGLOBAL, C_TEXCOUNT, C_FLDESTROY, C_TEXDESTROY, C_DEINITOBJ, C_TDTK2,
          C_FLCREATE, C_RASTERTK, C_IMAGETK2, C_BASE_TEX, C_BASE_RAS, C_BASE_IMG, C_V218C, C_V2160, C_V21B4, C_V272C, C_V2700, C_RASTERCOUNT, C_RASTERMODVAR,
          C_CB_FIND, C_CB_READ, C_CB_MIP, C_CB_MIPNAME, C_PQINIT, C_PQADD, C_PQRESOLVE, C_PQMATCH, C_PQTERM, C_IMGCREATE, C_ALLOCPIX, C_FINDFILETYPE, C_READMASKED, C_IMGCOPY, C_IMGRESAMPLE, C_PALETTIZE, C_SETFROMRASTER, C_MIPMAPREAD, C_EMPTYSTR, C_N };
static u8 **g_engine;
static int32_t *g_textureModule;
static void *g_c[C_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] texread2 called before mflrw_texread2_bind\n"); abort(); }
	return *g_engine;
}
#define ENG(off, T) (*(T *)(eng() + (off)))
#define TEXMOD(off, T) (*(T *)(eng() + *g_textureModule + (off)))

typedef void *(__cdecl *ErrorSetFn)(const int32_t *err);
typedef int (__cdecl *FindChunkFn)(void *stream, u32 type, u32 *length, u32 *version);
typedef u32 (__cdecl *StreamReadFn)(void *stream, void *buf, u32 n);
typedef int32_t (__cdecl *StrFRFn)(char *buf, void *stream);
typedef int32_t (__cdecl *ReadChunksFn)(void *reg, void *stream, void *obj);
typedef int32_t (__cdecl *SkipChunksFn)(void *reg, void *stream);
typedef RwTexture37 *(__cdecl *TexCreateFn)(void *raster);
typedef void *(__cdecl *SetNameFn)(void *t, const char *name);
typedef void *(__cdecl *TdCreateFn)(void);
typedef void (__cdecl *LegacyFn)(void *stream, void **dict);
typedef u8 *(__cdecl *ImgStreamReadFn)(void *stream);
typedef void *(__cdecl *Img1Fn)(void *img);
typedef void (__cdecl *FindRasterFmtFn)(void *img, u32 flags, int32_t *w, int32_t *h, int32_t *d, u32 *fmt);
typedef u8 *(__cdecl *RasterCreateFn)(int32_t w, int32_t h, int32_t d, u32 flags);
typedef int32_t (__cdecl *NumLevelsFn)(void *r);
typedef u8 *(__cdecl *ResampleFn)(void *img, int32_t w, int32_t h);
typedef void *(__cdecl *RLockFn)(void *r, int32_t level, int32_t mode);
typedef int (__cdecl *SetFromImageFn)(void *r, void *img);
typedef void *(__cdecl *SetRasterFn)(void *t, void *r);
typedef void *(__cdecl *TdAddFn)(void *d, void *t);
typedef void *(__cdecl *MallocFn)(u32 n, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);

extern "C" {

void __cdecl mflrw_texread2_bind(u8 **engine, int32_t *textureModule, void *const *tbl){
	g_engine = engine; g_textureModule = textureModule;
	for(int i = 0; i < C_N; i++) g_c[i] = tbl[i];
}

static void errSet(int32_t plugin, int32_t code){ int32_t e[2] = {plugin, code}; ((ErrorSetFn)g_c[C_ERRSET])(e); }	// _rwerror 0x53eac0 returns its first argument
// RwImageSetGamma 0x4dea10: stores the gamma and fills the two 256 byte gamma tables of the image module (+0xc: x ^ (1 / gamma), +0x10c: x ^ gamma, x = i / 255, both
// * 255 + 0.5 truncated).  x87 as the original: the exe's _pow 0x4b2c30 is called, its result is used at full register precision; check at CW 0x7f and 0x27f.
typedef long double (__cdecl *PowFn)(double x, double y);
int32_t __cdecl rw37_RwImageSetGamma(float gamma){
	u8 *im = eng() + *(int32_t *)g_c[C_IMGMOD];
	*(float *)(im + 0x20c) = gamma;
	static const uint32_t k1_255_bits = 0x3b808081u;		// the exe's k_f_0_003921569 (0x5dff00), not the nearest float to 0.003921569
	float k1_255; memcpy(&k1_255, &k1_255_bits, 4);
	volatile float inv = 1.0f / gamma;
	im[0xc] = 0; im[0x10c] = 0;
	for(int32_t i = 1; i < 256; i++){
		volatile float x = (float)i * k1_255;
		volatile float t1 = (float)(((PowFn)g_c[C_POW])((double)x, (double)inv) * 255.0f + 0.5f);
		im[0xc + i] = (u8)(int32_t)t1;
		volatile float t2 = (float)(((PowFn)g_c[C_POW])((double)x, (double)gamma) * 255.0f + 0.5f);
		im[0x10c + i] = (u8)(int32_t)t2;
	}
	return 1;
}

// RtBMPImageWrite 0x53da80: writes the image as a BMP (14 byte file header, 40 byte info header, palette BGRA for depth <= 8 images with a palette else 24 bit rows, rows bottom up
// through BMPWriteRow 0x53dd30); returns the image, NULL when the file cannot be opened.  Known quirks kept: the row buffer is not checked for NULL, the sizes are not
// checked against the image.
typedef void *(__cdecl *StreamOpenFn)(int32_t type, int32_t access, const void *data);
typedef u32 (__cdecl *StreamWriteFn)(void *s, const void *buf, u32 n);
typedef void *(__cdecl *WriteRowFn)(void *s, void *image, int32_t row, int32_t bpp, void *buf);
typedef int32_t (__cdecl *StreamCloseFn)(void *s, void *data);
void *__cdecl rw37_RtBMPImageWrite(u8 *image, const char *name){
	void *s = ((StreamOpenFn)g_c[C_STREAMOPEN])(2, 2, name);		// rwSTREAMFILENAME, rwSTREAMWRITE
	if(!s) return NULL;
	StreamWriteFn wr = (StreamWriteFn)g_c[C_STREAMWRITE], wi = (StreamWriteFn)g_c[C_WRITEINT32];
	u32 depth = 0x18;
	if(*(void **)(image + 0x18)) depth = *(u32 *)(image + 0xc);
	u8 c = 0x42; wr(s, &c, 1);
	c = 0x4d; wr(s, &c, 1);
	u32 w = *(u32 *)(image + 4), h = *(u32 *)(image + 8);
	u32 pal = depth <= 8 ? (1u << depth) << 2 : 0;
	u32 v = (((w * depth + 0x1f) >> 3) & 0x1ffffffcu) * h + pal + 0x36;
	wi(s, &v, 4);
	v = 0; wi(s, &v, 2);
	v = 0; wi(s, &v, 2);
	v = (depth <= 8 ? (1u << depth) << 2 : 0) + 0x36; wi(s, &v, 4);
	v = 0x28; wi(s, &v, 4);
	v = w; wi(s, &v, 4);
	v = h; wi(s, &v, 4);
	v = 1; wi(s, &v, 2);
	v = depth; wi(s, &v, 2);
	for(int i = 0; i < 4; i++){ v = 0; wi(s, &v, 4); }		// compression, image size, pixels per metre x / y
	if(depth <= 8){
		u32 n = 1u << depth;
		v = n; wi(s, &v, 4);
		v = n; wi(s, &v, 4);
		const u8 *p = *(const u8 **)(image + 0x18);
		for(u32 i = 0; i < n; i++, p += 4){
			u8 q[4] = {p[2], p[1], p[0], 0xff};
			wr(s, q, 4);
		}
	}else{
		v = 0; wi(s, &v, 4);
		v = 0; wi(s, &v, 4);
	}
	void *buf = ENG(0x108, MallocFn)(w * 3, 0x10000);
	for(int32_t row = (int32_t)h - 1; row >= 0; row--) ((WriteRowFn)g_c[C_BMPWRITEROW])(s, image, row, (int32_t)depth, buf);
	ENG(0x10c, FreeFn)(buf);
	((StreamCloseFn)g_c[C_STREAMCLOSE])(s, NULL);
	return image;
}

// TextureDefaultMipmapName 0x4d62d0: appends "m<hex level>" (levels 1..15) to the texture name and (when not empty) the mask name through the engine's strcat (+0xd4)
typedef char *(__cdecl *StrCatFn)(char *d, const char *s);
int32_t __cdecl rw37_TextureDefaultMipmapName(char *name, char *mask, uint8_t level){
	static const char hex[] = "0123456789abcdef";
	char suffix[3] = {'m', 0, 0};
	suffix[1] = (level != 0 && level < 0x10) ? hex[level] : 0;
	if(suffix[1]){
		ENG(0xd4, StrCatFn)(name, suffix);
		if(mask && *mask) ENG(0xd4, StrCatFn)(mask, suffix);
	}
	return 1;
}

// PalettizeMipmaps 0x4d6710 (palette out buffer, image to keep, image array, count, palette bits): images that already share one palette give it back (copy of
// 1 << depth entries); otherwise a palette is quantized over all images (RwPalQuant*) and every image is rebuilt as an indexed image of that palette, the old ones
// destroyed except the kept one.  Quantizer state is a 0x4000 byte stack object.
typedef int32_t (__cdecl *PqInitFn)(void *q);
typedef void (__cdecl *PqAddFn)(void *q, void *img, float w);
typedef void (__cdecl *PqResolveFn)(void *pal, int32_t n, void *q);
typedef void (__cdecl *PqMatchFn)(void *pix, int32_t stride, int32_t depth, int32_t z, void *q, void *img);
typedef void (__cdecl *PqTermFn)(void *q);
typedef u8 *(__cdecl *ImgCreateFn)(int32_t w, int32_t h, int32_t d);
int32_t __cdecl rw37_PalettizeMipmaps(u8 *palOut, u8 *keep, u8 **imgs, int32_t n, int32_t bits){
	u8 quant[0x4000];
	u32 *pal0 = *(u32 **)(imgs[0] + 0x18);
	if(pal0){
		int32_t k = 1;
		if(n > 1){
			do{
				u32 *p = *(u32 **)(imgs[k] + 0x18);
				if(pal0 && p){
					int32_t cnt = 1 << (bits & 31);
					for(int32_t i = 0; i < cnt; i++) if(pal0[i] != p[i]){ k = 0x40; break; }
				}else k = 0x40;
				k++;
			}while(k < n);
		}
		if(k == n){
			memcpy(palOut, pal0, ((u32)1 << (*(u32 *)(imgs[0] + 0xc) & 31)) << 2);
			return 1;
		}
	}
	if(!((PqInitFn)g_c[C_PQINIT])(quant)) return 0;
	for(int32_t i = 0; i < n; i++) ((PqAddFn)g_c[C_PQADD])(quant, imgs[i], 1.0f);
	((PqResolveFn)g_c[C_PQRESOLVE])(palOut, 1 << (bits & 31), quant);
	for(int32_t i = 0; i < n; i++){
		u8 *old = imgs[i];
		u8 *im = ((ImgCreateFn)g_c[C_IMGCREATE])(*(int32_t *)(old + 4), *(int32_t *)(old + 8), bits);
		if(!im) return 0;
		((Img1Fn)g_c[C_ALLOCPIX])(im);
		((PqMatchFn)g_c[C_PQMATCH])(*(void **)(im + 0x14), *(int32_t *)(im + 0x10), *(int32_t *)(im + 0xc), 0, quant, old);
		*(u8 **)(im + 0x18) = palOut;
		imgs[i] = im;
		if(old != keep) ((Img1Fn)g_c[C_IMGDESTROY])(old);
	}
	((PqTermFn)g_c[C_PQTERM])(quant);
	return 1;
}

// TextureImageReadAndSize 0x4d68e0 (name, mask, flags, int *w, int *h, int *depth, unsigned *format): reads the (masked) image with the registered file extension appended
// to name / mask (names of 256+ chars are cut, RwErrorSet 0x8000001e), asks the device for the raster size / format when *w or *h is 0 (failure: RwErrorSet 0x80000009),
// and converts the image to the size: to 32 bit, resampled, and for 4 / 8 bit images quantized back to a palette image of 16 / 256 colours.
typedef char *(__cdecl *StrNCpyFn)(char *d, const char *s, u32 n);
typedef u32 (__cdecl *StrLen2Fn)(const char *s);
typedef int32_t (__cdecl *FindRasterFmt2Fn)(void *img, u32 flags, int32_t *w, int32_t *h, int32_t *d, u32 *fmt);
typedef void *(__cdecl *FindFileTypeFn)(const char *name);
typedef u8 *(__cdecl *ReadMaskedFn)(const char *name, const char *mask);
typedef void (__cdecl *ImgCopyFn)(void *dst, void *src);
static void nameErr(const char *s){
	int32_t e[2] = {1, (int32_t)0x8000001e};	// _rwerror 0x53eac0 returns its first argument (the other arguments are ignored)
	(void)s;
	((ErrorSetFn)g_c[C_ERRSET])(e);
}
u8 *__cdecl rw37_TextureImageReadAndSize(const char *name, const char *mask, u32 flags, int32_t *pw, int32_t *ph, int32_t *pd, u32 *pfmt){
	char buf[0x210];		// the original keeps the mask buffer directly below the name buffer: a 255 char mask plus its extension overruns into the name (kept)
	char *mb = buf, *nb = buf + 0x100;
	ENG(0xd0, StrNCpyFn)(nb, name, 0x100);
	if(ENG(0xf4, StrLen2Fn)(name) >= 0x100){ nameErr(name); nb[0xff] = 0; }
	char *ft = (char *)((FindFileTypeFn)g_c[C_FINDFILETYPE])(name);
	if(ft) ENG(0xd4, StrCatFn)(nb, ft);
	mb[0] = 0;
	if(mask && *mask){
		ENG(0xd0, StrNCpyFn)(mb, mask, 0x100);
		if(ENG(0xf4, StrLen2Fn)(mask) >= 0x100){ nameErr(mask); mb[0xff] = 0; }
		ft = (char *)((FindFileTypeFn)g_c[C_FINDFILETYPE])(mask);
		if(ft) ENG(0xd4, StrCatFn)(mb, ft);
	}
	u8 *im = ((ReadMaskedFn)g_c[C_READMASKED])(nb, mb);
	if(!im) return NULL;
	if(*pw == 0 || *ph == 0){
		if(!((FindRasterFmt2Fn)g_c[C_FINDRASTERFMT])(im, flags, pw, ph, pd, pfmt)){
			((Img1Fn)g_c[C_IMGDESTROY])(im);
			errSet(1, (int32_t)0x80000009);
			return NULL;
		}
	}
	if(*(int32_t *)(im + 4) == *pw && *(int32_t *)(im + 8) == *ph) return im;
	int32_t depth = *(int32_t *)(im + 0xc);
	if(depth != 0x20){
		u8 *old = im;
		im = ((ImgCreateFn)g_c[C_IMGCREATE])(*(int32_t *)(old + 4), *(int32_t *)(old + 8), 0x20);
		if(!im){ ((Img1Fn)g_c[C_IMGDESTROY])(old); return NULL; }
		if(!((Img1Fn)g_c[C_ALLOCPIX])(im)){ ((Img1Fn)g_c[C_IMGDESTROY])(im); ((Img1Fn)g_c[C_IMGDESTROY])(old); return NULL; }
		((ImgCopyFn)g_c[C_IMGCOPY])(im, old);
		((Img1Fn)g_c[C_IMGDESTROY])(old);
	}
	u8 *res = ((ImgCreateFn)g_c[C_IMGCREATE])(*pw, *ph, 0x20);
	if(!res){ ((Img1Fn)g_c[C_IMGDESTROY])(im); return NULL; }
	if(!((Img1Fn)g_c[C_ALLOCPIX])(res)){ ((Img1Fn)g_c[C_IMGDESTROY])(res); ((Img1Fn)g_c[C_IMGDESTROY])(im); return NULL; }
	((ImgCopyFn)g_c[C_IMGRESAMPLE])(res, im);
	((Img1Fn)g_c[C_IMGDESTROY])(im);
	im = res;
	if(depth != 4 && depth != 8) return im;
	u8 quant[0x4000];
	u32 pal[256];
	int32_t colours = depth == 4 ? 0x10 : 0x100;
	if(!((PqInitFn)g_c[C_PQINIT])(quant)) return im;
	((PqAddFn)g_c[C_PQADD])(quant, res, 1.0f);
	((PqResolveFn)g_c[C_PQRESOLVE])(pal, colours, quant);
	u8 *pim = ((ImgCreateFn)g_c[C_IMGCREATE])(*(int32_t *)(res + 4), *(int32_t *)(res + 8), depth);
	if(!pim) return im;
	((Img1Fn)g_c[C_ALLOCPIX])(pim);
	((PqMatchFn)g_c[C_PQMATCH])(*(void **)(pim + 0x14), *(int32_t *)(pim + 0x10), *(int32_t *)(pim + 0xc), 0, quant, res);
	memcpy(*(void **)(pim + 0x18), pal, (size_t)colours * 4);
	((Img1Fn)g_c[C_IMGDESTROY])(res);
	((PqTermFn)g_c[C_PQTERM])(quant);
	return pim;
}

// TextureRasterDefaultBuildMipmaps 0x4d7390 (raster, image or NULL): converts / reads the base level to a 32 bit image (from the raster itself when no image is given), makes
// one resampled image per further level (the raster's lock bit 0x10 is dropped meanwhile), palettizes them for rasters with palette flags (0x40 = 4 bit, 0x20 = 8 bit),
// gamma corrects and copies every level into the raster.  The images are destroyed except the caller's image; failure restores the flag and returns NULL.
typedef int32_t (__cdecl *PalettizeFn)(void *palOut, void *keep, void *imgs, int32_t n, int32_t bits);
typedef void *(__cdecl *SetFromRasterFn)(void *img, void *raster);
typedef u8 *(__cdecl *ResampleCreateFn)(void *img, int32_t w, int32_t h);
void *__cdecl rw37_TextureRasterDefaultBuildMipmaps(u8 *raster, u8 *image){
	u8 *A[16];
	u8 palbuf[0x400];
	int32_t w = *(int32_t *)(raster + 0xc), h = *(int32_t *)(raster + 0x10);
	u8 *img;
	if(!image){
		img = ((ImgCreateFn)g_c[C_IMGCREATE])(w, h, 0x20);
		A[0] = img;
		if(!img) return NULL;
		if(!((Img1Fn)g_c[C_ALLOCPIX])(img)) return NULL;
		((SetFromRasterFn)g_c[C_SETFROMRASTER])(img, raster);
	}else if(*(int32_t *)(image + 0xc) != 0x20){
		img = ((ImgCreateFn)g_c[C_IMGCREATE])(w, h, 0x20);
		A[0] = img;
		if(!img) return NULL;
		if(!((Img1Fn)g_c[C_ALLOCPIX])(img)) return NULL;
		((ImgCopyFn)g_c[C_IMGCOPY])(img, image);
	}else{
		img = image;
		A[0] = img;
	}
	if(!img) return NULL;
	u8 saved = raster[0x23] & 0x10;
	raster[0x23] &= ~0x10;
	int32_t n = ((NumLevelsFn)g_c[C_NUMLEVELS])(raster), k = 1;
	if(n > 1){
		do{
			A[k] = NULL;
			if(((RLockFn)g_c[C_RLOCK])(raster, k, 2)){
				A[k] = ((ResampleCreateFn)g_c[C_RESAMPLE])(A[k - 1], *(int32_t *)(raster + 0xc), *(int32_t *)(raster + 0x10));
				((Img1Fn)g_c[C_RUNLOCK])(raster);
			}
			if(!A[k]){
				for(k--; k >= 0; k--) if(A[k] != image) ((Img1Fn)g_c[C_IMGDESTROY])(A[k]);
				raster[0x23] |= saved;
				return NULL;
			}
			k++;
		}while(k < n);
	}
	u32 fl = raster[0x23];
	if(fl & 0x60){
		int32_t bits = (fl & 0x40) ? 4 : 8;
		if(!((PalettizeFn)g_c[C_PALETTIZE])(palbuf, image, A, n, bits) && n > 0){
			if(A[0] != image) ((Img1Fn)g_c[C_IMGDESTROY])(A[0]);
			raster[0x23] |= saved;
			return NULL;
		}
		((Img1Fn)g_c[C_GAMMA])(A[0]);
	}else{
		for(int32_t i = 0; i < n; i++) ((Img1Fn)g_c[C_GAMMA])(A[i]);
	}
	for(int32_t i = 0; i < n; i++){
		if(((RLockFn)g_c[C_RLOCK])(raster, i, 5)){
			((SetFromImageFn)g_c[C_SETFROMIMAGE])(raster, A[i]);
			((Img1Fn)g_c[C_RUNLOCK])(raster);
		}
		if(A[i] != image) ((Img1Fn)g_c[C_IMGDESTROY])(A[i]);
	}
	raster[0x23] |= saved;
	return raster;
}

// TextureDefaultRead 0x4d6340 (the module's default read callback, name + mask): with mipmapping on it is TextureDefaultMipmapRead 0x4d6d30; otherwise the image is read
// (TextureImageReadAndSize, flags 4, size 0 = whatever the file has), a raster is made and filled (palettized for palette rasters; the palette stays the stack buffer's address,
// as in the original), and the texture is built from the module's texture free list with the names cut to 31 characters.
static void initObj(RwPluginRegistry37 *reg, void *obj){
	for(RwPluginRegEntry37 *e = reg->first; e; e = e->next)
		if(!e->ctor(obj, e->offset, e->size)){
			for(e = e->prev; e; e = e->prev) e->dtor(obj, e->offset, e->size);
			return;
		}
}
typedef void *(__cdecl *FlAllocFn)(void *list, u32 hint);
typedef RwTexture37 *(__cdecl *MipReadFn)(const char *name, const char *mask);
typedef int32_t (__cdecl *MipNameCb)(char *name, char *mask, int32_t level, int32_t four);
RwTexture37 *__cdecl rw37_TextureDefaultRead(const char *name, const char *mask){
	if(TEXMOD(0x1c, int32_t)) return ((MipReadFn)g_c[C_MIPMAPREAD])(name, mask);
	u8 frame[0x600];		// mask buffer, name buffer (directly above it, as in the original) and the palette buffer
	char *mb = (char *)frame, *nb = (char *)frame + 0x100;
	u8 *palbuf = frame + 0x200;
	ENG(0xd0, StrNCpyFn)(nb, name, 0x100);
	if(ENG(0xf4, StrLen2Fn)(name) >= 0x100){ nameErr(name); nb[0xff] = 0; }
	mb[0] = 0;
	if(mask && *mask){
		ENG(0xd0, StrNCpyFn)(mb, mask, 0x100);
		if(ENG(0xf4, StrLen2Fn)(mask) >= 0x100){ nameErr(mask); mb[0xff] = 0; }
	}
	MipNameCb cb = TEXMOD(0x30, MipNameCb);
	if(cb) cb(nb, mb, 0, 4);
	int32_t w = 0, h = 0, d = 0; u32 fmt = 0;
	u8 *img = rw37_TextureImageReadAndSize(nb, mb, 4, &w, &h, &d, &fmt);
	if(!img) return NULL;
	u8 *ras = ((RasterCreateFn)g_c[C_RASTERCREATE])(w, h, d, fmt);
	if(!ras){ ((Img1Fn)g_c[C_IMGDESTROY])(img); return NULL; }
	u32 fl = ras[0x23];
	if(fl & 0x60){
		rw37_PalettizeMipmaps(palbuf, NULL, &img, 1, (fl & 0x40) ? 4 : 8);
		*(u8 **)(img + 0x18) = palbuf;
	}
	((Img1Fn)g_c[C_GAMMA])(img);
	if(!((SetFromImageFn)g_c[C_SETFROMIMAGE])(ras, img)){
		((Img1Fn)g_c[C_RASTERDESTROY])(ras);
		((Img1Fn)g_c[C_IMGDESTROY])(img);
		return NULL;
	}
	((Img1Fn)g_c[C_IMGDESTROY])(img);
	u8 *t = (u8 *)ENG(0x118, FlAllocFn)(TEXMOD(8, void *), 0x30006);
	if(!t){ ((Img1Fn)g_c[C_RASTERDESTROY])(ras); return NULL; }
	*(u32 *)(t + 0x50) = 0; t[0x51] = 0x11; *(u32 *)(t + 4) = 0; t[0x10] = 0; t[0x30] = 0; *(u8 **)t = ras; *(u32 *)(t + 0x54) = 1; t[0x50] = 1;
	initObj((RwPluginRegistry37 *)g_c[C_TEXTK], t);
	ENG(0xd0, StrNCpyFn)((char *)t + 0x10, name, 0x20);
	if(ENG(0xf4, StrLen2Fn)(name) >= 0x20){ nameErr(name); t[0x2f] = 0; }
	if(!mask){
		((SetNameFn)g_c[C_SETMASK])(t, (const char *)g_c[C_EMPTYSTR]);
		return (RwTexture37 *)t;
	}
	ENG(0xd0, StrNCpyFn)((char *)t + 0x30, mask, 0x20);
	if(ENG(0xf4, StrLen2Fn)(mask) >= 0x20){ nameErr(mask); t[0x4f] = 0; }
	return (RwTexture37 *)t;
}

// TextureDefaultMipmapRead 0x4d6d30 (name, mask; the module's read callback when mipmapping is on, batch 3): level 0 is read like TextureDefaultRead (names go through the
// module's mipmap name callback +0x30 with the level), the raster is made with the device's idea of the size / format; a raster with mipmap bit 0x8000 (and not 0x1000) gets
// one image per further level (each read again under its own level name at the size of the raster level, raster locked / unlocked around the size query), then all levels are
// palettized (palette flags), gamma corrected and copied in; failures destroy the images read so far and the raster.
RwTexture37 *__cdecl rw37_TextureDefaultMipmapRead(const char *name, const char *mask){
	u8 frame[0x600];		// mask buffer, name buffer (directly above it), palette buffer (as in the original frame)
	char *mb = (char *)frame, *nb = (char *)frame + 0x100;
	u8 *palbuf = frame + 0x200;
	u8 *A[16];
	ENG(0xd0, StrNCpyFn)(nb, name, 0x100);
	if(ENG(0xf4, StrLen2Fn)(name) >= 0x100){ nameErr(name); nb[0xff] = 0; }
	mb[0] = 0;
	if(mask && *mask){
		ENG(0xd0, StrNCpyFn)(mb, mask, 0x100);
		if(ENG(0xf4, StrLen2Fn)(mask) >= 0x100){ nameErr(mask); mb[0xff] = 0; }
	}
	u32 flags = 4;
	if(TEXMOD(0x1c, int32_t)){
		flags = 0x8004;
		if(TEXMOD(0x20, int32_t)) flags = 0x9004;
	}
	MipNameCb cb = TEXMOD(0x30, MipNameCb);
	if(cb) cb(nb, mb, 0, (int32_t)flags);
	int32_t w = 0, h = 0, d, fmtv;
	u32 fmt;
	u8 *img = rw37_TextureImageReadAndSize(nb, mb, flags, &w, &h, &d, &fmt);
	A[0] = img;
	if(!img) return NULL;
	u8 *ras = ((RasterCreateFn)g_c[C_RASTERCREATE])(w, h, d, fmt);
	if(!ras){ ((Img1Fn)g_c[C_IMGDESTROY])(img); return NULL; }
	(void)fmtv;
	int32_t n = 0;
	if(!(fmt & 0x8000) || (fmt & 0x1000)){
		if(!(fmt & 0x8000)) ((Img1Fn)g_c[C_GAMMA])(img);
		if(!((SetFromImageFn)g_c[C_SETFROMIMAGE])(ras, img)){
			((Img1Fn)g_c[C_RASTERDESTROY])(ras);
			((Img1Fn)g_c[C_IMGDESTROY])(img);
			return NULL;
		}
		((Img1Fn)g_c[C_IMGDESTROY])(img);
	}else{
		n = ((NumLevelsFn)g_c[C_NUMLEVELS])(ras);
		int32_t k = 1;
		if(n > 1){
			do{
				ENG(0xd0, StrNCpyFn)(nb, name, 0x100);
				if(ENG(0xf4, StrLen2Fn)(name) >= 0x100){ nameErr(name); nb[0xff] = 0; }
				mb[0] = 0;
				if(mask && *mask){
					ENG(0xd0, StrNCpyFn)(mb, mask, 0x100);
					if(ENG(0xf4, StrLen2Fn)(mask) >= 0x100){ nameErr(mask); mb[0xff] = 0; }
				}
				cb = TEXMOD(0x30, MipNameCb);
				if(cb) cb(nb, mb, k, (int32_t)flags);
				((RLockFn)g_c[C_RLOCK])(ras, k, 5);
				w = *(int32_t *)(ras + 0xc); h = *(int32_t *)(ras + 0x10); d = *(int32_t *)(ras + 0x14);
				fmt = ((u32)ras[0x23] << 8) | ras[0x20];
				((Img1Fn)g_c[C_RUNLOCK])(ras);
				A[k] = rw37_TextureImageReadAndSize(nb, mb, flags, &w, &h, &d, &fmt);
				if(!A[k]){
					for(k--; k >= 0; k--) ((Img1Fn)g_c[C_IMGDESTROY])(A[k]);
					((Img1Fn)g_c[C_RASTERDESTROY])(ras);
					return NULL;
				}
				k++;
			}while(k < n);
		}
		u32 rf = ras[0x23];
		if(rf & 0x60){
			rw37_PalettizeMipmaps(palbuf, NULL, A, n, (rf & 0x40) ? 4 : 8);
			((Img1Fn)g_c[C_GAMMA])(A[0]);
		}else{
			for(int32_t i = 0; i < n; i++) ((Img1Fn)g_c[C_GAMMA])(A[i]);
		}
		for(int32_t i = 0; i < n; i++){
			if(((RLockFn)g_c[C_RLOCK])(ras, i, 5)){
				if(!((SetFromImageFn)g_c[C_SETFROMIMAGE])(ras, A[i])){
					for(int32_t j = i; j < n; j++) ((Img1Fn)g_c[C_IMGDESTROY])(A[j]);
					((Img1Fn)g_c[C_RASTERDESTROY])(ras);
					return NULL;
				}
				((Img1Fn)g_c[C_RUNLOCK])(ras);
			}
			((Img1Fn)g_c[C_IMGDESTROY])(A[i]);
		}
	}
	u8 *t = (u8 *)ENG(0x118, FlAllocFn)(TEXMOD(8, void *), 0x30006);
	if(!t){ ((Img1Fn)g_c[C_RASTERDESTROY])(ras); return NULL; }
	*(u32 *)(t + 0x50) = 0; t[0x51] = 0x11; *(u32 *)(t + 4) = 0; t[0x10] = 0; t[0x30] = 0; *(u8 **)t = ras; *(u32 *)(t + 0x54) = 1; t[0x50] = 1;
	initObj((RwPluginRegistry37 *)g_c[C_TEXTK], t);
	ENG(0xd0, StrNCpyFn)((char *)t + 0x10, name, 0x20);
	if(ENG(0xf4, StrLen2Fn)(name) >= 0x20){ nameErr(name); t[0x2f] = 0; }
	const char *msrc = mask ? mask : (const char *)g_c[C_EMPTYSTR];
	ENG(0xd0, StrNCpyFn)((char *)t + 0x30, msrc, 0x20);
	if(ENG(0xf4, StrLen2Fn)(msrc) >= 0x20){ nameErr(msrc); t[0x4f] = 0; }
	return (RwTexture37 *)t;
}

// ---- plugin open functions (engine start; arg1 is passed through, arg2 = the module's offset in the engine instance): free lists from the plugin registries' sizes, module defaults
typedef void (__cdecl *FlDestroyFn)(void *list);
typedef void *(__cdecl *FlCreateFn)(u32 size, u32 blockSize, u32 align, u32 prealloc, void *listVar, u32 hint);
static inline u32 W32(int slot, int idx){ return ((u32 *)g_c[slot])[idx]; }
static inline u32 regSize(int slot){ return *(u32 *)g_c[slot]; }
static void *flCreate(u32 reg, u32 blockSize, u32 prealloc, int varSlot, u32 hint){ return ((FlCreateFn)g_c[C_FLCREATE])(reg, blockSize, 4, prealloc, g_c[varSlot], hint); }

// rwRasterOpen 0x4d7a70
void *__cdecl rw37_rwRasterOpen(void *arg1, int32_t off){
	u8 *e = eng();
	*(int32_t *)g_c[C_RASTERMODVAR] = off;
	memset(e + off + 0x2c, 0, 0x34);
	u8 *m = eng() + off;
	*(u32 *)(m + 0x38) = 0; *(u32 *)(m + 0x3c) = 0; *(u32 *)(m + 0x40) = 0; m[0x4d] = 0x80; *(u32 *)(m + 0x30) = 0; *(u32 *)(m + 0x34) = 0; m[0x4c] = 0; *(u32 *)(m + 0x28) = 0;
	*(u8 **)m = m + 0x2c;
	void *l = flCreate(regSize(C_RASTERTK), W32(C_BASE_RAS, 0), W32(C_BASE_RAS, 1), C_V21B4, 0x40407);
	*(void **)(m + 0x60) = l;
	if(!l) return NULL;
	(*(int32_t *)g_c[C_RASTERCOUNT])++;
	return arg1;
}

// rwTextureOpen 0x4d60f0
void *__cdecl rw37_rwTextureOpen(void *arg1, int32_t off){
	*g_textureModule = off;
	u8 *m = eng() + off;
	*(void **)(m + 8) = flCreate(regSize(C_TEXTK), W32(C_BASE_TEX, 0), W32(C_BASE_TEX, 1), C_V218C, 0x40006);
	if(!*(void **)(m + 8)) return NULL;
	m = eng() + *g_textureModule;
	*(void **)(m + 0xc) = flCreate(regSize(C_TDTK2), W32(C_BASE_TEX, 2), W32(C_BASE_TEX, 3), C_V2160, 0x40408);
	if(!*(void **)(m + 0xc)){
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)(m + 8));
		*(void **)(m + 8) = NULL;
		return NULL;
	}
	*(u8 **)m = m; *(u8 **)(m + 4) = m;
	(*(int32_t *)g_c[C_TEXCOUNT])++;
	void *d = ((TdCreateFn)g_c[C_TDCREATE])();
	*(void **)g_c[C_TEXDICTGLOBAL] = d;
	m = eng() + *g_textureModule;
	*(void **)(m + 0x10) = d;
	if(!d){
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)(m + 0xc));
		m = eng() + *g_textureModule;
		*(void **)(m + 0xc) = NULL;
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)(m + 8));
		m = eng() + *g_textureModule;
		*(void **)(m + 8) = NULL;
		return NULL;
	}
	*(u32 *)(m + 0x1c) = 0; *(u32 *)(m + 0x20) = 0;
	*(void **)(m + 0x18) = g_c[C_CB_FIND]; *(void **)(m + 0x14) = g_c[C_CB_READ]; *(void **)(m + 0x2c) = g_c[C_CB_MIP]; *(void **)(m + 0x30) = g_c[C_CB_MIPNAME];
	*(u32 *)(m + 0x24) = 0; *(uint16_t *)(m + 0x28) = 0;
	return arg1;
}

// rwImageOpen 0x4dd1b0 (also sets the default gamma 1.0)
void *__cdecl rw37_rwImageOpen(void *arg1, int32_t off){
	*(int32_t *)g_c[C_IMGMOD] = off;
	u8 *m = eng() + off;
	*(void **)m = flCreate(regSize(C_IMAGETK2), W32(C_BASE_IMG, 0), W32(C_BASE_IMG, 1), C_V272C, 0x40018);
	if(!*(void **)(eng() + off)) return NULL;
	m = eng() + *(int32_t *)g_c[C_IMGMOD];
	*(void **)(m + 0x218) = flCreate(0x34, W32(C_BASE_IMG, 0), W32(C_BASE_IMG, 3), C_V2700, 0x40406);
	if(!*(void **)(m + 0x218)){
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)m);
		*(void **)(eng() + *(int32_t *)g_c[C_IMGMOD]) = NULL;
		return NULL;
	}
	*(u32 *)(m + 8) = 0x100;
	void *p = ENG(0x108, MallocFn)(*(u32 *)(m + 8), 0x1040406);
	m = eng() + *(int32_t *)g_c[C_IMGMOD];
	*(void **)(m + 4) = p;
	if(!p){
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)(m + 0x218));
		m = eng() + *(int32_t *)g_c[C_IMGMOD];
		*(void **)(m + 0x218) = NULL;
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)m);
		*(void **)(eng() + *(int32_t *)g_c[C_IMGMOD]) = NULL;
		return NULL;
	}
	*(u8 *)p = 0;
	(*(int32_t *)g_c[C_IMGCOUNT])++;
	rw37_RwImageSetGamma(1.0f);
	m = eng() + *(int32_t *)g_c[C_IMGMOD];
	*(u32 *)(m + 0x21c) = 0;
	*(u32 *)(m + 0x214) = 0x100;
	void *q = ENG(0x108, MallocFn)(*(u32 *)(m + 0x214), 0x1040018);
	m = eng() + *(int32_t *)g_c[C_IMGMOD];
	*(void **)(m + 0x210) = q;
	if(!q){
		ENG(0x10c, FreeFn)(*(void **)(m + 4));
		m = eng() + *(int32_t *)g_c[C_IMGMOD];
		*(u32 *)(m + 4) = 0; *(u32 *)(m + 8) = 0;
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)(m + 0x218));
		m = eng() + *(int32_t *)g_c[C_IMGMOD];
		*(void **)(m + 0x218) = NULL;
		((FlDestroyFn)g_c[C_FLDESTROY])(*(void **)m);
		*(void **)(eng() + *(int32_t *)g_c[C_IMGMOD]) = NULL;
		return NULL;
	}
	return arg1;
}

// ---- plugin close functions (engine shutdown; the argument is passed through)
typedef void (__cdecl *FlDestroyFn)(void *list);
typedef void (__cdecl *FlFreeFn)(void *list, void *mem);
typedef int32_t (__cdecl *TexDestroyFn)(void *tex, int32_t z);
typedef void (__cdecl *DeInitFn)(void *reg, void *obj);
#define IMGMOD2(off, T) (*(T *)(eng() + *(int32_t *)g_c[C_IMGMOD] + (off)))
// rwImageClose 0x4dd410: scratch buffer, search path, image format list and both image free lists go; the open counter (0x7d2728) drops
void *__cdecl rw37_rwImageClose(void *arg){
	if(IMGMOD2(0x210, void *)){
		ENG(0x10c, FreeFn)(IMGMOD2(0x210, void *));
		IMGMOD2(0x210, void *) = NULL;
		IMGMOD2(0x214, int32_t) = 0;
	}
	if(IMGMOD2(4, void *)){
		ENG(0x10c, FreeFn)(IMGMOD2(4, void *));
		IMGMOD2(4, void *) = NULL;
		IMGMOD2(8, int32_t) = 0;
	}
	if(IMGMOD2(0x21c, u8 *)){
		do{
			u8 *e = IMGMOD2(0x21c, u8 *);
			IMGMOD2(0x21c, u8 *) = *(u8 **)(e + 0x30);
			ENG(0x11c, FlFreeFn)(IMGMOD2(0x218, void *), e);
		}while(IMGMOD2(0x21c, u8 *));
	}
	if(IMGMOD2(0x218, void *)){
		((FlDestroyFn)g_c[C_FLDESTROY])(IMGMOD2(0x218, void *));
		IMGMOD2(0x218, void *) = NULL;
	}
	if(IMGMOD2(0, void *)){
		((FlDestroyFn)g_c[C_FLDESTROY])(IMGMOD2(0, void *));
		IMGMOD2(0, void *) = NULL;
	}
	(*(int32_t *)g_c[C_IMGCOUNT])--;
	return arg;
}

// rwTextureClose 0x4d5f90: the name buffer (+0x24) goes; the dictionary remembered in g_data_7d21b0 (if it is still on the engine's list) is cleared as current, its textures are
// destroyed (until one refuses), it leaves the list and returns to its free list; both texture free lists go; the open counter (0x7d2188) drops
void *__cdecl rw37_rwTextureClose(void *arg){
	if(TEXMOD(0x24, void *)){
		ENG(0x10c, FreeFn)(TEXMOD(0x24, void *));
		TEXMOD(0x24, void *) = NULL;
		*(uint16_t *)(eng() + *g_textureModule + 0x28) = 0;
	}
	u8 *mod = eng() + *g_textureModule;
	if(*(void **)(mod + 8) && *(void **)(mod + 0xc) && *(u8 **)mod != mod){
		u8 *want = *(u8 **)g_c[C_TEXDICTGLOBAL], *l = *(u8 **)mod;
		while(l != mod && l - 0x10 != want) l = *(u8 **)l;
		if(l != mod){
			u8 *d = want;
			if(*(u8 **)(mod + 0x10) == d) *(u8 **)(mod + 0x10) = NULL;
			u8 *head = d + 8, *t = *(u8 **)head;
			while(t != head){
				u8 *next = *(u8 **)t;
				if(!((TexDestroyFn)g_c[C_TEXDESTROY])(t - 8, 0)) break;
				t = next;
			}
			((DeInitFn)g_c[C_DEINITOBJ])(g_c[C_TDTK2], d);
			u8 *prev = *(u8 **)(d + 0x14), *nxt = *(u8 **)(d + 0x10);
			*(u8 **)prev = nxt;
			*(u8 **)(nxt + 4) = prev;
			ENG(0x11c, FlFreeFn)(*(void **)(eng() + *g_textureModule + 0xc), d);
			*(u8 **)g_c[C_TEXDICTGLOBAL] = NULL;
		}
	}
	if(TEXMOD(8, void *)){
		((FlDestroyFn)g_c[C_FLDESTROY])(TEXMOD(8, void *));
		TEXMOD(8, void *) = NULL;
	}
	if(TEXMOD(0xc, void *)){
		((FlDestroyFn)g_c[C_FLDESTROY])(TEXMOD(0xc, void *));
		TEXMOD(0xc, void *) = NULL;
	}
	(*(int32_t *)g_c[C_TEXCOUNT])--;
	return arg;
}

static void setMip(int32_t v){ TEXMOD(0x1c, int32_t) = v; }
static void setAuto(int32_t v){ TEXMOD(0x20, int32_t) = v; }

// rtpitexdTextureStreamReadPre 0x560760: RwTextureStreamRead without the raster / data chunks: the texture is created empty and only named (plugin error id 0x1b3)
RwTexture37 *__cdecl rw37_rtpitexdTextureStreamReadPre(void *stream){
	u32 len, ver, hdr = 0;
	char name[0x80], mask[0x80];
	if(!((FindChunkFn)g_c[C_FINDCHUNK])(stream, 1, &len, &ver)) return NULL;
	if(ver < 0x35000 || ver > 0x37002){ errSet(0x1b3, (int32_t)0x80000004); return NULL; }
	if(((StreamReadFn)g_c[C_STREAMREAD])(stream, &hdr, len) != len) return NULL;
	u32 filt = hdr & 0xff;
	if(!(hdr & 0xf000)) hdr |= (hdr & 0xf00) << 4;
	u32 mipFlags = (hdr >> 16) & 0xff;
	int32_t oldMip = TEXMOD(0x1c, int32_t), oldAuto = TEXMOD(0x20, int32_t);
	if(filt == 3 || filt == 4 || filt == 5 || filt == 6){ setMip(1); setAuto((mipFlags & 1) ? 0 : 1); }
	else{ setMip(0); setAuto(0); }
	if(!((StrFRFn)g_c[C_STRFR])(name, stream) || !((StrFRFn)g_c[C_STRFR])(mask, stream)){ setMip(oldMip); setAuto(oldAuto); return NULL; }
	RwTexture37 *t = ((TexCreateFn)g_c[C_TEXCREATE])(NULL);
	if(!t){
		((SkipChunksFn)g_c[C_SKIPCH])(g_c[C_TEXTK], stream);
		setMip(oldMip); setAuto(oldAuto);
		return NULL;
	}
	((SetNameFn)g_c[C_SETNAME])(t, name);
	((SetNameFn)g_c[C_SETMASK])(t, mask);
	if(t->refCount == 1) *(u32 *)((u8 *)t + 0x50) = hdr & 0xffff;
	setMip(oldMip); setAuto(oldAuto);
	return t;
}

// RtPITexDictionaryStreamRead 0x55ff00: header = {texture count (low 16 bits), format (high 16 bits; 0 = legacy image2texture records)}.  New format: per texture an image count,
// the images (chunk 0x18; gamma corrected unless flag 2), the texture (chunk 6, rtpitexdTextureStreamReadPre), then a raster made from the first image (palettised images
// get a 32 / 16 bit raster, a size the device changed resamples every level) filled level by level, joined to the texture and the current dictionary.
void *__cdecl rw37_RtPITexDictionaryStreamRead(void *stream){
	u32 hdr = 0;
	if(((StreamReadFn)g_c[C_STREAMREAD])(stream, &hdr, 4) < 4) return NULL;
	void *dict = ((TdCreateFn)g_c[C_TDCREATE])();
	if(!dict) return NULL;
	if((uint16_t)(hdr >> 16) < 1){
		if((uint16_t)hdr == 0) return dict;
		u32 i = 0;
		do{
			((LegacyFn)g_c[C_LEGACY])(stream, &dict);
			i++;
		}while(i < (hdr & 0xffff));
		return dict;
	}
	void *prevCur = TEXMOD(0x10, void *);
	TEXMOD(0x10, void *) = dict;
	u32 nTex = 0;
	if((uint16_t)hdr != 0){
		do{
			int32_t nImg = 0;
			if(((StreamReadFn)g_c[C_STREAMREAD])(stream, &nImg, 4) != 4) goto fail;
			{
			u8 **arr = (u8 **)ENG(0x108, MallocFn)((u32)nImg * 4, 0x101b3);
			if(!arr){
				errSet(0x1b3, (int32_t)0x80000013);
				TEXMOD(0x10, void *) = prevCur;
				return NULL;
			}
			int32_t i = 0;
			for(; i < nImg; i++){
				if(!((FindChunkFn)g_c[C_FINDCHUNK])(stream, 0x18, NULL, NULL)) goto fail;
				arr[i] = ((ImgStreamReadFn)g_c[C_IMGSTREAMREAD])(stream);
				if(!arr[i]) goto fail;
				if(!(*(u32 *)arr[i] & 2)) ((Img1Fn)g_c[C_GAMMA])(arr[i]);
			}
			if(!((FindChunkFn)g_c[C_FINDCHUNK])(stream, 6, NULL, NULL)) goto fail;
			RwTexture37 *tex = rw37_rtpitexdTextureStreamReadPre(stream);
			if(!tex) goto fail;
			int32_t w = *(int32_t *)(arr[0] + 4), h = *(int32_t *)(arr[0] + 8), d = *(int32_t *)(arr[0] + 0xc);
			u32 rf = 4;
			if(nImg > 1) rf = 0x8004;
			else{
				u32 f = *(u32 *)((u8 *)tex + 0x50) & 0xff;
				if(f != 1 && f != 2) rf = 0x9004;
			}
			u32 fmt;
			((FindRasterFmtFn)g_c[C_FINDRASTERFMT])(arr[0], rf, &w, &h, &d, &fmt);
			u8 *ras = ((RasterCreateFn)g_c[C_RASTERCREATE])(w, h, d, fmt | rf);
			if(!ras){
				for(int32_t k = 0; k < nImg; k++) ((Img1Fn)g_c[C_IMGDESTROY])(arr[k]);
				ENG(0x10c, FreeFn)(arr);
				goto fail;
			}
			int32_t rw = *(int32_t *)(ras + 0xc), rh;
			if(*(int32_t *)(arr[0] + 4) != rw || *(int32_t *)(arr[0] + 8) != *(int32_t *)(ras + 0x10)){
				rh = *(int32_t *)(ras + 0x10);
				if(d == 8){
					((Img1Fn)g_c[C_RASTERDESTROY])(ras);
					ras = ((RasterCreateFn)g_c[C_RASTERCREATE])(rw, rh, 0x20, rf | 0x500);
					if(!ras){
						rf |= 0x100;
						ras = ((RasterCreateFn)g_c[C_RASTERCREATE])(rw, rh, 0x10, rf);
					}
				}
				int32_t n = ((NumLevelsFn)g_c[C_NUMLEVELS])(ras);
				for(int32_t k = 0; k < n; k++){
					u8 *res = ((ResampleFn)g_c[C_RESAMPLE])(arr[k], rw, rh);
					if(!res) goto fail;
					((Img1Fn)g_c[C_IMGDESTROY])(arr[k]);
					arr[k] = res;
					if(rw > 1) rw >>= 1;
					if(rh > 1) rh >>= 1;
				}
			}
			u32 saved = 0;
			if(nImg > 1){ saved = ras[0x23] & 0x10; ras[0x23] &= ~0x10; }
			int32_t n = ((NumLevelsFn)g_c[C_NUMLEVELS])(ras), k = 0;
			while(k < n && k != nImg){
				if(!((RLockFn)g_c[C_RLOCK])(ras, k, 1)) goto fail;
				if(!((SetFromImageFn)g_c[C_SETFROMIMAGE])(ras, arr[k])) goto fail;
				if(!((Img1Fn)g_c[C_RUNLOCK])(ras)) goto fail;
				((Img1Fn)g_c[C_IMGDESTROY])(arr[k]);
				k++;
			}
			if(nImg > 1) ras[0x23] |= (u8)saved;
			while(k < nImg){ ((Img1Fn)g_c[C_IMGDESTROY])(arr[k]); k++; }
			ENG(0x10c, FreeFn)(arr);
			((SetRasterFn)g_c[C_SETRASTER])(tex, ras);
			if(!((ReadChunksFn)g_c[C_READCH])(g_c[C_TEXTK], stream, tex)) goto fail;
			void *cur = TEXMOD(0x10, void *);
			if(cur) ((TdAddFn)g_c[C_TDADD])(cur, tex);
			}
			nTex++;
		}while(nTex < (hdr & 0xffff));
	}
	TEXMOD(0x10, void *) = prevCur;
	return dict;
fail:
	TEXMOD(0x10, void *) = prevCur;
	return NULL;
}

}
