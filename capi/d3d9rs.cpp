// D3D9RS (LIB_RW group d3drs, D1 of plan_engine_switch_2026-11-07_v2): the 22 still-original functions of the RW 3.7 D3D9 driver's
// render-state module (d3d9rendst.obj, 0x4e3b30..0x4e63c0 of MFL.exe), written op for op from the disassembly ("kernel on exe state":
// the caches, dirty lists and RW-side state stay in the exe's bss block 0x7d2810..0x7d4d1c at their original addresses).
//   rwD3D9RenderStateFlushCache 0x4e3b30, RwD3D9SetTextureStageState 0x4e3c70, RwD3D9SetSurfaceProperties 0x4e3d10, rwD3D9RenderStateOpen 0x4e4320,
//   rwD3D9RenderStateReset 0x4e4960, rwD3D9RWGetRenderState 0x4e5070, rwD3D9RenderStateTextureFilter 0x4e52f0, rwD3D9RWSetRasterStage 0x4e5440,
//   RwD3D9SetTexture 0x4e55d0, rwD3D9RenderStateVertexAlphaEnable 0x4e5860, the 8 stencil setters 0x4e5960..0x4e5b90, rwD3D9RWSetRenderState 0x4e5be0,
//   rwD3D9RenderStateZTestEnable 0x4e6220, ShadeMode 0x4e6320, CullMode 0x4e6370.
// Not here (C in the exe, called through the bind): RwD3D9SetRenderState 0x4e3c00, RwD3D9SetSamplerState 0x4e3cd0, SrcBlend 0x4e53a0, DestBlend 0x4e53f0.
// The device is the exe's (*0x7d2240), called through its COM vtable: SetMaterial 49, SetRenderState 57, SetTexture 65, SetTextureStageState 67,
// SetSamplerState 69, SetCurrentTexturePalette 73.  No allocation, no CRT state.
// Exe-layout adapter: mflrw_d3d_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c).  Swap list: ghidra tools/delink/rw_d3drs.txt.
// Check: ghidra tools/delink/rw_d3drs_check.sh (fake device tools/delink/fakedev.c; both sides run the same op sequences natively under Wine).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef long double X;

static u8 *g_rs;		// d3d9rendst .bss 0x7d2810..0x7d4d1c
static void **g_dev;		// &_RwD3DDevice 0x7d2240
static const u8 *g_caps;	// _RwD3D9DeviceCaps 0x947600 (D3DCAPS9)
static const i32 *g_rasExt;	// raster plugin offset of the D3D9 raster extension 0x947144
static const float *g_amb;	// ambient colour r, g, b 0x62e738
static const float *g_k255;	// 1 / 255 0x5dff00
static const u8 *g_tab;		// d3d9rendst .rdata tables 0x5ea148..0x5ea270
typedef void (__cdecl *SetRSFn)(u32 state, u32 value);
typedef void (__cdecl *SetSSFn)(u32 stage, u32 type, u32 value);
typedef i32 (__cdecl *BlendFn)(i32 v);
static SetRSFn g_setRS;		// RwD3D9SetRenderState 0x4e3c00
static SetSSFn g_setSS;		// RwD3D9SetSamplerState 0x4e3cd0
static BlendFn g_srcBlend, g_destBlend;	// rwD3D9RenderStateSrcBlend 0x4e53a0 / DestBlend 0x4e53f0

typedef u32 __attribute__((may_alias)) UA;	// the block is read as words and floats
typedef float __attribute__((may_alias)) FA;
#define A(va) (*(UA *)(g_rs + ((va) - 0x7d2810u)))
#define AF(va) (*(FA *)(g_rs + ((va) - 0x7d2810u)))
#define TB(va, i) (((const u32 *)(g_tab + ((va) - 0x5ea148u)))[i])
#define CAPS(off) (*(const u32 *)(g_caps + (off)))
enum { RASTERCAPS = 0x24, TEXTUREFILTERCAPS = 0x40 };
// RW-side per-stage block 0x7d4c20 (stride 0x18): raster, addressU, addressV, filter, border colour, max anisotropy
#define ST(stage, f) A(0x7d4c20 + 0x18 * (stage) + 4 * (f))
enum { S_RASTER, S_ADDRU, S_ADDRV, S_FILTER, S_BORDER, S_ANISO };
// sampler state cache 0x7d3050 (8 stages x 14)
#define SAMP(stage, t) A(0x7d3050 + 0x38 * (stage) + 4 * (t))

static inline u8 *DEV(void){ return (u8 *)*g_dev; }
static inline void **VT(u8 *d){ return *(void ***)d; }
static inline i32 devSetMaterial(const void *m){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, const void *))VT(d)[49])(d, m); }
static inline void devSetRS(u32 s, u32 v){ u8 *d = DEV(); ((i32 (__stdcall *)(void *, u32, u32))VT(d)[57])(d, s, v); }
static inline void devSetTexture(u32 st, u32 t){ u8 *d = DEV(); ((i32 (__stdcall *)(void *, u32, u32))VT(d)[65])(d, st, t); }
static inline void devSetTSS(u32 st, u32 ty, u32 v){ u8 *d = DEV(); ((i32 (__stdcall *)(void *, u32, u32, u32))VT(d)[67])(d, st, ty, v); }
static inline void devSetSS(u32 st, u32 ty, u32 v){ u8 *d = DEV(); ((i32 (__stdcall *)(void *, u32, u32, u32))VT(d)[69])(d, st, ty, v); }
static inline void devSetPalette(u32 p){ u8 *d = DEV(); ((i32 (__stdcall *)(void *, u32))VT(d)[73])(d, p); }

// a value through the x87 stack (fld / fstp: an sNaN comes out quiet, as in the original)
static inline u32 x87w(u32 w){ u32 r; __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(w)); return r; }

// the render-state cache 0x7d38e8 {value, dirty} and the dirty list 0x7d3258 / count 0x7d4d04, as the original inlines them
static inline void dirty(u32 s){ u32 *e = &A(0x7d38e8 + 8 * s); if (e[1] == 0) { e[1] = 1; u32 n = A(0x7d4d04); A(0x7d3258 + 4 * n) = s; A(0x7d4d04) = n + 1; } }
static inline void rsSet(u32 s, u32 v){ if (A(0x7d38e8 + 8 * s) != v) { A(0x7d38e8 + 8 * s) = v; dirty(s); } }	// compare first
static inline void rsPut(u32 s, u32 v){ A(0x7d38e8 + 8 * s) = v; dirty(s); }					// the caller compared the RW value

extern "C" {

void __cdecl mflrw_d3d_bind(void **deviceVar, void *rsBss, const void *caps, const void *rasExtOffset, const void *ambient, const void *k255, const void *tables,
                            void *setRenderState, void *setSamplerState, void *srcBlend, void *destBlend){
	g_dev = deviceVar; g_rs = (u8 *)rsBss; g_caps = (const u8 *)caps; g_rasExt = (const i32 *)rasExtOffset; g_amb = (const float *)ambient;
	g_k255 = (const float *)k255; g_tab = (const u8 *)tables; g_setRS = (SetRSFn)setRenderState; g_setSS = (SetSSFn)setSamplerState;
	g_srcBlend = (BlendFn)srcBlend; g_destBlend = (BlendFn)destBlend;
}

// rwD3D9RenderStateFlushCache 0x4e3b30: the render-state dirty list, then the texture-stage dirty list 0x7d4398 {stage, type} / count 0x7d4d08,
// each against its device-side cache (0x7d35a0, 0x7d3f78).  eax at the return: the stage count if there was one, else the render-state count.
u32 __cdecl rw37__rwD3D9RenderStateFlushCache(void){
	u32 r = A(0x7d4d04), i;
	if (r != 0) {
		for (i = 0; ; ) {
			u32 s = A(0x7d3258 + 4 * i), v = A(0x7d38e8 + 8 * s);
			A(0x7d38ec + 8 * s) = 0;
			if (A(0x7d35a0 + 4 * s) != v) { A(0x7d35a0 + 4 * s) = v; devSetRS(s, v); }
			r = A(0x7d4d04); i++;
			if (i >= r) break;
		}
		A(0x7d4d04) = 0;
	}
	if (A(0x7d4d08) != 0) {
		for (i = 0; ; ) {
			u32 st = A(0x7d4398 + 8 * i), ty = A(0x7d439c + 8 * i), k = st * 33 + ty, v = A(0x7d2810 + 8 * k);
			A(0x7d2814 + 8 * k) = 0;
			if (A(0x7d3f78 + 4 * k) != v) { A(0x7d3f78 + 4 * k) = v; devSetTSS(st, ty, v); }
			r = A(0x7d4d08); i++;
			if (i >= r) break;
		}
		A(0x7d4d08) = 0;
	}
	return r;
}

// RwD3D9SetTextureStageState 0x4e3c70: cache 0x7d2810 (8 stages x 33 {value, dirty}); eax at the return as in the original
u32 __cdecl rw37_RwD3D9SetTextureStageState(u32 stage, u32 type, u32 value){
	u32 k = (stage * 33 + type) * 8;
	if (*(UA *)(g_rs + k) == value) return k;
	*(UA *)(g_rs + k) = value;
	if (*(UA *)(g_rs + k + 4) != 0) return k;
	*(UA *)(g_rs + k + 4) = 1;
	u32 n = A(0x7d4d08); A(0x7d4398 + 8 * n) = stage; A(0x7d439c + 8 * n) = type; A(0x7d4d08) = n + 1;
	return n + 1;
}

// RwD3D9SetSurfaceProperties 0x4e3d10: D3DMATERIAL9 0x7d3210 (diffuse, ambient, specular, emissive, power) from the surface properties
// {ambient, specular, diffuse}, the material colour and the flags (0x40 modulate material colour, 0x08 prelit); the last inputs are kept in
// 0x7d4d14 / 0x7d4d18 / 0x7d4d10 / 0x7d4d0c / 0x7d4cf0..f8 and an unchanged call returns 1 at once.
i32 __cdecl rw37_RwD3D9SetSurfaceProperties(const u32 *sp, const u8 *color, u32 flags){
	u32 f = flags & 0x48;
	const u32 *amb = (const u32 *)g_amb;
	if (A(0x7d4d14) == sp[2] && A(0x7d4d18) == sp[0] && A(0x7d4d10) == *(const u32 *)color && A(0x7d4d0c) == f &&
	    A(0x7d4cf0) == amb[0] && A(0x7d4cf4) == amb[1] && A(0x7d4cf8) == amb[2]) return 1;
	A(0x7d4d14) = sp[2]; A(0x7d4d18) = sp[0]; A(0x7d4d10) = *(const u32 *)color;
	A(0x7d4cf0) = amb[0]; A(0x7d4cf4) = amb[1];
	A(0x7d4d0c) = f; A(0x7d4cf8) = amb[2];
	const float *spf = (const float *)sp;
	if ((f & 0x40) && *(const u32 *)color != 0xffffffffu) {
		X d = (X)spf[2] * (X)*g_k255;
		AF(0x7d3210) = (float)((X)(i32)color[0] * d);
		AF(0x7d3214) = (float)((X)(i32)color[1] * d);
		AF(0x7d3218) = (float)((X)(i32)color[2] * d);
		AF(0x7d321c) = (float)((X)(i32)color[3] * (X)*g_k255);
		X a = (X)spf[0] * (X)*g_k255;
		if (f & 8) {
			u32 argb = (u32)color[3] << 24 | (u32)color[0] << 16 | (u32)color[1] << 8 | color[2];
			rsSet(0x8b, argb); rsSet(0x8d, 1); rsSet(0x93, 1); rsSet(0x94, 0);
			A(0x7d3220) = 0; A(0x7d3224) = 0; A(0x7d3228) = 0; A(0x7d322c) = 0x3f800000;
			AF(0x7d3240) = (float)((X)(i32)color[0] * (X)g_amb[0] * a);
			AF(0x7d3244) = (float)((X)(i32)color[1] * (X)g_amb[1] * a);
			A(0x7d324c) = 0x3f800000;
			AF(0x7d3248) = (float)((X)(i32)color[2] * (X)g_amb[2] * a);
		} else {
			rsSet(0x8b, 0xffffffffu); rsSet(0x8d, 0); rsSet(0x93, 0); rsSet(0x94, 0);
			AF(0x7d3220) = (float)((X)(i32)color[0] * (X)g_amb[0] * a);
			AF(0x7d3224) = (float)((X)(i32)color[1] * (X)g_amb[1] * a);
			A(0x7d3240) = 0; A(0x7d3244) = 0; A(0x7d3248) = 0; A(0x7d324c) = 0x3f800000;
			AF(0x7d3228) = (float)((X)(i32)color[2] * (X)g_amb[2] * a);
			A(0x7d322c) = 0x3f800000;
		}
	} else {
		u32 dw = x87w(sp[2]);
		A(0x7d3210) = dw; A(0x7d3214) = dw; A(0x7d3218) = dw;
		A(0x7d321c) = 0x3f800000;
		rsSet(0x8b, 0xffffffffu); rsSet(0x93, 0);
		if (f & 8) { rsSet(0x8d, 1); rsSet(0x94, 1); }
		else { rsSet(0x8d, 0); rsSet(0x94, 0); }
		A(0x7d3240) = 0; A(0x7d3244) = 0; A(0x7d3248) = 0; A(0x7d324c) = 0x3f800000;
		if (sp[0] != 0x3f800000u) {
			AF(0x7d3220) = (float)((X)g_amb[0] * (X)spf[0]);
			AF(0x7d3224) = (float)((X)g_amb[1] * (X)spf[0]);
			AF(0x7d3228) = (float)((X)g_amb[2] * (X)spf[0]);
		} else {
			A(0x7d3220) = amb[0]; A(0x7d3224) = amb[1]; A(0x7d3228) = amb[2];
		}
		A(0x7d322c) = 0x3f800000;
	}
	A(0x7d3230) = 0; A(0x7d3234) = 0; A(0x7d3238) = 0; A(0x7d323c) = 0x3f800000; A(0x7d3250) = 0;
	return devSetMaterial(&A(0x7d3210)) >= 0;
}

// the cache reset shared by Open and Reset: device-side cache -1 (except 0x22 / 0x3a / 0x3b / 0x8b = 0), RW cache = device cache with no
// dirty marks, texture-stage caches -1 / clean, sampler cache -1, material and the surface-property inputs 0
static void clearCaches(void){
	u32 i;
	for (i = 0; i < 0xd2; i++) A(0x7d35a0 + 4 * i) = 0xffffffffu;
	A(0x7d3628) = 0; A(0x7d37cc) = 0; A(0x7d3688) = 0; A(0x7d368c) = 0;
	for (i = 0; i < 0xd2; i++) { A(0x7d38ec + 8 * i) = 0; A(0x7d38e8 + 8 * i) = A(0x7d35a0 + 4 * i); }
	for (i = 0; i < 0x108; i++) A(0x7d3f78 + 4 * i) = 0xffffffffu;
	A(0x7d4d04) = 0;
	for (i = 0; i < 8 * 33; i++) { A(0x7d2810 + 8 * i) = 0xffffffffu; A(0x7d2814 + 8 * i) = 0; }
	A(0x7d4d08) = 0;
	for (i = 0; i < 0x70; i++) A(0x7d3050 + 4 * i) = 0xffffffffu;
	for (i = 0; i < 0x11; i++) A(0x7d3210 + 4 * i) = 0;
	A(0x7d4d14) = 0; A(0x7d4d18) = 0; A(0x7d4cf0) = 0; A(0x7d4cf4) = 0; A(0x7d4cf8) = 0;
	A(0x7d4d0c) = 0; A(0x7d4d10) = 0;
}
static inline bool tableFog(void){ u32 c = CAPS(RASTERCAPS); return (c & 0x100) && (c & 0x100000); }

// rwD3D9RenderStateOpen 0x4e4320: the RW defaults, then a flush (tail call; its eax is the return)
u32 __cdecl rw37__rwD3D9RenderStateOpen(void){
	u32 st;
	clearCaches();
	A(0x7d4c0c) = 0; A(0x7d4c10) = 1; A(0x7d4c14) = 0x3f800000; A(0x7d4c18) = 0;
	rsSet(0x1c, 0);
	if (tableFog()) { rsSet(0x23, 3); rsSet(0x8c, 0); }
	else { rsSet(0x23, 0); rsSet(0x8c, 3); }
	rsSet(0x22, 0);
	rsSet(9, 2); A(0x7d4c1c) = 2;
	rsSet(0x17, 4); rsSet(0xe, 1); rsSet(7, 1);
	A(0x7d4bd8) = 1; A(0x7d4bdc) = 1;
	rsSet(0x34, 0); rsSet(0x35, 1); rsSet(0x36, 1); rsSet(0x37, 1); rsSet(0x38, 8); rsSet(0x39, 0);
	g_setRS(0x3a, 0xffffffffu); g_setRS(0x3b, 0xffffffffu);
	A(0x7d4bf0) = 8; A(0x7d4bf8) = 0xffffffffu; A(0x7d4bfc) = 0xffffffffu; A(0x7d4be0) = 0;
	A(0x7d4be4) = 1; A(0x7d4be8) = 1; A(0x7d4bec) = 1; A(0x7d4bf4) = 0;
	for (st = 0; st < 8; st++) {
		devSetTexture(st, 0);
		ST(st, S_RASTER) = 0;
		if (CAPS(TEXTUREFILTERCAPS) & 0x200) {
			if (SAMP(st, 6) != 2) { SAMP(st, 6) = 2; devSetSS(st, 6, 2); }
			g_setSS(st, 5, 2);
			ST(st, S_FILTER) = 2;
		} else {
			g_setSS(st, 6, 1); g_setSS(st, 5, 1);
			ST(st, S_FILTER) = 1;
		}
		g_setSS(st, 7, 0); g_setSS(st, 1, 1); g_setSS(st, 2, 1);
		ST(st, S_ADDRU) = 1; ST(st, S_ADDRV) = 1;
		g_setSS(st, 4, 0xff000000u);
		ST(st, S_BORDER) = 0xff000000u; ST(st, S_ANISO) = 1;
		g_setSS(st, 10, 1);
		if (st != 0) { rw37_RwD3D9SetTextureStageState(st, 1, 1); rw37_RwD3D9SetTextureStageState(st, 4, 1); }
	}
	g_setRS(0x13, 5); g_setRS(0x14, 6);
	A(0x7d4ce0) = 5; A(0x7d4ce4) = 6;
	g_setRS(0x19, 5);
	A(0x7d4ce8) = 5; A(0x7d4cec) = 1;
	g_setRS(0x18, 0); g_setRS(0x1b, 0); g_setRS(0xf, 0);
	A(0x7d4c00) = 0; A(0x7d4c04) = 0;
	g_setRS(0x89, 0); g_setRS(0x16, 2);
	A(0x7d4c08) = 2;
	rw37_RwD3D9SetTextureStageState(0, 1, 3); rw37_RwD3D9SetTextureStageState(0, 3, 0);
	rw37_RwD3D9SetTextureStageState(0, 4, 3); rw37_RwD3D9SetTextureStageState(0, 6, 0);
	g_setRS(0x91, 0); g_setRS(0x92, 0); g_setRS(0x93, 0);
	g_setRS(0x1a, CAPS(RASTERCAPS) & 1);
	g_setRS(0x1d, 0); g_setRS(0x8e, 0); g_setRS(0x8b, 0xffffffffu); g_setRS(0x8f, 0);
	return rw37__rwD3D9RenderStateFlushCache();
}

// rwD3D9RenderStateReset 0x4e4960 (after a device reset): the caches cleared as in Open, then the RW-side state written back
u32 __cdecl rw37__rwD3D9RenderStateReset(void){
	u32 fogStart = A(0x7d3a08), fogEnd = A(0x7d3a10), alphaRef = A(0x7d39a8), st;
	clearCaches();
	rsSet(0x26, A(0x7d4c14));
	rsSet(0x1c, A(0x7d4c0c));
	if (tableFog()) { rsSet(0x23, TB(0x5ea154, A(0x7d4c10))); rsSet(0x8c, 0); }
	else { rsSet(0x23, 0); rsSet(0x8c, TB(0x5ea154, A(0x7d4c10))); }
	rsSet(0x24, fogStart); rsSet(0x25, fogEnd);
	rsSet(0x22, A(0x7d4c18));
	rsSet(9, TB(0x5ea148, A(0x7d4c1c)));
	rsSet(0xe, A(0x7d4bd8));
	if (A(0x7d4bdc) != 0) { rsSet(0x17, 4); rsSet(7, 1); }
	else { rsSet(0x17, 8); rsSet(7, A(0x7d4bd8) != 0 ? 1 : 0); }
	rsSet(0x34, A(0x7d4be0));
	rsSet(0x35, TB(0x5ea228, A(0x7d4be4))); rsSet(0x36, TB(0x5ea228, A(0x7d4be8))); rsSet(0x37, TB(0x5ea228, A(0x7d4bec)));
	g_setRS(0x38, TB(0x5ea24c, A(0x7d4bf0)));
	g_setRS(0x39, A(0x7d4bf4)); g_setRS(0x3a, A(0x7d4bf8)); g_setRS(0x3b, A(0x7d4bfc));
	for (st = 0; st < 8; st++) {
		devSetTexture(st, 0);
		ST(st, S_RASTER) = 0;
		g_setSS(st, 6, TB(0x5ea1a8, 2 * ST(st, S_FILTER)));
		g_setSS(st, 5, TB(0x5ea1a8, 2 * ST(st, S_FILTER)));
		g_setSS(st, 7, TB(0x5ea1ac, 2 * ST(st, S_FILTER)));
		g_setSS(st, 1, TB(0x5ea194, ST(st, S_ADDRU)));
		g_setSS(st, 2, TB(0x5ea194, ST(st, S_ADDRV)));
		g_setSS(st, 4, ST(st, S_BORDER));
		g_setSS(st, 10, ST(st, S_ANISO));
		if (st != 0) { rw37_RwD3D9SetTextureStageState(st, 1, 1); rw37_RwD3D9SetTextureStageState(st, 4, 1); }
	}
	g_setRS(0x13, TB(0x5ea164, A(0x7d4ce0)));
	g_setRS(0x14, TB(0x5ea164, A(0x7d4ce4)));
	g_setRS(0x19, TB(0x5ea24c, A(0x7d4ce8)));
	g_setRS(0x18, alphaRef);
	if (A(0x7d4c00) != 0) { g_setRS(0x1b, 1); g_setRS(0xf, A(0x7d4cec)); }
	else { g_setRS(0x1b, 0); g_setRS(0xf, 0); }
	A(0x7d4c04) = 0;
	g_setRS(0x89, 0);
	g_setRS(0x16, TB(0x5ea218, A(0x7d4c08)));
	rw37_RwD3D9SetTextureStageState(0, 1, 3); rw37_RwD3D9SetTextureStageState(0, 3, 0);
	rw37_RwD3D9SetTextureStageState(0, 4, 3); rw37_RwD3D9SetTextureStageState(0, 6, 0);
	g_setRS(0x91, 0); g_setRS(0x92, 0); g_setRS(0x93, 0);
	g_setRS(0x1a, CAPS(RASTERCAPS) & 1);
	g_setRS(0x1d, 0); g_setRS(0x8e, 0); g_setRS(0x8b, 0xffffffffu); g_setRS(0x8f, 0);
	return rw37__rwD3D9RenderStateFlushCache();
}

// rwD3D9RWGetRenderState 0x4e5070 (engine fpRenderStateGet)
i32 __cdecl rw37__rwD3D9RWGetRenderState(i32 state, u32 *out){
	switch (state) {
	case 1: *out = A(0x7d4c20); return 1;
	case 2: if (A(0x7d4c24) == A(0x7d4c28)) { *out = A(0x7d4c24); return 1; } *out = 0; return 0;
	case 3: *out = A(0x7d4c24); return 1;
	case 4: *out = A(0x7d4c28); return 1;
	case 6: *out = A(0x7d4bdc); return 1;
	case 7: *out = A(0x7d4c1c); return 1;
	case 8: *out = A(0x7d4bd8); return 1;
	case 9: *out = A(0x7d4c2c); return 1;
	case 10: *out = A(0x7d4ce0); return 1;
	case 11: *out = A(0x7d4ce4); return 1;
	case 12: *out = A(0x7d4c00); return 1;
	case 13: *out = A(0x7d4c30); return 1;
	case 14: *out = A(0x7d4c0c); return 1;
	case 15: *out = A(0x7d4c18); return 1;
	case 16: *out = A(0x7d4c10); return 1;
	case 17: *out = x87w(A(0x7d4c14)); return 1;
	case 20: *out = A(0x7d4c08); return 1;
	case 21: *out = A(0x7d4be0); return 1;
	case 22: *out = A(0x7d4be4); return 1;
	case 23: *out = A(0x7d4be8); return 1;
	case 24: *out = A(0x7d4bec); return 1;
	case 25: *out = A(0x7d4bf0); return 1;
	case 26: *out = A(0x7d4bf4); return 1;
	case 27: *out = A(0x7d4bf8); return 1;
	case 28: *out = A(0x7d4bfc); return 1;
	case 29: *out = A(0x7d4ce8); return 1;
	case 30: *out = A(0x7d39a8); return 1;
	default: return 0;		// 5 perspective, 18, 19, out of range: nothing written
	}
}

// rwD3D9RenderStateTextureFilter 0x4e52f0: stage 0 filter mode (anisotropy back to 1 first)
i32 __cdecl rw37__rwD3D9RenderStateTextureFilter(u32 filter){
	if ((i32)ST(0, S_ANISO) > 1) { ST(0, S_ANISO) = 1; SAMP(0, 10) = 1; devSetSS(0, 10, 1); }
	if (filter != ST(0, S_FILTER)) {
		u32 mip = TB(0x5ea1ac, 2 * filter), mn = TB(0x5ea1a8, 2 * filter);
		ST(0, S_FILTER) = filter;
		SAMP(0, 6) = mn; devSetSS(0, 6, mn);
		SAMP(0, 5) = mn; devSetSS(0, 5, mn);
		if (SAMP(0, 7) != mip) { SAMP(0, 7) = mip; devSetSS(0, 7, mip); }
	}
	return 1;
}

// the alpha-blend / alpha-test pair (0x1b / 0xf) behind the stage 0 raster alpha 0x7d4c04 and vertex alpha 0x7d4c00
static inline void alphaOn(void){
	rsPut(0x1b, 1);
	if (A(0x7d3960) != A(0x7d4cec)) { A(0x7d3960) = A(0x7d4cec); dirty(0xf); }
}
static inline void alphaOff(void){
	rsPut(0x1b, 0);
	if (A(0x7d3960) != 0) { A(0x7d3960) = 0; dirty(0xf); }
}

// rwD3D9RWSetRasterStage 0x4e5440: the raster's D3D9 extension {texture, palette, alpha}
i32 __cdecl rw37__rwD3D9RWSetRasterStage(u8 *raster, u32 stage){
	u8 *ext = raster ? raster + *g_rasExt : 0;
	if (stage == 0) {
		if (raster && ext[8]) {
			if (A(0x7d4c04) == 0) { A(0x7d4c04) = 1; if (A(0x7d4c00) == 0) alphaOn(); }
		} else {
			if (A(0x7d4c04) != 0) { A(0x7d4c04) = 0; if (A(0x7d4c00) == 0) alphaOff(); }
		}
	}
	if ((u32)(uintptr_t)raster != ST(stage, S_RASTER)) {
		ST(stage, S_RASTER) = (u32)(uintptr_t)raster;
		if (raster) {
			devSetTexture(stage, *(u32 *)ext);
			u8 *pal = *(u8 **)(ext + 4);
			if (pal) devSetPalette(*(u32 *)(pal + 0x400));
		} else devSetTexture(stage, 0);
	}
	return 1;
}

// RwD3D9SetTexture 0x4e55d0: addressing, anisotropy (texture plugin byte at offset 0x7d4d00, when > 0) and filter of the texture, then its raster
i32 __cdecl rw37_RwD3D9SetTexture(u8 *texture, u32 stage){
	if (texture) {
		u32 fa = *(u32 *)(texture + 0x50), u = fa >> 8 & 0xf;
		if (u != ST(stage, S_ADDRU)) { ST(stage, S_ADDRU) = u; u32 d = TB(0x5ea194, u); SAMP(stage, 1) = d; devSetSS(stage, 1, d); }
		fa = *(u32 *)(texture + 0x50); u32 v = fa >> 12 & 0xf;
		if (v != ST(stage, S_ADDRV)) { ST(stage, S_ADDRV) = v; u32 d = TB(0x5ea194, v); SAMP(stage, 2) = d; devSetSS(stage, 2, d); }
		u32 filt = *(u32 *)(texture + 0x50) & 0xff;
		i32 off = (i32)A(0x7d4d00);
		if (off > 0) {
			u32 an = texture[off];
			if (ST(stage, S_ANISO) != an) { ST(stage, S_ANISO) = an; SAMP(stage, 10) = an; devSetSS(stage, 10, an); }
			if ((i32)an > 1) filt += 7;
		}
		if (ST(stage, S_FILTER) != filt) {
			u32 mn = TB(0x5ea1a8, 2 * filt);
			ST(stage, S_FILTER) = filt;
			u32 mip = TB(0x5ea1ac, 2 * filt);
			if (mn == 3) {
				SAMP(stage, 6) = 3; devSetSS(stage, 6, 3);
				if (CAPS(TEXTUREFILTERCAPS) & 0x4000000) { SAMP(stage, 5) = 3; devSetSS(stage, 5, 3); }
				else { SAMP(stage, 5) = 2; devSetSS(stage, 5, 2); }
			} else {
				SAMP(stage, 6) = mn; devSetSS(stage, 6, mn);
				SAMP(stage, 5) = mn; devSetSS(stage, 5, mn);
			}
			SAMP(stage, 7) = mip; devSetSS(stage, 7, mip);
		}
		rw37__rwD3D9RWSetRasterStage(*(u8 **)texture, stage);
		return 1;
	}
	if (stage == 0 && A(0x7d4c04) != 0) { A(0x7d4c04) = 0; if (A(0x7d4c00) == 0) alphaOff(); }
	if (ST(stage, S_RASTER) != 0) { ST(stage, S_RASTER) = 0; devSetTexture(stage, 0); }
	return 1;
}

// rwD3D9RenderStateVertexAlphaEnable 0x4e5860
i32 __cdecl rw37__rwD3D9RenderStateVertexAlphaEnable(i32 enable){
	if (enable != 0) {
		if (A(0x7d4c00) == 0) { A(0x7d4c00) = 1; if (A(0x7d4c04) == 0) alphaOn(); }
	} else {
		if (A(0x7d4c00) != 0) { A(0x7d4c00) = 0; if (A(0x7d4c04) == 0) alphaOff(); }
	}
	return 1;
}

// the stencil setters 0x4e5960..0x4e5b90: RW value at 0x7d4be0 + 4 i, D3D state 0x34 + i (ops through the table 0x5ea228, the function through 0x5ea24c)
#define STENCIL(name, rw, s, map) i32 __cdecl rw37__rwD3D9RenderState##name(u32 v){ if (v != A(rw)) { rsPut(s, map); A(rw) = v; } return 1; }
STENCIL(StencilEnable, 0x7d4be0, 0x34, v)
STENCIL(StencilFail, 0x7d4be4, 0x35, TB(0x5ea228, v))
STENCIL(StencilZFail, 0x7d4be8, 0x36, TB(0x5ea228, v))
STENCIL(StencilPass, 0x7d4bec, 0x37, TB(0x5ea228, v))
STENCIL(StencilFunction, 0x7d4bf0, 0x38, TB(0x5ea24c, v))
STENCIL(StencilFunctionRef, 0x7d4bf4, 0x39, v)
STENCIL(StencilFunctionMask, 0x7d4bf8, 0x3a, v)
STENCIL(StencilFunctionWriteMask, 0x7d4bfc, 0x3b, v)
STENCIL(ShadeMode, 0x7d4c1c, 9, TB(0x5ea148, v))
STENCIL(CullMode, 0x7d4c08, 0x16, TB(0x5ea218, v))

// rwD3D9RenderStateZTestEnable 0x4e6220 (ZENABLE 7 follows test or write, ZFUNC 0x17 LESSEQUAL / ALWAYS)
i32 __cdecl rw37_rwD3D9RenderStateZTestEnable(i32 enable){
	if (enable != 0) {
		if (A(0x7d4bdc) == 0) { if (A(0x7d4bd8) == 0) rsPut(7, 1); rsPut(0x17, 4); A(0x7d4bdc) = 1; }
	} else {
		if (A(0x7d4bdc) != 0) { if (A(0x7d4bd8) == 0) rsPut(7, 0); rsPut(0x17, 8); A(0x7d4bdc) = 0; }
	}
	return 1;
}

// rwD3D9RWSetRenderState 0x4e5be0 (engine fpRenderStateSet)
i32 __cdecl rw37__rwD3D9RWSetRenderState(i32 state, u32 value){
	switch (state) {
	case 1: return rw37__rwD3D9RWSetRasterStage((u8 *)(uintptr_t)value, 0);
	case 2:
		if (value != A(0x7d4c24)) { u32 d = TB(0x5ea194, value); A(0x7d4c24) = value; SAMP(0, 1) = d; devSetSS(0, 1, d); }
		if (value != A(0x7d4c28)) { A(0x7d4c28) = value; u32 d = TB(0x5ea194, value); SAMP(0, 2) = d; devSetSS(0, 2, d); }
		return 1;
	case 3: if (value != A(0x7d4c24)) { A(0x7d4c24) = value; u32 d = TB(0x5ea194, value); SAMP(0, 1) = d; devSetSS(0, 1, d); } return 1;
	case 4: if (value != A(0x7d4c28)) { A(0x7d4c28) = value; u32 d = TB(0x5ea194, value); SAMP(0, 2) = d; devSetSS(0, 2, d); } return 1;
	case 5: return value != 0;
	case 6: return rw37_rwD3D9RenderStateZTestEnable((i32)value);
	case 7: return rw37__rwD3D9RenderStateShadeMode(value);
	case 8:
		if (value != 0) {
			if (A(0x7d4bd8) == 0) { if (A(0x7d4bdc) == 0) rsPut(7, 1); rsPut(0xe, 1); A(0x7d4bd8) = 1; }
		} else {
			if (A(0x7d4bd8) != 0) { if (A(0x7d4bdc) == 0) rsPut(7, 0); rsPut(0xe, 0); A(0x7d4bd8) = 0; }
		}
		return 1;
	case 9: return rw37__rwD3D9RenderStateTextureFilter(value);
	case 10: return g_srcBlend((i32)value);
	case 11: return g_destBlend((i32)value);
	case 12: return rw37__rwD3D9RenderStateVertexAlphaEnable((i32)value);
	case 13: if (value != A(0x7d4c30)) { A(0x7d4c30) = value; SAMP(0, 4) = value; devSetSS(0, 4, value); } return 1;
	case 14:
		if (value != 0) {
			if (A(0x7d4c0c) == 0 && (CAPS(RASTERCAPS) & 0x180)) { rsPut(0x1c, 1); A(0x7d4c0c) = 1; }
		} else {
			if (A(0x7d4c0c) != 0) { rsPut(0x1c, 0); A(0x7d4c0c) = 0; }
		}
		return 1;
	case 15: if (value != A(0x7d4c18)) { rsPut(0x22, value); A(0x7d4c18) = value; } return 1;
	case 16:
		if (value != A(0x7d4c10)) {
			if (tableFog()) {
				rsPut(0x23, TB(0x5ea154, value));
				if (A(0x7d3d48) != 0) { A(0x7d3d48) = 0; dirty(0x8c); }
			} else {
				if (A(0x7d3a00) != 0) { A(0x7d3a00) = 0; dirty(0x23); }
				rsPut(0x8c, TB(0x5ea154, value));
			}
			A(0x7d4c10) = value;
		}
		return 1;
	case 17: {
		u32 w = *(const u32 *)(uintptr_t)value;
		float fv = *(const float *)(uintptr_t)value, fc = AF(0x7d4c14);
		if (!(fv == fc)) { rsPut(0x26, w); A(0x7d4c14) = w; }
		return 1; }
	case 20: return rw37__rwD3D9RenderStateCullMode(value);
	case 21: return rw37__rwD3D9RenderStateStencilEnable(value);
	case 22: return rw37__rwD3D9RenderStateStencilFail(value);
	case 23: return rw37__rwD3D9RenderStateStencilZFail(value);
	case 24: return rw37__rwD3D9RenderStateStencilPass(value);
	case 25: return rw37__rwD3D9RenderStateStencilFunction(value);
	case 26: return rw37__rwD3D9RenderStateStencilFunctionRef(value);
	case 27: return rw37__rwD3D9RenderStateStencilFunctionMask(value);
	case 28: return rw37__rwD3D9RenderStateStencilFunctionWriteMask(value);
	case 29:
		if (value != A(0x7d4ce8)) {
			A(0x7d4ce8) = value;
			u32 e = value != 8;
			A(0x7d4cec) = e;
			if (A(0x7d39c0) != 0) g_setRS(0xf, e);
			rsPut(0x19, TB(0x5ea24c, value));
		}
		return 1;
	case 30: g_setRS(0x18, value); return 1;
	default: return 0;		// 18, 19, out of range
	}
}

}
