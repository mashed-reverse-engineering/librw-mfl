// COLLIS (plan_next_24h_2026-11-05_v2 item D1, plan_librw_engine_switch_2026-11-03_v2 "stateless leaf, safe pre-flip"): the RpCollision
// triangle queries of MFL.exe, written op for op from its disassembly like g1.cpp / g2.cpp (X = a value the original keeps on the x87 stack,
// float = a value it stores; every comparison keeps the original's unordered (NaN) outcome; valid at the game's x87 CW 0x7f).
//   WorldForAllTriIntsecLine 0x549370, WorldForAllTriIntsecBBox 0x549f10, WorldForAllTriIntsecSphere 0x54a4d0, GeometryForAllTriIntsecLine 0x54adb0.
// The kernels read the exe's RW 3.7 objects in place: world +0x1c root sector; plane sector {type, value, left, right, leftValue, rightValue};
// world sector {type < 0, triangles +4, vertices +8}; geometry {numTriangles +0x10, triangles +0x2c, morphTarget +0x5c (+0x14 vertices)};
// collision tree {flags, sup[3], inf[3], u16 numEntries +0x1c, u16 numSplits +0x1e, splits +0x20, u16 map +0x24}; split = two 8 byte nodes
// {u8 type, u8 contents, u16 index, float value}.  Callbacks get the exe-layout RpIntersection / RpCollisionTriangle.
// Exe-layout adapter (thrown away at the librw flip): the two plugin data offsets (_rpCollisionWorldSectorDataOffset 0x7da6f4,
// _rpCollisionGeometryDataOffset 0x7da6f0) come from mflrw_collis_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c).
// Stack depths as in the original (65 plane-sector entries, 33 tree entries, no overflow check: the game's trees are at most 32 deep).
// Check: tools/delink/rw_collis_check.sh (the original code and these exports on the same random worlds natively under Wine, callback logs compared).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define P(p, off)  (*(u8 **)((u8 *)(p) + (off)))
#define F(p, off)  (*(float *)((u8 *)(p) + (off)))
#define I(p, off)  (*(int32_t *)((u8 *)(p) + (off)))
#define W16(p, off) (*(u16 *)((u8 *)(p) + (off)))

static inline float CF(u32 b){ float f; memcpy(&f, &b, 4); return f; }
#define K_1EM6  CF(0x358637bd)		// 1e-6   0x5df83c
#define K_1EM8  CF(0x322bcc77)		// 1e-8   0x5f5a78
#define K_M1EM5 CF(0xb727c5ac)		// -1e-5  0x5dfd40
#define K_057   CF(0x3f11eb85)		// 0.57   0x5f5a7c

typedef void *(__cdecl *WorldTriCB)(void *is, void *sector, void *tri, float dist, void *data);
typedef void *(__cdecl *GeomTriCB)(void *is, void *tri, float dist, void *data);

// RpCollisionTriangle {RwV3d normal; RwV3d point; RwInt32 index; RwV3d *vertices[3]}
struct CollTri { float normal[3]; float point[3]; int32_t index; const float *vertices[3]; };

// ---- adapter
static int32_t *g_sectorOff, *g_geomOff;
static inline int32_t sectorOff(void){ if(!g_sectorOff){ fprintf(stderr, "[mflrw] collis called before mflrw_collis_bind\n"); abort(); } return *g_sectorOff; }
static inline int32_t geomOff(void){ if(!g_geomOff){ fprintf(stderr, "[mflrw] collis called before mflrw_collis_bind\n"); abort(); } return *g_geomOff; }

// _rwInvSqrt 0x4d5470 (g0.cpp, table identical to the exe's)
static inline float invSqrt(float f){ return _rwInvSqrt(f); }

// x87 "fcomp 0 / C0 -> fchs": the absolute value with the original's NaN / -0 behaviour
static inline X absC0(float v){ X x = v; return !(v >= 0.0f) ? -x : x; }

// TriangleNormalMacro as compiled: (v1 - v0) on the x87 stack, (v2 - v0) x, y stored, z on the stack
static inline void triNormal(float *n, const float *v0, const float *v1, const float *v2){
	X ax = (X)v1[0] - v0[0], ay = (X)v1[1] - v0[1], az = (X)v1[2] - v0[2];
	float bx = v2[0] - v0[0], by = v2[1] - v0[1]; X bz = (X)v2[2] - v0[2];
	float nx = bz*ay - by*az; float ny = bx*az - bz*ax; float nz = by*ax - bx*ay;
	X l = (X)ny*ny + (X)nx*nx; l = l + (X)nz*nz;
	float lf = l;
	X r = invSqrt(lf);
	n[0] = nx*r; n[1] = ny*r; n[2] = r*nz;
}
static inline void triInit(CollTri *t, int32_t index, const float *v0, const float *v1, const float *v2){
	t->point[0] = v0[0]; t->point[1] = v0[1]; t->point[2] = v0[2];
	t->index = index; t->vertices[0] = v0; t->vertices[1] = v1; t->vertices[2] = v2;
}

// RtIntersectionLineTriangleMacro as inlined in the queries (one sided, relative slack): S = line start, D = delta; 1 + *dist on a hit
static inline int lineTri(const float *S, const float *D, const float *v0, const float *v1, const float *v2, float *dist){
	float e1x = v1[0] - v0[0], e1y = v1[1] - v0[1], e1z = v1[2] - v0[2];
	float e2x = v2[0] - v0[0], e2y = v2[1] - v0[1], e2z = v2[2] - v0[2];
	X px = (X)e2z*D[1] - (X)e2y*D[2];
	X py = (X)e2x*D[2] - (X)e2z*D[0];
	X pz = (X)e2y*D[0] - (X)e2x*D[1];
	X dX = py*e1y + px*e1x; dX = dX + pz*e1z;
	float det = dX;
	if(!(dX > K_1EM8)) return 0;
	float lo = (X)det*K_M1EM5;
	float hi = (X)det - lo;
	float tx = S[0] - v0[0], ty = S[1] - v0[1], tz = S[2] - v0[2];
	X uX = ty*py + tx*px; uX = uX + tz*pz;
	float u = uX;
	if(!(u >= lo)) return 0;
	if(!(u <= hi)) return 0;
	float qx = (X)ty*e1z - (X)tz*e1y, qy = (X)tz*e1x - (X)tx*e1z, qz = (X)tx*e1y - (X)ty*e1x;
	X v = (X)qy*D[1] + (X)qx*D[0]; v = v + (X)qz*D[2];
	if(!(v >= lo)) return 0;
	if(!((X)u + v <= hi)) return 0;
	X tX = (X)qy*e2y + (X)qx*e2x; tX = tX + (X)qz*e2z;
	float t = tX;
	if(!(tX >= lo)) return 0;
	if(!(t <= hi)) return 0;
	*dist = (X)t/det;
	return 1;
}

// line iterator data (RpSectorLineIt / RpCollSectorLineIt: start, delta, recip, padding)
struct LineIt { float start[3], delta[3], recip[3], pad[3]; };
struct LStk { u8 *node; float t0, t1; };

static inline void lineItInit(LineIt *it, const float *start, const float *end){
	for(int a = 0; a < 3; a++){ it->start[a] = start[a]; it->delta[a] = (X)end[a] - start[a]; }
	for(int a = 0; a < 3; a++) it->recip[a] = it->delta[a] == 0.0f ? 0.0f : (float)((X)1.0f/it->delta[a]);
	X m[3];
	for(int a = 0; a < 3; a++){
		X s = absC0(start[a]), e = absC0(end[a]);
		m[a] = !(s >= e) ? absC0(end[a]) : absC0(start[a]);
	}
	for(int a = 0; a < 3; a++) it->pad[a] = m[a]*K_1EM6;
}

// one step of the line walk on a split (both trees): axis byte offset a, left / right values and children.  Returns with the stack top
// replaced (one child) or a new top pushed (near child on top, far child below), the t intervals clipped as the original does.
static inline void lineSplit(const LineIt *it, LStk *&top, int a, float lval, float rval, u8 *left, u8 *right){
	X d = it->delta[a/4], s = it->start[a/4], pd = it->pad[a/4];
	float p1 = d*top->t1 + s;
	float p0 = d*top->t0 + s;
	float rlo = (X)rval - pd;
	float lhi = (X)lval + pd;
	if(d > 0.0f){
		if(rlo > p1){ top->node = left; return; }
		if(lhi < p0){ top->node = right; return; }
		LStk *o = top++; X inv = it->recip[a/4];
		top->node = left; top->t0 = o->t0; top->t1 = o->t1;
		if(lhi < p1) top->t1 = ((X)lhi - p1)*inv + top->t1;
		o->node = right;
		if(rlo > p0) o->t0 = ((X)rlo - p0)*inv + o->t0;
	}else{
		if(lhi < p1){ top->node = right; return; }
		if(rlo > p0){ top->node = left; return; }
		LStk *o = top++; X inv = it->recip[a/4];
		top->node = right; top->t0 = o->t0; top->t1 = o->t1;
		if(rlo > p1) top->t1 = ((X)rlo - p1)*inv + top->t1;
		o->node = left;
		if(lhi < p0) o->t0 = ((X)lhi - p0)*inv + o->t0;
	}
}

// kd walk of a collision tree for a line, from a pseudo root over the interval (t0, t1); visit(start, count) per leaf, 0 = stop
template<class V> static inline int collLineWalk(const LineIt *it, u8 *tree, float t0, float t1, V visit){
	u8 dummy[8];
	dummy[1] = 0xff; W16(dummy, 2) = 0;
	if(W16(tree, 0x1e) == 0) dummy[1] = tree[0x1c];
	LStk stk[33], *top = &stk[1];
	stk[0].node = dummy; stk[0].t0 = t0; stk[0].t1 = t1;
	do{
		top--;
		while(top->node[1] == 0xff){
			u8 *sp = P(tree, 0x20) + W16(top->node, 2)*16;
			int a = sp[0] & 0xc;
			lineSplit(it, top, a, F(sp, 4), F(sp, 0xc), sp, sp + 8);
		}
		u32 i = W16(top->node, 2); int end = (int)(i & 0xffff) + top->node[1];
		for(; (int)(i & 0xffff) < end; i++)
			if(!visit(i & 0xffff)) return 0;
	}while(top > stk);
	return 1;
}

// box walk entry of a collision tree: outer box (culling), inner box (the "region fully inside" flags); visit(index, clip) per entry
template<class V> static inline int collBoxWalk(u8 *tree, const float *outer, const float *inner, u32 flags0, V visit){
	struct BStk { u8 *node; u32 flags; } stk[33], *top = &stk[1];
	u8 dummy[8];
	dummy[1] = 0xff; W16(dummy, 2) = 0;
	if(W16(tree, 0x1e) == 0) dummy[1] = tree[0x1c];
	stk[0].node = dummy; stk[0].flags = flags0;
	do{
		top--;
		u8 *node = top->node;
		u32 clip, start; int end;
		for(;;){
			if(node[1] != 0xff){ clip = top->flags; start = W16(node, 2); end = (int)(start & 0xffff) + node[1]; break; }
			if(top->flags == 0){
				// whole subtree inside the inner box: the entry range from its leftmost to its rightmost leaf, untested
				u8 *sp0 = P(tree, 0x20), *s = sp0 + W16(node, 2)*16, *l = s;
				while(l[1] == 0xff) l = sp0 + W16(l, 2)*16;
				u8 *r = s + 8;
				while(r[1] == 0xff) r = sp0 + W16(r, 2)*16 + 8;
				u16 rend = (u16)(r[1] + W16(r, 2));
				start = W16(l, 2);
				clip = 0;
				end = (int)(start & 0xffff) + (u16)(rend - (u16)start);
				break;
			}
			u8 *sp = P(tree, 0x20) + W16(node, 2)*16;
			int a = (sp[0] & 0xc)/4;
			if(outer[a] < F(sp, 0xc)) top->node = sp;
			else{
				top->node = sp + 8;
				if(outer[3 + a] <= F(sp, 4)){
					top++;
					top->node = sp; top->flags = top[-1].flags;
					if(inner[a] >= F(sp, 4)) top->flags &= ~(2u << (2*a));
					if(inner[3 + a] <= F(sp, 0xc)) top[-1].flags &= ~(1u << (2*a));
				}
			}
			node = top->node;
		}
		for(u32 i = start; (int)(i & 0xffff) < end; i++)
			if(!visit(i & 0xffff, clip)) return 0;
	}while(top > stk);
	return 1;
}

// plane sector walk for a box (left child first); leaf(sector) per world sector, 0 = stop
template<class V> static inline void sectorBoxWalk(u8 *world, const float *box, V leaf){
	u8 *stk[65], **top = &stk[1];
	stk[0] = P(world, 0x1c);
	do{
		top--;
		u8 *n = *top;
		while(I(n, 0) >= 0){
			int a = I(n, 0)/4;
			if(box[a] < F(n, 0x14)) *top = P(n, 8);
			else{
				*top = P(n, 0xc);
				if(box[3 + a] <= F(n, 0x10)) *++top = P(n, 8);
			}
			n = *top;
		}
		if(!leaf(n)) return;
	}while(top > stk);
}

// collision tree flags of a query box against the tree's box (inner = the box that fits in the query volume)
static inline u32 treeFlags(u8 *tree, const float *inner){
	u32 f = F(tree, 0x10) < inner[3] ? 1 : 0;
	f |= F(tree, 0x14) < inner[4] ? 4 : 0;
	f |= F(tree, 0x18) < inner[5] ? 0x10 : 0;
	f |= F(tree, 4) > inner[0] ? 2 : 0;
	f |= F(tree, 8) > inner[1] ? 8 : 0;
	f |= F(tree, 0xc) > inner[2] ? 0x20 : 0;
	return f;
}
// the tree's box overlaps the query box (inclusive)
static inline int treeOverlaps(u8 *tree, const float *box){
	return F(tree, 0x10) <= box[0] && F(tree, 0x14) <= box[1] && F(tree, 0x18) <= box[2]
		&& F(tree, 4) >= box[3] && F(tree, 8) >= box[4] && F(tree, 0xc) >= box[5];
}

extern "C" {

void __cdecl mflrw_collis_bind(int32_t *sectorOffVar, int32_t *geomOffVar){ g_sectorOff = sectorOffVar; g_geomOff = geomOffVar; }

// L1:exact 0x549370 WorldForAllTriIntsecLine(world, is, callBack, data): plane tree near to far, then each sector's tree near to far
void __cdecl rw37_WorldForAllTriIntsecLine(u8 *world, u8 *is, WorldTriCB cb, void *data){
	LineIt it;
	lineItInit(&it, (float *)is, (float *)(is + 0xc));
	LStk stk[65], *top = &stk[1];
	stk[0].node = P(world, 0x1c); stk[0].t0 = 0.0f; stk[0].t1 = 1.0f;
	do{
		top--;
		while(I(top->node, 0) >= 0){
			u8 *n = top->node;
			lineSplit(&it, top, I(n, 0), F(n, 0x10), F(n, 0x14), P(n, 8), P(n, 0xc));
		}
		u8 *sector = top->node;
		u8 *tree = P(sector, sectorOff());
		if(!tree) continue;
		u8 *tris = P(sector, 4); float *verts = (float *)P(sector, 8);
		LineIt cit = it;
		int go = collLineWalk(&cit, tree, top->t0, top->t1, [&](u32 i) -> int {
			u8 *map = P(tree, 0x24);
			u32 idx = map ? W16(map, 2*i) : i;
			u16 *t = (u16 *)(tris + idx*8);
			const float *v0 = verts + 3*t[0], *v1 = verts + 3*t[1], *v2 = verts + 3*t[2];
			float dist;
			if(!lineTri(it.start, it.delta, v0, v1, v2, &dist)) return 1;
			CollTri ct;
			triInit(&ct, idx, v0, v1, v2);
			triNormal(ct.normal, v0, v1, v2);
			return cb(is, sector, &ct, dist, data) != NULL;
		});
		if(!go) return;
	}while(top > stk);
}

// L1:exact 0x549f10 WorldForAllTriIntsecBBox(world, is, callBack, data): distance always 0; entries of regions inside the box untested
void __cdecl rw37_WorldForAllTriIntsecBBox(u8 *world, u8 *is, WorldTriCB cb, void *data){
	float box[6];
	memcpy(box, is, 24);
	sectorBoxWalk(world, box, [&](u8 *sector) -> int {
		u8 *tree = P(sector, sectorOff());
		if(!tree) return 1;
		u8 *tris = P(sector, 4); float *verts = (float *)P(sector, 8);
		if(!treeOverlaps(tree, (float *)is)) return 1;
		float outer[6], inner[6];
		memcpy(outer, is, 24); memcpy(inner, is, 24);
		u32 f = treeFlags(tree, (float *)is);
		return collBoxWalk(tree, outer, inner, f, [&](u32 i, u32 clip) -> int {
			u8 *map = P(tree, 0x24);
			u32 idx = map ? W16(map, 2*i) : i;
			u16 *t = (u16 *)(tris + idx*8);
			const float *v0 = verts + 3*t[0], *v1 = verts + 3*t[1], *v2 = verts + 3*t[2];
			if(clip && !RtIntersectionBBoxTriangle((RwBBox *)is, (RwV3d *)v0, (RwV3d *)v1, (RwV3d *)v2)) return 1;
			CollTri ct;
			triInit(&ct, idx, v0, v1, v2);
			triNormal(ct.normal, v0, v1, v2);
			return cb(is, sector, &ct, 0.0f, data) != NULL;
		});
	});
}

// L1:exact 0x54a4d0 WorldForAllTriIntsecSphere(world, sphere, is, callBack, data): distance = signed plane distance of the centre / radius
void __cdecl rw37_WorldForAllTriIntsecSphere(u8 *world, float *sp, u8 *is, WorldTriCB cb, void *data){
	X irX = (X)1.0f/sp[3];
	float c[12];		// outer sup, outer inf, inner sup, inner inf
	c[0] = sp[0]; c[1] = sp[1]; c[2] = sp[2]; c[3] = sp[0]; c[4] = sp[1]; c[5] = sp[2];
	c[6] = sp[0]; c[7] = sp[1]; c[8] = sp[2]; c[9] = sp[0]; c[10] = sp[1]; c[11] = sp[2];
	float ir = irX;
	X in = (X)sp[3]*K_057;
	c[3] = (X)c[3] - sp[3]; c[4] = (X)c[4] - sp[3]; c[5] = (X)c[5] - sp[3];
	c[0] = (X)sp[3] + c[0]; c[1] = (X)sp[3] + c[1]; c[2] = (X)sp[3] + c[2];
	c[9] = c[9] - in; c[10] = c[10] - in; c[11] = c[11] - in;
	c[6] = c[6] + in; c[7] = c[7] + in; c[8] = c[8] + in;
	float box[6];
	memcpy(box, c, 24);
	sectorBoxWalk(world, box, [&](u8 *sector) -> int {
		u8 *tree = P(sector, sectorOff());
		if(!tree) return 1;
		// the sphere against the tree's box (squared distance of the centre)
		X dx, dy, dz;
		if(F(tree, 0x10) > sp[0]) dx = (X)F(tree, 0x10) - sp[0];
		else if(F(tree, 4) < sp[0]) dx = (X)sp[0] - F(tree, 4);
		else dx = 0.0f;
		X acc = dx*dx;
		if(F(tree, 0x14) > sp[1]) dy = (X)F(tree, 0x14) - sp[1];
		else if(F(tree, 8) < sp[1]) dy = (X)sp[1] - F(tree, 8);
		else dy = 0.0f;
		acc = acc + dy*dy;
		if(F(tree, 0x18) > sp[2]) dz = (X)F(tree, 0x18) - sp[2];
		else if(F(tree, 0xc) < sp[2]) dz = (X)sp[2] - F(tree, 0xc);
		else dz = 0.0f;
		X r = sp[3];
		X d2 = dz*dz + acc;
		if(!(r*r >= d2)) return 1;
		u8 *tris = P(sector, 4); float *verts = (float *)P(sector, 8);
		if(!treeOverlaps(tree, c)) return 1;
		float outer[6], inner[6];
		memcpy(outer, c, 24); memcpy(inner, c + 6, 24);
		u32 f = treeFlags(tree, c + 6);
		return collBoxWalk(tree, outer, inner, f, [&](u32 i, u32 clip) -> int {
			u8 *map = P(tree, 0x24);
			u32 idx = map ? W16(map, 2*i) : i;
			u16 *t = (u16 *)(tris + idx*8);
			const float *v0 = verts + 3*t[0], *v1 = verts + 3*t[1], *v2 = verts + 3*t[2];
			CollTri ct;
			X dist;
			if(!clip){
				triNormal(ct.normal, v0, v1, v2);
				X ox = (X)sp[0] - v0[0], oy = (X)sp[1] - v0[1], oz = (X)sp[2] - v0[2];
				dist = oy*ct.normal[1] + ox*ct.normal[0];
				dist = dist + oz*ct.normal[2];
			}else{
				float d;
				if(!RtIntersectionSphereTriangle((RwSphere *)sp, (RwV3d *)v0, (RwV3d *)v1, (RwV3d *)v2, (RwV3d *)ct.normal, &d)) return 1;
				dist = d;
			}
			float df = dist*ir;
			triInit(&ct, idx, v0, v1, v2);
			return cb(is, sector, &ct, df, data) != NULL;
		});
	});
}

// L1:exact 0x54adb0 GeometryForAllTriIntsecLine(geometry, line, is, callBack, data): the line clipped to the tree's (padded) box, then
// the kd tree near to far; a geometry without a tree is brute forced over all its triangles
void __cdecl rw37_GeometryForAllTriIntsecLine(u8 *geom, float *line, u8 *is, GeomTriCB cb, void *data){
	u8 *tree = P(geom, geomOff());
	if(!tree){
		u8 *tri = P(geom, 0x2c);
		float *verts = (float *)P(P(geom, 0x5c), 0x14);
		float d[3] = {(float)((X)line[3] - line[0]), (float)((X)line[4] - line[1]), (float)((X)line[5] - line[2])};
		for(int32_t n = 0; n < I(geom, 0x10); n++, tri += 8){
			u16 *t = (u16 *)tri;
			const float *v0 = verts + 3*t[0], *v1 = verts + 3*t[1], *v2 = verts + 3*t[2];
			float dist;
			if(!lineTri(line, d, v0, v1, v2, &dist)) continue;
			CollTri ct;
			triInit(&ct, n, v0, v1, v2);
			triNormal(ct.normal, v0, v1, v2);
			if(!cb(is, &ct, dist, data)) return;
		}
		return;
	}
	u8 *tris = P(geom, 0x2c);
	float *verts = (float *)P(P(geom, 0x5c), 0x14);
	LineIt it;
	lineItInit(&it, line, line + 3);
	float d[3] = {it.delta[0], it.delta[1], it.delta[2]};
	// RpCollSectorLineItInit: clip [0, 1] to the tree's box widened by the padding, axis by axis
	X lox = (X)F(tree, 0x10) - it.pad[0];
	float loy = (X)F(tree, 0x14) - it.pad[1], loz = (X)F(tree, 0x18) - it.pad[2];
	float hix = (X)F(tree, 4) + it.pad[0], hiy = (X)F(tree, 8) + it.pad[1], hiz = (X)F(tree, 0xc) + it.pad[2];
	float t0 = 0.0f, t1 = 1.0f;
	if(it.recip[0] == 0.0f){
		if(!(it.start[0] >= lox)) return;
		if(!(it.start[0] <= hix)) return;
	}else{
		float a = (lox - it.start[0])*it.recip[0];
		X bX = ((X)hix - it.start[0])*it.recip[0];
		float b = bX, mn = a, mx = b;
		if(bX < a){ mx = a; mn = b; }
		if(!(0.0f <= mx)) return;
		if(!(1.0f >= mn)) return;
		if(!(0.0f >= mn)) t0 = mn;
		if(!(1.0f <= mx)) t1 = mx;
	}
	const float lo[2] = {loy, loz}, hi[2] = {hiy, hiz};
	for(int k = 1; k < 3; k++){
		if(it.recip[k] == 0.0f){
			if(!(it.start[k] >= lo[k - 1])) return;
			if(!(it.start[k] <= hi[k - 1])) return;
			continue;
		}
		float a = ((X)lo[k - 1] - it.start[k])*it.recip[k];
		X bX = ((X)hi[k - 1] - it.start[k])*it.recip[k];
		float b = bX, mn = a, mx = b;
		if(bX < a){ mn = b; mx = a; }
		if(!(t0 <= mx)) return;
		if(!(t1 >= mn)) return;
		if(!(t0 >= mn)) t0 = mn;
		if(!(t1 <= mx)) t1 = mx;
	}
	collLineWalk(&it, tree, t0, t1, [&](u32 i) -> int {
		u8 *map = P(tree, 0x24);
		u32 idx = map ? W16(map, 2*i) : i;
		u16 *t = (u16 *)(tris + idx*8);
		const float *v0 = verts + 3*t[0], *v1 = verts + 3*t[1], *v2 = verts + 3*t[2];
		float dist;
		if(!lineTri(line, d, v0, v1, v2, &dist)) return 1;
		CollTri ct;
		triInit(&ct, idx, v0, v1, v2);
		triNormal(ct.normal, v0, v1, v2);
		return cb(is, &ct, dist, data) != NULL;
	});
}

}
