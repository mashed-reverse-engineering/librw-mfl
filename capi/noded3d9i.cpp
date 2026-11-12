// NODED3D9I (group noded3d9i, lane K1 window 6): the two default instance callbacks of the D3D9 all-in-one pipeline nodes of MFL.exe (rpworld
// noded3d9worldsectorallinone / noded3d9atomicallinone buckets), written op for op from the disassembly, on the exe's own engine, device caps and callees:
//   D3D9WorldSectorDefaultInstanceCallback 0x5005b0 (1037 B), D3D9AtomicDefaultInstanceCallback 0x4ff980 (FUN_004ff980, 2438 B; one x87 multiply)
// Both build a vertex declaration, create the vertex buffer(s), lock them, call the exe's element writers and unlock.  The element writers, buffer create /
// destroy functions, usage flag getters, the CRT qsort and the element sort compare are reached through the bind table (the other rows of noded3d9 stay where they are).
// Exe binding: mflrw_noded3d9i_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_noded3d9i_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_WSUSAGE, E_GEOMUSAGE, E_CAPS, E_CREATEDECL, E_CREATEVB, E_DESTROYVB, E_DYNVBCREATE, E_DYNVBDESTROY, E_ADDDYNGEOM, E_QSORT, E_SORTCB,
         E_INSTV3D, E_INSTV3DCOMP, E_INSTV3DMORPH, E_INSTV2D, E_INSTCOLOR, E_INSTTANGENT,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] noded3d9i called before mflrw_noded3d9i_bind\n"); abort(); }
	return *g_engine;
}
#define U32AT(p) (*(u32 *)(p))
#define U16AT(p) (*(u16 *)(p))
#define P32(p) ((u32)(uintptr_t)(p))
typedef u32 (__cdecl *F1)(u32);
typedef u32 (__cdecl *F2)(u32, u32);
typedef u32 (__cdecl *F4)(u32, u32, u32, u32);
typedef u32 (__cdecl *F5)(u32, u32, u32, u32, u32);
typedef u32 (__cdecl *F7)(u32, u32, u32, u32, u32, u32, u32);
typedef u32 (__cdecl *F8)(u32, u32, u32, u32, u32, u32, u32, u32);
// IDirect3DVertexBuffer9: Lock = slot 11, Unlock = slot 12; IDirect3DVertexDeclaration9: GetDeclaration = slot 4
typedef long (__stdcall *LockFn)(void *self, u32 offset, u32 size, u8 **out, u32 flags);
typedef long (__stdcall *UnlockFn)(void *self);
typedef long (__stdcall *GetDeclFn)(void *self, void *elements, u32 *count);
static void vbLock(void *vb, u32 offset, u32 size, u8 **out, u32 flags){ ((LockFn)(*(void ***)vb)[11])(vb, offset, size, out, flags); }
static void vbUnlock(void *vb){ ((UnlockFn)(*(void ***)vb)[12])(vb); }

// D3DVERTEXELEMENT9
struct El { u16 stream, offset; u8 type, method, usage, usageIndex; };
static void setEl(El *e, u32 stream, u32 offset, u8 type, u8 usage, u8 usageIndex){
	e->stream = (u16)stream; e->offset = (u16)offset; e->type = type; e->method = 0; e->usage = usage; e->usageIndex = usageIndex;
}
// first element with this usage / usage index (the original's loops have no end test either)
static u32 findEl(const El *d, u8 usage, u32 usageIndex){
	u32 i = 0;
	while(d[i].usage != usage || (u32)d[i].usageIndex != usageIndex) i++;
	return i;
}

extern "C" {

void __cdecl mflrw_noded3d9i_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// D3D9WorldSectorDefaultInstanceCallback 0x5005b0 (sector, resEntryHeader, reinstance: not read).  Header: numMeshes +4, vertex stream 0 at +0x10 {buffer, offset,
// stride, geometry flags u16 +0xc, managed u8 +0xe}, useOffsets +0x30, declaration +0x34, vertex count +0x3c, meshes from +0x40 (0x24 each: minVert +4, vertexAlpha
// +0xc, baseIndex +0x14, numVertices +0x18).  One stream: position (float3), normal when the current world's flags (+8) have 0x10 (float3, or type 0xe = DEC3N when
// DeclTypes (caps +0xec) has bit 7), colour with flag 8, texture coordinates 0 with 0x84 and 1 with 0x80, tangent when the sector's usage flags have 0x10000.
// Declaration and a managed vertex buffer are created (0 when the buffer fails), every mesh's baseIndex = buffer offset / stride + minVert, the buffer is locked
// and filled by the exe's element writers (vertexAlpha per mesh from the colour writer, else 0).  Returns 1.
int32_t __cdecl rw37_D3D9WorldSectorDefaultInstanceCallback(u8 *sector, u8 *hdr, u32 reinstance){
	const u8 *caps = (const u8 *)g_m[E_CAPS];
	u32 flags = U32AT(*(u8 **)(eng() + 4) + 8);
	u32 usage = ((F1)g_m[E_WSUSAGE])(P32(sector));
	El decl[20];
	u8 *st = hdr + 0x10;
	u32 n = 1, off = 0xc;
	U32AT(hdr + 0x3c) = U16AT(sector + 0x82);
	setEl(&decl[0], 0, 0, 2, 0, 0);
	U32AT(st + 8) = 0xc;
	U16AT(st + 0xc) = 2;
	if(flags & 0x10){
		if(caps[0xec] & 0x80){ setEl(&decl[1], 0, off, 0xe, 3, 0); off = 0x10; }
		else{ setEl(&decl[1], 0, off, 2, 3, 0); off = 0x18; }
		U32AT(st + 8) = off;
		n = 2;
		U16AT(st + 0xc) = 6;
	}
	if(flags & 8){
		setEl(&decl[n], 0, off, 4, 0xa, 0);
		n++; off += 4;
		st[0xc] |= 8;
		U32AT(st + 8) += 4;
	}
	if(flags & 0x84){
		U32AT(st + 8) += 8;
		setEl(&decl[n], 0, off, 1, 5, 0);
		n++; off += 8;
		st[0xc] |= 0x10;
	}
	if(flags & 0x80){
		setEl(&decl[n], 0, off, 1, 5, 1);
		n++; off += 8;
		st[0xc] |= 0x20;
		U32AT(st + 8) += 8;
	}
	if(usage & 0x10000){
		if(caps[0xec] & 0x80){ setEl(&decl[n], 0, off, 0xe, 6, 0); U32AT(st + 8) += 4; }
		else{ setEl(&decl[n], 0, off, 2, 6, 0); U32AT(st + 8) += 0xc; }
		n++;
	}
	setEl(&decl[n], 0xff, 0, 0x11, 0, 0);
	((F2)g_m[E_CREATEDECL])(P32(decl), P32(hdr + 0x34));
	u32 stride = U32AT(st + 8);
	u32 size = U32AT(hdr + 0x3c) * stride;
	st[0xe] = 1;
	if(!((F4)g_m[E_CREATEVB])(stride, size, P32(st), P32(st + 4))) return 0;
	u32 nm = U32AT(hdr + 4);
	U32AT(hdr + 0x30) = 0;
	u8 *m = hdr + 0x54;
	do{
		U32AT(m) = U32AT(st + 4) / U32AT(st + 8) + U32AT(m - 0x10);
		m += 0x24;
	}while(--nm);
	u8 *locked = 0;      // the original's slot is not preset
	vbLock(*(void **)st, U32AT(st + 4), size, &locked, 0);
	u32 o = ((F5)g_m[E_INSTV3D])(decl[0].type, P32(locked), U32AT(sector + 8), U32AT(hdr + 0x3c), U32AT(st + 8));
	u32 idx = 1;
	if(flags & 0x10){
		o += ((F5)g_m[E_INSTV3DCOMP])(decl[1].type, P32(locked) + o, U32AT(sector + 0xc), U32AT(hdr + 0x3c), U32AT(st + 8));
		idx = 2;
	}
	if(flags & 8){
		u32 colors = U32AT(sector + 0x30);
		u32 cstride = U32AT(st + 8);
		nm = U32AT(hdr + 4);
		m = hdr + 0x44;
		do{
			u32 minv = U32AT(m);
			U32AT(m + 8) = ((F4)g_m[E_INSTCOLOR])(minv * cstride + P32(locked) + o, colors + minv * 4, U32AT(m + 0x14), cstride);
			m += 0x24;
		}while(--nm);
		idx++;
		o += 4;
	}else{
		nm = U32AT(hdr + 4);
		m = hdr + 0x4c;
		do{ U32AT(m) = 0; m += 0x24; }while(--nm);
	}
	if(flags & 0x84){
		o += ((F5)g_m[E_INSTV2D])(decl[idx].type, P32(locked) + o, U32AT(sector + 0x10), U32AT(hdr + 0x3c), U32AT(st + 8));
		idx++;
	}
	if(flags & 0x80){
		o += ((F5)g_m[E_INSTV2D])(decl[idx].type, P32(locked) + o, U32AT(sector + 0x14), U32AT(hdr + 0x3c), U32AT(st + 8));
		idx++;
	}
	if(usage & 0x10000){
		u32 tex = (flags & 0x80) ? U32AT(sector + 0x14) : U32AT(sector + 0x10);
		((F8)g_m[E_INSTTANGENT])(decl[idx].type, P32(locked) + o, U32AT(sector + 8), 0, U32AT(sector + 0xc), tex, P32(hdr), U32AT(st + 8));
	}
	vbUnlock(*(void **)st);
	return 1;
}

// D3D9AtomicDefaultInstanceCallback 0x4ff980 (atomic, resEntryHeader, reinstance).  Geometry at atomic +0x18: flags +8, lock flags u16 +0xc, vertex count +0x14,
// morph target count +0x18, texture coordinate set count +0x1c, colours +0x30, texture coordinates from +0x34, morph targets +0x5c (0x1c each: vertices +0x14,
// normals +0x18).  Header as above, with two vertex streams (+0x10, +0x20; dynamicLock u8 +0xf).
// First instance (reinstance 0): both streams are released (managed: RwD3D9DestroyVertexBuffer; dynamic with offset 0 and no dynamicLock: the dynamic destroy) and
// cleared.  Two streams ("dynamic") when the geometry morphs or has usage flags (low 12 bits), the device has hardware T&L (DevCaps +0x1c bit 0x10000) and the
// atomic's pipeline (+0x6c) is not plugin 0x12e; a morphing geometry then gets usage |= 6.  Each element goes to stream 0 when its usage bit is set (position 2,
// normal 4, colour 8, texture set i 0x10 << i, tangent any of 0xff2), else to the last stream; the elements are sorted by the exe's qsort with the element compare,
// the declaration is created, then the buffers: managed for a static geometry and for stream 1 when DevCaps2 (+0xd4) bit 0 (stream offset) is set, else dynamic
// (0 when one fails).  baseIndex = minVert with two streams (useOffsets = 1 with stream offsets), else offset / stride + minVert (useOffsets 0).  All locked 0x800.
// Reinstance: the declaration is read back, stream count = highest stream + 1, and only streams whose geometry flags meet the lock flags are locked (0x800 managed,
// 0x2800 dynamic: its flags are then added to the lock flags; lock flags 0xfff lock all).
// Then the writers by lock flag: position / normal from the start morph target, or the morph writer for both when the interpolator's targets (s16 +0x50 / +0x52,
// both below the count, else 0 / 0) differ and recipTime * position (+0x5c * +0x58, stored as float) is not 0; colours per mesh (vertexAlpha), else vertexAlpha
// 0 on a first instance; texture sets; tangent.  Locked streams are unlocked.  Returns 1.
int32_t __cdecl rw37_D3D9AtomicDefaultInstanceCallback(u8 *atomic, u8 *hdr, u32 reinstance){
	const u8 *caps = (const u8 *)g_m[E_CAPS];
	eng();
	u8 *geom = *(u8 **)(atomic + 0x18);
	u32 gflags = U32AT(geom + 8);
	u32 usage = ((F1)g_m[E_GEOMUSAGE])(P32(geom));
	u32 numTex = U32AT(geom + 0x1c);
	u32 numStreams, i, s, nm;
	u8 *st, *m;
	El decl[21];
	if(!reinstance){
		U32AT(hdr + 0x3c) = U32AT(geom + 0x14);
		for(i = 0; i < 2; i++){
			st = hdr + (i + 1) * 0x10;
			u32 vb = U32AT(st);
			if(vb){
				if(st[0xe]){
					u32 stride = U32AT(st + 8);
					((F4)g_m[E_DESTROYVB])(stride, U32AT(hdr + 0x3c) * stride, vb, U32AT(st + 4));
				}else if(U32AT(st + 4) == 0 && st[0xf] == 0)
					((F1)g_m[E_DYNVBDESTROY])(vb);
				U32AT(st) = 0;
			}
			U32AT(st + 4) = 0;
			U32AT(st + 8) = 0;
			U16AT(st + 0xc) = 0;
			st[0xe] = 0;
			st[0xf] = 0;
		}
		int32_t nmt = (int32_t)U32AT(geom + 0x18);
		u32 dynamic;
		u8 *pipe;
		if((nmt > 1 || (usage & 0xfff)) && (U32AT(caps + 0x1c) & 0x10000) && !((pipe = *(u8 **)(atomic + 0x6c)) != 0 && U32AT(pipe + 0x2c) == 0x12e)){
			if(nmt > 1) usage |= 6;
			dynamic = 1;
			numStreams = 2;
			((F1)g_m[E_ADDDYNGEOM])(P32(geom));
		}else{
			numStreams = 1;
			dynamic = 0;
		}
		u32 n = 1;
		s = (usage & 2) ? 0 : numStreams - 1;
		st = hdr + 0x10 + s * 0x10;
		setEl(&decl[0], s, U16AT(st + 8), 2, 0, 0);
		U32AT(st + 8) = 0xc;
		U16AT(st + 0xc) = 2;
		if(gflags & 0x10){
			s = (usage & 4) ? 0 : numStreams - 1;
			st = hdr + 0x10 + s * 0x10;
			if(caps[0xec] & 0x80){ setEl(&decl[1], s, U16AT(st + 8), 0xe, 3, 0); U32AT(st + 8) += 4; }
			else{ setEl(&decl[1], s, U16AT(st + 8), 2, 3, 0); U32AT(st + 8) += 0xc; }
			st[0xc] |= 4;
			n = 2;
		}
		if(gflags & 8){
			s = (usage & 8) ? 0 : numStreams - 1;
			st = hdr + 0x10 + s * 0x10;
			setEl(&decl[n], s, U16AT(st + 8), 4, 0xa, 0);
			n++;
			U32AT(st + 8) += 4;
			st[0xc] |= 8;
		}
		for(i = 0; i < numTex; i++){
			s = (usage & (0x10u << (i & 31))) ? 0 : numStreams - 1;
			st = hdr + 0x10 + s * 0x10;
			setEl(&decl[n], s, U16AT(st + 8), 1, 5, (u8)i);
			n++;
			U32AT(st + 8) += 8;
			U16AT(st + 0xc) |= (u16)(0x10u << (i & 31));
		}
		if((usage & 0x10000) && numTex){
			s = (usage & 0xff2) ? 0 : numStreams - 1;
			st = hdr + 0x10 + s * 0x10;
			if(caps[0xec] & 0x80){ setEl(&decl[n], s, U16AT(st + 8), 0xe, 6, 0); U32AT(st + 8) += 4; }
			else{ setEl(&decl[n], s, U16AT(st + 8), 2, 6, 0); U32AT(st + 8) += 0xc; }
			n++;
		}
		if(numStreams > 1){
			u8 *p = hdr + numStreams * 0x10 + 8;      // stride of the last stream
			do{
				if(U32AT(p)) break;
				numStreams--;
				p -= 0x10;
			}while(numStreams > 1);
		}
		((F4)g_m[E_QSORT])(P32(decl), n, 8, P32(g_m[E_SORTCB]));
		setEl(&decl[n], 0xff, 0, 0x11, 0, 0);
		((F2)g_m[E_CREATEDECL])(P32(decl), P32(hdr + 0x34));
		for(i = 0; i < numStreams; i++){
			st = hdr + (i + 1) * 0x10;
			u32 stride = U32AT(st + 8);
			u32 size = U32AT(hdr + 0x3c) * stride;
			u32 r;
			if(!dynamic || (i != 0 && (caps[0xd4] & 1))){
				st[0xe] = 1;
				r = ((F4)g_m[E_CREATEVB])(stride, size, P32(st), P32(st + 4));
			}else{
				st[0xe] = 0;
				r = ((F2)g_m[E_DYNVBCREATE])(size, P32(st));
			}
			if(!r) return 0;
		}
		if(numStreams > 1){
			if(caps[0xd4] & 1) U32AT(hdr + 0x30) = 1;
			nm = U32AT(hdr + 4);
			m = hdr + 0x54;
			do{ U32AT(m) = U32AT(m - 0x10); m += 0x24; }while(--nm);
		}else{
			nm = U32AT(hdr + 4);
			U32AT(hdr + 0x30) = 0;
			m = hdr + 0x54;
			do{
				U32AT(m) = U32AT(hdr + 0x14) / U32AT(hdr + 0x18) + U32AT(m - 0x10);
				m += 0x24;
			}while(--nm);
		}
	}else{
		void *d = *(void **)(hdr + 0x34);
		u32 count;
		((GetDeclFn)(*(void ***)d)[4])(d, decl, &count);
		numStreams = 0;
		for(const El *e = decl; e->type != 0x11; e++)
			if((u32)e->stream >= numStreams) numStreams = (u32)e->stream + 1;
	}
	u8 *locked[2] = {0, 0};
	u32 lockFlags;
	if(!reinstance){
		lockFlags = 0xfff;
		for(i = 0; i < numStreams; i++){
			st = hdr + (i + 1) * 0x10;
			vbLock(*(void **)st, U32AT(st + 4), U32AT(st + 8) * U32AT(hdr + 0x3c), &locked[i], 0x800);
		}
	}else{
		lockFlags = U16AT(geom + 0xc);
		for(i = 0; i < numStreams; i++){
			st = hdr + (i + 1) * 0x10;
			if(lockFlags == 0xfff || (lockFlags & U16AT(st + 0xc))){
				u32 size = U32AT(st + 8) * U32AT(hdr + 0x3c);
				u32 fl;
				if(st[0xe]) fl = 0x800;
				else{ fl = 0x2800; lockFlags |= U16AT(st + 0xc); }
				vbLock(*(void **)st, U32AT(st + 4), size, &locked[i], fl);
			}
		}
	}
	int32_t nmt = (int32_t)U32AT(geom + 0x18);
	int32_t start = 0;
	int morph = 0;
	if(nmt != 1){
		start = (int16_t)U16AT(atomic + 0x50);
		int32_t end = (int16_t)U16AT(atomic + 0x52);
		if(start >= nmt || end >= nmt){ end = 0; start = 0; }
		volatile union { float f; u32 u; } sc;      // x87 product stored as float (the original's stack slot), its bits are the writer's argument
		// operand order as in the original (fld +0x5c, fmul +0x58): with two NaN operands the x87 result is the NaN loaded first, and gcc commutes a C product
		__asm__ volatile("flds 0x5c(%1)\n\tfmuls 0x58(%1)\n\tfstps %0" : "=m"(sc.f) : "r"(atomic) : "st");
		if(start != end && !(sc.f == 0.0f)){
			u32 scale = sc.u;
			morph = 1;
			u8 *mt = *(u8 **)(geom + 0x5c);
			u32 v0 = U32AT(mt + start * 0x1c + 0x14), v1 = U32AT(mt + end * 0x1c + 0x14);
			u32 k = findEl(decl, 0, 0);
			s = decl[k].stream;
			((F7)g_m[E_INSTV3DMORPH])(decl[k].type, (u32)decl[k].offset + P32(locked[s]), v0, v1, scale, U32AT(hdr + 0x3c), U32AT(hdr + s * 0x10 + 0x18));
			if(gflags & 0x10){
				mt = *(u8 **)(geom + 0x5c);
				u32 n0 = U32AT(mt + start * 0x1c + 0x18), n1 = U32AT(mt + end * 0x1c + 0x18);
				k = findEl(decl, 3, 0);
				s = decl[k].stream;
				((F7)g_m[E_INSTV3DMORPH])(decl[k].type, (u32)decl[k].offset + P32(locked[s]), n0, n1, scale, U32AT(hdr + 0x3c), U32AT(hdr + s * 0x10 + 0x18));
			}
		}
	}
	if(!morph){
		if(lockFlags & 2){
			u32 v = U32AT(*(u8 **)(geom + 0x5c) + start * 0x1c + 0x14);
			u32 k = findEl(decl, 0, 0);
			s = decl[k].stream;
			((F5)g_m[E_INSTV3D])(decl[k].type, (u32)decl[k].offset + P32(locked[s]), v, U32AT(hdr + 0x3c), U32AT(hdr + s * 0x10 + 0x18));
		}
		if((gflags & 0x10) && (lockFlags & 4)){
			u32 v = U32AT(*(u8 **)(geom + 0x5c) + start * 0x1c + 0x18);
			u32 k = findEl(decl, 3, 0);
			s = decl[k].stream;
			((F5)g_m[E_INSTV3D])(decl[k].type, (u32)decl[k].offset + P32(locked[s]), v, U32AT(hdr + 0x3c), U32AT(hdr + s * 0x10 + 0x18));
		}
	}
	if(gflags & 8){
		if(lockFlags & 8){
			u32 k = findEl(decl, 0xa, 0);
			u32 colors = U32AT(geom + 0x30);
			u32 stride = U32AT(hdr + (u32)decl[k].stream * 0x10 + 0x18);
			nm = U32AT(hdr + 4);
			m = hdr + 0x44;
			do{
				u32 minv = U32AT(m);
				U32AT(m + 8) = ((F4)g_m[E_INSTCOLOR])(P32(locked[decl[k].stream]) + minv * stride + (u32)decl[k].offset, colors + minv * 4, U32AT(m + 0x14), stride);
				m += 0x24;
			}while(--nm);
		}
	}else if(!reinstance){
		nm = U32AT(hdr + 4);
		m = hdr + 0x4c;
		do{ U32AT(m) = 0; m += 0x24; }while(--nm);
	}
	if(numTex && (lockFlags & 0xff0)){
		for(i = 0; i < numTex; i++){
			if(lockFlags & (0x10u << (i & 31))){
				u32 tex = U32AT(geom + 0x34 + i * 4);
				u32 k = findEl(decl, 5, i);
				s = decl[k].stream;
				((F5)g_m[E_INSTV2D])(decl[k].type, (u32)decl[k].offset + P32(locked[s]), tex, U32AT(hdr + 0x3c), U32AT(hdr + s * 0x10 + 0x18));
			}
		}
	}
	if((usage & 0x10000) && numTex && (lockFlags & 0xff2)){
		u32 k = findEl(decl, 6, 0);
		s = decl[k].stream;
		u8 *mt = *(u8 **)(geom + 0x5c) + start * 0x1c;
		((F8)g_m[E_INSTTANGENT])(decl[k].type, (u32)decl[k].offset + P32(locked[s]), U32AT(mt + 0x14), U32AT(mt + 0x18), 0, U32AT(geom + numTex * 4 + 0x30), P32(hdr),
			U32AT(hdr + s * 0x10 + 0x18));
	}
	for(i = 0; i < numStreams; i++)
		if(locked[i]) vbUnlock(*(void **)(hdr + (i + 1) * 0x10));
	return 1;
}

}
