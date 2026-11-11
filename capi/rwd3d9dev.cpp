// D3D9DEV (group d3d9dev, lane K window 5 batch 4): the small device free-of-lifecycle rows of the D3D9 device layer of MFL.exe (rwcore d3d9device bucket), written op for op from the
// disassembly, on the exe's own globals and its IDirect3D9 / IDirect3DDevice9 objects:
//   _rwD3D9SetRenderTarget 0x4d7bb0, RwD3D9DeleteVertexDeclaration 0x4dbbc0, RwD3D9EngineSetRefreshRate 0x4da040, RwD3D9SetRenderTarget 0x4db110,
//   D3D9CalculateMaxMultisamplingLevels 0x4d88d0, rwD3D9CameraEndUpdate 0x4dac70, RwD3D9GetCurrentD3DDevice 0x4dafe0 (really the device state cache reset).
// Exe binding: mflrw_d3d9dev_bind(&RwEngineInstance, table), table order = enum D below.  Unbound: abort.  Check: tools/delink/rw_d3d9dev_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum D { D_DEVICE, D_CURSURF, D_RASTEXT, D_NUMDECL, D_DECLS, D_REFRESH, D_BBFMT, D_D3DOBJ, D_ADAPTER, D_ADAPTERTYPE, D_MSFMT, D_MSWIN, D_MAXMS, D_GLOBALS, D_NEEDCOPY,
         D_RENDERSURF, D_FILTER, D_DEPTHSURF, D_LASTSTATE, D_LASTSTREAM, D_MATFREELIST, D_LASTWORLDID, D_LASTMATRIX, D_FLCREATE, D_FLPURGE, D_MATSLOT, D_IDENT, D_MAXLIGHTS,
         D_LIGHTSCACHE, D_N };
static u8 **g_engine;
static void *g_m[D_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] d3d9dev called before mflrw_d3d9dev_bind\n"); abort(); }
	return *g_engine;
}
#define GU(i) ((u32 *)g_m[i])
#define VT(obj, off, Fn) (*(Fn *)((u8 *)*(void **)(obj) + (off)))
typedef long (__stdcall *DevSetRTFn)(void *dev, u32 index, void *surf);
typedef u32 (__stdcall *ReleaseFn)(void *obj);
typedef long (__stdcall *CubeSurfFn)(void *tex, u32 face, u32 level, void **pp);
typedef long (__stdcall *LevelSurfFn)(void *tex, u32 level, void **pp);
typedef u32 (__stdcall *ModeCountFn)(void *d3d, u32 adapter, u32 fmt);
typedef long (__stdcall *EnumModeFn)(void *d3d, u32 adapter, u32 fmt, u32 mode, u32 *out);
typedef long (__stdcall *MsCheckFn)(void *d3d, u32 adapter, u32 type, u32 fmt, u32 windowed, u32 mst, u32 *quality);
typedef long (__stdcall *StretchFn)(void *dev, u32 src, const int32_t *srcRect, void *dst, const int32_t *dstRect, u32 filter);
typedef void *(__cdecl *FlCreateFn)(u32 size, u32 a, u32 b, u32 hint);
typedef void (__cdecl *FlPurgeFn)(void *list);

extern "C" {

void __cdecl mflrw_d3d9dev_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < D_N; i++) g_m[i] = tbl[i];
}

// _rwD3D9SetRenderTarget 0x4d7bb0 (index, surface): sets render target `index` unless the cache says it is already set; 1 on success.
int32_t __cdecl rw37_rwD3D9SetRenderTargetSurface(u32 index, void *surf){
	u32 *cur = GU(D_CURSURF);
	if(cur[index] == (u32)(uintptr_t)surf) return 1;
	void *dev = *(void **)g_m[D_DEVICE];
	cur[index] = (u32)(uintptr_t)surf;
	return VT(dev, 0x94, DevSetRTFn)(dev, index, surf) >= 0;
}

// RwD3D9SetRenderTarget 0x4db110 (index, raster): the raster's texture surface (level 0, cube face from the raster extension byte 9 high nibble) becomes render target `index`; the
// surface reference taken for that is released again.  NULL raster: target NULL.
int32_t __cdecl rw37_RwD3D9SetRenderTarget(u32 index, void **raster){
	void *surf = 0;
	int32_t ok;
	if(raster){
		u8 *ext = *(u8 **)raster + *(int32_t *)g_m[D_RASTEXT];
		u32 al = ext[9];
		void *tex = *(void **)ext;
		if(al & 0xf) VT(tex, 0x48, CubeSurfFn)(tex, al >> 4, 0, &surf);
		else VT(tex, 0x48, LevelSurfFn)(tex, 0, &surf);
	}
	u32 *cur = GU(D_CURSURF);
	if(cur[index] != (u32)(uintptr_t)surf){
		void *dev = *(void **)g_m[D_DEVICE];
		cur[index] = (u32)(uintptr_t)surf;
		ok = VT(dev, 0x94, DevSetRTFn)(dev, index, surf) >= 0;
	}else ok = 1;
	if(surf) VT(surf, 8, ReleaseFn)(surf);
	return ok;
}

// RwD3D9DeleteVertexDeclaration 0x4dbbc0 (declaration): finds it in the exe's table (12 byte entries, the declaration first), releases it and clears the entry once the
// last reference is gone.  Returns the declaration count when it is not in the table, otherwise the reference count Release left (0 when the entry was cleared).
u32 __cdecl rw37_RwD3D9DeleteVertexDeclaration(void *decl){
	u32 n = *GU(D_NUMDECL), i;
	if(n == 0) return 0;
	u8 *tab = *(u8 **)g_m[D_DECLS];
	for(i = 0; i < n; i++) if(*(void **)(tab + 12 * i) == decl) break;
	if(i == n) return n;
	void *d = *(void **)(tab + 12 * i);
	u32 rc = VT(d, 8, ReleaseFn)(d);
	if(rc > 0) return rc;
	*(u32 *)(*(u8 **)g_m[D_DECLS] + 12 * i) = 0;
	return 0;
}

// RwD3D9EngineSetRefreshRate 0x4da040 (hz): stores the requested full screen refresh rate; for each of the three back buffer formats walks the adapter modes and, unless one has
// exactly that rate, falls back to the highest rate below it (0 when none), which is stored instead.
void __cdecl rw37_RwD3D9EngineSetRefreshRate(u32 hz){
	u32 *rate = GU(D_REFRESH), best = 0;
	*rate = hz;
	if(!hz) return;
	const u32 *fmt = GU(D_BBFMT);
	for(int k = 0; k < 3; k++){
		u32 f = fmt[k];
		void *d3d = *(void **)g_m[D_D3DOBJ];
		u32 n = VT(d3d, 0x18, ModeCountFn)(d3d, *GU(D_ADAPTER), f);
		if(!n) continue;
		do{
			u32 mode[4];
			d3d = *(void **)g_m[D_D3DOBJ];
			n--;
			VT(d3d, 0x1c, EnumModeFn)(d3d, *GU(D_ADAPTER), fmt[k], n, mode);
			u32 r = mode[2], cur = *rate;
			if(r == cur){ best = cur; break; }
			if(r < cur && best < r) best = r;
		}while(n);
	}
	if(*rate != best) *rate = best;
}

// D3D9CalculateMaxMultisamplingLevels 0x4d88d0: MaxMultisamplingLevels (g_m[D_MAXMS][0]) = highest of the multisample types 2..16 the adapter supports for the back buffer format,
// the other three counters 1; then the non maskable type: its quality level count (+1) goes to [2] and raises [0] when larger.
void __cdecl rw37_D3D9CalculateMaxMultisamplingLevels(void){
	u32 win = ~*GU(D_MSWIN) & 1, lvl, q, *ms = GU(D_MAXMS);
	void *d3d;
	ms[0] = 1; ms[1] = 1; ms[2] = 1; ms[3] = 1;
	for(lvl = 2; lvl <= 0x10; lvl++){
		d3d = *(void **)g_m[D_D3DOBJ];
		if(VT(d3d, 0x2c, MsCheckFn)(d3d, *GU(D_ADAPTER), *GU(D_ADAPTERTYPE), *GU(D_MSFMT), win, lvl, 0) >= 0) ms[0] = lvl;
	}
	q = lvl;
	d3d = *(void **)g_m[D_D3DOBJ];
	if(VT(d3d, 0x2c, MsCheckFn)(d3d, *GU(D_ADAPTER), *GU(D_ADAPTERTYPE), *GU(D_MSFMT), win, 1, &q) >= 0){
		ms[2] = q + 1;
		if(q + 1 > ms[0]) ms[0] = q + 1;
	}
}

// rwD3D9CameraEndUpdate 0x4dac70: when the camera's frame buffer raster (dgGGlobals + 0x60 -> {raster, ...}) is a texture raster (extension word 0x1c clear, cType 5) and a back
// buffer copy is pending, StretchRects the render surface's camera viewport rectangle (x / y words at +0x1c / +0x1e, width / height at +0xc / +0x10) into the texture's surface
// (filter 1 when the render surface flags 0x1000100 say so); the surface reference is released.  Clears the pending flag and dgGGlobals; returns 1.
int32_t __cdecl rw37_rwD3D9CameraEndUpdate(void){
	u8 *g = *(u8 **)g_m[D_GLOBALS];
	u32 extoff = *GU(D_RASTEXT);
	u8 *cam = *(u8 **)(g + 0x60);
	u8 *ras = *(u8 **)cam;
	if(*(u32 *)(ras + extoff + 0x1c) == 0 && ras[0x20] == 5 && *GU(D_NEEDCOPY) != 0){
		u32 al = ras[extoff + 9];
		void *tex = *(void **)(ras + extoff), *surf;
		if(al & 0xf) VT(tex, 0x48, CubeSurfFn)(tex, al >> 4, 0, &surf);
		else VT(tex, 0x48, LevelSurfFn)(tex, 0, &surf);
		int32_t x = (int16_t)*(u16 *)(cam + 0x1c), y = (int16_t)*(u16 *)(cam + 0x1e), rc[4];
		rc[0] = x; rc[1] = y; rc[2] = (int32_t)*(u32 *)(cam + 0xc) + x; rc[3] = (int32_t)*(u32 *)(cam + 0x10) + y;
		void *dev = *(void **)g_m[D_DEVICE];
		VT(dev, 0x88, StretchFn)(dev, *GU(D_RENDERSURF), rc, surf, rc, (*GU(D_FILTER) & 0x1000100) ? 1 : 0);
		VT(surf, 8, ReleaseFn)(surf);
		*GU(D_NEEDCOPY) = 0;
	}
	*GU(D_GLOBALS) = 0;
	return 1;
}

// RwD3D9GetCurrentD3DDevice 0x4dafe0 (name is a misnomer: it resets the device state caches): render targets (4) and depth surface cleared, last FVF / declaration / vertex shader /
// pixel shader / index buffer set to -1, the 4 vertex stream records {-1, 0, 0, pad}, the matrix cache freed (free list created first time, else purged after freeing the cached
// matrices through the engine's free list free), a fresh identity matrix allocated for the world matrix slot, the lights cache freed; returns the device pointer.
u32 __cdecl rw37_RwD3D9GetCurrentD3DDevice(void){
	u32 *cs = GU(D_CURSURF), *ls = GU(D_LASTSTATE), *st = GU(D_LASTSTREAM), *lm = GU(D_LASTMATRIX), k;
	cs[0] = 0; cs[1] = 0; cs[2] = 0; cs[3] = 0;
	*GU(D_DEPTHSURF) = 0;
	for(k = 0; k < 5; k++) ls[k] = 0xffffffffu;
	for(k = 0; k < 4; k++){ st[4 * k] = 0xffffffffu; st[4 * k + 1] = 0; st[4 * k + 2] = 0; }
	u32 fl = *GU(D_MATFREELIST);
	*GU(D_LASTWORLDID) = 0;
	if(fl){
		for(k = 0; k < 0x104; k++){
			if(lm[k]){
				(*(void (__cdecl **)(u32, u32))(eng() + 0x11c))(fl, lm[k]);
				fl = *GU(D_MATFREELIST);
				lm[k] = 0;
			}
		}
		((FlPurgeFn)g_m[D_FLPURGE])((void *)(uintptr_t)fl);
	}else{
		*GU(D_MATFREELIST) = (u32)(uintptr_t)((FlCreateFn)g_m[D_FLCREATE])(0x40, 0xf, 0x10, 0x30411);
		memset(lm, 0, 0x104 * 4);
	}
	void *m = (*(void *(__cdecl **)(u32, u32))(eng() + 0x118))(*GU(D_MATFREELIST), 0x30411);
	*GU(D_MATSLOT) = (u32)(uintptr_t)m;
	memcpy(m, g_m[D_IDENT], 0x40);
	*GU(D_MAXLIGHTS) = 0;
	if(*GU(D_LIGHTSCACHE)){
		(*(void (__cdecl **)(u32))(eng() + 0x10c))(*GU(D_LIGHTSCACHE));
		*GU(D_LIGHTSCACHE) = 0;
	}
	return *GU(D_DEVICE);
}

}
