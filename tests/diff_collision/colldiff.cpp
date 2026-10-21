// Reviewer tool: (1) dump the fork's parsed collision trees (compare with orig_coll.py),
// (2) differential: original MFL.exe query model (ported from the decompile of WorldForAllTriIntsecLine 0x549370,
// WorldForAllTriIntsecBBox 0x549f10, WorldForAllTriIntsecSphere 0x54a4d0, RtIntersectionSphereTriangle 0x557a80,
// RtIntersectionBBoxTriangle 0x558220, RtLineSphereIntersectionTest 0x569110, RtBaryGetTransform 0x569610),
// float32 throughout, vs the fork's collisionWorldForAllIntersections / Rt functions.
// usage: colldiff dump FILE... | colldiff diff [-n N] FILE... | colldiff rt [-n N]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "rw.h"
using namespace rw;

namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}

static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
static double urand(void) { return (rnd() & 0xFFFFFF) / 16777216.0; }
static double uni(double a, double b) { return a + (b-a)*urand(); }
static double logu(double a, double b) { return exp(uni(log(a), log(b))); }

static uint32 fbits(float f) { uint32 u; memcpy(&u, &f, 4); return u; }
static bool fneg(float f) { return (int32)fbits(f) < 0; }

static World*
loadWorld(const char *path)
{
	FILE *f = fopen(path, "rb");
	if(!f) return nil;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	uint8 *d = (uint8*)malloc(n); fread(d, 1, n, f); fclose(f);
	StreamMemory in; in.open(d, n);
	uint32 len, ver;
	World *w = nil;
	if(findChunk(&in, ID_WORLD, &len, &ver)) w = World::streamRead(&in);
	in.close(); free(d);
	return w;
}

// ------------------------------------------------------------------ dump
static void
dumpWorld(World *w)
{
	for(int i = 0; i < w->numSectors; i++){
		WorldSector *s = w->sectors[i];
		CollisionTree *t = s->collTree;
		if(t == nil){ printf("s %d ntri %d notree\n", i, s->numTriangles); continue; }
		printf("s %d ntri %d flags %x inf %08x %08x %08x sup %08x %08x %08x nent %u nsplit %u\n", i, s->numTriangles, t->flags,
			fbits(t->bbox.inf.x), fbits(t->bbox.inf.y), fbits(t->bbox.inf.z),
			fbits(t->bbox.sup.x), fbits(t->bbox.sup.y), fbits(t->bbox.sup.z), t->numEntries, t->numSplits);
		printf(" n");
		for(uint32 k = 0; k < t->numSplits; k++){
			const CollNode *n[2] = { &t->splits[k].left, &t->splits[k].right };
			for(int j = 0; j < 2; j++) printf(" %x,%x,%x,%08x", n[j]->type, n[j]->contents, n[j]->index, fbits(n[j]->value));
		}
		printf("\n m");
		if(t->map) for(uint32 k = 0; k < t->numEntries; k++) printf(" %x", t->map[k]);
		else printf(" -");
		printf("\n");
	}
}

// ------------------------------------------------------------------ original model
struct Hit { int sector; int index; float dist; float n[3]; float pt[3]; };
typedef std::vector<Hit> Hits;

struct OCtx { World *w; std::map<WorldSector*, int> sidx; Hits *out; };

static float invsqrtf_(float x) { return x != 0.0f ? 1.0f/sqrtf(x) : 0.0f; }	// game: 12 bit table (_rwInvSqrt)

static void
report(OCtx &c, WorldSector *s, uint32 idx, float dist)
{
	const Triangle &t = s->triangles[idx];
	const float *a = &s->vertices[t.v[0]].x, *b = &s->vertices[t.v[1]].x, *cc = &s->vertices[t.v[2]].x;
	float e2x = cc[0]-a[0], e2y = cc[1]-a[1];
	float nx = (cc[2]-a[2])*(b[1]-a[1]) - e2y*(b[2]-a[2]);
	float ny = e2x*(b[2]-a[2]) - (cc[2]-a[2])*(b[0]-a[0]);
	float nz = e2y*(b[0]-a[0]) - e2x*(b[1]-a[1]);
	float r = invsqrtf_(nz*nz + nx*nx + ny*ny);
	Hit h; h.sector = c.sidx[s]; h.index = idx; h.dist = dist;
	h.n[0] = nx*r; h.n[1] = ny*r; h.n[2] = r*nz;
	h.pt[0] = a[0]; h.pt[1] = a[1]; h.pt[2] = a[2];
	c.out->push_back(h);
}

// inline line/triangle test of WorldForAllTriIntsecLine (== RtIntersectionLineTriangle 0x557860)
static bool
origLineTri(const float *o, const float *d, const float *a, const float *b, const float *c, float *dist)
{
	float e1x = b[0]-a[0], e1y = b[1]-a[1], e1z = b[2]-a[2];
	float e2x = c[0]-a[0], e2y = c[1]-a[1], e2z = c[2]-a[2];
	float px = e2z*d[1] - e2y*d[2], py = e2x*d[2] - e2z*d[0], pz = e2y*d[0] - e2x*d[1];
	float det = pz*e1z + px*e1x + py*e1y;
	if(!(1e-8f < det)) return false;
	float eps = det * -1e-5f, hi = det - eps;
	float sx = o[0]-a[0], sy = o[1]-a[1], sz = o[2]-a[2];
	float u = sz*pz + sx*px + sy*py;
	if(!(eps <= u && u <= hi)) return false;
	float qx = sy*e1z - sz*e1y, qy = sz*e1x - sx*e1z, qz = sx*e1y - sy*e1x;
	float v = qz*d[2] + qx*d[0] + qy*d[1];
	if(!(eps <= v)) return false;
	float uv = u + v;
	if(!(uv <= hi)) return false;
	float t = qz*e2z + qx*e2x + qy*e2y;
	if(dist) *dist = t;
	if(!(eps <= t && t <= hi)) return false;
	if(dist) *dist = t/det;
	return true;
}

static const float *VX(WorldSector *s, uint32 idx, int k) { return &s->vertices[s->triangles[idx].v[k]].x; }

// pseudo root as built by the original (type byte left uninitialised there; unused for a split)
static CollNode rootNode(const CollisionTree *t)
{
	CollNode n; n.type = 0; n.index = 0; n.value = 0;
	n.contents = t->numSplits == 0 ? (uint8)t->numEntries : 0xFF;
	return n;
}

static bool	// false = callback stopped
origLineSector(OCtx &c, WorldSector *s, const float *L, float t0in, float t1in)
{
	const CollisionTree *t = s->collTree;
	if(t == nil) return true;	// sector without collision data: nothing
	CollNode root = rootNode(t);
	struct E { const CollNode *n; float t0, t1; };
	std::vector<E> st; st.push_back(E{ &root, t0in, t1in });
	const float *o = L, *d = L+3, *inv = L+6, *pad = L+9;
	while(!st.empty()){
		E cur = st.back(); st.pop_back();
		while(cur.n->contents == 0xFF){
			const CollSplit *pb = &t->splits[cur.n->index];
			int a = (pb->left.type & 0xC) >> 2;
			float dd = d[a], pd = pad[a];
			float p1 = dd*cur.t1 + o[a], p0 = dd*cur.t0 + o[a];
			float rlo = pb->right.value - pd, lhi = pb->left.value + pd;
			if(dd <= 0.0f){
				if(p1 <= lhi){
					if(rlo <= p0){
						E nearE{ &pb->right, cur.t0, cur.t1 };
						if(p1 < rlo) nearE.t1 = (rlo - p1)*inv[a] + nearE.t1;
						E farE{ &pb->left, cur.t0, cur.t1 };
						if(lhi < p0) farE.t0 = (lhi - p0)*inv[a] + farE.t0;
						st.push_back(farE); cur = nearE;
					}else cur.n = &pb->left;
				}else cur.n = &pb->right;
			}else{
				if(rlo <= p1){
					if(p0 <= lhi){
						E nearE{ &pb->left, cur.t0, cur.t1 };
						if(lhi < p1) nearE.t1 = (lhi - p1)*inv[a] + nearE.t1;
						E farE{ &pb->right, cur.t0, cur.t1 };
						if(p0 < rlo) farE.t0 = (rlo - p0)*inv[a] + farE.t0;
						st.push_back(farE); cur = nearE;
					}else cur.n = &pb->right;
				}else cur.n = &pb->left;
			}
		}
		uint32 b = cur.n->index, e = b + cur.n->contents;
		for(uint32 k = b; k < e; k++){
			uint32 idx = t->map ? t->map[k] : k;
			float dist;
			if(origLineTri(o, d, VX(s,idx,0), VX(s,idx,1), VX(s,idx,2), &dist)){
				report(c, s, idx, dist);
			}
		}
	}
	return true;
}

static void
origWorldLine(OCtx &c, const Line &line)
{
	float L[12];
	L[0] = line.start.x; L[1] = line.start.y; L[2] = line.start.z;
	L[3] = line.end.x - line.start.x; L[4] = line.end.y - line.start.y; L[5] = line.end.z - line.start.z;
	for(int a = 0; a < 3; a++) L[6+a] = L[3+a] == 0.0f ? 0.0f : 1.0f/L[3+a];
	const float *s = &line.start.x, *e = &line.end.x;
	for(int a = 0; a < 3; a++){ float m = fabsf(s[a]) < fabsf(e[a]) ? e[a] : s[a]; L[9+a] = fabsf(m) * 1e-6f; }
	struct E { BspNode *n; float t0, t1; };
	std::vector<E> st; st.push_back(E{ c.w->root, 0.0f, 1.0f });
	while(!st.empty()){
		E cur = st.back(); st.pop_back();
		while(!cur.n->isLeaf){
			PlaneSector *p = (PlaneSector*)cur.n;
			int a = p->type >> 2;
			float dd = L[3+a], pd = L[9+a], inv = L[6+a];
			float p1 = dd*cur.t1 + L[a], p0 = dd*cur.t0 + L[a];
			float rlo = p->rightValue - pd, lhi = p->leftValue + pd;
			if(dd <= 0.0f){
				if(p1 <= lhi){
					if(rlo <= p0){
						E nearE{ p->right, cur.t0, cur.t1 };
						if(p1 < rlo) nearE.t1 = (rlo - p1)*inv + nearE.t1;
						E farE{ p->left, cur.t0, cur.t1 };
						if(lhi < p0) farE.t0 = (lhi - p0)*inv + farE.t0;
						st.push_back(farE); cur = nearE;
					}else cur.n = p->left;
				}else cur.n = p->right;
			}else{
				if(rlo <= p1){
					if(p0 <= lhi){
						E nearE{ p->left, cur.t0, cur.t1 };
						if(lhi < p1) nearE.t1 = (lhi - p1)*inv + nearE.t1;
						E farE{ p->right, cur.t0, cur.t1 };
						if(p0 < rlo) farE.t0 = (rlo - p0)*inv + farE.t0;
						st.push_back(farE); cur = nearE;
					}else cur.n = p->right;
				}else cur.n = p->left;
			}
		}
		origLineSector(c, (WorldSector*)cur.n, L, cur.t0, cur.t1);
	}
}

// RtIntersectionBBoxTriangle 0x558220; box = {sup[3], inf[3]}
static uint32 outcode(const float *B, const float *v)
{
	uint32 c = 0;
	for(int a = 0; a < 3; a++){
		if(v[a] <= B[a]){ if(!(B[3+a] <= v[a])) c |= 2 << (2*a); }
		else c |= 1 << (2*a);
	}
	return c;
}
// edge P + s*d against the box faces in `faces`
static bool edgeFaces(const float *B, const float *P, const float *Q, uint32 faces)
{
	float d[3] = { Q[0]-P[0], Q[1]-P[1], Q[2]-P[2] };
	float pinf[3] = { P[0]-B[3], P[1]-B[4], P[2]-B[5] };	// fVar6..8
	float psup[3] = { P[0]-B[0], P[1]-B[1], P[2]-B[2] };	// fVar9..11
	for(int A = 0; A < 3; A++)
		for(int side = 0; side < 2; side++){
			if(!(faces & ((1u << side) << (2*A)))) continue;
			float fW = side == 0 ? psup[A] : pinf[A];
			int Bx = (A+1)%3, Cx = (A+2)%3;
			float mB = fW*d[Bx], mC = fW*d[Cx];
			if(0.0f <= d[A]){
				if(psup[Bx]*d[A] < mB && mB < pinf[Bx]*d[A] && psup[Cx]*d[A] < mC && mC < pinf[Cx]*d[A]) return true;
			}else{
				if(mB < psup[Bx]*d[A] && pinf[Bx]*d[A] < mB && mC < psup[Cx]*d[A] && pinf[Cx]*d[A] < mC) return true;
			}
		}
	return false;
}
static bool
origBoxTri(const float *B, const float *v0, const float *v1, const float *v2)
{
	uint32 c0 = outcode(B, v0); if(c0 == 0) return true;
	uint32 c1 = outcode(B, v1); if(c1 == 0) return true;
	uint32 c2 = outcode(B, v2); if(c2 == 0) return true;
	if(c0 & c1 & c2) return false;
	if((c0 & c1) == 0 && edgeFaces(B, v0, v1, c0 ^ c1)) return true;
	if((c0 & c2) == 0 && edgeFaces(B, v0, v2, c0 ^ c2)) return true;
	if((c2 & c1) == 0 && edgeFaces(B, v1, v2, c2 ^ c1)) return true;
	float f3 = v1[0]-v0[0], f4 = v1[1]-v0[1], f5 = v1[2]-v0[2];
	float f6 = v2[0]-v0[0], f7 = v2[1]-v0[1], f8 = v2[2]-v0[2];
	float n9 = f8*f4 - f7*f5, n10 = f6*f5 - f8*f3, n11 = f7*f3 - f6*f4;
	float f12 = B[0] - B[3], f13 = B[3] - v0[0];
	float l2c, f1, l28, f2;
	if(fneg(n10) != fneg(n9)){ l2c = B[4] - B[1]; f1 = B[1]; } else { l2c = B[1] - B[4]; f1 = B[4]; }
	f1 = f1 - v0[1];
	if(fneg(n9) != fneg(n11)){ l28 = B[5] - B[2]; f2 = B[2]; } else { l28 = B[2] - B[5]; f2 = B[5]; }
	f2 = f2 - v0[2];
	float f14 = -(l28*n11 + f12*n9 + l2c*n10);
	float g9 = f2*n11 + f13*n9 + f1*n10;
	if(0.0f <= f14){
		if(0.0f < g9 && g9 < f14){
			float a9 = l28*f1 - f2*l2c, a10 = f2*f12 - l28*f13, a11 = l2c*f13 - f1*f12;
			float g6 = a11*f8 + a9*f6 + a10*f7;
			if(0.0f < g6 && g6 < f14){
				float g3 = -(a11*f5 + a9*f3 + a10*f4);
				if(0.0f < g3 && g6 + g3 < f14) return true;
			}
		}
	}else if(g9 < 0.0f && f14 < g9){
		float a9 = l28*f1 - f2*l2c, a10 = f2*f12 - l28*f13, a11 = l2c*f13 - f1*f12;
		float g6 = a11*f8 + a9*f6 + a10*f7;
		if(g6 < 0.0f && f14 < g6){
			float g3 = -(a11*f5 + a9*f3 + a10*f4);
			if(g3 < 0.0f && f14 < g6 + g3) return true;
		}
	}
	return false;
}

// RtIntersectionSphereTriangle 0x557a80; S = {c, r}; out normal n, out dist (signed plane distance of c)
static bool
origSphereTri(const float *S, const float *p2, const float *p3, const float *p4, float *n, float *dist)
{
	float r = S[3];
	float A[3], Bv[3], C[3];
	for(int a = 0; a < 3; a++){ A[a] = p2[a]-S[a]; Bv[a] = p3[a]-S[a]; C[a] = p4[a]-S[a]; }
	for(int a = 0; a < 3; a++){
		int neg = fneg(C[a]) + fneg(Bv[a]) + fneg(A[a]);
		if(neg == 0){
			float x = C[a]-r, y = Bv[a]-r, z = A[a]-r;
			if(!fneg(x) && !fneg(y) && !fneg(z)) return false;
		}else if(neg == 3){
			float x = r+C[a], y = r+Bv[a], z = r+A[a];
			if(fneg(x) && fneg(y) && fneg(z)) return false;
		}
	}
	float n0 = (p4[2]-p2[2])*(p3[1]-p2[1]) - (p4[1]-p2[1])*(p3[2]-p2[2]);
	float n1 = (p4[0]-p2[0])*(p3[2]-p2[2]) - (p4[2]-p2[2])*(p3[0]-p2[0]);
	float n2 = (p4[1]-p2[1])*(p3[0]-p2[0]) - (p4[0]-p2[0])*(p3[1]-p2[1]);
	float l = n2*n2 + n0*n0 + n1*n1;
	if(l <= 0.0f) return false;
	float iv = invsqrtf_(l);
	n[0] = iv*n0; n[1] = n1*iv; n[2] = n2*iv;
	float pd = n[2]*A[2] + n[0]*A[0] + n[1]*A[1];
	if(pd < -r) return false;
	if(r < pd) return false;
	*dist = -pd;
	float rr = r*r;
	float aa = A[2]*A[2] + A[0]*A[0] + A[1]*A[1];
	if(aa <= rr) return true;
	float bb = Bv[2]*Bv[2] + Bv[0]*Bv[0] + Bv[1]*Bv[1];
	if(bb <= rr) return true;
	float cc = C[2]*C[2] + C[0]*C[0] + C[1]*C[1];
	if(cc <= rr) return true;
	float ab = Bv[2]*A[2] + Bv[0]*A[0] + Bv[1]*A[1];
	float bc = C[2]*Bv[2] + C[0]*Bv[0] + C[1]*Bv[1];
	float ac = C[2]*A[2] + C[0]*A[0] + C[1]*A[1];
	bool edges = false;
	if(bb <= aa){
		if(bb <= cc){
			if(bb < ab && bb < bc) return false;
			edges = true;
		}else goto Cn;
	}else{
		if(cc < aa) goto Cn;
		if(ab <= aa) edges = true;
		else if(!(ac <= aa)) return false;
		else edges = true;
	}
	goto Edges;
Cn:
	if(ac <= cc) goto Edges;
	if(!(bc <= cc)) return false;
Edges:
	(void)edges;
	{	// edge AB, C opposite
		float ex = Bv[0]-A[0], ey = Bv[1]-A[1], ez = Bv[2]-A[2];
		float t = (ez*A[2] + ex*A[0] + A[1]*ey)/(ez*ez + ex*ex + ey*ey);
		float qx = A[0]-ex*t, qy = A[1]-ey*t, qz = A[2]-ez*t;
		float qq = qz*qz + qx*qx + qy*qy;
		if(rr < qq && qq < qz*C[2] + qx*C[0] + qy*C[1]) return false;
	}
	{	// edge BC, A opposite
		float ex = C[0]-Bv[0], ey = C[1]-Bv[1], ez = C[2]-Bv[2];
		float t = (ez*Bv[2] + ex*Bv[0] + Bv[1]*ey)/(ez*ez + ex*ex + ey*ey);
		float qx = Bv[0]-ex*t, qy = Bv[1]-ey*t, qz = Bv[2]-ez*t;
		float qq = qz*qz + qx*qx + qy*qy;
		if(rr < qq && qq < qz*A[2] + qx*A[0] + qy*A[1]) return false;
	}
	{	// edge CA, B opposite
		float ex = A[0]-C[0], ey = A[1]-C[1], ez = A[2]-C[2];
		float t = (ez*C[2] + ex*C[0] + C[1]*ey)/(ez*ez + ex*ex + ey*ey);
		float qx = C[0]-ex*t, qy = C[1]-ey*t, qz = C[2]-ez*t;
		float qq = qz*qz + qx*qx + qy*qy;
		if(rr < qq && qq < qz*Bv[2] + qx*Bv[0] + qy*Bv[1]) return false;
	}
	return true;
}

// shared box/sphere kd walk (flags = which of the 6 query faces cut the node region; 0 = fully inside the inner box)
// Q = {outer sup[3], outer inf[3], inner sup[3], inner inf[3]}
template<class F>
static void
kdBoxWalk(const CollisionTree *t, const float *Q, uint32 flags0, F leaf)
{
	CollNode root = rootNode(t);
	struct E { const CollNode *n; uint32 f; };
	std::vector<E> st; st.push_back(E{ &root, flags0 });
	while(!st.empty()){
		E cur = st.back(); st.pop_back();
		for(;;){
			if(cur.n->contents != 0xFF){ leaf((uint32)cur.n->index, (uint32)cur.n->index + cur.n->contents, cur.f); break; }
			if(cur.f == 0){
				const CollSplit *l = &t->splits[cur.n->index];
				while(l->left.contents == 0xFF) l = &t->splits[l->left.index];
				const CollSplit *r = &t->splits[cur.n->index];
				while(r->right.contents == 0xFF) r = &t->splits[r->right.index];
				uint32 b = l->left.index;
				uint32 e = (uint16)((uint16)r->right.contents + r->right.index);
				// end computed as u16 (start+count) then length = (u16)(end - start)
				uint32 len = (uint16)(e - b);
				leaf(b, b + len, 0);
				break;
			}
			const CollSplit *pb = &t->splits[cur.n->index];
			int ao = pb->left.type & 0xC, a = ao >> 2, sh = ao >> 1;
			if(pb->right.value <= Q[a]){
				E right{ &pb->right, cur.f };
				if(Q[3+a] <= pb->left.value){
					E left{ &pb->left, cur.f };
					if(pb->left.value <= Q[6+a]) left.f &= ~(2u << sh);
					if(Q[9+a] <= pb->right.value) right.f &= ~(1u << sh);
					st.push_back(right);
					cur = left;
				}else cur = right;
			}else cur.n = &pb->left;
		}
	}
}

// plane tree walk shared by box/sphere (Q outer box sup/inf)
template<class F>
static void
planeBoxWalk(World *w, const float *Q, F sector)
{
	std::vector<BspNode*> st; st.push_back(w->root);
	while(!st.empty()){
		BspNode *n = st.back(); st.pop_back();
		while(!n->isLeaf){
			PlaneSector *p = (PlaneSector*)n;
			int a = p->type >> 2;
			if(p->rightValue <= Q[a]){
				if(Q[3+a] <= p->leftValue){ st.push_back(p->right); n = p->left; }
				else n = p->right;
			}else n = p->left;
		}
		sector((WorldSector*)n);
	}
}

static uint32
initFlags(const CollisionTree *t, const float *inner)	// inner = {sup[3], inf[3]}
{
	const float *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x;
	uint32 f = 0;
	for(int a = 0; a < 3; a++){
		if(ti[a] < inner[3+a]) f |= 1u << (2*a);
		if(inner[a] < ts[a]) f |= 2u << (2*a);
	}
	return f;
}

static bool
disjoint(const CollisionTree *t, const float *B)	// B = {sup, inf}
{
	const float *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x;
	for(int a = 0; a < 3; a++)
		if(ti[a] > B[a] || ts[a] < B[3+a]) return true;
	return false;
}

static void
origWorldBox(OCtx &c, const BBox &box)
{
	float Q[12] = { box.sup.x, box.sup.y, box.sup.z, box.inf.x, box.inf.y, box.inf.z,
	                box.sup.x, box.sup.y, box.sup.z, box.inf.x, box.inf.y, box.inf.z };
	planeBoxWalk(c.w, Q, [&](WorldSector *s){
		const CollisionTree *t = s->collTree;
		if(t == nil || disjoint(t, Q)) return;
		kdBoxWalk(t, Q, initFlags(t, Q+6), [&](uint32 b, uint32 e, uint32 f){
			for(uint32 k = b; k < e; k++){
				uint32 idx = t->map ? t->map[k] : k;
				if(f == 0 || origBoxTri(Q, VX(s,idx,0), VX(s,idx,1), VX(s,idx,2)))
					report(c, s, idx, 0.0f);
			}
		});
	});
}

static void
origWorldSphere(OCtx &c, const Sphere &sp)
{
	const float *cn = &sp.center.x; float r = sp.radius;
	float ir = 1.0f/r, ri = r*0.57f;
	float Q[12];
	for(int a = 0; a < 3; a++){ Q[a] = r + cn[a]; Q[3+a] = cn[a] - r; Q[6+a] = cn[a] + ri; Q[9+a] = cn[a] - ri; }
	float S[4] = { cn[0], cn[1], cn[2], r };
	planeBoxWalk(c.w, Q, [&](WorldSector *s){
		const CollisionTree *t = s->collTree;
		if(t == nil) return;
		const float *ts = &t->bbox.sup.x, *ti = &t->bbox.inf.x;
		float dd[3];
		for(int a = 0; a < 3; a++){
			if(ti[a] <= cn[a]){ dd[a] = 0.0f; if(ts[a] < cn[a]) dd[a] = cn[a] - ts[a]; }
			else dd[a] = ti[a] - cn[a];
		}
		if(!(dd[2]*dd[2] + dd[1]*dd[1] + dd[0]*dd[0] <= r*r)) return;
		if(disjoint(t, Q)) return;
		kdBoxWalk(t, Q, initFlags(t, Q+6), [&](uint32 b, uint32 e, uint32 f){
			for(uint32 k = b; k < e; k++){
				uint32 idx = t->map ? t->map[k] : k;
				const float *v0 = VX(s,idx,0), *v1 = VX(s,idx,1), *v2 = VX(s,idx,2);
				float n[3], dist;
				if(f == 0){
					float e2x = v2[0]-v0[0], e2y = v2[1]-v0[1];
					float nx = (v2[2]-v0[2])*(v1[1]-v0[1]) - e2y*(v1[2]-v0[2]);
					float ny = e2x*(v1[2]-v0[2]) - (v2[2]-v0[2])*(v1[0]-v0[0]);
					float nz = e2y*(v1[0]-v0[0]) - e2x*(v1[1]-v0[1]);
					float iv = invsqrtf_(nz*nz + nx*nx + ny*ny);
					nx *= iv; ny *= iv; nz = iv*nz;
					dist = (cn[2]-v0[2])*nz + (cn[0]-v0[0])*nx + (cn[1]-v0[1])*ny;
				}else if(!origSphereTri(S, v0, v1, v2, n, &dist)) continue;
				report(c, s, idx, dist*ir);
			}
		});
	});
}

// RtLineSphereIntersectionTest 0x569110 (line, sphere, &dist); _rwV3dNormalize returns 1/length
static bool
origLineSphere(const Line &L, const Sphere &S, float *dist)
{
	float d[3] = { L.end.x-L.start.x, L.end.y-L.start.y, L.end.z-L.start.z };
	float l2 = d[2]*d[2] + d[1]*d[1] + d[0]*d[0];
	float il = invsqrtf_(l2);
	for(int a = 0; a < 3; a++) d[a] *= il;
	float rr = S.radius*S.radius;
	float mx = S.center.x-L.start.x, my = S.center.y-L.start.y, mz = S.center.z-L.start.z;
	float b = mz*d[2] + mx*d[0] + my*d[1];
	float m2 = mz*mz + mx*mx + my*my;
	if(m2 < rr){ *dist = b*il; return true; }
	if(0.0f < b && 0.0f <= rr - (m2 - b*b)){ *dist = b*il; return true; }
	return false;
}

// RtBaryGetTransform 0x569610 -> 16 floats (RwMatrix incl. flags/pad words) + *scale
static bool
origBary(float *m, float *scale, const float *p3, const float *p4, const float *p5)
{
	float f14 = 0.0f;
	float f1 = p4[0]-p5[0], f2 = p4[1]-p5[1], f3 = p4[2]-p5[2];
	float f4 = p5[0]-p3[0], f5 = p5[1]-p3[1], f6 = p5[2]-p3[2];
	float f11 = f6*f2 - f5*f3, f12 = f4*f3 - f6*f1, f13 = f5*f1 - f4*f2;
	float f7 = f13*f13 + f11*f11 + f12*f12;
	bool deg = f7 <= 2.4414062e-4f;
	if(deg) *scale = 0.0f;
	else { *scale = sqrtf(f7); f14 = 1.0f/sqrtf(f7); }
	f11 *= f14; f12 = f14*f12; f13 = f14*f13;
	float f18 = (f2*f13 - f12*f3)*f14, f19 = (f11*f3 - f13*f1)*f14, f15 = (f12*f1 - f11*f2)*f14;
	float f16 = (f13*f5 - f12*f6)*f14, f17 = (f11*f6 - f13*f4)*f14, g14 = (f12*f4 - f11*f5)*f14;
	float cx = (p3[0]+p4[0]+p5[0])*0.33333334f, cy = (p5[1]+p3[1]+p4[1])*0.33333334f, cz = (p5[2]+p3[2]+p4[2])*0.33333334f;
	m[1] = f16; m[5] = f17; m[9] = g14;
	m[0] = f18; m[2] = -(f16+f18); m[3] = f11;
	m[4] = f19; m[6] = -(f17+f19); m[7] = f12;
	m[8] = f15; m[10] = -(g14+f15); m[11] = f13;
	m[12] = -(p5[2]*f15 + f18*p5[0] + p5[1]*f19);
	float t = -(p3[2]*g14 + f16*p3[0] + p3[1]*f17);
	m[13] = t;
	m[14] = 1.0f - (t + m[12]);
	m[15] = -(cz*f13 + cx*f11 + cy*f12);
	return !deg;
}

// ------------------------------------------------------------------ fork side
struct FCtx { std::map<WorldSector*, int> *sidx; Hits *out; };
static CollisionTriangle*
forkCB(Intersection *, WorldSector *s, CollisionTriangle *ct, float32 dist, void *data)
{
	FCtx *c = (FCtx*)data;
	Hit h; h.sector = (*c->sidx)[s]; h.index = ct->index; h.dist = dist;
	h.n[0] = ct->normal.x; h.n[1] = ct->normal.y; h.n[2] = ct->normal.z;
	h.pt[0] = ct->point.x; h.pt[1] = ct->point.y; h.pt[2] = ct->point.z;
	c->out->push_back(h);
	return ct;
}

// ------------------------------------------------------------------ diff
struct Stat {
	long q, qHit, setEq, seqEq, firstEq, nearestEq, onlyF, onlyO, onlyFback, onlyFnotree, onlyFother, onlyOother;
	long distCmp, distBad, ptV0, ptDiff, nrmBad, hitsF, hitsO;
	double maxDistErr;
};
static Stat ST[3];
static const char *TN[3] = { "line", "sphere", "box" };
static std::vector<std::string> samples[3];

static bool isBackface(WorldSector *s, uint32 idx, const Line &L)
{
	const float *a = VX(s,idx,0), *b = VX(s,idx,1), *c = VX(s,idx,2);
	double e1[3], e2[3], n[3], d[3] = { L.end.x-L.start.x, L.end.y-L.start.y, L.end.z-L.start.z };
	for(int i = 0; i < 3; i++){ e1[i] = b[i]-a[i]; e2[i] = c[i]-a[i]; }
	n[0] = e1[1]*e2[2]-e1[2]*e2[1]; n[1] = e1[2]*e2[0]-e1[0]*e2[2]; n[2] = e1[0]*e2[1]-e1[1]*e2[0];
	return n[0]*d[0]+n[1]*d[1]+n[2]*d[2] >= 0.0;
}

static void
compare(int ty, World *w, std::map<WorldSector*, int> &sidx, Hits &F, Hits &O, const Intersection &is)
{
	Stat &S = ST[ty];
	S.q++; if(!F.empty() || !O.empty()) S.qHit++;
	S.hitsF += F.size(); S.hitsO += O.size();
	std::map<std::pair<int,int>, const Hit*> mf, mo;
	for(auto &h : F) mf[std::make_pair(h.sector, h.index)] = &h;
	for(auto &h : O) mo[std::make_pair(h.sector, h.index)] = &h;
	bool seteq = true;
	for(auto &kv : mf) if(!mo.count(kv.first)){
		seteq = false; S.onlyF++;
		WorldSector *s = w->sectors[kv.first.first];
		if(s->collTree == nil) S.onlyFnotree++;
		else if(ty == 0 && isBackface(s, kv.first.second, is.t.line)) S.onlyFback++;
		else { S.onlyFother++;
			if(samples[ty].size() < 6){ char b[200]; snprintf(b, sizeof b, "onlyFork s%d t%d d=%g", kv.first.first, kv.first.second, kv.second->dist); samples[ty].push_back(b); } }
	}
	for(auto &kv : mo) if(!mf.count(kv.first)){
		seteq = false; S.onlyO++; S.onlyOother++;
		if(getenv("DBG")){ WorldSector *s = w->sectors[kv.first.first]; const float *p0 = VX(s,kv.first.second,0), *p1 = VX(s,kv.first.second,1), *p2 = VX(s,kv.first.second,2);
			double e1[3], e2[3], nn[3]; for(int i = 0; i < 3; i++){ e1[i] = p1[i]-p0[i]; e2[i] = p2[i]-p0[i]; }
			nn[0] = e1[1]*e2[2]-e1[2]*e2[1]; nn[1] = e1[2]*e2[0]-e1[0]*e2[2]; nn[2] = e1[0]*e2[1]-e1[1]*e2[0];
			printf("ONLYORIG %s area2 %g d %g\n", TN[ty], sqrt(nn[0]*nn[0]+nn[1]*nn[1]+nn[2]*nn[2]), kv.second->dist); }
		if(samples[ty].size() < 12){ char b[200]; snprintf(b, sizeof b, "onlyOrig s%d t%d d=%g", kv.first.first, kv.first.second, kv.second->dist); samples[ty].push_back(b); }
	}
	if(seteq) S.setEq++;
	// order on the common subsequence
	std::vector<std::pair<int,int>> sf, so;
	for(auto &h : F) if(mo.count(std::make_pair(h.sector, h.index))) sf.push_back(std::make_pair(h.sector, h.index));
	for(auto &h : O) if(mf.count(std::make_pair(h.sector, h.index))) so.push_back(std::make_pair(h.sector, h.index));
	if(sf == so) S.seqEq++;
	if((F.empty() && O.empty()) || (!F.empty() && !O.empty() && F[0].sector == O[0].sector && F[0].index == O[0].index)) S.firstEq++;
	auto nearest = [](Hits &H) -> const Hit* { const Hit *b = nil; for(auto &h : H) if(!b || h.dist < b->dist) b = &h; return b; };
	const Hit *nf = nearest(F), *no = nearest(O);
	if((!nf && !no) || (nf && no && nf->sector == no->sector && nf->index == no->index)) S.nearestEq++;
	if(ty == 0){ static long n1, eq1; const Hit *f1 = nil; for(auto &h : F) if(mo.count(std::make_pair(h.sector, h.index))){ f1 = &h; break; }
		if(f1 || !O.empty()){ n1++; if(f1 && !O.empty() && f1->sector == O[0].sector && f1->index == O[0].index) eq1++; }
		if(getenv("FIRSTSTAT") && S.q == 164000) printf("FIRST_ONESIDED_FORK_vs_ORIG equal %ld of %ld\n", eq1, n1); }
	if(ty == 0 && !F.empty()){ static long a, b, c2, d2; a++; if(F[0].dist == nf->dist) b++; if(!O.empty()){ c2++; if(O[0].dist == no->dist) d2++; }
		if(getenv("FIRSTSTAT") && (a % 20000) == 0) printf("FIRSTISNEAREST fork %ld/%ld orig %ld/%ld\n", b, a, d2, c2); }
	for(auto &kv : mf){
		auto it = mo.find(kv.first);
		if(it == mo.end()) continue;
		const Hit &a = *kv.second, &b = *it->second;
		S.distCmp++;
		double e = fabs((double)a.dist - b.dist);
		if(e > 1e-4*(1+fabs(b.dist))){ S.distBad++;
			if(ty == 0 && getenv("DBG")){ WorldSector *s = w->sectors[kv.first.first]; const float *p0 = VX(s,kv.first.second,0), *p1 = VX(s,kv.first.second,1), *p2 = VX(s,kv.first.second,2);
				double e1[3], e2[3], nn[3], d[3] = { is.t.line.end.x-is.t.line.start.x, is.t.line.end.y-is.t.line.start.y, is.t.line.end.z-is.t.line.start.z };
				for(int i = 0; i < 3; i++){ e1[i] = p1[i]-p0[i]; e2[i] = p2[i]-p0[i]; }
				nn[0] = e1[1]*e2[2]-e1[2]*e2[1]; nn[1] = e1[2]*e2[0]-e1[0]*e2[2]; nn[2] = e1[0]*e2[1]-e1[1]*e2[0];
				double ln = sqrt(nn[0]*nn[0]+nn[1]*nn[1]+nn[2]*nn[2]), ld = sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
				printf("DISTBAD fork %g orig %g cos %g area2 %g len %g\n", a.dist, b.dist, (nn[0]*d[0]+nn[1]*d[1]+nn[2]*d[2])/(ln*ld+1e-300), ln, ld); }
		}
		if(e > S.maxDistErr) S.maxDistErr = e;
		if(a.pt[0] == b.pt[0] && a.pt[1] == b.pt[1] && a.pt[2] == b.pt[2]) S.ptV0++; else S.ptDiff++;
		double nd = fabs(a.n[0]-b.n[0]) + fabs(a.n[1]-b.n[1]) + fabs(a.n[2]-b.n[2]);
		if(nd > 1e-3) S.nrmBad++;
	}
}

static void
diffFile(const char *path, long nq)
{
	World *w = loadWorld(path);
	if(w == nil){ printf("LOADFAIL %s\n", path); return; }
	std::map<WorldSector*, int> sidx;
	std::vector<int> withTris;
	for(int i = 0; i < w->numSectors; i++){ sidx[w->sectors[i]] = i; if(w->sectors[i]->numTriangles) withTris.push_back(i); }
	if(withTris.empty()){ return; }
	OCtx oc; oc.w = w; oc.sidx = sidx;
	for(long q = 0; q < nq; q++){
		WorldSector *s = w->sectors[withTris[rnd() % withTris.size()]];
		uint32 ti = rnd() % s->numTriangles;
		const float *a = VX(s,ti,0), *b = VX(s,ti,1), *c = VX(s,ti,2);
		double u = urand(), v = urand(); if(u+v > 1){ u = 1-u; v = 1-v; }
		double P[3];
		for(int i = 0; i < 3; i++) P[i] = a[i] + u*(b[i]-a[i]) + v*(c[i]-a[i]);
		if(urand() < 0.3){	// unanchored: random point in the world box
			for(int i = 0; i < 3; i++) P[i] = uni((&w->bbox.inf.x)[i], (&w->bbox.sup.x)[i]);
		}
		int ty = q % 3;
		Intersection is;
		Hits F, O;
		if(ty == 0){
			double dir[3], len = logu(0.05, 200);
			if(urand() < 0.4){ dir[0] = dir[2] = 0; dir[1] = urand() < 0.5 ? -1 : 1; }	// vertical (ground probes)
			else { double l2; do { for(int i = 0; i < 3; i++) dir[i] = uni(-1,1); l2 = dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]; } while(l2 > 1 || l2 < 1e-4); for(int i = 0; i < 3; i++) dir[i] /= sqrt(l2); }
			double f = urand();
			is.type = INTERSECTLINE;
			is.t.line.start.set(P[0] - f*len*dir[0], P[1] - f*len*dir[1], P[2] - f*len*dir[2]);
			is.t.line.end.set(P[0] + (1-f)*len*dir[0], P[1] + (1-f)*len*dir[1], P[2] + (1-f)*len*dir[2]);
		}else if(ty == 1){
			is.type = INTERSECTSPHERE;
			for(int i = 0; i < 3; i++) P[i] += uni(-1,1)*0.5;
			is.t.sphere.center.set(P[0], P[1], P[2]);
			is.t.sphere.radius = logu(0.02, 30);
		}else{
			is.type = INTERSECTBOX;
			double h[3]; double base = logu(0.02, 30);
			for(int i = 0; i < 3; i++) h[i] = base*uni(0.3, 1.0);
			is.t.box.inf.set(P[0]-h[0]+uni(-0.3,0.3)*h[0], P[1]-h[1], P[2]-h[2]);
			is.t.box.sup.set(P[0]+h[0], P[1]+h[1]+uni(-0.3,0.3)*h[1], P[2]+h[2]);
		}
		FCtx fc = { &sidx, &F };
		collisionWorldForAllIntersections(w, &is, forkCB, &fc);
		oc.out = &O;
		if(ty == 0) origWorldLine(oc, is.t.line);
		else if(ty == 1) origWorldSphere(oc, is.t.sphere);
		else origWorldBox(oc, is.t.box);
		compare(ty, w, sidx, F, O, is);
	}
	w->destroy();
}

static void
printStats(void)
{
	for(int t = 0; t < 3; t++){
		Stat &S = ST[t];
		printf("%s: queries %ld (with hits %ld) hitsFork %ld hitsOrig %ld | setEqual %ld seqEqual(common) %ld firstEqual %ld nearestEqual %ld\n",
			TN[t], S.q, S.qHit, S.hitsF, S.hitsO, S.setEq, S.seqEq, S.firstEq, S.nearestEq);
		printf("   onlyFork %ld (backface %ld, sector without tree %ld, other %ld) onlyOrig %ld | common %ld distBad %ld maxDistErr %g | point==v0 %ld point!=v0 %ld | normalBad %ld\n",
			S.onlyF, S.onlyFback, S.onlyFnotree, S.onlyFother, S.onlyO, S.distCmp, S.distBad, S.maxDistErr, S.ptV0, S.ptDiff, S.nrmBad);
		for(auto &s : samples[t]) printf("     %s\n", s.c_str());
	}
}

// ------------------------------------------------------------------ Rt maths
static void
rtTests(long n)
{
	long ls = 0, lsAgree = 0, lsDistBad = 0, lsOnlyF = 0, lsOnlyO = 0, lsBeyondEnd = 0;
	long lt = 0, ltAgree = 0, ltOnlyF = 0, ltOnlyO = 0, ltBack = 0, ltDistBad = 0;
	long by = 0, byWeightsBad = 0, byDetBad = 0, byRetBad = 0;
	for(long i = 0; i < n; i++){
		// line vs sphere
		Line L; Sphere S;
		L.start.set(uni(-10,10), uni(-10,10), uni(-10,10)); L.end.set(uni(-10,10), uni(-10,10), uni(-10,10));
		S.center.set(uni(-10,10), uni(-10,10), uni(-10,10)); S.radius = logu(0.1, 8);
		float df = -99, dor = -99;
		bool f = lineSphereIntersectionTest(&L, &S, &df), o = origLineSphere(L, S, &dor);
		ls++;
		if(f == o){ lsAgree++; if(f && fabs(df - dor) > 1e-3) lsDistBad++; }
		else if(f) lsOnlyF++; else { lsOnlyO++; if(dor > 1) lsBeyondEnd++; }
		// line vs triangle
		V3d v[3]; for(int k = 0; k < 3; k++) v[k].set(uni(-5,5), uni(-5,5), uni(-5,5));
		V3d ls0, ld; ls0.set(uni(-10,10), uni(-10,10), uni(-10,10)); ld.set(uni(-20,20), uni(-20,20), uni(-20,20));
		float tf = -1, to = -1;
		bool a = intersectionLineTriangle(&ls0, &ld, &v[0], &v[1], &v[2], &tf);
		bool b = origLineTri(&ls0.x, &ld.x, &v[0].x, &v[1].x, &v[2].x, &to);
		lt++;
		if(a == b){ ltAgree++; if(a && fabs(tf-to) > 1e-4) ltDistBad++; }
		else if(a){ ltOnlyF++; Line LL; LL.start = ls0; LL.end = add(ls0, ld);
			double e1[3], e2[3], nn[3]; for(int k = 0; k < 3; k++){ e1[k] = (&v[1].x)[k]-(&v[0].x)[k]; e2[k] = (&v[2].x)[k]-(&v[0].x)[k]; }
			nn[0] = e1[1]*e2[2]-e1[2]*e2[1]; nn[1] = e1[2]*e2[0]-e1[0]*e2[2]; nn[2] = e1[0]*e2[1]-e1[1]*e2[0];
			if(nn[0]*ld.x+nn[1]*ld.y+nn[2]*ld.z > 0) ltBack++; }
		else ltOnlyO++;
		// bary: does the fork's matrix, used the way course_world_ray_hit_light_sample uses it, give the vertex weights?
		Matrix m; float det = -1; float om[16], osc = -1;
		bool rf = baryGetTransform(&m, &det, &v[0], &v[1], &v[2]);
		bool ro = origBary(om, &osc, &v[0].x, &v[1].x, &v[2].x);
		by++;
		if(rf != ro) byRetBad++;
		if(fabs(det - osc) > 1e-3*(1+osc)) byDetBad++;
		double w0 = urand(), w1 = urand()*(1-w0), w2 = 1-w0-w1;
		float q[3]; for(int k = 0; k < 3; k++) q[k] = w0*(&v[0].x)[k] + w1*(&v[1].x)[k] + w2*(&v[2].x)[k];
		const float *fm = (const float*)&m;	// 16 floats, same as RwMatrix
		bool bad = false;
		double want[3] = { w0, w1, w2 };
		for(int c = 0; c < 3; c++){
			double gf = fm[c]*q[0] + fm[4+c]*q[1] + fm[8+c]*q[2] + fm[12+c];
			double go = om[c]*q[0] + om[4+c]*q[1] + om[8+c]*q[2] + om[12+c];
			if(ro && fabs(go - want[c]) > 1e-3) { printf("ORIG BARY MODEL WRONG c%d %g %g\n", c, go, want[c]); }
			if(ro && fabs(gf - go) > 1e-3) bad = true;
		}
		if(bad) byWeightsBad++;
	}
	printf("RtLineSphereIntersectionTest: %ld cases, agree %ld (dist differs >1e-3 in %ld), only fork %ld, only orig %ld (of which orig dist>1 i.e. beyond the segment end %ld)\n",
		ls, lsAgree, lsDistBad, lsOnlyF, lsOnlyO, lsBeyondEnd);
	printf("RtIntersectionLineTriangle: %ld cases, agree %ld (dist differs %ld), only fork %ld (backface %ld), only orig %ld\n",
		lt, ltAgree, ltDistBad, ltOnlyF, ltBack, ltOnlyO);
	printf("RtBaryGetTransform: %ld cases, return differs %ld, scale/determinant differs %ld, game-style weights (cols 0..2 + pos) differ %ld\n",
		by, byRetBad, byDetBad, byWeightsBad);
}

int
main(int argc, char **argv)
{
	Engine::init();
	registerCollisionPlugin();
	Engine::open(nil);
	Engine::start();
	if(argc < 2) return 1;
	std::string mode = argv[1];
	long n = 1000;
	int i = 2;
	if(i+1 < argc && strcmp(argv[i], "-n") == 0){ n = atol(argv[i+1]); i += 2; }
	if(mode == "rt"){ rtTests(n); return 0; }
	for(; i < argc; i++){
		if(mode == "dump"){
			World *w = loadWorld(argv[i]);
			printf("F %s\n", argv[i]);
			if(w == nil){ printf("LOADFAIL\n"); continue; }
			dumpWorld(w);
			w->destroy();
		}else diffFile(argv[i], n);
	}
	if(mode == "diff") printStats();
	return 0;
}
