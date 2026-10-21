// Standalone BSP world loader test: no GPU. Loads each given .BSP with
// rw::World::streamRead, validates structure and prints statistics.
// Build: tests/build_bsp_test.sh     Run: bsp_test FILE.BSP...   (exit 1 on any failure)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include "rw.h"

using namespace rw;

// no platform backend is linked: neutralise the per-platform plugin registration
namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}

struct Stats {
	std::vector<std::string> err;
	int planes, leaves, coll, maxDepth;
	long ntri, nvert;
	int minMat, maxMat;
};

#define CHECK(c, ...) do{ if(!(c)){ char b_[256]; snprintf(b_, sizeof b_, __VA_ARGS__); st.err.push_back(b_); } }while(0)

static void
walk(BspNode *n, Stats &st, int depth, const BBox *parent)
{
	if(depth > st.maxDepth) st.maxDepth = depth;
	CHECK(n != nil, "null tree node");
	if(n == nil) return;
	if(n->isLeaf){
		st.leaves++;
		return;
	}
	PlaneSector *p = (PlaneSector*)n;
	st.planes++;
	CHECK(p->type == 0 || p->type == 4 || p->type == 8, "plane type %d", p->type);
	CHECK(p->left && p->right, "plane without both children");
	walk(p->left, st, depth+1, parent);
	walk(p->right, st, depth+1, parent);
}

static bool
testFile(const char *path, Stats &st, World **out)
{
	uint32 len = 0;
	uint8 *data = nil;
	if(FILE *fp = fopen(path, "rb")){	// engine file functions need Engine::open (a backend)
		fseek(fp, 0, SEEK_END); len = ftell(fp); fseek(fp, 0, SEEK_SET);
		data = (uint8*)malloc(len ? len : 1);
		if(fread(data, 1, len, fp) != len){ free(data); data = nil; }
		fclose(fp);
	}
	if(data == nil){ st.err.push_back("cannot read file"); return false; }
	StreamMemory in;
	in.open(data, len);
	uint32 clen, ver;
	if(!findChunk(&in, ID_WORLD, &clen, &ver)){
		st.err.push_back("no WORLD chunk"); free(data); return false; }
	CHECK(clen + 12 == len, "world chunk length %u does not cover file %u", clen+12, len);
	World *w = World::streamRead(&in);
	if(w == nil){ st.err.push_back("World::streamRead failed"); free(data); return false; }
	CHECK(in.tell() == len, "%u trailing bytes after world", len - in.tell());
	in.close();
	free(data);

	st.planes = st.leaves = st.coll = st.maxDepth = 0;
	st.ntri = st.nvert = 0; st.minMat = 1<<30; st.maxMat = -1;
	walk(w->root, st, 0, &w->bbox);
	CHECK(st.planes == w->numPlaneSectors, "plane count %d != header %d", st.planes, w->numPlaneSectors);
	CHECK(st.leaves == w->numSectors, "leaf count %d != header %d", st.leaves, w->numSectors);

	const float eps = 1e-3f;
	CHECK(w->bbox.inf.x <= w->bbox.sup.x && w->bbox.inf.y <= w->bbox.sup.y && w->bbox.inf.z <= w->bbox.sup.z, "world bbox inverted");
	int nm = w->matList.numMaterials;
	CHECK(nm > 0, "no materials");
	for(int i = 0; i < nm; i++)
		CHECK(w->matList.materials[i] != nil, "null material %d", i);
	for(int s = 0; s < w->numSectors; s++){
		WorldSector *sec = w->sectors[s];
		CHECK(sec != nil, "sector %d missing", s);
		if(!sec) continue;
		st.ntri += sec->numTriangles; st.nvert += sec->numVertices;
		BBox *b = &sec->bbox;
		CHECK(b->inf.x <= b->sup.x && b->inf.y <= b->sup.y && b->inf.z <= b->sup.z, "sector %d bbox inverted", s);
		CHECK(b->inf.x >= w->bbox.inf.x - eps && b->inf.y >= w->bbox.inf.y - eps && b->inf.z >= w->bbox.inf.z - eps &&
		      b->sup.x <= w->bbox.sup.x + eps && b->sup.y <= w->bbox.sup.y + eps && b->sup.z <= w->bbox.sup.z + eps,
		      "sector %d bbox outside world bbox", s);
		for(int i = 0; i < sec->numVertices; i++){
			V3d v = sec->vertices[i];
			CHECK(isfinite(v.x) && isfinite(v.y) && isfinite(v.z), "sector %d vertex %d not finite", s, i);
			CHECK(v.x >= b->inf.x-eps && v.y >= b->inf.y-eps && v.z >= b->inf.z-eps &&
			      v.x <= b->sup.x+eps && v.y <= b->sup.y+eps && v.z <= b->sup.z+eps, "sector %d vertex %d outside sector bbox", s, i);
			if(!st.err.empty() && st.err.size() > 20) return false;
		}
		for(int i = 0; i < sec->numTriangles; i++){
			Triangle *t = &sec->triangles[i];
			for(int k = 0; k < 3; k++)
				CHECK(t->v[k] < sec->numVertices, "sector %d tri %d index %d out of range", s, i, t->v[k]);
			int m = sec->matIdBase + t->matId;
			CHECK(m >= 0 && m < nm, "sector %d tri %d material %d (base %d) out of range %d", s, i, m, sec->matIdBase, nm);
			if(m < st.minMat) st.minMat = m;
			if(m > st.maxMat) st.maxMat = m;
			if(st.err.size() > 20) return false;
		}
		if(sec->collision) st.coll++;
		// render path data (no GPU): geometry must build
		Geometry *g = sec->getGeometry();
		CHECK(g != nil, "sector %d geometry", s);
		if(g){
			CHECK(g->numTriangles == sec->numTriangles && g->numVertices == sec->numVertices, "sector %d geometry counts", s);
			CHECK(g->meshHeader && g->meshHeader->totalIndices == (uint32)sec->numTriangles*3, "sector %d mesh indices", s);
		}
	}
	CHECK(st.ntri == w->numTriangles, "triangle total %ld != header %d", st.ntri, w->numTriangles);
	CHECK(st.nvert == w->numVertices, "vertex total %ld != header %d", st.nvert, w->numVertices);
	// accessor round trip
	struct C { static WorldSector *cb(WorldSector *, void *d){ (*(int*)d)++; return (WorldSector*)1; } };
	int cnt = 0;
	w->forAllWorldSectors(C::cb, &cnt);
	CHECK(cnt == w->numSectors, "forAllWorldSectors visited %d", cnt);
	*out = w;
	return st.err.empty();
}

int
main(int argc, char **argv)
{
	Engine::init();
	Engine::open(nil);	// null device: textures are created without pixel data
	Engine::start();
	int fails = 0, files = 0;
	long T = 0, V = 0, S = 0, P = 0;
	for(int i = 1; i < argc; i++){
		Stats st; World *w = nil;
		bool ok = testFile(argv[i], st, &w);
		files++;
		if(w){
			World::numAllocated += 0;
			printf("%s %s: sectors %d planes %d tris %d verts %d mats %d fmt %08X depth %d coll %d bbox (%.1f %.1f %.1f)-(%.1f %.1f %.1f) matidx %d..%d\n",
			       ok ? "PASS" : "FAIL", argv[i], w->numSectors, w->numPlaneSectors, w->numTriangles, w->numVertices,
			       w->matList.numMaterials, w->format, st.maxDepth, st.coll,
			       w->bbox.inf.x, w->bbox.inf.y, w->bbox.inf.z, w->bbox.sup.x, w->bbox.sup.y, w->bbox.sup.z, st.minMat, st.maxMat);
			S += w->numSectors; P += w->numPlaneSectors; T += w->numTriangles; V += w->numVertices;
			w->destroy();
		}else
			printf("FAIL %s\n", argv[i]);
		for(size_t k = 0; k < st.err.size() && k < 5; k++)
			printf("    %s\n", st.err[k].c_str());
		if(!ok) fails++;
	}
	int leaks = World::numAllocated + Geometry::numAllocated + Material::numAllocated + Atomic::numAllocated + Frame::numAllocated;
	printf("%d files, %d failed; totals: %ld sectors %ld planes %ld tris %ld verts; leaked objects %d (world %d geo %d mat %d atomic %d frame %d)\n",
	       files, fails, S, P, T, V, leaks, World::numAllocated, Geometry::numAllocated, Material::numAllocated, Atomic::numAllocated, Frame::numAllocated);
	return fails || leaks ? 1 : 0;
}
