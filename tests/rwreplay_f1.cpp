// E3 (F1 rung of the engine switch, offline): raster create / lock / format choice and native + PI TXD read THROUGH mflrw.dll (the D3D9 build) on a real
// Wine device, run with the d3dtrace proxy (ghidra E3 f1run.sh: the DLL's d3d9.dll import is patched to the proxy, TEXDEF / TEXH = format and content
// hash of every texture).  Output lines (stdout), read by ghidra E3 f1cmp.py / fmtoracle.py:
//   CAPS tex rt ds disp z pal mode              the device answers, queried here independently of the DLL (encoding of fmtoracle.py)
//   F type flags result                         RwRasterCreate(16, 16, 0, type | DONTALLOCATE | flags) -> raster->format (RW format bits)
//   TXD file kind ntex ok|FAIL                  RwStreamFindChunk(0x23 / 0x16) + RtPITexDictionaryStreamRead / RwTexDictionaryStreamRead
//   TEX file name w h depth format levels hash  every texture, levels locked back through the driver (hash = the proxy's texture hash)
//   NEG name PASS|FAIL detail                   negative fixtures (version window, 31-char names, missing / bad mips) and native round trips
// build: tests/build_rwreplay_f1.sh <dir with mflrw.dll(.a)>     usage: rwreplay_f1.exe list.txt   (list: `file pi|native` per line)
#define WITH_D3D
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "rw.h"
#include "capi/rwmfl.h"
using namespace rw;
typedef unsigned long long u64;
struct RwMemory { uint8_t *start; uint32_t length; };

static int fails;
static u64 fnv(const void *d, size_t n, u64 h){ const unsigned char *p = (const unsigned char*)d; while(n--){ h ^= *p++; h *= 1099511628211ull; } return h; }
static const u64 FNV0 = 14695981039346656037ull;
static u64 combine(const std::vector<u64> &lh){ u64 all = 0; for(size_t i = 0; i < lh.size() && i < 16; i++) all += (lh[i] ^ ((u64)(i + 1) * 0x9E3779B97F4A7C15ull)) * 0xff51afd7ed558ccdull; return all; }
static Driver *drv(void){ return ((Engine*)rwmfl_engine())->driver[PLATFORM_D3D9]; }

// content hash of all levels, read back through the DLL's raster lock (same formula as the proxy's TEXH)
static u64 rasterHash(Raster *r, int *nlev)
{
	Driver *d = drv(); int n = d->rasterNumLevels(r); std::vector<u64> lh;
	for(int l = 0; l < n; l++){
		uint8 *px = d->rasterLock(r, l, Raster::LOCKREAD);
		if(!px){ printf("lock failed level %d\n", l); fails++; break; }
		size_t row = (size_t)r->width * (r->depth / 8); u64 h = FNV0;
		for(int y = 0; y < r->height; y++) h = fnv(px + (size_t)y * r->stride, row, h);
		lh.push_back(h | 1);
		d->rasterUnlock(r, l);
	}
	*nlev = n; return combine(lh);
}

// ---- stream builders for the fixtures (RW chunk = type, size, library stamp)
typedef std::vector<uint8> B;
static uint32 stamp(uint32 v){ return ((v - 0x30000) & 0x3ff00) << 14 | (v & 0x3f) << 16 | 0x65; }
static void u32(B &b, uint32 v){ for(int i = 0; i < 4; i++) b.push_back(v >> 8*i); }
static B chunk(uint32 type, const B &body, uint32 ver = 0x37002){ B b; u32(b, type); u32(b, body.size()); u32(b, stamp(ver)); b.insert(b.end(), body.begin(), body.end()); return b; }
static B cat(B a, const B &c){ a.insert(a.end(), c.begin(), c.end()); return a; }
static uint8 pix(int lvl, int i){ return (uint8)(lvl * 37 + i * 13 + (i >> 5)); }

// native D3D9 texture dictionary, one texture: rwfmt (RW format bits incl. MIPMAP), d3dfmt, bpp; levels written = `write` (missing mips), level 0 size + `grow`
static B nativeTxd(const char *name, int w, int h, uint32 rwfmt, uint32 d3dfmt, int bpp, int levels, int write, int grow, u64 *hash)
{
	B s; u32(s, 9); u32(s, 0x1106); char nm[64] = { 0 }; strncpy(nm, name, 63); s.insert(s.end(), nm, nm + 32); s.insert(s.end(), 32, 0);
	u32(s, rwfmt); u32(s, d3dfmt); s.push_back(w); s.push_back(w >> 8); s.push_back(h); s.push_back(h >> 8); s.push_back(bpp * 8); s.push_back(levels); s.push_back(4);
	uint32 cf = rwfmt & 0xf00; s.push_back(cf == 0x500 || cf == 0x100 || cf == 0x300);	// flags: has alpha
	std::vector<u64> lh;
	for(int l = 0; l < write; l++){
		int lw = w >> l, lhh = h >> l; if(lw < 1) lw = 1; if(lhh < 1) lhh = 1;
		uint32 n = lw * lhh * bpp + (l == 0 ? grow : 0); u32(s, n);
		B lv(n); for(uint32 i = 0; i < n; i++) lv[i] = pix(l, i);
		if(bpp == 4 && (rwfmt & 0xf00) == 0x600) for(uint32 i = 3; i < n; i += 4) lv[i] = 0xff;	// X8R8G8B8 keeps what was written; any value is fine
		lh.push_back(fnv(lv.data(), lw * lhh * bpp, FNV0) | 1); s.insert(s.end(), lv.begin(), lv.end());
	}
	if(hash) *hash = combine(lh);
	B st; u32(st, 1 | 2 << 16);	// one texture, device id 2 (d3d9)
	B tex = chunk(0x15, cat(chunk(1, s), chunk(3, B())));
	return chunk(0x16, cat(cat(chunk(1, st), tex), chunk(3, B())));
}
// PI dictionary, one 32-bit texture of `images` levels (only `write` image chunks present), texture name `name`
static B piTxd(const char *name, int w, int h, int images, int write)
{
	B body; u32(body, 1 | 1 << 16); u32(body, images);
	for(int l = 0; l < write; l++){
		int lw = w >> l, lhh = h >> l; if(lw < 1) lw = 1; if(lhh < 1) lhh = 1;
		B st; u32(st, lw); u32(st, lhh); u32(st, 32); u32(st, lw * 4);
		B px(lw * lhh * 4); for(size_t i = 0; i < px.size(); i++) px[i] = pix(l, i);
		body = cat(body, chunk(0x18, cat(chunk(1, st), px)));
	}
	B f; u32(f, 0x1106); B nm(name, name + strlen(name)); nm.push_back(0); while(nm.size() & 3) nm.push_back(0);
	body = cat(body, chunk(6, cat(cat(cat(chunk(1, f), chunk(2, nm)), chunk(2, B(4, 0))), chunk(3, B()))));
	return chunk(0x23, body);
}
static RwTexDictionary *readMem(B &b, uint32 type)
{
	RwMemory m = { b.data(), (uint32)b.size() }; uint32 len, ver;
	RwStream *s = RwStreamOpen(3, 1, &m); if(!s) return nil;
	RwTexDictionary *d = nil;
	if(RwStreamFindChunk(s, type, &len, &ver)) d = type == 0x23 ? RtPITexDictionaryStreamRead(s) : RwTexDictionaryStreamRead(s);
	RwStreamClose(s, &m);
	return d;
}
static Texture *only(RwTexDictionary *d){ TexDictionary *t = (TexDictionary*)d; return t && !t->textures.isEmpty() ? Texture::fromDict(t->textures.link.next) : nil; }
#define NEG(name, cond, ...) do{ bool ok_ = (cond); printf("NEG %s %s ", name, ok_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); if(!ok_) fails++; }while(0)

static void fixtures(const std::vector<std::string> &pifiles)
{
	// version window 0x35000..0x37002 (RwStreamFindChunk, 0x4dc7be): a shipped PI TXD with its top chunk stamp moved
	if(!pifiles.empty()){
		FILE *f = fopen(pifiles[0].c_str(), "rb"); B b; if(f){ int c; while((c = fgetc(f)) != EOF) b.push_back(c); fclose(f); }
		uint32 vs[4] = { 0x34f3f, 0x35000, 0x37002, 0x37003 };
		for(int i = 0; i < 4; i++){
			B t = b; uint32 st = stamp(vs[i]); memcpy(&t[8], &st, 4);
			RwTexDictionary *d = readMem(t, 0x23); bool in = vs[i] >= 0x35000 && vs[i] <= 0x37002;
			NEG("version", (d != nil) == in, "%x %s", vs[i], d ? "read" : "rejected");
			if(d) RwTexDictionaryDestroy(d);
		}
	}
	// 31-char names (native: a 32-byte field; PI: a string chunk); 32+ chars are cut to 31 and NUL terminated
	const char *n31 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcde", *n32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef", *n40 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn";
	{ B b = nativeTxd(n31, 16, 16, 0x500, 21, 4, 1, 1, 0, nil); RwTexDictionary *d = readMem(b, 0x16); Texture *t = only(d);
	  NEG("name31_native", t && !strcmp(t->name, n31) && RwTexDictionaryFindNamedTexture(d, n31), "%s", t ? t->name : "(no texture)"); if(d) RwTexDictionaryDestroy(d); }
	{ B b = nativeTxd(n32, 16, 16, 0x500, 21, 4, 1, 1, 0, nil); RwTexDictionary *d = readMem(b, 0x16); Texture *t = only(d);
	  NEG("name32_native", t && !strcmp(t->name, n31), "%s", t ? t->name : "(no texture)"); if(d) RwTexDictionaryDestroy(d); }
	{ B b = piTxd(n31, 16, 16, 1, 1); RwTexDictionary *d = readMem(b, 0x23); Texture *t = only(d);
	  NEG("name31_pi", t && !strcmp(t->name, n31) && RwTexDictionaryFindNamedTexture(d, n31), "%s", t ? t->name : "(no texture)"); if(d) RwTexDictionaryDestroy(d); }
	{ B b = piTxd(n40, 16, 16, 1, 1); RwTexDictionary *d = readMem(b, 0x23); Texture *t = only(d);
	  NEG("name40_pi", t && !strcmp(t->name, n31), "%s", t ? t->name : "(no texture)"); if(d) RwTexDictionaryDestroy(d); }
	// missing mips: the header promises more levels than the stream holds -> no dictionary, no crash; a level of the wrong size likewise
	{ B b = nativeTxd("mips", 16, 16, 0x8500, 21, 4, 5, 3, 0, nil); RwTexDictionary *d = readMem(b, 0x16);
	  NEG("missing_mips_native", d == nil, "%s", d ? "read" : "rejected"); if(d) RwTexDictionaryDestroy(d); }
	{ B b = nativeTxd("big", 16, 16, 0x500, 21, 4, 1, 1, 64, nil); RwTexDictionary *d = readMem(b, 0x16);
	  NEG("oversize_level_native", d == nil, "%s", d ? "read" : "rejected"); if(d) RwTexDictionaryDestroy(d); }
	{ B b = piTxd("mips", 16, 16, 3, 2); RwTexDictionary *d = readMem(b, 0x23);
	  NEG("missing_mips_pi", d == nil, "%s", d ? "read" : "rejected"); if(d) RwTexDictionaryDestroy(d); }
	// native round trips per class (no shipped PC native TXD: the data written is the oracle)
	struct { const char *n; uint32 rw, d3d; int bpp, lv; } cl[] = { { "c8888_mip", 0x8500, 21, 4, 5 }, { "c8888", 0x500, 21, 4, 1 }, { "c888_mip", 0x8600, 22, 4, 5 },
		{ "c565", 0x200, 23, 2, 1 }, { "c1555_mip", 0x8100, 25, 2, 5 }, { "c4444", 0x300, 26, 2, 1 } };
	for(size_t i = 0; i < sizeof cl / sizeof cl[0]; i++){
		u64 want; B b = nativeTxd(cl[i].n, 16, 16, cl[i].rw, cl[i].d3d, cl[i].bpp, cl[i].lv, cl[i].lv, 0, &want);
		RwTexDictionary *d = readMem(b, 0x16); Texture *t = only(d); int nl = 0; u64 got = t ? rasterHash(t->raster, &nl) : 0;
		NEG("native_roundtrip", t && got == want && nl == cl[i].lv, "%s fmt %x levels %d hash %016llx want %016llx", cl[i].n, t ? t->raster->format : 0, nl, got, want);
		if(d) RwTexDictionaryDestroy(d);
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	if(argc < 2){ fprintf(stderr, "usage: rwreplay_f1 list.txt\n"); return 2; }
	WNDCLASSA wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "rwf1"; RegisterClassA(&wc);
	HWND win = CreateWindowA("rwf1", "rwf1", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, wc.hInstance, NULL);
	struct { HWND w; } op = { win };
	if(!RwEngineInit(NULL, 0, 0) || !RwEngineOpen((RwEngineOpenParams*)&op) || !RwEngineStart()){ printf("FAIL engine\n"); return 1; }
	IDirect3DDevice9 *dev = (IDirect3DDevice9*)RwD3D9GetCurrentD3DDevice();
	if(!dev){ printf("FAIL no device\n"); return 1; }

	// ---- caps profile (fmtoracle.py CAPS encoding)
	{
		IDirect3D9 *d3d; dev->GetDirect3D(&d3d); D3DDISPLAYMODE m; d3d->GetAdapterDisplayMode(0, &m);
		IDirect3DSurface9 *zs = nil; D3DSURFACE_DESC zd; zd.Format = D3DFMT_D24S8; if(dev->GetDepthStencilSurface(&zs) == D3D_OK){ zs->GetDesc(&zd); zs->Release(); }
		D3DCAPS9 caps; dev->GetDeviceCaps(&caps);
		static const D3DFORMAT f[6] = { D3DFMT_P8, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A4R4G4B4, D3DFMT_L8, D3DFMT_A1R5G5B5 };
		unsigned tex = 0, rt = 0, ds = 0;
		for(int i = 0; i < 6; i++){
			if(d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m.Format, 0, D3DRTYPE_TEXTURE, f[i]) >= 0) tex |= 1 << i;
			if(d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, f[i]) >= 0) rt |= 1 << i;
			if(d3d->CheckDepthStencilMatch(0, D3DDEVTYPE_HAL, m.Format, f[i], zd.Format) >= 0) ds |= 1 << i;
		}
		int disp = m.Format == D3DFMT_R5G6B5 || m.Format == D3DFMT_X1R5G5B5 ? 16 : 32, z = zd.Format == D3DFMT_D16 || zd.Format == D3DFMT_D15S1 ? 16 : 32;
		printf("CAPS %x %x %x %d %d %x %x\n", tex, rt, ds, disp, z, (unsigned)(caps.TextureCaps & 0xff), disp == 16 ? 0x200 : 0x500);
		printf("# display format %u, depth format %u\n", (unsigned)m.Format, (unsigned)zd.Format);
		d3d->Release();
	}
	// ---- format choice grid through RwRasterCreate
	{
		static const int types[] = { 0, 1, 2, 4, 5 }; static const unsigned extra[] = { 0, 0x1000, 0x2000, 0x4000, 0x8000, 0x9000, 0xa000, 0xc000, 0xb000 };
		for(int ti = 0; ti < 5; ti++) for(unsigned fn = 0; fn < 16; fn++) for(int e = 0; e < 9; e++){
			unsigned fl = fn << 8 | extra[e];
			Raster *r = (Raster*)RwRasterCreate(16, 16, 0, types[ti] | 0x80 | fl);
			if(!r){ printf("# create failed type %d flags %x\n", types[ti], fl); continue; }
			printf("F %d %x %x\n", types[ti], fl, (unsigned)r->format);
			RwRasterDestroy((RwRaster*)r);
		}
	}
	// ---- shipped TXDs through the DLL (the d3dtrace proxy records CreateTexture format + content)
	std::vector<std::string> pis;
	FILE *lf = fopen(argv[1], "r"); char ln[512], fn[400], kind[16];
	while(lf && fgets(ln, sizeof ln, lf)){
		if(sscanf(ln, "%399s %15s", fn, kind) != 2) continue;
		bool pi = !strcmp(kind, "pi"); if(pi) pis.push_back(fn);
		RwStream *s = RwStreamOpen(2, 1, fn); uint32 len, ver; RwTexDictionary *d = nil;
		if(s && RwStreamFindChunk(s, pi ? 0x23 : 0x16, &len, &ver)) d = pi ? RtPITexDictionaryStreamRead(s) : RwTexDictionaryStreamRead(s);
		if(s) RwStreamClose(s, nil);
		int n = 0;
		if(d){
			TexDictionary *t = (TexDictionary*)d;
			// list order = creation order (add() appends), so the proxy's TEXDEF ids line up
			std::vector<Texture*> v; FORLIST(lnk, t->textures) v.push_back(Texture::fromDict(lnk));
			for(int i = 0; i < (int)v.size(); i++){
				Raster *r = v[i]->raster; int nl = 0; u64 h = rasterHash(r, &nl);
				printf("TEX %s %s %d %d %d %x %d %016llx\n", fn, v[i]->name, r->width, r->height, r->depth, (unsigned)r->format, nl, h); n++;
			}
		}
		printf("TXD %s %s %d %s\n", fn, kind, n, d ? "ok" : "FAIL");
		if(!d) fails++;
	}
	if(lf) fclose(lf);
	printf("# fixtures\n");
	fixtures(pis);
	printf("RESULT: %s (%d failures)\n", fails ? "FAILED" : "PASS", fails);
	return fails ? 1 : 0;
}
