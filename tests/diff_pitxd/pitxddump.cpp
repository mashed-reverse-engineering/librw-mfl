// Dump of what the fork's PI reader produces (via the test's in-memory driver), one line per texture / level.
// Format must match orig_reader.py dump.
#define main pitxd_test_main
#include "/home/daniel/git/mashed/librw-mfl/tests/pitxd_test.cpp"
#undef main
#include <zlib.h>

static unsigned h32(const uint8_t *p, size_t n) { return (unsigned)crc32(0, p, n); }

int main(int argc, char **argv)
{
	Engine::init(); Engine::open(nil); Engine::start();
	installDriver();
	for(int i = 1; i < argc; i++){
		Buf b;
		if(!readFile(argv[i], b) || b.size() < 12 || U32(&b[0]) != 0x23) continue;
		TexDictionary *txd = TexDictionary::readPI(b.data(), b.size());
		if(!txd){ printf("FAILLOAD\n"); continue; }
		printf("dict %d\n", txd->count());
		FORLIST(lnk, txd->textures){
			Texture *t = Texture::fromDict(lnk);
			Raster *r = t->raster;
			MemRas &m = g_ras[r];
			int depth = (r->format & Raster::PAL4) ? 4 : (r->format & Raster::PAL8) ? 8 : 32;
			printf("tex '%s' '%s' fa %04x %dx%d d%d mip %d auto %d fmt %s%s\n", t->name, t->mask, t->filterAddressing,
			       r->width, r->height, depth, !!(r->format & Raster::MIPMAP), !!(r->format & Raster::AUTOMIPMAP),
			       depth <= 8 ? (depth == 4 ? "PAL4|" : "PAL8|") : "", ((r->format & 0xF00) == Raster::C8888) ? "8888" : "888");
			if(depth <= 8) printf("  pal %08x\n", h32(m.pal.data(), depth == 4 ? 64 : 1024));
			// levels actually written: those that are not 0xCD fill (fork fills exactly numImages levels)
			for(size_t k = 0; k < m.lv.size(); k++){
				if((int)k >= m.unlocks) break;
				int w = r->width >> k, h = r->height >> k;
				printf("  lvl %zu %dx%d %08x\n", k, w ? w : 1, h ? h : 1, h32(m.lv[k].data(), m.lv[k].size()));
			}
		}
		txd->destroy();
		gcRas();
	}
	return 0;
}
