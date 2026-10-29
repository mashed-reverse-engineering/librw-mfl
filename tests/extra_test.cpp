// Tests for the MFL additions: world sector plugin registry/callbacks, HAnim update flags,
// RpRandom, RpSpline, RpLODAtomic, RpMorph. No GPU. Build: tests/build_extra_test.sh
// Run: extra_test [BSPFILE...]   (the BSP files exercise the sector plugin registry; exit 1 on failure)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "rw.h"
#include "rwextra.h"

using namespace rw;

namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}

static int nErr, nChk;
#define CHECK(c, ...) do{ nChk++; if(!(c)){ nErr++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } }while(0)
static bool near_(float a, float b, float e = 1e-4f) { return fabsf(a-b) <= e; }

// ---- sector plugin: counts chunk 0x50E (bin mesh) payload bytes per sector
struct SecPlg { int32 binBytes; int32 ctor; };
static int32 secOff;
static int secDtorCalls;
static void *secCtor(void *o, int32 off, int32) { SecPlg *p = PLUGINOFFSET(SecPlg, o, off); p->binBytes = -1; p->ctor = 0x1234; return o; }
static void *secDtor(void *o, int32, int32) { secDtorCalls++; return o; }
static Stream *secRead(Stream *s, int32 len, void *o, int32 off, int32)
{
	PLUGINOFFSET(SecPlg, o, off)->binBytes = len;
	s->seek(len);
	return s;
}

static int rendered;
static void myRender(WorldSector *) { rendered++; }

static void testSectorApi(const char *bsp)
{
	World *w = World::create();
	CHECK(w->getSectorRenderCallBack() != nil, "default render callback");
	WorldSectorRenderCallBack def = w->getSectorRenderCallBack();
	w->setSectorRenderCallBack(myRender);
	CHECK(w->getSectorRenderCallBack() == myRender, "set cb");
	rendered = 0;
	WorldSector *s = WorldSector::create();
	s->world = w;
	CHECK(s->getWorld() == w, "sector world");
	CHECK(PLUGINOFFSET(SecPlg, s, secOff)->ctor == 0x1234, "sector plugin ctor");
	s->render();
	CHECK(rendered == 1, "render goes through callback (%d)", rendered);
	w->setSectorRenderCallBack(nil);
	CHECK(w->getSectorRenderCallBack() == def, "nil restores default");
	int d0 = secDtorCalls;
	s->destroy();
	CHECK(secDtorCalls == d0+1, "sector plugin dtor");
	w->destroy();

	if(!bsp) return;
	FILE *fp = fopen(bsp, "rb");
	if(!fp){ CHECK(0, "open %s", bsp); return; }
	fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
	std::vector<uint8> buf(n);
	if(fread(buf.data(), 1, n, fp) != (size_t)n){ fclose(fp); CHECK(0, "read"); return; }
	fclose(fp);
	StreamMemory in;
	in.open(buf.data(), n);
	ChunkHeaderInfo h;
	readChunkHeaderInfo(&in, &h);
	CHECK(h.type == ID_WORLD, "world chunk");
	World *world = World::streamRead(&in);
	CHECK(world != nil, "world read");
	if(!world) return;
	int seen = 0, ok = 1;
	for(int32 i = 0; i < world->numSectors; i++){
		SecPlg *p = PLUGINOFFSET(SecPlg, world->sectors[i], secOff);
		ok &= p->ctor == 0x1234;
		if(p->binBytes > 0) seen++;
	}
	CHECK(ok, "all sectors constructed");
	CHECK(seen > 0, "registered chunk 0x50E reached the plugin in %d of %d sectors", seen, world->numSectors);
	// forAllMaterials / forAllClumps
	int nm = 0;
	world->forAllMaterials([](Material *, void *d){ ++*(int*)d; return (Material*)(void*)1; }, &nm);
	CHECK(nm == world->matList.numMaterials, "forAllMaterials %d", nm);
	nm = 0;
	world->forAllClumps([](Clump *, void *d){ ++*(int*)d; return (Clump*)(void*)1; }, &nm);
	CHECK(nm == 0, "no clumps");
	int d1 = secDtorCalls, ns = world->numSectors;
	world->destroy();
	CHECK(secDtorCalls == d1+ns, "dtor per sector (%d)", secDtorCalls-d1);
	printf("  %s: %d sectors, plugin saw bin mesh in %d\n", bsp, ns, seen);
}

static void testRandom(void)
{
	// reference values from an independent implementation (python) of the disassembled routine
	extra::randomSeed(0x9A319039u);
	static const uint32 ref[] = { 1373023817u, 253234266u, 2145093567u, 898975974u, 266244007u, 1683270901u, 1858982574u, 704667763u };
	for(int i = 0; i < 8; i++){
		uint32 v = extra::random();
		CHECK(v == ref[i], "random[%d]=%u want %u", i, v, ref[i]);
	}
	extra::randomSeed(1);
	uint32 a = extra::random();
	extra::randomSeed(1);
	CHECK(a == extra::random(), "reseed repeats");
	for(int i = 0; i < 1000; i++)
		CHECK(extra::random() < 0x80000000u, "31 bit");
}

static void testSpline(void)
{
	V3d pts[6];
	for(int i = 0; i < 6; i++)
		pts[i] = makeV3d(10.0f*cosf(i*1.0f), 5.0f*i, 10.0f*sinf(i*1.0f));
	for(int type = 1; type <= 2; type++){
		extra::Spline *s = extra::Spline::create(6, type, pts);
		CHECK(s != nil, "create");
		int nseg = s->numSegments();
		for(int i = 0; i <= nseg; i++){
			V3d p;
			s->findPosition(10, (float)i/nseg, &p, nil);
			V3d e = pts[i%6];
			CHECK(length(sub(p, e)) < 1e-3f, "type %d passes through point %d", type, i);
		}
		// tangent against finite difference, continuity across knots
		for(float t = 0.05f; t < 0.95f; t += 0.07f){
			V3d p0, p1, tg;
			s->findPosition(10, t-1e-3f, &p0, nil);
			s->findPosition(10, t+1e-3f, &p1, nil);
			s->findPosition(10, t, nil, &tg);
			V3d fd = scale(sub(p1, p0), 1.0f/2e-3f);
			CHECK(length(sub(fd, tg)) < 0.02f*length(tg)+0.05f, "tangent %g", length(sub(fd, tg)));
		}
		if(type == 2){
			V3d a, b, ta, tb;
			s->findPosition(10, 0.0f, &a, &ta);
			s->findPosition(10, 1.0f, &b, &tb);
			CHECK(length(sub(a, b)) < 1e-3f && length(sub(ta, tb)) < 1e-2f*length(ta), "closed loop is periodic");
		}
		Matrix m;
		V3d up = makeV3d(0, 1, 0);
		s->findMatrix(10, 0.3f, &up, &m);
		CHECK(near_(length(m.right), 1) && near_(length(m.at), 1) && near_(length(m.up), 1), "matrix axes");
		CHECK(near_(dot(m.right, m.at), 0) && near_(dot(m.up, m.at), 0), "orthogonal");
		// stream round trip
		uint8 buf[4096];
		StreamMemory st;
		st.open(buf, 0, sizeof buf);
		strcpy(s->name, "path");
		s->streamWrite(&st);
		CHECK(st.tell() == s->streamGetSize(), "size");
		StreamMemory rd;
		rd.open(buf, st.tell());
		extra::Spline *r = extra::Spline::streamRead(&rd);
		CHECK(r && r->numCtrlPoints == 6 && r->type == type && !strcmp(r->name, "path") &&
		      !memcmp(r->ctrlPoints, pts, sizeof pts), "stream round trip");
		if(r) r->destroy();
		extra::Spline *c = s->clone();
		V3d pa, pb;
		s->findPosition(10, 0.37f, &pa, nil);
		c->findPosition(10, 0.37f, &pb, nil);
		CHECK(equal(pa, pb), "clone");
		c->destroy();
		s->destroy();
	}
	CHECK(extra::Spline::create(1, 1, pts) == nil, "reject 1 point");
}

static void testLodMorph(void)
{
	Geometry *g0 = Geometry::create(3, 1, Geometry::TRISTRIP|0), *g1 = Geometry::create(3, 1, 0), *g2 = Geometry::create(3, 1, 0);
	Atomic *a = Atomic::create();
	Frame *f = Frame::create();
	a->setFrame(f);
	CHECK(extra::lodGetCurrentLOD(a) == -1, "no lod yet");
	extra::lodSetGeometry(a, 0, g0);
	extra::lodSetGeometry(a, 1, g1);
	extra::lodSetGeometry(a, 3, g2);
	CHECK(extra::lodGetCurrentLOD(a) == 0 && a->geometry == g0, "first geometry current");
	CHECK(extra::lodGetGeometry(a, 3) == g2, "get");
	extra::lodSetRange(a, 10.0f);
	CHECK(extra::lodGetRange(a) == 10.0f, "range");
	Camera *cam = Camera::create();
	Frame *cf = Frame::create();
	cam->setFrame(cf);
	extra::lodSetCamera(cam);
	V3d t = makeV3d(0, 0, 25.0f);
	f->translate(&t, COMBINEREPLACE);
	extra::lodSelectLOD(a);
	CHECK(extra::lodGetCurrentLOD(a) == 1 && a->geometry == g1, "dist 25 / range 10 -> lod 2 -> falls back to lod 1 (%d)", extra::lodGetCurrentLOD(a));
	t.z = 500.0f;
	f->translate(&t, COMBINEREPLACE);
	extra::lodSelectLOD(a);
	CHECK(extra::lodGetCurrentLOD(a) == 3 && a->geometry == g2, "far -> coarsest");
	CHECK(extra::lodSetCurrentLOD(a, 2) == nil, "missing lod rejected");
	extra::lodHookRender(a);
	CHECK(a->renderCB != Atomic::defaultRenderCB, "hooked");
	extra::lodUnHookRender(a);
	CHECK(a->renderCB == Atomic::defaultRenderCB, "unhooked");
	Atomic *c = a->clone();
	CHECK(c && extra::lodGetGeometry(c, 3) == g2, "clone copies lods");
	if(c) c->destroy();

	// morph
	Geometry *mg = Geometry::create(3, 1, 0);
	mg->addMorphTargets(1);
	CHECK(mg->numMorphTargets == 2, "targets");
	extra::morphCreateInterpolators(mg, 2);
	CHECK(extra::morphSetInterpolator(mg, 0, 0, 1, 2.0f) == mg, "set interp");
	CHECK(extra::morphSetInterpolator(mg, 1, 0, 5, 1.0f) == nil, "bad target rejected");
	CHECK(extra::morphGetInterpolator(mg, 0)->recipTime == 0.5f, "recip");
	Atomic *ma = Atomic::create();
	ma->setGeometry(mg, 0);
	CHECK(extra::morphSetCurrentInterpolator(ma, 0) == ma, "cur");
	extra::morphSetTime(ma, 1.0f);
	CHECK(near_(extra::morphGetBlend(ma), 0.5f), "blend");
	extra::morphAddTime(ma, 5.0f);
	CHECK(near_(extra::morphGetBlend(ma), 1.0f), "clamped");
	ma->destroy();
	mg->destroy();
	cam->destroy();
	a->destroy();
	f->destroy(); cf->destroy();
	g0->destroy(); g1->destroy(); g2->destroy();
}

static void testHAnim(void)
{
	// node0 root; node1 A is a leaf with siblings (PUSH|POP: keep the parent for the next sibling); node2 B
	int32 nflags[3] = { 0, HAnimHierarchy::PUSH|HAnimHierarchy::POP, 0 };
	int32 ids[3] = { 10, 11, 12 };
	HAnimHierarchy *h = HAnimHierarchy::create(3, nflags, ids,
		HAnimHierarchy::UPDATEMODELLINGMATRICES | HAnimHierarchy::UPDATELTMS, sizeof(HAnimInterpFrame));
	AnimInterpolatorInfo *info = AnimInterpolatorInfo::find(1);
	CHECK(info != nil, "hanim interp registered");
	Animation *anim = Animation::create(info, 6, 0, 1.0f);
	HAnimKeyFrame *kf = (HAnimKeyFrame*)anim->keyframes;
	V3d tr[3] = { makeV3d(1,0,0), makeV3d(0,2,0), makeV3d(0,0,3) };
	for(int i = 0; i < 6; i++){
		kf[i].q = makeQuat(1, makeV3d(0,0,0));
		kf[i].t = tr[i%3];
		kf[i].time = i < 3 ? 0.0f : 1.0f;
		kf[i].prev = i < 3 ? &kf[i] : &kf[i-3];
	}
	h->interpolator->setCurrentAnim(anim);
	h->interpolator->addTime(0.5f);

	Frame *root = Frame::create(), *fa = Frame::create(), *fb = Frame::create();
	root->addChild(fa); root->addChild(fb);
	HAnimData::get(root)->id = 10; HAnimData::get(fa)->id = 11; HAnimData::get(fb)->id = 12;
	h->parentFrame = root;
	h->attach();
	CHECK(HAnimData::get(root)->hierarchy == h, "attach sets root hierarchy");
	CHECK(h->nodeInfo[0].frame == root && h->nodeInfo[1].frame == fa && h->nodeInfo[2].frame == fb, "frames attached");
	h->updateMatrices();
	Matrix *m = h->matrices;
	CHECK(near_(m[0].pos.x, 1) && near_(m[1].pos.x, 1) && near_(m[1].pos.y, 2), "A under root: %g %g", m[1].pos.x, m[1].pos.y);
	CHECK(near_(m[2].pos.x, 1) && near_(m[2].pos.y, 0) && near_(m[2].pos.z, 3), "B is a sibling of A (pop): %g %g %g", m[2].pos.x, m[2].pos.y, m[2].pos.z);
	CHECK(near_(fa->matrix.pos.y, 2) && near_(fa->matrix.pos.x, 0), "modelling matrix = local key frame");
	Matrix *l = fb->getLTM();
	CHECK(near_(l->pos.z, 3) && near_(l->pos.x, 1), "ltm written");
	// NOMATRICES must not crash
	h->detach();
	CHECK(h->nodeInfo[1].frame == nil, "detach");
	h->flags |= HAnimHierarchy::NOMATRICES;
	Matrix *saved = h->matrices; void *savedU = h->matricesUnaligned;
	h->matrices = nil;
	h->updateMatrices();
	h->matrices = saved;
	h->flags &= ~HAnimHierarchy::NOMATRICES;
	// unbalanced pop must not crash
	h->nodeInfo[2].flags = HAnimHierarchy::POP;
	h->updateMatrices();
	h->matricesUnaligned = savedU;
	HAnimData::get(root)->hierarchy = nil;
	h->destroy();
	anim->destroy();
	fa->destroy(); fb->destroy(); root->destroy();
}

int main(int argc, char **argv)
{
	Engine::init();
	secOff = WorldSector::registerPlugin(sizeof(SecPlg), 0x50Eu, secCtor, secDtor, nil);
	WorldSector::registerPluginStream(0x50Eu, secRead, nil, nil);
	registerHAnimPlugin();
	extra::registerLODPlugin();
	extra::registerMorphPlugin();
	Engine::open(nil);
	Engine::start();

	testRandom();
	testSpline();
	testLodMorph();
	testHAnim();
	testSectorApi(nullptr);
	for(int i = 1; i < argc; i++)
		testSectorApi(argv[i]);
	printf("%d checks, %d failures\n", nChk, nErr);
	return nErr != 0;
}
