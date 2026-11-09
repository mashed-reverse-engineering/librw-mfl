// D3DRAS2 (LIB_RW group d3dras2, lane X of plan_engine_switch_window3_2026-11-09): the rest of the RW 3.7 D3D9 raster module (d3d9raster.obj) of MFL.exe
// next to group d3dras (capi/d3dras.cpp), written op for op from the disassembly, kernel on exe state (rasters, their D3D9 extension, the video-memory raster
// list, the device, the IDirect3D9 object and the present parameters stay the exe's):
//   format checks _rwD3D9CheckValid{Texture,CameraTexture,ZBufferTexture,CameraCubeTexture}Format 0x4e3660 / 0x4e36a0 / 0x4e3700 / 0x4e3760,
//   _rwD3D9CheckAutoMipmapGen{Texture,CameraTexture,CubeTexture}Format 0x4e37c0 / 0x4e3810 / 0x4e3860, _rwD3D9CheckRasterFormat 0x4e1ad0,
//   rwD3D9CreateCameraTextureRaster 0x4e2a30, rwD3D9CreateZBufferRaster 0x4e2be0, rwD3D9RasterGetMipLevels 0x4e3200, rwD3D9TextureSetRaster 0x4e3280,
//   rwD3D9RasterSubRaster 0x4e3290, _rxD3D9VideoMemoryRasterListRelease 0x4e1b20, _rxD3D9VideoMemoryRasterListRestore 0x4e1be0.
// Device (*0x7d2240): EvictManagedResources 5, CreateAdditionalSwapChain 13, CreateTexture 23, CreateCubeTexture 25, CreateDepthStencilSurface 29,
// SetPaletteEntries 71; texture: Release 2, GetLevelCount 13; swap chain: Release 2, GetBackBuffer 5; IDirect3D9 (*0x7d2238): CheckDeviceFormat 10,
// CheckDepthStencilMatch 12.
// Exe-layout adapter: mflrw_d3dras2_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612_d3dras2.inc), table layout = enum B below; the last entry
// (GetClientRect) is 0 in the game (the DLL's own user32 import) and a recorder in the check.
// Check: ghidra tools/delink/rw_d3dras_check.sh (d3dras + d3dras2 together on the fake device tools/delink/fakedev.c + fakedev_raster.c).
#include <stdint.h>
#include <string.h>
#include "rw37.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef RwRaster37 Ras;
typedef RwD3D9RasterExt37 Ext;

// bind table layout (an_hk_612_d3dras2.inc builds it in this order)
enum B { B_DEVVAR, B_D3DVAR, B_ADAPTER, B_DEVTYPE, B_HWND, B_DEPTHSURF, B_EXTOFF, B_CAPS, B_ADAPTERFMT, B_DISPDEPTH, B_ZDEPTH, B_DEPTHFMT, B_PFI, B_FMTTAB,
         B_VIDLIST, B_PRESENT, B_DISPW, B_DISPH, B_STR_CAMPAL, B_STR_RASFAIL, B_STR_OOVM,
         B_ERRORSET, B_ERRCODE, B_CHECKSIZE, B_FINDFMT, B_GETCLIENTRECT, B_N };
static void *const *g;

#define V(i, T) (*(T *)g[i])		// an exe variable
#define CALL(i, T) ((T)g[i])
typedef void *(__cdecl *ErrorSetFn)(i32 *err);
typedef i32 (__cdecl *ErrCodeFn)(i32 code, ...);
typedef void (__cdecl *CheckSizeFn)(i32 *w, i32 *h, u32 flags);
typedef u32 (__cdecl *FindFmtFn)(u32 type, u32 flags);
typedef i32 (__stdcall *ClientRectFn)(void *hwnd, i32 *rect);
extern "C" __declspec(dllimport) i32 __stdcall GetClientRect(void *hwnd, i32 *rect);	// user32 (the exe's import of the same function); table entry 0 = this one

static const i32 OOVM = (i32)0x8876017c;			// D3DERR_OUTOFVIDEOMEMORY
static inline Ext *EXT(Ras *r){ return (Ext *)((u8 *)r + V(B_EXTOFF, i32)); }
static inline const u8 *fmtEntry(u32 f){ return (const u8 *)g[B_FMTTAB] + ((f >> 8) & 0xf) * 8; }	// {d3dFormat u32, depth u8, alpha u8} by format nibble
static inline const u8 *pfi(u32 d3dFormat){ return (const u8 *)g[B_PFI] + d3dFormat * 4; }	// _rwD3D9PixelFormatInfo: alpha, depth, rwFormat u16
static inline u32 P(int i){ return ((const u32 *)g[B_PRESENT])[i]; }		// D3DPRESENT_PARAMETERS 0x947740: BackBufferWidth 0, Height 1, Format 2, MultiSample 4 / 5
static inline u32 caps2(void){ return *(const u32 *)((const u8 *)g[B_CAPS] + 0xc); }
static inline i32 isDepthFmt(i32 f){ return f >= 0x46 && f <= 0x50; }
static inline i32 clientRect(void *hwnd, i32 *rc){ return (g[B_GETCLIENTRECT] ? CALL(B_GETCLIENTRECT, ClientRectFn) : (ClientRectFn)GetClientRect)(hwnd, rc); }
static void error2(int str){ i32 e[2] = {1, CALL(B_ERRCODE, ErrCodeFn)(2, g[str])}; CALL(B_ERRORSET, ErrorSetFn)(e); }

// COM calls (vtable slot n)
static inline void **VT(void *o){ return *(void ***)o; }
static inline void comRelease(void *o){ ((u32 (__stdcall *)(void *))VT(o)[2])(o); }
static inline void *DEV(void){ return V(B_DEVVAR, void *); }
static inline void devEvict(void){ void *d = DEV(); ((i32 (__stdcall *)(void *))VT(d)[5])(d); }
static inline i32 devCreateTex(u32 w, u32 h, u32 levels, u32 usage, u32 fmt, u32 pool, void **out){
	void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, u32, void **, void *))VT(d)[23])(d, w, h, levels, usage, fmt, pool, out, 0);
}
static inline i32 devCreateCube(u32 edge, u32 levels, u32 usage, u32 fmt, u32 pool, void **out){
	void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, void **, void *))VT(d)[25])(d, edge, levels, usage, fmt, pool, out, 0);
}
static inline i32 devCreateDS(u32 w, u32 h, u32 fmt, u32 ms, u32 msq, u32 discard, void **out){
	void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, u32, void **, void *))VT(d)[29])(d, w, h, fmt, ms, msq, discard, out, 0);
}
static inline i32 devSetPalette(u32 idx, void *pal){ void *d = DEV(); return ((i32 (__stdcall *)(void *, u32, void *))VT(d)[71])(d, idx, pal); }
static inline i32 devCreateSwap(u32 *pp, void **out){ void *d = DEV(); return ((i32 (__stdcall *)(void *, u32 *, void **))VT(d)[13])(d, pp, out); }
// IDirect3D9 (*0x7d2238) on the exe's adapter / device type / adapter format
static inline i32 checkFmt(u32 usage, u32 rtype, u32 fmt){
	void *o = V(B_D3DVAR, void *);
	return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32, u32))VT(o)[10])(o, V(B_ADAPTER, u32), V(B_DEVTYPE, u32), V(B_ADAPTERFMT, u32), usage, rtype, fmt);
}
static inline i32 dsMatch(u32 rtFmt, u32 dsFmt){
	void *o = V(B_D3DVAR, void *);
	return ((i32 (__stdcall *)(void *, u32, u32, u32, u32, u32))VT(o)[12])(o, V(B_ADAPTER, u32), V(B_DEVTYPE, u32), V(B_ADAPTERFMT, u32), rtFmt, dsFmt);
}
static inline i32 camRT(u32 rtype, u32 fmt){ return checkFmt(1, rtype, fmt) >= 0 && dsMatch(fmt, V(B_DEPTHFMT, u32)) >= 0; }
static inline i32 zSurf(u32 fmt){ return checkFmt(2, 1, fmt) >= 0 && dsMatch(V(B_ADAPTERFMT, u32), fmt) >= 0; }
// the depth-stencil format for the z-buffer depth (16: D16 else D16_LOCKABLE; 32: D24S8, D24X4S4, D32, else D24X8; other depths keep the current one)
static u32 zFormat(u32 cur){
	i32 zd = V(B_ZDEPTH, i32);
	if (zd == 16) return zSurf(0x49) ? 0x49 : 0x50;
	if (zd != 32) return cur;
	if (zSurf(0x4b)) return 0x4b;
	if (zSurf(0x4f)) return 0x4f;
	if (zSurf(0x47)) return 0x47;
	return 0x4d;
}
// the render-target texture of a camera texture raster (DEFAULT pool; cube rasters: a cube texture)
static i32 camCreate(Ras *r, Ext *e, u32 levels, u32 usage, u32 fmt){
	if (e->cubeFace & 0xf) return devCreateCube(r->width, levels, usage, fmt, 0, &e->texture);
	return devCreateTex(r->width, r->height, levels, usage, fmt, 0, &e->texture);
}

extern "C" {

i32 __cdecl mflrw_d3dras2_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// the device format checks 0x4e3660..0x4e3860 (texture = D3DRTYPE_TEXTURE 3, cube = D3DRTYPE_CUBETEXTURE 5)
i32 __cdecl rw37__rwD3D9CheckValidTextureFormat(u32 f){ return checkFmt(0, 3, f) >= 0; }
i32 __cdecl rw37__rwD3D9CheckValidCameraTextureFormat(u32 f){ return camRT(3, f); }
i32 __cdecl rw37__rwD3D9CheckValidZBufferTextureFormat(u32 f){ return checkFmt(2, 3, f) >= 0 && dsMatch(V(B_ADAPTERFMT, u32), f) >= 0; }
i32 __cdecl rw37__rwD3D9CheckValidCameraCubeTextureFormat(u32 f){ return camRT(5, f); }
// automipmap generation: only with D3DCAPS2_CANAUTOGENMIPMAP, and only a plain D3D_OK counts (D3DOK_NOAUTOGEN does not)
i32 __cdecl rw37__rwD3D9CheckAutoMipmapGenTextureFormat(u32 f){ return (caps2() & 0x40000000) ? checkFmt(0x400, 3, f) == 0 : 0; }
i32 __cdecl rw37__rwD3D9CheckAutoMipmapGenCameraTextureFormat(u32 f){ return (caps2() & 0x40000000) ? checkFmt(0x401, 3, f) == 0 : 0; }
i32 __cdecl rw37__rwD3D9CheckAutoMipmapGenCubeTextureFormat(u32 f){ return (caps2() & 0x40000000) ? checkFmt(0x400, 5, f) == 0 : 0; }

// _rwD3D9CheckRasterFormat 0x4e1ad0: type / flags of the raster, the format the device gives it and that format's depth
i32 __cdecl rw37__rwD3D9CheckRasterFormat(Ras *r, u32 flags){
	r->cType = (u8)(flags & 7);
	r->cFlags = (u8)(flags & 0xf8);
	u32 f = CALL(B_FINDFMT, FindFmtFn)(flags & 7, flags);
	r->cFormat = (u8)(f >> 8);
	r->depth = fmtEntry(f)[4];
	return 1;
}

// rwD3D9CreateCameraTextureRaster 0x4e2a30: a render-target texture in the raster's format, else (not out of video memory) a plain DEFAULT pool texture
// in the display format; the extension keeps the asked-for format, the raster takes the display format's
i32 __cdecl rw37_rwD3D9CreateCameraTextureRaster(Ras *r, Ext *e){
	u32 f = (u32)r->cFormat << 8;
	if (f & 0x6000) { error2(B_STR_CAMPAL); return 0; }
	e->d3dFormat = *(const u32 *)fmtEntry(f);
	e->alpha = fmtEntry(f)[5];
	CALL(B_CHECKSIZE, CheckSizeFn)(&r->width, &r->height, r->cType | f);
	devEvict();
	u32 levels = (~f >> 15) & 1, mip = (e->autoMipCompressed & 0xf) ? 0x400 : 0;
	i32 hr = camCreate(r, e, levels, mip | 1, e->d3dFormat);
	if (hr == OOVM) { error2(B_STR_OOVM); return 0; }
	if (hr >= 0) return 1;
	if (camCreate(r, e, levels, mip, P(2)) < 0) { error2(B_STR_RASFAIL); return 0; }
	r->cFormat = pfi(P(2))[3];
	r->depth = V(B_DISPDEPTH, i32);
	return 1;
}

// rwD3D9CreateZBufferRaster 0x4e2be0: a window-sized z buffer shares the device's depth-stencil surface, any other size gets its own
i32 __cdecl rw37_rwD3D9CreateZBufferRaster(Ras *r, Ext *e){
	e->d3dFormat = zFormat(e->d3dFormat);
	e->alpha = 0;
	i32 rc[4];
	clientRect(V(B_HWND, void *), rc);
	if (rc[2] == r->width && rc[3] == r->height) { e->texture = V(B_DEPTHSURF, void *); return 1; }
	devEvict();
	i32 hr = devCreateDS(r->width, r->height, e->d3dFormat, P(4), P(5), 0, &e->texture);
	if (hr == OOVM) { error2(B_STR_OOVM); return 0; }
	if (hr >= 0) return 1;
	error2(B_STR_RASFAIL);
	return 0;
}

// rwD3D9RasterGetMipLevels 0x4e3200 (RwStandardFunc): the top raster's level count (texture types: the D3D9 texture's, else the full chain of its size)
i32 __cdecl rw37_rwD3D9RasterGetMipLevels(i32 *out, Ras *r, i32 flags){
	Ras *t = r;
	while (t->parent != t) t = t->parent;
	if (t->cType != 4 && t->cType != 5 && t->cType != 0) { *out = 1; return 1; }
	void *tex = EXT(t)->texture;
	if (tex) { *out = ((i32 (__stdcall *)(void *))VT(tex)[13])(tex); return 1; }
	u32 m = (u32)(t->width > t->height ? t->width : t->height);
	i32 n = 0;
	while (m) { n++; m >>= 1; }
	*out = n;
	return 1;
}

// rwD3D9TextureSetRaster 0x4e3280 (RwStandardFunc): texture->raster = raster
i32 __cdecl rw37_rwD3D9TextureSetRaster(void **texture, Ras *r, i32 flags){ *texture = r; return 1; }

// rwD3D9RasterSubRaster 0x4e3290 (RwStandardFunc): a sub raster takes the parent's stride, depth, type and format, no pixels
i32 __cdecl rw37_rwD3D9RasterSubRaster(Ras *r, Ras *parent, i32 flags){
	r->stride = parent->stride;
	r->depth = parent->depth;
	r->cType = parent->cType;
	r->cFormat = parent->cFormat;
	r->cpPixels = 0;
	return 1;
}

// _rxD3D9VideoMemoryRasterListRelease 0x4e1b20 (device lost): the DEFAULT pool objects of the listed top rasters go (z buffers keep the device's own
// surface, window cameras their back buffer surface but not their swap chain pointer)
void __cdecl rw37__rxD3D9VideoMemoryRasterListRelease(void){
	for (u32 *n = V(B_VIDLIST, u32 *); n; ) {
		Ras *r = (Ras *)(uintptr_t)n[0];
		n = (u32 *)(uintptr_t)n[1];
		Ext *e = EXT(r);
		if (r->parent != r) continue;
		switch (r->cType) {
		case 0: case 4:
			if (e->texture && isDepthFmt((i32)e->d3dFormat)) { comRelease(e->texture); e->texture = 0; }
			break;
		case 1:
			if (e->texture && e->texture != V(B_DEPTHSURF, void *)) { comRelease(e->texture); e->texture = 0; }
			break;
		case 2:
			if (e->swapChain) comRelease(e->swapChain);
			break;
		case 5:
			if (e->texture) { comRelease(e->texture); e->texture = 0; }
			break;
		}
	}
}

// _rxD3D9VideoMemoryRasterListRestore 0x4e1be0 (device reset): the listed top rasters get their DEFAULT pool objects back; 0 when any device call of the
// walk failed (the last HRESULT carries over to the next rasters, a failed CreateAdditionalSwapChain does not count)
i32 __cdecl rw37__rxD3D9VideoMemoryRasterListRestore(void){
	i32 res = 1, hr = 0;
	for (u32 *n = V(B_VIDLIST, u32 *); n; ) {
		Ras *r = (Ras *)(uintptr_t)n[0];
		n = (u32 *)(uintptr_t)n[1];
		Ext *e = EXT(r);
		if (r->parent == r) switch (r->cType) {
		case 0: case 4:
			if (r->cFormat & 0x60) { if (e->palette) hr = devSetPalette(*(u32 *)((u8 *)e->palette + 0x400), e->palette); }
			else if (isDepthFmt((i32)e->d3dFormat)) hr = devCreateTex(r->width, r->height, 1, 2, e->d3dFormat, 0, &e->texture);
			break;
		case 5: {
			if (e->texture) break;
			u32 levels = (~((u32)r->cFormat << 8) >> 15) & 1, mip = (e->autoMipCompressed & 0xf) ? 0x400 : 0;
			hr = camCreate(r, e, levels, mip | 1, e->d3dFormat);
			if (hr >= 0 || hr == OOVM) break;
			hr = camCreate(r, e, levels, mip, P(2));
			r->cFormat = pfi(P(2))[3];
			r->depth = pfi(P(2))[1];
			e->d3dFormat = P(2);
			e->alpha = pfi(P(2))[0];
			break;
		}
		case 1:
			if (e->texture) {
				i32 rc[4];
				clientRect(V(B_HWND, void *), rc);
				r->width = rc[2]; r->height = rc[3];
				e->texture = V(B_DEPTHSURF, void *);
				e->d3dFormat = V(B_DEPTHFMT, u32);
				break;
			}
			e->d3dFormat = zFormat(e->d3dFormat);
			hr = devCreateDS(r->width < 1 ? 1 : r->width, r->height < 1 ? 1 : r->height, e->d3dFormat, P(4), P(5), 0, &e->texture);
			break;
		case 2:
			if (e->swapChain) {		// a window camera: a new swap chain on its window, the extension takes its back buffer (released at once: a weak pointer)
				u32 pp[14];
				i32 rc[4];
				memcpy(pp, g[B_PRESENT], sizeof pp);
				pp[7] = (u32)(uintptr_t)e->window;
				clientRect(e->window, rc);
				e->swapChain = 0;
				pp[0] = rc[2] == 0; pp[1] = rc[3] == 0;		// sic: the size comparisons, not the size
				pp[9] = 0;
				if (devCreateSwap(pp, &e->swapChain) != 0) break;
				void *sc = e->swapChain;
				hr = ((i32 (__stdcall *)(void *, u32, u32, void **))VT(sc)[5])(sc, 0, 0, &e->texture);
				comRelease(e->texture);
				e->d3dFormat = pp[2];
			} else {			// the device's back buffer: the size scaled from the old mode to the new one, the display format
				u32 dw = V(B_DISPW, u32), dh = V(B_DISPH, u32);
				if (dw && (u32)r->width != P(0)) r->width = (i32)((u32)r->width * P(0) / dw);
				if (dh && (u32)r->height != P(1)) r->height = (i32)((u32)r->height * P(1) / dh);
				r->depth = pfi(P(2))[1];
				r->cFormat = pfi(P(2))[3];
				e->d3dFormat = P(2);
				e->alpha = pfi(P(2))[0];
			}
			break;
		}
		if (hr < 0) res = 0;
	}
	return res;
}

}
