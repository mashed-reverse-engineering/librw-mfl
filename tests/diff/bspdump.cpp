// Review dump tool (copy of tests/bsp_test.cpp main shell): prints the librw World in the
// canonical format orig_reader.py emits, so both can be diffed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
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
static unsigned long long fnv(const void *p, size_t n){
	unsigned long long x = 0xcbf29ce484222325ULL; const uint8 *b = (const uint8*)p;
	for(size_t i = 0; i < n; i++){ x ^= b[i]; x *= 0x100000001b3ULL; }
	return x;
}
static void rec(BspNode *n){
	if(n->isLeaf) return;
	PlaneSector *p = (PlaneSector*)n;
	printf("plane %d %.6g %.6g %.6g %s %s\n", p->type, p->value, p->leftValue, p->rightValue,
	       p->left->isLeaf ? "S" : "P", p->right->isLeaf ? "S" : "P");
	rec(p->left); rec(p->right);
}
int main(int argc, char **argv){
	Engine::init(); Engine::open(nil); Engine::start();
	FILE *fp = fopen(argv[1], "rb"); fseek(fp, 0, SEEK_END); uint32 len = ftell(fp); fseek(fp, 0, SEEK_SET);
	uint8 *data = (uint8*)malloc(len); fread(data, 1, len, fp); fclose(fp);
	StreamMemory in; in.open(data, len);
	uint32 clen, ver;
	if(!findChunk(&in, ID_WORLD, &clen, &ver)){ printf("NOWORLD\n"); return 1; }
	World *w = World::streamRead(&in);
	if(w == nil){ printf("READFAIL\n"); return 1; }
	int nsets = w->sectors && w->numSectors ? w->sectors[0]->numTexCoordSets : -1;
	printf("world fmt %08X nsets %d tri %d vert %d plane %d sect %d nmat %d\n", w->format, nsets,
	       w->numTriangles, w->numVertices, w->numPlaneSectors, w->numSectors, w->matList.numMaterials);
	printf("bbox inf %.6g %.6g %.6g sup %.6g %.6g %.6g\n", w->bbox.inf.x, w->bbox.inf.y, w->bbox.inf.z, w->bbox.sup.x, w->bbox.sup.y, w->bbox.sup.z);
	printf("invorigin %.6g %.6g %.6g\n", w->invOrigin.x, w->invOrigin.y, w->invOrigin.z);
	rec(w->root);
	for(int i = 0; i < w->numSectors; i++){
		WorldSector *s = w->sectors[i];
		int nv = s->numVertices, nt = s->numTriangles;
		printf("sector %d base %d tri %d vert %d tight %.6g %.6g %.6g %.6g %.6g %.6g\n", i, s->matIdBase, nt, nv,
		       s->bbox.inf.x, s->bbox.inf.y, s->bbox.inf.z, s->bbox.sup.x, s->bbox.sup.y, s->bbox.sup.z);
		char nb[32] = "-", cb[32] = "-", uv[200] = "";
		if(s->normals) snprintf(nb, sizeof nb, "%016llx", fnv(s->normals, 12*nv));
		if(s->colors) snprintf(cb, sizeof cb, "%016llx", fnv(s->colors, 4*nv));
		for(int k = 0; nv && k < s->numTexCoordSets; k++){
			char t[40]; snprintf(t, sizeof t, "%s%016llx", k ? "," : "", nv ? fnv(s->texCoords[k], 8*nv) : fnv("", 0));
			strcat(uv, t);
		}
		if(!uv[0]) strcpy(uv, "-");
		printf("  plbox %.6g %.6g %.6g %.6g %.6g %.6g\n", s->planeBBox.inf.x, s->planeBBox.inf.y, s->planeBBox.inf.z, s->planeBBox.sup.x, s->planeBBox.sup.y, s->planeBBox.sup.z);
		std::vector<uint16> tr;
		for(int k = 0; k < nt; k++){ Triangle *t = &s->triangles[k];
			tr.push_back(t->v[0]); tr.push_back(t->v[1]); tr.push_back(t->v[2]); tr.push_back(s->matIdBase + t->matId); }
		printf("  v %016llx n %s c %s uv %s t %016llx\n", fnv(s->vertices, 12*nv), nb, cb, uv, fnv(tr.data(), 2*tr.size()));
	}
	return 0;
}
