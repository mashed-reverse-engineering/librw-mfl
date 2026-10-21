// Collision kd tree (0x11D) + intersection query test. No GPU, no rendering.
// For every given BSP: loads the world, validates every sector tree structurally, checks that toBlob reproduces the
// stored bytes, rebuilds a tree with CollisionTree::build and validates that one too, then runs random line/sphere/box
// queries through collisionWorldForAllIntersections and compares against an independent brute force reference
// (different formulations: plane + barycentric line test, clipping box test, region based sphere distance).
// Build: [SAN=1] tests/build_collision_test.sh   Run: collision_test [-n queriesPerFile] [-g] FILES...  (exit 1 on failure)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <set>
#include <map>
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

static std::vector<std::string> errs;
static long nErr;
#define CHECK(c, ...) do{ if(!(c)){ nErr++; if(errs.size() < 8){ char b_[300]; snprintf(b_, sizeof b_, __VA_ARGS__); errs.push_back(b_); } } }while(0)

// ---------------------------------------------------------------- rng
static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
static double urand(void) { return (rnd() & 0xFFFFFF) / 16777216.0; }
static double uni(double a, double b) { return a + (b-a)*urand(); }
static double logu(double a, double b) { return exp(uni(log(a), log(b))); }

// ---------------------------------------------------------------- structure validation
struct TreeStats { long leaves, splits, maxDepth, maxLeaf, outOfParent, slop; };

static void
validateNode(const CollisionTree *t, const CollNode &n, const float *lo, const float *hi, int depth,
             const V3d *verts, const Triangle *tris, std::vector<int> &ref, std::vector<int> &splitSeen,
             TreeStats &st, const char *what)
{
	float nlo[3] = { lo[0], lo[1], lo[2] }, nhi[3] = { hi[0], hi[1], hi[2] };
	int a = (n.type >> 2) & 3;
	CHECK(a < 3 && (n.type & ~1) <= 8, "%s: bad node type %u", what, n.type);
	if(a >= 3) return;
	// the clip value must not widen the parent volume (informational counter, tree stays valid)
	if(n.type & 1){ if(n.value > nhi[a]) st.outOfParent++; nhi[a] = std::min(nhi[a], n.value); }
	else{ if(n.value < nlo[a]) st.outOfParent++; nlo[a] = std::max(nlo[a], n.value); }
	if(depth > st.maxDepth) st.maxDepth = depth;
	if(n.contents == CollisionTree::SPLIT){
		CHECK(n.index < t->numSplits, "%s: split index %u >= %u", what, n.index, t->numSplits);
		if(n.index >= t->numSplits) return;
		CHECK(++splitSeen[n.index] == 1, "%s: split %u referenced twice", what, n.index);
		if(splitSeen[n.index] != 1 || depth > 600) return;
		st.splits++;
		validateNode(t, t->splits[n.index].left, nlo, nhi, depth+1, verts, tris, ref, splitSeen, st, what);
		validateNode(t, t->splits[n.index].right, nlo, nhi, depth+1, verts, tris, ref, splitSeen, st, what);
		return;
	}
	st.leaves++;
	if(n.contents > st.maxLeaf) st.maxLeaf = n.contents;
	CHECK((uint32)n.index + n.contents <= t->numEntries, "%s: leaf range %u+%u > %u", what, n.index, n.contents, t->numEntries);
	if((uint32)n.index + n.contents > t->numEntries) return;
	for(uint32 k = n.index; k < (uint32)n.index + n.contents; k++){
		uint32 e = t->entry(k);
		CHECK(e < t->numEntries, "%s: entry %u out of range", what, e);
		if(e >= t->numEntries) continue;
		ref[e]++;
		for(int c = 0; c < 3; c++){
			const V3d &v = verts[tris[e].v[c]];
			const float *p = &v.x;
			for(int ax = 0; ax < 3; ax++){
				// the game's trees are exact only to about 1 ulp (max overshoot seen 9.5e-7); anything beyond 1e-5 is an error
				double tol = 1e-5*(1 + fabs(p[ax]));
				CHECK(p[ax] >= nlo[ax]-tol && p[ax] <= nhi[ax]+tol, "%s: triangle %u outside its leaf volume (axis %d: %.9g not in [%.9g,%.9g])",
				      what, e, ax, p[ax], nlo[ax], nhi[ax]);
				if(p[ax] < nlo[ax] || p[ax] > nhi[ax]) st.slop++;
			}
		}
	}
}

static void
validateTree(const CollisionTree *t, const V3d *verts, const Triangle *tris, int32 nt, const BBox *sectorBox,
             const char *what, TreeStats &st)
{
	CHECK(t->numEntries == (uint32)nt, "%s: numEntries %u != triangles %d", what, t->numEntries, nt);
	if(sectorBox){
		const float *a = &t->bbox.inf.x, *b = &sectorBox->inf.x, *c = &t->bbox.sup.x, *d = &sectorBox->sup.x;
		for(int i = 0; i < 3; i++)
			CHECK(a[i] == b[i] && c[i] == d[i], "%s: tree bbox differs from sector bbox", what);
	}
	if(nt == 0){ CHECK(t->numSplits == 0, "%s: splits for an empty tree", what); return; }
	std::vector<int> ref(nt, 0), splitSeen(t->numSplits, 0);
	// the root volume is the tree box
	const float *lo = &t->bbox.inf.x, *hi = &t->bbox.sup.x;
	if(t->numSplits == 0){	// one leaf of (u8)numEntries entries
		CHECK(nt < 256, "%s: single leaf tree with %d entries", what, nt);
		CollNode root; root.type = 0; root.contents = (uint8)nt; root.index = 0; root.value = lo[0];
		validateNode(t, root, lo, hi, 1, verts, tris, ref, splitSeen, st, what);
	}else{
		splitSeen[0] = 1;
		st.splits++;
		validateNode(t, t->splits[0].left, lo, hi, 1, verts, tris, ref, splitSeen, st, what);
		validateNode(t, t->splits[0].right, lo, hi, 1, verts, tris, ref, splitSeen, st, what);
	}
	for(int32 i = 0; i < nt; i++)
		CHECK(ref[i] == 1, "%s: triangle %d referenced %d times", what, i, ref[i]);
	for(uint32 i = 0; i < t->numSplits; i++)
		CHECK(splitSeen[i] == 1, "%s: split %u unreachable", what, i);
	// leaf ranges are disjoint and cover the map
	// (covered by 'referenced exactly once' for a permutation map)
	if(t->map){
		std::vector<int> m(nt, 0);
		for(int32 i = 0; i < nt; i++) if(t->map[i] < (uint32)nt) m[t->map[i]]++;
		for(int32 i = 0; i < nt; i++) CHECK(m[i] == 1, "%s: map is not a permutation at %d", what, i);
	}
}

// ---------------------------------------------------------------- reference queries (double, independent formulas)
struct Tri { double a[3], b[3], c[3]; int sector, index; double lo[3], hi[3]; };
static inline void sub3(double *o, const double *a, const double *b) { for(int i = 0; i < 3; i++) o[i] = a[i]-b[i]; }
static inline double dot3(const double *a, const double *b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static inline void cross3(double *o, const double *a, const double *b)
{ o[0] = a[1]*b[2]-a[2]*b[1]; o[1] = a[2]*b[0]-a[0]*b[2]; o[2] = a[0]*b[1]-a[1]*b[0]; }

// plane + barycentric, one sided: only a segment entering the front face (n.d < 0) can hit.
// margin = min(t, 1-t, u, v, w) (scale free); hit iff margin >= 0. *grazing: |cos(n,d)| too small to be sure of the side
static bool
refLine(const Tri &T, const double *s, const double *d, double *t, double *margin, bool *grazing, double *cosine)
{
	double e1[3], e2[3], n[3];
	sub3(e1, T.b, T.a); sub3(e2, T.c, T.a); cross3(n, e1, e2);
	double nn = dot3(n, n);
	if(nn == 0) return false;
	double nd = dot3(n, d);
	double cs = nd/sqrt(nn*dot3(d, d));
	*cosine = fabs(cs);
	*grazing = fabs(cs) < 1e-4 || fabs(nd) < 1e-6;	// float det vs the 1e-8 threshold
	if(nd >= 0 && !*grazing) return false;	// back face
	if(nd == 0){ *t = 0; *margin = 0; return true; }	// line in the plane: float rounding decides, ambiguous
	double sa[3]; sub3(sa, T.a, s);
	*t = dot3(n, sa)/nd;
	double p[3], pa[3];
	for(int i = 0; i < 3; i++) p[i] = s[i] + *t*d[i];
	sub3(pa, p, T.a);
	// barycentric from areas
	double c1[3], c2[3];
	cross3(c1, pa, e2); cross3(c2, e1, pa);
	double u = dot3(c1, n)/nn, v = dot3(c2, n)/nn;
	*margin = std::min(std::min(*t, 1 - *t), std::min(std::min(u, v), 1 - u - v));
	return true;
}

// distance from point p to the triangle by regions: projection inside or distance to the 3 edges
static double
segDist(const double *p, const double *a, const double *b)
{
	double ab[3], ap[3]; sub3(ab, b, a); sub3(ap, p, a);
	double l = dot3(ab, ab);
	double t = l > 0 ? dot3(ap, ab)/l : 0;
	t = std::max(0.0, std::min(1.0, t));
	double q[3] = { a[0]+t*ab[0]-p[0], a[1]+t*ab[1]-p[1], a[2]+t*ab[2]-p[2] };
	return sqrt(dot3(q, q));
}

static double
refTriDist(const Tri &T, const double *p)
{
	double e1[3], e2[3], n[3];
	sub3(e1, T.b, T.a); sub3(e2, T.c, T.a); cross3(n, e1, e2);
	double nn = dot3(n, n);
	double pa[3]; sub3(pa, p, T.a);
	double h = dot3(pa, n)/nn;	// plane coordinate along n
	double q[3] = { pa[0]-h*n[0], pa[1]-h*n[1], pa[2]-h*n[2] };	// projection relative to a
	double c1[3], c2[3];
	cross3(c1, q, e2); cross3(c2, e1, q);
	double u = dot3(c1, n)/nn, v = dot3(c2, n)/nn;
	if(u >= 0 && v >= 0 && u+v <= 1)
		return fabs(h)*sqrt(nn);
	return std::min(segDist(p, T.a, T.b), std::min(segDist(p, T.b, T.c), segDist(p, T.c, T.a)));
}

// Sutherland-Hodgman clip of the triangle by the 6 box planes; true if anything is left (touching counts)
struct P3 { double v[3]; };
static bool
refBox(const Tri &T, const double *inf, const double *sup)
{
	std::vector<P3> poly(3);
	for(int i = 0; i < 3; i++){ poly[0].v[i] = T.a[i]; poly[1].v[i] = T.b[i]; poly[2].v[i] = T.c[i]; }
	for(int ax = 0; ax < 3; ax++)
		for(int side = 0; side < 2; side++){
			double pl = side ? sup[ax] : inf[ax];
			double sg = side ? -1 : 1;	// keep sg*(v - pl) >= 0
			std::vector<P3> out;
			for(size_t i = 0; i < poly.size(); i++){
				const P3 &p = poly[i], &q = poly[(i+1) % poly.size()];
				double dp = sg*(p.v[ax]-pl), dq = sg*(q.v[ax]-pl);
				if(dp >= 0) out.push_back(p);
				if((dp >= 0) != (dq >= 0)){
					double t = dp/(dp-dq);
					P3 r;
					for(int k = 0; k < 3; k++) r.v[k] = p.v[k] + t*(q.v[k]-p.v[k]);
					r.v[ax] = pl;
					out.push_back(r);
				}
			}
			poly.swap(out);
			if(poly.empty()) return false;
		}
	return true;
}

// ---------------------------------------------------------------- queries
struct Q {
	int type;			// rw::INTERSECT*
	double s[3], d[3];		// line
	double c[3], r;			// sphere
	double inf[3], sup[3];		// box
	double qlo[3], qhi[3];		// aabb of the query
};

static Intersection
toIsec(const Q &q)
{
	Intersection i; memset(&i, 0, sizeof i);
	i.type = q.type;
	if(q.type == INTERSECTLINE){
		i.t.line.start.set(q.s[0], q.s[1], q.s[2]);
		i.t.line.end.set(q.s[0]+q.d[0], q.s[1]+q.d[1], q.s[2]+q.d[2]);
	}else if(q.type == INTERSECTSPHERE){
		i.t.sphere.center.set(q.c[0], q.c[1], q.c[2]); i.t.sphere.radius = q.r;
	}else{
		i.t.box.inf.set(q.inf[0], q.inf[1], q.inf[2]); i.t.box.sup.set(q.sup[0], q.sup[1], q.sup[2]);
	}
	return i;
}

// make the double description match the float one exactly
static void
fix(Q &q)
{
	Intersection i = toIsec(q);
	if(q.type == INTERSECTLINE){
		q.s[0] = i.t.line.start.x; q.s[1] = i.t.line.start.y; q.s[2] = i.t.line.start.z;
		q.d[0] = (double)i.t.line.end.x - q.s[0]; q.d[1] = (double)i.t.line.end.y - q.s[1]; q.d[2] = (double)i.t.line.end.z - q.s[2];
		// the library computes end - start in float
		V3d dl = sub(i.t.line.end, i.t.line.start);
		q.d[0] = dl.x; q.d[1] = dl.y; q.d[2] = dl.z;
		for(int a = 0; a < 3; a++){
			double e = (&i.t.line.end.x)[a];
			q.qlo[a] = std::min(q.s[a], e); q.qhi[a] = std::max(q.s[a], e);
		}
	}else if(q.type == INTERSECTSPHERE){
		q.c[0] = i.t.sphere.center.x; q.c[1] = i.t.sphere.center.y; q.c[2] = i.t.sphere.center.z; q.r = i.t.sphere.radius;
		for(int a = 0; a < 3; a++){ q.qlo[a] = q.c[a]-q.r; q.qhi[a] = q.c[a]+q.r; }
	}else{
		for(int a = 0; a < 3; a++){
			q.inf[a] = (&i.t.box.inf.x)[a]; q.sup[a] = (&i.t.box.sup.x)[a];
			q.qlo[a] = q.inf[a]; q.qhi[a] = q.sup[a];
		}
	}
}

// state 0 miss, 1 certain hit, 2 ambiguous (boundary); cosine: line vs normal; cond = |e1||e2|/|n| >= 1 (sliver
// triangles: the float32 normal, hence the distances, lose precision in proportion)
struct Ref { int state; double dist, cosine, cond; };

static Ref
refTest(const Tri &T, const Q &q)
{
	Ref r = { 0, 0, 1, 1 };
	double e1[3], e2[3], n[3];
	sub3(e1, T.b, T.a); sub3(e2, T.c, T.a); cross3(n, e1, e2);
	double nn = dot3(n, n);
	if(nn > 0) r.cond = sqrt(dot3(e1, e1)*dot3(e2, e2)/nn);
	// float32 maths in the library: borders are only certain to about 1e-6 of the coordinates
	double cmax = 0;
	for(int a = 0; a < 3; a++) cmax = std::max(cmax, std::max(fabs(T.a[a]), std::max(fabs(T.b[a]), fabs(T.c[a]))));
	if(q.type == INTERSECTLINE){
		double t, m;
		bool grazing = false;
		if(nn == 0 || !refLine(T, q.s, q.d, &t, &m, &grazing, &r.cosine)) return r;	// degenerate triangles never hit a line
		r.dist = t;
		double ln = sqrt(dot3(q.d, q.d)), sz = sqrt(std::max(dot3(e1, e1), dot3(e2, e2)));
		// slack 1e-5 + float error, which grows like 1/cos for grazing lines
		double band = (2e-5 + 1e-5*(cmax + ln)/std::min(ln, sz))/std::max(r.cosine, 1e-6);
		if(m > band && !grazing) r.state = 1;
		else if(m > -band) r.state = 2;
		return r;
	}
	if(nn < 1e-12*(1 + cmax*cmax)){	// (nearly) degenerate: reported only from fully contained kd nodes
		r.state = 2; r.dist = NAN;
		return r;
	}
	double tol = 1e-5;
	if(q.type == INTERSECTSPHERE){
		double d = refTriDist(T, q.c);
		double rad = q.r;
		double pa[3]; sub3(pa, q.c, T.a);
		r.dist = dot3(pa, n)/sqrt(nn)/rad;	// signed plane distance in radius units
		double t = tol*(1 + rad + d) + 2e-6*cmax;
		if(d < rad - t) r.state = 1;
		else if(d <= rad + t) r.state = 2;
		return r;
	}
	double gi[3], gs[3], si[3], ss[3];
	for(int a = 0; a < 3; a++){
		double t = tol*(1 + fabs(q.inf[a]) + fabs(q.sup[a]));
		gi[a] = q.inf[a]-t; gs[a] = q.sup[a]+t; si[a] = q.inf[a]+t; ss[a] = q.sup[a]-t;
	}
	bool grow = refBox(T, gi, gs);
	bool shrink = si[0] <= ss[0] && si[1] <= ss[1] && si[2] <= ss[2] && refBox(T, si, ss);
	r.state = shrink ? 1 : (grow ? 2 : 0);
	r.dist = 0;	// box distance is always 0
	return r;
}

struct Hit { int sector, index; float dist; };
struct Collect {
	World *w;
	std::map<WorldSector*, int> *sidx;
	std::vector<Hit> hits;
	long stopAfter;
	long calls;
	int lastSector;
	long orderViolations, structViolations;
	const Q *q;
};

static CollisionTriangle*
collectCB(Intersection *isec, WorldSector *sec, CollisionTriangle *ct, float32 dist, void *data)
{
	Collect *c = (Collect*)data;
	Hit h; h.sector = (*c->sidx)[sec]; h.index = ct->index; h.dist = dist;
	if(c->q->type != INTERSECTLINE && h.sector < c->lastSector) c->orderViolations++;	// lines: near to far instead
	c->lastSector = h.sector;
	c->calls++;
	// triangle consistency
	if(ct->index < 0 || ct->index >= sec->numTriangles) c->structViolations++;
	else{
		Triangle *t = &sec->triangles[ct->index];
		for(int k = 0; k < 3; k++)
			if(ct->vertices[k] != &sec->vertices[t->v[k]]) c->structViolations++;
		double e1[3], e2[3], n[3];
		double a[3] = { ct->vertices[0]->x, ct->vertices[0]->y, ct->vertices[0]->z };
		double b[3] = { ct->vertices[1]->x, ct->vertices[1]->y, ct->vertices[1]->z };
		double cc[3] = { ct->vertices[2]->x, ct->vertices[2]->y, ct->vertices[2]->z };
		sub3(e1, b, a); sub3(e2, cc, a); cross3(n, e1, e2);
		double l = sqrt(dot3(n, n));
		bool zeroN = ct->normal.x == 0 && ct->normal.y == 0 && ct->normal.z == 0;
		if(zeroN){	// degenerate triangle (float cross product 0)
			if(l > 1e-5*(1 + dot3(a, a))) c->structViolations++;
		}else{
			double dn = (n[0]/l-ct->normal.x)*(n[0]/l-ct->normal.x) + (n[1]/l-ct->normal.y)*(n[1]/l-ct->normal.y) + (n[2]/l-ct->normal.z)*(n[2]/l-ct->normal.z);
			if(!(dn < 1e-6)) c->structViolations++;
		}
		if(!(dist == dist)) c->structViolations++;
		// point = v0 for every query type
		if(ct->point.x != ct->vertices[0]->x || ct->point.y != ct->vertices[0]->y || ct->point.z != ct->vertices[0]->z) c->structViolations++;
		if(c->q->type == INTERSECTBOX && dist != 0) c->structViolations++;
	}
	c->hits.push_back(h);
	if(c->stopAfter > 0 && c->calls >= c->stopAfter)
		return nil;
	return ct;
}

struct QStats { long queries, hitsTotal, ambiguous, mismatches, missing, extra, distBad, byType[3], hitQueries[3]; };

// compares one query; refTris are (sector, index) addressable
static void
compareQuery(const Q &q, const std::vector<Tri> &tris, const std::vector<Hit> &hits, QStats &qs, const char *file)
{
	std::map<std::pair<int,int>, Hit> got;
	for(size_t i = 0; i < hits.size(); i++){
		bool dup = !got.insert(std::make_pair(std::make_pair(hits[i].sector, hits[i].index), hits[i])).second;
		CHECK(!dup, "%s: triangle (%d,%d) reported twice", file, hits[i].sector, hits[i].index);
	}
	std::set<std::pair<int,int> > certain, any;
	std::map<std::pair<int,int>, double> refDist, refCos, refCond;
	double slackv[3];
	for(int a = 0; a < 3; a++) slackv[a] = 1e-3*(1 + fabs(q.qlo[a]) + fabs(q.qhi[a]));
	for(size_t i = 0; i < tris.size(); i++){
		const Tri &T = tris[i];
		bool far = false;
		for(int a = 0; a < 3; a++)
			if(T.hi[a] < q.qlo[a]-slackv[a] || T.lo[a] > q.qhi[a]+slackv[a]) far = true;
		if(far) continue;
		Ref r = refTest(T, q);
		std::pair<int,int> k(T.sector, T.index);
		if(r.state == 1) certain.insert(k);
		if(r.state >= 1) { any.insert(k); refDist[k] = r.dist; refCos[k] = r.cosine; refCond[k] = r.cond; }
		if(r.state == 2) qs.ambiguous++;
	}
	for(std::set<std::pair<int,int> >::iterator it = certain.begin(); it != certain.end(); ++it)
		if(!got.count(*it)){
			qs.missing++; qs.mismatches++;
			CHECK(0, "%s: type %d query misses triangle (%d,%d)", file, q.type, it->first, it->second);
		}
	for(std::map<std::pair<int,int>, Hit>::iterator it = got.begin(); it != got.end(); ++it){
		if(!any.count(it->first)){
			qs.extra++; qs.mismatches++;
			CHECK(0, "%s: type %d query reports extra triangle (%d,%d)", file, q.type, it->first.first, it->first.second);
			continue;
		}
		double rd = refDist[it->first];
		if(rd != rd) continue;	// degenerate triangle: no reference distance
		double cmax = 0;
		for(int a = 0; a < 3; a++) cmax = std::max(cmax, std::max(fabs(q.qlo[a]), fabs(q.qhi[a])));
		double tolD = q.type == INTERSECTLINE ? (1e-4 + 1e-5*cmax/sqrt(dot3(q.d, q.d)))/std::max(refCos[it->first], 1e-6) :
			q.type == INTERSECTSPHERE ? 1e-3 + 1e-5*cmax/q.r : 0;
		tolD *= refCond[it->first];
		if(fabs(rd - it->second.dist) > tolD){
			qs.distBad++;
			CHECK(0, "%s: type %d distance %g vs reference %g (tri %d,%d)", file, q.type, it->second.dist, rd, it->first.first, it->first.second);
		}
	}
	qs.queries++;
	int ty = q.type == INTERSECTLINE ? 0 : q.type == INTERSECTSPHERE ? 1 : 2;
	qs.byType[ty]++;
	qs.hitsTotal += got.size();
	if(got.size()) qs.hitQueries[ty]++;
}

static Q
genQuery(const std::vector<Tri> &tris, const double *wlo, const double *whi, int type)
{
	Q q; memset(&q, 0, sizeof q);
	q.type = type;
	double diag = 0;
	for(int a = 0; a < 3; a++) diag += (whi[a]-wlo[a])*(whi[a]-wlo[a]);
	diag = sqrt(diag);
	if(diag == 0) diag = 1;
	// anchor: a point on a random triangle (70%) or random in the world box
	double P[3];
	if(!tris.empty() && urand() < 0.7){
		const Tri &T = tris[rnd() % tris.size()];
		double u = urand(), v = urand();
		if(u+v > 1){ u = 1-u; v = 1-v; }
		for(int a = 0; a < 3; a++) P[a] = T.a[a] + u*(T.b[a]-T.a[a]) + v*(T.c[a]-T.a[a]);
	}else
		for(int a = 0; a < 3; a++) P[a] = uni(wlo[a], whi[a]);
	double dir[3], l;
	do{ for(int a = 0; a < 3; a++) dir[a] = uni(-1, 1); l = sqrt(dot3(dir, dir)); }while(l < 0.1 || l > 1);
	for(int a = 0; a < 3; a++) dir[a] /= l;
	if(type == INTERSECTLINE){
		double l1 = diag*logu(0.0005, 0.5), l2 = diag*logu(0.0005, 0.5);
		if(urand() < 0.1) { dir[0] = 0; dir[2] = 0; dir[1] = 1; }	// axis aligned lines (zero deltas)
		for(int a = 0; a < 3; a++){ q.s[a] = P[a] - dir[a]*l1; q.d[a] = dir[a]*(l1+l2); }
	}else if(type == INTERSECTSPHERE){
		q.r = diag*logu(0.0005, 0.3);
		double off = q.r*uni(-1.5, 1.5);
		for(int a = 0; a < 3; a++) q.c[a] = P[a] + dir[a]*off;
	}else{
		double h[3];
		for(int a = 0; a < 3; a++) h[a] = diag*logu(0.0005, 0.2);
		for(int a = 0; a < 3; a++){
			double c = P[a] + uni(-1, 1)*h[a];
			q.inf[a] = c-h[a]; q.sup[a] = c+h[a];
		}
	}
	fix(q);
	return q;
}

// ---------------------------------------------------------------- driver
struct Totals {
	long files, sectors, sectorsWithTree, tris, splits, leaves, rebuilt, blobExact, fuzz, fuzzParsed, geoTests;
	long maxDepth, maxLeaf, outOfParent, buildMaxDepth, slop;
	QStats qs;
};

static uint8*
readFile(const char *path, uint32 *len)
{
	uint8 *data = nil;
	if(FILE *fp = fopen(path, "rb")){
		fseek(fp, 0, SEEK_END); *len = ftell(fp); fseek(fp, 0, SEEK_SET);
		data = (uint8*)malloc(*len ? *len : 1);
		if(fread(data, 1, *len, fp) != *len){ free(data); data = nil; }
		fclose(fp);
	}
	return data;
}

static void
fuzzBlob(const WorldSector *src, Totals &T, int rounds)
{
	std::vector<uint8> blob(src->collision, src->collision + src->collisionSize);
	fflush(stderr);
	int saved = dup(2), nul = open("/dev/null", O_WRONLY);	// the parser reports every rejected blob
	dup2(nul, 2); close(nul);
	for(int r = 0; r < rounds; r++){
		std::vector<uint8> m(blob);
		int mode = rnd() % 3;
		if(mode == 0) m.resize(rnd() % (m.size() + 1));
		else if(mode == 1){ int n = 1 + rnd() % 4; for(int i = 0; i < n; i++) m[rnd() % m.size()] ^= 1 << (rnd() % 8); }
		else{	// corrupt the header / counts / first nodes where it matters
			int n = 1 + rnd() % 3;
			for(int i = 0; i < n; i++) m[(rnd() % 120) % m.size()] = rnd();
		}
		T.fuzz++;
		CollisionTree *t = CollisionTree::fromBlob(m.empty() ? nil : &m[0], m.size(), src->numTriangles);
		if(!t) continue;
		T.fuzzParsed++;
		// the mutated tree must be safe to query
		WorldSector fake = *src;
		fake.collTree = t;
		World fw; memset(&fw, 0, sizeof fw);
		WorldSector *arr[1] = { &fake };
		fw.sectors = arr; fw.numSectors = 1; fw.root = (BspNode*)&fake;
		Collect c; std::map<WorldSector*, int> sidx; sidx[&fake] = 0;
		c.w = &fw; c.sidx = &sidx; c.stopAfter = 0; c.calls = 0; c.lastSector = 0; c.orderViolations = c.structViolations = 0;
		double lo[3] = { fake.bbox.inf.x, fake.bbox.inf.y, fake.bbox.inf.z }, hi[3] = { fake.bbox.sup.x, fake.bbox.sup.y, fake.bbox.sup.z };
		std::vector<Tri> none;
		for(int k = 0; k < 6; k++){
			Q q = genQuery(none, lo, hi, k % 3 == 0 ? INTERSECTLINE : k % 3 == 1 ? INTERSECTSPHERE : INTERSECTBOX);
			Intersection is = toIsec(q);
			c.q = &q; c.hits.clear();
			collisionWorldForAllIntersections(&fw, &is, collectCB, &c);
		}
		t->destroy();
	}
	fflush(stderr);
	dup2(saved, 2); close(saved);
}

static void
testBuildAndRoundtrip(WorldSector *s, int si, const char *file, Totals &T)
{
	char what[160];
	// (a) stored tree
	if(s->collTree){
		snprintf(what, sizeof what, "%s sector %d stored", file, si);
		TreeStats st = {0,0,0,0,0,0};
		validateTree(s->collTree, s->vertices, s->triangles, s->numTriangles, &s->bbox, what, st);
		T.slop += st.slop; T.splits += st.splits; T.leaves += st.leaves; T.outOfParent += st.outOfParent;
		T.maxDepth = std::max(T.maxDepth, st.maxDepth); T.maxLeaf = std::max(T.maxLeaf, st.maxLeaf);
		T.sectorsWithTree++;
		// byte-exact reserialisation except the chunk stamps
		uint32 sz = s->collTree->blobSize();
		CHECK(sz == s->collisionSize, "%s: blobSize %u != stored %u", what, sz, s->collisionSize);
		if(sz == s->collisionSize){
			std::vector<uint8> b(sz);
			s->collTree->toBlob(&b[0]);
			std::vector<uint8> o(s->collision, s->collision + sz);
			for(int off : {12, 24}) for(int k = 0; k < 4; k++) b[off+k] = o[off+k] = 0;
			bool same = memcmp(&b[0], &o[0], sz) == 0;
			CHECK(same, "%s: toBlob differs from the stored bytes", what);
			T.blobExact += same;
		}
	}
	// (b) freshly built tree over the same triangles
	if(s->numTriangles > 0 && s->numTriangles <= 65536){
		for(int leaf : {1, 2, 5}){
			CollisionTree *t = CollisionTree::build(s->vertices, s->triangles, s->numTriangles, leaf);
			CHECK(t != nil, "%s sector %d: build failed", file, si);
			if(!t) continue;
			snprintf(what, sizeof what, "%s sector %d built(%d)", file, si, leaf);
			TreeStats st = {0,0,0,0,0,0};
			validateTree(t, s->vertices, s->triangles, s->numTriangles, nil, what, st);
			CHECK(st.maxLeaf <= leaf, "%s: leaf bigger than requested", what);
			CHECK(st.outOfParent == 0 && st.slop == 0, "%s: node values widen the parent / are inexact", what);
			T.buildMaxDepth = std::max(T.buildMaxDepth, st.maxDepth);
			std::vector<uint8> b(t->blobSize());
			t->toBlob(&b[0]);
			CollisionTree *u = CollisionTree::fromBlob(&b[0], b.size(), s->numTriangles);
			CHECK(u != nil, "%s: built tree does not parse", what);
			if(u){
				CHECK(u->numSplits == t->numSplits && u->numEntries == t->numEntries &&
				      (u->numSplits == 0 || !memcmp(u->splits, t->splits, sizeof(CollSplit)*u->numSplits)) &&
				      !memcmp(u->map, t->map, 2*u->numEntries), "%s: roundtrip differs", what);
				CollisionTree *c = u->clone();
				CHECK(c && c->numSplits == u->numSplits && (u->numSplits == 0 || !memcmp(c->splits, u->splits, sizeof(CollSplit)*u->numSplits)), "%s: clone differs", what);
				if(c) c->destroy();
				u->destroy();
			}
			t->destroy();
			T.rebuilt++;
		}
	}
}

// geometry plugin: build, query, and agree with the sector's brute force; atomic path vs local query
static void
testGeometry(WorldSector *s, int si, const char *file, Totals &T)
{
	Geometry *g = Geometry::create(s->numVertices, s->numTriangles, Geometry::POSITIONS);
	if(g == nil){ CHECK(0, "%s: geometry create", file); return; }
	memcpy(g->morphTargets[0].vertices, s->vertices, sizeof(V3d)*s->numVertices);
	memcpy(g->triangles, s->triangles, sizeof(Triangle)*s->numTriangles);
	CHECK(collisionGeometryQueryData(g) == nil, "%s: fresh geometry has data", file);
	CHECK(collisionGeometryBuildData(g, nil), "%s: GeometryBuildData failed", file);
	CollisionTree *t = collisionGeometryQueryData(g);
	if(t == nil){ g->destroy(); return; }
	char what[160]; snprintf(what, sizeof what, "%s sector %d geometry", file, si);
	TreeStats st = {0,0,0,0,0,0};
	validateTree(t, s->vertices, s->triangles, s->numTriangles, nil, what, st);
	std::vector<Tri> tris;
	double lo[3] = {1e30,1e30,1e30}, hi[3] = {-1e30,-1e30,-1e30};
	for(int i = 0; i < s->numTriangles; i++){
		Tri T2; memset(&T2, 0, sizeof T2);
		const V3d *v[3] = { &s->vertices[s->triangles[i].v[0]], &s->vertices[s->triangles[i].v[1]], &s->vertices[s->triangles[i].v[2]] };
		double *o[3] = { T2.a, T2.b, T2.c };
		for(int k = 0; k < 3; k++) for(int a = 0; a < 3; a++) o[k][a] = (&v[k]->x)[a];
		for(int a = 0; a < 3; a++){
			T2.lo[a] = std::min(T2.a[a], std::min(T2.b[a], T2.c[a])); T2.hi[a] = std::max(T2.a[a], std::max(T2.b[a], T2.c[a]));
			lo[a] = std::min(lo[a], T2.lo[a]); hi[a] = std::max(hi[a], T2.hi[a]);
		}
		T2.sector = 0; T2.index = i;
		tris.push_back(T2);
	}
	struct G { std::vector<Hit> hits; Intersection *isec; long badIsec; G() : isec(nil), badIsec(0) {} };
	struct Cb {
		static CollisionTriangle *f(Intersection *is, CollisionTriangle *ct, float32 d, void *data){
			G *g = (G*)data;
			if(g->isec && is != g->isec) g->badIsec++;
			Hit h; h.sector = 0; h.index = ct->index; h.dist = d; g->hits.push_back(h); return ct; }
	};
	// atomic with a rotated, uniformly scaled frame
	Frame *fr = Frame::create();
	Atomic *at = Atomic::create();
	at->setFrame(fr);
	at->setGeometry(g, 0);
	// axis permutation with sign, scale 2, no translation: exact in float, so the atomic path must match the local query
	fr->matrix.right.set(0, 2, 0); fr->matrix.up.set(0, 0, -2); fr->matrix.at.set(2, 0, 0); fr->matrix.pos.set(0, 0, 0);
	fr->matrix.update();
	fr->updateObjects();
	Frame::syncDirty();
	Matrix *ltm = fr->getLTM();
	QStats qs; memset(&qs, 0, sizeof qs);
	for(int k = 0; k < 400; k++){
		Q q = genQuery(tris, lo, hi, k % 3 == 0 ? INTERSECTLINE : k % 3 == 1 ? INTERSECTSPHERE : INTERSECTBOX);
		Intersection is = toIsec(q);
		G gh;
		collisionGeometryForAllIntersections(g, &is, Cb::f, &gh);
		Collect c; c.hits = gh.hits;
		compareQuery(q, tris, gh.hits, qs, what);
		// same shape through the atomic: world shape = LTM * local (lines/spheres only; boxes would need the AABB of rotated corners)
		if(q.type != INTERSECTBOX){
			Intersection wi = is;
			if(q.type == INTERSECTLINE){
				V3d p[2] = { is.t.line.start, is.t.line.end };
				V3d::transformPoints(p, p, 2, ltm);
				wi.t.line.start = p[0]; wi.t.line.end = p[1];
			}else{
				V3d p = is.t.sphere.center;
				V3d::transformPoints(&p, &p, 1, ltm);
				wi.t.sphere.center = p; wi.t.sphere.radius *= 2.0f;
			}
			G ah; ah.isec = &wi;	// the callback gets the caller's (world space) intersection
			collisionAtomicForAllIntersections(at, &wi, Cb::f, &ah);
			CHECK(ah.badIsec == 0, "%s: atomic callback got another intersection", what);
			// borderline contacts can flip through float transforms; compare via symmetric difference size
			std::set<int> a1, a2;
			for(size_t i = 0; i < gh.hits.size(); i++) a1.insert(gh.hits[i].index);
			for(size_t i = 0; i < ah.hits.size(); i++) a2.insert(ah.hits[i].index);
			std::vector<int> d;
			std::set_symmetric_difference(a1.begin(), a1.end(), a2.begin(), a2.end(), std::back_inserter(d));
			CHECK(d.size() <= 3 + a1.size()/20, "%s: atomic path (type %d) differs from local query in %zu of %zu triangles (%zu via atomic)", what, q.type, d.size(), a1.size(), a2.size());
		}else{	// atomic box queries do nothing (as the original)
			G ah;
			collisionAtomicForAllIntersections(at, &is, Cb::f, &ah);
			CHECK(ah.hits.empty(), "%s: atomic box query reported %zu triangles", what, ah.hits.size());
		}
		if(k % 50 == 0){	// type 5 = sphere query with the atomic's world bounding sphere (geometry: used as is)
			Intersection ai; memset(&ai, 0, sizeof ai);
			ai.type = INTERSECTATOMIC; ai.t.atomic = at;
			Intersection si; memset(&si, 0, sizeof si);
			si.type = INTERSECTSPHERE; si.t.sphere = *at->getWorldBoundingSphere();
			G x, y;
			collisionGeometryForAllIntersections(g, &ai, Cb::f, &x);
			collisionGeometryForAllIntersections(g, &si, Cb::f, &y);
			CHECK(x.hits.size() == y.hits.size(), "%s: INTERSECTATOMIC %zu vs sphere %zu", what, x.hits.size(), y.hits.size());
		}
	}
	// without a tree the geometry is brute forced (same hit sets)
	collisionGeometryDestroyData(g);
	CHECK(collisionGeometryQueryData(g) == nil, "%s: GeometryDestroyData", what);
	for(int k = 0; k < 60; k++){
		Q q = genQuery(tris, lo, hi, k % 3 == 0 ? INTERSECTLINE : k % 3 == 1 ? INTERSECTSPHERE : INTERSECTBOX);
		Intersection is = toIsec(q);
		G gh;
		collisionGeometryForAllIntersections(g, &is, Cb::f, &gh);
		compareQuery(q, tris, gh.hits, qs, what);
	}
	T.qs.queries += qs.queries; T.qs.hitsTotal += qs.hitsTotal; T.qs.mismatches += qs.mismatches;
	T.qs.ambiguous += qs.ambiguous; T.qs.missing += qs.missing; T.qs.extra += qs.extra; T.qs.distBad += qs.distBad;
	for(int i = 0; i < 3; i++){ T.qs.byType[i] += qs.byType[i]; T.qs.hitQueries[i] += qs.hitQueries[i]; }
	T.geoTests++;
	at->destroy();
	fr->destroy();
	g->destroy();
}

static bool
testFile(const char *path, long nQueries, bool geo, Totals &T)
{
	uint32 len = 0;
	uint8 *data = readFile(path, &len);
	if(data == nil){ CHECK(0, "%s: cannot read", path); return false; }
	StreamMemory in;
	in.open(data, len);
	uint32 clen, ver;
	if(!findChunk(&in, ID_WORLD, &clen, &ver)){ CHECK(0, "%s: no world chunk", path); free(data); return false; }
	World *w = World::streamRead(&in);
	in.close();
	free(data);
	if(w == nil){ CHECK(0, "%s: world load failed", path); return false; }
	T.files++;
	std::map<WorldSector*, int> sidx;
	std::vector<Tri> tris, treeTris;	// treeTris: the triangles a world query can report (sectors with a tree)
	double lo[3] = {1e30,1e30,1e30}, hi[3] = {-1e30,-1e30,-1e30};
	for(int32 i = 0; i < w->numSectors; i++){
		WorldSector *s = w->sectors[i];
		sidx[s] = i;
		T.sectors++; T.tris += s->numTriangles;
		testBuildAndRoundtrip(s, i, path, T);
		for(int32 k = 0; k < s->numTriangles; k++){
			Tri t2; memset(&t2, 0, sizeof t2);
			const V3d *v[3] = { &s->vertices[s->triangles[k].v[0]], &s->vertices[s->triangles[k].v[1]], &s->vertices[s->triangles[k].v[2]] };
			double *o[3] = { t2.a, t2.b, t2.c };
			for(int c = 0; c < 3; c++) for(int a = 0; a < 3; a++) o[c][a] = (&v[c]->x)[a];
			for(int a = 0; a < 3; a++){
				t2.lo[a] = std::min(t2.a[a], std::min(t2.b[a], t2.c[a])); t2.hi[a] = std::max(t2.a[a], std::max(t2.b[a], t2.c[a]));
				lo[a] = std::min(lo[a], t2.lo[a]); hi[a] = std::max(hi[a], t2.hi[a]);
			}
			t2.sector = i; t2.index = k;
			tris.push_back(t2);
			if(s->collTree) treeTris.push_back(t2);
		}
	}
	// 0x11D present or not must be consistent with the stored blob
	for(int32 i = 0; i < w->numSectors; i++)
		CHECK((w->sectors[i]->collTree != nil) == (w->sectors[i]->collisionSize != 0), "%s: tree/blob presence", path);
	// QueryData / DestroyData on a copy of the pointer is covered in main (unit part); here the world queries
	if(!tris.empty()){
		QStats qs; memset(&qs, 0, sizeof qs);
		for(long k = 0; k < nQueries; k++){
			int type = k % 3 == 0 ? INTERSECTLINE : k % 3 == 1 ? INTERSECTSPHERE : INTERSECTBOX;
			Q q = genQuery(tris, lo, hi, type);
			Intersection is = toIsec(q);
			Collect c; c.w = w; c.sidx = &sidx; c.stopAfter = 0; c.calls = 0; c.lastSector = 0;
			c.orderViolations = c.structViolations = 0; c.q = &q;
			World *r = collisionWorldForAllIntersections(w, &is, collectCB, &c);
			CHECK(r == w, "%s: ForAllIntersections returned %p", path, (void*)r);
			CHECK(c.orderViolations == 0, "%s: sectors not visited in stream order", path);
			CHECK(c.structViolations == 0, "%s: %ld bad callback arguments", path, c.structViolations);
			compareQuery(q, treeTris, c.hits, qs, path);
			if(c.hits.size() >= 2 && k % 7 == 0){	// early stop
				long stop = 1 + rnd() % (c.hits.size() - 1);
				Collect c2 = c; c2.hits.clear(); c2.stopAfter = stop; c2.calls = 0; c2.lastSector = 0;
				collisionWorldForAllIntersections(w, &is, collectCB, &c2);
				CHECK(c2.calls == stop, "%s: early stop after %ld got %ld calls", path, stop, c2.calls);
			}
		}
		T.qs.queries += qs.queries; T.qs.hitsTotal += qs.hitsTotal; T.qs.mismatches += qs.mismatches;
		T.qs.ambiguous += qs.ambiguous; T.qs.missing += qs.missing; T.qs.extra += qs.extra; T.qs.distBad += qs.distBad;
		for(int i = 0; i < 3; i++){ T.qs.byType[i] += qs.byType[i]; T.qs.hitQueries[i] += qs.hitQueries[i]; }
	}
	// parser fuzz on a few trees, geometry plugin on a few sectors
	int fz = 0, gt = 0;
	for(int32 i = 0; i < w->numSectors; i++){
		WorldSector *s = w->sectors[i];
		if(s->collTree && fz < 3 && s->collisionSize < 20000){ fuzzBlob(s, T, 150); fz++; }
		if(geo && s->numTriangles > 0 && s->numTriangles <= 2500 && gt < 2){ testGeometry(s, i, path, T); gt++; }
	}
	// SectorDestroyData frees and clears
	for(int32 i = 0; i < w->numSectors; i++){
		WorldSector *s = w->sectors[i];
		CHECK(collisionWorldSectorQueryData(s) == s->collTree, "%s: QueryData", path);
		collisionWorldSectorDestroyData(s);
		CHECK(collisionWorldSectorQueryData(s) == nil, "%s: DestroyData", path);
	}
	// sectors without a collision tree are skipped (as the original): nothing is reported any more
	if(!tris.empty()){
		long n = 0;
		for(int k = 0; k < 30; k++){
			Q q = genQuery(tris, lo, hi, k % 3 == 0 ? INTERSECTLINE : k % 3 == 1 ? INTERSECTSPHERE : INTERSECTBOX);
			Intersection is = toIsec(q);
			Collect c; c.w = w; c.sidx = &sidx; c.stopAfter = 0; c.calls = 0; c.lastSector = 0;
			c.orderViolations = c.structViolations = 0; c.q = &q;
			collisionWorldForAllIntersections(w, &is, collectCB, &c);
			n += c.hits.size();
		}
		CHECK(n == 0, "%s: %ld hits from sectors without a tree", path, n);
	}
	w->destroy();
	return true;
}

// primitive unit checks with hand computed answers
static CollisionTriangle*
countCB(Intersection*, WorldSector*, CollisionTriangle *ct, float32, void *data) { (*(int*)data)++; return ct; }

static void
unitTests(void)
{
	V3d a = { 0, 0, 0 }, b = { 4, 0, 0 }, c = { 0, 4, 0 };	// front face +z
	V3d s = { 1, 1, 5 }, d = { 0, 0, -10 };
	float t = -1;
	CHECK(intersectionLineTriangle(&s, &d, &a, &b, &c, &t) && fabs(t - 0.5) < 1e-6, "line/tri hit t=%g", t);
	V3d d2 = { 0, 0, -4 };
	CHECK(!intersectionLineTriangle(&s, &d2, &a, &b, &c, &t), "short line must miss");
	V3d s3 = { 3, 3, 5 };
	CHECK(!intersectionLineTriangle(&s3, &d, &a, &b, &c, &t), "line outside triangle must miss");
	V3d s4 = { 1, 1, -5 }, d4 = { 0, 0, 10 };
	CHECK(!intersectionLineTriangle(&s4, &d4, &a, &b, &c, &t), "one sided: back face must miss");
	V3d s5 = { 0, 0, 5 };	// through the vertex: inclusive
	CHECK(intersectionLineTriangle(&s5, &d, &a, &b, &c, &t) && fabs(t - 0.5) < 1e-6, "vertex hit");
	V3d s6 = { -1e-6f, 1, 5 };	// just outside an edge: within the 1e-5 slack
	CHECK(intersectionLineTriangle(&s6, &d, &a, &b, &c, &t), "edge slack");
	V3d s7 = { 1, 1, 0 }, d7 = { 0, 0, -10 };	// start on the plane: t = 0
	CHECK(intersectionLineTriangle(&s7, &d7, &a, &b, &c, &t) && t == 0, "start on plane t=%g", t);
	Line l = { { -5, 0, 0 }, { 5, 0, 0 } };
	Sphere sp = { { 0, 0, 1 }, 1.5f };
	CHECK(lineSphereIntersectionTest(&l, &sp, &t) && fabs(t - 0.5) < 1e-6, "line/sphere t=%g (closest approach)", t);
	Sphere sp2 = { { 0, 0, 2 }, 1.5f };
	CHECK(!lineSphereIntersectionTest(&l, &sp2, nil), "line/sphere miss");
	Sphere sp3 = { { -5, 0, 0 }, 1.0f };
	CHECK(lineSphereIntersectionTest(&l, &sp3, &t) && t == 0, "start inside");
	Sphere sp4 = { { 8, 0, 0 }, 1.0f };	// beyond the end: a ray test
	CHECK(lineSphereIntersectionTest(&l, &sp4, &t) && fabs(t - 1.3) < 1e-6, "ray beyond end t=%g", t);
	Sphere sp5 = { { -8, 0, 0 }, 1.0f };	// behind the start
	CHECK(!lineSphereIntersectionTest(&l, &sp5, nil), "behind start");
	Matrix m; float sc = 0;
	V3d p0 = { 1, 2, 3 }, p1 = { 3, 2, 3 }, p2 = { 1, 5, 3 };	// n = (0,0,6)
	CHECK(baryGetTransform(&m, &sc, &p0, &p1, &p2) && fabs(sc - 6) < 1e-5, "bary scale %g", sc);
	// q = 0.25 v0 + 0.25 v1 + 0.5 v2 + 1.0 n^ ; the game reads w_i = m[i]x + m[4+i]y + m[8+i]z + m[12+i]
	const float *M = (const float*)&m;
	float q[3] = { 1.5f, 3.5f, 4.0f }, want[4] = { 0.25f, 0.25f, 0.5f, 1.0f };
	for(int i = 0; i < 4; i++){
		float w = M[i]*q[0] + M[4+i]*q[1] + M[8+i]*q[2] + M[12+i];
		CHECK(fabs(w - want[i]) < 1e-5, "bary column %d: %g, want %g", i, w, want[i]);
	}
	V3d z = { 0, 0, 0 };
	sc = -1;
	CHECK(!baryGetTransform(&m, &sc, &p0, &p0, &z) && sc == 0, "degenerate bary");
	V3d t0 = { 0, 0, 0 }, t1 = { 0.01f, 0, 0 }, t2 = { 0, 0.01f, 0 };	// |n|^2 = 1e-8 <= 2.44e-4
	CHECK(!baryGetTransform(&m, &sc, &t0, &t1, &t2), "small triangle is degenerate");

	// numSplits == 0 tree: one leaf of numEntries entries; parses, builds and is queried
	V3d gv[6] = { {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}, {1,0,1}, {0,1,1} };
	Triangle gt[3];
	gt[0].v[0] = 0; gt[0].v[1] = 1; gt[0].v[2] = 2; gt[1].v[0] = 3; gt[1].v[1] = 4; gt[1].v[2] = 5;
	gt[2].v[0] = 0; gt[2].v[1] = 4; gt[2].v[2] = 1;
	for(int i = 0; i < 3; i++) gt[i].matId = 0;
	CollisionTree *ct = CollisionTree::build(gv, gt, 3, 5);
	CHECK(ct && ct->numSplits == 0 && ct->numEntries == 3, "single leaf build");
	if(ct){
		std::vector<uint8> bl(ct->blobSize());
		ct->toBlob(&bl[0]);
		CollisionTree *u = CollisionTree::fromBlob(&bl[0], bl.size(), 3);
		CHECK(u && u->numSplits == 0, "numSplits == 0 tree must parse");
		if(u) u->destroy();
		// as a sector: a downward line through both z planes hits the two front faces (normals +z), near first
		WorldSector sec; memset(&sec, 0, sizeof sec);
		sec.isLeaf = 1; sec.numTriangles = 3; sec.numVertices = 6; sec.vertices = gv; sec.triangles = gt; sec.collTree = ct;
		World w; memset(&w, 0, sizeof w);
		WorldSector *arr[1] = { &sec };
		w.sectors = arr; w.numSectors = 1; w.root = (BspNode*)&sec;
		Intersection is; memset(&is, 0, sizeof is);
		is.type = INTERSECTLINE; is.t.line.start.set(0.2f, 0.2f, 2); is.t.line.end.set(0.2f, 0.2f, -1);
		int n = 0;
		collisionWorldForAllIntersections(&w, &is, countCB, &n);
		CHECK(n == 2, "single leaf tree line hits %d", n);
		is.t.line.start.set(0.2f, 0.2f, -1); is.t.line.end.set(0.2f, 0.2f, 2);	// from below: back faces
		n = 0;
		collisionWorldForAllIntersections(&w, &is, countCB, &n);
		CHECK(n == 0, "back faces hit %d", n);
		is.type = INTERSECTPOINT;	// unsupported: nothing, returns the world
		n = 0;
		CHECK(collisionWorldForAllIntersections(&w, &is, countCB, &n) == &w && n == 0, "unsupported type");
		ct->destroy();
	}
}

int
main(int argc, char **argv)
{
	long nq = 3000;
	bool geo = true;
	int first = 1;
	for(; first < argc && argv[first][0] == '-'; first++){
		if(!strcmp(argv[first], "-n") && first+1 < argc) nq = atol(argv[++first]);
		else if(!strcmp(argv[first], "-g")) geo = false;
		else if(!strcmp(argv[first], "-s") && first+1 < argc) rs ^= (uint64_t)atol(argv[++first]) * 0x9E3779B97F4A7C15ull;
	}
	Engine::init();
	registerCollisionPlugin();
	Engine::open(nil);
	Engine::start();
	unitTests();
	Totals T; memset(&T, 0, sizeof T);
	int bad = 0;
	for(int i = first; i < argc; i++){
		long before = nErr;
		bool ok = testFile(argv[i], nq, geo, T) && nErr == before;
		if(!ok){ bad++; printf("FAIL %s\n", argv[i]); }
		for(size_t k = 0; k < errs.size(); k++) printf("    %s\n", errs[k].c_str());
		errs.clear();
	}
	int leaks = World::numAllocated + Geometry::numAllocated + Atomic::numAllocated + Frame::numAllocated + Material::numAllocated;
	printf("%ld files (%d failed), %ld sectors, %ld with tree, %ld triangles\n", T.files, bad, T.sectors, T.sectorsWithTree, T.tris);
	printf("stored trees: %ld splits, %ld leaves, max depth %ld, max leaf %ld, nodes with value outside the parent volume %ld, triangle vertices outside their leaf volume by rounding (<=1e-5) %ld, reserialised byte-exact %ld/%ld\n",
	       T.splits, T.leaves, T.maxDepth, T.maxLeaf, T.outOfParent, T.slop, T.blobExact, T.sectorsWithTree);
	printf("rebuilt trees %ld (max depth %ld), parser fuzz %ld mutations (%ld still parsed and were queried), geometry plugin tests %ld\n",
	       T.rebuilt, T.buildMaxDepth, T.fuzz, T.fuzzParsed, T.geoTests);
	printf("queries %ld (line %ld sphere %ld box %ld; with hits: %ld/%ld/%ld), total hits %ld, ambiguous boundary cases skipped %ld\n",
	       T.qs.queries, T.qs.byType[0], T.qs.byType[1], T.qs.byType[2], T.qs.hitQueries[0], T.qs.hitQueries[1], T.qs.hitQueries[2],
	       T.qs.hitsTotal, T.qs.ambiguous);
	printf("mismatches %ld (missing %ld, extra %ld), bad distances %ld, other errors %ld, leaked objects %d\n",
	       T.qs.mismatches, T.qs.missing, T.qs.extra, T.qs.distBad, nErr, leaks);
	printf("%s\n", (bad || nErr || leaks) ? "FAIL" : "PASS");
	return (bad || nErr || leaks) ? 1 : 0;
}
