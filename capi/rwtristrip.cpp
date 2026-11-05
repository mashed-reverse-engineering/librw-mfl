// TRISTRIP (LIB_RW group tristrip; output/rw_next_family_2026-11-05.md row 7; not src/tristrip.cpp, librw's own stripper): the RpBuildMesh tri-strip / tri-list mesh builders of MFL.exe, written op for op
// from its disassembly (integer only):
//   TriStripMeshGenerate 0x5021e0, TriStripStripTris 0x5027a0, TriStripBinEntryArrayCreate 0x5030a0, TriStripAddEdge 0x5031c0,
//   TriStripMarkTriUsed 0x503260, TriStripFollow 0x503380, TriStripJoin 0x503670, _rpTriListMeshGenerate 0x5038f0,
//   plus the two unnamed statics behind them: the triangle sort comparator 0x502650 and the tri-list reorder callback 0x503af0.
// Every allocation goes through the exe's engine (+0x108 malloc, +0x10c free, +0x118 / +0x11c free-list alloc / free) and the exe's
// RwFreeListCreate / Destroy, in the original's order with the original's hints, so the heap ends up the same.  Sorting uses the exe's
// CRT qsort (not stable: the same algorithm is needed for the same order of equal keys).
// Exe-layout adapter: mflrw_tristrip_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance 0x7d2128,
// &meshModule offset 0x947778 (module data: u16 serial number, RwFreeList *strip free list), RwFreeListCreate 0x4e90b0,
// RwFreeListDestroy 0x4e92b0, _rpMeshHeaderCreate 0x500ba0, _rpMeshHeaderForAllMeshes 0x500fb0, qsort 0x5d38b0, RwErrorSet 0x4ea160.
// Check: tools/delink/rw_tristrip_check.sh (the original code and these exports natively under Wine, same harness heap).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// RpBuildMeshTriangle / RpBuildMesh (RW 3.7)
struct BuildTri { u16 v[3]; u16 pad; void *material; u16 matIndex, texIndex, rasIndex, pipeIndex; };
struct BuildMesh { u32 maxTriangles; u32 numTriangles; BuildTri *tris; };
struct Edge;
// per-triangle entry (free list of 0x24): adjacency count bins are doubly linked lists
struct TriEntry { u32 index; Edge *edge[3]; TriEntry *binNext, *binPrev; u32 used, usedTemp; u8 adj; };
// shared edge (free list of 0x10): one list for the whole mesh
struct Edge { u16 v0, v1; TriEntry *tri0, *tri1; Edge *next; };
// strip under construction (the module's free list of 0x10)
struct Strip { u16 *indices; u32 count, cap; Strip *next; };
// RpMesh / RpMeshHeader
struct Mesh { u16 *indices; u32 numIndices; void *material; };
struct MeshHeader { u32 flags; u16 numMeshes, serial; u32 totalIndices, firstMeshOffset; };
// one material run of the sorted triangles / one finished strip (TriStripMeshGenerate)
struct MatRun { u32 unused, count, material; };
struct OutMesh { u16 *indices; u32 count; void *material; };
typedef char sz1[sizeof(BuildTri) == 0x14 ? 1 : -1];
typedef char sz2[sizeof(TriEntry) == 0x24 ? 1 : -1];

typedef void *(__cdecl *MallocFn)(u32 size, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *FlAllocFn)(void *fl, u32 hint);
typedef void *(__cdecl *FlFreeFn)(void *fl, void *p);
typedef void *(__cdecl *FlCreateFn)(u32 entrySize, u32 entriesPerBlock, u32 align, u32 hint);
typedef void (__cdecl *FlDestroyFn)(void *fl);
typedef MeshHeader *(__cdecl *HdrCreateFn)(u32 size);
typedef Mesh *(__cdecl *MeshCb)(Mesh *mesh, MeshHeader *hdr, void *data);
typedef MeshHeader *(__cdecl *ForAllFn)(MeshHeader *hdr, MeshCb cb, void *data);
typedef int (__cdecl *CmpFn)(const void *, const void *);
typedef void (__cdecl *QsortFn)(void *base, u32 n, u32 size, CmpFn cmp);
typedef void *(__cdecl *ErrorSetFn)(int32_t *err);

// ---- adapter
static u8 **g_engine;			// &RwEngineInstance 0x7d2128
static int32_t *g_meshModule;		// 0x947778: offset of the mesh module data in the engine
static FlCreateFn g_flCreate;		// RwFreeListCreate 0x4e90b0
static FlDestroyFn g_flDestroy;		// RwFreeListDestroy 0x4e92b0
static HdrCreateFn g_hdrCreate;		// _rpMeshHeaderCreate 0x500ba0
static ForAllFn g_forAll;		// _rpMeshHeaderForAllMeshes 0x500fb0
static QsortFn g_qsort;			// qsort 0x5d38b0
static ErrorSetFn g_errorSet;		// RwErrorSet 0x4ea160

static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] tristrip called before mflrw_tristrip_bind\n"); abort(); }
	return *g_engine;
}
static void *rwMalloc(u32 n, u32 hint){ return (*(MallocFn *)(eng() + 0x108))(n, hint); }
static void rwFree(void *p){ (*(FreeFn *)(eng() + 0x10c))(p); }
static void *flAlloc(void *fl, u32 hint){ return (*(FlAllocFn *)(eng() + 0x118))(fl, hint); }
static void flFree(void *fl, void *p){ (*(FlFreeFn *)(eng() + 0x11c))(fl, p); }
static u16 &modSerial(void){ return *(u16 *)(eng() + *g_meshModule); }
static void *&modStripFL(void){ return *(void **)(eng() + *g_meshModule + 4); }

// an unused triangle on an edge: `used` is the committed mark, `usedTemp` the one of the trial passes (pass < 4)
static inline int freeTri(TriEntry *t){ return t && t->used == 0; }
static inline int freeTriP(TriEntry *t, int pass){ return t && (pass < 4 ? t->usedTemp : t->used) == 0; }
static inline int freeOn(Edge *e, int pass){ return freeTriP(e->tri0, pass) + freeTriP(e->tri1, pass); }

extern "C" {

void __cdecl mflrw_tristrip_bind(char **engine, int32_t *meshModule, void *flCreate, void *flDestroy, void *hdrCreate, void *forAll, void *qsortFn, void *errorSet){
	g_engine = (u8 **)engine; g_meshModule = meshModule; g_flCreate = (FlCreateFn)flCreate; g_flDestroy = (FlDestroyFn)flDestroy;
	g_hdrCreate = (HdrCreateFn)hdrCreate; g_forAll = (ForAllFn)forAll; g_qsort = (QsortFn)qsortFn; g_errorSet = (ErrorSetFn)errorSet;
}

// 0x502650: sort key of a triangle pointer: material pointer first (equal pointers compare equal), then a score per side: bit 0x10 when its
// material is translucent (raster format 1555 / 4444 / 8888 or colour alpha != 0xff), plus 8 / 4 / 2 / 1 when the OTHER side's raster / pipeline
// / texture / material index is smaller.  Result = score(a) - score(b).
static u32 matFlags(const u8 *mat){
	u32 f = 0;
	if(mat){
		const u8 *tex = *(u8 *const *)mat;
		if(tex){
			u32 fmt = (*(*(u8 *const *)tex + 0x23) & 0xf) << 8;
			if(fmt == 0x100 || fmt == 0x300 || fmt == 0x500) f = 0x10;
		}
		if(mat[7] != 0xff) f |= 0x10;
	}
	return f;
}
static int __cdecl cmpTris(const void *pa, const void *pb){
	const BuildTri *a = *(BuildTri *const *)pa, *b = *(BuildTri *const *)pb;
	if(a->material == b->material) return 0;
	u32 fa = matFlags((const u8 *)a->material), fb = matFlags((const u8 *)b->material);
	u32 sb = (a->rasIndex < b->rasIndex ? 8 : 0) | (a->pipeIndex < b->pipeIndex ? 4 : 0) | (a->texIndex < b->texIndex ? 2 : 0) | (a->matIndex < b->matIndex ? 1 : 0) | fb;
	u32 sa = (b->rasIndex < a->rasIndex ? 8 : 0) | (b->pipeIndex < a->pipeIndex ? 4 : 0) | (b->texIndex < a->texIndex ? 2 : 0) | (b->matIndex < a->matIndex ? 1 : 0) | fa;
	return (int)(sa - sb);
}

// 0x5031c0: the edge v0 -> v1 of triangle i: joins the reverse edge v1 -> v0 when one has no second triangle yet (both adjacency counts
// grow), else a new edge at the head of the list
Edge *__cdecl rw37_TriStripAddEdge(void *edgeFL, Edge **head, u32 v0_, u32 v1_, u32 i, TriEntry **entries){
	u16 v0 = (u16)v0_, v1 = (u16)v1_;
	for(Edge *e = *head; e; e = e->next)
		if(e->v1 == v0 && e->v0 == v1 && !e->tri1){
			e->tri0->adj++;
			entries[i]->adj++;
			e->tri1 = entries[i];
			return e;
		}
	Edge *e = (Edge *)flAlloc(edgeFL, 0x30502);
	e->v0 = v0; e->v1 = v1;
	e->tri0 = entries[i]; e->tri1 = 0;
	e->next = *head; *head = e;
	return e;
}

// 0x5030a0: one entry per triangle (free list fls[0] of 0x24 entries), its three edges (free list fls[1] of 0x10, list *head)
TriEntry **__cdecl rw37_TriStripBinEntryArrayCreate(u32 n, void **fls, Edge **head, BuildTri *tris){
	TriEntry **entries = (TriEntry **)rwMalloc(n*4, 0x30502);
	fls[0] = g_flCreate(0x24, n, 4, 0x30502);
	fls[1] = g_flCreate(0x10, (n >> 1) + 1, 4, 0x30502);
	for(u32 i = 0; i < n; i++){
		entries[i] = (TriEntry *)flAlloc(fls[0], 0x30502);
		entries[i]->adj = 0;
		entries[i]->index = i;
		entries[i]->binPrev = 0;
		entries[i]->binNext = 0;
		entries[i]->used = 0;
		const u16 *v = tris[i].v;
		entries[i]->edge[0] = rw37_TriStripAddEdge(fls[1], head, v[0], v[1], i, entries);
		entries[i]->edge[1] = rw37_TriStripAddEdge(fls[1], head, v[1], v[2], i, entries);
		entries[i]->edge[2] = rw37_TriStripAddEdge(fls[1], head, v[2], v[0], i, entries);
	}
	return entries;
}

static inline void binUnlink(TriEntry *t, TriEntry **bins){
	if(bins[t->adj] == t){
		bins[t->adj] = t->binNext;
		if(bins[t->adj]) bins[t->adj]->binPrev = 0;
	}else{
		if(t->binNext) t->binNext->binPrev = t->binPrev;
		if(t->binPrev) t->binPrev->binNext = t->binNext;
	}
}

// 0x503260: trial passes (pass < 4) only set usedTemp.  The committed pass marks the triangle, takes it out of its bin and moves every
// unused neighbour one bin down
void __cdecl rw37_TriStripMarkTriUsed(TriEntry *t, TriEntry **bins, int pass){
	if(pass < 4){ t->usedTemp = 1; return; }
	t->used = 1;
	binUnlink(t, bins);
	for(int k = 0; k < 3; k++){
		Edge *e = t->edge[k];
		TriEntry *n = e->tri0;
		if(!(n && n != t && n->used == 0)){
			n = e->tri1;
			if(!n || n->used) continue;
		}
		binUnlink(n, bins);
		n->adj--;
		n->binNext = bins[n->adj];
		if(bins[n->adj]) bins[n->adj]->binPrev = n;
		bins[n->adj] = n;
		n->binPrev = 0;
	}
}

// 0x503380: walk from edge e across unused triangles, appending one index per triangle (a swap index when the strip has to turn the other
// way); returns the number of triangles taken
u32 __cdecl rw37_TriStripFollow(Strip *s, Edge *e, TriEntry **bins, BuildTri *tris, int pass){
	u32 count = 0;
	Edge *alt = 0;
	u16 nv;
	if(!e) return 0;
	for(;;){
		TriEntry *t = e->tri0;
		if(!freeTriP(t, pass)){
			t = e->tri1;
			if(!freeTriP(t, pass)) return count;
		}
		count++;
		rw37_TriStripMarkTriUsed(t, bins, pass);
		Edge *e0 = t->edge[0];
		int k = -1;
		if(e == e0) k = 0; else if(e == t->edge[1]) k = 1; else if(e == t->edge[2]) k = 2;
		Edge *o = t->edge[(k + 1) % 3];
		nv = o->tri0 == t ? o->v1 : o->v0;
		Edge *prev = e;
		u16 *idx = s->indices;
		u32 cnt = s->count;
		u16 last = idx[cnt - 1];
		Edge *ne;
		if((e0->v0 == last && e0->v1 == nv) || (e0->v0 == nv && e0->v1 == last)) ne = e0;
		else if((t->edge[1]->v0 == last && t->edge[1]->v1 == nv) || (t->edge[1]->v0 == nv && t->edge[1]->v1 == last)) ne = t->edge[1];
		else if((t->edge[2]->v0 == last && t->edge[2]->v1 == nv) || (t->edge[2]->v0 == nv && t->edge[2]->v1 == last)) ne = t->edge[2];
		else ne = 0;
		if(ne && freeOn(ne, pass)){		// straight on
			idx[cnt] = nv;
			s->count++;
			e = ne;
			continue;
		}
		// turn: another edge of t (not the one we came over, not the blocked one); the last one found stays as fallback
		if(e0 != prev && e0 != ne) ne = alt = e0;
		else if(t->edge[1] != prev && t->edge[1] != ne) ne = alt = t->edge[1];
		else if(t->edge[2] != prev && t->edge[2] != ne) ne = alt = t->edge[2];
		else ne = alt;
		if(!ne || !freeOn(ne, pass) || (cnt & 1)) break;
		idx[cnt] = idx[cnt - 2];		// swap index
		s->count++;
		s->indices[s->count] = nv;
		s->count++;
		e = ne;
	}
	s->indices[s->count] = nv;
	s->count++;
	return count;
}

// start orientation of a strip from triangle T (0x5029b4..0x502c56), u_k = unused triangles on edge k (T itself counts: it is not used yet)
static u32 startSide(TriEntry *T){
	Edge **e = T->edge;
	int u0 = freeTri(e[0]->tri0) + freeTri(e[0]->tri1), u1 = freeTri(e[1]->tri0) + freeTri(e[1]->tri1), u2 = freeTri(e[2]->tri0) + freeTri(e[2]->tri1);
	if(u2 >= 2 && u1 >= 2) return 1;
	if(u0 >= 2 && u2 >= 2) return 2;
	if(u1 >= 2 && u0 >= 2) return 0;
	if(u0 > u1) return u0 > u2 ? 2 : 1;
	return u1 <= u2 ? 1 : 0;
}

// 0x5027a0: strips of n triangles (one material run) prepended to *listp.  Triangles are taken lowest adjacency first; tryAll != 0 tries
// three start orientations (trial passes 0..2 on usedTemp) before the committed pass 3 with the longest; tryAll == 0 commits at once.
// A is the strip forward from the start triangle, B the one backward over its first edge; both are joined into one (B reversed, then A).
u32 __cdecl rw37_TriStripStripTris(BuildTri *tris, u32 n, Strip **listp, u32 tryAll){
	Edge *edgeHead = 0;
	void *fls[2] = {0, 0};		// triangle entries, edges
	TriEntry *bins[4] = {0, 0, 0, 0};
	u32 processed = 0;
	Edge *firstEdge = 0, *nextEdge = 0;
	TriEntry **entries = rw37_TriStripBinEntryArrayCreate(n, fls, &edgeHead, tris);
	for(u32 i = 0; i < n; i++){
		TriEntry *t = entries[i];
		t->binNext = bins[t->adj];
		if(t->binNext) t->binNext->binPrev = t;
		bins[t->adj] = t;
		t->binPrev = 0;
	}
	Strip *A = (Strip *)flAlloc(modStripFL(), 0x10502);
	A->count = 0; A->cap = 2*n + 2;
	A->indices = (u16 *)rwMalloc(A->cap*4, 0x10502);
	Strip *B = (Strip *)flAlloc(modStripFL(), 0x10502);
	B->cap = 2*n + 2; B->count = 0;
	B->indices = (u16 *)rwMalloc(B->cap*4, 0x10502);
	if(n) do{
		int pass = tryAll ? 0 : 3;
		u32 bestLen = 0;
		if(bins[0]){		// isolated triangle: a strip of its own
			u32 i = bins[0]->index;
			Strip *s = (Strip *)flAlloc(modStripFL(), 0x30502);
			s->next = *listp; *listp = s;
			s->cap = 3; s->count = 3;
			s->indices = (u16 *)rwMalloc(0xc, 0x30502);
			s->indices[0] = tris[i].v[0]; s->indices[1] = tris[i].v[1]; s->indices[2] = tris[i].v[2];
			bins[0]->used = 1;
			bins[0]->usedTemp = 1;
			TriEntry *nx = bins[0]->binNext;
			bins[0] = nx;
			if(nx) nx->binPrev = 0;
			processed++;
			continue;
		}
		u32 k = 1;
		while(!bins[k]) k++;
		TriEntry **bk = &bins[k];
		u32 base = startSide(*bk), best = base, saved = processed;
		for(;;){
			for(u32 j = 0; j < n; j++) entries[j]->usedTemp = entries[j]->used;
			u32 p = (u32)pass++, side = p < 3 ? (base + p) % 3 : best;
			TriEntry *T = *bk;
			switch(side){
			case 0: nextEdge = T->edge[1]; firstEdge = T->edge[0]; break;
			case 1: nextEdge = T->edge[2]; firstEdge = T->edge[1]; break;
			case 2: nextEdge = T->edge[0]; firstEdge = T->edge[2]; break;
			}
			const u16 *v = tris[T->index].v;
			A->indices[0] = v[side % 3]; A->indices[1] = v[(side + 1) % 3]; A->indices[2] = v[(side + 2) % 3];
			A->count = 3;
			rw37_TriStripMarkTriUsed(T, bins, pass);
			u32 len = rw37_TriStripFollow(A, nextEdge, bins, tris, pass) + saved + 1;
			processed = len;
			int back = freeOn(firstEdge, pass) > 0;
			if(back){
				B->indices[0] = A->indices[1];
				B->indices[1] = A->indices[0];
				B->count = 2;
				len += rw37_TriStripFollow(B, firstEdge, bins, tris, pass);
				processed = len;
				if(B->count & 1){ B->indices[B->count] = B->indices[B->count - 2]; B->count++; }
			}
			if(len > bestLen){ bestLen = len; best = side; }
			if(pass < 4) continue;
			if(back){		// B reversed (without its two start indices), then A; A is copied as count*4 bytes like the original
				Strip *c = (Strip *)flAlloc(modStripFL(), 0x30502);
				c->next = *listp; *listp = c;
				c->count = 0;
				c->cap = A->count + B->count - 2;
				c->indices = (u16 *)rwMalloc(c->cap*4, 0x30502);
				while(B->count > 2){ c->indices[c->count] = B->indices[B->count - 1]; c->count++; B->count--; }
				memcpy(c->indices + c->count, A->indices, A->count*4);
				c->count = c->cap;
			}else{			// A alone
				Strip *c = (Strip *)flAlloc(modStripFL(), 0x30502);
				c->next = *listp; *listp = c;
				c->cap = c->count = A->count;
				c->indices = (u16 *)rwMalloc(A->count*4, 0x30502);
				memcpy(c->indices, A->indices, A->count*4);
			}
			break;
		}
	}while(processed < n);
	rwFree(B->indices); B->indices = 0; flFree(modStripFL(), B);
	rwFree(A->indices); A->indices = 0; flFree(modStripFL(), A);
	for(Edge *e = edgeHead; e; ){ Edge *nx = e->next; flFree(fls[1], e); e = nx; }
	g_flDestroy(fls[1]); fls[1] = 0;
	for(u32 i = 0; i < n; i++){ flFree(fls[0], entries[i]); entries[i] = 0; }
	g_flDestroy(fls[0]); fls[0] = 0;
	rwFree(entries);
	return 1;
}

// 0x503670: all strips of *listp into one (capacity sum(count + 6)), joined by repeated indices: a strip that starts with the last index
// (or the one before it) needs fewer; joinFlag keeps the winding (an extra index on an odd length)
u32 __cdecl rw37_TriStripJoin(Strip **listp, u32 joinFlag){
	if(!*listp) return 0;
	Strip *j = (Strip *)flAlloc(modStripFL(), 0x30502);
	j->count = 0; j->cap = 0;
	for(Strip *s = *listp; s; s = s->next) j->cap += s->count + 6;
	j->indices = (u16 *)rwMalloc(j->cap*4, 0x30502);
	Strip *s = *listp;
	for(u32 i = 0; i < s->count; i++) j->indices[j->count++] = s->indices[i];
	rwFree(s->indices);
	Strip *rest = s->next;
	s->indices = 0;
	flFree(modStripFL(), s);
	while(rest){
		u16 *idx = j->indices;
		u32 c = j->count;
		for(s = rest; s && s->indices[0] != idx[c - 1]; s = s->next) ;
		if(s){			// starts with the last index
			if((c & 1) && joinFlag){ idx[c] = s->indices[0]; j->count = c + 1; }
		}else{
			for(s = rest; s && s->indices[0] != idx[c - 2]; s = s->next) ;
			if(s){		// starts with the one before
				if(!(c & 1) && joinFlag){ idx[c] = s->indices[0]; idx[c + 1] = s->indices[0]; j->count += 2; }
				else{ idx[c] = s->indices[0]; j->count = c + 1; }
			}else{		// the next one in the list, over a degenerate join
				s = rest;
				if((c & 1) && joinFlag){
					idx[c] = idx[c - 1]; j->indices[j->count + 1] = s->indices[0]; j->indices[j->count + 2] = s->indices[0];
					j->count += 3;
				}else{
					idx[c] = idx[c - 1]; j->indices[j->count + 1] = s->indices[0];
					j->count += 2;
				}
			}
		}
		for(u32 i = 0; i < s->count; i++) j->indices[j->count++] = s->indices[i];
		rwFree(s->indices);
		s->indices = 0;
		if(rest == s) rest = rest->next;
		else{
			Strip *p = rest;
			while(p->next != s) p = p->next;
			p->next = s->next;
		}
		flFree(modStripFL(), s);
	}
	*listp = j;
	j->next = 0;
	return 1;
}

// 0x5021e0: RpBuildMesh -> tri-strip RpMeshHeader (flags 1): triangles sorted by material (cmpTris), each material run stripped and joined
// into one mesh per strip left after the join
MeshHeader *__cdecl rw37_TriStripMeshGenerate(BuildMesh *bm, u32 tryAll, u32 joinFlag){
	u32 nOut = 0;
	BuildTri **sorted = (BuildTri **)rwMalloc(bm->numTriangles*4, 0x10502);
	if(!sorted) return 0;
	for(u32 i = 0; i < bm->numTriangles; i++) sorted[i] = &bm->tris[i];
	g_qsort(sorted, bm->numTriangles, 4, cmpTris);
	u32 runs = 1;
	if(bm->numTriangles >= 2){
		void *m = sorted[0]->material;
		for(u32 i = 1; i < bm->numTriangles; i++) if(sorted[i]->material != m){ m = sorted[i]->material; runs++; }
	}
	OutMesh **meshes = (OutMesh **)rwMalloc(runs*4, 0x1010502);
	MatRun *run = (MatRun *)rwMalloc(runs*12, 0x10502);
	run[0].material = (u32)sorted[0]->material;
	run[0].unused = 0;
	run[0].count = 0;		// first triangle of the run, turned into the count when the run ends
	runs = 1;
	if(bm->numTriangles >= 2){
		MatRun *r = run;
		for(u32 i = 0; i < bm->numTriangles - 1; i++)
			if(sorted[i]->material != sorted[i + 1]->material){
				r[1].material = (u32)sorted[i + 1]->material;
				r[1].unused = 0;
				r[1].count = i + 1;
				r[0].count = i - r[0].count + 1;
				runs++;
				r++;
			}
	}
	run[runs - 1].count = bm->numTriangles - run[runs - 1].count;
	modStripFL() = g_flCreate(0x10, bm->numTriangles/10 + 5, 4, 0x10502);
	BuildTri **cur = sorted;
	Strip *list = 0;
	MatRun *r = run;
	for(u32 k = runs; k; k--, r++){
		BuildTri *tris = (BuildTri *)rwMalloc(r->count*20, 0x10502);
		for(u32 i = 0; i < r->count; i++) tris[i] = **cur++;
		rw37_TriStripStripTris(tris, r->count, &list, tryAll);
		rw37_TriStripJoin(&list, joinFlag);
		for(Strip *s = list; s; s = s->next){
			OutMesh *om = (OutMesh *)rwMalloc(s->count*2 + 0xc, 0x10502);
			om->material = (void *)r->material;
			om->count = s->count;
			om->indices = (u16 *)(om + 1);
			for(u32 i = 0; i < om->count; i++) om->indices[i] = s->indices[i];
			meshes[nOut & 0xffff] = om;
			nOut++;
		}
		while(list){
			Strip *s = list;
			list = s->next;
			rwFree(s->indices);
			s->indices = 0;
			flFree(modStripFL(), s);
		}
		rwFree(tris);
	}
	g_flDestroy(modStripFL());
	modStripFL() = 0;
	u32 nm = nOut & 0xffff, size = 0x10, total = 0;
	for(u32 i = 0; i < nm; i++){ total += meshes[i]->count; size += meshes[i]->count*2 + 0xc; }
	MeshHeader *h = g_hdrCreate(size);
	h->flags = 1;
	h->numMeshes = (u16)nOut;
	h->serial = modSerial();
	h->firstMeshOffset = 0;
	h->totalIndices = total;
	modSerial()++;
	Mesh *m = (Mesh *)(h + 1);
	u16 *dst = (u16 *)(m + nm);
	for(u32 i = 0; i < nm; i++, m++){
		m->indices = dst;
		m->numIndices = meshes[i]->count;
		m->material = meshes[i]->material;
		memcpy(dst, meshes[i]->indices, meshes[i]->count*2);
		dst += m->numIndices;
		rwFree(meshes[i]);
		meshes[i] = 0;
	}
	rwFree(sorted);
	rwFree(meshes);
	rwFree(run);
	return h;
}

static void errNoMem(u32 size){
	if(!g_errorSet){ fprintf(stderr, "[mflrw] tristrip called before mflrw_tristrip_bind\n"); abort(); }
	(void)size;			// _rwerror 0x53eac0 returns its first argument
	int32_t err[2] = {2, (int32_t)0x80000013};
	g_errorSet(err);
}

// 0x503af0: reorders a tri-list mesh so that consecutive triangles share no vertex as far as possible: repeated sweeps over the remaining
// triangles, each sweep takes a triangle when none of its vertices is in the sweep's bitmap (taken ones become 0xffff in the copy)
static Mesh *__cdecl triListReorder(Mesh *mesh, MeshHeader *, void *){
	u32 nTri = mesh->numIndices / 3, all = 0;
	u16 *dst = mesh->indices;
	for(u32 i = 0; i < mesh->numIndices; i++) all |= dst[i];
	u32 bmBytes = (all + 0x1f) >> 3;
	u32 *bm = (u32 *)rwMalloc(bmBytes, 0x10502);
	if(!bm) return mesh;
	u16 *copy = (u16 *)rwMalloc(mesh->numIndices*2, 0x10502);
	if(!copy){ rwFree(bm); return mesh; }
	memcpy(copy, mesh->indices, mesh->numIndices*2);
	u32 done = 0;
	if(nTri) do{
		if(bmBytes) memset(bm, 0, bmBytes);
		u16 *t = copy;
		for(u32 k = nTri; k; k--, t += 3){
			if((u16)(t[0] & t[2] & t[1]) == 0xffff) continue;
			u32 *w0 = &bm[t[0] >> 5], b0 = 1u << (t[0] & 31), w0v = *w0;
			if(w0v & b0) continue;
			if(bm[t[1] >> 5] & 1u << (t[1] & 31)) continue;
			if(bm[t[2] >> 5] & 1u << (t[2] & 31)) continue;
			*w0 = w0v | b0;
			bm[t[1] >> 5] |= 1u << (t[1] & 31);
			bm[t[2] >> 5] |= 1u << (t[2] & 31);
			dst[0] = t[0]; dst[1] = t[1]; dst[2] = t[2];
			dst += 3;
			t[2] = t[1] = t[0] = 0xffff;
			done++;
		}
	}while(done < nTri);
	rwFree(copy);
	rwFree(bm);
	return mesh;
}

// 0x5038f0: RpBuildMesh -> tri-list RpMeshHeader (flags 0): one mesh per material run of the sorted triangles, then each mesh reordered
MeshHeader *__cdecl rw37__rpTriListMeshGenerate(BuildMesh *bm, void *){
	BuildTri **sorted = (BuildTri **)rwMalloc(bm->numTriangles*4, 0x10502);
	if(!sorted){ errNoMem(bm->numTriangles*4); return 0; }
	for(u32 i = 0; i < bm->numTriangles; i++) sorted[i] = &bm->tris[i];
	g_qsort(sorted, bm->numTriangles, 4, cmpTris);
	u32 n = bm->numTriangles, runs = 1;
	if(n >= 2){
		void *m = sorted[0]->material;
		for(u32 i = 1; i < n; i++) if(sorted[i]->material != m){ m = sorted[i]->material; runs++; }
	}
	u32 size = (n + runs*2)*3*2 + 0x10;
	MeshHeader *h = g_hdrCreate(size);
	if(!h){ rwFree(sorted); errNoMem(size); return 0; }
	h->flags = 0;
	h->numMeshes = 1;
	h->serial = modSerial();
	h->firstMeshOffset = 0;
	h->totalIndices = bm->numTriangles*3;
	modSerial()++;
	Mesh *m = (Mesh *)(h + 1);
	u16 *dst = (u16 *)(m + runs);
	m->numIndices = 0;
	m->indices = dst;
	m->material = sorted[0]->material;
	u32 i = 0;
	do{
		if(sorted[i]->material != m->material){
			m++;
			m->numIndices = 0;
			m->indices = dst;
			m->material = sorted[i]->material;
			h->numMeshes++;
		}
		dst[0] = sorted[i]->v[0]; dst[1] = sorted[i]->v[1]; dst[2] = sorted[i]->v[2];
		dst += 3;
		m->numIndices += 3;
		i++;
	}while(i < bm->numTriangles);
	g_forAll(h, triListReorder, 0);
	rwFree(sorted);
	return h;
}

} // extern "C"
