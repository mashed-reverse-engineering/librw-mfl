// D3D9SUBMIT (group d3d9submit, lane K1 window 6): the Im3D submit node of the D3D9 driver of MFL.exe (rpworld noded3d9submitnolight bucket), written op for op from
// the disassembly, on the exe's own device, Im3D globals, render state caches and callees:
//   rxD3D9SubmitNode 0x4f0c90 (1788 B incl. its two jump tables), _rwD3D9Im3DRenderOpen 0x4f0a40 (578), _rwD3D9Im3DRenderClose 0x4f09d0 (98),
//   D3D9ImmInstanceNode 0x4f1390 (18)
// Exe binding: mflrw_d3d9submit_bind(table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_d3d9submit_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_DEVICE, E_CAPS, E_STASH, E_BASEINDEX, E_CURVB, E_IBOFFSET, E_INDEXBUFFER, E_DECLOLD, E_DECLIM3D, E_DECLNOTEX,
         E_LASTFVF, E_LASTDECL, E_LASTVS, E_LASTPS, E_LASTIB, E_PRIMTABLE,
         E_VERTEXALPHA, E_SETXFORMWORLD, E_SETRS, E_GETRASTERSTAGE, E_SETTSS, E_DYNLOCK, E_DYNUNLOCK, E_SETSTREAM, E_FLUSH, E_CREATEDECL, E_DELETEDECL,
         E_N };
static void *g_m[E_N];
static int g_bound;
static void need(void){
	if(!g_bound){ fprintf(stderr, "[mflrw] d3d9submit called before mflrw_d3d9submit_bind\n"); abort(); }
}
#define U32AT(p) (*(u32 *)(p))
#define U16AT(p) (*(u16 *)(p))
#define P32(p) ((u32)(uintptr_t)(p))
#define G(e) (*(u32 *)g_m[e])      // an exe global word
typedef u32 (__cdecl *F0)(void);
typedef u32 (__cdecl *F1)(u32);
typedef u32 (__cdecl *F2)(u32, u32);
typedef u32 (__cdecl *F3)(u32, u32, u32);
typedef u32 (__cdecl *F4)(u32, u32, u32, u32);
typedef u32 (__cdecl *F5)(u32, u32, u32, u32, u32);
// COM calls: IDirect3DDevice9 slots 27 CreateIndexBuffer, 81 DrawPrimitive, 82 DrawIndexedPrimitive, 83 DrawPrimitiveUP, 84 DrawIndexedPrimitiveUP,
// 87 SetVertexDeclaration, 92 SetVertexShader, 104 SetIndices, 107 SetPixelShader; IDirect3DIndexBuffer9 slots 2 Release, 11 Lock, 12 Unlock
typedef long (__stdcall *M1)(u32 self);
typedef long (__stdcall *M2)(u32 self, u32);
typedef long (__stdcall *M4)(u32 self, u32, u32, u32);
typedef long (__stdcall *M5)(u32 self, u32, u32, u32, u32);
typedef long (__stdcall *M7)(u32 self, u32, u32, u32, u32, u32, u32);
typedef long (__stdcall *M9)(u32 self, u32, u32, u32, u32, u32, u32, u32, u32);
#define VT(obj, slot) ((*(void ***)(uintptr_t)(obj))[slot])

struct El { u16 stream, offset; u8 type, method, usage, usageIndex; };      // D3DVERTEXELEMENT9
static void setEl(El *e, u32 stream, u32 offset, u8 type, u8 usage){
	e->stream = (u16)stream; e->offset = (u16)offset; e->type = type; e->method = 0; e->usage = usage; e->usageIndex = 0;
}
// the index buffer and the three declarations are released and their globals cleared (the same sequence in RenderClose and in RenderOpen's failure path)
static void im3dRelease(void){
	u32 o = G(E_INDEXBUFFER);
	if(o){ ((M1)VT(o, 2))(o); G(E_INDEXBUFFER) = 0; }
	if((o = G(E_DECLNOTEX)) != 0){ ((F1)g_m[E_DELETEDECL])(o); G(E_DECLNOTEX) = 0; }
	if((o = G(E_DECLIM3D)) != 0){ ((F1)g_m[E_DELETEDECL])(o); G(E_DECLIM3D) = 0; }
	if((o = G(E_DECLOLD)) != 0){ ((F1)g_m[E_DELETEDECL])(o); G(E_DECLOLD) = 0; }
}
// one Im3D vertex (0x24 bytes: position, normal, colour +0x18, u +0x1c, v +0x20) to the buffer layout: position, colour (, u, v)
static u32 *putVertex(u32 *dst, const u32 *src, u32 hasTex){
	dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[6];
	if(!hasTex) return dst + 4;
	dst[4] = src[7]; dst[5] = src[8];
	return dst + 6;
}
static void setDecl(u32 decl){
	if(G(E_LASTDECL) != decl){
		u32 dev = G(E_DEVICE);
		G(E_LASTDECL) = decl;
		G(E_LASTFVF) = 0xffffffff;
		((M2)VT(dev, 87))(dev, decl);
	}
}

extern "C" {

void __cdecl mflrw_d3d9submit_bind(void *const *tbl){
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
	g_bound = 1;
}

// D3D9ImmInstanceNode 0x4f1390 (node, params): params[0] (the Im3D stash) becomes the current stash.  Returns 1.
int32_t __cdecl rw37_D3D9ImmInstanceNode(void *node, u32 *params){
	need();
	G(E_STASH) = params[0];
	return 1;
}

// _rwD3D9Im3DRenderClose 0x4f09d0: releases the Im3D index buffer and the three vertex declarations.
void __cdecl rw37_rwD3D9Im3DRenderClose(void){
	need();
	im3dRelease();
}

// _rwD3D9Im3DRenderOpen 0x4f0a40: index buffer offset 0, a dynamic write only 16 bit index buffer of 0x1fffe bytes (pool 2 = system memory without hardware T&L,
// DevCaps +0x1c bit 0x10000, else 0), then the three declarations: "old" (position, normal +0xc, colour +0x18, uv +0x1c: the RwIm3DVertex itself), Im3D (position,
// colour +0xc, uv +0x10), no texture (position, colour +0xc).  Returns 1; when the index buffer fails everything is released and 0 returned.
int32_t __cdecl rw37_rwD3D9Im3DRenderOpen(void){
	need();
	u32 pool = (~U32AT((u8 *)g_m[E_CAPS] + 0x1c) >> 0xf) & 2;
	u32 dev = G(E_DEVICE);
	G(E_IBOFFSET) = 0;
	if(((M7)VT(dev, 27))(dev, 0x1fffe, 0x208, 0x65, pool, P32(g_m[E_INDEXBUFFER]), 0) != 0){
		im3dRelease();
		return 0;
	}
	El d[5];
	setEl(&d[0], 0, 0, 2, 0);
	setEl(&d[1], 0, 0xc, 2, 3);
	setEl(&d[2], 0, 0x18, 4, 0xa);
	setEl(&d[3], 0, 0x1c, 1, 5);
	setEl(&d[4], 0xff, 0, 0x11, 0);
	((F2)g_m[E_CREATEDECL])(P32(d), P32(g_m[E_DECLOLD]));
	setEl(&d[0], 0, 0, 2, 0);
	setEl(&d[1], 0, 0xc, 4, 0xa);
	setEl(&d[2], 0, 0x10, 1, 5);
	setEl(&d[3], 0xff, 0, 0x11, 0);
	((F2)g_m[E_CREATEDECL])(P32(d), P32(g_m[E_DECLIM3D]));
	setEl(&d[0], 0, 0, 2, 0);
	setEl(&d[1], 0, 0xc, 4, 0xa);
	setEl(&d[2], 0xff, 0, 0x11, 0);
	((F2)g_m[E_CREATEDECL])(P32(d), P32(g_m[E_DECLNOTEX]));
	return 1;
}

// rxD3D9SubmitNode 0x4f0c90 (reads no argument).  Stash: vertex count u16 +0, vertices +4, flags +0xc (1 textured, 2 all opaque, 4 no clipping), matrix +0x10, primitive type +0x30, indices +0x34, index count +0x38.
// States: vertex alpha = !(flags & 2), world transform, render states 0x89 = 0, 0x8f = 0, 0x88 = !(flags & 4); texture stage 0: without a raster colour op 3 /
// arg2 0 and alpha op 3 / arg2 0, with one modulate (4) of texture (2) and diffuse (0) for both.  Declaration (Im3D or no texture), no vertex / pixel shader.
// Nothing more without vertices.  Primitive count from the D3D primitive type (table at 0x5ea2a8): 2 n / 2, 3 n - 1, 4 n / 3, 5 and 6 n - 2 (any other type: the
// original uses an unset stack slot, here 0).
// Indexed with at most 8 indices (and not more than vertices): the indexed vertices are copied into the dynamic vertex buffer and drawn as a plain primitive.
// Indexed otherwise (needs the index buffer and at most 0xffff indices): all vertices go to the dynamic vertex buffer, the indices to the index buffer (appended
// with lock flags 0x1800, or from offset 0 with 0x2800 when offset + count would pass 0xffff), DrawIndexedPrimitive, offset += count.
// Not indexed: all vertices to the dynamic vertex buffer, DrawPrimitive.  When a lock or the draw fails (or the index buffer is missing): the "old" declaration, no
// stream (indexed: no indices), DrawIndexedPrimitiveUP / DrawPrimitiveUP from the stash memory (stride 0x24).  Returns 1.
int32_t __cdecl rw37_rxD3D9SubmitNode(void){
	need();
	u8 *stash = (u8 *)(uintptr_t)G(E_STASH);
	u8 *p = stash + 0xc;
	u32 verts = U32AT(stash + 4);
	u32 flags = U32AT(p);
	u32 decl, hasTex, stride, dev, i;
	if(flags & 1){ decl = G(E_DECLIM3D); hasTex = 1; stride = 0x18; }
	else{ decl = G(E_DECLNOTEX); hasTex = 0; stride = 0x10; }
	((F1)g_m[E_VERTEXALPHA])((flags & 2) ? 0 : 1);
	((F1)g_m[E_SETXFORMWORLD])(U32AT(p + 4));
	((F2)g_m[E_SETRS])(0x89, 0);
	((F2)g_m[E_SETRS])(0x8f, 0);
	((F2)g_m[E_SETRS])(0x88, (p[0] & 4) ? 0 : 1);
	F3 tss = (F3)g_m[E_SETTSS];
	if(((F1)g_m[E_GETRASTERSTAGE])(0) == 0){
		tss(0, 1, 3); tss(0, 3, 0); tss(0, 4, 3); tss(0, 6, 0);
	}else{
		tss(0, 1, 4); tss(0, 2, 2); tss(0, 3, 0); tss(0, 4, 4); tss(0, 5, 2); tss(0, 6, 0);
	}
	setDecl(decl);
	if(G(E_LASTVS) != 0){
		dev = G(E_DEVICE);
		G(E_LASTVS) = 0;
		((M2)VT(dev, 92))(dev, 0);
	}
	if(G(E_LASTPS) != 0){
		dev = G(E_DEVICE);
		G(E_LASTPS) = 0;
		((M2)VT(dev, 107))(dev, 0);
	}
	u32 prim = ((u32 *)g_m[E_PRIMTABLE])[U32AT(p + 0x24)];
	u32 numVerts = U16AT((u8 *)(uintptr_t)G(E_STASH));
	if(numVerts == 0) return 1;
	u32 indices = U32AT(p + 0x28);
	u32 numIndices;
	u32 numPrims = 0;      // the original's slot is not preset: undefined for primitive types outside 2..6
	u32 *locked = 0;       // the original's slot is not preset either (written by a successful lock)
	long hr;
	if(indices != 0 && (numIndices = U32AT(p + 0x2c)) != 0){
		switch(prim){
		case 2: numPrims = numIndices >> 1; break;
		case 3: numPrims = numIndices - 1; break;
		case 4: numPrims = numIndices / 3; break;
		case 5: case 6: numPrims = numIndices - 2; break;
		}
		if(numIndices <= numVerts && numIndices <= 8){
			if(!((F5)g_m[E_DYNLOCK])(stride, numIndices, P32(g_m[E_CURVB]), P32(&locked), P32(g_m[E_BASEINDEX]))) goto indexedUP;
			u32 *dst = locked;
			for(i = 0; i < numIndices; i++)
				dst = putVertex(dst, (const u32 *)(uintptr_t)(verts + (u32)((u16 *)(uintptr_t)indices)[i] * 0x24), hasTex);
			((F1)g_m[E_DYNUNLOCK])(G(E_CURVB));
			((F4)g_m[E_SETSTREAM])(0, G(E_CURVB), 0, stride);
			((F0)g_m[E_FLUSH])();
			u32 base = G(E_BASEINDEX);
			dev = G(E_DEVICE);
			hr = ((M4)VT(dev, 81))(dev, prim, base, numPrims);
		}else{
			if(G(E_INDEXBUFFER) == 0 || numIndices > 0xffff) goto indexedUP;
			if(!((F5)g_m[E_DYNLOCK])(stride, numVerts, P32(g_m[E_CURVB]), P32(&locked), P32(g_m[E_BASEINDEX]))) goto indexedUP;
			u32 *dst = locked;
			const u32 *src = (const u32 *)(uintptr_t)verts;
			i = numVerts;
			do{ dst = putVertex(dst, src, hasTex); src += 9; }while(--i);
			((F1)g_m[E_DYNUNLOCK])(G(E_CURVB));
			u32 ibo = G(E_IBOFFSET);
			u32 bytes = numIndices + numIndices;
			u32 ib = G(E_INDEXBUFFER);
			if(ibo + numIndices > 0xffff){
				G(E_IBOFFSET) = 0;
				hr = ((M5)VT(ib, 11))(ib, 0, bytes, P32(&locked), 0x2800);
			}else
				hr = ((M5)VT(ib, 11))(ib, ibo + ibo, bytes, P32(&locked), 0x1800);
			if(hr < 0) goto indexedUP;
			memcpy(locked, (const void *)(uintptr_t)indices, bytes);
			ib = G(E_INDEXBUFFER);
			((M1)VT(ib, 12))(ib);
			((F4)g_m[E_SETSTREAM])(0, G(E_CURVB), 0, stride);
			ib = G(E_INDEXBUFFER);
			if(G(E_LASTIB) != ib){
				dev = G(E_DEVICE);
				G(E_LASTIB) = ib;
				((M2)VT(dev, 104))(dev, ib);
			}
			((F0)g_m[E_FLUSH])();
			dev = G(E_DEVICE);
			hr = ((M7)VT(dev, 82))(dev, prim, G(E_BASEINDEX), 0, numVerts, G(E_IBOFFSET), numPrims);
			G(E_IBOFFSET) += numIndices;
		}
		if(hr >= 0) return 1;
	indexedUP:
		setDecl(G(E_DECLOLD));
		((F4)g_m[E_SETSTREAM])(0, 0, 0, 0);
		if(G(E_LASTIB) != 0){
			dev = G(E_DEVICE);
			G(E_LASTIB) = 0;
			((M2)VT(dev, 104))(dev, 0);
		}
		((F0)g_m[E_FLUSH])();
		dev = G(E_DEVICE);
		((M9)VT(dev, 84))(dev, prim, 0, numVerts, numPrims, indices, 0x65, verts, 0x24);
		return 1;
	}
	switch(prim){
	case 2: numPrims = numVerts >> 1; break;
	case 3: numPrims = numVerts - 1; break;
	case 4: numPrims = numVerts / 3; break;
	case 5: case 6: numPrims = numVerts - 2; break;
	}
	if(((F5)g_m[E_DYNLOCK])(stride, numVerts, P32(g_m[E_CURVB]), P32(&locked), P32(g_m[E_BASEINDEX]))){
		u32 *dst = locked;
		const u32 *src = (const u32 *)(uintptr_t)verts;
		i = numVerts;
		do{ dst = putVertex(dst, src, hasTex); src += 9; }while(--i);
		((F1)g_m[E_DYNUNLOCK])(G(E_CURVB));
		((F4)g_m[E_SETSTREAM])(0, G(E_CURVB), 0, stride);
		((F0)g_m[E_FLUSH])();
		dev = G(E_DEVICE);
		hr = ((M4)VT(dev, 81))(dev, prim, G(E_BASEINDEX), numPrims);
		if(hr >= 0) return 1;
	}
	setDecl(G(E_DECLOLD));
	((F4)g_m[E_SETSTREAM])(0, 0, 0, 0);
	((F0)g_m[E_FLUSH])();
	dev = G(E_DEVICE);
	((M5)VT(dev, 83))(dev, prim, numPrims, verts, 0x24);
	return 1;
}

}
