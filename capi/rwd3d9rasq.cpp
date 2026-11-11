// D3D9RASQ (group d3d9rasq, lane K window 5 batch 6): the remaining d3d9raster rows of MFL.exe (rwcore d3d9raster bucket), written op for op from the disassembly, on the exe's
// own globals and its IDirect3DDevice9 / texture / surface objects:
//   rwD3D9RasterPluginAttach 0x4df7b0, rwD3D9RasterRenderQuad 0x4e0560, rwD3D9RasterCopyQuad 0x4e0ab0, D3D9RasterClearGeneric 0x4e0dd0,
//   _rwD3D9RasterOpen 0x4dfbd0, rwD3D9RasterRender 0x4e03f0 (LAB_004e03f0)
// Exe binding: mflrw_d3d9rasq_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_d3d9rasq_check.sh.
// x87: the texture coordinates of RenderQuad are int / int divisions kept in an x87 register and stored as float (fild, fidiv, fstp dword): written as double division with a
// float store, so the result follows the caller's control word as the original does (checked at 0x7f and 0x27f).  Build with -mfpmath=387 -ffp-contract=off.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_EXTOFF, E_FMTINFO, E_REGPLUGIN, E_RCTOR, E_RDTOR,
         E_CURTARGET, E_RENDERSURF, E_DEPTHSURF, E_DEVICE, E_G9476F4,
         E_RWSETRT, E_SETRT, E_SETDS, E_BEGINSCENE, E_QUADVERTS,
         E_RWGETRS, E_GETRS, E_RWSETRASTER, E_VALPHA, E_SRCBLEND, E_DSTBLEND, E_RWSETRS, E_SETRS, E_IM2DPRIM,
         E_PIX2RGB, E_RGB2PIX, E_RASTERLOCK, E_RASTERUNLOCK, E_ERRCODE, E_ERRORSET,
         E_FLCREATE, E_VMFREELIST, E_RENDERGENERIC,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static void need(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] d3d9rasq called before mflrw_d3d9rasq_bind\n"); abort(); }
}
#define GU(i) ((u32 *)g_m[i])
#define VT(obj, off, Fn) (*(Fn *)((u8 *)*(void **)(obj) + (off)))
#define S16(p) ((int32_t)*(int16_t *)(p))
#define U32AT(p) (*(u32 *)(p))
typedef int32_t (__cdecl *RegPluginFn)(u32 size, u32 id, void *ctor, void *dtor, void *copy);
typedef int32_t (__cdecl *RwSetRTFn)(u32 index, void *raster);
typedef void (__cdecl *SetRTFn)(u32 index, u32 surf);
typedef void (__cdecl *SetDSFn)(u32 surf);
typedef int32_t (__cdecl *F0)(void);
typedef void (__cdecl *GetRSFn)(u32 state, void *out);
typedef void (__cdecl *V2)(u32 a, u32 b);
typedef void (__cdecl *V1)(u32 a);
typedef void (__cdecl *Im2DFn)(u32 type, void *verts, u32 n);
typedef void (__cdecl *PixFn)(void *out, void *in, u32 fmt);
typedef int32_t (__cdecl *LockFn)(void *out, void *raster, u32 mode);
typedef void (__cdecl *UnlockFn)(void *p, void *raster, u32 mode);
typedef u32 (__cdecl *ErrCodeFn)(u32 code, ...);
typedef void (__cdecl *ErrSetFn)(void *e);
typedef void *(__cdecl *FlCreateFn)(u32 size, u32 n, u32 align, u32 hint);
typedef u32 (__cdecl *RenderGenFn)(u32 a, u32 b, u32 c, u32 d);
typedef long (__stdcall *SetVpFn)(void *dev, const void *vp);
typedef long (__stdcall *GetLevelFn)(void *tex, u32 level, void *pp);
typedef long (__stdcall *GetCubeFn)(void *tex, u32 face, u32 level, void *pp);
typedef long (__stdcall *GetDescFn)(void *tex, u32 level, void *desc);
typedef long (__stdcall *StretchFn)(void *dev, u32 src, const void *sr, u32 dst, const void *dr, u32 filter);
typedef long (__stdcall *UpdSurfFn)(void *dev, u32 src, const void *sr, u32 dst, const void *pt);
typedef long (__stdcall *GetRTDataFn)(void *dev, u32 src, u32 dst);
typedef long (__stdcall *ClearFn)(void *dev, u32 count, const void *rects, u32 flags, u32 color, u32 z, u32 stencil);
typedef long (__stdcall *ColorFillFn)(void *dev, u32 surf, const void *rect, u32 color);
typedef u32 (__stdcall *ReleaseFn)(u32 obj);

static u8 *topParent(u8 *r){
	while(*(u8 **)r != r) r = *(u8 **)r;
	return r;
}
// GetSurfaceLevel(0) of a 2D texture or GetCubeMapSurface(face, 0) of a cube texture (ext+9: low nibble = cube flag, high nibble = face); both are vtable slot 18.
static void texSurface(u8 *ext, u32 *out){
	u8 al = ext[9];
	void *tex = *(void **)ext;
	if(al & 0xf) VT(tex, 0x48, GetCubeFn)(tex, al >> 4, 0, out);
	else VT(tex, 0x48, GetLevelFn)(tex, 0, out);
}

extern "C" {

void __cdecl mflrw_d3d9rasq_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// rwD3D9RasterPluginAttach 0x4df7b0 (): registers the raster extension (0x24 bytes, id 0x40c, ctor 0x4dfb80, dtor 0x4dfba0, no copy), stores the offset and, when it is not
// negative, clears the first 128 entries of _rwD3D9PixelFormatInfo ({u8 alpha, u8 depth, u16 rwFormat}, index = D3DFORMAT) and fills the known formats.  Returns 1 (0 on failure).
int32_t __cdecl rw37_rwD3D9RasterPluginAttach(void){
	static const struct { u8 idx, alpha, depth; u16 fmt; } tab[] = {
		{20, 0, 0x18, 0}, {21, 1, 0x20, 0x500}, {22, 0, 0x20, 0x600}, {23, 0, 0x10, 0x200}, {24, 0, 0x10, 0xa00}, {25, 1, 0x10, 0x100}, {26, 1, 0x10, 0x300},
		{27, 0, 8, 0}, {28, 1, 8, 0}, {29, 1, 0x10, 0}, {30, 0, 0x10, 0}, {31, 1, 0x20, 0}, {32, 1, 0x20, 0}, {33, 0, 0x20, 0}, {34, 0, 0x20, 0}, {35, 1, 0x20, 0},
		{36, 1, 0x40, 0}, {40, 1, 0x10, 0}, {41, 0, 8, 0x2000}, {50, 0, 8, 0x400}, {51, 1, 0x10, 0}, {52, 1, 8, 0}, {60, 0, 0x10, 0}, {61, 0, 0x10, 0},
		{62, 0, 0x20, 0}, {63, 0, 0x20, 0}, {64, 0, 0x20, 0}, {67, 1, 0x20, 0}, {70, 0, 0x10, 0x700}, {71, 0, 0x20, 0x900}, {73, 0, 0x10, 0x700}, {75, 0, 0x20, 0x900},
		{77, 0, 0x20, 0x900}, {79, 0, 0x20, 0x900}, {80, 0, 0x10, 0x700}, {81, 0, 0x10, 0}, {82, 0, 0x20, 0x900}, {83, 0, 0x20, 0x900},
		{110, 0, 0x40, 0}, {111, 0, 0x10, 0}, {112, 0, 0x20, 0}, {113, 1, 0x40, 0}, {114, 0, 0x20, 0}, {115, 0, 0x40, 0}, {116, 1, 0x80, 0}, {117, 0, 0x10, 0}};
	need();
	int32_t off = ((RegPluginFn)g_m[E_REGPLUGIN])(0x24, 0x40c, g_m[E_RCTOR], g_m[E_RDTOR], 0);
	*(int32_t *)g_m[E_EXTOFF] = off;
	if(off < 0) return 0;
	u8 *t = (u8 *)g_m[E_FMTINFO];
	memset(t, 0, 0x200);
	for(u32 i = 0; i < sizeof tab / sizeof tab[0]; i++){
		u8 *e = t + 4 * tab[i].idx;
		e[0] = tab[i].alpha; e[1] = tab[i].depth; *(u16 *)(e + 2) = tab[i].fmt;
	}
	return 1;
}

// _rwD3D9RasterOpen 0x4dfbd0 (): VideoMemoryRastersFreeList = RwFreeListCreate(8, 0x7f, 4, 0x30411); returns the list.
void *__cdecl rw37_rwD3D9RasterOpen(void){
	need();
	void *fl = ((FlCreateFn)g_m[E_FLCREATE])(8, 0x7f, 4, 0x30411);
	*(void **)g_m[E_VMFREELIST] = fl;
	return fl;
}

// rwD3D9RasterRender 0x4e03f0 (raster, rect): rwD3D9RasterRenderGeneric(raster, rect, 0, 1).
u32 __cdecl rw37_rwD3D9RasterRender(u32 raster, u32 rect){
	need();
	return ((RenderGenFn)g_m[E_RENDERGENERIC])(raster, rect, 0, 1);
}

// rwD3D9RasterRenderQuad 0x4e0560 (src raster, int rect[4] = x y w h, scaled, alpha): draws the source raster as a textured quad of 4 pre-transformed vertices
// (_RwD3D9QuadVerts, 28 bytes each: x y z rhw colour u v) into the current raster target.  A non camera target is set as render target through RwD3D9SetRenderTarget; when
// that fails (or the target is a camera) the back buffer surface is the target and, for a non camera target, the result is copied afterwards with StretchRect into surface
// level 0 of the SOURCE raster's texture (the original uses the source extension here) with the two rectangles swapped: kept as it is.
void __cdecl rw37_rwD3D9RasterRenderQuad(u8 *src, const int32_t *rect, u32 scaled, u32 alpha){
	need();
	int32_t rtset = 0;
	u32 l6c = (u32)(uintptr_t)src, l70 = (u32)(uintptr_t)rect, l74 = scaled;      // the argument slots, reused by the original as locals
	u32 sB = 0, sC = 0, sE = 0, s9 = 0, s1 = 0, s3 = 0, s4 = 0;
	u8 *top = topParent(*(u8 **)src);
	u8 *ext = top + *GU(E_EXTOFF);
	u8 *tgt = *(u8 **)g_m[E_CURTARGET];
	if(tgt[0x20] == 2 || (rtset = ((RwSetRTFn)g_m[E_RWSETRT])(0, tgt)) == 0)
		((SetRTFn)g_m[E_SETRT])(0, *GU(E_RENDERSURF));
	((SetDSFn)g_m[E_SETDS])(0);
	tgt = *(u8 **)g_m[E_CURTARGET];
	u32 vp[6];
	vp[0] = (u32)S16(tgt + 0x1c); vp[1] = (u32)S16(tgt + 0x1e); vp[2] = U32AT(tgt + 0xc); vp[3] = U32AT(tgt + 0x10); vp[4] = 0; vp[5] = 0x3f800000;
	void *dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0xbc, SetVpFn)(dev, vp);
	if(((F0)g_m[E_BEGINSCENE])()){
		float *q = (float *)g_m[E_QUADVERTS];
		u32 *qu = (u32 *)q;
		q[0] = (float)rect[0];
		q[1] = (float)rect[1];
		q[7] = (float)rect[0];
		if(scaled){
			q[8] = (float)(int32_t)((u32)rect[3] + (u32)rect[1]);
			q[14] = (float)(int32_t)((u32)rect[2] + (u32)rect[0]);
			q[15] = (float)rect[1];
			q[21] = (float)(int32_t)((u32)rect[2] + (u32)rect[0]);
			q[22] = (float)(int32_t)((u32)rect[3] + (u32)rect[1]);
		}else{
			q[8] = (float)(int32_t)(U32AT(src + 0x10) + (u32)rect[1]);
			q[14] = (float)(int32_t)((u32)rect[0] + U32AT(src + 0xc));
			q[15] = (float)rect[1];
			q[21] = (float)(int32_t)((u32)rect[0] + U32AT(src + 0xc));
			q[22] = (float)(int32_t)(U32AT(src + 0x10) + (u32)rect[1]);
		}
		int32_t tw = *(int32_t *)(top + 0xc), th = *(int32_t *)(top + 0x10);
		qu[2] = 0; qu[3] = 0x3f800000; qu[4] = 0xffffffff;
		q[5] = (float)((double)S16(src + 0x1c) / (double)tw);
		q[6] = (float)((double)S16(src + 0x1e) / (double)th);
		qu[9] = 0; qu[10] = 0x3f800000; qu[11] = 0xffffffff;
		q[12] = (float)((double)S16(src + 0x1c) / (double)tw);
		q[13] = (float)((double)(int32_t)((u32)S16(src + 0x1e) + U32AT(src + 0x10)) / (double)th);
		qu[16] = 0; qu[17] = 0x3f800000; qu[18] = 0xffffffff;
		q[19] = (float)((double)(int32_t)((u32)S16(src + 0x1c) + U32AT(src + 0xc)) / (double)tw);
		q[20] = (float)((double)S16(src + 0x1e) / (double)th);
		qu[23] = 0; qu[24] = 0x3f800000; qu[25] = 0xffffffff;
		q[26] = (float)((double)(int32_t)((u32)S16(src + 0x1c) + U32AT(src + 0xc)) / (double)tw);
		l70 = (u32)S16(src + 0x1e) + U32AT(src + 0x10);
		q[27] = (float)((double)(int32_t)l70 / (double)th);
		GetRSFn rwget = (GetRSFn)g_m[E_RWGETRS], get = (GetRSFn)g_m[E_GETRS];
		V2 rwset = (V2)g_m[E_RWSETRS], set = (V2)g_m[E_SETRS], setraster = (V2)g_m[E_RWSETRASTER];
		rwget(0xa, &l74); rwget(0xb, &sB); rwget(0xc, &sC); rwget(0xe, &sE); rwget(9, &s9); rwget(3, &s3); rwget(4, &s4); rwget(1, &s1);
		get(7, &l70);
		get(0x16, &l6c);
		setraster((u32)(uintptr_t)top, 0);
		if(alpha && ext[8]){
			((V1)g_m[E_VALPHA])(1);
			((V1)g_m[E_SRCBLEND])(5);
			((V1)g_m[E_DSTBLEND])(6);
		}else ((V1)g_m[E_VALPHA])(0);
		rwset(0xe, 0);
		rwset(9, (scaled != 0) + 1);
		rwset(2, 3);
		set(7, 0);
		set(0x16, 1);
		((Im2DFn)g_m[E_IM2DPRIM])(4, q, 4);
		set(7, l70);
		set(0x16, l6c);
		rwset(0xa, l74); rwset(0xb, sB); rwset(0xc, sC); rwset(0xe, sE); rwset(9, s9);
		setraster(s1, 0);
		rwset(3, s3); rwset(4, s4);
	}
	tgt = *(u8 **)g_m[E_CURTARGET];
	if(tgt[0x20] == 2) return;
	if(rtset){
		((SetRTFn)g_m[E_SETRT])(0, *GU(E_RENDERSURF));
		((SetDSFn)g_m[E_SETDS])(*GU(E_DEPTHSURF));
		return;
	}
	texSurface(ext, &l70);
	tgt = *(u8 **)g_m[E_CURTARGET];
	int32_t tr[4], sr[4];
	tr[0] = S16(tgt + 0x1c); tr[1] = S16(tgt + 0x1e); tr[2] = (int32_t)(U32AT(tgt + 0xc) + (u32)tr[0]); tr[3] = (int32_t)(U32AT(tgt + 0x10) + (u32)tr[1]);
	sr[0] = S16(src + 0x1c); sr[1] = S16(src + 0x1e); sr[2] = (int32_t)(U32AT(src + 0xc) + (u32)sr[0]); sr[3] = (int32_t)(U32AT(src + 0x10) + (u32)sr[1]);
	u32 caps = *GU(E_G9476F4), filter = 0;
	if(scaled){ if(caps & 0x2000200) filter = 2; }
	else if(caps & 0x1000100) filter = 1;
	dev = *(void **)g_m[E_DEVICE];
	VT(dev, 0x88, StretchFn)(dev, *GU(E_RENDERSURF), tr, l70, sr, filter);
	VT((void *)(uintptr_t)l70, 8, ReleaseFn)(l70);
}

// rwD3D9RasterCopyQuad 0x4e0ab0 (src raster, int rect[4], scaled): copies the source raster's surface to the current raster target's surface.  A camera raster (type 2) uses
// its own surface when it has a swap chain, else the back buffer; every other type takes level 0 of its texture (released at the end).  Source not a render target (type
// 2 / 5): UpdateSurface with the source position as the destination point (as the original does); source a render target and destination not: GetRenderTargetData; both render
// targets: StretchRect with the filter of the device caps word.  Returns hr >= 0.
int32_t __cdecl rw37_rwD3D9RasterCopyQuad(u8 *src, const int32_t *rect, u32 scaled){
	need();
	u8 *srcTop = topParent(src);
	u32 extoff = *GU(E_EXTOFF);
	u32 srcSurf = 0, dstSurf = (u32)(uintptr_t)src;
	if(srcTop[0x20] != 2){
		texSurface(srcTop + extoff, &srcSurf);
		extoff = *GU(E_EXTOFF);
	}else srcSurf = U32AT(srcTop + extoff + 0x1c) ? U32AT(srcTop + extoff) : *GU(E_RENDERSURF);
	u8 *tgt = *(u8 **)g_m[E_CURTARGET];
	u8 *dstTop = topParent(tgt);
	if(dstTop[0x20] != 2){
		texSurface(dstTop + extoff, &dstSurf);
		tgt = *(u8 **)g_m[E_CURTARGET];
	}else dstSurf = U32AT(dstTop + extoff + 0x1c) ? U32AT(dstTop + extoff) : *GU(E_RENDERSURF);
	int32_t sl = S16(src + 0x1c), st = S16(src + 0x1e);
	u32 sw = U32AT(src + 0xc), sh = U32AT(src + 0x10);
	int32_t sr[4], dr[4], pt[2];
	sr[0] = sl; sr[1] = st; sr[2] = (int32_t)((u32)sl + sw); sr[3] = (int32_t)(sh + (u32)st);
	dr[0] = (int32_t)((u32)S16(tgt + 0x1c) + (u32)rect[0]);
	dr[1] = (int32_t)((u32)S16(tgt + 0x1e) + (u32)rect[1]);
	if(scaled){
		dr[2] = (int32_t)((u32)rect[2] + (u32)dr[0]);
		dr[3] = (int32_t)((u32)rect[3] + (u32)dr[1]);
	}else{
		dr[2] = (int32_t)(sw + (u32)dr[0]);
		dr[3] = (int32_t)(sh + (u32)dr[1]);
	}
	u8 sc = srcTop[0x20], dc = dstTop[0x20];
	void *dev = *(void **)g_m[E_DEVICE];
	long hr;
	if(sc != 2 && sc != 5){
		pt[0] = sl; pt[1] = st;
		hr = VT(dev, 0x78, UpdSurfFn)(dev, srcSurf, sr, dstSurf, pt);
	}else if(dc != 2 && dc != 5)
		hr = VT(dev, 0x80, GetRTDataFn)(dev, srcSurf, dstSurf);
	else{
		u32 caps = *GU(E_G9476F4), filter = 0;
		if(scaled){ if(caps & 0x2000200) filter = 2; }
		else if(caps & 0x1000100) filter = 1;
		hr = VT(dev, 0x88, StretchFn)(dev, srcSurf, sr, dstSurf, dr, filter);
	}
	if(dstTop[0x20] != 2) VT((void *)(uintptr_t)dstSurf, 8, ReleaseFn)(dstSurf);
	if(srcTop[0x20] != 2) VT((void *)(uintptr_t)srcSurf, 8, ReleaseFn)(srcSurf);
	return hr >= 0;
}

static int32_t clearError(void){
	u32 e[2];
	e[0] = 1;
	e[1] = ((ErrCodeFn)g_m[E_ERRCODE])(0x8000000d);
	((ErrSetFn)g_m[E_ERRORSET])(e);
	return 0;
}
// the rectangle of the clear in device coordinates: raster offset (int16 at +0x1c / +0x1e) + rect x y, then + w h
static void clearRect(int32_t *r, const int32_t *rect){
	u8 *tgt = *(u8 **)g_m[E_CURTARGET];
	r[0] = (int32_t)((u32)S16(tgt + 0x1c) + (u32)rect[0]);
	r[1] = (int32_t)((u32)S16(tgt + 0x1e) + (u32)rect[1]);
	r[2] = (int32_t)((u32)rect[2] + (u32)r[0]);
	r[3] = (int32_t)((u32)rect[3] + (u32)r[1]);
}

// D3D9RasterClearGeneric 0x4e0dd0 (int rect[4] = x y w h, pixel value): clears a rectangle of the current raster target, by its type (+0x20):
//  2 camera: colour (converted through RGB to 8888 unless the depth is 32) and device Clear(TARGET) on the back buffer / depth surfaces;
//  0 / 4 normal / texture: GetLevelDesc of the PARENT's texture (one level up only, as the original); pool DEFAULT: ColorFill on surface level 0, else lock the raster and
//    fill 32 / 16 bit pixels or, for depth 8, the palette index nearest to the colour bytes (first minimum of the squared distance, stop at 0); other depths: error;
//  5 camera texture: ColorFill on the top parent's surface;  1 z buffer: Clear(ZBUFFER) with that depth surface set;  else error 0x8000000d.  Returns 1, 0 on error.
int32_t __cdecl rw37_D3D9RasterClearGeneric(const int32_t *rect, u32 pixel){
	need();
	u8 *tgt = *(u8 **)g_m[E_CURTARGET];
	void *dev;
	int32_t r[4];
	u32 rgba = 0, col = 0, surf = 0;
	switch(tgt[0x20]){
	case 2:
		if(U32AT(tgt + 0x14) != 0x20){
			((PixFn)g_m[E_PIX2RGB])(&rgba, &pixel, (u32)tgt[0x23] << 8);
			((PixFn)g_m[E_RGB2PIX])(&col, &rgba, 0x500);
		}else col = pixel;
		((SetRTFn)g_m[E_SETRT])(0, *GU(E_RENDERSURF));
		((SetDSFn)g_m[E_SETDS])(*GU(E_DEPTHSURF));
		clearRect(r, rect);
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xac, ClearFn)(dev, 1, r, 1, col, 0x3f800000, 0);
		return 1;
	case 0: case 4: {
		u8 *ext = *(u8 **)tgt + *GU(E_EXTOFF);
		u32 desc[8];
		void *tex = *(void **)ext;
		VT(tex, 0x44, GetDescFn)(tex, 0, desc);
		if(desc[3] == 0){
			tgt = *(u8 **)g_m[E_CURTARGET];
			((PixFn)g_m[E_PIX2RGB])(&rgba, &pixel, (u32)tgt[0x23] << 8);
			((PixFn)g_m[E_RGB2PIX])(&col, &rgba, 0x500);
			texSurface(ext, &surf);
			clearRect(r, rect);
			dev = *(void **)g_m[E_DEVICE];
			VT(dev, 0x8c, ColorFillFn)(dev, surf, r, col);
			VT((void *)(uintptr_t)surf, 8, ReleaseFn)(surf);
			return 1;
		}
		u8 *pixels = (u8 *)(uintptr_t)rgba;      // the slot of the lock result: only written by a successful lock
		int32_t ok = 0;
		if(!((LockFn)g_m[E_RASTERLOCK])(&pixels, *(void **)g_m[E_CURTARGET], 1)) return clearError();
		tgt = *(u8 **)g_m[E_CURTARGET];
		u32 depth = U32AT(tgt + 0x14);
		if(depth == 0x20){
			for(int32_t y = 0; y < rect[3]; y++){
				tgt = *(u8 **)g_m[E_CURTARGET];
				u32 *p = (u32 *)(pixels + ((u32)rect[1] + (u32)y) * U32AT(tgt + 0x18) + (u32)rect[0] * 4);
				for(int32_t x = 0; x < rect[2]; x++) *p++ = pixel;
			}
			ok = 1;
		}else if(depth == 0x10){
			for(int32_t y = 0; y < rect[3]; y++){
				tgt = *(u8 **)g_m[E_CURTARGET];
				u16 *p = (u16 *)(pixels + ((u32)rect[1] + (u32)y) * U32AT(tgt + 0x18) + (u32)rect[0] * 2);
				for(int32_t x = 0; x < rect[2]; x++) *p++ = (u16)pixel;
			}
			ok = 1;
		}else if(depth == 8){
			u8 *pal = *(u8 **)(topParent(*(u8 **)tgt) + *GU(E_EXTOFF) + 4);
			u32 b0 = pixel & 0xff, b1 = (pixel >> 8) & 0xff, b2 = (pixel >> 16) & 0xff, b3 = pixel >> 24, best = 0xffffffff;
			u8 idx = 0;
			for(u32 i = 0; i < 0x100; i++, pal += 4){
				u32 c0 = pal[0] - b0, c1 = pal[1] - b1, c2 = pal[2] - b2, c3 = pal[3] - b3;
				u32 d = c3 * c3 + c2 * c2 + c1 * c1 + c0 * c0;
				if(d < best){
					best = d; idx = (u8)i;
					if(d == 0) break;
				}
			}
			for(int32_t y = 0; y < rect[3]; y++){
				tgt = *(u8 **)g_m[E_CURTARGET];
				u8 *p = pixels + ((u32)rect[1] + (u32)y) * U32AT(tgt + 0x18) + (u32)rect[0];
				for(int32_t x = 0; x < rect[2]; x++) *p++ = idx;
			}
			ok = 1;
		}
		((UnlockFn)g_m[E_RASTERUNLOCK])(pixels, *(void **)g_m[E_CURTARGET], 1);
		if(ok) return 1;
		return clearError(); }
	case 5:
		if(U32AT(tgt + 0x14) != 0x20){
			((PixFn)g_m[E_PIX2RGB])(&rgba, &pixel, (u32)tgt[0x23] << 8);
			((PixFn)g_m[E_RGB2PIX])(&col, &rgba, 0x500);
			tgt = *(u8 **)g_m[E_CURTARGET];
		}else col = pixel;
		texSurface(topParent(*(u8 **)tgt) + *GU(E_EXTOFF), &surf);
		clearRect(r, rect);
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0x8c, ColorFillFn)(dev, surf, r, col);
		VT((void *)(uintptr_t)surf, 8, ReleaseFn)(surf);
		return 1;
	case 1:
		((SetDSFn)g_m[E_SETDS])(U32AT(topParent(*(u8 **)tgt) + *GU(E_EXTOFF)));
		clearRect(r, rect);
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xac, ClearFn)(dev, 1, r, 2, 0, 0x3f800000, 0);
		((SetDSFn)g_m[E_SETDS])(*GU(E_DEPTHSURF));
		return 1;
	default:
		return clearError();
	}
}

}
