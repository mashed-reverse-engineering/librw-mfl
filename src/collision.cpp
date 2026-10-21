// RpCollision pieces (collision kd tree plugin 0x11D, intersection queries). Tree format and builder written from
// MashedFormats/FORMATS.md + rwscene.py (read_colltree) + the shipped data; the queries and Rt tests are ports of the
// game's statically linked routines (decompilation-informed, review patch 0017, see MFL_NOTES.md "Collision").
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwcollision.h"

#define PLUGIN_ID ID_COLLISION

namespace rw {

//
// Tree storage / blob
//

static const uint32 STAMP = 0x1C02000A;	// library stamp of the game's data
static const uint32 CHUNK_COLLTREE = 0x2C;

static void put32(uint8 *&p, uint32 v) { memcpy(p, &v, 4); p += 4; }
static void putF(uint8 *&p, float32 v) { memcpy(p, &v, 4); p += 4; }
static uint32 get32(const uint8 *p) { uint32 v; memcpy(&v, p, 4); return v; }
static float32 getF(const uint8 *p) { float32 v; memcpy(&v, p, 4); return v; }

static CollisionTree*
allocTree(uint32 nent, uint32 nsplit, bool map)
{
	CollisionTree *t = rwNewT(CollisionTree, 1, MEMDUR_EVENT | ID_COLLISION);
	if(t == nil)
		return nil;
	memset(t, 0, sizeof(*t));
	t->numEntries = nent;
	t->numSplits = nsplit;
	if(nsplit){
		t->splits = rwNewT(CollSplit, nsplit, MEMDUR_EVENT | ID_COLLISION);
		if(t->splits == nil){ t->destroy(); return nil; }
	}
	if(map){
		t->flags |= CollisionTree::HASMAP;
		if(nent){
			t->map = rwNewT(uint16, nent, MEMDUR_EVENT | ID_COLLISION);
			if(t->map == nil){ t->destroy(); return nil; }
		}
	}
	return t;
}

void
CollisionTree::destroy(void)
{
	rwFree(this->splits);
	rwFree(this->map);
	rwFree(this);
}

CollisionTree*
CollisionTree::clone(void) const
{
	CollisionTree *t = allocTree(this->numEntries, this->numSplits, (this->flags & HASMAP) != 0);
	if(t == nil)
		return nil;
	t->flags = this->flags;
	t->bbox = this->bbox;
	if(this->numSplits)
		memcpy(t->splits, this->splits, sizeof(CollSplit)*this->numSplits);
	if(this->map)
		memcpy(t->map, this->map, 2*this->numEntries);
	return t;
}

uint32
CollisionTree::blobSize(void) const
{
	return 4 + 12 + 12 + 36 + 16*this->numSplits + ((this->flags & HASMAP) ? 2*this->numEntries : 0);
}

void
CollisionTree::toBlob(uint8 *p) const
{
	uint32 st = blobSize() - 4 - 12 - 12;
	put32(p, VERSION);
	put32(p, CHUNK_COLLTREE); put32(p, st + 12); put32(p, STAMP);
	put32(p, ID_STRUCT); put32(p, st); put32(p, STAMP);
	put32(p, this->flags);
	putF(p, bbox.inf.x); putF(p, bbox.inf.y); putF(p, bbox.inf.z);
	putF(p, bbox.sup.x); putF(p, bbox.sup.y); putF(p, bbox.sup.z);
	put32(p, this->numEntries);
	put32(p, this->numSplits);
	for(uint32 i = 0; i < this->numSplits; i++){
		const CollNode *n[2] = { &splits[i].left, &splits[i].right };
		for(int k = 0; k < 2; k++){
			*p++ = n[k]->type;
			*p++ = n[k]->contents;
			memcpy(p, &n[k]->index, 2); p += 2;
			putF(p, n[k]->value);
		}
	}
	if(this->flags & HASMAP){
		memcpy(p, this->map, 2*this->numEntries);
	}
}

static bool
nodeOk(const CollNode &n, uint32 nent, uint32 nsplit)
{
	if((n.type & ~1) != 0 && (n.type & ~1) != 4 && (n.type & ~1) != 8)
		return false;
	if(n.value != n.value)
		return false;
	if(n.contents == CollisionTree::SPLIT)
		return n.index < nsplit;
	return (uint32)n.index + n.contents <= nent;
}

// splits must form a tree rooted at 0: each referenced exactly once, depth limited
static bool
treeOk(const CollisionTree *t)
{
	if(t->numSplits == 0)
		return true;
	std::vector<uint8> seen(t->numSplits, 0);
	std::vector<std::pair<uint32,int> > stack;
	stack.push_back(std::make_pair(0u, 1));
	seen[0] = 1;
	uint32 count = 0;
	while(!stack.empty()){
		uint32 si = stack.back().first;
		int depth = stack.back().second;
		stack.pop_back();
		count++;
		if(depth > CollisionTree::MAXDEPTH)
			return false;
		const CollNode *n[2] = { &t->splits[si].left, &t->splits[si].right };
		for(int k = 0; k < 2; k++)
			if(n[k]->contents == CollisionTree::SPLIT){
				if(seen[n[k]->index])
					return false;
				seen[n[k]->index] = 1;
				stack.push_back(std::make_pair((uint32)n[k]->index, depth+1));
			}
	}
	return count == t->numSplits;
}

CollisionTree*
CollisionTree::fromBlob(const uint8 *b, uint32 size, int32 numTriangles)
{
	const char *err = "invalid collision tree";
	if(b == nil || size < 4+12+12+36 || numTriangles < 0 || numTriangles > 65536 ||
	   get32(b) != VERSION || get32(b+4) != CHUNK_COLLTREE || (uint64)get32(b+8) + 16 != size ||
	   get32(b+16) != ID_STRUCT || (uint64)get32(b+20) + 12 != get32(b+8)){
		RWERROR((ERR_GENERAL, err));
		return nil;
	}
	const uint8 *s = b + 28;
	uint32 slen = get32(b+20);
	uint32 flags = get32(s);
	uint32 nent = get32(s+28), nsplit = get32(s+32);
	if((flags & ~1u) || nent != (uint32)numTriangles || nsplit > 65536 ||
	   slen != 36 + 16*(uint64)nsplit + ((flags & 1) ? 2*(uint64)nent : 0)){
		RWERROR((ERR_GENERAL, err));
		return nil;
	}
	CollisionTree *t = allocTree(nent, nsplit, (flags & 1) != 0);
	if(t == nil)
		return nil;
	t->flags = flags;
	t->bbox.inf.set(getF(s+4), getF(s+8), getF(s+12));
	t->bbox.sup.set(getF(s+16), getF(s+20), getF(s+24));
	bool ok = true;
	const uint8 *p = s + 36;
	for(uint32 i = 0; i < nsplit; i++){
		CollNode *n[2] = { &t->splits[i].left, &t->splits[i].right };
		for(int k = 0; k < 2; k++){
			n[k]->type = p[0];
			n[k]->contents = p[1];
			memcpy(&n[k]->index, p+2, 2);
			n[k]->value = getF(p+4);
			p += 8;
			ok = ok && nodeOk(*n[k], nent, nsplit);
		}
	}
	if(flags & 1){
		memcpy(t->map, p, 2*nent);
		for(uint32 i = 0; i < nent; i++)
			ok = ok && t->map[i] < nent;
	}
	if(!ok || !treeOk(t)){
		t->destroy();
		RWERROR((ERR_GENERAL, err));
		return nil;
	}
	return t;
}

//
// Builder
//

struct Builder
{
	const V3d *v;
	const Triangle *t;
	std::vector<uint16> order;
	std::vector<float32> lo, hi, cen;	// 3 per triangle
	std::vector<CollSplit> splits;
	int32 maxLeaf;
	float32 binf[3], bsup[3];
};

static CollNode makeNode(Builder &b, int32 s, int32 e, int axis, bool left, float32 value);

static uint32
splitRange(Builder &b, int32 s, int32 e)
{
	uint32 idx = b.splits.size();
	b.splits.push_back(CollSplit());
	float32 cl[3] = { 1e30f, 1e30f, 1e30f }, ch[3] = { -1e30f, -1e30f, -1e30f };
	for(int32 i = s; i < e; i++)
		for(int a = 0; a < 3; a++){
			float32 c = b.cen[3*b.order[i]+a];
			cl[a] = std::min(cl[a], c); ch[a] = std::max(ch[a], c);
		}
	int axis = 0;
	for(int a = 1; a < 3; a++)
		if(ch[a]-cl[a] > ch[axis]-cl[axis])
			axis = a;
	int32 mid = s + (e-s)/2;
	if(mid > s && mid < e){
		const std::vector<float32> &cen = b.cen;
		std::nth_element(b.order.begin()+s, b.order.begin()+mid, b.order.begin()+e,
			[&cen, axis](uint16 x, uint16 y){ return cen[3*x+axis] < cen[3*y+axis] || (cen[3*x+axis] == cen[3*y+axis] && x < y); });
	}
	float32 lv = (&b.binf[0])[axis], rv = (&b.bsup[0])[axis];
	bool any = false;
	for(int32 i = s; i < mid; i++){
		float32 h = b.hi[3*b.order[i]+axis];
		lv = any ? std::max(lv, h) : h; any = true;
	}
	any = false;
	for(int32 i = mid; i < e; i++){
		float32 l = b.lo[3*b.order[i]+axis];
		rv = any ? std::min(rv, l) : l; any = true;
	}
	CollSplit sp;
	sp.left = makeNode(b, s, mid, axis, true, lv);
	sp.right = makeNode(b, mid, e, axis, false, rv);
	b.splits[idx] = sp;
	return idx;
}

static CollNode
makeNode(Builder &b, int32 s, int32 e, int axis, bool left, float32 value)
{
	CollNode n;
	n.type = axis*4 | (left ? 1 : 0);
	n.value = value;
	if(e - s <= b.maxLeaf){
		n.contents = e - s;
		n.index = s;
	}else{
		n.contents = CollisionTree::SPLIT;
		n.index = splitRange(b, s, e);
	}
	return n;
}

void
collisionBuildParamInitDefaults(CollisionBuildParams *p)
{
	p->maxEntriesPerLeaf = 2;
}

CollisionTree*
CollisionTree::build(const V3d *vertices, const Triangle *tris, int32 n, int32 maxLeaf)
{
	if(n < 0 || n > 65536 || maxLeaf < 1 || maxLeaf > 254){
		RWERROR((ERR_GENERAL, "invalid collision build parameters"));
		return nil;
	}
	if(n <= maxLeaf){	// a single leaf: no splits, the root is the pseudo node {contents (u8)n, index 0}
		CollisionTree *t = allocTree(n, 0, true);
		if(t == nil)
			return nil;
		for(int32 i = 0; i < n; i++){
			if(i == 0) t->bbox.inf = t->bbox.sup = vertices[tris[0].v[0]];
			for(int k = 0; k < 3; k++)
				t->bbox.addPoint((V3d*)&vertices[tris[i].v[k]]);
			t->map[i] = i;
		}
		return t;
	}
	Builder b;
	b.v = vertices; b.t = tris; b.maxLeaf = maxLeaf;
	b.order.resize(n); b.lo.resize(3*n); b.hi.resize(3*n); b.cen.resize(3*n);
	for(int a = 0; a < 3; a++){ b.binf[a] = 1e30f; b.bsup[a] = -1e30f; }
	for(int32 i = 0; i < n; i++){
		b.order[i] = i;
		for(int a = 0; a < 3; a++){
			float32 c[3];
			for(int k = 0; k < 3; k++)
				c[k] = (&vertices[tris[i].v[k]].x)[a];
			float32 l = std::min(c[0], std::min(c[1], c[2]));
			float32 h = std::max(c[0], std::max(c[1], c[2]));
			b.lo[3*i+a] = l; b.hi[3*i+a] = h;
			b.cen[3*i+a] = (l+h)*0.5f;
			b.binf[a] = std::min(b.binf[a], l); b.bsup[a] = std::max(b.bsup[a], h);
		}
	}
	splitRange(b, 0, n);	// the root is always a pair
	CollisionTree *t = allocTree(n, b.splits.size(), true);
	if(t == nil)
		return nil;
	t->bbox.inf.set(b.binf[0], b.binf[1], b.binf[2]);
	t->bbox.sup.set(b.bsup[0], b.bsup[1], b.bsup[2]);
	memcpy(t->splits, &b.splits[0], sizeof(CollSplit)*b.splits.size());
	memcpy(t->map, &b.order[0], 2*n);
	return t;
}

//
// Primitive tests: float32 ports of the game's RtIntersection / RtBary (SPEC_COLLISION 2.5-2.7, 5), same operation
// order, so x87 single precision results are reproduced to the ulp. Decompilation-informed (MFL_NOTES "Collision").
//

static uint32 fbits(float32 f) { uint32 u; memcpy(&u, &f, 4); return u; }
static bool fneg(float32 f) { return (int32)fbits(f) < 0; }
// the game uses the 12 bit _rwInvSqrt table (|n| = 1 to ~1e-4); exact here. 0 -> 0 (degenerate triangle: zero normal)
static float32 invSqrt(float32 x) { return x != 0.0f ? 1.0f/sqrtf(x) : 0.0f; }

// one sided (front face, det > 1e-8), borders inclusive with 1e-5 relative slack. *dist = t'/det; when u and v pass
// but t' fails it is left holding the raw t' (as the original)
static bool
lineTri(const float32 *o, const float32 *d, const float32 *a, const float32 *b, const float32 *c, float32 *dist)
{
	float32 e1x = b[0]-a[0], e1y = b[1]-a[1], e1z = b[2]-a[2];
	float32 e2x = c[0]-a[0], e2y = c[1]-a[1], e2z = c[2]-a[2];
	float32 px = e2z*d[1] - e2y*d[2], py = e2x*d[2] - e2z*d[0], pz = e2y*d[0] - e2x*d[1];
	float32 det = pz*e1z + px*e1x + py*e1y;
	if(!(1e-8f < det)) return false;
	float32 eps = det * -1e-5f, hi = det - eps;
	float32 sx = o[0]-a[0], sy = o[1]-a[1], sz = o[2]-a[2];
	float32 u = sz*pz + sx*px + sy*py;
	if(!(eps <= u && u <= hi)) return false;
	float32 qx = sy*e1z - sz*e1y, qy = sz*e1x - sx*e1z, qz = sx*e1y - sy*e1x;
	float32 v = qz*d[2] + qx*d[0] + qy*d[1];
	if(!(eps <= v)) return false;
	float32 uv = u + v;
	if(!(uv <= hi)) return false;
	float32 t = qz*e2z + qx*e2x + qy*e2y;
	if(dist) *dist = t;
	if(!(eps <= t && t <= hi)) return false;
	if(dist) *dist = t/det;
	return true;
}

// RtIntersectionBBoxTriangle; B = {sup[3], inf[3]}
static uint32
outcode(const float32 *B, const float32 *v)
{
	uint32 c = 0;
	for(int a = 0; a < 3; a++){
		if(v[a] <= B[a]){ if(!(B[3+a] <= v[a])) c |= 2 << (2*a); }
		else c |= 1 << (2*a);
	}
	return c;
}

// edge P..Q against the box faces in `faces` (strict)
static bool
edgeFaces(const float32 *B, const float32 *P, const float32 *Q, uint32 faces)
{
	float32 d[3] = { Q[0]-P[0], Q[1]-P[1], Q[2]-P[2] };
	float32 pinf[3] = { P[0]-B[3], P[1]-B[4], P[2]-B[5] };
	float32 psup[3] = { P[0]-B[0], P[1]-B[1], P[2]-B[2] };
	for(int A = 0; A < 3; A++)
		for(int side = 0; side < 2; side++){
			if(!(faces & ((1u << side) << (2*A)))) continue;
			float32 fW = side == 0 ? psup[A] : pinf[A];
			int Bx = (A+1)%3, Cx = (A+2)%3;
			float32 mB = fW*d[Bx], mC = fW*d[Cx];
			if(0.0f <= d[A]){
				if(psup[Bx]*d[A] < mB && mB < pinf[Bx]*d[A] && psup[Cx]*d[A] < mC && mC < pinf[Cx]*d[A]) return true;
			}else{
				if(mB < psup[Bx]*d[A] && pinf[Bx]*d[A] < mB && mC < psup[Cx]*d[A] && pinf[Cx]*d[A] < mC) return true;
			}
		}
	return false;
}

static bool
boxTri(const float32 *B, const float32 *v0, const float32 *v1, const float32 *v2)
{
	uint32 c0 = outcode(B, v0); if(c0 == 0) return true;
	uint32 c1 = outcode(B, v1); if(c1 == 0) return true;
	uint32 c2 = outcode(B, v2); if(c2 == 0) return true;
	if(c0 & c1 & c2) return false;
	if((c0 & c1) == 0 && edgeFaces(B, v0, v1, c0 ^ c1)) return true;
	if((c0 & c2) == 0 && edgeFaces(B, v0, v2, c0 ^ c2)) return true;
	if((c2 & c1) == 0 && edgeFaces(B, v1, v2, c2 ^ c1)) return true;
	// one box diagonal (picked by the normal's signs) against the triangle
	float32 f3 = v1[0]-v0[0], f4 = v1[1]-v0[1], f5 = v1[2]-v0[2];
	float32 f6 = v2[0]-v0[0], f7 = v2[1]-v0[1], f8 = v2[2]-v0[2];
	float32 n9 = f8*f4 - f7*f5, n10 = f6*f5 - f8*f3, n11 = f7*f3 - f6*f4;
	float32 f12 = B[0] - B[3], f13 = B[3] - v0[0];
	float32 l2c, f1, l28, f2;
	if(fneg(n10) != fneg(n9)){ l2c = B[4] - B[1]; f1 = B[1]; } else { l2c = B[1] - B[4]; f1 = B[4]; }
	f1 = f1 - v0[1];
	if(fneg(n9) != fneg(n11)){ l28 = B[5] - B[2]; f2 = B[2]; } else { l28 = B[2] - B[5]; f2 = B[5]; }
	f2 = f2 - v0[2];
	float32 f14 = -(l28*n11 + f12*n9 + l2c*n10);
	float32 g9 = f2*n11 + f13*n9 + f1*n10;
	float32 a9 = l28*f1 - f2*l2c, a10 = f2*f12 - l28*f13, a11 = l2c*f13 - f1*f12;
	float32 g6 = a11*f8 + a9*f6 + a10*f7;
	float32 g3 = -(a11*f5 + a9*f3 + a10*f4);
	if(0.0f <= f14)
		return 0.0f < g9 && g9 < f14 && 0.0f < g6 && g6 < f14 && 0.0f < g3 && g6 + g3 < f14;
	return g9 < 0.0f && f14 < g9 && g6 < 0.0f && f14 < g6 && g3 < 0.0f && f14 < g6 + g3;
}

// RtIntersectionSphereTriangle; S = {centre, radius}. Out: unit normal, *dist = signed plane distance of the centre
static bool
sphereTri(const float32 *S, const float32 *p2, const float32 *p3, const float32 *p4, float32 *n, float32 *dist)
{
	float32 r = S[3];
	float32 A[3], Bv[3], C[3];
	for(int a = 0; a < 3; a++){ A[a] = p2[a]-S[a]; Bv[a] = p3[a]-S[a]; C[a] = p4[a]-S[a]; }
	for(int a = 0; a < 3; a++){
		int neg = fneg(C[a]) + fneg(Bv[a]) + fneg(A[a]);
		if(neg == 0){
			if(!fneg(C[a]-r) && !fneg(Bv[a]-r) && !fneg(A[a]-r)) return false;
		}else if(neg == 3){
			if(fneg(r+C[a]) && fneg(r+Bv[a]) && fneg(r+A[a])) return false;
		}
	}
	float32 n0 = (p4[2]-p2[2])*(p3[1]-p2[1]) - (p4[1]-p2[1])*(p3[2]-p2[2]);
	float32 n1 = (p4[0]-p2[0])*(p3[2]-p2[2]) - (p4[2]-p2[2])*(p3[0]-p2[0]);
	float32 n2 = (p4[1]-p2[1])*(p3[0]-p2[0]) - (p4[0]-p2[0])*(p3[1]-p2[1]);
	float32 l = n2*n2 + n0*n0 + n1*n1;
	if(l <= 0.0f) return false;
	float32 iv = invSqrt(l);
	n[0] = iv*n0; n[1] = n1*iv; n[2] = n2*iv;
	float32 pd = n[2]*A[2] + n[0]*A[0] + n[1]*A[1];
	if(pd < -r || r < pd) return false;
	*dist = -pd;
	float32 rr = r*r;
	float32 aa = A[2]*A[2] + A[0]*A[0] + A[1]*A[1];
	if(aa <= rr) return true;
	float32 bb = Bv[2]*Bv[2] + Bv[0]*Bv[0] + Bv[1]*Bv[1];
	if(bb <= rr) return true;
	float32 cc = C[2]*C[2] + C[0]*C[0] + C[1]*C[1];
	if(cc <= rr) return true;
	float32 ab = Bv[2]*A[2] + Bv[0]*A[0] + Bv[1]*A[1];
	float32 bc = C[2]*Bv[2] + C[0]*Bv[0] + C[1]*Bv[1];
	float32 ac = C[2]*A[2] + C[0]*A[0] + C[1]*A[1];
	// nearest vertex separated by its Voronoi region?
	if(bb <= aa && bb <= cc){
		if(bb < ab && bb < bc) return false;
	}else if(bb <= aa || cc < aa){
		if(!(ac <= cc) && !(bc <= cc)) return false;
	}else{
		if(!(ab <= aa) && !(ac <= aa)) return false;
	}
	// edges: foot of the perpendicular from the centre, separated if outside r and on the far side of the edge
	const float32 *P[3] = { A, Bv, C };
	for(int i = 0; i < 3; i++){
		const float32 *p = P[i], *q = P[(i+1)%3], *o = P[(i+2)%3];
		float32 ex = q[0]-p[0], ey = q[1]-p[1], ez = q[2]-p[2];
		float32 t = (ez*p[2] + ex*p[0] + p[1]*ey)/(ez*ez + ex*ex + ey*ey);
		float32 qx = p[0]-ex*t, qy = p[1]-ey*t, qz = p[2]-ez*t;
		float32 qq = qz*qz + qx*qx + qy*qy;
		if(rr < qq && qq < qz*o[2] + qx*o[0] + qy*o[1]) return false;
	}
	return true;
}

bool
intersectionLineTriangle(const V3d *ls, const V3d *ld, const V3d *v0, const V3d *v1, const V3d *v2, float32 *distance)
{
	return lineTri(&ls->x, &ld->x, &v0->x, &v1->x, &v2->x, distance);
}

bool
lineSphereIntersectionTest(const Line *line, const Sphere *sph, float32 *distance)
{
	float32 d[3] = { line->end.x-line->start.x, line->end.y-line->start.y, line->end.z-line->start.z };
	float32 il = invSqrt(d[2]*d[2] + d[1]*d[1] + d[0]*d[0]);	// _rwV3dNormalize returns 1/length
	for(int a = 0; a < 3; a++) d[a] *= il;
	float32 rr = sph->radius*sph->radius;
	float32 mx = sph->center.x-line->start.x, my = sph->center.y-line->start.y, mz = sph->center.z-line->start.z;
	float32 b = mz*d[2] + mx*d[0] + my*d[1];
	float32 m2 = mz*mz + mx*mx + my*my;
	if(m2 < rr || (0.0f < b && 0.0f <= rr - (m2 - b*b))){
		if(distance) *distance = b*il;
		return true;
	}
	return false;
}

bool
baryGetTransform(Matrix *mat, float32 *scale, const V3d *v0, const V3d *v1, const V3d *v2)
{
	static_assert(sizeof(Matrix) == 64, "RwMatrix layout");
	const float32 *p3 = &v0->x, *p4 = &v1->x, *p5 = &v2->x;
	float32 m[16], k = 0.0f;
	float32 f1 = p4[0]-p5[0], f2 = p4[1]-p5[1], f3 = p4[2]-p5[2];
	float32 f4 = p5[0]-p3[0], f5 = p5[1]-p3[1], f6 = p5[2]-p3[2];
	float32 nx = f6*f2 - f5*f3, ny = f4*f3 - f6*f1, nz = f5*f1 - f4*f2;	// (v1-v2)x(v2-v0)
	float32 l2 = nz*nz + nx*nx + ny*ny;
	bool deg = l2 <= 2.4414062e-4f;
	float32 sc = 0.0f;
	if(!deg){ sc = sqrtf(l2); k = 1.0f/sqrtf(l2); }
	if(scale) *scale = sc;
	nx *= k; ny = k*ny; nz = k*nz;
	float32 a0 = (f2*nz - ny*f3)*k, a4 = (nx*f3 - nz*f1)*k, a8 = (ny*f1 - nx*f2)*k;	// ((v1-v2) x n)*k
	float32 a1 = (nz*f5 - ny*f6)*k, a5 = (nx*f6 - nz*f4)*k, a9 = (ny*f4 - nx*f5)*k;	// ((v2-v0) x n)*k
	float32 cx = (p3[0]+p4[0]+p5[0])*0.33333334f, cy = (p5[1]+p3[1]+p4[1])*0.33333334f, cz = (p5[2]+p3[2]+p4[2])*0.33333334f;
	m[0] = a0; m[1] = a1; m[2] = -(a1+a0); m[3] = nx;
	m[4] = a4; m[5] = a5; m[6] = -(a5+a4); m[7] = ny;
	m[8] = a8; m[9] = a9; m[10] = -(a9+a8); m[11] = nz;
	m[12] = -(p5[2]*a8 + a0*p5[0] + p5[1]*a4);
	m[13] = -(p3[2]*a9 + a1*p3[0] + p3[1]*a5);
	m[14] = 1.0f - (m[13] + m[12]);
	m[15] = -(cz*nz + cx*nx + cy*ny);
	memcpy(mat, m, sizeof(m));
	return !deg;
}

//
// Queries: ports of the game's walkers (SPEC_COLLISION 2.1-2.4, 3)
//

// callback plumbing; the callbacks always get the caller's Intersection
struct Visit
{
	Intersection *isec;
	const V3d *verts;
	const Triangle *tris;
	WorldSector *sector;	// nil: geometry callback
	WorldIntersectionCB wcb;
	GeometryIntersectionCB gcb;
	void *data;

	const float32 *v(uint32 idx, int k) const { return &verts[tris[idx].v[k]].x; }
};

// triangle normal as the walkers compute it
static void
triNormal(const float32 *a, const float32 *b, const float32 *c, float32 *n)
{
	float32 e2x = c[0]-a[0], e2y = c[1]-a[1];
	float32 nx = (c[2]-a[2])*(b[1]-a[1]) - e2y*(b[2]-a[2]);
	float32 ny = e2x*(b[2]-a[2]) - (c[2]-a[2])*(b[0]-a[0]);
	float32 nz = e2y*(b[0]-a[0]) - e2x*(b[1]-a[1]);
	float32 r = invSqrt(nz*nz + nx*nx + ny*ny);
	n[0] = nx*r; n[1] = ny*r; n[2] = r*nz;
}

// false = stop
static bool
report(const Visit *vis, uint32 idx, float32 dist)
{
	const Triangle *t = &vis->tris[idx];
	CollisionTriangle ct;
	for(int k = 0; k < 3; k++)
		ct.vertices[k] = (V3d*)&vis->verts[t->v[k]];
	triNormal(&ct.vertices[0]->x, &ct.vertices[1]->x, &ct.vertices[2]->x, &ct.normal.x);
	ct.point = *ct.vertices[0];
	ct.index = idx;
	CollisionTriangle *r = vis->sector ?
		vis->wcb(vis->isec, vis->sector, &ct, dist, vis->data) :
		vis->gcb(vis->isec, &ct, dist, vis->data);
	return r != nil;
}

// the walkers start from this node: the root split, or for numSplits == 0 a leaf of (u8)numEntries entries
static CollNode
rootNode(const CollisionTree *t)
{
	CollNode n;
	n.type = 0; n.index = 0; n.value = 0.0f;
	n.contents = t->numSplits == 0 ? (uint8)t->numEntries : (uint8)CollisionTree::SPLIT;
	return n;
}

// line parameters: start[3], delta[3], 1/delta[3] (0 for 0), pad[3] = 1e-6*max(|start|,|end|)
struct LineQ { float32 L[12]; };

static LineQ
lineSetup(const Line &line)
{
	LineQ q;
	const float32 *s = &line.start.x, *e = &line.end.x;
	for(int a = 0; a < 3; a++){
		q.L[a] = s[a];
		q.L[3+a] = e[a] - s[a];
		q.L[6+a] = q.L[3+a] == 0.0f ? 0.0f : 1.0f/q.L[3+a];
		float32 m = fabsf(s[a]) < fabsf(e[a]) ? e[a] : s[a];
		q.L[9+a] = fabsf(m) * 1e-6f;
	}
	return q;
}

template<class N> struct LineE { N n; float32 t0, t1; };

// one split of the near-to-far line walk (plane tree and kd tree alike): the child on the start side first, the
// t interval clipped per child. lv/rv = left/right child bounds on axis a.
template<class N> static void
lineSplit(const float32 *L, int a, float32 lv, float32 rv, N left, N right, LineE<N> &cur, std::vector<LineE<N> > &st)
{
	float32 dd = L[3+a], pd = L[9+a], inv = L[6+a];
	float32 p1 = dd*cur.t1 + L[a], p0 = dd*cur.t0 + L[a];
	float32 rlo = rv - pd, lhi = lv + pd;
	if(dd <= 0.0f){	// towards -axis: right is near
		if(!(p1 <= lhi)){ cur.n = right; return; }
		if(!(rlo <= p0)){ cur.n = left; return; }
		LineE<N> nearE = { right, cur.t0, cur.t1 }, farE = { left, cur.t0, cur.t1 };
		if(p1 < rlo) nearE.t1 = (rlo - p1)*inv + nearE.t1;
		if(lhi < p0) farE.t0 = (lhi - p0)*inv + farE.t0;
		st.push_back(farE);
		cur = nearE;
	}else{
		if(!(rlo <= p1)){ cur.n = left; return; }
		if(!(p0 <= lhi)){ cur.n = right; return; }
		LineE<N> nearE = { left, cur.t0, cur.t1 }, farE = { right, cur.t0, cur.t1 };
		if(lhi < p1) nearE.t1 = (lhi - p1)*inv + nearE.t1;
		if(p0 < rlo) farE.t0 = (rlo - p0)*inv + farE.t0;
		st.push_back(farE);
		cur = nearE;
	}
}

static bool
lineEntry(const Visit *vis, const float32 *L, uint32 idx)
{
	float32 dist;
	if(!lineTri(L, L+3, vis->v(idx,0), vis->v(idx,1), vis->v(idx,2), &dist))
		return true;
	return report(vis, idx, dist);
}

// kd tree, near to far, starting with the interval (t0,t1); false = stopped
static bool
kdLine(const CollisionTree *t, const float32 *L, float32 t0, float32 t1, const Visit *vis)
{
	CollNode root = rootNode(t);
	typedef LineE<const CollNode*> E;
	std::vector<E> st;
	E first = { &root, t0, t1 };
	st.push_back(first);
	while(!st.empty()){
		E cur = st.back();
		st.pop_back();
		while(cur.n->contents == CollisionTree::SPLIT){
			const CollSplit *s = &t->splits[cur.n->index];
			lineSplit<const CollNode*>(L, (s->left.type & 0xC) >> 2, s->left.value, s->right.value, &s->left, &s->right, cur, st);
		}
		for(uint32 k = cur.n->index; k < (uint32)cur.n->index + cur.n->contents; k++)
			if(!lineEntry(vis, L, t->entry(k)))
				return false;
	}
	return true;
}

// Q = {outer sup[3], outer inf[3], inner sup[3], inner inf[3]}; the outer box culls, a node whose region is inside the
// inner box (flags 0) has its triangles reported untested
static uint32
initFlags(const CollisionTree *t, const float32 *Q)
{
	const float32 *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x;
	uint32 f = 0;
	for(int a = 0; a < 3; a++){
		if(ti[a] < Q[9+a]) f |= 1u << (2*a);
		if(Q[6+a] < ts[a]) f |= 2u << (2*a);
	}
	return f;
}

static bool
disjoint(const CollisionTree *t, const float32 *B)	// B = {sup, inf}
{
	const float32 *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x;
	for(int a = 0; a < 3; a++)
		if(ti[a] > B[a] || ts[a] < B[3+a]) return true;
	return false;
}

// box style kd walk, left first; leaf(begin, end, flags) returns false to stop
template<class F> static bool
kdBox(const CollisionTree *t, const float32 *Q, F leaf)
{
	CollNode root = rootNode(t);
	struct E { const CollNode *n; uint32 f; };
	std::vector<E> st;
	E first = { &root, initFlags(t, Q) };
	st.push_back(first);
	while(!st.empty()){
		E cur = st.back();
		st.pop_back();
		for(;;){
			if(cur.n->contents != CollisionTree::SPLIT){
				if(!leaf((uint32)cur.n->index, (uint32)cur.n->index + cur.n->contents, cur.f))
					return false;
				break;
			}
			if(cur.f == 0){	// inside: the contiguous entry range from the leftmost to the rightmost leaf (u16 maths)
				const CollSplit *l = &t->splits[cur.n->index], *r = l;
				while(l->left.contents == CollisionTree::SPLIT) l = &t->splits[l->left.index];
				while(r->right.contents == CollisionTree::SPLIT) r = &t->splits[r->right.index];
				uint32 b = l->left.index;
				uint32 len = (uint16)((uint16)(r->right.contents + r->right.index) - b);
				uint32 e = std::min(b + len, t->numEntries);	// hardening: only differs for a malformed tree
				if(!leaf(b, e, 0))
					return false;
				break;
			}
			const CollSplit *s = &t->splits[cur.n->index];
			int ao = s->left.type & 0xC, a = ao >> 2, sh = ao >> 1;
			if(s->right.value <= Q[a]){
				E right = { &s->right, cur.f };
				if(Q[3+a] <= s->left.value){
					E left = { &s->left, cur.f };
					if(s->left.value <= Q[6+a]) left.f &= ~(2u << sh);
					if(Q[9+a] <= s->right.value) right.f &= ~(1u << sh);
					st.push_back(right);
					cur = left;
				}else
					cur = right;
			}else
				cur.n = &s->left;
		}
	}
	return true;
}

struct SphereQ { float32 S[4]; float32 Q[12]; float32 ir; };

static SphereQ
sphereSetup(const Sphere &sp)
{
	SphereQ q;
	const float32 *c = &sp.center.x;
	float32 r = sp.radius, ri = r*0.57f;
	q.ir = 1.0f/r;
	for(int a = 0; a < 3; a++){
		q.S[a] = c[a];
		q.Q[a] = r + c[a]; q.Q[3+a] = c[a] - r; q.Q[6+a] = c[a] + ri; q.Q[9+a] = c[a] - ri;
	}
	q.S[3] = r;
	return q;
}

// tree bbox within r of the centre and overlapping the outer box
static bool
sphereNearTree(const CollisionTree *t, const SphereQ &q)
{
	const float32 *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x, *c = q.S;
	float32 dd[3];
	for(int a = 0; a < 3; a++){
		if(ti[a] <= c[a]){ dd[a] = 0.0f; if(ts[a] < c[a]) dd[a] = c[a] - ts[a]; }
		else dd[a] = ti[a] - c[a];
	}
	if(!(dd[2]*dd[2] + dd[1]*dd[1] + dd[0]*dd[0] <= q.S[3]*q.S[3]))
		return false;
	return !disjoint(t, q.Q);
}

// distance = signed plane distance of the centre in units of the radius. inside = reported without the test
static bool
sphereEntry(const Visit *vis, const SphereQ &q, uint32 idx, bool inside)
{
	const float32 *v0 = vis->v(idx,0), *c = q.S;
	float32 n[3], dist;
	if(inside){
		triNormal(v0, vis->v(idx,1), vis->v(idx,2), n);
		dist = (c[2]-v0[2])*n[2] + (c[0]-v0[0])*n[0] + (c[1]-v0[1])*n[1];
	}else if(!sphereTri(q.S, v0, vis->v(idx,1), vis->v(idx,2), n, &dist))
		return true;
	return report(vis, idx, dist*q.ir);
}

static bool
sphereTree(const CollisionTree *t, const SphereQ &q, const Visit *vis)
{
	if(!sphereNearTree(t, q))
		return true;
	return kdBox(t, q.Q, [&](uint32 b, uint32 e, uint32 f) -> bool {
		for(uint32 k = b; k < e; k++)
			if(!sphereEntry(vis, q, t->entry(k), f == 0))
				return false;
		return true;
	});
}

// box distance is always 0
static bool
boxTree(const CollisionTree *t, const float32 *Q, const Visit *vis)
{
	if(disjoint(t, Q))
		return true;
	return kdBox(t, Q, [&](uint32 b, uint32 e, uint32 f) -> bool {
		for(uint32 k = b; k < e; k++){
			uint32 idx = t->entry(k);
			if((f == 0 || boxTri(Q, vis->v(idx,0), vis->v(idx,1), vis->v(idx,2))) && !report(vis, idx, 0.0f))
				return false;
		}
		return true;
	});
}

static void
boxSetup(const BBox &box, float32 *Q)
{
	memcpy(Q, &box.sup.x, 24);
	memcpy(Q+6, &box.sup.x, 24);
}

//
// World: plane tree, then the sector's kd tree. Sectors without a collision tree are skipped (as the original).
//

static void
worldLine(World *w, const Line &line, Visit *vis)
{
	LineQ q = lineSetup(line);
	typedef LineE<BspNode*> E;
	std::vector<E> st;
	E first = { w->root, 0.0f, 1.0f };
	st.push_back(first);
	while(!st.empty()){
		E cur = st.back();
		st.pop_back();
		while(!cur.n->isLeaf){
			PlaneSector *p = (PlaneSector*)cur.n;
			lineSplit<BspNode*>(q.L, p->type >> 2, p->leftValue, p->rightValue, p->left, p->right, cur, st);
		}
		WorldSector *s = (WorldSector*)cur.n;
		if(s->collTree == nil)
			continue;
		vis->sector = s; vis->verts = s->vertices; vis->tris = s->triangles;
		if(!kdLine(s->collTree, q.L, cur.t0, cur.t1, vis))
			return;
	}
}

// plane tree with the outer box, left first; sector(s) returns false to stop
template<class F> static void
worldBoxWalk(World *w, const float32 *Q, Visit *vis, F sector)
{
	std::vector<BspNode*> st(1, w->root);
	while(!st.empty()){
		BspNode *n = st.back();
		st.pop_back();
		while(!n->isLeaf){
			PlaneSector *p = (PlaneSector*)n;
			int a = p->type >> 2;
			if(p->rightValue <= Q[a]){
				if(Q[3+a] <= p->leftValue){ st.push_back(p->right); n = p->left; }
				else n = p->right;
			}else
				n = p->left;
		}
		WorldSector *s = (WorldSector*)n;
		if(s->collTree == nil)
			continue;
		vis->sector = s; vis->verts = s->vertices; vis->tris = s->triangles;
		if(!sector(s->collTree))
			return;
	}
}

static void
worldSphere(World *w, const Sphere &sp, Visit *vis)
{
	SphereQ q = sphereSetup(sp);
	worldBoxWalk(w, q.Q, vis, [&](const CollisionTree *t) -> bool { return sphereTree(t, q, vis); });
}

static void
worldBox(World *w, const BBox &box, Visit *vis)
{
	float32 Q[12];
	boxSetup(box, Q);
	worldBoxWalk(w, Q, vis, [&](const CollisionTree *t) -> bool { return boxTree(t, Q, vis); });
}

World*
collisionWorldForAllIntersections(World *world, Intersection *isec, WorldIntersectionCB cb, void *data)
{
	if(world == nil || isec == nil || cb == nil){
		RWERROR((ERR_GENERAL, "invalid intersection query"));
		return nil;
	}
	if(world->root == nil)
		return world;
	Visit vis = { isec, nil, nil, nil, cb, nil, data };
	switch(isec->type){
	case INTERSECTLINE: worldLine(world, isec->t.line, &vis); break;
	case INTERSECTSPHERE: worldSphere(world, isec->t.sphere, &vis); break;
	case INTERSECTBOX: worldBox(world, isec->t.box, &vis); break;
	case INTERSECTATOMIC:
		if(isec->t.atomic)
			worldSphere(world, *isec->t.atomic->getWorldBoundingSphere(), &vis);
		break;
	}
	return world;
}

CollisionTree*
collisionWorldSectorQueryData(WorldSector *s)
{
	return s->collTree;
}

void
collisionWorldSectorDestroyData(WorldSector *s)
{
	if(s->collTree){
		s->collTree->destroy();
		s->collTree = nil;
	}
}

//
// Geometry plugin
//

static int32 collisionOffset;
struct CollisionGeo { CollisionTree *tree; };
#define COLLGEO(g) PLUGINOFFSET(CollisionGeo, g, collisionOffset)

static void*
createCollGeo(void *object, int32 offset, int32)
{
	COLLGEO(object)->tree = nil;
	return object;
}

static void*
destroyCollGeo(void *object, int32, int32)
{
	CollisionGeo *c = COLLGEO(object);
	if(c->tree)
		c->tree->destroy();
	c->tree = nil;
	return object;
}

static void*
copyCollGeo(void *dst, void *src, int32, int32)
{
	CollisionGeo *s = COLLGEO(src), *d = COLLGEO(dst);
	d->tree = s->tree ? s->tree->clone() : nil;
	return dst;
}

static Stream*
readCollGeo(Stream *stream, int32 length, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	uint8 *buf = rwNewT(uint8, length, MEMDUR_EVENT | ID_COLLISION);
	if(buf == nil)
		return nil;
	stream->read8(buf, length);
	CollisionTree *t = CollisionTree::fromBlob(buf, length, geo->numTriangles);
	rwFree(buf);
	if(t == nil)
		return nil;
	CollisionGeo *c = COLLGEO(object);
	if(c->tree)
		c->tree->destroy();
	c->tree = t;
	return stream;
}

static Stream*
writeCollGeo(Stream *stream, int32 length, void *object, int32, int32)
{
	CollisionGeo *c = COLLGEO(object);
	uint8 *buf = rwNewT(uint8, length, MEMDUR_EVENT | ID_COLLISION);
	if(buf == nil)
		return nil;
	c->tree->toBlob(buf);
	stream->write8(buf, length);
	rwFree(buf);
	return stream;
}

static int32
getSizeCollGeo(void *object, int32, int32)
{
	CollisionGeo *c = COLLGEO(object);
	return c->tree ? c->tree->blobSize() : 0;
}

void
registerCollisionPlugin(void)
{
	collisionOffset = Geometry::registerPlugin(sizeof(CollisionGeo), ID_COLLISION,
		createCollGeo, destroyCollGeo, copyCollGeo);
	Geometry::registerPluginStream(ID_COLLISION, readCollGeo, writeCollGeo, getSizeCollGeo);
}

CollisionTree*
collisionGeometryQueryData(Geometry *geo)
{
	return COLLGEO(geo)->tree;
}

void
collisionGeometryDestroyData(Geometry *geo)
{
	CollisionGeo *c = COLLGEO(geo);
	if(c->tree)
		c->tree->destroy();
	c->tree = nil;
}

bool
collisionGeometryBuildData(Geometry *geo, const CollisionBuildParams *params)
{
	CollisionBuildParams def;
	if(params == nil){
		collisionBuildParamInitDefaults(&def);
		params = &def;
	}
	if(geo->numMorphTargets < 1 || geo->morphTargets[0].vertices == nil){
		RWERROR((ERR_GENERAL, "geometry has no vertices"));
		return false;
	}
	for(int32 i = 0; i < geo->numTriangles; i++)
		for(int k = 0; k < 3; k++)
			if(geo->triangles[i].v[k] >= geo->numVertices){
				RWERROR((ERR_GENERAL, "triangle index out of range"));
				return false;
			}
	CollisionTree *t = CollisionTree::build(geo->morphTargets[0].vertices, geo->triangles,
		geo->numTriangles, params->maxEntriesPerLeaf);
	if(t == nil)
		return false;
	collisionGeometryDestroyData(geo);
	COLLGEO(geo)->tree = t;
	return true;
}

//
// Geometry / atomic queries (local space). A geometry without a tree, or a morphing atomic, is brute forced over all
// triangles (unlike world sectors); with a tree the kd walk starts at the pseudo root.
//

static void
geoLine(const CollisionTree *t, int32 numTris, const Line &line, const Visit *vis)
{
	LineQ q = lineSetup(line);
	const float32 *L = q.L;
	if(t == nil){
		for(int32 i = 0; i < numTris; i++)
			if(!lineEntry(vis, L, i))
				return;
		return;
	}
	// clip the segment to the padded tree box first
	float32 t0 = 0.0f, t1 = 1.0f;
	const float32 *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x;
	for(int a = 0; a < 3; a++){
		float32 lo = ti[a] - L[9+a], hi = ts[a] + L[9+a];
		if(L[6+a] == 0.0f){
			if(L[a] < lo || L[a] > hi)
				return;
			continue;
		}
		float32 mn = (lo - L[a])*L[6+a], mx = (hi - L[a])*L[6+a];
		if(mx < mn) std::swap(mn, mx);
		if(mx < t0 || t1 < mn)
			return;
		if(t0 < mn) t0 = mn;
		if(mx < t1) t1 = mx;
	}
	kdLine(t, L, t0, t1, vis);
}

static void
geoSphere(const CollisionTree *t, int32 numTris, const Sphere &sp, const Visit *vis)
{
	SphereQ q = sphereSetup(sp);
	if(t == nil){
		for(int32 i = 0; i < numTris; i++)
			if(!sphereEntry(vis, q, i, false))
				return;
		return;
	}
	sphereTree(t, q, vis);
}

static void
geoBox(const CollisionTree *t, int32 numTris, const BBox &box, const Visit *vis)
{
	float32 Q[12];
	boxSetup(box, Q);
	if(t == nil){
		for(int32 i = 0; i < numTris; i++)
			if(boxTri(Q, vis->v(i,0), vis->v(i,1), vis->v(i,2)) && !report(vis, i, 0.0f))
				return;
		return;
	}
	boxTree(t, Q, vis);
}

Geometry*
collisionGeometryForAllIntersections(Geometry *geo, Intersection *isec, GeometryIntersectionCB cb, void *data)
{
	if(geo == nil || isec == nil || cb == nil){
		RWERROR((ERR_GENERAL, "invalid intersection query"));
		return nil;
	}
	if(geo->numMorphTargets < 1)
		return geo;
	Visit vis = { isec, geo->morphTargets[0].vertices, geo->triangles, nil, nil, cb, data };
	const CollisionTree *t = COLLGEO(geo)->tree;
	switch(isec->type){
	case INTERSECTLINE: geoLine(t, geo->numTriangles, isec->t.line, &vis); break;
	case INTERSECTSPHERE: geoSphere(t, geo->numTriangles, isec->t.sphere, &vis); break;
	case INTERSECTBOX: geoBox(t, geo->numTriangles, isec->t.box, &vis); break;
	case INTERSECTATOMIC:	// the atomic's world bounding sphere, used as is
		if(isec->t.atomic)
			geoSphere(t, geo->numTriangles, *isec->t.atomic->getWorldBoundingSphere(), &vis);
		break;
	}
	return geo;
}

Atomic*
collisionAtomicForAllIntersections(Atomic *atomic, Intersection *isec, GeometryIntersectionCB cb, void *data)
{
	if(atomic == nil || atomic->geometry == nil || isec == nil || cb == nil)
		return nil;
	Geometry *geo = atomic->geometry;
	if(geo->numMorphTargets < 1)
		return atomic;
	Sphere sp;
	switch(isec->type){
	case INTERSECTLINE: break;
	case INTERSECTSPHERE: sp = isec->t.sphere; break;
	case INTERSECTATOMIC:
		if(isec->t.atomic == nil)
			return atomic;
		sp = *isec->t.atomic->getWorldBoundingSphere();
		break;
	default:	// box and the rest: nothing (as the original)
		return atomic;
	}
	Matrix inv;
	Frame *f = atomic->getFrame();
	if(f)
		Matrix::invertGeneral(&inv, f->getLTM());	// not invert(): stale orthonormal flags after a scale would pick the wrong path
	else
		inv.setIdentity();
	// librw has no morph interpolator: a morphing atomic uses morph target 0 (= start == end == 0), brute forced
	// like the original's MorphGeomForAllTriIntsec*
	const CollisionTree *t = geo->numMorphTargets > 1 ? nil : COLLGEO(geo)->tree;
	Visit vis = { isec, geo->morphTargets[0].vertices, geo->triangles, nil, nil, cb, data };
	if(isec->type == INTERSECTLINE){
		Line l = isec->t.line;
		V3d::transformPoints(&l.start, &l.start, 2, &inv);
		geoLine(t, geo->numTriangles, l, &vis);
	}else{
		V3d::transformPoints(&sp.center, &sp.center, 1, &inv);
		sp.radius *= length(inv.at);
		geoSphere(t, geo->numTriangles, sp, &vis);
	}
	return atomic;
}

}
