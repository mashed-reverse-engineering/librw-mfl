// D3D9TEXDIC (LIB_RW group d3d9texdic, D6 of plan_engine_switch_window3_2026-11-09): the still-original native texture reader / size of the RW 3.7 D3D9 driver
// (d3d9texdic.obj of rwcore.lib in MFL.exe), written op for op from the disassembly ("kernel on exe state": rasters, textures, stream and device are the exe's).
//   _rwD3D9NativeTextureGetSize 0x4ed270, rwD3D9NativeTextureRead 0x4ed780 (rwD3D9NativeTextureWrite 0x4ed400 is trapped code, not here).
// Raster extension (raster + _RwD3D9RasterExtOffset): +0 texture, +8 alpha, +9 cube (low nibble) / face (high nibble), +0xa automipmap (low) / compressed (high),
// +0x18 d3d format.  Device (*0x7d2240): CreateTexture 23, CreateCubeTexture 25; textures: GetLevelCount 13, GetLevelDesc 17, LockRect 19, UnlockRect 20.
// Exe-layout adapter: mflrw_d3d9texdic_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612_d3d9texdic.inc), table layout = enum B below.
// Check: ghidra tools/delink/rw_d3d9texdic_check.sh (fake device / textures / stream; both sides run the same op sequences natively under Wine).
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

// bind table layout (an_hk_612_d3d9texdic.inc builds it in this order)
enum B { B_DEVVAR, B_RASEXT, B_CAPS, B_FINDCHUNK, B_READ, B_CHECKVALID, B_RASTERCREATE, B_AUTOMIP, B_AUTOMIPCUBE, B_D3DRASTERCREATE,
         B_LOCKPAL, B_UNLOCKPAL, B_RASTERDESTROY, B_LOCK, B_UNLOCK, B_TEXCREATE, B_SETNAME, B_SETMASK, B_N };
static void *const *g;

typedef u32 __attribute__((may_alias)) UA;
typedef u16 __attribute__((may_alias)) UA16;
typedef u8 __attribute__((may_alias)) UA8;
#define V(i) (*(UA *)g[i])
#define W32(a) (*(UA *)(uintptr_t)(a))
#define W8(a) (*(UA8 *)(uintptr_t)(a))
#define P(x) ((u32)(uintptr_t)(x))

#define CALL(i, T) ((T)g[i])
typedef u32 (__cdecl *R1)(u32);
typedef u32 (__cdecl *R2)(u32, u32);
typedef u32 (__cdecl *R3)(u32, u32, u32);
typedef u32 (__cdecl *R4)(u32, u32, u32, u32);
static inline void **VT(u32 o){ return *(void ***)(uintptr_t)o; }
static inline u32 DEV(void){ return V(B_DEVVAR); }
static inline i32 devCreateTex(u32 w, u32 h, u32 lv, u32 usage, u32 fmt, u32 pool, u32 pp){
	u32 d = DEV(); return ((i32 (__stdcall *)(u32, u32, u32, u32, u32, u32, u32, u32, u32))VT(d)[23])(d, w, h, lv, usage, fmt, pool, pp, 0);
}
static inline i32 devCreateCube(u32 edge, u32 lv, u32 usage, u32 fmt, u32 pool, u32 pp){
	u32 d = DEV(); return ((i32 (__stdcall *)(u32, u32, u32, u32, u32, u32, u32, u32))VT(d)[25])(d, edge, lv, usage, fmt, pool, pp, 0);
}
static inline u32 caps(void){ return W32((u8 *)g[B_CAPS] + 0x44); }		// D3DCAPS9.CubeTextureFilterCaps (0x947644)
static inline u32 readS(u32 s, u32 p, u32 n){ return CALL(B_READ, R3)(s, p, n); }
static inline void rasDestroy(u32 r){ CALL(B_RASTERDESTROY, R1)(r); }
// bytes per level as GetSize counts them: compressed = pitch per 4-row block line (a level under 4 rows = one block line)
static inline u32 levelBytes(u32 ext, u32 pitch, u32 height){
	if (W8(ext + 0xa) & 0xf0) return height < 4 ? pitch : (height >> 2) * pitch;
	return pitch * height;
}

extern "C" {

i32 __cdecl mflrw_d3d9texdic_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// _rwD3D9NativeTextureGetSize 0x4ed270: stream size of a native texture: header 0x64 (+0x80 PAL4 / +0x400 PAL8 palette), then per level 4 + the locked
// level's bytes (6 faces for a cube texture); a texture without raster = 0x54.  Always TRUE.
i32 __cdecl rw37__rwD3D9NativeTextureGetSize(u32 *size, u32 tex, u32 unused){
	u32 r = W32(tex), total, desc[8], lr[2];
	if (!r) { *size = 0x54; return 1; }
	total = 0x64;
	if (W8(r + 0x23) & 0x40) total = 0xe4;
	else if (W8(r + 0x23) & 0x20) total = 0x464;
	u32 ext = r + V(B_RASEXT);
	if (W8(ext + 9) & 0xf) {
		u32 t = W32(ext), acc = 0, n;
		if (W8(ext + 0xa) & 0xf) n = 1;
		else n = ((u32 (__stdcall *)(u32))VT(t)[13])(t);
		while (n) {
			n--;
			((i32 (__stdcall *)(u32, u32, u32 *))VT(t)[17])(t, n, desc);
			((i32 (__stdcall *)(u32, u32, u32, u32 *, u32, u32))VT(t)[19])(t, 0, n, lr, 0, 0x8010);
			((i32 (__stdcall *)(u32, u32, u32))VT(t)[20])(t, 0, n);
			acc += levelBytes(ext, lr[0], desc[7]) + 4;
		}
		*size = total + acc * 6;
		return 1;
	}
	u32 n;
	if (W8(ext + 0xa) & 0xf) n = 1;
	else { u32 t = W32(ext); n = ((u32 (__stdcall *)(u32))VT(t)[13])(t); }
	while (n) {
		n--;
		u32 t = W32(ext); ((i32 (__stdcall *)(u32, u32, u32 *))VT(t)[17])(t, n, desc);
		t = W32(ext); ((i32 (__stdcall *)(u32, u32, u32 *, u32, u32))VT(t)[19])(t, n, lr, 0, 0x8010);
		t = W32(ext); ((i32 (__stdcall *)(u32, u32))VT(t)[20])(t, n);
		total += levelBytes(ext, lr[0], desc[7]) + 4;
	}
	*size = total;
	return 1;
}

// rwD3D9NativeTextureRead 0x4ed780: struct chunk (version 0x35000..0x37002), platform 9, raster header; the raster is created (with a d3d format: an empty RW raster
// + CreateTexture / CreateCubeTexture; cube without one: CreateCubeTexture; else RwRasterCreate or RwD3D9RasterCreate), checked against the header, palette and every
// level of every face read into it, then the texture (filter / addressing, name, mask) made.  FALSE on any failure (the raster is destroyed once it exists).
i32 __cdecl rw37_rwD3D9NativeTextureRead(u32 stream, u32 *texOut, u32 unused){
	u32 len, ver, sz;
	u8 nat[0x48], rh[0x10];
	if (!CALL(B_FINDCHUNK, R4)(stream, 1, P(&len), P(&ver))) return 0;
	if (ver < 0x35000 || ver > 0x37002) return 0;
	if (readS(stream, P(nat), 0x48) != 0x48) return 0;
	if (*(UA *)nat != 9) return 0;
	if (readS(stream, P(rh), 0x10) != 0x10) return 0;
	u32 rf = *(UA *)rh, fmt = *(UA *)(rh + 4), w = *(UA16 *)(rh + 8), h = *(UA16 *)(rh + 0xa), depth = rh[0xc], nmip = rh[0xd], type = rh[0xe], fl = rh[0xf];
	u32 r, ext;
	if (fl & 8) {
		if (!CALL(B_CHECKVALID, R1)(fmt)) return 0;
		r = CALL(B_RASTERCREATE, R4)(w, h, depth, (type | 0x80) | rf);
		if (!r) return 0;
		ext = r + V(B_RASEXT);
		u32 lv = (W8(r + 0x23) & 0x80) ? nmip : 1;
		if (fl & 2) {
			if (!(caps() & 0x30000) && lv > 1) goto fail;
			W8(ext + 9) = (W8(ext + 9) & 0xf1) | 1;
			if ((rf & 0x9000) == 0x9000 && CALL(B_AUTOMIPCUBE, R1)(fmt)) W8(ext + 0xa) = (W8(ext + 0xa) & 0xf1) | 1;
			if (devCreateCube(W32(r + 0xc), lv, (W8(ext + 0xa) & 0xf) ? 0x400 : 0, fmt, 1, ext) < 0) goto fail;
			W8(r + 0x21) &= 0x7f;
			u8 c9 = (W8(ext + 9) & 0xf1) | 1, ca = (W8(ext + 0xa) & 0xf) | 0x10;
			W32(ext + 0x18) = fmt; W8(ext + 8) = fl & 1; W8(ext + 9) = c9; W8(ext + 0xa) = ca;
		} else {
			if ((rf & 0x9000) == 0x9000 && CALL(B_AUTOMIP, R1)(fmt)) W8(ext + 0xa) = (W8(ext + 0xa) & 0xf1) | 1;
			if (devCreateTex(W32(r + 0xc), W32(r + 0x10), lv, (W8(ext + 0xa) & 0xf) ? 0x400 : 0, fmt, 1, ext) < 0) goto fail;
			W8(r + 0x21) &= 0x7f;
			u8 ca = (W8(ext + 0xa) & 0xf) | 0x10;
			W32(ext + 0x18) = fmt; W8(ext + 8) = fl & 1; W8(ext + 0xa) = ca;
		}
	} else if (fl & 2) {
		r = CALL(B_RASTERCREATE, R4)(w, h, depth, (type | 0x80) | rf);
		if (!r) return 0;
		ext = r + V(B_RASEXT);
		u32 lv = (W8(r + 0x23) & 0x80) ? nmip : 1;
		if (!(caps() & 0x30000) && lv > 1) goto fail;
		W8(ext + 9) = (W8(ext + 9) & 0xf1) | 1;
		if ((rf & 0x9000) == 0x9000 && CALL(B_AUTOMIPCUBE, R1)(fmt)) W8(ext + 0xa) = (W8(ext + 0xa) & 0xf1) | 1;
		if (devCreateCube(W32(r + 0xc), lv, (W8(ext + 0xa) & 0xf) ? 0x400 : 0, fmt, 1, ext) < 0) goto fail;
		W8(r + 0x21) &= 0x7f;
		W8(ext + 8) = fl & 1;
	} else if (rf & 0xffff6fffu) {
		r = CALL(B_RASTERCREATE, R4)(w, h, depth, type | rf);
		if (!r) return 0;
		ext = r + V(B_RASEXT);
		W8(ext + 0xa) &= 0xf;
	} else {
		r = CALL(B_D3DRASTERCREATE, R4)(w, h, fmt, (rf & 0x9000) | type);
		if (!r) return 0;
		ext = r + V(B_RASEXT);
	}
	if ((fl & 4) && !(W8(ext + 0xa) & 0xf)) goto fail;
	if (rf != (u32)W8(r + 0x23) << 8) goto fail;
	if (fmt != W32(ext + 0x18)) goto fail;
	if (w != W32(r + 0xc)) goto fail;
	if (h != W32(r + 0x10)) goto fail;
	if (rf & 0x4000) {
		u32 p = CALL(B_LOCKPAL, R2)(r, 1);
		u32 n = readS(stream, p, 0x80);
		CALL(B_UNLOCKPAL, R1)(r);
		if (n != 0x80) { rasDestroy(r); return 0; }
	} else if (rf & 0x2000) {
		u32 p = CALL(B_LOCKPAL, R2)(r, 1);
		u32 n = readS(stream, p, 0x400);
		CALL(B_UNLOCKPAL, R1)(r);
		if (n != 0x400) { rasDestroy(r); return 0; }
	}
	{
		u32 mip = W8(r + 0x23) & 0x10;
		W8(r + 0x23) = W8(r + 0x23) & (u8)~mip;
		u8 f9 = W8(ext + 9) & 0xf;
		u32 faces = f9 ? 6 : 1, levels = (W8(ext + 0xa) & 0xf) ? 1 : nmip;
		W8(ext + 9) = f9;
		do {
			for (u32 l = 0; l < levels; l++) {
				u32 px = CALL(B_LOCK, R3)(r, l, 1);
				if (readS(stream, P(&sz), 4) != 4) { CALL(B_UNLOCK, R1)(r); rasDestroy(r); return 0; }
				if (readS(stream, px, sz) != sz) { CALL(B_UNLOCK, R1)(r); rasDestroy(r); return 0; }
				CALL(B_UNLOCK, R1)(r);
			}
			u8 a = W8(ext + 9), c = a & 0xf;
			if (c) W8(ext + 9) = (u8)((u8)((a & 0xf0) + 0x10) ^ c);
		} while (--faces);
		W8(r + 0x23) |= (u8)mip;
	}
	{
		u32 t = CALL(B_TEXCREATE, R1)(r);
		if (!t) goto fail;
		u32 faa = *(UA *)(nat + 4);
		W8(t + 0x50) = (u8)faa;
		u32 v = (W32(t + 0x50) & ~0xf00u) | ((u32)((i32)faa >> 8) & 0xf) << 8;
		W32(t + 0x50) = v;
		W32(t + 0x50) = (v & ~0xf000u) | ((u32)((i32)faa >> 12) & 0xf) << 12;
		CALL(B_SETNAME, R2)(t, P(nat + 8));
		CALL(B_SETMASK, R2)(t, P(nat + 0x28));
		*texOut = t;
		return 1;
	}
fail:
	rasDestroy(r);
	return 0;
}

}
