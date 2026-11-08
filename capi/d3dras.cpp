// D3DRAS (LIB_RW group d3dras, X2 of plan_engine_switch_window2_2026-11-08): the raster create / lock / destroy core of the RW 3.7 D3D9 driver
// (d3d9raster.obj) of MFL.exe, written op for op from the disassembly ("kernel on exe state": the rasters, their D3D9 extension, the video-memory
// raster list, the palette free list and the palette index stack stay the exe's; the device, the IDirect3D9 object and the free lists are the exe's).
//   _rwD3D9RasterLock 0x4dfcd0, _rwD3D9RasterUnlock 0x4e0040, _rwD3D9FindCorrectRasterFormat 0x4e13f0, _rwD3D9RasterCreate 0x4e20f0,
//   rwD3D9CreateTextureRaster 0x4e2730, _rwD3D9RasterDestroy 0x4e2e40, RwD3D9RasterCreate 0x4e32c0.
// Not here (exe): the format checks (_rwD3D9CheckValid*Format, CheckAutoMipmapGen*Format), the camera-texture / z-buffer creators, the video-memory list
// release / restore, RwRasterCreate / Destroy, _rwD3D9CheckRasterSize (group d3dconv), the raster stage get / set, RwTextureRasterGenerateMipmaps.
// Device (*0x7d2240): CreateTexture 23, CreateCubeTexture 25, UpdateSurface 30, GetRenderTargetData 32, CreateOffscreenPlainSurface 36; texture:
// Release 2, GetSurfaceLevel / GetCubeMapSurface 18; surface: Release 2, GetDesc 12, LockRect 13, UnlockRect 14; IDirect3D9 (*0x7d2238):
// CheckDeviceFormat 10, CheckDepthStencilMatch 12.  Entries come from the exe's free lists / memory functions through RwEngineInstance; no CRT heap.
// Exe-layout adapter: mflrw_d3dras_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612.c), table layout = enum B below; the last entry
// (GetClientRect) is 0 in the game (the DLL's own user32 import) and a recorder in the check.
// Check: ghidra tools/delink/rw_d3dras_check.sh (fake device tools/delink/fakedev.c + fakedev_raster.c; both sides run the same op sequences natively under Wine).
#include <stdint.h>
#include "rw37.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef RwRaster37 Ras;
typedef RwD3D9RasterExt37 Ext;

// bind table layout (an_hk_612.c builds it in this order)
enum B { B_DEVVAR, B_D3DVAR, B_ADAPTER, B_DEVTYPE, B_HWND, B_DEPTHSURF, B_BACKSURF, B_ENGINEVAR, B_EXTOFF, B_CAPS, B_ADAPTERFMT, B_DISPDEPTH, B_ZDEPTH,
         B_DISPFMT, B_DEPTHFMT, B_PFI, B_FMTTAB, B_PALCOUNT, B_PALCAP, B_PALNEXT, B_PALARRAY, B_PALFL, B_VIDLIST, B_VIDFL,
         B_STR_CAMSIZE, B_STR_TEXFAIL, B_STR_PALFMT,
         B_ERRORSET, B_ERRCODE, B_FLCREATE, B_RASCREATE, B_RASDESTROY, B_CHECKSIZE, B_VALIDTEX, B_VALIDCAMTEX, B_VALIDZTEX, B_AUTOMIPTEX, B_AUTOMIPCAMTEX,
         B_CREATECAMTEX, B_CREATEZ, B_GENMIPS, B_GETSTAGE, B_SETSTAGE, B_GETCLIENTRECT, B_N };
static void *const *g;

#define V(i, T) (*(T *)g[i])		// an exe variable
#define CALL(i, T) ((T)g[i])
typedef void *(__cdecl *ErrorSetFn)(i32 *err);
typedef i32 (__cdecl *ErrCodeFn)(i32 code, ...);
typedef void *(__cdecl *FlCreateFn)(u32 size, u32 entries, u32 align, u32 hint);
typedef Ras *(__cdecl *RasCreateFn)(i32 w, i32 h, i32 depth, u32 flags);
typedef i32 (__cdecl *RasFn)(Ras *r);
typedef void (__cdecl *CheckSizeFn)(i32 *w, i32 *h, u32 flags);
typedef i32 (__cdecl *FmtFn)(u32 d3dFormat);
typedef i32 (__cdecl *CreateFn)(Ras *r, Ext *e);
typedef void (__cdecl *GenMipsFn)(Ras *r, i32 unused);
typedef Ras *(__cdecl *GetStageFn)(u32 stage);
typedef void (__cdecl *SetStageFn)(Ras *r, u32 stage);
typedef i32 (__stdcall *ClientRectFn)(void *hwnd, i32 *rect);
extern "C" __declspec(dllimport) i32 __stdcall GetClientRect(void *hwnd, i32 *rect);	// user32 (the exe's import of the same function); table entry 0 = this one

static inline Ext *EXT(Ras *r){ return (Ext *)((u8 *)r + V(B_EXTOFF, i32)); }
static inline Ras *top(Ras *r){ Ras *p = r->parent; while (p->parent != p) p = p->parent; return p; }
static inline const u8 *fmtEntry(u32 f){ return (const u8 *)g[B_FMTTAB] + ((f >> 8) & 0xf) * 8; }	// {d3dFormat u32, depth u8, alpha u8} by format nibble (0x5ea0f0)
static inline u32 fmtD3D(u32 f){ return *(const u32 *)fmtEntry(f); }
static inline const u8 *pfi(u32 d3dFormat){ return (const u8 *)g[B_PFI] + d3dFormat * 4; }	// _rwD3D9PixelFormatInfo: alpha, depth, rwFormat u16
static inline u32 dispFmtRw(void){ return *(const u16 *)(pfi(V(B_DISPFMT, u32)) + 2); }
static inline u8 texCaps(void){ return ((const u8 *)g[B_CAPS])[0x3c]; }			// D3DCAPS9.TextureCaps low byte (0x80 ALPHAPALETTE)
static inline u32 caps2(void){ return *(const u32 *)((const u8 *)g[B_CAPS] + 0xc); }		// D3DCAPS9.Caps2 (0x40000000 CANAUTOGENMIPMAP)
static inline i32 isDepthFmt(i32 f){ return f >= 0x46 && f <= 0x50; }

static void error(i32 code){ i32 e[2] = {1, CALL(B_ERRCODE, ErrCodeFn)(code)}; CALL(B_ERRORSET, ErrorSetFn)(e); }
static void error2(int str){ i32 e[2] = {1, CALL(B_ERRCODE, ErrCodeFn)(2, g[str])}; CALL(B_ERRORSET, ErrorSetFn)(e); }

// RwEngineInstance memory functions: malloc 0x108, realloc 0x110, free-list alloc 0x118 / free 0x11c
static inline u8 *ENG(void){ return V(B_ENGINEVAR, u8 *); }
static inline void *flAlloc(void *list, u32 hint){ return (*(void *(__cdecl **)(void *, u32))(ENG() + 0x118))(list, hint); }
static inline void flFree(void *list, void *p){ (*(void (__cdecl **)(void *, void *))(ENG() + 0x11c))(list, p); }
static inline void *rwMalloc(u32 size, u32 hint){ return (*(void *(__cdecl **)(u32, u32))(ENG() + 0x108))(size, hint); }
static inline void *rwRealloc(void *p, u32 size, u32 hint){ return (*(void *(__cdecl **)(void *, u32, u32))(ENG() + 0x110))(p, size, hint); }

// COM calls (vtable slot n)
static inline void **VT(void *o){ return *(void ***)o; }
static inline void comRelease(void *o){ ((u32 (__stdcall *)(void *))VT(o)[2])(o); }
static inline i32 texGetLevel(void *t, u32 level, void **out){ return ((i32 (__stdcall *)(void *, u32, void **))VT(t)[18])(t, level, out); }
static inline i32 texGetCube(void *t, u32 face, u32 level, void **out){ return ((i32 (__stdcall *)(void *, u32, u32, void **))VT(t)[18])(t, face, level, out); }
static inline void surfGetDesc(void *s, u32 *desc){ ((i32 (__stdcall *)(void *, u32 *))VT(s)[12])(s, desc); }
static inline i32 surfLock(void *s, void *lr, const i32 *rect, u32 flags){ return ((i32 (__stdcall *)(void *, void *, const i32 *, u32))VT(s)[13])(s, lr, rect, flags); }
static inline void surfUnlock(void *s){ ((i32 (__stdcall *)(void *))VT(s)[14])(s); }
static inline void *DEV(void){ return V(B_DEVVAR, void *); }
static inline i32 devCreateTex(u32 w, u32 h, u32 levels, u32 usage, u32 fmt, u32 pool, void **out){
	void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, u32, void **, void *))VT(d)[23])(d, w, h, levels, usage, fmt, pool, out, 0);
}
static inline i32 devCreateCube(u32 edge, u32 levels, u32 usage, u32 fmt, u32 pool, void **out){
	void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, void **, void *))VT(d)[25])(d, edge, levels, usage, fmt, pool, out, 0);
}
static inline void devUpdateSurface(void *src, const i32 *rect, void *dst, const i32 *point){
	void *d = DEV(); ((i32 (__stdcall *)(void *, void *, const i32 *, void *, const i32 *))VT(d)[30])(d, src, rect, dst, point);
}
static inline void devGetRTData(void *rt, void *dst){ void *d = DEV(); ((i32 (__stdcall *)(void *, void *, void *))VT(d)[32])(d, rt, dst); }
static inline i32 devCreateOffscreen(u32 w, u32 h, u32 fmt, u32 pool, void **out){
	void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, void **, void *))VT(d)[36])(d, w, h, fmt, pool, out, 0);
}
// IDirect3D9 (*0x7d2238) on the exe's adapter / device type / adapter format
static inline i32 checkFmt(u32 usage, u32 fmt){
	void *o = V(B_D3DVAR, void *);
	return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, u32))VT(o)[10])(o, V(B_ADAPTER, u32), V(B_DEVTYPE, u32), V(B_ADAPTERFMT, u32), usage, 3, fmt);
}
static inline i32 dsMatch(u32 adapterFmt, u32 rtFmt, u32 dsFmt){
	void *o = V(B_D3DVAR, void *);
	return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32))VT(o)[12])(o, V(B_ADAPTER, u32), V(B_DEVTYPE, u32), adapterFmt, rtFmt, dsFmt);
}
static inline i32 tex(u32 fmt){ return checkFmt(0, fmt) >= 0; }
static inline i32 camRT(u32 fmt){ return checkFmt(1, fmt) >= 0 && dsMatch(V(B_ADAPTERFMT, u32), fmt, V(B_DEPTHFMT, u32)) >= 0; }

// the video-memory raster list (VideoMemoryRasters: {raster, next} nodes from VideoMemoryRastersFreeList)
static void vidAdd(Ras *r){
	u32 *n = (u32 *)flAlloc(V(B_VIDFL, void *), 0x30411);
	n[0] = (u32)(uintptr_t)r; n[1] = V(B_VIDLIST, u32); V(B_VIDLIST, u32) = (u32)(uintptr_t)n;
}
static void vidRemove(Ras *r){
	u32 head = V(B_VIDLIST, u32), prev = 0, n;
	for (n = head; n; prev = n, n = ((u32 *)(uintptr_t)n)[1]) if (((u32 *)(uintptr_t)n)[0] == (u32)(uintptr_t)r) break;
	if (!n) return;
	if (prev) ((u32 *)(uintptr_t)prev)[1] = ((u32 *)(uintptr_t)n)[1];
	else if (head == n) V(B_VIDLIST, u32) = ((u32 *)(uintptr_t)n)[1];
	flFree(V(B_VIDFL, void *), (void *)(uintptr_t)n);
}
// palettes: 0x404-byte entries of PaletteFreeList (created on first use), the index at +0x400 from the free-index stack, else MaxPaletteIndex++
static void *palAlloc(void){
	void *fl = V(B_PALFL, void *);
	if (!fl) { fl = CALL(B_FLCREATE, FlCreateFn)(0x404, 0x40, 4, 0x30411); V(B_PALFL, void *) = fl; }
	return flAlloc(fl, 0x30411);
}
static u32 palIndex(void){
	u32 n = V(B_PALCOUNT, u32);
	if (n) { n--; V(B_PALCOUNT, u32) = n; return V(B_PALARRAY, u16 *)[n]; }
	return V(B_PALNEXT, u32)++;
}
// a palette goes back: its index onto the stack (grown by 0x200 entries), the entry to the free list
static void palFree(Ext *e){
	u32 idx = *(u32 *)((u8 *)e->palette + 0x400);
	u32 cap = V(B_PALCAP, u32);
	if (V(B_PALCOUNT, u32) >= cap) {
		cap += 0x200; V(B_PALCAP, u32) = cap;
		u16 *a = V(B_PALARRAY, u16 *);
		V(B_PALARRAY, u16 *) = (u16 *)(a ? rwRealloc(a, cap * 2, 0x1030411) : rwMalloc(cap * 2, 0x1030411));
	}
	V(B_PALARRAY, u16 *)[V(B_PALCOUNT, u32)] = (u16)idx;
	V(B_PALCOUNT, u32)++;
	flFree(V(B_PALFL, void *), e->palette);
	e->palette = 0;
}
static inline void setAutoMip(Ras *r){ Ext *e = EXT(r); e->autoMipCompressed = (e->autoMipCompressed & 0xf1) | 1; }

extern "C" {

i32 __cdecl rw37_rwD3D9CreateTextureRaster(Ras *r, Ext *e);

i32 __cdecl mflrw_d3dras_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// _rwD3D9FindCorrectRasterFormat 0x4e13f0: the format a raster of this type gets for the requested flags on this device (the request when it is usable)
static inline u32 try4444(u32 f){ return tex(0x1a) ? f | 0x300 : f | 0x100; }
static inline u32 to565(u32 f){ return (f & ~0x400u) | 0x200; }
u32 __cdecl rw37__rwD3D9FindCorrectRasterFormat(u32 type, u32 flags){
	u32 f = flags & 0xff00, fmt;
	switch (type) {
	case 0: case 4:		// normal, texture
		if (!(f & 0xf00)) {
			if (f & 0x2000) {
				if (tex(0x29)) f |= (texCaps() & 0x80) ? 0x500 : 0x600;
				else f &= ~0x2000u;
				if (f & 0x2000) return f;
			}
			if (tex(0x15)) return f | 0x500;
			return try4444(f);
		}
		if (f & 0x4000) f = (f & ~0x4000u) | 0x2000;		// PAL4 is asked for as PAL8
		if (f & 0x2000) {
			if (tex(0x29) && ((f & 0xf00) != 0x500 || (texCaps() & 0x80))) return f;
			f &= ~0x2000u;
			if ((f & 0xf00) == 0x600) return V(B_DISPDEPTH, i32) > 16 && tex(0x16) ? f | 0x600 : to565(f);
			if (V(B_DISPDEPTH, i32) > 16) return tex(0x15) ? f | 0x500 : try4444(f & ~0x500u);
			return try4444(f & ~0x500u);
		}
		fmt = f & 0xf00;
		if (fmt == 0x500) return tex(0x15) ? f : try4444(f & ~0x500u);
		if (fmt == 0x600) return tex(0x16) ? f : to565(f);
		if (fmt == 0x300) return tex(0x1a) ? f : (f & ~0x200u) | 0x100;
		if (fmt == 0x400) {
			if (tex(0x32)) return f;
			f &= ~0x400u;
			return tex(0x16) ? f | 0x16 : f | 0x200;	// sic: 0x16, not 0x600
		}
		return f;
	case 5: {		// camera texture
		fmt = f & 0xf00;
		if (!fmt) {
			if (V(B_ZDEPTH, i32) == 16) return f | 0x200;
			return camRT(0x16) ? f | 0x600 : f | 0x200;
		}
		u32 d = fmtEntry(fmt)[4];
		if (V(B_ZDEPTH, i32) == 16) {
			if (d == 32) f = fmt == 0x500 ? (f & ~0x400u) | 0x300 : fmt == 0x600 ? (f & ~0x400u) | 0x200 : 0x200;
			if ((f & 0xf00) == 0x300 && !camRT(0x1a)) f = (f & ~0x200u) | 0x100;
			if ((f & 0xf00) != 0x100 || camRT(0x19)) return f;
			return (f & ~0x100u) | 0x200;
		}
		if (d == 16) f = fmt == 0x300 ? (f & ~0x200u) | 0x500 : fmt == 0x100 ? f | 0x500 : fmt == 0x200 ? f | 0x600 : 0x600;
		if ((f & 0xf00) != 0x500 || camRT(0x15)) return f;
		return (f & ~0x100u) | 0x600;
	}
	case 2:			// camera: the display mode's format, only when none is given
		if (!(f & 0xf00)) return dispFmtRw();
		error(0x8000000d);
		return f;
	case 1:			// z buffer
		if (!(f & 0xf00)) {
			if (V(B_ZDEPTH, i32) == 16) return 0x700;
			if (V(B_ZDEPTH, i32) == 32) return 0x900;
			return f;
		}
		error(0x8000000d);
		return f;
	default:
		error(0x8000000d);
		return f;
	}
}

// _rwD3D9RasterCreate 0x4e20f0 (RwStandardFunc RASTERCREATE): type / flags / format / depth and the extension of a raster RwRasterCreate made, then
// the device object (texture, camera texture, z buffer) unless it is "don't allocate" or has no size; camera rasters only get checked against the window
i32 __cdecl rw37__rwD3D9RasterCreate(void *out, Ras *r, u32 flags){
	Ext *e = EXT(r);
	r->cFlags = (u8)(flags & 0xf8); r->cType = (u8)(flags & 7);
	r->cpPixels = 0; r->palette = 0;
	e->texture = 0; e->palette = 0; e->alpha = 0; e->autoMipCompressed = 0; e->cubeFace = 0; e->lockedMipLevel = 0xff; e->lockedSurface = 0; e->d3dFormat = 0;
	u32 f = flags & 0xff00, fmt;
	switch (flags & 7) {
	case 0: case 4:
		fmt = f & 0xf00;
		if (!fmt) {
			if (f & 0x2000) {
				if (!CALL(B_VALIDTEX, FmtFn)(0x29)) return 0;
				f |= (texCaps() & 0x80) ? 0x500 : 0x600;
				r->depth = 8;
			} else if (V(B_DISPDEPTH, i32) > 16 && CALL(B_VALIDTEX, FmtFn)(0x15)) { f |= 0x500; r->depth = 32; }
			else if (CALL(B_VALIDTEX, FmtFn)(0x1a)) { f |= 0x300; r->depth = 16; }
			else { f |= 0x100; r->depth = 16; }
		} else {
			if (f & 0x4000) { error(0x8000000d); return 0; }
			if (f & 0x2000) {
				if (!CALL(B_VALIDTEX, FmtFn)(0x29)) return 0;
				if (fmt != 0x600 && !(fmt == 0x500 && (texCaps() & 0x80))) return 0;
				r->depth = 8;
			} else {
				i32 d3d = (i32)fmtD3D(f);
				if (!(isDepthFmt(d3d) ? CALL(B_VALIDZTEX, FmtFn)(d3d) : CALL(B_VALIDTEX, FmtFn)(d3d))) return 0;
				r->depth = fmtEntry(f)[4];
			}
		}
		if (!(f & 0x2000) && (flags & 0x9000) == 0x9000 && CALL(B_AUTOMIPTEX, FmtFn)(fmtD3D(f))) setAutoMip(r);
		break;
	case 5:
		if (!(f & 0xf00)) {
			if (V(B_DISPDEPTH, i32) == 16) { f |= 0x200; r->depth = 16; }
			else if (CALL(B_VALIDCAMTEX, FmtFn)(0x16)) { f |= 0x600; r->depth = 32; }
			else { f |= 0x200; r->depth = 16; }
		} else {
			u8 d = fmtEntry(f)[4];
			if (!CALL(B_VALIDCAMTEX, FmtFn)(fmtD3D(f))) { error(0x8000000d); return 0; }
			r->depth = d;
		}
		if ((flags & 0x9000) == 0x9000 && CALL(B_AUTOMIPCAMTEX, FmtFn)(fmtD3D(f))) setAutoMip(r);
		break;
	case 2:
		if (!(f & 0xf00)) f = dispFmtRw();
		else if (dispFmtRw() != f) { error(0x8000000d); return 0; }
		r->depth = V(B_DISPDEPTH, i32);
		break;
	case 1:
		fmt = f & 0xf00;
		if (!fmt) {
			if (V(B_ZDEPTH, i32) == 16) { r->depth = 16; f = 0x700; }
			else if (V(B_ZDEPTH, i32) == 32) { f = 0x900; r->depth = 32; }
		} else if (fmt == 0x700 && V(B_ZDEPTH, i32) == 16) r->depth = 16;
		else if (fmt == 0x900 && V(B_ZDEPTH, i32) == 32) r->depth = 32;
		else { error(0x8000000d); return 0; }
		break;
	default:
		error(0x8000000d);
		return 0;
	}
	r->cFormat = (u8)(f >> 8);
	if (!r->width || !r->height) {
		r->cFlags = 0x80; r->stride = 0;
		if (!r->depth) r->depth = 16;
		if (r->cType == 2) vidAdd(r);
		return 1;
	}
	switch (r->cType & 7) {
	case 0: case 4:
		if (r->cFlags & 0x80) return 1;
		return rw37_rwD3D9CreateTextureRaster(r, e) ? 1 : 0;
	case 5:
		if (r->cFlags & 0x80) return 1;
		if (!CALL(B_CREATECAMTEX, CreateFn)(r, e)) return 0;
		vidAdd(r);
		return 1;
	case 1:
		if (r->cFlags & 0x80) return 1;
		if (!CALL(B_CREATEZ, CreateFn)(r, e)) return 0;
		vidAdd(r);
		return 1;
	case 2: {
		i32 rc[4];
		(g[B_GETCLIENTRECT] ? CALL(B_GETCLIENTRECT, ClientRectFn) : (ClientRectFn)GetClientRect)(V(B_HWND, void *), rc);
		if (!(r->cFlags & 0x80) && (rc[2] < r->width || rc[3] < r->height)) { error2(B_STR_CAMSIZE); return 0; }
		u32 df = V(B_DISPFMT, u32);
		r->depth = df == 0x15 || df == 0x16 || df == 0x23 ? 32 : df == 0x17 || df == 0x18 || df == 0x19 ? 16 : 0;
		r->stride = 0; r->cpPixels = 0; r->cFlags = 0x80;
		r->originalWidth = r->width; r->originalHeight = r->height;
		e->d3dFormat = V(B_DISPFMT, u32);
		vidAdd(r);
		return 1;
	}
	default:
		return 1;
	}
}

// rwD3D9CreateTextureRaster 0x4e2730: the D3D9 texture of a normal / texture raster (palettised: P8 with a palette entry; depth formats: a DEFAULT pool
// depth-stencil texture on the video-memory list; cube rasters: a cube texture)
i32 __cdecl rw37_rwD3D9CreateTextureRaster(Ras *r, Ext *e){
	u32 f = (u32)r->cFormat << 8;
	if (!(f & 0x6000)) e->d3dFormat = fmtD3D(f);
	else {
		if ((f & 0xf00) != 0x600 && (f & 0xf00) != 0x500) { error2(B_STR_PALFMT); return 0; }
		void *p = palAlloc();
		e->palette = p;
		if (p) *(u32 *)((u8 *)p + 0x400) = palIndex();
		e->d3dFormat = 0x29;
	}
	e->alpha = fmtEntry(f)[5];
	CALL(B_CHECKSIZE, CheckSizeFn)(&r->width, &r->height, r->cType | f);
	u32 levels = (~f >> 15) & 1, usage = (e->autoMipCompressed & 0xf) ? 0x400 : 0;	// 1 level without rwRASTERFORMATMIPMAP, else all
	i32 hr;
	if (e->cubeFace & 0xf) hr = devCreateCube(r->width, levels, usage, e->d3dFormat, 1, &e->texture);
	else if (isDepthFmt((i32)e->d3dFormat)) {
		hr = devCreateTex(r->width, r->height, 1, 2, e->d3dFormat, 0, &e->texture);
		if (hr >= 0) vidAdd(r);
	} else hr = devCreateTex(r->width, r->height, levels, usage, e->d3dFormat, 1, &e->texture);
	if (hr < 0) {
		if (e->d3dFormat == 0x29 && e->palette) palFree(e);
		error2(B_STR_TEXFAIL);
		return 0;
	}
	if (e->palette) vidAdd(r);
	return 1;
}

// _rwD3D9RasterLock 0x4dfcd0 (RASTERLOCK, flags = level << 8 | rwRASTERLOCKWRITE 1 / READ 2): a surface of the top raster's texture (camera rasters:
// an offscreen copy, filled from the render target when reading), locked over the sub raster's rectangle; the raster shows the mip level's size
i32 __cdecl rw37__rwD3D9RasterLock(void **pixels, Ras *r, u32 flags){
	if (r->cpPixels) { error(0x8000000e); return 0; }
	Ras *t = top(r);
	Ext *e = EXT(r), *te = EXT(t);
	u8 level = (u8)(flags >> 8);
	u32 write = flags & 1, lf = write ? 0x800 : 0x8810;		// NOSYSLOCK, read-only: | READONLY | NO_DIRTY_UPDATE
	i32 rc[4] = { r->nOffsetX, r->nOffsetY, r->width + r->nOffsetX, r->height + r->nOffsetY };
	i32 hr;
	switch (r->cType & 7) {
	case 0: case 4:
		hr = (te->cubeFace & 0xf) ? texGetCube(te->texture, te->cubeFace >> 4, level, &e->lockedSurface) : texGetLevel(te->texture, level, &e->lockedSurface);
		if (hr < 0) return 0;
		if (surfLock(e->lockedSurface, &e->lockedPitch, t == r ? 0 : rc, lf) < 0) return 0;
		if (flags & 2) r->privateFlags |= 2;
		if (write) r->privateFlags |= 4;
		break;
	case 2: case 5: {
		void *s;
		u32 isCamTex = (r->cType & 7) == 5;
		if (isCamTex) {
			hr = (te->cubeFace & 0xf) ? texGetCube(te->texture, te->cubeFace >> 4, level, &s) : texGetLevel(te->texture, level, &s);
			if (hr < 0) return 0;
		} else s = te->swapChain ? te->texture : V(B_BACKSURF, void *);
		u32 desc[8];			// D3DSURFACE_DESC: Format 0, Width 0x18, Height 0x1c
		surfGetDesc(s, desc);
		hr = devCreateOffscreen(desc[6], desc[7], desc[0], 2, &e->lockedSurface);	// SYSTEMMEM
		if (hr >= 0) {
			u32 rd = flags & 2;
			if (rd) devGetRTData(s, e->lockedSurface);
			hr = surfLock(e->lockedSurface, &e->lockedPitch, t == r ? 0 : rc, lf);
			if (hr >= 0) {
				if (rd) r->privateFlags |= 2;
				if (write) r->privateFlags |= 4;
			} else comRelease(e->lockedSurface);
		}
		if (isCamTex) comRelease(s);
		if (hr < 0) return 0;
		break;
	}
	default:
		error(0x8000000e);
		return 0;
	}
	r->cpPixels = (u8 *)e->lockedBits;
	r->originalWidth = r->width; r->originalHeight = r->height;
	r->width >>= level & 31; r->height >>= level & 31;
	if (!r->width) r->width = 1;
	if (!r->height) r->height = 1;
	r->stride = e->lockedPitch;
	e->lockedMipLevel = level;
	*pixels = r->cpPixels;
	return 1;
}

// _rwD3D9RasterUnlock 0x4e0040 (RASTERUNLOCK): the surface goes back (camera rasters: written back to the render target first); a written level 0 of an
// automipmapped raster without hardware mipmap generation regenerates the mip levels
i32 __cdecl rw37__rwD3D9RasterUnlock(void *out, Ras *r, u32 flags){
	if (!r->cpPixels) { error(0x80000011); return 0; }
	Ext *e = EXT(r);
	switch (r->cType & 7) {
	case 0: case 4: {
		surfUnlock(e->lockedSurface);
		comRelease(e->lockedSurface);
		r->height = r->originalHeight; r->width = r->originalWidth; r->stride = 0; r->cpPixels = 0;
		u8 pf = r->privateFlags;
		r->privateFlags = pf & 0xf9;
		if ((pf & 4) && (r->cFormat & 0x10) && e->lockedMipLevel == 0 && !(e->autoMipCompressed & 0xf)) {
			e->lockedMipLevel = 0xff;
			CALL(B_GENMIPS, GenMipsFn)(top(r), 0);
		}
		return 1;
	}
	case 2: case 5: {
		r->width = r->originalWidth; r->height = r->originalHeight; r->stride = 0; r->cpPixels = 0;
		surfUnlock(e->lockedSurface);
		if (!(r->privateFlags & 4)) { comRelease(e->lockedSurface); r->privateFlags &= 0xf9; return 1; }
		Ras *t = top(r);
		Ext *te = EXT(t);
		u32 isCamTex = (r->cType & 7) == 5;
		void *s;
		i32 hr = 0;
		if (isCamTex) hr = (te->cubeFace & 0xf) ? texGetCube(te->texture, te->cubeFace >> 4, e->lockedMipLevel, &s) : texGetLevel(te->texture, e->lockedMipLevel, &s);
		else s = te->swapChain ? te->texture : V(B_BACKSURF, void *);
		if (hr >= 0) {
			if (t == r) devUpdateSurface(e->lockedSurface, 0, s, 0);
			else {
				i32 rc[4] = { r->nOffsetX, r->nOffsetY, r->width + r->nOffsetX, r->height + r->nOffsetY }, pt[2] = { r->nOffsetX, r->nOffsetY };
				devUpdateSurface(e->lockedSurface, rc, s, pt);
			}
			if (isCamTex) comRelease(s);
		}
		comRelease(e->lockedSurface);
		r->privateFlags &= 0xf9;
		if (isCamTex && (r->cFormat & 0x10) && e->lockedMipLevel == 0) {
			e->lockedMipLevel = 0xff;
			CALL(B_GENMIPS, GenMipsFn)(t, 0);
		}
		return 1;
	}
	default:
		error(0x80000011);
		return 0;
	}
}

// _rwD3D9RasterDestroy 0x4e2e40 (RASTERDESTROY): off every texture stage, off the video-memory list, palette and device object released
i32 __cdecl rw37__rwD3D9RasterDestroy(void *out, Ras *r, u32 flags){
	Ext *e = EXT(r);
	for (u32 s = 0; s < 8; s++) if (CALL(B_GETSTAGE, GetStageFn)(s) == r) CALL(B_SETSTAGE, SetStageFn)(0, s);
	if (r->parent != r) {		// sub raster: nothing of its own
		if (r->cType == 2) vidRemove(r);
		return 1;
	}
	switch (r->cType) {
	case 0: case 4:
		if (r->cFlags & 0x80) return 1;
		if (e->palette) { vidRemove(r); palFree(e); }
		else if (isDepthFmt((i32)e->d3dFormat)) vidRemove(r);
		if (e->texture) comRelease(e->texture);
		return 1;
	case 5:
		if (r->cFlags & 0x80) return 1;
		vidRemove(r);
		if (e->texture) comRelease(e->texture);
		return 1;
	case 1:
		if (r->cFlags & 0x80) return 1;
		vidRemove(r);
		if (e->texture != V(B_DEPTHSURF, void *) && e->texture) comRelease(e->texture);
		return 1;
	case 2:
		vidRemove(r);
		return 1;
	default:
		error(0x8000000d);
		return 0;
	}
}

// RwD3D9RasterCreate 0x4e32c0: a raster on a given D3D9 format (texture, camera texture = render target, depth formats = depth stencil), checked
// against the device first; DEFAULT pool rasters go on the video-memory list
Ras *__cdecl rw37_RwD3D9RasterCreate(i32 w, i32 h, u32 d3dFormat, u32 flags){
	if ((flags & 7) == 0 || (flags & 7) == 1) flags &= 0xffff6fffu;		// no automipmap / mipmap for normal and z-buffer rasters
	Ras *r = CALL(B_RASCREATE, RasCreateFn)(w, h, 0, flags | 0x80);
	if (!r) return 0;
	Ext *e = EXT(r);
	if ((flags & 0x9000) == 0x9000 && (caps2() & 0x40000000) && checkFmt(0x400, d3dFormat) == 0) e->autoMipCompressed = (e->autoMipCompressed & 0xf1) | 1;
	u32 usage = (e->autoMipCompressed & 0xf) ? 0x400 : 0, pool;
	if ((flags & 5) == 5) {			// camera texture: render target with a matching depth buffer
		pool = 0;
		if (checkFmt(1, d3dFormat) < 0 || dsMatch(V(B_ADAPTERFMT, u32), d3dFormat, V(B_DEPTHFMT, u32)) < 0) { CALL(B_RASDESTROY, RasFn)(r); return 0; }
		usage |= 1;
	} else if (d3dFormat >= 0x46 && d3dFormat <= 0x50) {
		pool = 0;
		u32 af = V(B_ADAPTERFMT, u32);
		if (checkFmt(2, d3dFormat) < 0 || dsMatch(af, af, d3dFormat) < 0) { CALL(B_RASDESTROY, RasFn)(r); return 0; }
		usage |= 2;
	} else {
		pool = 1;
		if (checkFmt(0, d3dFormat) < 0) { CALL(B_RASDESTROY, RasFn)(r); return 0; }
	}
	if (devCreateTex(r->width, r->height, (~flags >> 15) & 1, usage, d3dFormat, pool, &e->texture) < 0) { CALL(B_RASDESTROY, RasFn)(r); return 0; }
	if (pool == 0) vidAdd(r);
	r->cFlags &= 0x7f; r->cFormat &= 0xf0;
	e->d3dFormat = d3dFormat;
	if (d3dFormat >= 0x31545844 && d3dFormat <= 0x35545844) {		// DXT1..DXT5
		e->autoMipCompressed = (e->autoMipCompressed & 0xf) | 0x10;
		if (d3dFormat == 0x31545844) { e->alpha = 0; r->cFormat |= 2; }
		else { e->alpha = 1; r->cFormat |= 3; }
		r->depth = 16;
		return r;
	}
	e->autoMipCompressed &= 0xf;
	if (d3dFormat < 0x80) {
		const u8 *p = pfi(d3dFormat);
		e->alpha = p[0]; r->depth = p[1]; r->cFormat |= p[3];
		if (d3dFormat == 0x29) {
			void *pal = palAlloc();
			e->palette = pal;
			if (pal) *(u32 *)((u8 *)pal + 0x400) = palIndex();
		}
	}
	return r;
}

}
