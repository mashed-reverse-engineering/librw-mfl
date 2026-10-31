// Asset-parity oracle, librw side (plan_librw_integration S2-b): loads TXD / DFF / BSP files through librw (no GPU, in-memory raster driver) and writes the
// same normalised rows as ghidra/tools/asset_oracle/walk.py writes for the original exe's memory (row format: see the header of walk.py).
// Build: tests/build_asset_dump.sh      Run: asset_dump OUT.csv FILE...   (TXDs are loaded first into one merged current dictionary, as the game keeps them)
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rw.h"
#ifdef VIA_DLL		// S2-c: the same dump, but every load goes through the C ABI of mflrw_null.dll (capi/rwmfl.h) instead of linked librw sources
#include "capi/rwmfl.h"
struct RwMemory { uint8_t *start; uint32_t length; };	// RW 3.7 RwMemory (memory stream argument)
#endif

using namespace rw;
typedef std::vector<uint8_t> Buf;

#if !defined(MFL_FULL_BACKENDS) && !defined(VIA_DLL)	// native build: backends are not linked, their plugin registration is neutralised
namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}
#endif

// ---- in-memory raster driver (as tests/pitxd_test.cpp, only what readPI needs)
struct MemRas { std::vector<Buf> lv; Buf pal; int bpp; };
static std::map<Raster*, MemRas> g_ras;
static int numLevels(int w, int h){ int sz = w >= h ? w : h, n; for(n = 0; sz != 0; n++) sz /= 2; return n; }	// = Raster::calculateNumLevels
static Raster *mCreate(Raster *r)
{
	MemRas m;
	m.bpp = (r->format & (Raster::PAL4|Raster::PAL8)) ? 1 : 4;
	int n = (r->format & Raster::MIPMAP) ? numLevels(r->width, r->height) : 1;
	for(int i = 0; i < n; i++){
		int w = r->width >> i, h = r->height >> i;
		if(w < 1) w = 1;
		if(h < 1) h = 1;
		m.lv.push_back(Buf((size_t)w*h*m.bpp, 0));
	}
	if(m.bpp == 1) m.pal.assign(1024, 0);
	g_ras[r] = m;
	r->originalWidth = r->width; r->originalHeight = r->height;
	r->originalStride = r->stride = r->width*m.bpp; r->originalPixels = r->pixels = nil;
	return r;
}
static uint8 *mLock(Raster *r, int32 level, int32)
{
	MemRas &m = g_ras[r];
	if(r->pixels || level >= (int)m.lv.size()) return nil;
	int w = r->width >> level, h = r->height >> level;
	r->width = w ? w : 1; r->height = h ? h : 1;
	r->stride = r->width*m.bpp;
	r->pixels = m.lv[level].data();
	return r->pixels;
}
static void mUnlock(Raster *r, int32) { r->width = r->originalWidth; r->height = r->originalHeight; r->stride = r->originalStride; r->pixels = nil; }
static uint8 *mLockPal(Raster *r, int32) { return g_ras[r].pal.data(); }
static void mUnlockPal(Raster*) {}
static int32 mLevels(Raster *r) { return (int32)g_ras[r].lv.size(); }
static bool32 mFindFormat(Image *img, int32 type, int32 *w, int32 *h, int32 *d, int32 *f)
{
	*w = img->width; *h = img->height; *d = img->depth;
	#ifdef VIA_DLL
	bool alpha = true;	// Image::hasAlpha lives in the DLL; the PI loader never calls this driver hook
#else
	bool alpha = img->hasAlpha();
#endif
	*f = (img->depth == 8 ? Raster::PAL8 : img->depth == 4 ? Raster::PAL4 : 0) | (img->depth == 32 && !alpha ? Raster::C888 : Raster::C8888) | type;
	return img->depth == 32 || img->depth == 8 || img->depth == 4;
}
static bool32 mFromImage(Raster*, Image*) { return 1; }

// ---- rows
static std::set<std::string> rows, blocks;
static std::string fmt(const char *f, ...)
{
	char b[1024]; va_list ap; va_start(ap, f); vsnprintf(b, sizeof b, f, ap); va_end(ap); return b;
}
static std::string hx(float v) { uint32 u; memcpy(&u, &v, 4); return fmt("%08x", u); }
static uint32 crc32(const void *p, size_t n)
{
	static uint32 t[256]; if(!t[1]) for(uint32 i = 0; i < 256; i++){ uint32 c = i; for(int k = 0; k < 8; k++) c = c&1 ? 0xEDB88320u ^ (c>>1) : c>>1; t[i] = c; }
	uint32 c = 0xffffffffu; for(size_t i = 0; i < n; i++) c = t[(c ^ ((const uint8*)p)[i]) & 255] ^ (c >> 8); return ~c;
}
static std::string hs(const void *p, size_t n) { return n == 0 ? "0" : p ? fmt("%08x", crc32(p, n)) : "x"; }
static std::string sph(const Sphere &s) { return hx(s.center.x) + "," + hx(s.center.y) + "," + hx(s.center.z) + "," + hx(s.radius); }

static std::set<Texture*> seenTex;
static void texRow(Texture *t)
{
	if(!t || seenTex.count(t)) return;
	seenTex.insert(t);
	int w = 0, h = 0, d = 0;
	if(t->raster){ w = t->raster->width; h = t->raster->height; d = t->raster->depth; }
	rows.insert(fmt("T,%s,%s,%04x,%d,%d,%d", t->name, t->mask, t->filterAddressing & 0xffff, w, h, d));
}

static std::set<Geometry*> seenGeo;
static std::string geoRows(Geometry *g)
{
	int nv = g->numVertices, nt = g->numTriangles;
	std::string bs = "x,x,x,x", hv = "x", hn = "x";
	if(g->morphTargets){
		MorphTarget *m = &g->morphTargets[0];
		bs = sph(m->boundingSphere);
		if(m->vertices) hv = hs(m->vertices, nv*12);
		if(m->normals) hn = hs(m->normals, nv*12);
	}
	if(nv == 0) hv = "0";
	std::string gk = fmt("%d:%d:%d:%s", nv, nt, g->matList.numMaterials, hv.c_str());
	if(seenGeo.count(g)) return gk;
	seenGeo.insert(g);
	rows.insert(fmt("G,%s,%08x,%d,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s", gk.c_str(), g->flags, nv, nt, g->numMorphTargets, g->numTexCoordSets, g->matList.numMaterials,
		bs.c_str(), hv.c_str(), hn.c_str(), hs(g->triangles, nt*8).c_str(),
		g->numTexCoordSets > 0 ? hs(g->texCoords[0], nv*8).c_str() : "x", g->numTexCoordSets > 1 ? hs(g->texCoords[1], nv*8).c_str() : "x",
		hs(g->colors, nv*4).c_str()));
	for(int i = 0; i < g->matList.numMaterials; i++){
		Material *m = g->matList.materials[i];
		if(!m){ rows.insert(fmt("M,%s,%d,x,x,x,x,,", gk.c_str(), i)); continue; }
		uint32 rgba = m->color.red | m->color.green<<8 | m->color.blue<<16 | (uint32)m->color.alpha<<24;
		Texture *t = m->texture;
		texRow(t);
		rows.insert(fmt("M,%s,%d,%08x,%s,%s,%s,%s,%s", gk.c_str(), i, rgba, hx(m->surfaceProps.ambient).c_str(), hx(m->surfaceProps.specular).c_str(),
			hx(m->surfaceProps.diffuse).c_str(), t ? t->name : "", t ? t->mask : ""));
	}
	return gk;
}

static void frames(Frame *f, int d, std::vector<std::pair<Frame*,int> > &out)
{
	out.push_back(std::make_pair(f, d));
	std::vector<Frame*> kids;
	for(Frame *c = f->child; c; c = c->next) kids.push_back(c);
	std::sort(kids.begin(), kids.end(), [](Frame *a, Frame *b){
		uint32 x[12], y[12];
		memcpy(x, &a->matrix, 48); memcpy(y, &b->matrix, 48);
		return memcmp(x, y, 48) < 0; });
	for(size_t i = 0; i < kids.size(); i++) frames(kids[i], d+1, out);
}

static void clumpRows(Clump *c)
{
	std::vector<std::pair<Frame*,int> > fr;
	Frame *root = c->getFrame();
	if(root) frames(root, 0, fr);
	std::map<Frame*,int> fdep;
	for(size_t i = 0; i < fr.size(); i++) fdep[fr[i].first] = fr[i].second;
	std::vector<std::pair<std::string,std::string> > ar;
	long tv = 0, tt = 0;
	FORLIST(lnk, c->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		std::string gk = "none";
		if(a->geometry){ gk = geoRows(a->geometry); tv += a->geometry->numVertices; tt += a->geometry->numTriangles; }
		Frame *f = a->getFrame();
		ar.push_back(std::make_pair(
			fmt("%s,%s,%d,%02x,%08x,%d,%d", gk.c_str(), sph(a->boundingSphere).c_str(), f && fdep.count(f) ? fdep[f] : -1, a->object.object.flags,
			    a->interpolatorFlags, a->pipeline != nil, a->renderCB != nil), std::string()));
	}
	std::string gks;
	{ std::vector<std::string> g2; for(size_t i = 0; i < ar.size(); i++) g2.push_back(ar[i].first.substr(0, ar[i].first.find(','))); std::sort(g2.begin(), g2.end());
	  for(size_t i = 0; i < g2.size(); i++) gks += (i ? "," : "") + g2[i]; }
	std::string ck = fmt("%d:%ld:%ld:%s", (int)ar.size(), tv, tt, hs(gks.data(), gks.size()).c_str());
	std::vector<std::string> crow;
	crow.push_back(fmt("C,%s,%d,%d", ck.c_str(), (int)ar.size(), (int)fr.size()));
	for(size_t i = 0; i < fr.size(); i++){
		Matrix &m = fr[i].first->matrix;
		crow.push_back(fmt("F,%s,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%08x", ck.c_str(), fr[i].second,
			hx(m.right.x).c_str(), hx(m.right.y).c_str(), hx(m.right.z).c_str(), hx(m.up.x).c_str(), hx(m.up.y).c_str(), hx(m.up.z).c_str(),
			hx(m.at.x).c_str(), hx(m.at.y).c_str(), hx(m.at.z).c_str(), hx(m.pos.x).c_str(), hx(m.pos.y).c_str(), hx(m.pos.z).c_str(), m.flags));
	}
	std::sort(ar.begin(), ar.end());
	for(size_t i = 0; i < ar.size(); i++)
		crow.push_back(fmt("A,%s,%s", ck.c_str(), ar[i].first.c_str()));
	std::string blk = "#CLUMP " + ck;
	for(size_t i = 0; i < crow.size(); i++) blk += "\n" + crow[i];
	blocks.insert(blk);
}

static void bbox6(std::string &o, const BBox &b)
{
	o += hx(b.sup.x) + "," + hx(b.sup.y) + "," + hx(b.sup.z) + "," + hx(b.inf.x) + "," + hx(b.inf.y) + "," + hx(b.inf.z);
}
static void worldWalk(BspNode *n, std::vector<PlaneSector*> &pl, std::vector<WorldSector*> &se)
{
	if(n->isLeaf){ se.push_back((WorldSector*)n); return; }
	PlaneSector *p = (PlaneSector*)n;
	pl.push_back(p);
	worldWalk(p->left, pl, se); worldWalk(p->right, pl, se);
}
static void worldRows(World *w)
{
	std::vector<PlaneSector*> pl; std::vector<WorldSector*> se;
	worldWalk(w->root, pl, se);
	long nv = 0, nt = 0;
	for(size_t i = 0; i < se.size(); i++){ nv += se[i]->numVertices; nt += se[i]->numTriangles; }
	std::string allv;
	for(size_t i = 0; i < se.size(); i++) allv.append((const char*)se[i]->vertices, se[i]->vertices ? (size_t)se[i]->numVertices*12 : 0);
	std::string wk = fmt("%d:%d:%d:%ld:%ld:%s", (int)se.size(), (int)pl.size(), w->matList.numMaterials, nt, nv, hs(allv.data(), allv.size()).c_str());
	std::string bb; bbox6(bb, w->bbox);
	std::string blk = "#WORLD " + wk;
	blk += "\n" + fmt("W,%s,%08x,%d,%s", wk.c_str(), w->format, se.empty() ? 0 : se[0]->numTexCoordSets, bb.c_str());
	for(size_t i = 0; i < pl.size(); i++)
		blk += "\n" + fmt("WP,%s,%d,%d,%s,%s,%s", wk.c_str(), (int)i, pl[i]->type, hx(pl[i]->value).c_str(), hx(pl[i]->leftValue).c_str(), hx(pl[i]->rightValue).c_str());
	for(size_t i = 0; i < se.size(); i++){
		WorldSector *s = se[i];
		std::string tb, ob; bbox6(tb, s->bbox); bbox6(ob, s->planeBBox);
		blk += "\n" + fmt("WS,%s,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s", wk.c_str(), (int)i, s->numVertices, s->numTriangles, s->matIdBase, tb.c_str(), ob.c_str(),
			hs(s->vertices, s->numVertices*12).c_str(), hs(s->triangles, s->numTriangles*8).c_str(), s->normals ? "1" : "0",
			s->numVertices == 0 ? "0" : s->numTexCoordSets > 0 ? hs(s->texCoords[0], s->numVertices*8).c_str() : "x", hs(s->colors, s->numVertices*4).c_str());
	}
	for(int i = 0; i < w->matList.numMaterials; i++){
		Material *m = w->matList.materials[i];
		if(!m){ blk += "\n" + fmt("WM,%s,%d,x,x,x,x,,", wk.c_str(), i); continue; }
		uint32 rgba = m->color.red | m->color.green<<8 | m->color.blue<<16 | (uint32)m->color.alpha<<24;
		Texture *t = m->texture; texRow(t);
		blk += "\n" + fmt("WM,%s,%d,%08x,%s,%s,%s,%s,%s", wk.c_str(), i, rgba, hx(m->surfaceProps.ambient).c_str(), hx(m->surfaceProps.specular).c_str(),
			hx(m->surfaceProps.diffuse).c_str(), t ? t->name : "", t ? t->mask : "");
	}
	blocks.insert(blk);
}

static bool readFile(const char *fn, Buf &b)
{
	FILE *f = fopen(fn, "rb"); if(!f) return false;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	b.resize(n); bool ok = fread(b.data(), 1, n, f) == (size_t)n; fclose(f); return ok;
}
static bool ends(const std::string &s, const char *e)
{
	size_t n = strlen(e); if(s.size() < n) return false;
	for(size_t i = 0; i < n; i++) if(toupper(s[s.size()-n+i]) != e[i]) return false;
	return true;
}

int main(int argc, char **argv)
{
	if(argc < 3){ fprintf(stderr, "usage: asset_dump OUT.csv FILE...\n"); return 2; }
#ifdef VIA_DLL
	if(!RwEngineInit(nil, 0, 0) || !RwEngineOpen(nil) || !RwEngineStart()){ fprintf(stderr, "engine start failed\n"); return 2; }
	Engine *eng = (Engine*)rwmfl_engine();
#else
	Engine::init();
	Engine::open(nil);
	Engine::start();
	Engine *eng = engine;
#endif
	{ Driver *d = eng->driver[PLATFORM_NULL];
	  d->rasterCreate = mCreate; d->rasterLock = mLock; d->rasterUnlock = mUnlock; d->rasterLockPalette = mLockPal; d->rasterUnlockPalette = mUnlockPal;
	  d->rasterNumLevels = mLevels; d->imageFindRasterFormat = mFindFormat; d->rasterFromImage = mFromImage; }
	// groups of files separated by "--": each group gets its own merged texture dictionary (a course / car piz is loaded on its own); nothing is destroyed (no address reuse)
	std::vector<std::vector<std::string> > groups(1);
	std::vector<std::string> av(argv, argv+argc);
	if(argc == 3 && argv[2][0] == '@'){	// @listfile: one argument per line (Wine's command line is limited)
		av.resize(2);
		if(FILE *lf = fopen(argv[2]+1, "r")){ char ln[1024]; while(fgets(ln, sizeof ln, lf)){ size_t n = strlen(ln); while(n && (ln[n-1] == '\n' || ln[n-1] == '\r')) ln[--n] = 0; if(n) av.push_back(ln); } fclose(lf); }
	}
	for(size_t i = 2; i < av.size(); i++){ if(av[i] == "--") groups.push_back(std::vector<std::string>()); else groups.back().push_back(av[i]); }
	int nt = 0, nd = 0, nb = 0, bad = 0;
	for(size_t gi = 0; gi < groups.size(); gi++){
	std::vector<std::string> &files = groups[gi];
	std::stable_sort(files.begin(), files.end(), [](const std::string &a, const std::string &b){ return ends(a, ".TXD") > ends(b, ".TXD"); });
#ifdef VIA_DLL
	RwTexDictionary *merged = RwTexDictionaryCreate();
	RwTexDictionarySetCurrent(merged);
#else
	TexDictionary *merged = TexDictionary::create();
	TexDictionary::setCurrent(merged);
#endif
	for(size_t i = 0; i < files.size(); i++){
		Buf b;
		if(!readFile(files[i].c_str(), b)){ fprintf(stderr, "cannot read %s\n", files[i].c_str()); bad++; continue; }
#ifdef VIA_DLL
		// TXD and BSP are read as FILENAME streams, DFF as MEMORY streams: both RwStreamOpen paths are exercised
		RwMemory mem = { b.data(), (uint32)b.size() };
		RwStream *in = ends(files[i], ".DFF") ? RwStreamOpen(2 + 1, 1, &mem) : RwStreamOpen(2, 1, files[i].c_str());
		if(!in){ fprintf(stderr, "stream open failed: %s\n", files[i].c_str()); bad++; continue; }
#define CLOSE() RwStreamClose(in, nil)
#else
		StreamMemory sin; sin.open(b.data(), (uint32)b.size()); Stream *in = &sin;
#define CLOSE()
#endif
		if(ends(files[i], ".TXD")){
#ifdef VIA_DLL
			TexDictionary *d = (TexDictionary*)RtPITexDictionaryStreamRead(in);
#else
			TexDictionary *d = TexDictionary::streamReadPI(in);
#endif
			if(!d){ fprintf(stderr, "TXD failed: %s\n", files[i].c_str()); bad++; CLOSE(); continue; }
			std::vector<std::string> names;
			FORLIST(lnk, d->textures) names.push_back(Texture::fromDict(lnk)->name);
			std::sort(names.begin(), names.end());
			std::string all; for(size_t k = 0; k < names.size(); k++) all += (k ? "," : "") + names[k];
			rows.insert(fmt("TD,%d,%s", (int)names.size(), hs(all.data(), all.size()).c_str()));
			std::vector<Texture*> ts;
			FORLIST(lnk, d->textures) ts.push_back(Texture::fromDict(lnk));
			for(size_t k = 0; k < ts.size(); k++){
				Texture *t = ts[k]; texRow(t);
#ifdef VIA_DLL
				if(!RwTexDictionaryFindNamedTexture(merged, t->name)){ t->addRef(); RwTexDictionaryAddTexture(merged, (RwTexture*)t); }
#else
				if(!merged->find(t->name)){ t->addRef(); merged->add(t); }
#endif
			}
			nd++;
		}else if(ends(files[i], ".DFF")){
			uint32 len, ver;
#ifdef VIA_DLL
			if(!RwStreamFindChunk(in, ID_CLUMP, &len, &ver)){ fprintf(stderr, "no clump chunk: %s\n", files[i].c_str()); bad++; CLOSE(); continue; }
			Clump *c = (Clump*)RpClumpStreamRead(in);
#else
			if(!findChunk(in, ID_CLUMP, &len, &ver)){ fprintf(stderr, "no clump chunk: %s\n", files[i].c_str()); bad++; continue; }
			Clump *c = Clump::streamRead(in);
#endif
			if(!c){ fprintf(stderr, "DFF failed: %s\n", files[i].c_str()); bad++; CLOSE(); continue; }
			clumpRows(c); nt++;
		}else if(ends(files[i], ".BSP")){
			uint32 len, ver;
#ifdef VIA_DLL
			if(!RwStreamFindChunk(in, ID_WORLD, &len, &ver)){ fprintf(stderr, "no world chunk: %s\n", files[i].c_str()); bad++; CLOSE(); continue; }
			World *w = (World*)RpWorldStreamRead(in);
#else
			if(!findChunk(in, ID_WORLD, &len, &ver)){ fprintf(stderr, "no world chunk: %s\n", files[i].c_str()); bad++; continue; }
			World *w = World::streamRead(in);
#endif
			if(!w){ fprintf(stderr, "BSP failed: %s\n", files[i].c_str()); bad++; CLOSE(); continue; }
			worldRows(w); nb++;
		}
		CLOSE();
	}
	}
	FILE *o = fopen(argv[1], "w");
	for(std::set<std::string>::iterator it = rows.begin(); it != rows.end(); ++it) fprintf(o, "%s\n", it->c_str());
	for(std::set<std::string>::iterator it = blocks.begin(); it != blocks.end(); ++it) fprintf(o, "%s\n", it->c_str());
	fclose(o);
	printf("dicts %d clumps %d bsps %d failed %d rows %d\n", nd, nt, nb, bad, (int)rows.size());
	return bad != 0;
}
