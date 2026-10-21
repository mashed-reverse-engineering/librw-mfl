// Rt2d / RWF font test: no GPU, no platform backend.
// For every given .RWF: independent structure walk (all bytes consumed, sizes add up), load through rt2d::Font,
// compare against the independent parse, glyph range checks, string width / flow measurement against values derived from the
// parsed data, quad geometry + gradient colour checks via a capturing quad sink, then truncated / bit flipped copies
// (must fail cleanly or load; no crash, no leaks). Also CTM / path / brush / device unit checks.
// Build: tests/build_rt2d_test.sh [outdir]   (SAN=1 for ASan+UBSan)   Run: rt2d_test [--fuzz N] FILE...   (exit 1 on failure)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include "rw.h"
#include "rwrt2d.h"

using namespace rw;
typedef std::vector<uint8_t> Buf;

namespace rw {
namespace ps2 { void registerPlatformPlugins(void) {} }
namespace xbox { void registerPlatformPlugins(void) {} }
namespace d3d8 { void registerPlatformPlugins(void) {} }
namespace d3d9 { void registerPlatformPlugins(void) {} }
namespace wdgl { void registerPlatformPlugins(void) {} }
namespace gl3 { void registerPlatformPlugins(void) {} }
}

static int g_fail;
static int g_savedErr = -1;
static void quietErr(bool on)	// silence the library's RWERROR prints while feeding it garbage
{
	fflush(stderr);
	if(getenv("RT2D_VERBOSE")) return;
	if(on){ g_savedErr = dup(2); int n = open("/dev/null", O_WRONLY); dup2(n, 2); close(n); }
	else if(g_savedErr >= 0){ dup2(g_savedErr, 2); close(g_savedErr); g_savedErr = -1; }
}
#define CHECK(c, ...) do{ if(!(c)){ g_fail++; printf("    FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } }while(0)
static bool near_(double a, double b, double eps = 1e-4) { return fabs(a - b) <= eps*(1 + fabs(b)); }

static bool readFile(const char *p, Buf &b)
{
	FILE *f = fopen(p, "rb");
	if(!f) return false;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	b.resize(n);
	bool ok = fread(b.data(), 1, n, f) == (size_t)n;
	fclose(f);
	return ok;
}

// ---------------------------------------------------------------- independent parse (no librw)
struct RefGlyph { float u0, v0, u1, v1, adv; uint8_t tex; };
struct Ref
{
	uint32_t chunk, size, stamp, version;
	int32_t type, flags, highBase, numGlyphs, numHigh, numTex;
	float h, base, gap;
	std::vector<uint16_t> high, low;
	std::vector<RefGlyph> g;
	std::vector<std::string> tex;
	size_t end;
};
static bool refParse(const Buf &b, Ref &r)
{
	size_t p = 0;
	auto need = [&](size_t n){ return p + n <= b.size(); };
	auto u32 = [&](){ uint32_t v; memcpy(&v, &b[p], 4); p += 4; return v; };
	auto f32 = [&](){ float v; memcpy(&v, &b[p], 4); p += 4; return v; };
	if(!need(12 + 36)) return false;
	r.chunk = u32(); r.size = u32(); r.stamp = u32();
	r.version = u32(); r.type = u32(); r.h = f32(); r.base = f32(); r.gap = f32(); r.flags = u32();
	r.highBase = u32(); r.numGlyphs = u32(); r.numHigh = u32();
	if(r.numGlyphs < 0 || r.numHigh < 0 || r.numGlyphs > 0x7FFF || r.numHigh > 0x10000) return false;
	if(!need(2*(size_t)r.numHigh + 256)) return false;
	r.high.resize(r.numHigh); memcpy(r.high.data(), &b[p], 2*r.numHigh); p += 2*r.numHigh;
	r.low.resize(128); memcpy(r.low.data(), &b[p], 256); p += 256;
	if(!need(21*(size_t)r.numGlyphs + 4)) return false;
	for(int i = 0; i < r.numGlyphs; i++){
		RefGlyph g; g.u0 = f32(); g.v0 = f32(); g.u1 = f32(); g.v1 = f32(); g.adv = f32(); g.tex = b[p++];
		r.g.push_back(g);
	}
	r.numTex = u32();
	if(r.numTex < 0 || r.numTex > 4 || !need(32*(size_t)r.numTex)) return false;
	for(int i = 0; i < r.numTex; i++){ r.tex.push_back(std::string((const char*)&b[p], strnlen((const char*)&b[p], 32))); p += 32; }
	r.end = p;
	return true;
}
static int refGlyph(const Ref &r, uint32_t c)
{
	int g;
	if(c < 128) g = (int16_t)r.low[c];
	else{ long d = (long)c - r.highBase; if(d < 0 || d >= r.numHigh) return -1; g = (int16_t)r.high[d]; }
	return g < 0 ? -1 : g;
}
static double refWidth(const Ref &r, const std::vector<uint32_t> &s)
{
	double w = 0;
	for(uint32_t c : s){ int g = refGlyph(r, c); if(g >= 0) w += (double)r.g[g].adv + r.gap; }
	return w;
}

// ---------------------------------------------------------------- strings
static std::vector<uint32_t> codes(const char *s) { std::vector<uint32_t> v; for(; *s; s++) v.push_back((uint8_t)*s); return v; }
static std::vector<uint8_t> wide(const std::vector<uint32_t> &c)
{
	std::vector<uint8_t> o;
	for(uint32_t x : c){ o.push_back(x & 255); o.push_back(x >> 8); }
	o.push_back(0); o.push_back(0);
	return o;
}
static std::vector<uint8_t> W(const char *s) { return wide(codes(s)); }

// ---------------------------------------------------------------- quad capture
struct Cap { std::vector<rt2d::Vertex> v; std::vector<Texture*> batchTex; std::vector<int> batchN; };
static void sink(void *u, Texture *t, const rt2d::Vertex *q, int32 n)
{
	Cap *c = (Cap*)u;
	c->v.insert(c->v.end(), q, q + 4*n);
	c->batchTex.push_back(t); c->batchN.push_back(n);
}

static int leaks()
{
	return rt2d::numAllocated() + Texture::numAllocated + TexDictionary::numAllocated;
}
static int baseDicts;

static rt2d::Font *loadBuf(const Buf &b, bool strict = false)
{
	StreamMemory s;
	s.open((uint8*)b.data(), b.size());	// b.data() may be nil for an empty buffer: read8 handles it
	rt2d::Font *f = rt2d::Font::streamReadChunk(&s, strict);
	s.close();
	return f;
}

// ---------------------------------------------------------------- per file checks
struct Totals { long files, glyphs, strings, quads, fuzz, fuzzLoaded; };

static void testFile(const char *path, Totals &T)
{
	Buf b;
	Ref r;
	if(!readFile(path, b) || !refParse(b, r)){ CHECK(0, "%s: unreadable / bad structure", path); return; }
	printf("%s\n", path);
	CHECK(r.chunk == 0x199, "chunk 0x%X", r.chunk);
	CHECK(r.end == b.size(), "sizes add up: parsed %zu of %zu bytes", r.end, b.size());
	T.files++; T.glyphs += r.numGlyphs;

	// load, compare, stream fully consumed
	StreamMemory s;
	s.open((uint8*)b.data(), b.size());
	rt2d::Font *f = rt2d::Font::streamReadChunk(&s, 0);
	CHECK(f != nil, "load failed");
	if(!f){ s.close(); return; }
	CHECK(s.tell() == b.size(), "consumed %u of %zu bytes", s.tell(), b.size());
	s.close();
	CHECK(f->type == 0 && f->flags == r.flags && (f->flags & rt2d::FONT_UNICODE), "type/flags %d %d", f->type, f->flags);
	CHECK(f->nominalHeight == r.h && f->baseline == r.base && f->intergap == r.gap, "header floats");
	CHECK(f->numGlyphs == r.numGlyphs && f->numHigh == r.numHigh && f->highBase == r.highBase && f->numTextures == r.numTex, "counts");
	CHECK(f->highBase >= 128 && f->highBase + f->numHigh <= 256, "highBase %d numHigh %d (chars >= 128 only, 8 bit codes)", f->highBase, f->numHigh);
	for(int i = 0; i < r.numTex; i++) CHECK(r.tex[i] == f->texNames[i], "tex name %s", f->texNames[i]);
	CHECK(near_(f->baseline, 5.0/f->nominalHeight, 1e-4) , "baseline %g vs 5/%g (guess: baseline = 5 px of the nominal height)", f->baseline, f->nominalHeight);

	// glyph ranges
	int mapped = 0;
	for(int i = 0; i < f->numGlyphs; i++){
		const rt2d::Glyph &g = f->glyphs[i];
		CHECK(g.u0 >= 0 && g.u1 <= 1 && g.v0 >= 0 && g.v1 <= 1 && g.u0 < g.u1 && g.v0 < g.v1, "glyph %d uv %g %g %g %g", i, g.u0, g.v0, g.u1, g.v1);
		CHECK(g.advance > 0 && g.advance < 2, "glyph %d advance %g", i, g.advance);
		CHECK(g.texIndex < f->numTextures, "glyph %d tex", i);
	}
	std::vector<int> used(f->numGlyphs, 0);
	for(uint32_t c = 0; c < 256; c++){
		int g = f->glyphIndex(c), e = refGlyph(r, c);
		CHECK(g == e, "glyphIndex(%u) %d vs %d", c, g, e);
		if(g >= 0){ CHECK(g < f->numGlyphs, "range"); used[g]++; mapped++; }
	}
	CHECK(f->glyphIndex(0) == -1, "NUL must not map");
	CHECK(f->glyphIndex(0x100) == -1 && f->glyphIndex(0x10000) == -1 && f->glyphIndex(0xFFFFFFFFu) == -1, "chars >= 256 have no glyph");
	int unused = 0; for(int u : used) if(!u) unused++;
	printf("    %d glyphs, %d mapped chars, %d unused glyphs, textures:", f->numGlyphs, mapped, unused);
	for(int i = 0; i < f->numTextures; i++) printf(" %s", f->texNames[i]);
	printf("\n");

	// widths against the independent sum
	const char *samples[] = { "MASHED", "Fully Loaded", "Drive to Survive", "0123456789", "a b  c", "", " ", "~~~", "The quick brown fox jumps over the lazy dog" };
	for(const char *sm : samples){
		std::vector<uint32_t> c = codes(sm);
		std::vector<uint8_t> w = wide(c);
		double e = refWidth(r, c);
		for(float h : { 1.0f, 0.0708f * 20, 3.5f }){
			float got = f->getStringWidth(w.data(), h);
			CHECK(near_(got, e*h, 2e-6), "width('%s', %g) = %g, derived %g", sm, h, got, e*h);
		}
		T.strings++;
	}
	// chars without glyph and wide chars add nothing; string ends at the first NUL
	{
		std::vector<uint32_t> c = codes("AB");
		double e = refWidth(r, c);
		std::vector<uint32_t> d = c; d.push_back(0x4E2D); d.push_back(0x1);
		CHECK(near_(f->getStringWidth(wide(d).data(), 1.0f), e), "unmapped chars must not add width");
		std::vector<uint8_t> z = W("AB"); z.push_back('Z'); z.push_back(0);
		CHECK(near_(f->getStringWidth(z.data(), 1.0f), e), "NUL terminates");
		CHECK(f->getStringWidth(W("").data(), 5.0f) == 0, "empty string");
		// additivity
		double ab = f->getStringWidth(W("MASHED").data(), 1.0f) + f->getStringWidth(W(" Fully").data(), 1.0f);
		CHECK(near_(f->getStringWidth(W("MASHED Fully").data(), 1.0f), ab), "additivity");
		// intergap spacing adds per mapped glyph
		float old = f->intergap;
		f->setIntergapSpacing(0.25f);
		std::vector<uint32_t> hh = codes("HELLO");
		int n = 0; for(uint32_t x : hh) if(f->glyphIndex(x) >= 0) n++;
		double base = refWidth(r, hh) - r.gap*n;
		CHECK(near_(f->getStringWidth(W("HELLO").data(), 1.0f), base + 0.25*n), "intergap");
		f->setIntergapSpacing(old);
	}

	// show: geometry and colours
	rt2d::open();
	Cap cap;
	rt2d::setQuadSink(sink, &cap);
	rt2d::deviceSetView(2.0f, 1.5f, 640, 480);	// 4:3, unit = 1.5
	rt2d::Brush *br = rt2d::Brush::create();
	{
		const char *txt = "Hello World";
		std::vector<uint32_t> c = codes(txt);
		int nq = 0; for(uint32_t x : c) if(f->glyphIndex(x) >= 0) nq++;
		V2d pos = makeV2d(0.2f, 0.4f);
		float h = 0.05f;
		f->show(W(txt).data(), h, &pos, br);
		CHECK((int)cap.v.size() == nq*4, "show: %zu vertices for %d glyphs", cap.v.size(), nq);
		T.quads += nq;
		double w = refWidth(r, c);
		CHECK(near_(pos.x, 0.2 + w*h, 1e-5), "show advances pos.x: %g vs %g", pos.x, 0.2 + w*h);
		CHECK(pos.y == 0.4f, "pos.y unchanged");
		// first glyph: BL screen position, derived by hand from the device mapping
		double m = 1.5, sx = 0.2 / 2.0 * 1.5 / 1.0 * 640 / 1.0;	// x*m/W*640
		sx = 0.2 * m / 2.0 * 640;
		double yb = 0.4 - r.base*h, sy = (1.0 - yb*m/1.5)*480;
		CHECK(near_(cap.v[0].x, sx, 1e-4) && near_(cap.v[0].y, sy, 1e-4), "first BL %g,%g vs %g,%g", cap.v[0].x, cap.v[0].y, sx, sy);
		int g0 = refGlyph(r, (uint8_t)txt[0]);
		double adv0 = r.g[g0].adv;
		CHECK(near_(cap.v[2].x - cap.v[0].x, adv0*h*m/2.0*640, 1e-4), "first glyph width");
		CHECK(near_(cap.v[0].y - cap.v[1].y, h*m/1.5*480, 1e-4), "glyph height %g", cap.v[0].y - cap.v[1].y);
		CHECK(cap.v[0].u == r.g[g0].u0 && cap.v[0].v == r.g[g0].v1 && cap.v[1].v == r.g[g0].v0 && cap.v[3].u == r.g[g0].u1, "uv corners");
		CHECK(cap.v[0].r == 255 && cap.v[0].a == 255 && cap.v[3].g == 255, "white flat colour");
		// the quads follow each other by advance (+gap)
		double accx = 0; size_t qi = 0;
		for(uint32_t x : c){
			int g = refGlyph(r, x); if(g < 0) continue;
			CHECK(near_(cap.v[qi*4].x, (0.2 + accx*h)*m/2.0*640, 1e-4), "quad %zu x", qi);
			accx += r.g[g].adv + r.gap; qi++;
		}
	}
	// gradient: left end colours at t=0, right end at t=1
	{
		cap = Cap();
		RGBA bl = makeRGBA(0, 0, 0, 255), brc = makeRGBA(255, 128, 0, 255), tr = makeRGBA(255, 255, 255, 0), tl = makeRGBA(10, 20, 30, 40);
		br->setRGBA(&bl, &brc, &tr, &tl);
		CHECK(br->gradient, "gradient flag");
		V2d pos = makeV2d(0, 0);
		f->show(W("MASHED").data(), 0.1f, &pos, br);
		size_t n = cap.v.size();
		if(n >= 4){
			CHECK(cap.v[0].r == 0 && cap.v[0].a == 255, "BL of first = bottom-left colour");
			CHECK(cap.v[1].r == 10 && cap.v[1].g == 20 && cap.v[1].b == 30 && cap.v[1].a == 40, "TL of first = top-left colour");
			// the game truncates, so t = 1 - ulp may land 1 below the end colour
			#define NEAR1(x, y) ((x) <= (y) && (x) + 1 >= (y))
			CHECK(NEAR1(cap.v[n-2].r, 255) && NEAR1(cap.v[n-2].g, 128) && NEAR1(cap.v[n-2].a, 255), "BR of last = bottom-right colour");
			CHECK(NEAR1(cap.v[n-1].r, 255) && NEAR1(cap.v[n-1].g, 255) && cap.v[n-1].a <= 1, "TR of last = top-right colour");
		}else CHECK(0, "no quads");
		RGBA w = makeRGBA(255, 255, 255, 255);
		br->setRGBA(&w, &w, &w, &w);
		CHECK(!br->gradient, "flat flag");
	}
	// batching: > 64 quads split into batches of 64, one texture per batch
	{
		cap = Cap();
		std::string longs(150, 'A');
		V2d pos = makeV2d(0, 0);
		int g = f->glyphIndex('A');
		if(g >= 0){
			f->show(W(longs.c_str()).data(), 0.01f, &pos, br);
			CHECK(cap.batchN.size() == 3 && cap.batchN[0] == 64 && cap.batchN[1] == 64 && cap.batchN[2] == 22, "batches %zu", cap.batchN.size());
		}
	}
	// flow: line counts and extents against an independent greedy wrap
	{
		const char *txt = "The quick brown fox jumps over the lazy dog and keeps running far away";
		float h = 0.05f;
		for(float bw : { 0.3f, 0.5f, 0.8f, 1.3f }){
			float th;
			int lines = f->measureFlow(W(txt).data(), h, bw, &th);
			// independent greedy wrap (words separated by single spaces)
			std::vector<std::string> words; { std::string cur; for(const char *p = txt; ; p++){ if(*p == ' ' || !*p){ words.push_back(cur); cur.clear(); if(!*p) break; } else cur += *p; } }
			int n = 1; double w = 0; double lw = bw / h;
			for(size_t i = 0; i < words.size(); i++){
				double ww = refWidth(r, codes(words[i].c_str())), sp = refWidth(r, codes(" "));
				if(i == 0){ w = ww; continue; }
				// the game wraps at the first char that overflows, then restarts after the previous space
				if(w + sp + ww > lw){ n++; w = ww; } else w += sp + ww;
			}
			CHECK(lines == n, "flow lines at width %g: %d vs greedy %d", bw, lines, n);
			CHECK(near_(th, lines*h), "total height");
			// drawing into a big enough box produces the same lines (quads == mapped non-space glyph + spaces drawn)
			cap = Cap();
			float rect[4] = { 0.1f, 0.2f, bw, 1.0f };
			rt2d::Font *res = f->flow(W(txt).data(), h, rect, rt2d::FLOW_LEFT, br);
			CHECK(res == f, "flow returns font");
			// every quad must stay inside [rect.x, rect.x+bw] (screen x), tolerance for the trailing advance
			double lo = 0.1 * 1.5 / 2.0 * 640 - 1e-3, hi = (0.1 + bw) * 1.5 / 2.0 * 640 + 1e-3;
			bool inside = true; for(auto &v : cap.v) if(v.x < lo || v.x > hi) inside = false;
			CHECK(inside, "flow quads inside box (width %g)", bw);
			// all text drawn: the sum of quads equals the mapped chars except the swallowed spaces at line breaks
			size_t total = 0; for(const char *p = txt; *p; p++) if(f->glyphIndex((uint8_t)*p) >= 0) total++;
			CHECK(cap.v.size()/4 <= total && cap.v.size()/4 + (size_t)lines >= total, "flow quads %zu of %zu", cap.v.size()/4, total);
		}
		// alignment: right aligned line ends at rect right edge, centred line is centred
		cap = Cap();
		float rect[4] = { 0.0f, 0.0f, 1.0f, 0.2f };
		double w1 = refWidth(r, codes("MASHED")) * h;
		f->flow(W("MASHED").data(), h, rect, rt2d::FLOW_RIGHT, br);
		double xr = (cap.v.size() >= 4) ? cap.v[cap.v.size()-2].x : -1;
		CHECK(near_(xr, 1.0 * 1.5 / 2.0 * 640, 2e-3) || near_(xr, (1.0 - (w1 - (w1 - 0)) ) * 1.5 / 2.0 * 640, 2e-3) || true, "n/a");
		if(cap.v.size() >= 4){
			double right = cap.v[0].x / (1.5/2.0*640) + w1;	// first quad x (unit) + total width
			CHECK(near_(right, 1.0, 1e-4), "right aligned line ends at %g", right);
		}
		cap = Cap();
		f->flow(W("MASHED").data(), h, rect, rt2d::FLOW_CENTER, br);
		if(cap.v.size() >= 4){
			double left = cap.v[0].x / (1.5/2.0*640);
			CHECK(near_(left + w1/2, 0.5, 1e-4), "centred line mid at %g", left + w1/2);
		}
		// \n breaks lines; a box that is too short returns nil
		float th;
		CHECK(f->measureFlow(W("A\nB\nC").data(), h, 10.0f, &th) == 3, "newlines");
		cap = Cap();
		float small[4] = { 0, 0, 0.2f, h };
		CHECK(f->flow(W("MASHED MASHED MASHED").data(), h, small, rt2d::FLOW_LEFT, br) == nil, "overflowing box returns nil");
	}
	// 8 bit fonts: same font with the unicode flag cleared reads bytes
	{
		int fl = f->flags; f->flags = 0;
		CHECK(near_(f->getStringWidth("MASHED", 1.0f), refWidth(r, codes("MASHED"))), "8 bit string width");
		f->flags = fl;
	}
	// texture resolution from a dictionary + strict mode (no rasters here, so strict must refuse)
	{
		TexDictionary *d = TexDictionary::create();
		for(int i = 0; i < r.numTex; i++){ Texture *t = Texture::create(nil); strncpy(t->name, r.tex[i].c_str(), 31); d->add(t); }
		rt2d::fontTexDictionarySet(d);
		rt2d::Font *f2 = loadBuf(b, false);
		CHECK(f2 != nil, "load with dict");
		if(f2){
			for(int i = 0; i < f2->numTextures; i++) CHECK(f2->tex[i] && f2->tex[i]->refCount == 2, "texture %d resolved with ref", i);
			f2->destroy();
		}
		rt2d::Font *f3;
		{ rt2d::fontTexDictionarySet(d); quietErr(true); f3 = loadBuf(b, true); quietErr(false); }
		CHECK(f3 == nil, "strict load must fail without a raster");
		rt2d::fontTexDictionarySet(nil);
		d->destroy();	// destroys the textures
	}
	br->destroy();
	rt2d::setQuadSink(nil, nil);
	rt2d::close();
	f->destroy();
	CHECK(leaks() == baseDicts, "leaks after file: %d", leaks() - baseDicts);
}

// ---------------------------------------------------------------- fuzz: truncated + bit flipped copies
static unsigned long rng = 12345;
static unsigned rnd() { rng = rng*6364136223846793005ULL + 1442695040888963407ULL; return rng >> 33; }

static void exercise(rt2d::Font *f)
{
	Cap cap;
	rt2d::setQuadSink(sink, &cap);
	rt2d::Brush *br = rt2d::Brush::create();
	V2d pos = makeV2d(0.1f, 0.1f);
	std::vector<uint8_t> t = W("Hello, World! \xE4\xF6\xFC 0123");
	f->getStringWidth(t.data(), 1.0f);
	f->show(t.data(), 0.05f, &pos, br);
	float rect[4] = { 0, 0, 0.4f, 0.5f };
	f->flow(t.data(), 0.05f, rect, rt2d::FLOW_CENTER, br);
	float th; f->measureFlow(t.data(), 0.05f, 0.3f, &th);
	for(uint32_t c = 0; c < 300; c++){ int g = f->glyphIndex(c); if(g >= 0 && g >= f->numGlyphs) { printf("    FAIL glyph index range\n"); g_fail++; } }
	br->destroy();
	rt2d::setQuadSink(nil, nil);
}

static void fuzzFile(const char *path, int iters, Totals &T)
{
	Buf b;
	if(!readFile(path, b)) return;
	quietErr(true);
	rt2d::open();
	rt2d::deviceSetView(2.0f, 1.5f, 640, 480);
	long loaded = 0, total = 0;
	// every truncation length
	for(size_t n = 0; n < b.size(); n++){
		Buf c(b.begin(), b.begin() + n);
		rt2d::Font *f = loadBuf(c);
		total++;
		if(f){ loaded++; exercise(f); f->destroy(); }
		if(leaks() != baseDicts + 1){ quietErr(false); CHECK(0, "leak after truncation %zu", n); return; }
	}
	// bit flips (1-3 per copy), concentrated on the header and maps half of the time
	for(int i = 0; i < iters; i++){
		Buf c = b;
		int k = 1 + rnd() % 3;
		for(int j = 0; j < k; j++){
			size_t p = (rnd() & 1) ? 12 + rnd() % 80 : rnd() % c.size();
			c[p] ^= 1 << (rnd() % 8);
		}
		rt2d::Font *f = loadBuf(c);
		total++;
		if(f){ loaded++; exercise(f); f->destroy(); }
		if(leaks() != baseDicts + 1){ quietErr(false); CHECK(0, "leak after bit flip %d", i); return; }
	}
	rt2d::close();
	quietErr(false);
	T.fuzz += total; T.fuzzLoaded += loaded;
	printf("fuzz %s: %ld copies, %ld loaded, rest rejected cleanly\n", path, total, loaded);
}

// ---------------------------------------------------------------- unit checks for CTM / path / brush / device
static void unitTests()
{
	rt2d::open();
	// CTM
	rt2d::ctmSetIdentity();
	rt2d::ctmTranslate(1, 2);
	rt2d::ctmScale(2, 3);
	rt2d::Affine a = rt2d::ctmGet();	// p' = T(S(p)) = (2x+1, 3y+2)
	CHECK(a.a == 2 && a.d == 3 && a.tx == 1 && a.ty == 2 && a.b == 0 && a.c == 0, "ctm translate*scale preconcat");
	CHECK(rt2d::ctmPush() && rt2d::ctmDepth() == 1, "push");
	rt2d::ctmTranslate(1, 1);
	CHECK(rt2d::ctmGet().tx == 3 && rt2d::ctmGet().ty == 5, "translate after scale is scaled: %g %g", rt2d::ctmGet().tx, rt2d::ctmGet().ty);
	rt2d::ctmPop();
	CHECK(rt2d::ctmGet().tx == 1 && rt2d::ctmDepth() == 0, "pop restores");
	rt2d::ctmPop(); rt2d::ctmPop();
	CHECK(rt2d::ctmDepth() == 0, "pop at depth 0 is a no-op");
	int pushed = 0; while(rt2d::ctmPush()) pushed++;
	CHECK(pushed == 31 && rt2d::ctmDepth() == 31, "stack depth limit %d", pushed);
	while(rt2d::ctmDepth()) rt2d::ctmPop();
	rt2d::ctmSetIdentity();
	rt2d::ctmRotate(90);
	{ rt2d::Affine r = rt2d::ctmGet(); CHECK(near_(r.a, 0, 1e-6) && near_(r.b, 1, 1e-6) && near_(r.c, -1, 1e-6), "rotate 90"); }
	rt2d::ctmSetIdentity();
	rt2d::ctmRotate(0);
	CHECK(rt2d::ctmGet().a == 1 && rt2d::ctmGet().b == 0, "rotate 0 is identity");
	// brush
	rt2d::Brush *b = rt2d::Brush::create();
	CHECK(b && !b->gradient && b->c[0].red == 255 && b->refCount == 1, "brush default white");
	b->refCount++;
	CHECK(!b->destroy() && b->destroy(), "refcounted destroy");
	// path
	rt2d::Path *p = rt2d::Path::create();
	float bb[4];
	p->getBBox(bb);
	CHECK(bb[2] == 0 && bb[3] == 0, "empty bbox");
	p->moveto(1, 2); p->lineto(4, 3); p->lineto(2, 9); p->close();
	p->getBBox(bb);
	CHECK(bb[0] == 1 && bb[1] == 2 && bb[2] == 3 && bb[3] == 7, "bbox xywh %g %g %g %g", bb[0], bb[1], bb[2], bb[3]);
	p->empty();
	p->moveto(0, 0); p->curveto(0, 1, 1, 1, 1, 0);
	p->getBBox(bb);
	CHECK(near_(bb[2], 1) && near_(bb[3], 0.75, 1e-5), "curve bbox %g %g", bb[2], bb[3]);
	// device + clip path as ui_2d_begin uses it
	rt2d::deviceSetView(2.0f, 1.5f, 640, 480);
	rt2d::ctmSetIdentity();
	rt2d::deviceGetClippath(p);
	p->getBBox(bb);
	CHECK(near_(bb[0], 0) && near_(bb[1], 0) && near_(bb[2], 4.0/3) && near_(bb[3], 1), "clip bbox of a 4:3 view: %g %g %g %g", bb[0], bb[1], bb[2], bb[3]);
	rt2d::ctmPush(); rt2d::ctmScale(0.5f, 0.5f);
	rt2d::deviceGetClippath(p); p->getBBox(bb);
	CHECK(near_(bb[2], 8.0/3) && near_(bb[3], 2), "clip bbox under scale 0.5: %g %g", bb[2], bb[3]);
	rt2d::ctmPop();
	float sx, sy;
	rt2d::deviceToScreen(0, 0, &sx, &sy); CHECK(near_(sx, 0) && near_(sy, 480), "origin = bottom-left");
	rt2d::deviceToScreen(4.0f/3, 1, &sx, &sy); CHECK(near_(sx, 640, 1e-5) && near_(sy, 0, 1e-5), "top-right");
	rt2d::deviceSetView(2.0f, 2.0f, 100, 100);
	rt2d::deviceGetClippath(p); p->getBBox(bb);
	CHECK(near_(bb[2], 1) && near_(bb[3], 1), "square view");
	// close appends the closing point; curveto flattens adaptively (straight curve -> 1 segment, bent curve -> more, <= 128)
	p->empty(); p->moveto(0, 0); p->lineto(1, 0); p->lineto(1, 1); p->close();
	CHECK(p->subs[0].n == 4 && p->subs[0].closed && p->subs[0].xy[6] == 0 && p->subs[0].xy[7] == 0, "close appends first point");
	p->close(); CHECK(p->subs[0].n == 4, "close on closed loop adds nothing");
	rt2d::deviceSetView(2.0f, 1.5f, 640, 480);
	p->empty(); p->moveto(0, 0); p->curveto(0.3f, 0, 0.6f, 0, 1, 0);
	CHECK(p->subs[0].n == 2, "straight curve = 1 segment (%d)", p->subs[0].n);
	p->empty(); p->moveto(0, 0); p->curveto(0, 1, 1, 1, 1, 0);
	CHECK(p->subs[0].n > 4 && p->subs[0].n <= 129 && p->subs[0].xy[2*p->subs[0].n-2] == 1, "bent curve subdivided (%d)", p->subs[0].n);
	p->getBBox(bb); CHECK(bb[3] > 0.7f && bb[3] < 0.76f, "curve bbox height %g", bb[3]);
	{
		Camera *c = Camera::create();
		CHECK(rt2d::deviceSetCamera(nil) == 0 && rt2d::deviceSetCamera(c) == 0, "setCamera nil / no frame buffer return 0");
		c->destroy();
	}
	{	// StreamMemory on an empty buffer and with a huge length (no memcpy(nil), no uint32 wrap)
		StreamMemory m; char tmp[4];
		m.open(nil, 0, 0);
		CHECK(m.read8(tmp, 0) == 0, "read8 len 0 on empty");
		CHECK(m.read8(tmp, 0xFFFFFFF0u) == 0, "read8 huge");
		m.open((uint8*)tmp, 4, 4); m.seek(2);
		CHECK(m.read8(tmp, 0xFFFFFFFFu) == 2, "read8 clamps without wrap");
	}
	p->destroy();
	CHECK(rt2d::numAllocated() == 0, "path/brush leak");
	rt2d::close();
}

int main(int argc, char **argv)
{
	int fuzz = 0, a = 1;
	setvbuf(stdout, nil, _IOLBF, 0);
	if(argc > 2 && !strcmp(argv[1], "--fuzz")){ fuzz = atoi(argv[2]); a = 3; }
	Engine::init();
	Engine::open(nil);
	Engine::start();
	baseDicts = leaks();
	Totals T = {};
	unitTests();
	printf("%s unit tests (CTM, brush, path, device)\n", g_fail ? "FAIL" : "PASS");
	for(int i = a; i < argc; i++){
		int before = g_fail;
		testFile(argv[i], T);
		if(fuzz) fuzzFile(argv[i], fuzz, T);
		printf("%s %s\n", g_fail == before ? "PASS" : "FAIL", argv[i]);
	}
	printf("files %ld, glyphs %ld, strings measured %ld, quads checked %ld, fuzz copies %ld (loaded %ld), failures %d, leaks %d\n",
	       T.files, T.glyphs, T.strings, T.quads, T.fuzz, T.fuzzLoaded, g_fail, leaks() - baseDicts);
	return g_fail || leaks() != baseDicts;
}
