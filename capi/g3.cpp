// G3 (plan_texdict_2026-11-03 steps T1 + T2): the device independent raster / texture / texture dictionary layer of RW 3.7, written op for
// op from the disassembly of MFL.exe (address in the comment), on the exe's own objects in the RW 3.7 byte layout (capi/rw37.h).
// The device part stays the exe's D3D9 driver, called through the engine's standard-function table (RwEngineInstance + 0x48 + 4 * i), and
// the memory stays the exe's: the objects come from the module free lists through the engine's RwFreeListAlloc / Free (+0x118 / +0x11c)
// exactly as in the original, so the raster list the driver walks on a device reset sees the same objects.  The exe hands over the
// addresses of its globals once with mflrw_exe_bind (called before RwEngineInit by rw_engine_init, recomp/automatch_near/an_hk_612.c).
// Exports are prefixed rw37_ (the plain names are the librw-object facade of rwobj.cpp); swap list: tools/delink/rw_g3.txt.
// Check: tools/delink/rw_g3_check.sh (original and DLL run the same random operation sequences natively; logs, objects and globals compared).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rw37.h"

typedef int32_t (__cdecl *StdFn)(void *out, void *in, int32_t flags);
typedef void *(__cdecl *FlAllocFn)(void *freeList, uint32_t hint);
typedef void (__cdecl *FlFreeFn)(void *freeList, void *mem);
typedef char *(__cdecl *StrNCpyFn)(char *dst, const char *src, uint32_t n);
typedef uint32_t (__cdecl *StrLenFn)(const char *s);
typedef void *(__cdecl *ErrorSetFn)(const int32_t *err);

static uint8_t **g_engine;		// &RwEngineInstance 0x7d2128
static int32_t *g_rasterModule;		// 0x7d21d8: offset of the raster module globals in the engine instance
static int32_t *g_textureModule;	// 0x7d2184: offset of the texture module globals
static RwPluginRegistry37 *g_rasterTK, *g_textureTK, *g_texDictTK;	// 0x62e718 / 0x62e6d0 / 0x62e6e8
static ErrorSetFn g_errorSet;		// RwErrorSet 0x4ea160

static uint8_t *E(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] g3 called before mflrw_exe_bind\n"); abort(); }
	return *g_engine;
}
#define ENG(off, T) (*(T *)(E() + (off)))
static inline StdFn STD(int off){ return ENG(off, StdFn); }
// raster module globals: +0x60 raster free list, +0x28 context stack index, +4 * i the context stack
#define RASMOD(off, T) (*(T *)(E() + *g_rasterModule + (off)))
// texture module globals: +0 texdict list head, +8 texture free list, +0xc texdict free list, +0x10 current dict, +0x20 auto mipmapping
#define TEXMOD(off, T) (*(T *)(E() + *g_textureModule + (off)))
static inline void *flAlloc(void *list, uint32_t hint){ return ENG(RW37_FREELISTALLOC, FlAllocFn)(list, hint); }
static inline void flFree(void *list, void *mem){ ENG(RW37_FREELISTFREE, FlFreeFn)(list, mem); }

// rwPluginRegistryInitObject 0x4e9ce0: constructors first to last; on a failure the destructors of the entries before it, last first
static void initObject(RwPluginRegistry37 *reg, void *obj){
	for(RwPluginRegEntry37 *e = reg->first; e; e = e->next)
		if(!e->ctor(obj, e->offset, e->size)){
			for(e = e->prev; e; e = e->prev) e->dtor(obj, e->offset, e->size);
			return;
		}
}
// rwPluginRegistryDeInitObject 0x4e9d40: destructors last to first
static void deInitObject(RwPluginRegistry37 *reg, void *obj){
	for(RwPluginRegEntry37 *e = reg->last; e; e = e->prev) e->dtor(obj, e->offset, e->size);
}
static inline void unlink(RwLLLink37 *l){ l->prev->next = l->next; l->next->prev = l->prev; }

extern "C" {

void __cdecl mflrw_exe_bind(uint8_t **engineVar, int32_t *rasterModuleVar, int32_t *textureModuleVar, void *rasterTK, void *textureTK,
                            void *texDictTK, void *errorSet){
	g_engine = engineVar; g_rasterModule = rasterModuleVar; g_textureModule = textureModuleVar;
	g_rasterTK = (RwPluginRegistry37 *)rasterTK; g_textureTK = (RwPluginRegistry37 *)textureTK; g_texDictTK = (RwPluginRegistry37 *)texDictTK;
	g_errorSet = (ErrorSetFn)errorSet;
}

// ---- T1 raster
// RwRasterCreate 0x4d7950: the driver (RASTERCREATE) sets type / flags / format and its extension; the header fields are set here first
RwRaster37 *__cdecl rw37_RwRasterCreate(int32_t width, int32_t height, int32_t depth, int32_t flags){
	void *list = RASMOD(0x60, void *);
	RwRaster37 *r = (RwRaster37 *)flAlloc(list, 0x30407);
	if(!r) return NULL;
	r->width = width; r->privateFlags = 0; r->height = height; r->depth = depth;
	r->cFlags = 0; r->nOffsetX = 0; r->nOffsetY = 0;
	r->parent = r; r->cpPixels = NULL; r->palette = NULL;
	if(!STD(RW37_STD_RASTERCREATE)(NULL, r, flags)){ flFree(RASMOD(0x60, void *), r); return NULL; }
	initObject(g_rasterTK, r);
	return r;
}
// RwRasterDestroy 0x4d77e0 (recomp/tier1/RwRasterDestroy.c)
int32_t __cdecl rw37_RwRasterDestroy(RwRaster37 *r){
	deInitObject(g_rasterTK, r);
	STD(RW37_STD_RASTERDESTROY)(NULL, r, 0);
	flFree(RASMOD(0x60, void *), r);
	return 1;
}
// RwRasterSubRaster 0x4d78f0: only into a raster created with rwRASTERDONTALLOCATE (cFlags bit 7)
RwRaster37 *__cdecl rw37_RwRasterSubRaster(RwRaster37 *sub, RwRaster37 *raster, const int32_t *rect){
	if(!(sub->cFlags & 0x80)) return NULL;
	sub->width = rect[2]; sub->height = rect[3];
	sub->nOffsetX = (int16_t)(raster->nOffsetX + (int16_t)rect[0]);
	sub->nOffsetY = (int16_t)(raster->nOffsetY + (int16_t)rect[1]);
	if(!STD(RW37_STD_RASTERSUBRASTER)(sub, raster, 0)) return NULL;
	sub->parent = raster->parent;
	return sub;
}
// RwRasterGetNumLevels 0x4d7880: 1 without rwRASTERFORMATMIPMAP (cFormat bit 7); the driver's answer goes to the raster argument's stack slot
int32_t __cdecl rw37_RwRasterGetNumLevels(RwRaster37 *r){
	if(!(r->cFormat & 0x80)) return 1;
	int32_t n = (int32_t)(uintptr_t)r;
	return STD(RW37_STD_RASTERGETMIPLEVELS)(&n, r, 0) ? n : -1;
}
// RwRasterLock 0x4d79f0 (recomp/tier1/RwRasterLock.c): pixels come back in the level argument's slot
uint8_t *__cdecl rw37_RwRasterLock(RwRaster37 *r, uint32_t level, int32_t lockMode){
	uint32_t px = level;
	return STD(RW37_STD_RASTERLOCK)(&px, r, (int32_t)(((level & 0xff) << 8) + lockMode)) ? (uint8_t *)(uintptr_t)px : NULL;
}
// RwRasterUnlock 0x4d7790 (recomp/tier1/RwRasterUnlock.c)
RwRaster37 *__cdecl rw37_RwRasterUnlock(RwRaster37 *r){ STD(RW37_STD_RASTERUNLOCK)(NULL, r, 0); return r; }
// RwRasterLockPalette 0x4d7850: the palette comes back in the mode argument's slot
uint8_t *__cdecl rw37_RwRasterLockPalette(RwRaster37 *r, int32_t lockMode){
	int32_t p = lockMode;
	return STD(RW37_STD_RASTERLOCKPALETTE)(&p, r, lockMode) ? (uint8_t *)(uintptr_t)p : NULL;
}
// RwRasterUnlockPalette 0x4d77b0 (recomp/tier1/RwRasterUnlockPalette.c): clears the palette lock bits 0x18
RwRaster37 *__cdecl rw37_RwRasterUnlockPalette(RwRaster37 *r){
	STD(RW37_STD_RASTERUNLOCKPALETTE)(NULL, r, 0);
	r->privateFlags &= 0xe7;
	return r;
}
// RwRasterGetCurrentContext 0x4d7750: top of the raster context stack
RwRaster37 *__cdecl rw37_RwRasterGetCurrentContext(void){ return RASMOD(4 * RASMOD(0x28, int32_t), RwRaster37 *); }
// RwRasterGetOffset 0x4d7770 (FUN_004d7770, recomp/tier1/RwRasterGetOffset.c)
RwRaster37 *__cdecl rw37_RwRasterGetOffset(RwRaster37 *r, int16_t *x, int16_t *y){ *x = r->nOffsetX; *y = r->nOffsetY; return r; }

// ---- T2 texture / texture dictionary
// RwTextureGetAutoMipmapping 0x4d59e0
int32_t __cdecl rw37_RwTextureGetAutoMipmapping(void){ return TEXMOD(0x20, int32_t); }
// RwTexDictionaryCreate 0x4d5a20: RwObject type 6, joins the front of the engine's dictionary list
RwTexDictionary37 *__cdecl rw37_RwTexDictionaryCreate(void){
	RwTexDictionary37 *d = (RwTexDictionary37 *)flAlloc(TEXMOD(0xc, void *), 0x30016);
	if(!d) return NULL;
	d->type = 6; d->subType = 0; d->flags = 0; d->privateFlags = 0; d->parent = NULL;
	RwLLLink37 *head = &TEXMOD(0, RwLLLink37);
	d->lInInstance.next = head->next; d->lInInstance.prev = head;
	head->next->prev = &d->lInInstance; head->next = &d->lInInstance;
	d->texturesInDict.prev = &d->texturesInDict; d->texturesInDict.next = &d->texturesInDict;
	initObject(g_texDictTK, d);
	return d;
}
// RwTextureCreate 0x4d5b90: refCount 1, filter 1 (nearest), addressing 0x11 (wrap / wrap); the dictionary link is left as allocated
RwTexture37 *__cdecl rw37_RwTextureCreate(RwRaster37 *raster){
	RwTexture37 *t = (RwTexture37 *)flAlloc(TEXMOD(8, void *), 0x30006);
	if(t){
		t->filter = 0; t->addressing = 0; t->pad = 0;
		t->raster = raster; t->addressing = 0x11; t->dict = NULL; t->name[0] = 0; t->mask[0] = 0; t->refCount = 1; t->filter = 1;
		initObject(g_textureTK, t);
	}
	return t;
}
// RwTextureDestroy 0x4d5bf0: drops one reference; the last one leaves the dictionary and destroys the raster
int32_t __cdecl rw37_RwTextureDestroy(RwTexture37 *t){
	int32_t n = --t->refCount;
	if(n > 0) return 1;
	t->refCount = n + 1;
	deInitObject(g_textureTK, t);
	if(t->dict) unlink(&t->lInDictionary);
	if(t->raster){ rw37_RwRasterDestroy(t->raster); t->raster = NULL; }
	t->refCount--;
	flFree(TEXMOD(8, void *), t);
	return 1;
}
// FUN_004df790 (destroyTexture, recomp/tier1/destroyTexture.c): RwTextureDestroy as a callback that returns the texture
RwTexture37 *__cdecl rw37_destroyTexture(RwTexture37 *t){ rw37_RwTextureDestroy(t); return t; }
// RwTexDictionaryDestroy 0x4d5ac0 (RwTexDictionaryDestroy_004d5ac0): stops being current, destroys its textures, leaves the engine list
int32_t __cdecl rw37_RwTexDictionaryDestroy(RwTexDictionary37 *d){
	if(TEXMOD(0x10, RwTexDictionary37 *) == d) TEXMOD(0x10, RwTexDictionary37 *) = NULL;
	RwLLLink37 *head = &d->texturesInDict;
	for(RwLLLink37 *l = head->next; l != head; ){
		RwLLLink37 *next = l->next;
		if(!rw37_RwTextureDestroy((RwTexture37 *)((uint8_t *)l - offsetof(RwTexture37, lInDictionary)))) break;
		l = next;
	}
	deInitObject(g_texDictTK, d);
	unlink(&d->lInInstance);
	flFree(TEXMOD(0xc, void *), d);
	return 1;
}
// RwTextureSetName 0x4d5c70 / RwTextureSetMaskName 0x4d5ce0: rwstrncpy 32; a name of 32+ chars is cut to 31 with error E_RW_NOMEM-like
// code 0x8000001e (_rwerror 0x53eac0 returns its first argument) through RwErrorSet
static RwTexture37 *setName(RwTexture37 *t, char *dst, const char *name){
	ENG(RW37_STRNCPY, StrNCpyFn)(dst, name, 32);
	if(ENG(RW37_STRLEN, StrLenFn)(name) >= 32){
		int32_t err[2] = {1, (int32_t)0x8000001e};
		g_errorSet(err);
		dst[31] = 0;
	}
	return t;
}
RwTexture37 *__cdecl rw37_RwTextureSetName(RwTexture37 *t, const char *name){ return setName(t, t->name, name); }
RwTexture37 *__cdecl rw37_RwTextureSetMaskName(RwTexture37 *t, const char *name){ return setName(t, t->mask, name); }
// RwTexDictionaryAddTexture 0x4d5d50: leaves its old dictionary, joins the front of the new one
RwTexture37 *__cdecl rw37_RwTexDictionaryAddTexture(RwTexDictionary37 *d, RwTexture37 *t){
	if(t->dict) unlink(&t->lInDictionary);
	t->dict = d;
	RwLLLink37 *head = &d->texturesInDict, *l = &t->lInDictionary;
	l->prev = head; l->next = head->next; head->next->prev = l; head->next = l;
	return t;
}
// RwTexDictionaryFindNamedTexture 0x4d5d90: ASCII a-z case-insensitive on signed chars; after the loop the raw terminators must match
RwTexture37 *__cdecl rw37_RwTexDictionaryFindNamedTexture(RwTexDictionary37 *d, const char *name){
	RwLLLink37 *head = &d->texturesInDict;
	for(RwLLLink37 *l = head->next; l != head; l = l->next){
		RwTexture37 *t = (RwTexture37 *)((uint8_t *)l - offsetof(RwTexture37, lInDictionary));
		const signed char *a = (const signed char *)t->name, *b = (const signed char *)name;
		int same = 1;
		while(*a && *b){
			signed char x = *a, y = *b;
			if(x >= 'a' && x <= 'z') x -= 0x20;
			if(y >= 'a' && y <= 'z') y -= 0x20;
			if(x != y){ same = 0; break; }
			a++; b++;
		}
		if(same && *a == *b) return t;
	}
	return NULL;
}

}
