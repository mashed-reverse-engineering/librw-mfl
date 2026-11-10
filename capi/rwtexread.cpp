// TEXREAD (LIB_RW group texread, plan_w5_laneK): the device independent image / texture read layer of MFL.exe (RW 3.7 baimage / batextur / babintex /
// rtpitexd), written op for op from the disassembly (integer only), on the exe's own memory.  Exports are prefixed rw37_ (swap list: tools/delink/rw_texread.txt).
// Stage 1 (image path search / image streams), VAs:
//   RwImageRead 0x4ddc40, ImagePathForAllFullNames 0x4ddd30, ImageGetScratchMem 0x4dde80, ImageAttempRead FUN_004ddf40 0x4ddf40, RwImageFindFileType 0x4ddf70,
//   RwImageStreamRead 0x4de740, RwImageReadMaskedImage 0x4de220.
// Stage 2 (texture module): RwTexDictionarySetCurrent 0x4d5e10 / GetCurrent 0x4d5e30, RwTextureRasterGenerateMipmaps 0x4d5f60, FUN_004d5f30 (RwTextureRegisterPlugin),
//   RwTextureRead 0x4d5e40, TextureDefaultFind 0x4d7620, RwTextureSetMipmapping 0x4d5990 / Get 0x4d59b0 / SetAuto 0x4d59c0, RwTextureSetRaster 0x4d59f0.
// Stage 3 (stream readers): RwTextureStreamRead 0x4df350, RwTexDictionaryStreamRead 0x4df580.
// The extension callback of RwImageFindFileType (0x4ddfa0, no symbol of its own) stays the exe's code; mflrw_texread_bind hands over its address.
// Exe-layout adapter: mflrw_texread_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance 0x7d2128 (string functions
// strcpy +0xcc, strrchr +0xdc, strchr +0xe0, strcmp +0xe8, strlen +0xf4; RwMalloc +0x108, RwRealloc +0x110), &imageModule 0x7d2724 (module offset of the image
// globals: +4 search path string, +0x210 / +0x214 scratch buffer and size, +0x21c image format list), RwErrorSet 0x4ea160, _rwpathisabsolute 0x4f1bb0,
// FUN_00561060 0x561060 (extension filter) and the callback 0x4ddfa0.  Unbound: abort.
// Check: tools/delink/rw_texread_check.sh (the original code and this export natively under Wine, same harness engine, stand-ins and call logs).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "capi/rw37.h"

typedef uint8_t u8;
typedef uint32_t u32;
struct Image;

typedef char *(__cdecl *StrCpyFn)(char *d, const char *s);
typedef char *(__cdecl *StrChrFn)(const char *s, int c);
typedef int (__cdecl *StrCmpFn)(const char *a, const char *b);
typedef u32 (__cdecl *StrLenFn)(const char *s);
typedef void *(__cdecl *MallocFn)(u32 n, u32 hint);
typedef void *(__cdecl *ReallocFn)(void *p, u32 n, u32 hint);
typedef void *(__cdecl *ErrorSetFn)(const int32_t *err);
typedef int (__cdecl *PathAbsFn)(const char *name);
typedef int (__cdecl *FilterFn)(const char *name);
typedef int (__cdecl *PathCb)(char *full, void *ctx);
typedef Image *(__cdecl *ReadCb)(const char *full);

typedef int (__cdecl *FindChunkFn)(void *stream, u32 type, u32 *length, u32 *version);
typedef u32 (__cdecl *StreamReadFn)(void *stream, void *buf, u32 n);
typedef void *(__cdecl *StreamSkipFn)(void *stream, u32 n);
typedef void *(__cdecl *AllocPixFn)(void *image);
typedef void *(__cdecl *FlAllocFn)(void *list, u32 hint);
typedef void (__cdecl *FlFreeFn)(void *list, void *mem);
typedef void (__cdecl *FreeFn)(void *p);
typedef int (__cdecl *ImgOpFn)(void *a, void *b);
typedef int (__cdecl *ImgOp1Fn)(void *a);
typedef double (__cdecl *PowFn)(double x, double y);

// slots of the table handed over by mflrw_texread_bind
enum { B_ERRSET, B_PATHABS, B_FILTER, B_EXTCB, B_MAKEMASK, B_APPLYMASK, B_FINDCHUNK, B_STREAMREAD, B_STREAMSKIP, B_ALLOCPIX, B_IMAGETK, B_POW, B_ADDPLUGIN, B_TEXTK,
       B_STRFR, B_SKIPCH, B_READCH, B_DEVREQ, B_TDCREATE, B_TDADD, B_FORALL, B_TDDESTROY, B_DESTROYTEX, B_TDTK, B_N };
static u8 **g_engine;
static int32_t *g_imageModule, *g_textureModule;
// texture module globals (module offset var 0x7d2184): +0 texdict list head, +0x10 current dict, +0x14 read callback, +0x18 find callback, +0x2c mipmap generator
#define TEXMOD(off, T) (*(T *)(eng() + *g_textureModule + (off)))
static void *g_b[B_N];
#define g_errorSet ((ErrorSetFn)g_b[B_ERRSET])
#define g_pathAbs ((PathAbsFn)g_b[B_PATHABS])
#define g_filter ((FilterFn)g_b[B_FILTER])
#define g_extCb g_b[B_EXTCB]
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] texread called before mflrw_texread_bind\n"); abort(); }
	return *g_engine;
}
#define ENG(off, T) (*(T *)(eng() + (off)))
#define IMGMOD(off, T) (*(T *)(eng() + *g_imageModule + (off)))

// _rwPluginRegistryInitObject 0x4e9ce0 / _rwPluginRegistryDeInitObject 0x4e9d40 (as capi/g3.cpp)
static void initObject(RwPluginRegistry37 *reg, void *obj){
	for(RwPluginRegEntry37 *e = reg->first; e; e = e->next)
		if(!e->ctor(obj, e->offset, e->size)){
			for(e = e->prev; e; e = e->prev) e->dtor(obj, e->offset, e->size);
			return;
		}
}
static void deInitObject(RwPluginRegistry37 *reg, void *obj){
	for(RwPluginRegEntry37 *e = reg->last; e; e = e->prev) e->dtor(obj, e->offset, e->size);
}

extern "C" {

void __cdecl mflrw_texread_bind(u8 **engine, int32_t *imageModule, int32_t *textureModule, void *const *tbl){
	g_engine = engine; g_imageModule = imageModule; g_textureModule = textureModule;
	for(int i = 0; i < B_N; i++) g_b[i] = tbl[i];
}

// ImageGetScratchMem 0x4dde80: grows the shared scratch buffer (never shrinks); a failed (re)allocation reports 0x80000013 and returns NULL
void *__cdecl rw37_ImageGetScratchMem(int32_t size){
	if(size > IMGMOD(0x214, int32_t)){
		void *buf = IMGMOD(0x210, void *), *p;
		if(buf) p = ENG(0x110, ReallocFn)(buf, (u32)size, 0x1040018);
		else p = ENG(0x108, MallocFn)((u32)size, 0x1040018);
		if(!p){
			int32_t e[2] = {1, (int32_t)0x80000013};	// _rwerror 0x53eac0 returns its first argument
			g_errorSet(e);
			return NULL;
		}
		IMGMOD(0x210, void *) = p;
		IMGMOD(0x214, int32_t) = size;
	}
	return IMGMOD(0x210, void *);
}

// ImagePathForAllFullNames 0x4ddd30: calls cb(full name, ctx) for every directory of the search path joined with the name (a name with a drive or a
// root, or an empty path: the name alone, cb result ignored); stops when cb returns 0.  Returns name, or NULL when the scratch buffer fails.
char *__cdecl rw37_ImagePathForAllFullNames(char *name, int32_t extra, PathCb cb, void *ctx){
	char *path = IMGMOD(4, char *), *next = NULL;
	if(!g_pathAbs(name) && path && *path){
		while(*path){
			ENG(0xe0, StrChrFn)(path, ';');
			char *semi = ENG(0xe0, StrChrFn)(path, ';');
			u32 n;
			if(semi){ n = (u32)(semi - path); next = semi + 1; }
			else{ n = ENG(0xf4, StrLenFn)(path); next = NULL; }
			u32 nameLen = ENG(0xf4, StrLenFn)(name);
			char *buf = (char *)rw37_ImageGetScratchMem((int32_t)(nameLen + n + (u32)extra));
			if(!buf) return NULL;
			memcpy(buf, path, n);
			ENG(0xcc, StrCpyFn)(buf + n, name);
			if(!cb(buf, ctx)) return name;
			path = next;
			if(!path) return name;
		}
		return name;
	}
	char *buf = (char *)rw37_ImageGetScratchMem((int32_t)(ENG(0xf4, StrLenFn)(name) + (u32)extra));
	if(!buf) return NULL;
	ENG(0xcc, StrCpyFn)(buf, name);
	cb(buf, ctx);
	return name;
}

// FUN_004ddf40 (ImageAttempRead): ctx = {read function, result}; names the filter rejects are passed on (return the name = keep searching)
char *__cdecl rw37_ImageAttempRead(char *full, void **ctx){
	if(g_filter(full)){
		ctx[1] = (void *)((ReadCb)ctx[0])(full);
		if(ctx[1]) return NULL;
	}
	return full;
}

// RwImageFindFileType 0x4ddf70
int32_t __cdecl rw37_RwImageFindFileType(char *name){
	int32_t r = 0;
	rw37_ImagePathForAllFullNames(name, 0x14, (PathCb)g_extCb, &r);
	return r;
}

// RwImageRead 0x4ddc40: the extension (after the last ':' '/' '\\' and then the first... last '.') selects the format entry (name at +0 or +0x14,
// reader at +0x28, next at +0x30); the reader runs on every path candidate until one returns an image
void *__cdecl rw37_RwImageRead(const char *name){
	const char *s = name, *p;
	if((p = ENG(0xdc, StrChrFn)(s, ':'))) s = p;
	if((p = ENG(0xdc, StrChrFn)(s, '/'))) s = p;
	if((p = ENG(0xdc, StrChrFn)(s, '\\'))) s = p;
	const char *ext = ENG(0xdc, StrChrFn)(s, '.');
	if(!ext) return NULL;
	u8 *e = IMGMOD(0x21c, u8 *);
	while(e){
		if(ENG(0xe8, StrCmpFn)((const char *)e, ext) == 0 || ENG(0xe8, StrCmpFn)((const char *)(e + 0x14), ext) == 0){
			void *rd = *(void **)(e + 0x28);
			if(!rd) return NULL;
			void *ctx[2] = {rd, NULL};
			rw37_ImagePathForAllFullNames((char *)name, 5, (PathCb)rw37_ImageAttempRead, ctx);
			return ctx[1];
		}
		e = *(u8 **)(e + 0x30);
	}
	return NULL;
}

// RwImageStreamRead 0x4de740: chunk 1 = {width, height, depth, stride} + pixel rows (the file stride may differ from ours: rows are cut / skipped) + palette (depth <= 8)
void *__cdecl rw37_RwImageStreamRead(void *stream){
	u32 version, length;
	if(!((FindChunkFn)g_b[B_FINDCHUNK])(stream, 1, &length, &version)) return NULL;
	int32_t hdr[4];
	((StreamReadFn)g_b[B_STREAMREAD])(stream, hdr, 0x10);
	u8 *im = (u8 *)ENG(0x118, FlAllocFn)(IMGMOD(0, void *), 0x30018);
	if(im){
		*(int32_t *)(im + 4) = hdr[0]; *(int32_t *)(im + 8) = hdr[1]; *(int32_t *)(im + 0xc) = hdr[2];
		*(void **)(im + 0x14) = NULL; *(void **)(im + 0x18) = NULL; *(u32 *)im = 0;
		initObject((RwPluginRegistry37 *)g_b[B_IMAGETK], im);
	}
	((AllocPixFn)g_b[B_ALLOCPIX])(im);		// (an allocation failure runs on into a NULL image, as in the original)
	int32_t stride = *(int32_t *)(im + 0x10);
	if(stride == hdr[3]){
		((StreamReadFn)g_b[B_STREAMREAD])(stream, *(void **)(im + 0x14), (u32)(*(int32_t *)(im + 8) * stride));
	}else{
		int32_t off = 0;
		for(int32_t n = 0; n < hdr[1]; n++){
			if(stride > hdr[3]){
				((StreamReadFn)g_b[B_STREAMREAD])(stream, *(u8 **)(im + 0x14) + off, (u32)hdr[3]);
			}else{
				((StreamReadFn)g_b[B_STREAMREAD])(stream, *(u8 **)(im + 0x14) + off, (u32)stride);
				((StreamSkipFn)g_b[B_STREAMSKIP])(stream, (u32)(hdr[3] - stride));
			}
			off += stride;
		}
	}
	int32_t depth = *(int32_t *)(im + 0xc);
	if(depth <= 8) ((StreamReadFn)g_b[B_STREAMREAD])(stream, *(void **)(im + 0x18), (u32)((1 << (depth & 31)) << 2));
	return im;
}

// RwImageDestroy (inlined in RwImageReadMaskedImage; also 0x4dd5b0): frees the pixels, runs the plugin destructors, returns the image to its free list
static void imageDestroy(u8 *im){
	if(*(u32 *)im & 1){
		ENG(0x10c, FreeFn)(*(void **)(im + 0x14));
		*(void **)(im + 0x14) = NULL; *(void **)(im + 0x18) = NULL;
		*(u32 *)im &= ~1u;
	}
	deInitObject((RwPluginRegistry37 *)g_b[B_IMAGETK], im);
	ENG(0x11c, FlFreeFn)(IMGMOD(0, void *), im);
}

// RwImageReadMaskedImage 0x4de220: an image plus an optional mask image (name and mask name); the mask is made into alpha and applied; any failure frees both
void *__cdecl rw37_RwImageReadMaskedImage(const char *name, const char *maskName){
	u8 *im = (u8 *)rw37_RwImageRead(name);
	if(!im) return NULL;
	if(maskName && *maskName){
		u8 *mask = (u8 *)rw37_RwImageRead(maskName);
		if(!mask){ imageDestroy(im); return NULL; }
		if(((ImgOp1Fn)g_b[B_MAKEMASK])(mask) &&((ImgOpFn)g_b[B_APPLYMASK])(im, mask)){
			imageDestroy(mask);
			return im;
		}
		imageDestroy(im);
		imageDestroy(mask);
		return NULL;
	}
	return im;
}

// ---- texture module (RwTexture37 / RwTexDictionary37: capi/rw37.h; refCount +0x54, dict +4, link +8, name +0x10)
// RwTexDictionarySetCurrent 0x4d5e10 / RwTexDictionaryGetCurrent 0x4d5e30
RwTexDictionary37 *__cdecl rw37_RwTexDictionarySetCurrent(RwTexDictionary37 *d){ TEXMOD(0x10, RwTexDictionary37 *) = d; return d; }
RwTexDictionary37 *__cdecl rw37_RwTexDictionaryGetCurrent(void){ return TEXMOD(0x10, RwTexDictionary37 *); }
// RwTextureRasterGenerateMipmaps 0x4d5f60: the module's generator (0x2c) decides
int32_t __cdecl rw37_RwTextureRasterGenerateMipmaps(void *raster, void *image){ return ((ImgOpFn)TEXMOD(0x2c, void *))(raster, image) != 0; }
// FUN_004d5f30 = RwTextureRegisterPlugin: _rwPluginRegistryAddPlugin(textureTKList 0x62e6d0, ...)
typedef int32_t (__cdecl *AddPluginFn)(void *reg, int32_t a, int32_t b, void *c, void *d, void *e);
int32_t __cdecl rw37_RwTextureRegisterPlugin(int32_t a, int32_t b, void *c, void *d, void *e){ return ((AddPluginFn)g_b[B_ADDPLUGIN])(g_b[B_TEXTK], a, b, c, d, e); }

// RwTextureRead 0x4d5e40: a texture the find callback (+0x18) knows gets another reference; else the read callback (+0x14) loads it (failure: RwErrorSet 0x16)
// and it joins the current dictionary (leaving its old one)
typedef RwTexture37 *(__cdecl *TexFind1Fn)(const char *name);
typedef RwTexture37 *(__cdecl *TexReadFn)(const char *name, const char *mask);
RwTexture37 *__cdecl rw37_RwTextureRead(const char *name, const char *mask){
	RwTexture37 *t = TEXMOD(0x18, TexFind1Fn)(name);
	if(t){ t->refCount++; return t; }
	t = TEXMOD(0x14, TexReadFn)(name, mask);
	if(!t){
		int32_t e[2] = {1, 0x16};		// _rwerror 0x53eac0 returns its first argument
		g_errorSet(e);
		return NULL;
	}
	RwTexDictionary37 *d = TEXMOD(0x10, RwTexDictionary37 *);
	if(d){
		if(t->dict){ t->lInDictionary.prev->next = t->lInDictionary.next; t->lInDictionary.next->prev = t->lInDictionary.prev; }
		t->dict = d;
		RwLLLink37 *head = &d->texturesInDict, *l = &t->lInDictionary;
		l->prev = head; l->next = head->next; head->next->prev = l; head->next = l;
	}
	return t;
}

// RwTextureSetMipmapping 0x4d5990 / RwTextureGetMipmapping 0x4d59b0 / RwTextureSetAutoMipmapping 0x4d59c0 (RwTextureGetAutoMipmapping 0x4d59e0 is g3's)
int32_t __cdecl rw37_RwTextureSetMipmapping(int32_t on){ TEXMOD(0x1c, int32_t) = on; return 1; }
int32_t __cdecl rw37_RwTextureGetMipmapping(void){ return TEXMOD(0x1c, int32_t); }
int32_t __cdecl rw37_RwTextureSetAutoMipmapping(int32_t on){ TEXMOD(0x20, int32_t) = on; return 1; }

// RwTextureSetRaster 0x4d59f0: a raster goes through the device's standard function +0x68 (answer 0 = failure: NULL); a NULL raster just clears the field
typedef int32_t (__cdecl *SetRasterFn)(RwTexture37 *t, RwRaster37 *r, int32_t z);
RwTexture37 *__cdecl rw37_RwTextureSetRaster(RwTexture37 *t, RwRaster37 *r){
	if(r) return ENG(0x68, SetRasterFn)(t, r, 0) ? t : NULL;
	t->raster = NULL;
	return t;
}

// stream readers (chunk 1 = struct, versions 0x35000..0x37002 accepted, else RwErrorSet 0x80000004)
typedef int32_t (__cdecl *StrFRFn)(char *buf, void *stream);
typedef int32_t (__cdecl *ReadChunksFn)(void *reg, void *stream, void *obj);
typedef int32_t (__cdecl *SkipChunksFn)(void *reg, void *stream);
typedef int32_t (__cdecl *DevReqFn)(void *dev, int32_t req, void *out, int32_t a, int32_t b);
typedef RwTexDictionary37 *(__cdecl *TdCreateFn)(void);
typedef void *(__cdecl *TdAddFn)(void *d, void *t);
typedef void *(__cdecl *ForAllFn)(void *d, void *cb, void *arg);
typedef int32_t (__cdecl *TdDestroyFn)(void *d);
typedef int32_t (__cdecl *NativeReadFn)(void *stream, void **out, u32 len);
static void badVersion(void){ int32_t e[2] = {1, (int32_t)0x80000004}; g_errorSet(e); }	// _rwerror 0x53eac0 returns its first argument

// RwTextureStreamRead 0x4df350: header = {filter / addressing (low 16 bits), mipmap flags (bits 16..23)}; filter 3..6 reads with mipmapping on (auto mipmapping unless bit 16)
RwTexture37 *__cdecl rw37_RwTextureStreamRead(void *stream){
	u32 len, ver, hdr = 0;
	char name[0x80], mask[0x80];
	if(!((FindChunkFn)g_b[B_FINDCHUNK])(stream, 1, &len, &ver)) return NULL;
	if(ver < 0x35000 || ver > 0x37002){ badVersion(); return NULL; }
	if(((StreamReadFn)g_b[B_STREAMREAD])(stream, &hdr, len) != len) return NULL;
	u32 filt = hdr & 0xff;
	if(!(hdr & 0xf000)) hdr |= (hdr & 0xf00) << 4;
	u32 mipFlags = (hdr >> 16) & 0xff;
	int32_t oldMip = rw37_RwTextureGetMipmapping(), oldAuto = TEXMOD(0x20, int32_t);
	if(filt == 3 || filt == 4 || filt == 5 || filt == 6){
		rw37_RwTextureSetMipmapping(1);
		rw37_RwTextureSetAutoMipmapping((mipFlags & 1) ? 0 : 1);
	}else{
		rw37_RwTextureSetMipmapping(0);
		rw37_RwTextureSetAutoMipmapping(0);
	}
	if(!((StrFRFn)g_b[B_STRFR])(name, stream) || !((StrFRFn)g_b[B_STRFR])(mask, stream)){
		rw37_RwTextureSetMipmapping(oldMip); rw37_RwTextureSetAutoMipmapping(oldAuto);
		return NULL;
	}
	RwTexture37 *t = rw37_RwTextureRead(name, mask);
	if(!t){
		((SkipChunksFn)g_b[B_SKIPCH])(g_b[B_TEXTK], stream);
		rw37_RwTextureSetMipmapping(oldMip); rw37_RwTextureSetAutoMipmapping(oldAuto);
		return NULL;
	}
	rw37_RwTextureSetMipmapping(oldMip); rw37_RwTextureSetAutoMipmapping(oldAuto);
	if(t->refCount == 1){
		*(u32 *)((u8 *)t + 0x50) = hdr & 0xffff;		// filter, addressing and the pad
		if(!((ReadChunksFn)g_b[B_READCH])(g_b[B_TEXTK], stream, t)) return NULL;
		return t;
	}
	if(!((SkipChunksFn)g_b[B_SKIPCH])(g_b[B_TEXTK], stream)) return NULL;
	return t;
}

// RwTexDictionaryStreamRead 0x4df580: header = {texture count (low 16 bits), device id (high 16 bits; 0 = any)}; native textures (chunk 0x15) through the
// device's native texture reader (engine +0xb0); any failure destroys the dictionary and its textures
RwTexDictionary37 *__cdecl rw37_RwTexDictionaryStreamRead(void *stream){
	u32 len, ver, hdr = 0, d = 0;
	if(!((FindChunkFn)g_b[B_FINDCHUNK])(stream, 1, &len, &ver)) return NULL;
	if(ver < 0x35000 || ver > 0x37002){ badVersion(); return NULL; }
	if(((StreamReadFn)g_b[B_STREAMREAD])(stream, &hdr, len) != len) return NULL;
	((DevReqFn)g_b[B_DEVREQ])(eng() + 0x10, 0x16, &d, 0, 0);
	uint16_t dev = (uint16_t)(d & 0xff), id = (uint16_t)(hdr >> 16);
	if(dev != 0 && id != 0 && dev != id) return NULL;
	RwTexDictionary37 *dict = ((TdCreateFn)g_b[B_TDCREATE])();
	if(!dict) return NULL;
	for(uint16_t n = (uint16_t)hdr; n; n--){
		void *tex = NULL;
		if(!((FindChunkFn)g_b[B_FINDCHUNK])(stream, 0x15, &len, &ver)) goto fail;
		if(ver < 0x35000 || ver > 0x37002){
			((ForAllFn)g_b[B_FORALL])(dict, g_b[B_DESTROYTEX], NULL);
			((TdDestroyFn)g_b[B_TDDESTROY])(dict);
			badVersion();
			return NULL;
		}
		if(!ENG(0xb0, NativeReadFn)(stream, &tex, len) || !tex) goto fail;
		if(!((ReadChunksFn)g_b[B_READCH])(g_b[B_TEXTK], stream, tex)) goto fail;
		((TdAddFn)g_b[B_TDADD])(dict, tex);
	}
	if(((ReadChunksFn)g_b[B_READCH])(g_b[B_TDTK], stream, dict)) return dict;
fail:
	((ForAllFn)g_b[B_FORALL])(dict, g_b[B_DESTROYTEX], NULL);
	((TdDestroyFn)g_b[B_TDDESTROY])(dict);
	return NULL;
}

// TextureDefaultFind 0x4d7620 (the module's default find callback): case-insensitive (ASCII a-z on signed chars) name search in the current dictionary,
// else in every dictionary of the engine list in order
static int nameEq(const char *x, const char *y){
	const signed char *a = (const signed char *)x, *b = (const signed char *)y;
	while(*a && *b){
		signed char p = *a, q = *b;
		if(p >= 'a' && p <= 'z') p -= 0x20;
		if(q >= 'a' && q <= 'z') q -= 0x20;
		if(p != q) return 0;
		a++; b++;
	}
	return *a == *b;
}
static RwTexture37 *findIn(RwTexDictionary37 *d, const char *name){
	RwLLLink37 *head = &d->texturesInDict;
	for(RwLLLink37 *l = head->next; l != head; l = l->next){
		RwTexture37 *t = (RwTexture37 *)((u8 *)l - offsetof(RwTexture37, lInDictionary));
		if(nameEq(t->name, name)) return t;
	}
	return NULL;
}
RwTexture37 *__cdecl rw37_TextureDefaultFind(const char *name){
	RwTexDictionary37 *cur = TEXMOD(0x10, RwTexDictionary37 *);
	if(cur) return findIn(cur, name);
	RwLLLink37 *head = &TEXMOD(0, RwLLLink37);
	for(RwLLLink37 *l = head->next; l != head; l = l->next){
		RwTexture37 *t = findIn((RwTexDictionary37 *)((u8 *)l - offsetof(RwTexDictionary37, lInInstance)), name);
		if(t) return t;
	}
	return NULL;
}

}
