// Standalone PI texture dictionary (chunk 0x23) loader test: no GPU, no platform backend.
// Installs a tiny in-memory raster driver on the null platform, loads every given .TXD / .TXD.CMP with
// rw::TexDictionary::readPI / streamReadPI, validates against an independent walk of the file, prints stats.
// zlib is used here ONLY to inflate TRACKIMAGES.TXD.CMP (the library itself reads uncompressed chunks).
// Build: tests/build_pitxd_test.sh     Run: pitxd_test [--fuzz N] FILE...   (exit 1 on any failure)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <zlib.h>
#include "rw.h"

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

// ---------------------------------------------------------------- memory raster driver (mimics d3d lock semantics)
struct MemRas {
	std::vector<Buf> lv;	// per level pixel data, tightly packed
	Buf pal;
	int bpp, w, h;
	int unlocks;	// levels written (used by the differential dump)
};
static std::map<Raster*, MemRas> g_ras;
static int g_lockedLevel = -1;

static Raster *mCreate(Raster *r)
{
	MemRas m;
	m.unlocks = 0;
	m.bpp = (r->format & (Raster::PAL4|Raster::PAL8)) ? 1 : 4;
	m.w = r->width; m.h = r->height;
	int n = (r->format & Raster::MIPMAP) ? Raster::calculateNumLevels(r->width, r->height) : 1;
	for(int i = 0; i < n; i++){
		int w = r->width >> i, h = r->height >> i;
		if(w < 1) w = 1;
		if(h < 1) h = 1;
		m.lv.push_back(Buf((size_t)w*h*m.bpp, 0xCD));
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
static void mUnlock(Raster *r, int32)
{
	g_ras[r].unlocks++;
	r->width = r->originalWidth; r->height = r->originalHeight; r->stride = r->originalStride; r->pixels = nil;
}
static uint8 *mLockPal(Raster *r, int32) { return g_ras[r].pal.data(); }
static void mUnlockPal(Raster*) {}
static int32 mLevels(Raster *r) { return (int32)g_ras[r].lv.size(); }
static bool32 mFindFormat(Image *img, int32 type, int32 *w, int32 *h, int32 *d, int32 *f)
{
	*w = img->width; *h = img->height; *d = img->depth;
	// like the d3d backend: palettised -> PAL|C8888 (the PI reader downgrades opaque ones), 32 bit -> C888 when opaque
	*f = (img->depth == 8 ? Raster::PAL8 : img->depth == 4 ? Raster::PAL4 : 0) | (img->depth == 32 && !img->hasAlpha() ? Raster::C888 : Raster::C8888) | type;
	return img->depth == 32 || img->depth == 8 || img->depth == 4;
}
static bool32 mFromImage(Raster *r, Image *img)
{
	MemRas &m = g_ras[r];
	if(m.bpp == 1) memcpy(m.pal.data(), img->palette, (r->format & Raster::PAL4) ? 64 : 1024);
	bool lk = r->pixels == nil;
	if(lk) r->lock(0, 0);
	if(img->width != r->width || img->height != r->height) return 0;
	for(int y = 0; y < img->height; y++)
		memcpy(r->pixels + (size_t)y*r->stride, img->pixels + (size_t)y*img->stride, (size_t)img->width*m.bpp);
	if(lk) r->unlock(0);
	return 1;
}
static void installDriver()
{
	Driver *d = engine->driver[PLATFORM_NULL];
	d->rasterCreate = mCreate; d->rasterLock = mLock; d->rasterUnlock = mUnlock;
	d->rasterLockPalette = mLockPal; d->rasterUnlockPalette = mUnlockPal; d->rasterNumLevels = mLevels;
	d->imageFindRasterFormat = mFindFormat; d->rasterFromImage = mFromImage;
}
// the library frees rasters through Raster::destroy; drop our bookkeeping lazily
static void gcRas() { /* entries of destroyed rasters are stale but unreferenced */ g_ras.clear(); }

// ---------------------------------------------------------------- independent reference walk
struct RLevel { uint32_t w, h, depth, stride; const uint8_t *pix, *pal; };
struct RTex { std::string name, mask; uint32_t flags; std::vector<RLevel> lv; };
static uint32_t U32(const uint8_t *p) { return p[0] | p[1]<<8 | p[2]<<16 | (uint32_t)p[3]<<24; }

static bool refParse(const Buf &b, std::vector<RTex> &out, std::string &err)
{
	size_t n = b.size(), pos = 16;
	if(n < 16 || U32(&b[0]) != 0x23 || U32(&b[4]) + 12 != n){ err = "header/size"; return false; }
	unsigned num = b[12] | b[13]<<8;
	for(unsigned t = 0; t < num; t++){
		RTex tx;
		if(pos + 4 > n){ err = "eof"; return false; }
		uint32_t ni = U32(&b[pos]); pos += 4;
		for(uint32_t i = 0; i < ni; i++){
			if(pos + 40 > n || U32(&b[pos]) != 0x18 || U32(&b[pos+12]) != 1 || U32(&b[pos+16]) != 16){ err = "image hdr"; return false; }
			RLevel l = { U32(&b[pos+24]), U32(&b[pos+28]), U32(&b[pos+32]), U32(&b[pos+36]), &b[pos+40], nil };
			size_t pal = l.depth <= 8 ? (size_t)4 << l.depth : 0;
			if(U32(&b[pos+4]) != 28 + pal + (size_t)l.h*l.stride){ err = "image size"; return false; }
			l.pal = pal ? l.pix + (size_t)l.h*l.stride : nil;
			pos += 12 + U32(&b[pos+4]);
			tx.lv.push_back(l);
		}
		if(pos + 12 > n || U32(&b[pos]) != 6){ err = "tex chunk"; return false; }
		size_t end = pos + 12 + U32(&b[pos+4]), p = pos + 12;
		tx.flags = U32(&b[p+12]); p += 16;
		for(int s = 0; s < 2; s++){
			uint32_t len = U32(&b[p+4]);
			std::string str((const char*)&b[p+12], strnlen((const char*)&b[p+12], len));
			(s ? tx.mask : tx.name) = str; p += 12 + len;
		}
		if(U32(&b[p]) != 3){ err = "ext"; return false; }
		p += 12 + U32(&b[p+4]);
		if(p != end){ err = "tex size"; return false; }
		pos = end;
		out.push_back(tx);
	}
	if(pos != n){ err = "trailing"; return false; }
	return true;
}

// ---------------------------------------------------------------- file loading
static bool readFile(const char *fn, Buf &b)
{
	FILE *f = fopen(fn, "rb");
	if(!f) return false;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	b.resize(n);
	bool ok = fread(b.data(), 1, n, f) == (size_t)n;
	fclose(f);
	if(ok && n > 2 && b[0] == 0x78 && (b[1] == 0xDA || b[1] == 0x9C || b[1] == 0x01 || b[1] == 0x5E)){	// zlib
		Buf out(1 << 20);
		for(;;){
			uLongf len = out.size();
			int r = uncompress(out.data(), &len, b.data(), b.size());
			if(r == Z_OK){ out.resize(len); b.swap(out); return true; }
			if(r != Z_BUF_ERROR) return false;
			out.resize(out.size()*2);
		}
	}
	return ok;
}

static int baseDicts;	// the engine's own initial dictionary
static int leaks()
{
	return Texture::numAllocated + Raster::numAllocated + TexDictionary::numAllocated - baseDicts + Image::numAllocated;
}

struct Totals { long files, tex, images, bytes, d4, d8, d32, mipTex; };

#define CHECK(c, ...) do{ if(!(c)){ char b_[256]; snprintf(b_, sizeof b_, __VA_ARGS__); errs.push_back(b_); } }while(0)

static bool validate(const Buf &b, TexDictionary *txd, const std::vector<RTex> &ref, Totals &T, std::vector<std::string> &errs)
{
	CHECK((size_t)txd->count() == ref.size(), "count %d != %zu", txd->count(), ref.size());
	size_t i = 0;
	FORLIST(lnk, txd->textures){
		Texture *t = Texture::fromDict(lnk);
		if(i >= ref.size()) break;
		const RTex &r = ref[i++];
		CHECK(t->dict == txd, "%s dict link", r.name.c_str());
		CHECK(r.name.substr(0, 31) == t->name, "name '%s' vs '%s'", t->name, r.name.c_str());
		CHECK(r.mask.substr(0, 31) == t->mask, "mask '%s' vs '%s'", t->mask, r.mask.c_str());
		uint32_t fa = r.flags;
		if((fa & 0xF000) == 0) fa |= (fa & 0xF00) << 4;
		CHECK(t->filterAddressing == (fa & 0xFFFF), "%s flags %x vs %x", t->name, t->filterAddressing, fa);
		Raster *ras = t->raster;
		CHECK(ras != nil, "%s no raster", t->name);
		if(!ras || !g_ras.count(ras)) continue;
		MemRas &m = g_ras[ras];
		const RLevel &l0 = r.lv[0];
		int pal = l0.depth == 8 ? Raster::PAL8 : l0.depth == 4 ? Raster::PAL4 : 0;
		CHECK((uint32_t)ras->width == l0.w && (uint32_t)ras->height == l0.h, "%s dims", t->name);
		// alpha format only if the image really has alpha (palette entry of a USED index / 32 bit pixel alpha != 0xFF)
		bool alpha = false;
		for(uint32_t y = 0; y < l0.h; y++)
			for(uint32_t x = 0; x < l0.w; x++)
				alpha |= pal ? l0.pal[l0.pix[y*l0.stride + x]*4 + 3] != 0xFF : l0.pix[y*l0.stride + x*4 + 3] != 0xFF;
		CHECK((ras->format & (Raster::PAL4|Raster::PAL8|0xF00)) == (pal | (alpha ? Raster::C8888 : Raster::C888)), "%s format %x", t->name, ras->format);
		bool autoMip = r.lv.size() == 1 && (r.flags & 0xFF) != 1 && (r.flags & 0xFF) != 2;
		CHECK(((ras->format & Raster::MIPMAP) != 0) == (r.lv.size() > 1 || autoMip), "%s mip flag", t->name);
		CHECK(((ras->format & Raster::AUTOMIPMAP) != 0) == autoMip, "%s automip flag", t->name);
		CHECK(m.lv.size() >= r.lv.size(), "%s levels %zu < %zu", t->name, m.lv.size(), r.lv.size());
		if(r.lv.size() > 1){
			T.mipTex++;
			// mip chain: each level half the previous (max 1), and the chain ends at 1x? (informational)
		}
		if(pal) CHECK(!memcmp(m.pal.data(), l0.pal, (size_t)4 << l0.depth), "%s palette", t->name);
		for(size_t k = 0; k < r.lv.size() && k < m.lv.size(); k++){
			const RLevel &l = r.lv[k];
			uint32_t ew = l0.w >> k, eh = l0.h >> k;
			if(ew < 1) ew = 1; if(eh < 1) eh = 1;
			CHECK(l.w == ew && l.h == eh, "%s level %zu dims %ux%u", t->name, k, l.w, l.h);
			CHECK(m.lv[k].size() == (size_t)ew*eh*m.bpp, "%s level %zu size", t->name, k);
			bool same = true;
			for(uint32_t y = 0; y < l.h && same; y++)
				for(uint32_t x = 0; x < l.w; x++){
					const uint8_t *src = l.pix + (size_t)y*l.stride;
					const uint8_t *dst = &m.lv[k][((size_t)y*l.w + x)*m.bpp];
					if(l.depth == 32){ if(memcmp(src + x*4, dst, 4)) same = false; }
					else if(l.depth == 8){ if(src[x] != *dst) same = false; }
					else if(src[x] != *dst) same = false;	// 4 bit: one index per byte
					if(!same) break;
				}
			CHECK(same, "%s level %zu pixels differ", t->name, k);
			T.images++; T.bytes += (long)l.h*l.stride;
		}
		T.tex++;
		(l0.depth == 4 ? T.d4 : l0.depth == 8 ? T.d8 : T.d32)++;
	}
	return errs.empty();
}


// ---------------------------------------------------------------- synthetic dictionaries (legacy layout, UTF-16 names, auto mips, 4 bit)
static void P32(Buf &b, uint32_t v) { for(int i = 0; i < 4; i++) b.push_back(v >> 8*i); }
static void CH(Buf &b, uint32_t t, const Buf &body, uint32_t stamp = 0x1C02000A) { P32(b, t); P32(b, body.size()); P32(b, stamp); b.insert(b.end(), body.begin(), body.end()); }
static Buf image4(uint32_t w, uint32_t h, bool alphaUsed, bool alphaUnused)
{
	uint32_t st = (w + 3) & ~3u;
	Buf s, body, data, out;
	P32(s, w); P32(s, h); P32(s, 4); P32(s, st);
	CH(body, 1, s);
	for(uint32_t y = 0; y < h; y++) for(uint32_t x = 0; x < st; x++) data.push_back(x < w ? (x + y*3) & 15 : 0xEE);
	size_t pal = data.size();
	for(int i = 0; i < 16; i++) P32(data, 0xFF000000u | i*0x010203);
	if(alphaUsed) data[pal + 3] = 0x80;		// entry 0 is used by pixel (0,0)
	if(alphaUnused) data[pal + 4*15 + 3] = 0x80;	// entry 15 is not used by a 5x2 image (indices (x+3y)&15 = 0..4,3..7)
	body.insert(body.end(), data.begin(), data.end());
	CH(out, 0x18, body);
	return out;
}
static bool synth(std::vector<std::string> &errs)
{
	for(int variant = 0; variant < 4; variant++){
		bool legacy = variant == 3, wide = variant == 1, aused = variant == 2;
		Buf d, tx, str;
		Buf img = image4(5, 2, aused, variant == 0);
		P32(d, 1 | (legacy ? 0u : 1u << 16));
		if(legacy){
			char rec[64] = "legacy"; memset(rec + 6, 0, 58); strcpy(rec + 32, "msk");
			d.insert(d.end(), rec, rec + 64); P32(d, 1); P32(d, 0x0004);	// filter 4, U = V = 0
		}else P32(d, 1);
		d.insert(d.end(), img.begin(), img.end());
		if(!legacy){
			Buf st; P32(st, 0x0204 /*filter 4, U 2, V 0*/);
			CH(tx, 1, st, 0x1803FFFF);
			Buf junk(8, 0); CH(tx, 0x99, junk);	// skipped chunk before the name
			if(wide){ Buf u; const char *n = "wide"; for(int i = 0; n[i]; i++){ u.push_back(n[i]); u.push_back(0); } CH(tx, 0x13, u); }
			else { Buf n = { 'n','a','m','e',0,0,0,0 }; CH(tx, 2, n); }
			Buf m = { 0,0,0,0 }; CH(tx, 2, m);
			Buf e; CH(tx, 3, e);
			CH(d, 6, tx);
			Buf e2; e2 = d; d.clear(); P32(d, 0x23); P32(d, 0); P32(d, 0x1C02000A);
			d.insert(d.end(), e2.begin(), e2.end());
		}else{ Buf e2 = d; d.clear(); P32(d, 0x23); P32(d, 0); P32(d, 0x1C02000A); d.insert(d.end(), e2.begin(), e2.end()); }
		d[4] = (d.size() - 12) & 0xFF; d[5] = ((d.size() - 12) >> 8) & 0xFF;
		TexDictionary *t = TexDictionary::readPI(d.data(), d.size());
		if(!t){ errs.push_back("synthetic: load failed"); continue; }
		Texture *x = Texture::fromDict(t->textures.link.next);
		Raster *r = x->raster; MemRas &m = g_ras[r];
		char b_[200];
		#define SC(c, what) if(!(c)){ snprintf(b_, sizeof b_, "synthetic %d: %s", variant, what); errs.push_back(b_); }
		SC(!strcmp(x->name, legacy ? "legacy" : wide ? "wide" : "name"), "name");
		SC(x->filterAddressing == (legacy ? 0x1104 : 0x2204), "filterAddressing");
		SC((r->format & Raster::AUTOMIPMAP) && (r->format & Raster::MIPMAP), "auto mipmap flags (single image, filter 4)");
		SC((r->format & (Raster::PAL4|0xF00)) == (Raster::PAL4 | (aused ? Raster::C8888 : Raster::C888)), "opaque rule");
		bool pix = true;
		for(int y = 0; y < 2; y++) for(int xx = 0; xx < 5; xx++) pix &= m.lv[0][y*5 + xx] == ((xx + y*3) & 15);
		SC(pix, "4 bit pixels are one index per byte");
		t->destroy(); gcRas();
	}
	return errs.empty();
}

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }

// Loads a (possibly damaged) buffer; must not crash or leak. Returns whether it loaded.
static bool loadOnly(const Buf &b, bool viaStream)
{
	TexDictionary *t;
	if(viaStream){
		StreamMemory sm;
		static uint8 dummy[1];	// empty vectors have a null data(); StreamMemory memcpy's from it
		sm.open(b.empty() ? dummy : (uint8*)b.data(), b.size());
		t = TexDictionary::streamReadPI(&sm);
		sm.close();
	}else
		t = TexDictionary::readPI(b.empty() ? (const uint8*)"" : b.data(), b.size());
	if(t) t->destroy();
	gcRas();
	return t != nil;
}

int main(int argc, char **argv)
{
	int fuzz = 0, a = 1;
	if(argc > 2 && !strcmp(argv[1], "--fuzz")){ fuzz = atoi(argv[2]); a = 3; }
	Engine::init();
	Engine::open(nil);
	Engine::start();
	installDriver();
	baseDicts = TexDictionary::numAllocated;
	int fails = 0, skipped = 0;
	{ std::vector<std::string> e; bool ok = synth(e); printf("%s synthetic (legacy, UTF-16, auto mip, 4 bit)\n", ok ? "PASS" : "FAIL"); for(auto &m : e) printf("    %s\n", m.c_str()); if(!ok) fails++; }
	Totals T = {};
	long fz = 0, fzok = 0;
	for(int i = a; i < argc; i++){
		Buf b;
		std::vector<std::string> errs;
		if(!readFile(argv[i], b) || b.size() < 12){ printf("FAIL %s: unreadable\n", argv[i]); fails++; continue; }
		if(U32(&b[0]) != 0x23){
			printf("SKIP %s: chunk 0x%X (native platform TXD, not PI)\n", argv[i], U32(&b[0]));
			skipped++;
			continue;
		}
		std::vector<RTex> ref;
		std::string e;
		bool ok = refParse(b, ref, e);
		if(!ok) errs.push_back("reference parse: " + e);
		TexDictionary *txd = TexDictionary::readPI(b.data(), b.size());
		if(!txd){ errs.push_back("readPI failed"); ok = false; }
		else{
			if(ok) ok = validate(b, txd, ref, T, errs);
			txd->destroy();
		}
		// the stream entry point must give the same result and consume exactly the chunk
		{
			StreamMemory sm;
			sm.open(b.data(), b.size());
			TexDictionary *t2 = TexDictionary::streamReadPI(&sm);
			if(!t2 || (txd && t2->count() != (int)ref.size())){ errs.push_back("streamReadPI mismatch"); ok = false; }
			if(sm.tell() != b.size()){ errs.push_back("stream not fully consumed"); ok = false; }
			if(t2) t2->destroy();
			sm.close();
		}
		gcRas();
		if(leaks()){ char m[100]; snprintf(m, sizeof m, "leak tex %d ras %d dict %d img %d", Texture::numAllocated, Raster::numAllocated, TexDictionary::numAllocated, Image::numAllocated); errs.push_back(m); ok = false; }
		ok = ok && errs.empty();
		T.files++;
		printf("%s %s: %zu textures, %zu bytes\n", ok ? "PASS" : "FAIL", argv[i], ref.size(), b.size());
		for(size_t k = 0; k < errs.size() && k < 5; k++) printf("    %s\n", errs[k].c_str());
		if(!ok) fails++;
		if(fuzz && ok){
			// truncations: cut at fuzz points incl. every header boundary region, odd lengths
			for(int k = 0; k < fuzz; k++){
				size_t cut = k < 8 ? (size_t)k*3 : rnd() % b.size();
				Buf c(b.begin(), b.begin() + cut);
				fzok += loadOnly(c, k & 1); fz++;
			}
			// bit flips: 1..8 flips, biased to the first 64KB (headers) half of the time
			for(int k = 0; k < fuzz; k++){
				Buf c(b);
				int nf = 1 + rnd() % 8;
				for(int f = 0; f < nf; f++){
					size_t at = (rnd() & 1) ? rnd() % (b.size() < 65536 ? b.size() : 65536) : rnd() % b.size();
					c[at] ^= 1 << (rnd() & 7);
				}
				fzok += loadOnly(c, k & 1); fz++;
			}
			if(leaks()){ printf("FAIL %s: leak after fuzz (tex %d ras %d dict %d img %d)\n", argv[i], Texture::numAllocated, Raster::numAllocated, TexDictionary::numAllocated, Image::numAllocated); fails++; }
		}
	}
	printf("%ld PI files, %d failed, %d skipped (native); %ld textures (%ld 4bit, %ld 8bit, %ld 32bit, %ld with mips), %ld images, %ld bytes of image data; leaked objects %d\n",
	       T.files, fails, skipped, T.tex, T.d4, T.d8, T.d32, T.mipTex, T.images, T.bytes, leaks());
	if(fuzz) printf("fuzz: %ld damaged copies loaded without crash (%ld still parsed, %ld rejected)\n", fz, fzok, fz - fzok);
	return fails || leaks() ? 1 : 0;
}
