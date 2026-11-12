// D3D9PTANK (group d3d9ptank, lane K2 window 6): the D3D9 driver rows of the particle tank plugin of MFL.exe (rpptank ptankd3d9 bucket), written op for op from the
// disassembly, on the exe's own engine, device, device state caches, the ptank d3d9 bss block and callees:
//   rpPTankD3D9InsEnding 0x545f50, rpPTankD3D9InsSetup 0x545f60, rpPTankD3D9Create 0x546780, rpPTankD3D9RenderCallBack 0x5462c0
// Not here: the seven point sprite instance functions (rpPTankD3D9Ins*_spr, trap rows) and the generic instance functions: rpPTankD3D9Create only stores / compares
// their addresses, which come through the bind table.
// Exe binding: mflrw_d3d9ptank_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_d3d9ptank_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_PTBSS, E_MAXPOINT, E_PLUGOFF, E_DEVICE, E_LASTFVF, E_LASTDECL, E_LASTVS, E_LASTPS, E_LASTIB,
         E_VBUNLOCK, E_VBLOCK, E_FRAMEGETLTM, E_ATLIGHTCB, E_SETXFORMWORLD, E_GETRS, E_SETRS, E_SETTSS, E_SETRASTER, E_SETSURF, E_SETSTREAM, E_FLUSH,
         E_GEOMCREATE, E_MATCREATE, E_TRISETMAT, E_ATSETGEOM, E_GEOMDESTROY, E_MATDESTROY, E_IBCREATE, E_SELECTCB,
         E_GEN_NCPSNR, E_GEN_CCPSNR, E_GEN_NCCSNR, E_GEN_CCCSNR, E_GEN_NORMALS, E_GEN_COLORPP,
         E_SPR_SETUP, E_SPR_NCPSNR, E_SPR_CCPSNR, E_SPR_NCCSNR, E_SPR_CCCSNR, E_SPR_NORMALS, E_SPR_COLOR, E_INSSETUP, E_INSENDING,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] d3d9ptank called before mflrw_d3d9ptank_bind\n"); abort(); }
	return *g_engine;
}
#define U32AT(p) (*(u32 *)(p))
#define P(p) ((u32)(uintptr_t)(p))
typedef u32 (__cdecl *F0)(void);
typedef u32 (__cdecl *F1)(u32);
typedef u32 (__cdecl *F2)(u32, u32);
typedef u32 (__cdecl *F3)(u32, u32, u32);
typedef u32 (__cdecl *F4)(u32, u32, u32, u32);
typedef u32 (__cdecl *F5)(u32, u32, u32, u32, u32);
#define VT(obj, slot) ((*(void ***)(obj))[slot])
typedef int32_t (__stdcall *D1)(void *self, u32);
typedef int32_t (__stdcall *D3)(void *self, u32, u32, u32);
typedef int32_t (__stdcall *D6)(void *self, u32, u32, u32, u32, u32, u32);
typedef int32_t (__stdcall *LockFn)(void *self, u32 offset, u32 size, void *out, u32 flags);
typedef u32 (__stdcall *Obj0)(void *self);

// the ptank d3d9 bss block 0x7da6c4 .. 0x7da6dc (E_PTBSS = its first word; the three words without a symbol are addressed from it)
enum { B_IB, B_MAXINDEX, B_VB, B_VBOFFSET, B_SIZEPERPARTICLE, B_SPRITES };
static u32 *bss(void){ eng(); return (u32 *)g_m[E_PTBSS]; }
// dst[0..2] = -src[0..2] as the original does it: fld, fchs, fstp (a signalling NaN comes out quiet)
static inline void neg3(const void *src, void *dst){
	__asm__ volatile("flds (%0)\n\tfchs\n\tfstps (%1)\n\tflds 4(%0)\n\tfchs\n\tfstps 4(%1)\n\tflds 8(%0)\n\tfchs\n\tfstps 8(%1)" : : "r"(src), "r"(dst) : "memory", "st");
}
static void rs(u32 state, u32 value){ ((F2)g_m[E_SETRS])(state, value); }
static void tss(u32 stage, u32 type, u32 value){ ((F3)g_m[E_SETTSS])(stage, type, value); }

extern "C" {

void __cdecl mflrw_d3d9ptank_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// rpPTankD3D9InsEnding 0x545f50 (): unlocks the dynamic vertex buffer of the block.  Returns what RwD3D9DynamicVertexBufferUnlock returns.
u32 __cdecl rw37_rpPTankD3D9InsEnding(void){
	return ((F1)g_m[E_VBUNLOCK])(bss()[B_VB]);
}

// rpPTankD3D9InsSetup 0x545f60 (lock, atomic, ptank, numParticles): geometry flag 4 (textured) follows the material's texture.  lock[0] (positions) = 1, lock[3]
// (normals) for geometry flag 0x10, lock[2] (colours) for 8, lock[1] (texture coordinates) for 4; vertex size 12 + 12 + 4 + 8.  The billboard axes lock[12..14] /
// lock[15..17]: ptank flag 0x8000: the constant vectors of the ptank (+0xb0 / +0xc0); else from the camera frame LTM (-right, up), rotated by the angle (+0x78) for
// ptank flag 0x20000 (fsincos; the sine and cosine are stored as floats, -sine stays in the register).  Then 4 vertices per particle of the dynamic vertex buffer are
// locked (block words 2 / 3 = buffer / first vertex): failure returns 0; else lock[4..11] = pointer / stride of each enabled stream ({4,5} positions, {10,11}
// normals, {8,9} colours, {6,7} texture coordinates).  The point sprite flag of the block is cleared.  Returns what the original leaves in eax (the last offset sum).
u32 __cdecl rw37_rpPTankD3D9InsSetup(u32 *lock, u8 *atomic, u8 *pt, u32 numParticles){
	u8 *geom = *(u8 **)(atomic + 0x18);
	u32 locked = 0;
	u32 numVerts = numParticles * 4;
	u8 *mat = **(u8 ***)(geom + 0x20);
	u32 gf = U32AT(geom + 8);
	if(U32AT(mat)){ if(!(gf & 4)) U32AT(geom + 8) = gf | 4; }
	else if(gf & 4) U32AT(geom + 8) = gf & ~4u;
	const u32 *ltm = (const u32 *)(uintptr_t)((F1)g_m[E_FRAMEGETLTM])(U32AT(*(u8 **)eng() + 4));
	const float *ltmf = (const float *)ltm;
	gf = U32AT(geom + 8);
	u32 hasNormals = gf & 0x10, hasColors = gf & 8, stride = 0xc;
	lock[0] = 1;
	if(hasNormals){ stride = 0x18; lock[3] = 1; }
	if(hasColors){ stride += 4; lock[2] = 1; }
	if(gf & 4){ stride += 8; lock[1] = 1; }
	u32 pf = U32AT(pt + 0x58);
	float *out = (float *)lock;
	if((pf & 0x8000) == 0x8000){
		lock[12] = U32AT(pt + 0xb0); lock[13] = U32AT(pt + 0xb4); lock[14] = U32AT(pt + 0xb8);
		lock[15] = U32AT(pt + 0xc0); lock[16] = U32AT(pt + 0xc4); lock[17] = U32AT(pt + 0xc8);
	}else if((pf & 0x20000) == 0x20000){
		// t = the original's stack slots: -right (3 floats, stored), up (3 words copied), cosine, sine, angle.  The x87 sequence is the original's, instruction
		// for instruction (memory operand forms included: which NaN an operation returns depends on them), so it is written as such.
		u32 t[9];
		neg3(ltm, t);
		t[3] = ltm[4]; t[4] = ltm[5]; t[5] = ltm[6];
		t[8] = U32AT(pt + 0x78);
		__asm__ volatile(
			"flds 32(%0)\n\tfsincos\n\tfstps 24(%0)\n\tfstps 28(%0)\n\t"
			"flds 28(%0)\n\tfchs\n\t"
			"flds 12(%0)\n\tfmul %%st(1), %%st\n\tflds 24(%0)\n\tfmuls 0(%0)\n\tfaddp %%st, %%st(1)\n\tfstps 48(%1)\n\t"
			"flds 16(%0)\n\tfmul %%st(1), %%st\n\tflds 24(%0)\n\tfmuls 4(%0)\n\tfaddp %%st, %%st(1)\n\tfstps 52(%1)\n\t"
			"fmuls 20(%0)\n\tflds 24(%0)\n\tfmuls 8(%0)\n\tfaddp %%st, %%st(1)\n\tfstps 56(%1)\n\t"
			"flds 24(%0)\n\tfmuls 12(%0)\n\tflds 28(%0)\n\tfmuls 0(%0)\n\tfaddp %%st, %%st(1)\n\tfstps 60(%1)\n\t"
			"flds 24(%0)\n\tfmuls 16(%0)\n\tflds 28(%0)\n\tfmuls 4(%0)\n\tfaddp %%st, %%st(1)\n\tfstps 64(%1)\n\t"
			"flds 24(%0)\n\tfmuls 20(%0)\n\tflds 28(%0)\n\tfmuls 8(%0)\n\tfaddp %%st, %%st(1)\n\tfstps 68(%1)"
			: : "r"(t), "r"(lock) : "memory", "st", "st(1)", "st(2)");
	}else{
		neg3(ltm, lock + 12);
		lock[15] = ltm[4]; lock[16] = ltm[5]; lock[17] = ltm[6];
	}
	u32 *b = bss();
	if(!((F5)g_m[E_VBLOCK])(stride, numVerts, P(&b[B_VB]), P(&locked), P(&b[B_VBOFFSET]))){
		b[B_SPRITES] = 0;
		return 0;
	}
	if(lock[0]){ lock[4] = locked; lock[5] = stride; }
	u32 eax = 0xc;
	if(lock[3]){ lock[11] = stride; lock[10] = locked + 0xc; }
	if(hasNormals) eax = 0x18;
	if(lock[2]){ lock[9] = stride; lock[8] = eax + locked; }
	if(hasColors) eax += 4;
	if(lock[1]){ eax += locked; lock[7] = stride; lock[6] = eax; }
	b[B_SPRITES] = 0;
	return eax;
}

// rpPTankD3D9Create 0x546780 (atomic, ptank, maxParticles, dataFlags, platformFlags): geometry flags 2 (+0x70 with normals: dataFlags 0x10; +8 colours: dataFlags &
// 0x40042).  Point sprites (platformFlags bit 0) are dropped when the card's maximum point size is <= 1.0, when the ptank flags (+0x58) have a bit of 0xcef7ffe8, or
// have only bit 4 of 0xcef7ffec and the card has no per particle size.  Without point sprites: geometry flag 4 and the shared index buffer (6 indices per particle:
// 0 1 2 0 2 3) is created or regrown.  A geometry with one material (surface 0.3, 1, 1) is set on the atomic, the ptank's colour (+0x7c) becomes 0 0 0 255, the
// generic instance functions are selected, then the driver functions replace them in the atomic's ptank data: point sprites: setup, the position function that
// matches the generic one (constant or per particle size: ptank flag 4), normals, colour per particle, ending; else setup and ending.  Returns 1.
int32_t __cdecl rw37_rpPTankD3D9Create(u8 *atomic, u8 *pt, int32_t maxParticles, u32 dataFlags, u32 platformFlags){
	u32 *b = bss();
	u32 gf = 2;
	if((dataFlags & 0x10) == 0x10) gf = 0x72;
	if(dataFlags & 0x40042) gf |= 8;
	if((platformFlags & 1) == 1){
		int drop;
		if(*(float *)g_m[E_MAXPOINT] <= 1.0f) drop = 1;
		else{
			u32 f = U32AT(pt + 0x58);
			if(!(f & 0xcef7ffec)) drop = 0;
			else if(f & 0xcef7ffe8) drop = 1;
			else drop = b[B_SIZEPERPARTICLE] == 0;
		}
		if(drop) platformFlags &= ~1u;
	}
	u32 sprites = platformFlags & 1;
	if(sprites != 1) gf |= 4;
	u8 *geom = (u8 *)(uintptr_t)((F1)g_m[E_GEOMCREATE])(0x3f800000);
	U32AT(geom + 8) = gf;
	u8 *mat = (u8 *)(uintptr_t)((F0)g_m[E_MATCREATE])();
	u32 tri[2];      // the original passes an uninitialised stack triangle too
	U32AT(mat + 0xc) = 0x3e99999a; U32AT(mat + 0x10) = 0x3f800000; U32AT(mat + 0x14) = 0x3f800000;
	((F3)g_m[E_TRISETMAT])(P(geom), P(tri), P(mat));
	((F3)g_m[E_ATSETGEOM])(P(atomic), P(geom), 0);
	((F1)g_m[E_GEOMDESTROY])(P(geom));
	((F1)g_m[E_MATDESTROY])(P(mat));
	if(sprites != 1){
		void *ib = (void *)(uintptr_t)b[B_IB];
		int keep = 0;
		if(ib){
			if((int32_t)b[B_MAXINDEX] >= maxParticles * 6) keep = 1;
			else{ ((Obj0)VT(ib, 2))(ib); b[B_IB] = 0; }
		}
		if(!keep){
			b[B_MAXINDEX] = (u32)(maxParticles * 6);
			if(((F2)g_m[E_IBCREATE])((u32)(maxParticles * 6), P(&b[B_IB]))){
				u16 *p = (u16 *)(uintptr_t)dataFlags;      // the original's slot (the dataFlags argument) when Lock leaves it unwritten
				ib = (void *)(uintptr_t)b[B_IB];
				if(((LockFn)VT(ib, 11))(ib, 0, 0, &p, 0) >= 0){
					for(int32_t i = 0; i < maxParticles; i++){
						u32 v = (u32)i * 4;
						p[0] = (u16)v; p[1] = (u16)(v + 1); p[2] = (u16)(v + 2); p[3] = (u16)v; p[4] = (u16)(v + 2); p[5] = (u16)(v + 3);
						p += 6;
					}
					ib = (void *)(uintptr_t)b[B_IB];
					((Obj0)VT(ib, 12))(ib);
				}
			}
		}
	}
	pt[0x7c] = 0; pt[0x7d] = 0; pt[0x7e] = 0; pt[0x7f] = 0xff;
	((F1)g_m[E_SELECTCB])(P(atomic));
	u8 *pd = *(u8 **)(atomic + *(u32 *)g_m[E_PLUGOFF]);
	if(sprites == 1){
		U32AT(pd + 0x24) = P(g_m[E_SPR_SETUP]);
		u32 pos = U32AT(pd + 0x28);
		if(pt[0x58] & 4){
			if(pos == P(g_m[E_GEN_NCPSNR])) U32AT(pd + 0x28) = P(g_m[E_SPR_NCPSNR]);
			else if(pos == P(g_m[E_GEN_CCPSNR])) U32AT(pd + 0x28) = P(g_m[E_SPR_CCPSNR]);
		}else{
			if(pos == P(g_m[E_GEN_NCCSNR])) U32AT(pd + 0x28) = P(g_m[E_SPR_NCCSNR]);
			else if(pos == P(g_m[E_GEN_CCCSNR])) U32AT(pd + 0x28) = P(g_m[E_SPR_CCCSNR]);
		}
		if(U32AT(pd + 0x34) == P(g_m[E_GEN_NORMALS])) U32AT(pd + 0x34) = P(g_m[E_SPR_NORMALS]);
		u32 col = U32AT(pd + 0x30);
		U32AT(pd + 0x38) = P(g_m[E_INSENDING]);
		if(col == P(g_m[E_GEN_COLORPP])) U32AT(pd + 0x30) = P(g_m[E_SPR_COLOR]);
	}else{
		U32AT(pd + 0x24) = P(g_m[E_INSSETUP]);
		U32AT(pd + 0x38) = P(g_m[E_INSENDING]);
	}
	return 1;
}

// rpPTankD3D9RenderCallBack 0x5462c0 (atomic, ptank, numParticles): FVF and vertex size from the geometry flags (normals 0x10, colours 8, texture coordinates 4;
// point size when the block's point sprite flag is set and the ptank has per particle sizes: flag 4).  Lighting callback, identity world transform, render states
// 0x89 (lighting) and 9 (shade mode) read, 0x8f = 0.  Unlit without vertex colours: texture factor = the ptank colour, shade mode 1, stage 0 modulates texture and
// factor; else (unlit, coloured, ptank flags without 0x40040: shade mode 1) stage 0 modulates texture and diffuse.  Render state 0x88 = 1, shaders off, FVF set,
// raster of the material's texture on stage 0, vertex alpha and blend modes from the ptank (+0x64, +0x5c, +0x60) through the engine's render state functions (old
// modes restored at the end), surface properties when lit, stream 0 = the block's vertex buffer.  Point sprites: point states, DrawPrimitive(point list) in runs of
// 0xffff; else the shared index buffer and DrawIndexedPrimitive(triangle list) in runs of 0xfffc vertices.  Shade mode restored.  Returns 0.
u32 __cdecl rw37_rpPTankD3D9RenderCallBack(u8 *atomic, u8 *pt, int32_t numParticles){
	u32 *b = bss();
	u8 *geom = *(u8 **)(atomic + 0x18);
	u32 stride = 0xc, fvf = 2;
	u8 *mat = **(u8 ***)(geom + 0x20);
	u32 gf = U32AT(geom + 8);
	if(gf & 0x10){ stride = 0x18; fvf = 0x12; }
	u32 hasColors = gf & 8;
	if(hasColors){ stride += 4; fvf |= 0x40; }
	if(gf & 4){ stride += 8; fvf |= 0x100; }
	if(b[B_SPRITES] && (pt[0x58] & 4)){ stride += 4; fvf |= 0x20; }
	((F1)g_m[E_ATLIGHTCB])(P(atomic));
	((F1)g_m[E_SETXFORMWORLD])(0);
	u32 lighting = P(pt), shade = 0;      // the original reads lighting into the slot of its ptank argument
	((F2)g_m[E_GETRS])(0x89, P(&lighting));
	((F2)g_m[E_GETRS])(9, P(&shade));
	rs(0x8f, 0);
	if(!lighting && !hasColors){
		rs(0x3c, ((u32)pt[0x7f] << 24) | ((u32)pt[0x7c] << 16) | ((u32)pt[0x7d] << 8) | pt[0x7e]);
		rs(9, 1);
		u32 a = U32AT(mat) ? 4 : 3;
		tss(0, 1, a); tss(0, 4, a); tss(0, 2, 2); tss(0, 3, 3); tss(0, 5, 2); tss(0, 6, 3);
	}else{
		if(!lighting && !(U32AT(pt + 0x58) & 0x40040)) rs(9, 1);
		u32 a = U32AT(mat) ? 4 : 3;
		tss(0, 1, a); tss(0, 4, a); tss(0, 2, 2); tss(0, 3, 0); tss(0, 5, 2); tss(0, 6, 0);
	}
	rs(0x88, 1);
	void *dev;
	if(U32AT(g_m[E_LASTPS])){ dev = *(void **)g_m[E_DEVICE]; U32AT(g_m[E_LASTPS]) = 0; ((D1)VT(dev, 107))(dev, 0); }
	if(U32AT(g_m[E_LASTVS])){ dev = *(void **)g_m[E_DEVICE]; U32AT(g_m[E_LASTVS]) = 0; ((D1)VT(dev, 92))(dev, 0); }
	if(U32AT(g_m[E_LASTFVF]) != fvf){
		dev = *(void **)g_m[E_DEVICE];
		U32AT(g_m[E_LASTFVF]) = fvf;
		U32AT(g_m[E_LASTDECL]) = 0xffffffff;
		((D1)VT(dev, 89))(dev, fvf);
	}
	((F2)g_m[E_SETRASTER])(U32AT(mat), 0);
	u32 src = 0, dst = 0;
	if(U32AT(pt + 0x64)){
		(*(F2 *)(eng() + 0x24))(0xa, P(&src));
		(*(F2 *)(eng() + 0x20))(0xa, U32AT(pt + 0x5c));
		(*(F2 *)(eng() + 0x24))(0xb, P(&dst));
		(*(F2 *)(eng() + 0x20))(0xb, U32AT(pt + 0x60));
		(*(F2 *)(eng() + 0x20))(0xc, 1);
	}else (*(F2 *)(eng() + 0x20))(0xc, 0);
	if(lighting) ((F3)g_m[E_SETSURF])(P(mat + 0xc), P(mat + 4), gf);
	((F4)g_m[E_SETSTREAM])(0, b[B_VB], 0, stride);
	if(b[B_SPRITES]){
		rs(0x9c, 1);
		rs(0x9d, 1);
		rs(0x9a, U32AT(pt + 0x70));
		rs(0x9b, 0);
		rs(0xa6, U32AT(g_m[E_MAXPOINT]));
		rs(0x9e, 0);
		rs(0x9f, 0);
		rs(0xa0, 0x3f000000);
		int32_t n = numParticles;
		u32 start = b[B_VBOFFSET];
		for(;;){
			((F0)g_m[E_FLUSH])();
			dev = *(void **)g_m[E_DEVICE];
			if(n <= 0xffff) break;
			((D3)VT(dev, 81))(dev, 1, start, 0xffff);
			start += 0xffff;
			n -= 0xffff;
		}
		((D3)VT(dev, 81))(dev, 1, start, (u32)n);
		rs(0x9c, 0);
		rs(0x9d, 0);
	}else{
		u32 ib = b[B_IB];
		u32 start = b[B_VBOFFSET];
		int32_t n = (int32_t)((u32)numParticles * 4);
		if(U32AT(g_m[E_LASTIB]) != ib){
			dev = *(void **)g_m[E_DEVICE];
			U32AT(g_m[E_LASTIB]) = ib;
			((D1)VT(dev, 104))(dev, ib);
		}
		for(;;){
			((F0)g_m[E_FLUSH])();
			if(n <= 0xfffc) break;
			dev = *(void **)g_m[E_DEVICE];
			((D6)VT(dev, 82))(dev, 4, start, 0, 0xfffc, 0, 0x7ffe);
			n -= 0xfffc;
			start += 0xfffc;
		}
		dev = *(void **)g_m[E_DEVICE];
		((D6)VT(dev, 82))(dev, 4, start, 0, (u32)n, 0, (u32)(n / 2));
	}
	rs(9, shade);
	if(U32AT(pt + 0x64)){
		(*(F2 *)(eng() + 0x20))(0xa, src);
		(*(F2 *)(eng() + 0x20))(0xb, dst);
	}
	return 0;
}

}
