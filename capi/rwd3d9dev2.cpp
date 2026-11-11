// D3D9DEV2 (group d3d9dev2, lane K window 5 batch 5): more rows of the D3D9 device layer of MFL.exe (rwcore d3d9device bucket), written op for op from the disassembly, on the
// exe's own globals and its IDirect3D9 / IDirect3DDevice9 objects:
//   RwD3D9CreateVertexDeclaration 0x4db9e0, rwD3D9IndexBuffer32bitsCreate 0x4db9b0, RwD3D9CameraAttachWindow 0x4da0f0, D3D9DeviceSystemStop 0x4d8e00, rwD3D9BeginScene 0x4d9e60,
//   D3D9DeviceReleaseVideoMemory 0x4d9c60, D3D9DeviceSystemStandards 0x4d8fe0, D3D9CreateDisplayModesList 0x4d9230, rwD3D9RasterShowRaster 0x4dad50, D3D9SetPresentParameters 0x4d8990,
//   rwD3D9CameraClear 0x4d9420, rwD3D9CameraBeginUpdate 0x4da2a0 and the d3d9raster driver functions (ctor, dtor, close, palette lock / unlock, render generic / fast / scaled, clear rect,
//   clear, set context) and _rwD3D9RasterConvertToNonPalettized 0x4e38b0)
// Exe binding: mflrw_d3d9dev2_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_d3d9dev2_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_DEVICE, E_NUMDECL, E_MAXDECL, E_DECLS,
         E_GETWINDOWRECT, E_FLAG947760, E_WINDOWHANDLE, E_PRESENT, E_EXTOFF, E_FMTINFO, E_RASTERCREATE, E_RASTERDESTROY,
         E_SYSSTARTED, E_DISPMODES, E_NUMDISPMODES, E_FLDCW, E_SSE, E_LASTMATRIX, E_MATFREELIST, E_FLDESTROY, E_MAXLIGHTS, E_LIGHTSCACHE, E_RASTERCLOSE, E_VBMCLOSE, E_IM2DCLOSE,
         E_RELVIDMEM, E_D3DOBJ, E_ADAPTER, E_9475F0, E_9475F4, E_RENDERSURF, E_DEPTHSURF, E_RASTLISTRESTORE, E_DYNVBRESTORE, E_RSRESET, E_IM2DOPEN, E_IM3DOPEN, E_RESTORECB,
         E_INSIDESCENE,
         E_MAXMODES, E_BBFORMATS, E_STDS, E_LASTSTATE, E_LASTSTREAM, E_LASTWORLDID, E_WORLDSLOT, E_IDENT, E_CURSURF, E_CURDEPTH, E_RELCB, E_IM3DCLOSE, E_DYNVBREL, E_RASTLISTREL,
         E_FLPURGEALL, E_FLCREATE, E_FLPURGE, E_TOGGLE, E_DYNVBDISCARD,
         E_ADAPTERTYPE, E_SELMS, E_SELMSNM, E_G7D226C, E_ZDEPTH, E_HASSTENCIL, E_REFRESH,
         E_GETCLIENTRECT, E_ISICONIC, E_GETWINDOWLONG, E_SETWINDOWLONG, E_GETMENU, E_ADJUSTRECT, E_SETWINDOWPOS, E_ATTACH, E_9475E4, E_9475E8, E_STENCILCLEAR, E_G9476F4, E_CLEARTBL,
         E_SETCW, E_FRAMEGETLTM, E_MATRIXINVERT, E_SETTRANSFORM, E_SETRENDERSTATE, E_VIEWTRANS, E_PROJTRANS, E_GLOBALS, E_ACTIVEVP, E_NEEDCOPY,
         E_VMRASTERS, E_VMFREELIST, E_PALIDXFREE, E_NUMPALFREE, E_MAXPALFREE, E_MAXPALIDX, E_PALFREELIST, E_RENDERGENERIC, E_CLEARGENERIC, E_CURRASTERTARGET, E_ERRORSET, E_ERRCODE,
         E_COPYQUAD, E_RENDERQUAD, E_STRSRCDST,
         E_IMGCREATE, E_IMGALLOC, E_IMGFROMRASTER, E_CHKCAMTEX, E_CHKCAMCUBE, E_RASTERDESTROY2, E_RASTERCREATE2, E_SETFROMIMAGE, E_IMGDESTROY, E_ZFORMAT,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] d3d9dev2 called before mflrw_d3d9dev2_bind\n"); abort(); }
	return *g_engine;
}
#define GU(i) ((u32 *)g_m[i])
#define VT(obj, off, Fn) (*(Fn *)((u8 *)*(void **)(obj) + (off)))
#define ENGFN(off, Fn) (*(Fn *)(eng() + (off)))
typedef long (__stdcall *CreateDeclFn)(void *dev, const void *elems, void **pp);
typedef long (__stdcall *CreateIBFn)(void *dev, u32 len, u32 usage, u32 fmt, u32 pool, void **pp, void *share);
typedef u32 (__stdcall *AddRefFn)(void *obj);
typedef u32 (__stdcall *ReleaseFn)(void *obj);
typedef void *(__cdecl *MallocFn)(u32 size, u32 hint);
typedef void *(__cdecl *ReallocFn)(void *p, u32 size, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void (__cdecl *FlFreeFn)(u32 fl, u32 p);
typedef int (__stdcall *GetWinRectFn)(void *hwnd, int32_t *rc);
typedef void *(__cdecl *RasterCreateFn)(u32 w, u32 h, u32 depth, u32 flags);
typedef void (__cdecl *RasterDestroyFn)(void *r);
typedef long (__stdcall *AddSwapFn)(void *dev, void *pp, void **sc);
typedef long (__stdcall *GetBBFn)(void *sc, u32 i, u32 type, void **pp);
typedef long (__stdcall *GetDescFn)(void *surf, u32 *desc);
typedef long (__stdcall *SetTexFn)(void *dev, u32 stage, void *tex);
typedef long (__stdcall *SetIdxFn)(void *dev, void *ib);
typedef long (__stdcall *SetStreamFn)(void *dev, u32 s, void *vb, u32 off, u32 stride);
typedef long (__stdcall *SetObjFn)(void *dev, void *o);
typedef long (__stdcall *DevTestFn)(void *dev);
typedef long (__stdcall *ResetFn)(void *dev, void *pp);
typedef long (__stdcall *GetModeFn)(void *d3d, u32 adapter, u32 *mode);
typedef long (__stdcall *GetRTFn)(void *dev, u32 i, void **pp);
typedef long (__stdcall *GetDSFn)(void *dev, void **pp);
typedef long (__stdcall *SetLightFn)(void *dev, u32 i, const void *l);
typedef long (__stdcall *LightEnFn)(void *dev, u32 i, int32_t e);
typedef u32 (__stdcall *ModeCountFn)(void *d3d, u32 adapter, u32 fmt);
typedef long (__stdcall *EnumModeFn)(void *d3d, u32 adapter, u32 fmt, u32 mode, void *out);
typedef long (__stdcall *DevSetRTFn)(void *dev, u32 index, void *surf);
typedef void *(__cdecl *FlAllocFn)(u32 fl, u32 hint);
typedef void *(__cdecl *FlCreateFn)(u32 size, u32 a, u32 b, u32 hint);
typedef void (__cdecl *FlPurgeFn)(void *list);
typedef long (__stdcall *SetPalFn)(void *dev, u32 number, const void *entries);
typedef int (__cdecl *F0)(void);
typedef void (__cdecl *V0)(void);
typedef void (__cdecl *V1)(u32);

extern "C" {

void __cdecl mflrw_d3d9dev2_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// rwD3D9IndexBuffer32bitsCreate 0x4db9b0 (count, IDirect3DIndexBuffer9 **pp): CreateIndexBuffer(count*4, usage 8, D3DFMT_INDEX32 0x66, D3DPOOL_MANAGED 1, pp, NULL) >= 0.
int32_t __cdecl rw37_rwD3D9IndexBuffer32bitsCreate(u32 count, void **pp){
	void *dev = *(void **)g_m[E_DEVICE];
	return VT(dev, 0x6c, CreateIBFn)(dev, count * 4, 8, 0x66, 1, pp, 0) >= 0;
}

// RwD3D9CreateVertexDeclaration 0x4db9e0 (D3DVERTEXELEMENT9 *elems, IDirect3DVertexDeclaration9 **pp): the exe keeps a table of 12 byte entries {decl, element count incl. the
// terminator, copy of the elements}.  A declaration with equal elements (all bytes up to and including the terminator, type 0x11 = UNUSED) is shared: AddRef, or recreated from
// the stored copy when its object is gone.  Otherwise the table grows by 16 entries when full (engine malloc 0x108 / realloc 0x110), the device creates the declaration (slot 86)
// and the entry takes the object and a copy of the elements (engine malloc, hint 0x40411).  *pp = the object; returns whether it is non null (0 when the device failed).
int32_t __cdecl rw37_RwD3D9CreateVertexDeclaration(const u8 *elems, void **pp){
	u32 cnt = 0, i, n;
	const u8 *e = elems + 4;
	while(*e != 0x11){ e += 8; cnt++; }
	cnt++;
	n = *GU(E_NUMDECL);
	for(i = 0; i < n; i++){
		u8 *ent = *(u8 **)g_m[E_DECLS] + 12 * i;
		if(*(u32 *)(ent + 4) != cnt) continue;
		const u32 *a = (const u32 *)elems, *b = *(const u32 **)(ent + 8);
		u32 k = cnt * 2;
		while(k && *a == *b){ a++; b++; k--; }
		if(k) continue;
		void *d = *(void **)ent;
		if(d) VT(d, 4, AddRefFn)(d);
		else{
			void *dev = *(void **)g_m[E_DEVICE];
			VT(dev, 0x158, CreateDeclFn)(dev, *(void **)(ent + 8), (void **)ent);
		}
		*pp = *(void **)(*(u8 **)g_m[E_DECLS] + 12 * i);
		return *pp != 0;
	}
	if(n >= *GU(E_MAXDECL)){
		u32 mx = *GU(E_MAXDECL) + 0x10;
		void *old = *(void **)g_m[E_DECLS], *p;
		*GU(E_MAXDECL) = mx;
		if(!old) p = ENGFN(0x108, MallocFn)(mx * 12, 0x1040411);
		else p = ENGFN(0x110, ReallocFn)(old, mx * 12, 0x1040411);
		*(void **)g_m[E_DECLS] = p;
	}
	void *dev = *(void **)g_m[E_DEVICE];
	if(VT(dev, 0x158, CreateDeclFn)(dev, elems, pp) < 0){
		*pp = 0;
		return 0;
	}
	n = *GU(E_NUMDECL);
	*(void **)(*(u8 **)g_m[E_DECLS] + 12 * n) = *pp;
	n = *GU(E_NUMDECL);
	*(u32 *)(*(u8 **)g_m[E_DECLS] + 12 * n + 4) = cnt;
	void *copy = ENGFN(0x108, MallocFn)(cnt * 8, 0x40411);
	n = *GU(E_NUMDECL);
	*(void **)(*(u8 **)g_m[E_DECLS] + 12 * n + 8) = copy;
	n = *GU(E_NUMDECL);
	memcpy(*(void **)(*(u8 **)g_m[E_DECLS] + 12 * n + 8), elems, cnt * 8);
	*GU(E_NUMDECL) = *GU(E_NUMDECL) + 1;
	return *pp != 0;
}

// RwD3D9CameraAttachWindow 0x4da0f0 (camera, hwnd): needs the system flag (0x947760) and a window other than the current one.  The camera's two rasters (+0x60 / +0x64) are
// destroyed, an additional swap chain is created from the exe's present parameters (width / height flags 1 when the window rect is empty in that dimension, window = hwnd, no
// auto depth stencil), its back buffer becomes the new +0x60 camera raster (flags 0x82, size and depth / format info from the surface description and the pixel format table;
// raster extension: surface, format info byte, format, swap chain, window) and a plain 1 flag raster the new +0x64.  The back buffer reference is released.  Returns 1, or 0.
int32_t __cdecl rw37_RwD3D9CameraAttachWindow(u8 *cam, void *hwnd){
	if(*GU(E_FLAG947760) == 0) return 0;
	if(*GU(E_WINDOWHANDLE) == (u32)(uintptr_t)hwnd) return 0;
	int32_t rc[4];
	(*(GetWinRectFn *)g_m[E_GETWINDOWRECT])(hwnd, rc);
	void *r = *(void **)(cam + 0x60);
	if(r){ *(u32 *)(cam + 0x60) = 0; ((RasterDestroyFn)g_m[E_RASTERDESTROY])(r); }
	r = *(void **)(cam + 0x64);
	if(r){ *(u32 *)(cam + 0x64) = 0; ((RasterDestroyFn)g_m[E_RASTERDESTROY])(r); }
	u32 pp[14], desc[8];
	memcpy(pp, g_m[E_PRESENT], 56);
	pp[0] = (rc[2] - rc[0]) == 0;
	pp[1] = (rc[3] - rc[1]) == 0;
	pp[7] = (u32)(uintptr_t)hwnd;
	pp[9] = 0;
	void *dev = *(void **)g_m[E_DEVICE], *sc = 0, *surf = hwnd;
	if(VT(dev, 0x34, AddSwapFn)(dev, pp, &sc) != 0) return 0;
	VT(sc, 0x14, GetBBFn)(sc, 0, 0, &surf);
	VT(surf, 0x30, GetDescFn)(surf, desc);
	u32 w = desc[6], h = desc[7], fmt = desc[0];
	u8 *ras = (u8 *)((RasterCreateFn)g_m[E_RASTERCREATE])(w, h, 0, 0x82);
	if(ras){
		const u8 *tab = (const u8 *)g_m[E_FMTINFO];
		*(u32 *)(ras + 0xc) = w;
		*(u32 *)(ras + 0x10) = h;
		*(u32 *)(ras + 0x14) = tab[fmt * 4 + 1];
		ras[0x23] = tab[fmt * 4 + 3];
		u8 *ext = ras + *GU(E_EXTOFF);
		*(void **)ext = surf;
		ext[8] = tab[fmt * 4];
		*(u32 *)(ext + 0x18) = fmt;
		*(u32 *)(ext + 0x20) = (u32)(uintptr_t)hwnd;
		*(void **)(ext + 0x1c) = sc;
		*(void **)(cam + 0x60) = ras;
	}
	ras = (u8 *)((RasterCreateFn)g_m[E_RASTERCREATE])(w, h, 0, 1);
	if(ras) *(void **)(cam + 0x64) = ras;
	VT(surf, 8, ReleaseFn)(surf);
	return 1;
}

// D3D9DeviceSystemStop 0x4d8e00: system not started; display mode list freed; x87 control word restored (exe routine 0x5dc42b = fldcw of the saved word) and, with SSE, the
// MXCSR flush to zero bit cleared; matrix cache freed through the engine free list (then the list destroyed), light count and light cache cleared; device state unset (8 textures,
// indices, 4 streams, pixel shader, vertex declaration, vertex shader); every cached vertex declaration released until its count is 0 and its element copy freed, the table freed;
// raster / vertex buffer manager / Im2D closed; the device released.  Returns 1.
int32_t __cdecl rw37_D3D9DeviceSystemStop(void){
	u32 k, n;
	*GU(E_SYSSTARTED) = 0;
	u32 dm = *GU(E_DISPMODES);
	if(dm){ ENGFN(0x10c, FreeFn)((void *)(uintptr_t)dm); *GU(E_DISPMODES) = 0; *GU(E_NUMDISPMODES) = 0; }
	((V0)g_m[E_FLDCW])();
	if(*GU(E_SSE)){
		u32 x;
		__asm__ volatile("stmxcsr %0" : "=m"(x));
		x &= 0xffff7fffu;
		__asm__ volatile("ldmxcsr %0" :: "m"(x));
	}
	u32 *lm = GU(E_LASTMATRIX);
	for(k = 0; k < 0x104; k++){
		if(lm[k]){
			ENGFN(0x11c, FlFreeFn)(*GU(E_MATFREELIST), lm[k]);
			lm[k] = 0;
		}
	}
	((V1)g_m[E_FLDESTROY])(*GU(E_MATFREELIST));
	u32 lc = *GU(E_LIGHTSCACHE);
	*GU(E_MATFREELIST) = 0;
	*GU(E_MAXLIGHTS) = 0;
	if(lc){ ENGFN(0x10c, FreeFn)((void *)(uintptr_t)lc); *GU(E_LIGHTSCACHE) = 0; }
	void *dev;
	for(k = 0; k < 8; k++){ dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x104, SetTexFn)(dev, k, 0); }
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x1a0, SetIdxFn)(dev, 0);
	for(k = 0; k < 4; k++){ dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x190, SetStreamFn)(dev, k, 0, 0, 0); }
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x1ac, SetObjFn)(dev, 0);
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x15c, SetObjFn)(dev, 0);
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x170, SetObjFn)(dev, 0);
	n = *GU(E_NUMDECL);
	if(n > 0){
		u32 i = 0, off = 0;
		do{
			if(*(u32 *)(*(u8 **)g_m[E_DECLS] + off) != 0){
				u32 rc;
				do{
					void *d = *(void **)(*(u8 **)g_m[E_DECLS] + off);
					rc = VT(d, 8, ReleaseFn)(d);
				}while(rc > 0);
			}
			void *el = *(void **)(*(u8 **)g_m[E_DECLS] + off + 8);
			if(el) ENGFN(0x10c, FreeFn)(el);
			n = *GU(E_NUMDECL);
			i++; off += 12;
		}while(i < n);
	}
	void *tab = *(void **)g_m[E_DECLS];
	if(tab){ ENGFN(0x10c, FreeFn)(tab); *(void **)g_m[E_DECLS] = 0; }
	*GU(E_NUMDECL) = 0;
	*GU(E_MAXDECL) = 0;
	((V0)g_m[E_RASTERCLOSE])();
	((V0)g_m[E_VBMCLOSE])();
	((V0)g_m[E_IM2DCLOSE])();
	dev = *(void **)g_m[E_DEVICE];
	VT(dev, 8, ReleaseFn)(dev);
	*(void **)g_m[E_DEVICE] = 0;
	return 1;
}

// D3D9DeviceReleaseVideoMemory 0x4d9c60: like the device state cache reset (RwD3D9GetCurrentD3DDevice 0x4dafe0: last FVF / declaration / shaders / index buffer = -1, stream
// records {-1, 0, 0}, matrix cache released and the free list purged or created, a fresh identity world matrix) but without render targets / lights; the device state is unset
// (8 textures, indices, 4 streams, pixel shader, vertex declaration, vertex shader), render target 0 / depth stencil set back to the saved surfaces (slot 37 / 39), targets 1-3
// cleared if set; Im2D / Im3D closed, dynamic vertex buffers and video memory rasters released, all free lists purged, the release callback called.  Returns 1.
int32_t __cdecl rw37_D3D9DeviceReleaseVideoMemory(void){
	u32 *ls = GU(E_LASTSTATE), *st = GU(E_LASTSTREAM), *lm = GU(E_LASTMATRIX), k;
	for(k = 0; k < 5; k++) ls[k] = 0xffffffffu;
	for(k = 0; k < 4; k++){ st[4 * k] = 0xffffffffu; st[4 * k + 1] = 0; st[4 * k + 2] = 0; }
	u32 fl = *GU(E_MATFREELIST);
	*GU(E_LASTWORLDID) = 0;
	if(fl){
		for(k = 0; k < 0x104; k++){
			if(lm[k]){
				ENGFN(0x11c, FlFreeFn)(fl, lm[k]);
				fl = *GU(E_MATFREELIST);
				lm[k] = 0;
			}
		}
		((FlPurgeFn)g_m[E_FLPURGE])((void *)(uintptr_t)fl);
	}else{
		*GU(E_MATFREELIST) = (u32)(uintptr_t)((FlCreateFn)g_m[E_FLCREATE])(0x40, 0xf, 0x10, 0x30411);
		memset(lm, 0, 0x104 * 4);
	}
	void *m = ENGFN(0x118, FlAllocFn)(*GU(E_MATFREELIST), 0x30411);
	*GU(E_WORLDSLOT) = (u32)(uintptr_t)m;
	memcpy(m, g_m[E_IDENT], 0x40);
	void *dev;
	for(k = 0; k < 8; k++){ dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x104, SetTexFn)(dev, k, 0); }
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x1a0, SetIdxFn)(dev, 0);
	for(k = 0; k < 4; k++){ dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x190, SetStreamFn)(dev, k, 0, 0, 0); }
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x1ac, SetObjFn)(dev, 0);
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x15c, SetObjFn)(dev, 0);
	dev = *(void **)g_m[E_DEVICE]; VT(dev, 0x170, SetObjFn)(dev, 0);
	u32 *cur = GU(E_CURSURF), rs = *GU(E_RENDERSURF);
	if(cur[0] != rs){
		dev = *(void **)g_m[E_DEVICE];
		cur[0] = rs;
		VT(dev, 0x94, DevSetRTFn)(dev, 0, (void *)(uintptr_t)rs);
	}
	for(k = 1; k < 4; k++){
		if(cur[k] != 0){
			dev = *(void **)g_m[E_DEVICE];
			cur[k] = 0;
			VT(dev, 0x94, DevSetRTFn)(dev, k, 0);
		}
	}
	u32 ds = *GU(E_DEPTHSURF);
	if(*GU(E_CURDEPTH) != ds){
		dev = *(void **)g_m[E_DEVICE];
		*GU(E_CURDEPTH) = ds;
		VT(dev, 0x9c, SetObjFn)(dev, (void *)(uintptr_t)ds);
	}
	((V0)g_m[E_IM2DCLOSE])();
	((V0)g_m[E_IM3DCLOSE])();
	((V0)g_m[E_DYNVBREL])();
	((V0)g_m[E_RASTLISTREL])();
	((V0)g_m[E_FLPURGEALL])();
	void (__cdecl *cb)(void) = *(void (__cdecl **)(void))g_m[E_RELCB];
	if(cb) cb();
	return 1;
}

// D3D9DeviceSystemStandards 0x4d8fe0 (table, unused, count): the first `count` entries of the standards table get the stub (exe 0x405e40, returns 0); with count > 1 the 27
// raster / camera / native texture standards are stored at their indices (1..0x1c, no bounds check).  g_m[E_STDS] = 28 function pointers: the stub, then the functions in the
// order of the original's constant pairs.  Returns 1.
int32_t __cdecl rw37_D3D9DeviceSystemStandards(u32 *out, int32_t a1, int32_t count){
	static const u8 idx[27] = {1, 0xa, 0x15, 0x14, 2, 3, 7, 6, 5, 4, 9, 8, 0xf, 0x10, 0x17, 0x18, 0xe, 0xd, 0x11, 0x12, 0x13, 0xb, 0xc, 0x19, 0x1b, 0x1a, 0x1c};
	void *const *fns = (void *const *)g_m[E_STDS];
	int32_t i;
	if(count > 0) for(i = 0; i < count; i++) out[i] = (u32)(uintptr_t)fns[0];
	if(count > 1) for(i = 26; i >= 0; i--) out[idx[i]] = (u32)(uintptr_t)fns[1 + i];
	return 1;
}

// D3D9CreateDisplayModesList 0x4d9230: the list of display modes (20 bytes each {width, height, rate, format, flag}) is (re)allocated for g_9475e0 + 1 entries (engine realloc /
// malloc, hint 0x1040411); entry 0 = the adapter's current mode, counted (flag 0, count 1) only for formats 21-25 and 35.  For each of the 3 back buffer formats every adapter mode
// is enumerated behind the list: a mode with the same width / height / format as an entry from 1 on only raises that entry's rate, else it is kept (flag 1).  The list is
// shrunk with realloc when it holds less than g_9475e0 + 1 modes.  (No return value used.)
void __cdecl rw37_D3D9CreateDisplayModesList(void){
	u32 maxn = *GU(E_MAXMODES), n, f;
	u8 *dm = *(u8 **)g_m[E_DISPMODES], *p;
	if(dm) p = (u8 *)ENGFN(0x110, ReallocFn)(dm, (maxn * 5 + 5) * 4, 0x1040411);
	else p = (u8 *)ENGFN(0x108, MallocFn)((maxn * 5 + 5) * 4, 0x1040411);
	*(u8 **)g_m[E_DISPMODES] = p;
	void *d3d = *(void **)g_m[E_D3DOBJ];
	VT(d3d, 0x20, GetModeFn)(d3d, *GU(E_ADAPTER), (u32 *)p);
	dm = *(u8 **)g_m[E_DISPMODES];
	u32 idx = *(u32 *)(dm + 0xc) - 0x15;
	static const u8 sel[15] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};
	if(idx <= 0xe && sel[idx] == 0){ *(u32 *)(dm + 0x10) = 0; *GU(E_NUMDISPMODES) = 1; }
	else *GU(E_NUMDISPMODES) = 0;
	const u32 *fmts = (const u32 *)g_m[E_BBFORMATS];
	for(f = 0; f < 3; f++){
		d3d = *(void **)g_m[E_D3DOBJ];
		int32_t count = (int32_t)VT(d3d, 0x18, ModeCountFn)(d3d, *GU(E_ADAPTER), fmts[f]);
		for(int32_t k = 0; k < count; k++){
			n = *GU(E_NUMDISPMODES);
			dm = *(u8 **)g_m[E_DISPMODES];
			d3d = *(void **)g_m[E_D3DOBJ];
			VT(d3d, 0x1c, EnumModeFn)(d3d, *GU(E_ADAPTER), fmts[f], k, dm + n * 20);
			n = *GU(E_NUMDISPMODES);
			dm = *(u8 **)g_m[E_DISPMODES];
			int32_t e = 1;
			if((int32_t)n > 1){
				for(; e < (int32_t)n; e++){
					const u32 *a = (const u32 *)(dm + n * 20), *b = (const u32 *)(dm + e * 20);
					if(a[0] == b[0] && a[1] == b[1] && a[3] == b[3]) break;
				}
			}
			if(e < (int32_t)n){
				u32 nr = *(u32 *)(dm + n * 20 + 8), *er = (u32 *)(dm + e * 20 + 8);
				if(*er < nr) *er = nr;
			}else{
				*(u32 *)(dm + n * 20 + 0x10) = 1;
				*GU(E_NUMDISPMODES) = *GU(E_NUMDISPMODES) + 1;
			}
		}
	}
	n = *GU(E_NUMDISPMODES);
	dm = *(u8 **)g_m[E_DISPMODES];
	if((int32_t)n < (int32_t)(*GU(E_MAXMODES) + 1)) *(void **)g_m[E_DISPMODES] = ENGFN(0x110, ReallocFn)(dm, n * 20, 0x1040411);
}

typedef long (__stdcall *CheckTypeFn)(void *d3d, u32 adapter, u32 type, u32 dfmt, u32 bfmt, u32 windowed);
typedef long (__stdcall *CheckFmtFn)(void *d3d, u32 adapter, u32 type, u32 afmt, u32 usage, u32 rtype, u32 fmt);
typedef long (__stdcall *DepthMatchFn)(void *d3d, u32 adapter, u32 type, u32 afmt, u32 bfmt, u32 dfmt);
typedef long (__stdcall *MsCheckFn)(void *d3d, u32 adapter, u32 type, u32 fmt, u32 windowed, u32 mst, u32 *quality);

// D3D9SetPresentParameters 0x4d8990 (display mode {width, height, rate, format}, flags, format): fills the exe's D3DPRESENT_PARAMETERS (0x947740, 14 words) and its neighbours.
// Flag bit 0 = full screen: Windowed 0, size and format from the mode, rate = min(mode rate, requested full screen rate), swap effect 2; else windowed: size flags 1 where the
// window rect (GetWindowRect of the window handle) is empty, format = third argument, swap effect 3.  Back buffer count 1, interval word 0x80000000 (also 0x947774).  Back buffer
// format = the format (X8R8G8B8 0x16 becomes A8R8G8B8 0x15 when CheckDeviceType likes it, unless the 0x7d226c switch is set, which also selects swap effect 1); auto depth
// stencil 1 with the depth bits chosen from the format (0x20 / 0x10 / 0 like rwD3D9BeginScene) and then the first depth format the adapter supports and matches: 32 bit
// D24S8 0x4b, D24X4S4 0x4f (stencil), D32 0x47, D24X8 0x4d (no stencil), else 16 bit D15S1 0x49 (stencil) or D16 0x50; z depth 0x10 when no 32 bit format fits.  Without the
// 0x7d226c switch the selected multisampling level / non maskable level give the multisample type and quality (quality - 1; non maskable: min(levels - 2, quality - 1)).
void __cdecl rw37_D3D9SetPresentParameters(const u32 *mode, u32 flags, u32 fmt){
	u32 *P = GU(E_PRESENT);
	u32 af;
	if(flags & 1){
		u32 fsr = *GU(E_REFRESH);
		af = mode[3];
		P[8] = 0; P[3] = 1;
		P[12] = mode[2];
		if(!(mode[2] < fsr)) P[12] = fsr;
		*GU(E_TOGGLE) = 0x80000000u;
		P[0] = mode[0]; P[1] = mode[1]; P[6] = 2;
	}else{
		int32_t rc[4];
		P[8] = 1; P[3] = 1; P[12] = 0; *GU(E_TOGGLE) = 0x80000000u;
		(*(GetWinRectFn *)g_m[E_GETWINDOWRECT])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), rc);
		P[6] = 3;
		P[0] = (rc[2] - rc[0]) == 0;
		P[1] = (rc[3] - rc[1]) == 0;
		af = fmt;
	}
	u32 bf;
	if(*GU(E_G7D226C) != 0){ P[6] = 1; bf = af; }
	else if(af == 0x16){
		void *d3d = *(void **)g_m[E_D3DOBJ];
		long hr = VT(d3d, 0x24, CheckTypeFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), af, 0x15, P[8]);
		bf = 0x15 + (hr < 0 ? 1 : 0);
	}else bf = af;
	P[2] = bf;
	P[9] = 1;
	u32 idx = bf - 0x15, depth;
	if(idx > 0xe) depth = 0;
	else{
		static const u8 sel[15] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0};
		depth = sel[idx] == 0 ? 0x20 : sel[idx] == 1 ? 0x10 : 0;
	}
	*GU(E_ZDEPTH) = depth;
	struct L{
		static bool ok(u32 af, u32 f){
			void *d3d = *(void **)g_m[E_D3DOBJ];
			if(VT(d3d, 0x28, CheckFmtFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), af, 2, 1, f) != 0) return false;
			d3d = *(void **)g_m[E_D3DOBJ];
			return VT(d3d, 0x30, DepthMatchFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), af, GU(E_PRESENT)[2], f) == 0;
		}
	};
	bool sixteen = depth == 0x10;
	if(depth == 0x20){
		if(L::ok(af, 0x4b)){ P[10] = 0x4b; *GU(E_HASSTENCIL) = 1; }
		else if(L::ok(af, 0x4f)){ P[10] = 0x4f; *GU(E_HASSTENCIL) = 1; }
		else if(L::ok(af, 0x47)){ P[10] = 0x47; *GU(E_HASSTENCIL) = 0; }
		else if(L::ok(af, 0x4d)){ P[10] = 0x4d; *GU(E_HASSTENCIL) = 0; }
		else{ *GU(E_ZDEPTH) = 0x10; sixteen = true; }
	}
	if(sixteen){
		if(L::ok(af, 0x49)){ P[10] = 0x49; *GU(E_HASSTENCIL) = 1; }
		else{ P[10] = 0x50; *GU(E_HASSTENCIL) = 0; }
	}
	P[4] = 0; P[5] = 0;
	if(*GU(E_G7D226C) != 0) return;
	u32 ms = *GU(E_SELMS), q = 0;
	if(!(ms > 1) && !(*GU(E_SELMSNM) > 1)) return;
	void *d3d = *(void **)g_m[E_D3DOBJ];
	long hr = VT(d3d, 0x2c, MsCheckFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), P[2], P[8], ms, &q);
	if(hr >= 0 && q > 0){
		P[4] = *GU(E_SELMS);
		P[5] = q - 1;
		if(P[4] != 0) P[6] = 1;
		return;
	}
	q = 0;
	d3d = *(void **)g_m[E_D3DOBJ];
	hr = VT(d3d, 0x2c, MsCheckFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), P[2], P[8], 1, &q);
	if(hr >= 0 && q > 0){
		u32 e = *GU(E_SELMSNM) - 2, c = q - 1;
		if(!(e <= c)) e = c;
		P[4] = 1; P[6] = 1; P[5] = e;
		return;
	}
	if(P[4] != 0) P[6] = 1;
}

// rwD3D9BeginScene 0x4d9e60: TestCooperativeLevel; DEVICENOTRESET (0x88760869): video memory released, Reset(present parameters); when that is INVALIDCALL (0x8876086c) and the
// system flag is set: display mode of the adapter -> format (stored in 0x9475f0 and the present parameters), the depth bits value (0x20 for formats 21 22 35, 0x10 for 23-25,
// else 0) to 0x9475f4, Reset again.  A failed result ends with InsideScene 0 and returns 0.  After a successful reset the render target / depth surfaces are fetched (and
// released), video memory rasters, dynamic vertex buffers, render states restored, cached lights set and enabled, Im2D / Im3D reopened (each only while the previous succeeded),
// the restore callback called.  Then BeginScene unless already inside a scene (returns the InsideScene value then).
static int32_t begin_scene_body(void){
	void *dev = *(void **)g_m[E_DEVICE];
	int32_t hr = VT(dev, 0xc, DevTestFn)(dev);
	if(hr == (int32_t)0x88760869){
		((V0)g_m[E_RELVIDMEM])();
		dev = *(void **)g_m[E_DEVICE];
		hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
		if(hr == (int32_t)0x8876086c){
			if(*GU(E_FLAG947760) == 0){ *GU(E_INSIDESCENE) = 0; return 0; }
			void *d3d = *(void **)g_m[E_D3DOBJ];
			u32 mode[4], v;
			VT(d3d, 0x20, GetModeFn)(d3d, *GU(E_ADAPTER), mode);
			u32 fmt = mode[3];
			*GU(E_9475F0) = fmt;
			GU(E_PRESENT)[2] = fmt;
			u32 idx = fmt - 0x15;
			if(idx > 0xe) v = 0;
			else{
				static const u8 sel[15] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0};
				v = sel[idx] == 0 ? 0x20 : sel[idx] == 1 ? 0x10 : 0;
			}
			*GU(E_9475F4) = v;
			dev = *(void **)g_m[E_DEVICE];
			hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
		}
		if(hr < 0){ *GU(E_INSIDESCENE) = 0; return 0; }
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0x98, GetRTFn)(dev, 0, (void **)g_m[E_RENDERSURF]);
		void *s = *(void **)g_m[E_RENDERSURF];
		VT(s, 8, ReleaseFn)(s);
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xa0, GetDSFn)(dev, (void **)g_m[E_DEPTHSURF]);
		s = *(void **)g_m[E_DEPTHSURF];
		VT(s, 8, ReleaseFn)(s);
		if(((F0)g_m[E_RASTLISTRESTORE])() && ((F0)g_m[E_DYNVBRESTORE])()){
			((V0)g_m[E_RSRESET])();
			int32_t ml = (int32_t)*GU(E_MAXLIGHTS);
			bool fail = false;
			if(ml > 0){
				int32_t i = 0, off = 0, r = 0;
				do{
					u8 *cache = *(u8 **)g_m[E_LIGHTSCACHE];
					dev = *(void **)g_m[E_DEVICE];
					VT(dev, 0xcc, SetLightFn)(dev, i, cache + off);
					cache = *(u8 **)g_m[E_LIGHTSCACHE];
					dev = *(void **)g_m[E_DEVICE];
					r = VT(dev, 0xd4, LightEnFn)(dev, i, *(int32_t *)(cache + off + 0x68));
					i++; off += 0x6c;
				}while(i < (int32_t)*GU(E_MAXLIGHTS));
				fail = r < 0;
			}
			if(!fail && ((F0)g_m[E_IM2DOPEN])()) ((F0)g_m[E_IM3DOPEN])();
		}
		void (__cdecl *cb)(void) = *(void (__cdecl **)(void))g_m[E_RESTORECB];
		if(cb) cb();
	}
	if(hr < 0){ *GU(E_INSIDESCENE) = 0; return 0; }
	if(*GU(E_INSIDESCENE) != 0) return *GU(E_INSIDESCENE);
	dev = *(void **)g_m[E_DEVICE];
	if(VT(dev, 0xa4, DevTestFn)(dev) < 0) return *GU(E_INSIDESCENE);
	*GU(E_INSIDESCENE) = 1;
	return 1;
}
extern "C" int32_t __cdecl rw37_rwD3D9BeginScene(void){ return begin_scene_body(); }

// the state restore block after a successful Reset (the same code sits in rwD3D9BeginScene and twice in rwD3D9RasterShowRaster): render target / depth surface fetched and
// released, video memory rasters + dynamic vertex buffers restored, render states reset, lights set / enabled, Im2D / Im3D reopened, restore callback.
static void restore_after_reset(void){
	void *dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0x98, GetRTFn)(dev, 0, (void **)g_m[E_RENDERSURF]);
	void *s = *(void **)g_m[E_RENDERSURF];
	VT(s, 8, ReleaseFn)(s);
	dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0xa0, GetDSFn)(dev, (void **)g_m[E_DEPTHSURF]);
	s = *(void **)g_m[E_DEPTHSURF];
	VT(s, 8, ReleaseFn)(s);
	if(((F0)g_m[E_RASTLISTRESTORE])() && ((F0)g_m[E_DYNVBRESTORE])()){
		((V0)g_m[E_RSRESET])();
		int32_t ml = (int32_t)*GU(E_MAXLIGHTS);
		bool fail = false;
		if(ml > 0){
			int32_t i = 0, off = 0, r = 0;
			do{
				u8 *cache = *(u8 **)g_m[E_LIGHTSCACHE];
				dev = *(void **)g_m[E_DEVICE];
				VT(dev, 0xcc, SetLightFn)(dev, i, cache + off);
				cache = *(u8 **)g_m[E_LIGHTSCACHE];
				dev = *(void **)g_m[E_DEVICE];
				r = VT(dev, 0xd4, LightEnFn)(dev, i, *(int32_t *)(cache + off + 0x68));
				i++; off += 0x6c;
			}while(i < (int32_t)*GU(E_MAXLIGHTS));
			fail = r < 0;
		}
		if(!fail && ((F0)g_m[E_IM2DOPEN])()) ((F0)g_m[E_IM3DOPEN])();
	}
	void (__cdecl *cb)(void) = *(void (__cdecl **)(void))g_m[E_RESTORECB];
	if(cb) cb();
}

typedef int (__stdcall *GetClientRectFn)(void *hwnd, int32_t *rc);
typedef int (__stdcall *IsIconicFn)(void *hwnd);
typedef u32 (__stdcall *GetWinLongFn)(void *hwnd, int idx);
typedef u32 (__stdcall *SetWinLongFn)(void *hwnd, int idx, u32 v);
typedef void *(__stdcall *GetMenuFn)(void *hwnd);
typedef int (__stdcall *AdjustRectFn)(int32_t *rc, u32 style, int menu, u32 ex);
typedef int (__stdcall *SetWinPosFn)(void *hwnd, void *after, int x, int y, int cx, int cy, u32 flags);
typedef long (__stdcall *SetVPFn)(void *dev, const u32 *vp);
typedef long (__stdcall *ClearFn)(void *dev, u32 count, void *rects, u32 flags, u32 color, u32 zbits, u32 stencil);
typedef long (__stdcall *StretchRectFn)(void *dev, void *src, const int32_t *sr, void *dst, const int32_t *dr, u32 filter);
typedef long (__stdcall *LevelSurfFn)(void *tex, u32 level, void **pp);
typedef long (__stdcall *CubeSurfFn)(void *tex, u32 face, u32 level, void **pp);
typedef void (__cdecl *AttachFn)(void *cam, void *hwnd);

// rwD3D9CameraClear 0x4d9420 (camera, RwRGBA *color, flags): colour = ARGB word of the RGBA bytes when flag bit 0 (clear colour).  The camera's frame buffer raster is +0x60, its z
// buffer raster +0x64, raster extension (at the exe's offset) +0 surface / texture, +9 face byte, +0x1c swap chain, +0x20 window.
//  - Swap chain raster (extension +0x1c set): the window's client size (GetClientRect of ext+0x20; 0 -> return 0) is compared with the raster parent size (+0x28 / +0x2c): equal ->
//    the z raster is recreated when it is a top level raster of another size; different and the raster is its own parent -> RwD3D9CameraAttachWindow.  Render target 0 / depth
//    stencil surface set from the rasters, viewport = the frame raster size, Clear with flags table[flags] (exe 0x5ea0d0), colour, z 1.0, stencil value.  Returns 1.
//  - Else the main window: client size 0 or iconic -> return 0; a size other than the cached (0x9475e4 / e8) one resets the device with the new size (video memory released, restore
//    block) and on failure goes back to the old size and adjusts the window rect / style (GetWindowLong, AdjustWindowRectEx, SetWindowPos); then raster type 5 (camera texture,
//    parent +0x20): render target from the texture surface (cube face from ext +9), z from the z raster, viewport, Clear (z raster missing: depth / stencil flags dropped), StretchRect
//    fallback when the surface is no render target; otherwise device surfaces and a Clear whose flags are built from the colour / depth / stencil bits.  Returns 1.
int32_t __cdecl rw37_rwD3D9CameraClear(u8 *cam, const u8 *color, u32 flags){
	u32 col = 0, cflag = flags & 1;
	if(cflag) col = ((u32)color[3] << 24) | ((u32)color[0] << 16) | ((u32)color[1] << 8) | color[2];
	u32 extoff = *GU(E_EXTOFF);
	u8 *ebp = *(u8 **)(cam + 0x60);
	u8 *par = *(u8 **)ebp;
	u8 *ebx = par + extoff;
	void *dev;
	int32_t rc[4];
	u32 *cursurf = GU(E_CURSURF);
	u32 vp[6];
	bool texpath = false;
	if(*(u32 *)(ebx + 0x1c) != 0){
		(*(GetClientRectFn *)g_m[E_GETCLIENTRECT])(*(void **)(ebx + 0x20), rc);
		if(rc[2] == 0 || rc[3] == 0) return 0;
		u8 *z;
		if((u32)rc[2] == *(u32 *)(par + 0x28) && (u32)rc[3] == *(u32 *)(par + 0x2c)){
			z = *(u8 **)(cam + 0x64);
			if(z && z == *(u8 **)z){
				if((u32)rc[2] != *(u32 *)(z + 0xc) || (u32)rc[3] != *(u32 *)(z + 0x10)){
					*(u32 *)(cam + 0x64) = 0;
					((RasterDestroyFn)g_m[E_RASTERDESTROY])(z);
					z = (u8 *)((RasterCreateFn)g_m[E_RASTERCREATE])(rc[2], rc[3], 0, 1);
					if(z) *(u8 **)(cam + 0x64) = z;
				}
			}
		}else{
			if(ebp == par){
				((AttachFn)g_m[E_ATTACH])(cam, *(void **)(ebx + 0x20));
				ebp = *(u8 **)(cam + 0x60);
				ebx = *(u8 **)ebp + extoff;
			}
			z = *(u8 **)(cam + 0x64);
		}
		u32 surf = *(u32 *)ebx;
		if(cursurf[0] != surf){
			dev = *(void **)g_m[E_DEVICE];
			cursurf[0] = surf;
			VT(dev, 0x94, DevSetRTFn)(dev, 0, (void *)(uintptr_t)surf);
		}
		if(z){
			u32 ds = *(u32 *)(*(u8 **)z + *GU(E_EXTOFF));
			if(*GU(E_CURDEPTH) != ds){
				dev = *(void **)g_m[E_DEVICE];
				*GU(E_CURDEPTH) = ds;
				VT(dev, 0x9c, SetObjFn)(dev, (void *)(uintptr_t)ds);
			}
		}else if(*GU(E_CURDEPTH) != 0){
			dev = *(void **)g_m[E_DEVICE];
			*GU(E_CURDEPTH) = 0;
			VT(dev, 0x9c, SetObjFn)(dev, 0);
		}
		vp[0] = 0; vp[1] = 0; vp[2] = *(u32 *)(ebp + 0xc); vp[3] = *(u32 *)(ebp + 0x10); vp[4] = 0; vp[5] = 0x3f800000;
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xbc, SetVPFn)(dev, vp);
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xac, ClearFn)(dev, 0, 0, ((u32 *)g_m[E_CLEARTBL])[flags], col, 0x3f800000, *GU(E_STENCILCLEAR));
		return 1;
	}
	(*(GetClientRectFn *)g_m[E_GETCLIENTRECT])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), rc);
	if(rc[2] == 0 || rc[3] == 0) return 0;
	if((*(IsIconicFn *)g_m[E_ISICONIC])((void *)(uintptr_t)*GU(E_WINDOWHANDLE)) != 0) return 0;
	if((u32)rc[2] != *GU(E_9475E4) || (u32)rc[3] != *GU(E_9475E8)){
		((V0)g_m[E_RELVIDMEM])();
		dev = *(void **)g_m[E_DEVICE];
		long hr = VT(dev, 0xc, DevTestFn)(dev);
		if(hr >= 0 && *GU(E_FLAG947760) != 0){ GU(E_PRESENT)[0] = rc[2]; GU(E_PRESENT)[1] = rc[3]; }
		dev = *(void **)g_m[E_DEVICE];
		hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
		bool ok = false, failed = hr < 0;
		if(!failed){
			dev = *(void **)g_m[E_DEVICE];
			VT(dev, 0x98, GetRTFn)(dev, 0, (void **)g_m[E_RENDERSURF]);
			void *s = *(void **)g_m[E_RENDERSURF];
			VT(s, 8, ReleaseFn)(s);
			dev = *(void **)g_m[E_DEVICE];
			VT(dev, 0xa0, GetDSFn)(dev, (void **)g_m[E_DEPTHSURF]);
			s = *(void **)g_m[E_DEPTHSURF];
			VT(s, 8, ReleaseFn)(s);
			if(((F0)g_m[E_RASTLISTRESTORE])() && ((F0)g_m[E_DYNVBRESTORE])()){
				((V0)g_m[E_RSRESET])();
				int32_t ml = (int32_t)*GU(E_MAXLIGHTS);
				bool lfail = false;
				if(ml > 0){
					int32_t i = 0, off = 0, r = 0;
					do{
						u8 *cache = *(u8 **)g_m[E_LIGHTSCACHE];
						dev = *(void **)g_m[E_DEVICE];
						VT(dev, 0xcc, SetLightFn)(dev, i, cache + off);
						cache = *(u8 **)g_m[E_LIGHTSCACHE];
						dev = *(void **)g_m[E_DEVICE];
						r = VT(dev, 0xd4, LightEnFn)(dev, i, *(int32_t *)(cache + off + 0x68));
						i++; off += 0x6c;
					}while(i < (int32_t)*GU(E_MAXLIGHTS));
					lfail = r < 0;
				}
				if(!lfail && ((F0)g_m[E_IM2DOPEN])() && ((F0)g_m[E_IM3DOPEN])()) ok = true;
			}
			void (__cdecl *cb)(void) = *(void (__cdecl **)(void))g_m[E_RESTORECB];
			if(cb) cb();
			if(ok){ *GU(E_9475E4) = rc[2]; *GU(E_9475E8) = rc[3]; }
			else{ ((V0)g_m[E_RELVIDMEM])(); failed = true; }
		}
		if(failed){
			if(*GU(E_FLAG947760) == 0) return 0;
			GU(E_PRESENT)[0] = *GU(E_9475E4); GU(E_PRESENT)[1] = *GU(E_9475E8);
			dev = *(void **)g_m[E_DEVICE];
			hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
			if(hr < 0) return 0;
			void *hw = (void *)(uintptr_t)*GU(E_WINDOWHANDLE);
			rc[2] = *GU(E_9475E4) + rc[0];
			rc[3] = *GU(E_9475E8) + rc[1];
			GetWinLongFn gwl = *(GetWinLongFn *)g_m[E_GETWINDOWLONG];
			if(gwl(hw, -0x10) & 0x1000000){
				u32 st = gwl((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x10) & 0xfeffffffu;
				(*(SetWinLongFn *)g_m[E_SETWINDOWLONG])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x10, st);
			}
			u32 ex = gwl((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x14);
			void *menu = (*(GetMenuFn *)g_m[E_GETMENU])((void *)(uintptr_t)*GU(E_WINDOWHANDLE));
			u32 st = gwl((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x10);
			(*(AdjustRectFn *)g_m[E_ADJUSTRECT])(rc, st, menu != 0, ex);
			(*(SetWinPosFn *)g_m[E_SETWINDOWPOS])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), 0, rc[0], rc[3], rc[2] - rc[0], rc[3] - rc[1], 6);
			restore_after_reset();
		}
	}
	if(par[0x20] == 5){
		u32 al = ebx[9];
		void *tex = *(void **)ebx, *surf;
		if(al & 0xf) VT(tex, 0x48, CubeSurfFn)(tex, (al & 0xff) >> 4, 0, &surf);
		else VT(tex, 0x48, LevelSurfFn)(tex, 0, &surf);
		u32 desc[8];
		VT(surf, 0x30, GetDescFn)(surf, desc);
		int32_t esi;
		if(desc[2] & 1){
			if(cursurf[0] != (u32)(uintptr_t)surf){
				dev = *(void **)g_m[E_DEVICE];
				cursurf[0] = (u32)(uintptr_t)surf;
				VT(dev, 0x94, DevSetRTFn)(dev, 0, surf);
			}
			u8 *z = *(u8 **)(cam + 0x64);
			if(z){
				u8 *zp = *(u8 **)z;
				u32 ty = zp[0x20] & 7;
				void *zs = *(void **)(zp + *GU(E_EXTOFF));
				if(ty == 1){
					if(*GU(E_CURDEPTH) != (u32)(uintptr_t)zs){
						dev = *(void **)g_m[E_DEVICE];
						*GU(E_CURDEPTH) = (u32)(uintptr_t)zs;
						VT(dev, 0x9c, SetObjFn)(dev, zs);
					}
				}else{
					void *lvl;
					VT(zs, 0x48, LevelSurfFn)(zs, 0, &lvl);
					if(*GU(E_CURDEPTH) != (u32)(uintptr_t)lvl){
						dev = *(void **)g_m[E_DEVICE];
						*GU(E_CURDEPTH) = (u32)(uintptr_t)lvl;
						VT(dev, 0x9c, SetObjFn)(dev, lvl);
					}
					VT(lvl, 8, ReleaseFn)(lvl);
				}
			}else if(*GU(E_CURDEPTH) != 0){
				dev = *(void **)g_m[E_DEVICE];
				*GU(E_CURDEPTH) = 0;
				VT(dev, 0x9c, SetObjFn)(dev, 0);
			}
			esi = 0;
		}else esi = (int32_t)0x80004005;
		int32_t x = (int16_t)*(u16 *)(ebp + 0x1c), y = (int16_t)*(u16 *)(ebp + 0x1e);
		vp[0] = x; vp[1] = y; vp[2] = *(u32 *)(ebp + 0xc); vp[3] = *(u32 *)(ebp + 0x10); vp[4] = 0; vp[5] = 0x3f800000;
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xbc, SetVPFn)(dev, vp);
		u32 fl = ((u32 *)g_m[E_CLEARTBL])[flags];
		if((fl & 6) && *(u32 *)(cam + 0x64) == 0) fl &= 0xf9;
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xac, ClearFn)(dev, 0, 0, fl, col, 0x3f800000, *GU(E_STENCILCLEAR));
		if(esi < 0){
			int32_t r2[4];
			x = (int16_t)*(u16 *)(ebp + 0x1c); y = (int16_t)*(u16 *)(ebp + 0x1e);
			r2[0] = x; r2[1] = y; r2[2] = *(u32 *)(ebp + 0xc) + x; r2[3] = *(u32 *)(ebp + 0x10) + y;
			dev = *(void **)g_m[E_DEVICE];
			VT(dev, 0x88, StretchRectFn)(dev, *(void **)g_m[E_RENDERSURF], r2, surf, r2, (*GU(E_G9476F4) & 0x1000100) ? 1 : 0);
		}
		VT(surf, 8, ReleaseFn)(surf);
		return 1;
	}
	u32 rs = *GU(E_RENDERSURF);
	if(cursurf[0] != rs){
		dev = *(void **)g_m[E_DEVICE];
		cursurf[0] = rs;
		VT(dev, 0x94, DevSetRTFn)(dev, 0, (void *)(uintptr_t)rs);
	}
	u32 ds = *GU(E_DEPTHSURF);
	if(*GU(E_CURDEPTH) != ds){
		dev = *(void **)g_m[E_DEVICE];
		*GU(E_CURDEPTH) = ds;
		VT(dev, 0x9c, SetObjFn)(dev, (void *)(uintptr_t)ds);
	}
	{
		int32_t x = (int16_t)*(u16 *)(ebp + 0x1c), y = (int16_t)*(u16 *)(ebp + 0x1e);
		vp[0] = x; vp[1] = y; vp[2] = *(u32 *)(ebp + 0xc); vp[3] = *(u32 *)(ebp + 0x10); vp[4] = 0; vp[5] = 0x3f800000;
	}
	dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0xbc, SetVPFn)(dev, vp);
	u32 cf = 0;
	if(cflag) cf = 1;
	if(flags & 2){ cf |= 2; if(*GU(E_HASSTENCIL)) cf |= 4; }
	else if(flags & 4){ if(*GU(E_HASSTENCIL)) cf |= 4; }
	dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0xac, ClearFn)(dev, 0, 0, cf, col, 0x3f800000, *GU(E_STENCILCLEAR));
	return 1;
}

typedef void *(__cdecl *FrameGetLTMFn)(void *frame);
typedef void (__cdecl *MatInvertFn)(void *dst, void *src);
typedef void (__cdecl *SetTransformFn)(u32 state, void *m);
typedef void (__cdecl *SetRSFn)(u32 state, u32 value);

// rwD3D9CameraBeginUpdate 0x4da2a0 (unused, camera, unused): x87 precision set to 24 bit (exe 0x5dc432); dgGGlobals = camera; the view matrix (exe 0x947380) is the inverse of the camera frame's
// LTM with the first column's x components (elements 0 4 8 12) negated and 3 7 11 15 = 0 0 0 1, set as transform 2; the projection (exe 0x62e778) from the camera's view window,
// offsets and clip planes (floats at +0x70 .. +0x84, projection type word +0x14: 2 = parallel) as transform 3.  Then the same render target / depth surface / window size handling as
// rwD3D9CameraClear (swap chain raster, main window with device reset, camera texture raster with back buffer copy flag when it is no render target), the viewport, fog start /
// end render states from +0x88 / +0x84, and finally the body of rwD3D9BeginScene.  Returns its result, 0 on the early exits.
int32_t __cdecl rw37_rwD3D9CameraBeginUpdate(u32 a0, u8 *cam, u32 a2){
	((V0)g_m[E_SETCW])();
	*(u8 **)g_m[E_GLOBALS] = cam;
	void *ltm = ((FrameGetLTMFn)g_m[E_FRAMEGETLTM])(*(void **)(cam + 4));
	float *V = (float *)g_m[E_VIEWTRANS];
	((MatInvertFn)g_m[E_MATRIXINVERT])(V, ltm);
	V[0] = -V[0]; V[4] = -V[4]; V[8] = -V[8]; V[12] = -V[12];
	((u32 *)V)[3] = 0; ((u32 *)V)[7] = 0; ((u32 *)V)[11] = 0; ((u32 *)V)[15] = 0x3f800000;
	((SetTransformFn)g_m[E_SETTRANSFORM])(2, V);
	u32 *P = (u32 *)g_m[E_PROJTRANS];
	float *Pf = (float *)P;
	const u32 *cu = (const u32 *)cam;
	const float *cf = (const float *)cam;
	P[0] = cu[0x70 / 4];
	P[5] = cu[0x74 / 4];
	Pf[8] = cf[0x78 / 4] * cf[0x70 / 4];
	Pf[9] = cf[0x7c / 4] * cf[0x74 / 4];
	Pf[12] = -(cf[0x78 / 4] * cf[0x70 / 4]);
	Pf[13] = -(cf[0x7c / 4] * cf[0x74 / 4]);
	float d = cf[0x84 / 4] - cf[0x80 / 4], inv;
	if(cu[0x14 / 4] == 2){ inv = 1.0f / d; P[11] = 0; P[15] = 0x3f800000; }
	else{ inv = cf[0x84 / 4] / d; P[11] = 0x3f800000; P[15] = 0; }
	Pf[10] = inv;
	Pf[14] = -(Pf[10] * cf[0x80 / 4]);
	((SetTransformFn)g_m[E_SETTRANSFORM])(3, P);
	*GU(E_ACTIVEVP) = 0;
	u32 extoff = *GU(E_EXTOFF);
	u8 *edi = *(u8 **)(cam + 0x60), *edi0 = edi;
	u8 *par = *(u8 **)edi;
	u8 *ebp = par + extoff;
	void *dev;
	int32_t rc[4];
	u32 *cursurf = GU(E_CURSURF);
	bool viewport0 = false;      // true: viewport from the (possibly re-attached) raster, false: from the one saved at the start
	if(*(u32 *)(ebp + 0x1c) != 0){
		(*(GetClientRectFn *)g_m[E_GETCLIENTRECT])(*(void **)(ebp + 0x20), rc);
		if(rc[2] == 0 || rc[3] == 0) return 0;
		u8 *z;
		if((u32)rc[2] == *(u32 *)(par + 0x28) && (u32)rc[3] == *(u32 *)(par + 0x2c)){
			z = *(u8 **)(cam + 0x64);
			if(z && z == *(u8 **)z){
				if((u32)rc[2] != *(u32 *)(z + 0xc) || (u32)rc[3] != *(u32 *)(z + 0x10)){
					*(u32 *)(cam + 0x64) = 0;
					((RasterDestroyFn)g_m[E_RASTERDESTROY])(z);
					z = (u8 *)((RasterCreateFn)g_m[E_RASTERCREATE])(rc[2], rc[3], 0, 1);
					if(z) *(u8 **)(cam + 0x64) = z;
				}
			}
		}else{
			if(edi == par){
				((AttachFn)g_m[E_ATTACH])(cam, *(void **)(ebp + 0x20));
				edi = *(u8 **)(cam + 0x60);
				ebp = *(u8 **)edi + *GU(E_EXTOFF);
			}
			z = *(u8 **)(cam + 0x64);
		}
		u32 surf = *(u32 *)ebp;
		if(cursurf[0] != surf){
			dev = *(void **)g_m[E_DEVICE];
			cursurf[0] = surf;
			VT(dev, 0x94, DevSetRTFn)(dev, 0, (void *)(uintptr_t)surf);
		}
		if(z){
			u32 ds = *(u32 *)(*(u8 **)z + *GU(E_EXTOFF));
			if(*GU(E_CURDEPTH) != ds){
				dev = *(void **)g_m[E_DEVICE];
				*GU(E_CURDEPTH) = ds;
				VT(dev, 0x9c, SetObjFn)(dev, (void *)(uintptr_t)ds);
			}
		}else if(*GU(E_CURDEPTH) != 0){
			dev = *(void **)g_m[E_DEVICE];
			*GU(E_CURDEPTH) = 0;
			VT(dev, 0x9c, SetObjFn)(dev, 0);
		}
		viewport0 = true;
	}else{
		(*(GetClientRectFn *)g_m[E_GETCLIENTRECT])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), rc);
		if(rc[2] == 0 || rc[3] == 0) return 0;
		if((*(IsIconicFn *)g_m[E_ISICONIC])((void *)(uintptr_t)*GU(E_WINDOWHANDLE)) != 0) return 0;
		if((u32)rc[2] != *GU(E_9475E4) || (u32)rc[3] != *GU(E_9475E8)){
			((V0)g_m[E_RELVIDMEM])();
			dev = *(void **)g_m[E_DEVICE];
			long hr = VT(dev, 0xc, DevTestFn)(dev);
			if(hr >= 0 && *GU(E_FLAG947760) != 0){ GU(E_PRESENT)[0] = rc[2]; GU(E_PRESENT)[1] = rc[3]; }
			dev = *(void **)g_m[E_DEVICE];
			hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
			bool ok = false, failed = hr < 0;
			if(!failed){
				dev = *(void **)g_m[E_DEVICE];
				VT(dev, 0x98, GetRTFn)(dev, 0, (void **)g_m[E_RENDERSURF]);
				void *s = *(void **)g_m[E_RENDERSURF];
				VT(s, 8, ReleaseFn)(s);
				dev = *(void **)g_m[E_DEVICE];
				VT(dev, 0xa0, GetDSFn)(dev, (void **)g_m[E_DEPTHSURF]);
				s = *(void **)g_m[E_DEPTHSURF];
				VT(s, 8, ReleaseFn)(s);
				if(((F0)g_m[E_RASTLISTRESTORE])() && ((F0)g_m[E_DYNVBRESTORE])()){
					((V0)g_m[E_RSRESET])();
					int32_t ml = (int32_t)*GU(E_MAXLIGHTS);
					bool lfail = false;
					if(ml > 0){
						int32_t i = 0, off = 0, r = 0;
						do{
							u8 *cache = *(u8 **)g_m[E_LIGHTSCACHE];
							dev = *(void **)g_m[E_DEVICE];
							VT(dev, 0xcc, SetLightFn)(dev, i, cache + off);
							cache = *(u8 **)g_m[E_LIGHTSCACHE];
							dev = *(void **)g_m[E_DEVICE];
							r = VT(dev, 0xd4, LightEnFn)(dev, i, *(int32_t *)(cache + off + 0x68));
							i++; off += 0x6c;
						}while(i < (int32_t)*GU(E_MAXLIGHTS));
						lfail = r < 0;
					}
					if(!lfail && ((F0)g_m[E_IM2DOPEN])() && ((F0)g_m[E_IM3DOPEN])()) ok = true;
				}
				void (__cdecl *cb)(void) = *(void (__cdecl **)(void))g_m[E_RESTORECB];
				if(cb) cb();
				if(ok){ *GU(E_9475E4) = rc[2]; *GU(E_9475E8) = rc[3]; }
				else{ ((V0)g_m[E_RELVIDMEM])(); failed = true; }
			}
			if(failed){
				if(*GU(E_FLAG947760) == 0) return 0;
				GU(E_PRESENT)[1] = *GU(E_9475E8); GU(E_PRESENT)[0] = *GU(E_9475E4);
				dev = *(void **)g_m[E_DEVICE];
				hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
				if(hr < 0) return 0;
				void *hw = (void *)(uintptr_t)*GU(E_WINDOWHANDLE);
				rc[2] = *GU(E_9475E4) + rc[0];
				rc[3] = *GU(E_9475E8) + rc[1];
				GetWinLongFn gwl = *(GetWinLongFn *)g_m[E_GETWINDOWLONG];
				if(gwl(hw, -0x10) & 0x1000000){
					u32 st = gwl((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x10) & 0xfeffffffu;
					(*(SetWinLongFn *)g_m[E_SETWINDOWLONG])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x10, st);
				}
				u32 ex = gwl((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x14);
				void *menu = (*(GetMenuFn *)g_m[E_GETMENU])((void *)(uintptr_t)*GU(E_WINDOWHANDLE));
				u32 st = gwl((void *)(uintptr_t)*GU(E_WINDOWHANDLE), -0x10);
				(*(AdjustRectFn *)g_m[E_ADJUSTRECT])(rc, st, menu != 0, ex);
				(*(SetWinPosFn *)g_m[E_SETWINDOWPOS])((void *)(uintptr_t)*GU(E_WINDOWHANDLE), 0, rc[0], rc[3], rc[2] - rc[0], rc[3] - rc[1], 6);
				restore_after_reset();
			}
		}
		if(par[0x20] == 5){
			u32 al = ebp[9];
			void *tex = *(void **)ebp, *surf;
			if(al & 0xf) VT(tex, 0x48, CubeSurfFn)(tex, (al & 0xff) >> 4, 0, &surf);
			else VT(tex, 0x48, LevelSurfFn)(tex, 0, &surf);
			u32 desc[8];
			VT(surf, 0x30, GetDescFn)(surf, desc);
			if(desc[2] & 1){
				if(cursurf[0] != (u32)(uintptr_t)surf){
					dev = *(void **)g_m[E_DEVICE];
					cursurf[0] = (u32)(uintptr_t)surf;
					VT(dev, 0x94, DevSetRTFn)(dev, 0, surf);
				}
				u8 *z = *(u8 **)(cam + 0x64);
				if(z){
					u8 *zp = *(u8 **)z;
					u32 ty = zp[0x20] & 7;
					void *zs = *(void **)(zp + *GU(E_EXTOFF));
					if(ty == 1){
						if(*GU(E_CURDEPTH) != (u32)(uintptr_t)zs){
							dev = *(void **)g_m[E_DEVICE];
							*GU(E_CURDEPTH) = (u32)(uintptr_t)zs;
							VT(dev, 0x9c, SetObjFn)(dev, zs);
						}
						VT(surf, 8, ReleaseFn)(surf);
					}else{
						void *lvl;
						VT(zs, 0x48, LevelSurfFn)(zs, 0, &lvl);
						if(*GU(E_CURDEPTH) != (u32)(uintptr_t)lvl){
							dev = *(void **)g_m[E_DEVICE];
							*GU(E_CURDEPTH) = (u32)(uintptr_t)lvl;
							VT(dev, 0x9c, SetObjFn)(dev, lvl);
						}
						VT(lvl, 8, ReleaseFn)(lvl);
						VT(surf, 8, ReleaseFn)(surf);
					}
				}else{
					if(*GU(E_CURDEPTH) != 0){
						dev = *(void **)g_m[E_DEVICE];
						*GU(E_CURDEPTH) = 0;
						VT(dev, 0x9c, SetObjFn)(dev, 0);
					}
					VT(surf, 8, ReleaseFn)(surf);
				}
			}else{
				u32 rs = *GU(E_RENDERSURF);
				*GU(E_NEEDCOPY) = 1;
				if(cursurf[0] != rs){
					dev = *(void **)g_m[E_DEVICE];
					cursurf[0] = rs;
					VT(dev, 0x94, DevSetRTFn)(dev, 0, (void *)(uintptr_t)rs);
				}
				u32 ds = *GU(E_DEPTHSURF);
				if(*GU(E_CURDEPTH) != ds){
					dev = *(void **)g_m[E_DEVICE];
					*GU(E_CURDEPTH) = ds;
					VT(dev, 0x9c, SetObjFn)(dev, (void *)(uintptr_t)ds);
				}
				VT(surf, 8, ReleaseFn)(surf);
			}
		}else{
			u32 rs = *GU(E_RENDERSURF);
			if(cursurf[0] != rs){
				dev = *(void **)g_m[E_DEVICE];
				cursurf[0] = rs;
				VT(dev, 0x94, DevSetRTFn)(dev, 0, (void *)(uintptr_t)rs);
			}
			u32 ds = *GU(E_DEPTHSURF);
			if(*GU(E_CURDEPTH) != ds){
				dev = *(void **)g_m[E_DEVICE];
				*GU(E_CURDEPTH) = ds;
				VT(dev, 0x9c, SetObjFn)(dev, (void *)(uintptr_t)ds);
			}
		}
	}
	u8 *vr = viewport0 ? edi : edi0;
	u32 vp[6];
	vp[0] = (int32_t)(int16_t)*(u16 *)(vr + 0x1c); vp[1] = (int32_t)(int16_t)*(u16 *)(vr + 0x1e);
	vp[2] = *(u32 *)(vr + 0xc); vp[3] = *(u32 *)(vr + 0x10); vp[4] = 0; vp[5] = 0x3f800000;
	dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0xbc, SetVPFn)(dev, vp);
	((SetRSFn)g_m[E_SETRENDERSTATE])(0x24, cu[0x88 / 4]);
	((SetRSFn)g_m[E_SETRENDERSTATE])(0x25, cu[0x84 / 4]);
	return begin_scene_body();
}

// ---- d3d9raster standards: small raster driver functions (raster extension at the exe's offset: +4 palette / data pointer, +0x1c swap chain / texture object) ----
typedef int (__cdecl *RenderGenericFn)(u32 a, u32 b, u32 c, u32 d);
typedef int (__cdecl *ClearGenericFn)(u32 a, u32 b);
typedef int (__cdecl *ErrCodeFn)(u32 code);
typedef void (__cdecl *ErrorSetFn)(u32 *err);

// rwD3D9RasterCtor 0x4dfb80 (raster): swap chain / object slot (extension +0x1c) cleared; returns the raster.
void *__cdecl rw37_rwD3D9RasterCtor(u8 *raster){
	*(u32 *)(raster + *GU(E_EXTOFF) + 0x1c) = 0;
	return raster;
}

// rwD3D9RasterDtor 0x4dfba0 (raster): releases the object in extension +0x1c (slot 2) and clears it; returns the raster.
void *__cdecl rw37_rwD3D9RasterDtor(u8 *raster){
	u32 *slot = (u32 *)(raster + *GU(E_EXTOFF) + 0x1c);
	void *o = (void *)(uintptr_t)*slot;
	if(o){
		VT(o, 8, ReleaseFn)(o);
		*slot = 0;
	}
	return raster;
}

// D3D9 RasterClose 0x4dfbf0: video memory raster list entries ({?, next}) returned to their free list (engine free, +0x11c) one by one, the list destroyed, the palette index
// counters cleared and the palette index table (engine free +0x10c) and palette free list released.
void __cdecl rw37_rwD3D9RasterClose(void){
	u32 v = *GU(E_VMRASTERS);
	if(v){
		u32 next;
		do{
			next = *(u32 *)(uintptr_t)(v + 4);
			ENGFN(0x11c, FlFreeFn)(*GU(E_VMFREELIST), v);
			v = next;
			*GU(E_VMRASTERS) = v;
		}while(next != 0);
	}
	u32 fl = *GU(E_VMFREELIST);
	if(fl){
		((V1)g_m[E_FLDESTROY])(fl);
		*GU(E_VMFREELIST) = 0;
	}
	u32 pal = *GU(E_PALIDXFREE);
	*GU(E_NUMPALFREE) = 0;
	*GU(E_MAXPALFREE) = 0;
	*GU(E_MAXPALIDX) = 0;
	if(pal){
		ENGFN(0x10c, FreeFn)((void *)(uintptr_t)pal);
		*GU(E_PALIDXFREE) = 0;
	}
	fl = *GU(E_PALFREELIST);
	if(fl){
		((V1)g_m[E_FLDESTROY])(fl);
		*GU(E_PALFREELIST) = 0;
	}
}

// rwD3D9RasterLockPalette 0x4e02d0 (out, raster, mode): only raster types 0, 4 and 5 (raster->cType & 7) are palette rasters, any other sets error 0x8000000e (error struct {1, code
// from exe 0x53eac0}) and returns 0; else raster->+8 = extension +4, flags +0x22 |= 8 (| 0x10 too when mode bit 0), *out = extension +4.  Returns 1.
int32_t __cdecl rw37_rwD3D9RasterLockPalette(u32 *out, u8 *raster, u32 mode){
	u32 t = raster[0x20] & 7;
	if(t != 0 && ((int32_t)t <= 3 || (int32_t)t > 5)){
		u32 err[2];
		err[0] = 1;
		err[1] = ((ErrCodeFn)g_m[E_ERRCODE])(0x8000000e);
		((ErrorSetFn)g_m[E_ERRORSET])(err);
		return 0;
	}
	u32 p = *(u32 *)(raster + *GU(E_EXTOFF) + 4);
	u8 fl = raster[0x22];
	*(u32 *)(raster + 8) = p;
	raster[0x22] = (mode & 1) ? (fl | 0x18) : (fl | 8);
	*out = p;
	return 1;
}

// rwD3D9RasterUnlockPalette 0x4e0360 (unused, raster, unused): same type test (error 0x80000011); with the lock flag 0x10 set the palette (extension +4, number at its +0x400) is set on the device
// (slot 71); raster->+8 = 0, flags +0x22 &= 0xe7.  Returns 1.
int32_t __cdecl rw37_rwD3D9RasterUnlockPalette(u32 a0, u8 *raster, u32 a2){
	u32 t = raster[0x20] & 7;
	if(t != 0 && ((int32_t)t <= 3 || (int32_t)t > 5)){
		u32 err[2];
		err[0] = 1;
		err[1] = ((ErrCodeFn)g_m[E_ERRCODE])(0x80000011);
		((ErrorSetFn)g_m[E_ERRORSET])(err);
		return 0;
	}
	if(raster[0x22] & 0x10){
		u8 *p = *(u8 **)(raster + *GU(E_EXTOFF) + 4);
		void *dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0x11c, SetPalFn)(dev, *(u32 *)(p + 0x400), p);
	}
	u8 fl = raster[0x22];
	*(u32 *)(raster + 8) = 0;
	raster[0x22] = fl & 0xe7;
	return 1;
}

// rwD3D9RasterRenderGeneric 0x4e0410 (src, a1, a2, a3): dispatch on the current raster target's type byte (+0x20, > 5: error) and the source raster's type (+0x20 & 7, > 5: return 1):
// target 0 / 4 with source 0 2 4 5 -> CopyQuad(src, a1, a2) (exe 0x4e0ab0; 0 -> error), source 1 -> error, 3 -> return 1; target 2 / 5 with source 0 4 5 -> RenderQuad(src, a1, a2, a3)
// (exe 0x4e0560), source 2 -> CopyQuad, source 1 -> error, 3 -> return 1; target 1 / 3 -> error.  The error: {1, code of exe 0x53eac0(2, "SRC / DST Raster render combination ...")}
// via RwErrorSet, return 0.  NULL source: 0.  Returns 1 otherwise.
typedef int (__cdecl *CopyQuadFn)(u8 *src, u32 a1, u32 a2);
typedef void (__cdecl *RenderQuadFn)(u8 *src, u32 a1, u32 a2, u32 a3);
typedef int (__cdecl *ErrCode2Fn)(u32 code, void *str);
int32_t __cdecl rw37_rwD3D9RasterRenderGeneric(u8 *src, u32 a1, u32 a2, u32 a3){
	enum { RET0, RET1, COPY, RENDER, ERR };
	if(!src) return 0;
	u32 tt = ((const u8 *)(uintptr_t)*GU(E_CURRASTERTARGET))[0x20], act;
	if(tt > 5) act = ERR;
	else{
		static const u8 tab[6] = {0, 1, 2, 1, 0, 2};      // 0: group A, 1: error, 2: group B
		u32 st = src[0x20] & 7;
		if(tab[tt] == 1) act = ERR;
		else if(st > 5) act = RET1;
		else if(tab[tt] == 0){
			static const u8 A[6] = {COPY, ERR, COPY, RET1, COPY, COPY};
			act = A[st];
		}else{
			static const u8 B[6] = {RENDER, ERR, COPY, RET1, RENDER, RENDER};
			act = B[st];
		}
	}
	if(act == COPY){
		if(((CopyQuadFn)g_m[E_COPYQUAD])(src, a1, a2) != 0) return 1;
		act = ERR;
	}
	if(act == RENDER){ ((RenderQuadFn)g_m[E_RENDERQUAD])(src, a1, a2, a3); return 1; }
	if(act == ERR){
		u32 err[2];
		err[0] = 1;
		err[1] = ((ErrCode2Fn)g_m[E_ERRCODE])(2, g_m[E_STRSRCDST]);
		((ErrorSetFn)g_m[E_ERRORSET])(err);
		return 0;
	}
	return 1;
}

// rwD3D9RasterRenderFast 0x4e0d70 (a, b): RenderGeneric(a, b, 0, 0).
int32_t __cdecl rw37_rwD3D9RasterRenderFast(u32 a, u32 b){ return ((RenderGenericFn)g_m[E_RENDERGENERIC])(a, b, 0, 0); }
// rwD3D9RasterRenderScaled 0x4e0d90 (a, b): RenderGeneric(a, b, 1, 1).
int32_t __cdecl rw37_rwD3D9RasterRenderScaled(u32 a, u32 b){ return ((RenderGenericFn)g_m[E_RENDERGENERIC])(a, b, 1, 1); }
// rwD3D9RasterClearRect 0x4e0db0 (unused, rect, value): ClearGeneric(rect, value).
int32_t __cdecl rw37_rwD3D9RasterClearRect(u32 unused, u32 rect, u32 value){ return ((ClearGenericFn)g_m[E_CLEARGENERIC])(rect, value); }
// rwD3D9RasterClear 0x4e13a0 (unused, unused, value): ClearGeneric of the whole current raster target {0, 0, width (+0xc), height (+0x10)}.
int32_t __cdecl rw37_rwD3D9RasterClear(u32 a, u32 b, u32 value){
	u32 rect[4];
	const u8 *t = (const u8 *)(uintptr_t)*GU(E_CURRASTERTARGET);
	rect[0] = 0; rect[1] = 0;
	rect[2] = *(const u32 *)(t + 0xc);
	rect[3] = *(const u32 *)(t + 0x10);
	return ((ClearGenericFn)g_m[E_CLEARGENERIC])((u32)(uintptr_t)rect, value);
}
// rwD3D9SetRasterContext 0x4e13e0 (unused, raster): the current raster target = raster.  Returns 1.
int32_t __cdecl rw37_rwD3D9SetRasterContext(u32 a, u32 raster){ *GU(E_CURRASTERTARGET) = raster; return 1; }

typedef void *(__cdecl *ImgCreateFn)(u32 w, u32 h, u32 d);
typedef void (__cdecl *P1Fn)(void *p);
typedef void (__cdecl *ImgFromRasterFn)(void *img, void *parent, u32 z);
typedef int (__cdecl *ChkCamFmtFn)(u32 fmt);
typedef void (__cdecl *RasterCtorFn)(u32 a, void *raster, u32 flags);
typedef void (__cdecl *SetFromImgFn)(void *raster, void *img, u32 z);

// _rwD3D9RasterConvertToNonPalettized 0x4e38b0 (raster): the raster's pixels are read into a new 32 bit image (RwImageCreate / AllocatePixels / ImageGetFromRaster), then the raster
// is destroyed and created again (_rwD3D9RasterDestroy / Create, flags = raster type | 0x9000 | format class) and filled from the image (SetFromImage), the image destroyed.  Format
// class: palettised rasters (extension +8) 0x500 (render target formats 0x15, only with depth bits > 16 and a camera / texture raster type 0 4 5), 0x300 / 0x100 (format 0x1a);
// non palettised 0x600 / 0x200 (format 0x16).  Supported-ness: CheckDeviceFormat (slot 10; rtype 3 = texture, 5 = cube texture from the extension +9 low nibble) or the exe's
// _rwD3D9CheckValidCameraTextureFormat / CubeTextureFormat, and for cube render targets CheckDepthStencilMatch.  (No return value used.)
void __cdecl rw37__rwD3D9RasterConvertToNonPalettized(u8 *raster){
	u8 *img = (u8 *)((ImgCreateFn)g_m[E_IMGCREATE])(*(u32 *)(raster + 0xc), *(u32 *)(raster + 0x10), 0x20);
	if(!img) return;
	u8 *ext = raster + *GU(E_EXTOFF);
	((P1Fn)g_m[E_IMGALLOC])(img);
	((ImgFromRasterFn)g_m[E_IMGFROMRASTER])(img, *(void **)raster, 0);
	u32 d = *GU(E_9475F4), esi, t, cube;
	bool ok;
	struct L{
		static long dev(u32 usage, u32 rtype, u32 fmt){
			void *d3d = *(void **)g_m[E_D3DOBJ];
			return VT(d3d, 0x28, CheckFmtFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), *GU(E_9475F0), usage, rtype, fmt);
		}
		static bool cam(u32 cube, u32 fmt){ return (cube ? ((ChkCamFmtFn)g_m[E_CHKCAMCUBE])(fmt) : ((ChkCamFmtFn)g_m[E_CHKCAMTEX])(fmt)) != 0; }
	};
	if(ext[8] != 0){
		cube = ext[9] & 0xf;
		bool done = false;
		if((int32_t)d > 0x10){
			t = raster[0x20] & 7;
			bool tested = false;
			if(t == 0 || t == 4){ ok = L::dev(0, cube ? 5 : 3, 0x15) >= 0; tested = true; }
			else if(t == 5){
				if(cube == 0) ok = L::cam(0, 0x15);
				else{
					long hr = L::dev(1, 5, 0x15);
					if(hr >= 0){
						void *d3d = *(void **)g_m[E_D3DOBJ];
						hr = VT(d3d, 0x30, DepthMatchFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), *GU(E_9475F0), 0x15, *GU(E_ZFORMAT));
					}
					ok = hr >= 0;
				}
				tested = true;
			}
			if(tested && ok){ esi = 0x500; done = true; }
		}
		if(!done){
			cube = ext[9] & 0xf;
			t = raster[0x20] & 7;
			if(t == 0 || t == 4) ok = L::dev(0, cube ? 5 : 3, 0x1a) >= 0;
			else if(t == 5) ok = L::cam(cube, 0x1a);
			else ok = false;
			esi = ok ? 0x300 : 0x100;
		}
	}else{
		esi = 0x200;
		if((int32_t)d > 0x10){
			t = raster[0x20] & 7;
			cube = ext[9] & 0xf;
			if(t == 0 || t == 4){ ok = L::dev(0, cube ? 5 : 3, 0x16) >= 0; if(ok) esi = 0x600; }
			else if(t == 5){ ok = L::cam(cube, 0x16); if(ok) esi = 0x600; }
		}
	}
	((RasterCtorFn)g_m[E_RASTERDESTROY2])(0, raster, 0);
	u32 flags = (raster[0x20] & 7) | esi;
	flags |= 0x9000;
	((RasterCtorFn)g_m[E_RASTERCREATE2])(0, raster, flags);
	((SetFromImgFn)g_m[E_SETFROMIMAGE])(raster, img, 0);
	((P1Fn)g_m[E_IMGDESTROY])(img);
}

// rwD3D9RasterShowRaster 0x4dad50 (raster, unused, flags): ends a running scene (EndScene, slot 42); the full screen toggle word (0x947774): flag bit 0 set and word 0x80000000 -> 1,
// flag clear and word 1 -> 0x80000000 (any other combination: no mode change), then video memory is released, the device reset with the present parameters and, on success, the
// state restored.  The raster's swap chain (extension +0x1c) presents (swap chain slot 3) or else the device (slot 17).  DEVICELOST (0x88760868): TestCooperativeLevel, on
// DEVICENOTRESET release / Reset / restore again.  Dynamic vertex buffers are forced to discard (exe 0x4ec9c0); returns hr >= 0.
int32_t __cdecl rw37_rwD3D9RasterShowRaster(void **raster, void *unused, u32 flags){
	void *dev;
	if(*GU(E_INSIDESCENE) != 0){
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xa8, DevTestFn)(dev);
		*GU(E_INSIDESCENE) = 0;
	}
	u32 g = *GU(E_TOGGLE);
	bool go;
	if(flags & 1){
		if(g != 0x80000000u) go = false;
		else{ *GU(E_TOGGLE) = 1; go = true; }
	}else{
		if(g != 1) go = false;
		else{ *GU(E_TOGGLE) = 0x80000000u; go = true; }
	}
	if(go){
		((V0)g_m[E_RELVIDMEM])();
		dev = *(void **)g_m[E_DEVICE];
		int32_t hr0 = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
		if(hr0 >= 0) restore_after_reset();
	}
	void *sc = *(void **)(*(u8 **)raster + *GU(E_EXTOFF) + 0x1c);
	int32_t hr;
	if(sc) hr = ((long (__stdcall *)(void *, u32, u32, u32, u32, u32))*(void **)((u8 *)*(void **)sc + 0xc))(sc, 0, 0, 0, 0, 0);
	else{
		dev = *(void **)g_m[E_DEVICE];
		hr = ((long (__stdcall *)(void *, u32, u32, u32, u32))*(void **)((u8 *)*(void **)dev + 0x44))(dev, 0, 0, 0, 0);
	}
	if(hr == (int32_t)0x88760868){
		dev = *(void **)g_m[E_DEVICE];
		hr = VT(dev, 0xc, DevTestFn)(dev);
		if(hr == (int32_t)0x88760869){
			((V0)g_m[E_RELVIDMEM])();
			dev = *(void **)g_m[E_DEVICE];
			hr = VT(dev, 0x40, ResetFn)(dev, g_m[E_PRESENT]);
			if(hr >= 0) restore_after_reset();
		}
	}
	((V0)g_m[E_DYNVBDISCARD])();
	return hr >= 0;
}

}
