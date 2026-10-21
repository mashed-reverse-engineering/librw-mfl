// Platform independent texture dictionary (chunk 0x23, "PITexD") reader.
// Clean-room: written from the format description in MashedFormats/FORMATS.md (pitexd.py) and from the
// shipped data files only. Only the librw API is used.
//
//   chunk 0x23: u16 numTextures, u16 deviceId
//     numTextures x {
//     (deviceId == 0: legacy layout, see readTexture)
//       u32 numImages (mip levels, each half the size of the previous one, largest first)
//       numImages x chunk 0x18 { chunk 0x01 (16 bytes): u32 width, height, depth (4/8/32), stride;
//                                pixels (height*stride); palette (4<<depth bytes, depth <= 8 only, AFTER the pixels) }
//       chunk 0x06 { STRUCT u32 filter|addressing, STRING name, STRING mask, EXTENSION } }
//
// 4 bit images are ONE palette index (0..15) per byte, rows padded to a multiple of 4 (like 8 bit), not nibble packed.
// Gamma correction (done by the original on every image) is skipped: the engine gamma is 1.0, the table is the identity.
// The reader never trusts sizes: every access is bounds checked and the chunk sizes must add up exactly.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID 0

namespace rw {

namespace {

enum { MAXDIM = 8192, MAXLEVELS = 16, MAXCHUNK = 256*1024*1024 };

struct Rd {
	const uint8 *p;
	uint32 n, pos;
	bool u32(uint32 *v){
		if(n - pos < 4) return false;
		*v = p[pos] | p[pos+1]<<8 | p[pos+2]<<16 | (uint32)p[pos+3]<<24;
		pos += 4;
		return true;
	}
	// chunk header: type, size, stamp; size must fit in the remainder
	bool hdr(uint32 want, uint32 *size, uint32 *stamp = nil){
		uint32 t, s, v;
		if(!u32(&t) || !u32(&s) || !u32(&v)) return false;
		if(t != want || s > n - pos) return false;
		*size = s;
		if(stamp) *stamp = v;
		return true;
	}
};

// version of a chunk stamp
static uint32 stampVersion(uint32 s){ return ((s >> 14) & 0x3FF00) + 0x30000 | ((s >> 16) & 0x3F); }
static bool versionOK(uint32 s){ uint32 v = stampVersion(s); return v >= 0x35000 && v <= 0x37002; }

// Reads a string chunk (0x02 bytes, 0x13 UTF-16 -> low bytes), skipping other chunks in front. Result is cut at the
// first NUL and truncated to 31 chars. Longer than 128 bytes is rejected (the original overflows its buffer).
static bool
readString(Rd &r, char *dst)
{
	uint32 t, size, stamp, i;
	for(;;){
		if(!r.u32(&t) || !r.u32(&size) || !r.u32(&stamp) || size > r.n - r.pos) return false;
		if(t == ID_STRING || t == 0x13) break;
		r.pos += size;
	}
	if(size > 128 || !versionOK(stamp)) return false;
	memset(dst, 0, 32);
	if(t == 0x13){
		if(size & 1) return false;
		for(i = 0; i < size/2 && i < 31 && r.p[r.pos + 2*i]; i++)
			dst[i] = r.p[r.pos + 2*i];
	}else
		for(i = 0; i < size && i < 31 && r.p[r.pos + i]; i++)
			dst[i] = r.p[r.pos + i];
	r.pos += size;
	return true;
}

#define FAIL(...) do{ RWERROR((ERR_GENERAL, __VA_ARGS__)); goto fail; }while(0)

// Builds a librw Image for one mip level; palette is shared (copied from level 0)
static Image*
makeImage(int32 w, int32 h, int32 depth, int32 stride, const uint8 *pix, const uint8 *pal)
{
	Image *img = Image::create(w, h, depth);
	if(img == nil) return nil;
	img->allocate();	// stride = w*bpp; 4 bit images hold one index per byte, like the file
	for(int32 y = 0; y < h; y++){
		const uint8 *s = pix + (size_t)y*stride;
		uint8 *d = img->pixels + (size_t)y*img->stride;
		memcpy(d, s, (size_t)w*(depth == 32 ? 4 : 1));
		if(depth == 4)
			for(int32 x = 0; x < w; x++)
				if(s[x] > 15){ img->destroy(); return nil; }	// padding bytes past w may be garbage, not checked
	}
	if(depth <= 8) memcpy(img->palette, pal, (size_t)4 << depth);
	return img;
}

struct Level { int32 w, h, stride; const uint8 *pix; };

static Texture*
readTexture(Rd &r, bool legacy)
{
	uint32 numImages, size, v, flags = 0, len;
	Level lv[MAXLEVELS];
	int32 depth = 0;
	const uint8 *pal = nil;
	Texture *tex = nil;
	Raster *ras = nil;
	Image *img = nil;
	char name[32], mask[32];
	uint32 extPos = 0, extEnd = 0;
	int32 fmt, w0, h0, d0, rfmt, i;

	if(legacy){	// 72 byte record: name[32], mask[32], i32 numImages, u32 flags; no texture chunk
		if(r.n - r.pos < 72) FAIL("PITexD: truncated legacy record");
		memset(name, 0, 32); memset(mask, 0, 32);
		for(i = 0; i < 31 && r.p[r.pos + i]; i++) name[i] = r.p[r.pos + i];
		for(i = 0; i < 31 && r.p[r.pos + 32 + i]; i++) mask[i] = r.p[r.pos + 32 + i];
		r.pos += 64;
	}
	if(!r.u32(&numImages) || numImages < 1 || numImages > MAXLEVELS)
		FAIL("PITexD: bad mip level count");
	if(legacy) r.u32(&flags);
	for(i = 0; i < (int32)numImages; i++){
		uint32 st[4], palsz, end;
		if(!r.hdr(ID_IMAGE, &size)) FAIL("PITexD: expected image chunk");
		end = r.pos + size;
		if(!r.hdr(ID_STRUCT, &v) || v != 16 || !r.u32(&st[0]) || !r.u32(&st[1]) || !r.u32(&st[2]) || !r.u32(&st[3]))
			FAIL("PITexD: bad image struct");
		if(st[0] < 1 || st[0] > MAXDIM || st[1] < 1 || st[1] > MAXDIM) FAIL("PITexD: bad image size");
		if(st[2] != 4 && st[2] != 8 && st[2] != 32) FAIL("PITexD: unsupported depth %u", st[2]);
		if(i == 0) depth = st[2];
		else if((int32)st[2] != depth || (int32)st[0] != (lv[i-1].w > 1 ? lv[i-1].w/2 : 1) ||
		        (int32)st[1] != (lv[i-1].h > 1 ? lv[i-1].h/2 : 1)) FAIL("PITexD: bad mip chain");
		uint32 minStride = st[2] == 32 ? st[0]*4 : st[2] == 8 ? st[0] : st[0];
		if(st[3] < minStride || st[3] > 4*MAXDIM) FAIL("PITexD: bad stride");
		palsz = st[2] <= 8 ? 4u << st[2] : 0;
		uint32 pixsz = st[3]*st[1];	// <= 32768*8192, no overflow
		if(size != 28 + palsz + pixsz || r.pos + pixsz + palsz != end || end > r.n)
			FAIL("PITexD: image size mismatch");
		lv[i].w = st[0]; lv[i].h = st[1]; lv[i].stride = st[3]; lv[i].pix = r.p + r.pos;
		if(palsz){
			const uint8 *pp = r.p + r.pos + pixsz;
			if(i == 0) pal = pp;
			else if(memcmp(pal, pp, palsz)) FAIL("PITexD: per-level palettes are not supported");
		}
		r.pos = end;
	}
	if(!legacy && !r.hdr(ID_TEXTURE, &size)) FAIL("PITexD: expected texture chunk");
	if(!legacy){
		uint32 end = r.pos + size, stamp;
		if(!r.hdr(ID_STRUCT, &v, &stamp) || v != 4 || !versionOK(stamp) || !r.u32(&flags)) FAIL("PITexD: bad texture struct");
		if(!readString(r, name)) FAIL("PITexD: bad texture name");
		if(!readString(r, mask)) FAIL("PITexD: bad mask name");
		extPos = r.pos;
		if(!r.hdr(ID_EXTENSION, &len)) FAIL("PITexD: expected extension chunk");
		r.pos += len;
		extEnd = r.pos;
		if(r.pos != end) FAIL("PITexD: texture chunk size mismatch");
	}

	// level 0 through the backend's own image path, further levels through lock()
	img = makeImage(lv[0].w, lv[0].h, depth, lv[0].stride, lv[0].pix, pal);
	if(img == nil) goto fail;
	if(!Raster::imageFindRasterFormat(img, Raster::TEXTURE, &w0, &h0, &d0, &fmt)) goto fail;
	// opaque palettised textures get no alpha format (original rule, see MFL_NOTES.md); d3d rasterFromImage accepts PAL|C888
	if((fmt & (Raster::PAL4|Raster::PAL8)) && (fmt & 0xF00) == Raster::C8888 && !img->hasAlpha())
		fmt = (fmt & ~0xF00) | Raster::C888;
	// 2+ images: MIPMAP; 1 image: none for filter NEAREST/LINEAR, else MIPMAP|AUTOMIPMAP (driver generates the levels)
	rfmt = fmt | (numImages > 1 ? Raster::MIPMAP :
	              ((flags&0xFF) == 1 || (flags&0xFF) == 2) ? 0 : Raster::MIPMAP|Raster::AUTOMIPMAP);
	ras = Raster::create(w0, h0, d0, rfmt);
	if(ras == nil) goto fail;
	if(ras->setFromImage(img) == nil) goto fail;
	img->destroy(); img = nil;
	if(numImages > 1 && (ras->format & Raster::MIPMAP)){
		for(i = 1; i < (int32)numImages; i++){
			img = makeImage(lv[i].w, lv[i].h, depth, lv[i].stride, lv[i].pix, pal);
			if(img == nil) goto fail;
			if(ras->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH) == nil) goto fail;
			Raster *ok = ras->setFromImage(img);
			ras->unlock(i);
			if(ok == nil) goto fail;
			img->destroy(); img = nil;
		}
	}
	tex = Texture::create(ras);
	if(tex == nil) goto fail;
	ras = nil;
	memcpy(tex->name, name, 32);
	memcpy(tex->mask, mask, 32);
	if(legacy){	// U == V == 0 -> WRAP; a single 0 copies the other
		uint32 u = (flags >> 8) & 0xF, vv = (flags >> 12) & 0xF;
		if(!u && !vv) u = vv = 1;
		else if(!u) u = vv;
		else if(!vv) vv = u;
		flags = (flags & 0xFF) | u << 8 | vv << 12;
	}else if((flags & 0xF000) == 0)	// V addressing 0: copy U (as Texture::streamRead)
		flags |= (flags & 0xF00) << 4;
	tex->filterAddressing = flags & 0xFFFF;
	if(!legacy && extEnd - extPos > 12){	// plugin data (empty in all shipped files)
		StreamMemory sm;
		sm.open((uint8*)r.p + extPos, extEnd - extPos);
		bool32 ok = Texture::s_plglist.streamRead(&sm, tex);
		sm.close();
		if(!ok) goto fail;
	}
	return tex;
fail:
	if(img) img->destroy();
	if(ras) ras->destroy();
	if(tex) tex->destroy();
	return nil;
}

}	// namespace

// Parses a complete chunk (header included) from memory. Returns a new dictionary or nil.
// The whole buffer must be consumed, trailing bytes are an error.
TexDictionary*
TexDictionary::readPI(const uint8 *data, uint32 size)
{
	Rd r = { data, size, 0 };
	uint32 len, v;
	TexDictionary *txd = nil;
	if(!r.hdr(ID_PITEXDICTIONARY, &len)){
		RWERROR((ERR_CHUNK, "PITEXDICTIONARY"));
		return nil;
	}
	if(len != size - 12 || len < 4){
		RWERROR((ERR_GENERAL, "PITexD: size mismatch"));
		return nil;
	}
	r.u32(&v);
	uint32 numTex = v & 0xFFFF;
	bool legacy = (v >> 16) == 0;	// deviceId 0: legacy layout (not in the shipped files), else current
	txd = TexDictionary::create();
	if(txd == nil) return nil;
	for(uint32 i = 0; i < numTex; i++){
		Texture *tex = readTexture(r, legacy);
		if(tex == nil){
			txd->destroy();	// destroys the textures added so far
			return nil;
		}
		txd->add(tex);
	}
	if(r.pos != size){
		RWERROR((ERR_GENERAL, "PITexD: trailing data"));
		txd->destroy();
		return nil;
	}
	return txd;
}

// Reads a PI dictionary from a stream positioned at the chunk header (consumes exactly that chunk).
TexDictionary*
TexDictionary::streamReadPI(Stream *stream)
{
	uint32 hdr[3];
	if(stream->read32(hdr, 12) != 12 || hdr[0] != ID_PITEXDICTIONARY){
		RWERROR((ERR_CHUNK, "PITEXDICTIONARY"));
		return nil;
	}
	if(hdr[1] > MAXCHUNK || hdr[1] < 4){
		RWERROR((ERR_GENERAL, "PITexD: bad chunk size"));
		return nil;
	}
	uint8 *buf = rwNewT(uint8, hdr[1] + 12, MEMDUR_EVENT | ID_TEXTURE);
	memcpy(buf, hdr, 12);
	TexDictionary *txd = nil;
	if(stream->read8(buf + 12, hdr[1]) != hdr[1])
		RWERROR((ERR_GENERAL, "PITexD: truncated stream"));
	else
		txd = readPI(buf, hdr[1] + 12);
	rwFree(buf);
	return txd;
}

}
