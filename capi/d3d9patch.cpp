// D3D9PATCH (group d3d9patch, lane K2 window 6): the D3D9 patch atomic pipeline node of MFL.exe (rppatch noded3d9patchatomicinstance bucket), written op for op from
// the disassembly, on the exe's own engine, resource arena list, patch plugin offset and callees:
//   _rpD3D9PatchInstanceAtomic 0x55a750, rwD3D9PatchAtomicAllInOneNodeNode 0x55b060
// The two bodies are different code (0x55a750 = control point gather + refine calls per patch; 0x55b060 = the pipeline node around it), so two kernels.
// Not here: the refiners / index generators (quad: group bezpat; tri: trap rows) and rpPatchReInstanceAtomic 0x55d760 (trap row): called through the bind table.
// Exe binding: mflrw_d3d9patch_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_d3d9patch_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_REFTRIVEC, E_REFTRICOL, E_REFTRITEX, E_GENTRIIDX, E_REFQUADVEC, E_REFQUADCOL, E_REFQUADTEX, E_GENQUADIDX,
         E_PATCHOFF, E_IDX32, E_RESMODULE,
         E_FREERESENTRY, E_CREATEDECL, E_CREATEVB, E_DESTROYVB, E_PATCHINSTANCE, E_REINSTANCE, E_ALLOCRESENTRY, E_INSTDATADESTROY, E_IB32CREATE, E_IBCREATE,
         E_ATLIGHTCB, E_FRAMEGETLTM, E_SETXFORMWORLD, E_GETRS, E_GETCAPS, E_SETRS,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] d3d9patch called before mflrw_d3d9patch_bind\n"); abort(); }
	return *g_engine;
}
#define U32AT(p) (*(u32 *)(p))
#define U16AT(p) (*(u16 *)(p))
#define P(p) ((u32)(uintptr_t)(p))
typedef u32 (__cdecl *F0)(void);
typedef u32 (__cdecl *F1)(u32);
typedef u32 (__cdecl *F2)(u32, u32);
typedef u32 (__cdecl *F3)(u32, u32, u32);
typedef u32 (__cdecl *F4)(u32, u32, u32, u32);
typedef int32_t (__stdcall *LockFn)(void *self, u32 offset, u32 size, void *out, u32 flags);
typedef int32_t (__stdcall *UnlockFn)(void *self);
#define VT(obj, slot) ((*(void ***)(obj))[slot])

union V4 { u32 u[4]; float f[4]; };      // one control point slot of the 4 x 4 block the refiners read (16 bytes)

// the 10 control points of a tri patch as the lower triangle of the 4 x 4 block: column i takes 4 - i points (slots i, i + 4, ...), the rest of the column is zeroed
static void triVector(V4 *ctrl, const u16 *pi, const u8 *src){
	u32 k = 0;
	for(int i = 0; i < 4; i++){
		int j = 0;
		for(; j < 4 - i; j++){
			const u32 *v = (const u32 *)(src + (u32)pi[k++] * 12);
			V4 *e = &ctrl[i + 4 * j];
			e->u[0] = v[0]; e->u[1] = v[1]; e->u[2] = v[2];
		}
		for(; j < 4; j++){ V4 *e = &ctrl[i + 4 * j]; e->u[0] = 0; e->u[1] = 0; e->u[2] = 0; }
	}
}
// the same for the prelit colours: bytes 0 1 2 3 of the RwRGBA go to floats 2 1 0 3
static void triColor(V4 *ctrl, const u16 *pi, const u8 *src){
	u32 k = 0;
	for(int i = 0; i < 4; i++){
		int j = 0;
		for(; j < 4 - i; j++){
			const u8 *c = src + (u32)pi[k++] * 4;
			V4 *e = &ctrl[i + 4 * j];
			e->f[2] = (float)c[0]; e->f[1] = (float)c[1]; e->f[0] = (float)c[2]; e->f[3] = (float)c[3];
		}
		for(; j < 4; j++){ V4 *e = &ctrl[i + 4 * j]; e->u[0] = 0; e->u[1] = 0; e->u[2] = 0; e->u[3] = 0; }
	}
}
// the 16 control points of a quad patch, transposed: slot i * 4 + j takes index i + 4 * j
static void quadVector(V4 *ctrl, const u16 *pi, const u8 *src){
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++){
			const u32 *v = (const u32 *)(src + (u32)pi[i + 4 * j] * 12);
			V4 *e = &ctrl[i * 4 + j];
			e->u[0] = v[0]; e->u[1] = v[1]; e->u[2] = v[2];
		}
}
static void quadColor(V4 *ctrl, const u16 *pi, const u8 *src){
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++){
			const u8 *c = src + (u32)pi[i + 4 * j] * 4;
			V4 *e = &ctrl[i * 4 + j];
			e->f[2] = (float)c[0]; e->f[1] = (float)c[1]; e->f[0] = (float)c[2]; e->f[3] = (float)c[3];
		}
}
// first line of a new instanced mesh {indices, numIndices, minVert, numVerts, material, mesh}
static void meshOpen(u8 *im, u16 *iout, u8 *mesh, u32 base){
	U32AT(im) = P(iout);
	U32AT(im + 0x14) = P(mesh);
	U32AT(im + 0x10) = U32AT(mesh + 8);
	U32AT(im + 4) = 0;
	U32AT(im + 8) = base;
	U32AT(im + 0xc) = 0;
}

extern "C" {

void __cdecl mflrw_d3d9patch_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// _rpD3D9PatchInstanceAtomic 0x55a750 (inst, geometry, res): fills the locked vertex buffer (inst +0x10, stride +0xc) and the index list (inst +4) of the instance block
// from the geometry's patch meshes (mesh header +0x54: u16 numMeshes +4, meshes {indices, numIndices, material} at header + 0x10 + firstMeshOffset; 16 indices per patch).
// A patch is a tri patch when the geometry has texture coordinate sets and u of set 0 at control index 5 is not 0.0, else a quad patch.  Per patch: positions, then
// normals (inst flags +8 bit 2; skipped but counted when the morph target has none), prelit colours (bit 4, same rule), min(numTexCoordSets, flags byte 2) texture
// coordinate sets, then the strip indices; consecutive patches of a mesh are joined by 2 degenerate indices.  A mesh whose index count would pass 0xffff starts a new
// instanced mesh while the block has room (count < inst +0x14).  A mesh without indices is dropped.  inst +0x14 = number of instanced meshes.  Returns 1.
int32_t __cdecl rw37_rpD3D9PatchInstanceAtomic(u8 *inst, u8 *geom, int32_t res){
	eng();
	V4 ctrl[16];
	u32 tc[4][2];
	u8 *mt = *(u8 **)(geom + 0x5c);
	u8 *verts = *(u8 **)(mt + 0x14), *normals = *(u8 **)(mt + 0x18);
	u8 *mh = *(u8 **)(geom + 0x54);
	u8 *prelit = *(u8 **)(geom + 0x30);
	u32 meshesLeft = U16AT(mh + 4);
	u8 *mesh = mh + U32AT(mh + 0xc) + 0x10;
	u32 stride = U32AT(inst + 0xc);
	u32 flags = U32AT(inst + 8);
	u8 *im = inst + 0x24;
	u16 *iout = *(u16 **)(inst + 4);
	u8 *vout = *(u8 **)(inst + 0x10);
	int32_t ntc = (int32_t)((flags >> 16) & 0xff);
	u32 r = (u32)res;
	u32 quadIdx = (r + 1) * r * 2;
	u32 quadVerts = (r + 1) * (r + 1);
	u32 triIdx = (r + 2) * r;
	u32 triVerts = (u32)((int32_t)((r + 2) * (r + 1)) / 2);
	int32_t nInst = (int32_t)meshesLeft;
	u32 base = 0;
	int32_t nsets = *(int32_t *)(geom + 0x1c);
	if(r & 1) triIdx++;
	if(meshesLeft == 0){ U32AT(inst + 0x14) = (u32)nInst; return 1; }
	do{
		u32 meshBase = 0;
		meshOpen(im, iout, mesh, base);
		int32_t left = *(int32_t *)(mesh + 4);
		if(left == 0) nInst--;
		else{
			const u16 *pi = *(const u16 **)mesh;
			if(left > 0){
				int first = 1;
				do{
					int tri = 0;
					if(nsets != 0){
						float u = *(float *)(*(u8 **)(geom + 0x34) + (u32)pi[5] * 8);
						if(!(u == 0.0f)) tri = 1;
					}
					if(U32AT(im + 4) + (tri ? triIdx : quadIdx) > 0xffff && nInst < *(int32_t *)(inst + 0x14)){
						im += 0x18;
						meshBase = 0;
						U32AT(im) = P(iout);
						U32AT(im + 0x14) = P(mesh);
						U32AT(im + 8) = base;
						U32AT(im + 0x10) = U32AT(mesh + 8);
						U32AT(im + 4) = 0;
						U32AT(im + 0xc) = 0;
						nInst++;
					}else if(!first){
						iout[0] = iout[-1];
						iout[1] = (u16)meshBase;
						iout += 2;
						U32AT(im + 4) += 2;
					}
					u8 *cur = vout + 0xc;
					u32 nv, ni;
					if(tri){
						triVector(ctrl, pi, verts);
						((F4)g_m[E_REFTRIVEC])(P(vout), stride, P(ctrl), r);
						if(flags & 2){
							if(normals){
								triVector(ctrl, pi, normals);
								((F4)g_m[E_REFTRIVEC])(P(cur), stride, P(ctrl), r);
							}
							cur += 0xc;
						}
						if(flags & 4){
							if(prelit){
								triColor(ctrl, pi, prelit);
								((F4)g_m[E_REFTRICOL])(P(cur), stride, P(ctrl), r);
							}
							cur += 4;
						}
						for(int32_t k = 0; k < nsets && k < ntc; k++){
							const u32 *tex = *(const u32 **)(geom + 0x34 + 4 * k);
							tc[0][0] = tex[(u32)pi[0] * 2]; tc[0][1] = tex[(u32)pi[0] * 2 + 1];
							tc[1][0] = tex[(u32)pi[9] * 2]; tc[1][1] = tex[(u32)pi[9] * 2 + 1];
							tc[2][0] = tex[(u32)pi[3] * 2]; tc[2][1] = tex[(u32)pi[3] * 2 + 1];
							((F4)g_m[E_REFTRITEX])(P(cur), stride, P(tc), r);
							cur += 8;
						}
						((F3)g_m[E_GENTRIIDX])(P(iout), meshBase, r);
						nv = triVerts; ni = triIdx;
					}else{
						quadVector(ctrl, pi, verts);
						((F4)g_m[E_REFQUADVEC])(P(vout), stride, P(ctrl), r);
						if(flags & 2){
							if(normals){
								quadVector(ctrl, pi, normals);
								((F4)g_m[E_REFQUADVEC])(P(cur), stride, P(ctrl), r);
							}
							cur += 0xc;
						}
						if(flags & 4){
							if(prelit){
								quadColor(ctrl, pi, prelit);
								((F4)g_m[E_REFQUADCOL])(P(cur), stride, P(ctrl), r);
							}
							cur += 4;
						}
						for(int32_t k = 0; k < nsets && k < ntc; k++){
							const u32 *tex = *(const u32 **)(geom + 0x34 + 4 * k);
							tc[0][0] = tex[(u32)pi[0] * 2]; tc[0][1] = tex[(u32)pi[0] * 2 + 1];
							tc[1][0] = tex[(u32)pi[3] * 2]; tc[1][1] = tex[(u32)pi[3] * 2 + 1];
							tc[2][0] = tex[(u32)pi[12] * 2]; tc[2][1] = tex[(u32)pi[12] * 2 + 1];
							tc[3][0] = tex[(u32)pi[15] * 2]; tc[3][1] = tex[(u32)pi[15] * 2 + 1];
							((F4)g_m[E_REFQUADTEX])(P(cur), stride, P(tc), r);
							cur += 8;
						}
						((F3)g_m[E_GENQUADIDX])(P(iout), meshBase, r);
						nv = quadVerts; ni = quadIdx;
					}
					meshBase += nv;
					vout += nv * stride;
					iout += ni;
					U32AT(im + 0xc) += nv;
					U32AT(im + 4) += ni;
					base += nv;
					left -= 0x10;
					pi += 0x10;
					first = 0;
				}while(left > 0);
			}
			im += 0x18;
		}
		mesh += 0xc;
	}while(--meshesLeft);
	U32AT(inst + 0x14) = (u32)nInst;
	return 1;
}

// the used resource list of the engine's resources module: unlink the entry and put it at the front (same instruction sequence as in the noded3d9 nodes)
static void resEntryToFront(u8 *res){
	U32AT(*(u8 **)(res + 4)) = U32AT(res);
	U32AT(*(u8 **)res + 4) = U32AT(res + 4);
	u32 mod = *(u32 *)g_m[E_RESMODULE];
	U32AT(res) = U32AT(*(u8 **)(eng() + mod + 0x24));
	U32AT(res + 4) = U32AT(eng() + mod + 0x24);
	U32AT(*(u8 **)*(u8 **)(eng() + mod + 0x24) + 4) = P(res);
	U32AT(*(u8 **)(eng() + mod + 0x24)) = P(res);
}
// squared length of a matrix row as the original computes it (x87 register, stored as float), outside [0.9, 1.1] by the signed compare of the float's bits
static int rowNotUnit(const float *r){
	volatile float len = (float)((double)r[0] * (double)r[0] + (double)r[1] * (double)r[1] + (double)r[2] * (double)r[2]);
	union { float f; int32_t i; } u;
	u.f = len;
	return u.i > 0x3f8ccccd || u.i < 0x3f666666;
}
// strip index count of a patch mesh with nq quad and nt tri patches at this refinement
static u32 patchIndexCount(u32 nq, u32 nt, u32 r){
	u32 n = (nt * (r + 2) + nq * (r + 1) * 2) * r + (nt + nq) * 2 - 2;
	if(r & 1) n += nt;
	return n;
}

// rwD3D9PatchAtomicAllInOneNodeNode 0x55b060 (node, params): params[0] = atomic, geometry at +0x18, patch plugin data at atomic + offset {LOD callback, user data,
// patch mesh (numTriPatches +0x34, numQuadPatches +0x38, flags +0x2c)}.  Nothing to do (1) without vertices or meshes.  res = LOD callback (atomic, user data).  The
// resource entry (geometry +0x58 for one morph target, else atomic +0x14) is kept when its serial (+0x18) is the mesh header's (u16 +6) and its vertex count (+0x54)
// is the one of this refinement: a locked geometry (lock byte +0xc & 6) is reinstanced into the entry's vertex buffer (rpPatchReInstanceAtomic; 0 = failure, returns
// 0) and the entry moves to the front of the used list.  Any other entry is freed and the atomic instanced: instance block (engine malloc), vertex declaration
// (position, normal for flag 2, colour for flag 4, flags byte 2 texture coordinate sets), vertex buffer (failure: returns 0), _rpD3D9PatchInstanceAtomic into the locked
// buffer (0 = failure: buffer destroyed, returns 0; a failed Lock skips it), a new resource entry with the header (serial, mesh count, 16 or 32 bit index buffer, strip
// primitive type, stream 0, declaration, vertex count) and one instance record per mesh (vertex alpha from the prelit alphas of the mesh's control points); the lock
// word is cleared; no entry: returns 0.  Then lighting callback, world transform = frame LTM, render state 0x8f (normalize normals) = 1 when render state 0x89 is on
// and the device caps (+0x1c) have 0x10000, or the LTM is not flagged 0x20001 and one of its 3 rows has a squared length outside [0.9, 1.1]; render callback (node
// callbacks +0xc) (resEntry, atomic, 1, geometry flags).  Returns 1.
int32_t __cdecl rw37_rwD3D9PatchAtomicAllInOneNodeNode(u8 *node, u8 **params){
	u8 *cbs = *(u8 **)(node + 0x14);
	u8 *atomic = params[0];
	u32 po = *(u32 *)g_m[E_PATCHOFF];
	u8 *geom = *(u8 **)(atomic + 0x18);
	u32 gflags = U32AT(geom + 8);
	if(U32AT(geom + 0x14) == 0) return 1;
	u8 *mh = *(u8 **)(geom + 0x54);
	u32 numMeshes = U16AT(mh + 4);
	if(numMeshes == 0) return 1;
	u8 *pm = *(u8 **)(atomic + po + 8);
	u32 nq = U32AT(pm + 0x38), nt = U32AT(pm + 0x34);
	u32 r = (*(F2 *)(atomic + po))(P(atomic), U32AT(atomic + po + 4));
	u32 quadVerts = (r + 1) * (r + 1);
	u32 triVerts = (u32)((int32_t)((r + 1) * (r + 2)) / 2);
	u32 numVerts = quadVerts * nq + triVerts * nt;
	u8 *re = U32AT(geom + 0x18) == 1 ? *(u8 **)(geom + 0x58) : *(u8 **)(atomic + 0x14);
	if(re && U32AT(re + 0x18) == (u32)U16AT(mh + 6) && numVerts == U32AT(re + 0x54)){
		if(geom[0xc] & 6){
			u32 numIndices = patchIndexCount(nq, nt, r);
			u8 *inst = (u8 *)(uintptr_t)(*(F2 *)(eng() + 0x108))(numMeshes * 24 + 0x3c, 0x10123);
			U32AT(inst + 4) = 0;
			U32AT(inst + 0x14) = numMeshes;
			U32AT(inst + 0x18) = numIndices;
			U32AT(inst + 0x1c) = numVerts;
			U32AT(inst + 8) = U32AT(pm + 0x2c);
			U32AT(inst + 0xc) = U32AT(re + 0x30);
			void *vb = *(void **)(re + 0x28);
			if(((LockFn)VT(vb, 11))(vb, U32AT(re + 0x2c), U32AT(re + 0x30) * numVerts, inst + 0x10, 0) >= 0){
				if(!((F3)g_m[E_REINSTANCE])(P(inst), P(geom), r)){
					vb = *(void **)(re + 0x28);
					((UnlockFn)VT(vb, 12))(vb);
					(*(F1 *)(eng() + 0x10c))(P(inst));
					return 0;
				}
				vb = *(void **)(re + 0x28);
				((UnlockFn)VT(vb, 12))(vb);
			}
			(*(F1 *)(eng() + 0x10c))(P(inst));
			U16AT(geom + 0xc) = 0;
		}
		if(U32AT(re)) resEntryToFront(re);
	}else{
		if(re) ((F1)g_m[E_FREERESENTRY])(P(re));
		nq = U32AT(pm + 0x38); nt = U32AT(pm + 0x34);
		numVerts = nq * quadVerts + nt * triVerts;
		u32 numIndices = patchIndexCount(nq, nt, r);
		mh = *(u8 **)(geom + 0x54);
		u32 maxMeshes = (u32)U16AT(mh + 4) * (numIndices / 0xffffu + 1);
		u8 *inst = (u8 *)(uintptr_t)(*(F2 *)(eng() + 0x108))((numIndices + maxMeshes * 12) * 2 + 0x3c, 0x10123);
		U32AT(inst + 0x18) = numIndices;
		U32AT(inst + 4) = P(inst + maxMeshes * 24 + 0x24);
		U32AT(inst + 0x14) = maxMeshes;
		U32AT(inst + 0x1c) = numVerts;
		U32AT(inst + 8) = U32AT(pm + 0x2c);
		// the vertex declaration: D3DVERTEXELEMENT9 {u16 stream, u16 offset, u8 type, u8 method, u8 usage, u8 usageIndex}
		u8 el[8 * 40];
		u32 stride = 0xc, n = 1, vb = 0, vbOffset = 0;
		u32 decl = quadVerts;      // the original's slot still holds (res + 1)^2 when RwD3D9CreateVertexDeclaration leaves it unwritten
		union { u32 w; struct { u16 geomFlags; u8 managed, dynamicLock; } s; } sf;
		sf.w = 0;
		sf.s.geomFlags = 2;
		U16AT(el) = 0; U16AT(el + 2) = 0; el[4] = 2; el[5] = 0; el[6] = 0; el[7] = 0;
		if(inst[8] & 2){
			U16AT(el + 8) = 0; U16AT(el + 10) = 0xc; el[12] = 2; el[13] = 0; el[14] = 3; el[15] = 0;
			n = 2; stride = 0x18; sf.s.geomFlags = 6;
		}
		if(inst[8] & 4){
			u8 *e = el + 8 * n;
			U16AT(e) = 0; U16AT(e + 2) = (u16)stride; e[4] = 4; e[5] = 0; e[6] = 0xa; e[7] = 0;
			n++; stride += 4; sf.s.geomFlags |= 8;
		}
		for(u32 i = 0, ntc = inst[0xa]; i < ntc; i++){
			u8 *e = el + 8 * n;
			U16AT(e) = 0; U16AT(e + 2) = (u16)stride; e[4] = 1; e[5] = 0; e[6] = 5; e[7] = (u8)i;
			stride += 8; n++; sf.s.geomFlags |= (u16)(0x10u << (i & 31));
		}
		{
			u8 *e = el + 8 * n;
			U16AT(e) = 0xff; U16AT(e + 2) = 0; e[4] = 0x11; e[5] = 0; e[6] = 0; e[7] = 0;
		}
		((F2)g_m[E_CREATEDECL])(P(el), P(&decl));
		sf.s.managed = 1; sf.s.dynamicLock = 0;
		if(!((F4)g_m[E_CREATEVB])(stride, stride * numVerts, P(&vb), P(&vbOffset))){
			(*(F1 *)(eng() + 0x10c))(P(inst));
			return 0;
		}
		U32AT(inst + 0xc) = stride;
		if(((LockFn)VT((void *)(uintptr_t)vb, 11))((void *)(uintptr_t)vb, vbOffset, stride * numVerts, inst + 0x10, 0) >= 0){
			if(!((F3)g_m[E_PATCHINSTANCE])(P(inst), P(geom), r)){
				((F4)g_m[E_DESTROYVB])(stride, stride * numVerts, vb, vbOffset);
				(*(F1 *)(eng() + 0x10c))(P(inst));
				return 0;
			}
			((UnlockFn)VT((void *)(uintptr_t)vb, 12))((void *)(uintptr_t)vb);
		}
		u32 nm = U32AT(inst + 0x14);
		u32 size = nm * 36 + 0x40;
		if(U32AT(geom + 0x18) == 1) re = (u8 *)(uintptr_t)((F4)g_m[E_ALLOCRESENTRY])(P(geom), P(geom + 0x58), size, P(g_m[E_INSTDATADESTROY]));
		else re = (u8 *)(uintptr_t)((F4)g_m[E_ALLOCRESENTRY])(P(atomic), P(atomic + 0x14), size, P(g_m[E_INSTDATADESTROY]));
		u8 *hdr = re + 0x18;
		memset(hdr, 0, size);
		U32AT(hdr + 4) = nm;
		U32AT(hdr) = U16AT(mh + 6);
		U32AT(hdr + 0x3c) = numVerts;
		u16 *idx16 = 0;
		u32 *idx32 = 0;
		U32AT(hdr + 8) = 0;
		U32AT(hdr + 0x38) = 0;
		if(!(mh[1] & 1)){
			u8 *im = inst + 0x28;
			for(u32 k = nm; k; k--){ U32AT(hdr + 0x38) += U32AT(im); im += 0x18; }
			nm = U32AT(hdr + 4);
			if(U32AT(hdr + 0x38) != 0){
				if(*(u32 *)g_m[E_IDX32] != 0 && numVerts > 0xffff && ((F2)g_m[E_IB32CREATE])(U32AT(hdr + 0x38), P(hdr + 8))){
					void *ib = *(void **)(hdr + 8);
					((LockFn)VT(ib, 11))(ib, 0, 0, &idx32, 0);
				}
				if(!idx32 && ((F2)g_m[E_IBCREATE])(U32AT(hdr + 0x38), P(hdr + 8))){
					void *ib = *(void **)(hdr + 8);
					((LockFn)VT(ib, 11))(ib, 0, 0, &idx16, 0);
				}
			}
		}
		U32AT(hdr + 0xc) = 5;
		U32AT(hdr + 0x30) = 0;
		U32AT(hdr + 0x10) = vb;
		U32AT(hdr + 0x14) = vbOffset;
		U32AT(hdr + 0x18) = stride;
		U16AT(hdr + 0x1c) = sf.s.geomFlags;
		hdr[0x1e] = sf.s.managed;
		hdr[0x1f] = sf.s.dynamicLock;
		U32AT(hdr + 0x20) = 0; U32AT(hdr + 0x24) = 0; U32AT(hdr + 0x28) = 0; U16AT(hdr + 0x2c) = 0; hdr[0x2e] = 0; hdr[0x2f] = 0;
		U32AT(hdr + 0x34) = decl;
		u8 *id = hdr + 0x40;
		u8 *im = inst + 0x24;
		u32 start = 0;
		for(u32 k = nm; k; k--, id += 0x24, im += 0x18){
			U32AT(id + 4) = U32AT(im + 8);
			U32AT(id + 0x18) = U32AT(im + 0xc);
			U32AT(id + 0x20) = U32AT(im + 4) - 2;
			U32AT(id + 8) = U32AT(im + 0x10);
			U32AT(id + 0x10) = 0;
			U32AT(id + 0x14) = vbOffset / stride + U32AT(im + 8);
			const u8 *prelit;
			if((inst[8] & 4) && (prelit = *(const u8 **)(geom + 0x30)) != 0){
				const u8 *mesh = *(const u8 **)(im + 0x14);
				const u16 *pi = *(const u16 **)mesh;
				u32 alpha = 0xff;
				for(u32 c = U32AT(mesh + 4); c; c--) alpha &= prelit[(u32)*pi++ * 4 + 3];
				U32AT(id + 0xc) = alpha != 0xff;
			}else U32AT(id + 0xc) = 0;
			if(!idx16 && !idx32){ U32AT(id) = 0; U32AT(id + 0x1c) = 0; continue; }
			U32AT(id) = U32AT(im + 4);
			U32AT(id + 0x1c) = start;
			if(idx32){
				for(u32 i = 0; i < U32AT(im + 4); i++) *idx32++ = (*(const u16 **)im)[i];
			}else{
				memcpy(idx16, *(const void **)im, U32AT(im + 4) * 2);
				idx16 += U32AT(im + 4);
			}
			start += U32AT(im + 4);
		}
		if(idx16 || idx32){
			void *ib = *(void **)(hdr + 8);
			((UnlockFn)VT(ib, 12))(ib);
		}
		(*(F1 *)(eng() + 0x10c))(P(inst));
		U16AT(geom + 0xc) = 0;
		if(!re) return 0;
	}
	((F1)g_m[E_ATLIGHTCB])(P(atomic));
	u8 *ltm = (u8 *)(uintptr_t)((F1)g_m[E_FRAMEGETLTM])(U32AT(atomic + 4));
	((F1)g_m[E_SETXFORMWORLD])(P(ltm));
	u32 normalize = 0, on = 0;
	((F2)g_m[E_GETRS])(0x89, P(&on));
	if(on){
		if(U32AT((u8 *)(uintptr_t)((F0)g_m[E_GETCAPS])() + 0x1c) & 0x10000) normalize = 1;
		else if(!(U32AT(ltm + 0xc) & 0x20001))
			normalize = rowNotUnit((float *)ltm) || rowNotUnit((float *)(ltm + 0x10)) || rowNotUnit((float *)(ltm + 0x20));
	}
	((F2)g_m[E_SETRS])(0x8f, normalize);
	if(U32AT(cbs + 0xc)) (*(F4 *)(cbs + 0xc))(P(re), P(atomic), 1, gflags);
	return 1;
}

}
