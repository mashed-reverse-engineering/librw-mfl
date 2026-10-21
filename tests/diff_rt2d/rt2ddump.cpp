// Differential dump of the librw-mfl Rt2d fork. usage: rt2ddump strings.txt FILE.RWF
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <rw.h>
#include <rwrt2d.h>
using namespace rw;
namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}

static unsigned fb(float f){ unsigned u; memcpy(&u, &f, 4); return u; }
struct Cap { std::vector<rt2d::Vertex> v; };
static void sink(void *u, Texture*, const rt2d::Vertex *q, int32 n){ Cap *c = (Cap*)u; c->v.insert(c->v.end(), q, q+n*4); }

static std::vector<std::vector<uint16_t>> loadStrings(const char *p)
{
	std::vector<std::vector<uint16_t>> out;
	FILE *f = fopen(p, "r"); char line[4096];
	while(fgets(line, sizeof line, f)){
		std::vector<uint16_t> s;
		// format: space separated hex codes
		char *t = strtok(line, " \n");
		while(t){ s.push_back((uint16_t)strtoul(t, 0, 16)); t = strtok(0, " \n"); }
		s.push_back(0);
		out.push_back(s);
	}
	fclose(f);
	return out;
}

static void dumpQuads(Cap &c)
{
	for(size_t i = 0; i < c.v.size(); i++){
		rt2d::Vertex &v = c.v[i];
		printf(" V %.3f %.3f %08x %08x %d %d %d %d\n", v.x, v.y, fb(v.u), fb(v.v), v.r, v.g, v.b, v.a);
	}
	c.v.clear();
}

int main(int argc, char **argv)
{
	Engine::init(); Engine::open(nil); Engine::start();
	std::vector<std::vector<uint16_t>> strs = loadStrings(argv[1]);
	FILE *fp = fopen(argv[2], "rb"); fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
	std::vector<uint8> buf(n); fread(buf.data(), 1, n, fp); fclose(fp);
	StreamMemory s; s.open(buf.data(), n);
	rt2d::open();
	rt2d::Font *f = rt2d::Font::streamReadChunk(&s, 0);
	if(!f){ printf("LOADFAIL\n"); return 1; }
	printf("H %d %08x %08x %08x %d %d %d %d %d\n", f->type, fb(f->nominalHeight), fb(f->baseline), fb(f->intergap), f->flags,
	       f->highBase, f->numGlyphs, f->numHigh, f->numTextures);
	for(int i = 0; i < f->numTextures; i++) printf("T %d %s\n", i, f->texNames[i]);
	for(int i = 0; i < f->numGlyphs; i++){
		rt2d::Glyph &g = f->glyphs[i];
		printf("G %d %08x %08x %08x %08x %08x %d\n", i, fb(g.advance), fb(g.u0), fb(g.v0), fb(g.u1), fb(g.v1), g.texIndex);
	}
	for(uint32 c = 0; c < 0x300; c++) printf("M %x %d\n", c, f->glyphIndex(c));
	printf("M ffff %d\n", f->glyphIndex(0xffff));
	float hs[4] = { 1.0f, 0.0708f, 0.0020833334f*24, 0.05f };
	for(size_t i = 0; i < strs.size(); i++)
		for(int k = 0; k < 4; k++) printf("W %zu %d %08x\n", i, k, fb(f->getStringWidth(strs[i].data(), hs[k])));

	Cap cap;
	rt2d::setQuadSink(sink, &cap);
	rt2d::deviceSetView(2.0f, 1.5f, 640, 480);
	rt2d::Brush *br = rt2d::Brush::create();
	RGBA c0 = makeRGBA(255, 0, 10, 255), c1 = makeRGBA(0, 255, 200, 128), c2 = makeRGBA(7, 99, 255, 0), c3 = makeRGBA(250, 3, 128, 77);
	for(size_t i = 0; i < strs.size(); i++){
		br->setRGBA(&c0, &c1, &c2, &c3);
		V2d pos = makeV2d(0.2f, 0.4f);
		f->show(strs[i].data(), 0.05f, &pos, br);
		printf("S %zu %08x\n", i, fb(pos.x));
		dumpQuads(cap);
	}
	RGBA w = makeRGBA(200, 100, 50, 25);
	br->setRGBA(&w, &w, &w, &w);
	float widths[3] = { 0.3f, 0.6f, 1.2f };
	for(size_t i = 0; i < strs.size(); i++)
		for(int k = 0; k < 3; k++)
			for(int fmt = 0; fmt < 3; fmt++){
				float rect[4] = { 0.1f, 0.1f, widths[k], 0.8f };
				rt2d::Font *r = f->flow(strs[i].data(), 0.0708f*0.8f, rect, fmt, br);
				printf("F %zu %d %d %d\n", i, k, fmt, r != nil);
				dumpQuads(cap);
				int lines = f->measureFlow(strs[i].data(), 0.0708f*0.8f, widths[k], nil);
				printf("N %zu %d %d\n", i, k, lines);
			}
	br->destroy();
	f->destroy();
	return 0;
}
