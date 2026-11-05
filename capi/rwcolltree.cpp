// COLLTREE (LIB_RW group colltree; output/rw_next_family_2026-11-05.md row 8): the collision tree builder of MFL.exe (RpCollision plugin),
// written op for op from its disassembly like hanimcam.cpp (X = a value the original keeps on the x87 stack, float = a value it stores, R()
// = the store; valid at the game's x87 control word, the same operations in the same order):
//   BuildTriListCollTree 0x54da80, _rpCollTreeCreate 0x54dbc0, _rpCollTreeBuild 0x54e1c0, BuildTreeDestroy 0x54e470,
//   BuildTreeGenerate 0x54e4c0, GetClipStats 0x54ea50, DivisionScore 0x54ebf0, FuzzyExtentScore 0x54ecb0, SortPolygons 0x54ed10,
//   BuildPolySectorCreate 0x54ed50, BuildPlaneSectorCreate 0x54ed80, BuildDataDestroy 0x54edb0, BuildTreeCountLeafNodes 0x54ee10,
//   plus the unnamed qsort comparator 0x54ecf0 (float difference returned as its bit pattern).
// Every allocation goes through the exe's engine (+0x108 malloc, +0x10c free) in the original's order, sorting through the exe's CRT qsort.
// Exe-layout adapter: mflrw_colltree_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance 0x7d2128,
// RwBBoxInitialize 0x4d1990, RwBBoxAddPoint 0x4d19d0, qsort 0x5d38b0, RwErrorSet 0x4ea160.
// Check: tools/delink/rw_colltree_check.sh (the original code and these exports natively under Wine, same harness heap).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
#define AX(p, axis)  (*(const float *)((const u8 *)(p) + (axis)))		// axis = byte offset 0 / 4 / 8 of x / y / z

// build-time vertex (12): position, side flags (1 left / on, 2 right / on; first the use count), sort key
struct VInfo { const float *pos; u32 flags; float value; };
// build-time polygon (8): its RpTriangle {u16 v[3], matIndex}, side 1 left / 2 right
struct Poly { const u16 *tri; u32 side; };
// build data (0x3c); bbox {sup, inf}
struct BuildData { u32 numVerts; VInfo *vinfo; Poly *polys; const u8 *tris; u32 depth; float bbox[6]; u32 numPolys, first, numActive; VInfo **active; };
// build sector: plane (0x14) {axis, left, right, leftValue, rightValue} or polygon leaf (8) {-1, u16 numPolys, u16 first}
struct BSector { int32_t type; BSector *left, *right; float leftValue, rightValue; };
struct ClipStats { u32 nLeft, nRight; float leftValue, rightValue; };
struct CollParam { u32 flags; void (__cdecl *callBack)(void *geom, void *tree, u16 *map, void *data); void *data; };
typedef char sz1[sizeof(BuildData) == 0x3c ? 1 : -1];

typedef void *(__cdecl *MallocFn)(u32 size, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *BBoxFn)(float *bbox, const float *v);
typedef int (__cdecl *CmpFn)(const void *, const void *);
typedef void (__cdecl *QsortFn)(void *base, u32 n, u32 size, CmpFn cmp);
typedef void *(__cdecl *ErrorSetFn)(int32_t *err);

// ---- adapter
static u8 **g_engine;			// &RwEngineInstance 0x7d2128
static BBoxFn g_bboxInit, g_bboxAdd;	// RwBBoxInitialize 0x4d1990, RwBBoxAddPoint 0x4d19d0
static QsortFn g_qsort;			// qsort 0x5d38b0
static ErrorSetFn g_errorSet;		// RwErrorSet 0x4ea160
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] colltree called before mflrw_colltree_bind\n"); abort(); }
	return *g_engine;
}
static void *rwMalloc(u32 n, u32 hint){ return (*(MallocFn *)(eng() + 0x108))(n, hint); }
static void rwFree(void *p){ (*(FreeFn *)(eng() + 0x10c))(p); }
static void err(int32_t code){ int32_t e[2] = {0x11d, code}; g_errorSet(e); }	// _rwerror 0x53eac0 returns its first argument

// the original's float constants (0x5de19c 1e-4, 0x5de290 50, 0x5dd340 0.5, 0x5dd330 1, 0x5dfdf8 FLT_MAX, 0x5e64a4 0): float objects
static const float k1em4 = 9.99999974737875163555145263671875e-5f, k50 = 50.0f, kHalf = 0.5f, kOne = 1.0f, kZero = 0.0f;
static const u32 kFltMaxBits = 0x7f7fffff;

// 0x54ecf0: qsort key order of two VInfo pointers by value; the float difference is returned as its bit pattern (-0 is negative)
static int __cdecl cmpValue(const void *a, const void *b){
	float d = R((X)(*(VInfo *const *)a)->value - (*(VInfo *const *)b)->value);
	int32_t r; memcpy(&r, &d, 4); return r;
}

// side flags of the active vertices against the plane axis = value
static void classify(BuildData *bd, u32 axis, float value){
	for(u32 i = 0; i < bd->numActive; i++){
		VInfo *a = bd->active[i];
		X d = (X)AX(a->pos, axis) - value;
		a->flags = 0;
		if(d <= kZero) a->flags |= 1;
		if(d >= kZero) a->flags |= 2;
	}
}

extern "C" {

void __cdecl mflrw_colltree_bind(char **engine, void *bboxInit, void *bboxAdd, void *qsortFn, void *errorSet){
	g_engine = (u8 **)engine; g_bboxInit = (BBoxFn)bboxInit; g_bboxAdd = (BBoxFn)bboxAdd; g_qsort = (QsortFn)qsortFn; g_errorSet = (ErrorSetFn)errorSet;
}

BSector *__cdecl rw37_BuildPolySectorCreate(u32 numPolys, u32 first){
	u8 *s = (u8 *)rwMalloc(8, 0);
	if(!s) return 0;
	*(int32_t *)s = -1; *(u16 *)(s + 4) = (u16)numPolys; *(u16 *)(s + 6) = (u16)first;
	return (BSector *)s;
}

// the two plane values are copied as words (an SNaN stays signalling)
BSector *__cdecl rw37_BuildPlaneSectorCreate(int32_t axis, u32 leftValue, u32 rightValue){
	BSector *s = (BSector *)rwMalloc(0x14, 0);
	if(!s) return 0;
	s->type = axis; memcpy(&s->leftValue, &leftValue, 4); memcpy(&s->rightValue, &rightValue, 4);
	return s;
}

void __cdecl rw37_BuildDataDestroy(BuildData *bd){
	if(!bd) return;
	if(bd->vinfo) rwFree(bd->vinfo);
	if(bd->polys) rwFree(bd->polys);
	if(bd->active) rwFree(bd->active);
	rwFree(bd);
}

void __cdecl rw37_BuildTreeDestroy(BSector *s){
	if(!s) return;
	if(s->type < 0){ rwFree(s); return; }
	rw37_BuildTreeDestroy(s->left);
	rw37_BuildTreeDestroy(s->right);
	rwFree(s);
}

// 0x54ee10: leaves of the tree (iterative, 32 deep)
u32 __cdecl rw37_BuildTreeCountLeafNodes(BSector *root){
	BSector *stk[33];
	u32 n = 0;
	int sp = 0;
	stk[0] = 0;
	if(!root) return 0;
	BSector *c = root;
	do{
		if(c->type < 0){ c = stk[sp]; n++; sp--; }
		else{ sp++; stk[sp] = c->right; c = c->left; }
	}while(c);
	return n;
}

// 0x54ed10: the left polygons (side 1) to the front of the build data's range (only the triangle pointers move)
void __cdecl rw37_SortPolygons(BuildData *bd){
	Poly *p = bd->polys + bd->first;
	u32 k = 0;
	for(u32 i = 0; i < bd->numPolys; i++)
		if(p[i].side == 1){
			if(i > k){ const u16 *t = p[k].tri; p[k].tri = p[i].tri; p[i].tri = t; }
			k++;
		}
}

// 0x54ecb0: 1 / (a / b + 0.5) when a / b <= 0.5, else 1
X __cdecl rw37_FuzzyExtentScore(float a, float b){
	X r = (X)a / b;
	if(r > kHalf) return (X)kOne / kOne;
	return (X)kOne / (r + kHalf);
}

// 0x54ebf0: (Q * (sup - right) + P * (left - inf)) / (sup - inf) on the axis; P / Q = (bits + 1) - 2^bits / n of the left / right count
X __cdecl rw37_DivisionScore(BuildData *bd, ClipStats *st, u32 axis){
	X w = (X)AX(bd->bbox, axis) - AX(bd->bbox + 3, axis);
	X l = (X)st->leftValue - AX(bd->bbox + 3, axis);
	X r = (X)AX(bd->bbox, axis) - st->rightValue;
	int32_t bl = 0, br = 0;
	for(u32 v = st->nLeft; v; v >>= 1) bl++;
	for(u32 v = st->nRight; v; v >>= 1) br++;
	X p = (X)(bl + 1) - (X)(int32_t)(1u << (bl & 31)) / (X)(int32_t)st->nLeft;
	X q = (X)(br + 1) - (X)(int32_t)(1u << (br & 31)) / (X)(int32_t)st->nRight;
	return (q*r + p*l) / w;
}

// 0x54ea50: polygon sides against axis = value (all left / all right by the vertex flags, a straddler goes to the side it reaches less into),
// counts and the plane values (left max = value + furthest left-side reach to the right, right min = value - furthest right-side reach left)
void __cdecl rw37_GetClipStats(BuildData *bd, u32 axis, float value, ClipStats *st){
	X acc = kZero;
	Poly *p = bd->polys + bd->first;
	VInfo *vi = bd->vinfo;
	u32 nL = 0, nR = 0;
	float D = kZero;
	for(u32 i = bd->numPolys; (int32_t)i > 0; i--, p++){
		float maxPos = kZero, maxNeg = kZero;
		u32 fl = 3;
		for(u32 k = 0; k < 3; k++){
			VInfo *v = &vi[p->tri[k]];
			X d = (X)AX(v->pos, axis) - value;
			fl &= v->flags;
			float dS = R(d);
			X nd = -d;
			if(nd > maxNeg) maxNeg = R(nd);
			else if((X)dS > maxPos) maxPos = dS;
		}
		switch(fl & 3){
		case 1: p->side = 1; nL++; break;
		case 2: case 3: p->side = 2; nR++; break;
		default:
			if(maxPos > maxNeg){ p->side = 2; nR++; if(maxNeg > D) D = maxNeg; }
			else{ p->side = 1; nL++; if((X)maxPos > acc) acc = maxPos; }
		}
	}
	st->nLeft = nL;
	st->nRight = nR;
	st->leftValue = R(acc + value);
	st->rightValue = R((X)value - D);
}

// 0x54e4c0: one build sector for the build data's polygon range: a leaf under 4 polygons (or 32 levels deep), else the best of up to 50
// candidate planes per axis around the median of the active vertices (DivisionScore * FuzzyExtentScore), or a forced halving
BSector *__cdecl rw37_BuildTreeGenerate(BuildData *bd){
	u32 n = bd->numPolys;
	if(n < 4){
		u32 first = bd->first;
		u8 *s = (u8 *)rwMalloc(8, 0);
		if(!s) return 0;
		*(u16 *)(s + 4) = (u16)n; *(u16 *)(s + 6) = (u16)first; *(int32_t *)s = -1;
		return (BSector *)s;
	}
	if(bd->depth >= 0x20){
		if(n <= 0xef) return rw37_BuildPolySectorCreate(n, bd->first);
		err(0);
		return 0;
	}
	Poly *pp = bd->polys + bd->first;
	for(u32 i = 0; i < bd->numVerts; i++) bd->vinfo[i].flags = 0;
	for(u32 i = 0; i < bd->numPolys; i++, pp++) for(u32 k = 0; k < 3; k++) bd->vinfo[pp->tri[k]].flags++;
	u32 m = 0;
	for(u32 i = 0; i < bd->numVerts; i++) if(bd->vinfo[i].flags) bd->active[m++] = &bd->vinfo[i];
	bd->numActive = m;
	float ext[3];
	ext[0] = R((X)bd->bbox[0] - bd->bbox[3]);
	ext[1] = R((X)bd->bbox[1] - bd->bbox[4]);
	ext[2] = R((X)bd->bbox[2] - bd->bbox[5]);
	float best, bestValue = kZero;
	memcpy(&best, &kFltMaxBits, 4);
	u32 bestAxis = 0, e = 0;
	for(u32 c = 0; c < 3; c++) if((X)ext[e] < ext[c]) e = c;
	float thresh = R((X)ext[e] * k1em4);
	const float *mx = &ext[e];
	for(u32 axis = 0; axis < 12; axis += 4){
		float fuzz = R(rw37_FuzzyExtentScore(ext[axis >> 2], *mx));
		u32 cnt = 0;
		for(; cnt < bd->numActive; cnt++){ VInfo *a = bd->active[cnt]; a->value = R((X)AX(a->pos, axis)); }	// fld / fstp
		g_qsort(bd->active, bd->numActive, 4, cmpValue);
		u32 mid = cnt >> 1, lo = cnt >> 2, hi = lo + mid;
		float median = bd->active[mid]->value, scale;
		X range = (X)bd->active[hi]->value - bd->active[lo]->value;
		if(range < thresh){ scale = kZero; lo = hi = mid; }
		else{
			scale = R((X)k50 / range);
			for(u32 j = lo; j <= hi; j++){
				VInfo *a = bd->active[j];
				X v = (X)median - a->value;
				if(v < kZero) v = -v;
				a->value = R(v);
			}
			g_qsort(bd->active + lo, hi - lo + 1, 4, cmpValue);
		}
		int32_t lastQ = -1, tries = 50;
		for(u32 j = lo; j <= hi; j++){
			if(tries <= 0) break;
			float cand = R((X)AX(bd->active[j]->pos, axis));		// fld / fst
			float t = R((X)cand * scale);
			int32_t q = (int32_t)(X)t;
			if(q == lastQ) continue;
			classify(bd, axis, cand);
			ClipStats st;
			rw37_GetClipStats(bd, axis, cand, &st);
			if(st.nLeft && st.nRight){
				X s = rw37_DivisionScore(bd, &st, axis) * (X)fuzz;
				if(s < best){ bestAxis = axis; best = R(s); bestValue = cand; }
			}
			tries--;
			lastQ = q;
		}
	}
	ClipStats s2;
	u32 axis2;
	float fmax;
	memcpy(&fmax, &kFltMaxBits, 4);
	if((X)best < fmax){
		axis2 = bestAxis;
		classify(bd, axis2, bestValue);
		rw37_GetClipStats(bd, axis2, bestValue, &s2);
		rw37_SortPolygons(bd);
	}else{
		if(bd->numPolys <= 0xef) return rw37_BuildPolySectorCreate(bd->numPolys, bd->first);
		s2.nLeft = bd->numPolys >> 1;
		s2.nRight = bd->numPolys - s2.nLeft;
		memcpy(&s2.leftValue, &bd->bbox[0], 4);
		memcpy(&s2.rightValue, &bd->bbox[3], 4);
		axis2 = 0;
	}
	bd->depth++;
	u32 lv, rv;
	memcpy(&lv, &s2.leftValue, 4); memcpy(&rv, &s2.rightValue, 4);
	BSector *node = rw37_BuildPlaneSectorCreate((int32_t)axis2, lv, rv);
	if(!node) return 0;
	BuildData l = *bd;
	l.numPolys = s2.nLeft;
	*(float *)((u8 *)l.bbox + axis2) = R(s2.leftValue);
	node->left = rw37_BuildTreeGenerate(&l);
	if(!node->left){ rwFree(node); return 0; }
	BuildData r = *bd;
	r.numPolys = s2.nRight;
	r.first = bd->first + s2.nLeft;
	*(float *)((u8 *)(r.bbox + 3) + axis2) = R(s2.rightValue);
	node->right = rw37_BuildTreeGenerate(&r);
	if(!node->right){ rw37_BuildTreeDestroy(node); return 0; }
	return node;
}

// 0x54dbc0: RpCollTree {flags, bbox, u16 numPolygons, u16 numSplits, splits (16 aligned), map (2 aligned, flags & 1)} in one block
u8 *__cdecl rw37__rpCollTreeCreate(u32 numPolys_, u32 numSplits_, const float *bbox, u32 flags){
	u16 np = (u16)numPolys_, ns = (u16)numSplits_;
	u32 size = ns ? (u32)ns*16 + 0x37 : 0x28;
	if(flags & 1) size += (u32)np*2 + 1;
	u8 *t = (u8 *)rwMalloc(size, 0x3002c);
	if(!t){ err((int32_t)0x80000013); return 0; }
	*(u16 *)(t + 0x1c) = np;
	*(u32 *)t = flags;
	*(u16 *)(t + 0x1e) = ns;
	memcpy(t + 4, bbox, 24);
	uintptr_t e = (uintptr_t)(t + 0x28);
	if(ns){ e = (e + 0xf) & ~(uintptr_t)0xf; *(uintptr_t *)(t + 0x20) = e; e += (u32)ns*16; }
	else *(u32 *)(t + 0x20) = 0;
	if(flags & 1) *(uintptr_t *)(t + 0x24) = (e + 1) & ~(uintptr_t)1;
	else *(u32 *)(t + 0x24) = 0;
	return t;
}

// 0x54e1c0: collision tree of numTris triangles (8 byte RpTriangle) over numVerts vertices; map (or the tree's own) gets the triangle order
u8 *__cdecl rw37__rpCollTreeBuild(int32_t numVerts, const float *verts, int32_t numTris, const u8 *tris, u16 *map){
	u8 *result = 0;
	BuildData *bd = (BuildData *)rwMalloc(0x3c, 0);
	if(!bd) return 0;
	bd->vinfo = (VInfo *)rwMalloc(numVerts*12, 0);
	if(!bd->vinfo){ rw37_BuildDataDestroy(bd); return 0; }
	bd->active = (VInfo **)rwMalloc(numVerts*4, 0);
	if(!bd->active){ rw37_BuildDataDestroy(bd); return 0; }
	bd->polys = (Poly *)rwMalloc(numTris*8, 0);
	if(!bd->polys){ rw37_BuildDataDestroy(bd); return 0; }
	bd->first = 0;
	bd->depth = 0;
	bd->numPolys = numTris;
	bd->numVerts = numVerts;
	bd->tris = tris;
	g_bboxInit(bd->bbox, verts);
	for(int32_t i = 0; i < numVerts; i++){ g_bboxAdd(bd->bbox, verts + 3*i); bd->vinfo[i].pos = verts + 3*i; }
	for(int32_t i = 0; i < numTris; i++) bd->polys[i].tri = (const u16 *)(tris + 8*i);
	BSector *root = rw37_BuildTreeGenerate(bd);
	if(root){
		int32_t nSplits = (int32_t)rw37_BuildTreeCountLeafNodes(root) - 1;
		u8 *ct = rw37__rpCollTreeCreate(bd->numPolys, nSplits, bd->bbox, map == 0);
		if(ct){
			if(!map) map = *(u16 **)(ct + 0x24);
			for(u32 i = 0; i < bd->numPolys; i++) map[i] = (u16)((int32_t)((const u8 *)bd->polys[i].tri - bd->tris) >> 3);
			if(root->type >= 0){		// splits in preorder, left first: {axis | 1, left leaf count or 0xff, left index, leftValue, axis, right ..., rightValue}
				struct { u16 *slot; BSector *node; } stk[33];
				u16 dummy;
				int top = 0;
				u32 off = 0, idx = 0;
				stk[0].slot = &dummy; stk[0].node = root;
				do{
					u8 *s = *(u8 **)(ct + 0x20) + off;
					BSector *nd = stk[top].node;
					*stk[top].slot = (u16)idx;
					s[8] = (u8)nd->type;
					s[0] = (u8)nd->type | 1;
					memcpy(s + 4, &nd->leftValue, 4);
					memcpy(s + 0xc, &nd->rightValue, 4);
					BSector *c = nd->right;
					if(c->type < 0){ s[9] = ((u8 *)c)[4]; *(u16 *)(s + 0xa) = *(u16 *)((u8 *)c + 6); }
					else{ s[9] = 0xff; stk[top].node = c; stk[top].slot = (u16 *)(s + 0xa); top++; }
					c = nd->left;
					if(c->type < 0){ s[1] = ((u8 *)c)[4]; *(u16 *)(s + 2) = *(u16 *)((u8 *)c + 6); }
					else{ s[1] = 0xff; stk[top].node = c; stk[top].slot = (u16 *)(s + 2); top++; }
					off += 0x10;
					idx++;
					top--;
				}while(top >= 0);
			}
			result = ct;
		}
		rw37_BuildTreeDestroy(root);
	}
	rw37_BuildDataDestroy(bd);
	return result;
}

// 0x54da80: RpGeometry collision data; with the sort flag (param flags & 1) the triangles are reordered to the tree order and the callback
// gets the map
u8 *__cdecl rw37_BuildTriListCollTree(void *geom, int32_t numVerts, const float *verts, u32 numTris, const u8 *tris, u8 *trisOut, CollParam *param){
	CollParam local;
	if(!param){ local.flags = 0; local.callBack = 0; local.data = 0; param = &local; }
	if(!(param->flags & 1)) return rw37__rpCollTreeBuild(numVerts, verts, numTris, tris, 0);
	u32 size = numTris*10;
	u16 *map = (u16 *)rwMalloc(size, 0x1011d);
	if(!map){ err((int32_t)0x80000013); return 0; }
	u8 *copy = (u8 *)(map + numTris);
	memcpy(copy, trisOut, numTris*8);
	u8 *t = rw37__rpCollTreeBuild(numVerts, verts, numTris, tris, map);
	if(t){
		for(u32 i = 0; i < numTris; i++) memcpy(trisOut + 8*i, copy + 8*map[i], 8);
		if(param->callBack) param->callBack(geom, t, map, param->data);
	}
	rwFree(map);
	return t;
}

} // extern "C"
