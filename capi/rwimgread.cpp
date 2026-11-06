// IMGREAD (LIB_RW group imgread, plan_next_24h_2026-11-05_v2 item 3c): the BMP image file reader of MFL.exe (rtbmp), written op for op from its
// disassembly (integer only):  RtBMPImageRead 0x53d050.
// Header: 'BM', BITMAPCOREHEADER (12) or BITMAPINFOHEADER (40), other sizes and RLE4 / BITFIELDS fail with RwError 0x80000009; palette BGR(x) -> RGBA
// with alpha 0xff; rows bottom up through BMPImageSetSpan (bgr = 1 for RLE8); depth 1 / 4 -> 4, 8 -> 8, else 32.  The row buffer is
// RwMalloc(((width + 7) & ~7) * ((bpp + 7) / 8), 0x10000) as in the original (rows wider than that overrun it there too).
// Known quirks kept: the palette read / unknown header size paths close the stream without destroying the image (leak), RLE8 delta (escape 2) fails.
// Exe-layout adapter: mflrw_imgread_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c) hands over &RwEngineInstance 0x7d2128 (RwMalloc +0x108,
// RwFree +0x10c), RwStreamOpen 0x4dc370, RwStreamRead 0x4dbe70, RwStreamSkip 0x4dc190, RwStreamClose 0x4dc2a0, RwImageCreate 0x4dd550,
// RwImageDestroy 0x4dd5b0, RwImageAllocatePixels 0x4dd610, BMPImageSetSpan 0x53d6c0 (exe entry: original or the rwleaf2 swap), RwErrorSet 0x4ea160.
// Check: tools/delink/rw_imgread_check.sh (the original code and this export natively under Wine, same harness heap and stream).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

struct Image { int32_t flags, width, height, depth, stride; u8 *pixels, *palette; };

typedef void *(__cdecl *OpenFn)(int32_t type, int32_t access, const void *data);
typedef u32 (__cdecl *ReadFn)(void *s, void *buf, u32 n);
typedef void *(__cdecl *SkipFn)(void *s, u32 n);
typedef int32_t (__cdecl *CloseFn)(void *s, void *data);
typedef Image *(__cdecl *CreateFn)(int32_t w, int32_t h, int32_t depth);
typedef int32_t (__cdecl *DestroyFn)(Image *im);
typedef Image *(__cdecl *AllocPixFn)(Image *im);
typedef int32_t (__cdecl *SpanFn)(Image *im, int32_t y, const u8 *src, int32_t bpp, int32_t bgr);
typedef void *(__cdecl *ErrorSetFn)(int32_t *err);
typedef void *(__cdecl *MallocFn)(u32 n, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);

static u8 **g_engine;
static OpenFn g_open; static ReadFn g_read; static SkipFn g_skip; static CloseFn g_close;
static CreateFn g_create; static DestroyFn g_destroy; static AllocPixFn g_allocPixels; static SpanFn g_span; static ErrorSetFn g_errorSet;
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] imgread called before mflrw_imgread_bind\n"); abort(); }
	return *g_engine;
}
static void *rwMalloc(u32 n, u32 hint){ return (*(MallocFn *)(eng() + 0x108))(n, hint); }
static void rwFree(void *p){ (*(FreeFn *)(eng() + 0x10c))(p); }
static void err(int32_t code){ int32_t e[2] = {0x19d, code}; g_errorSet(e); }	// rtbmp plugin id 0x19d; _rwerror 0x53eac0 returns its first argument
static inline int16_t s16(const u8 *p){ int16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t s32(const u8 *p){ int32_t v; memcpy(&v, p, 4); return v; }

extern "C" {

void __cdecl mflrw_imgread_bind(u8 **engine, void *open, void *read, void *skip, void *close, void *create, void *destroy, void *allocPixels,
                                void *setSpan, void *errorSet){
	g_engine = engine; g_open = (OpenFn)open; g_read = (ReadFn)read; g_skip = (SkipFn)skip; g_close = (CloseFn)close;
	g_create = (CreateFn)create; g_destroy = (DestroyFn)destroy; g_allocPixels = (AllocPixFn)allocPixels; g_span = (SpanFn)setSpan; g_errorSet = (ErrorSetFn)errorSet;
}

// RtBMPImageRead 0x53d050
Image *__cdecl rw37_RtBMPImageRead(const char *name){
	u8 magic[4], hdr[0x400], rle[2];
	void *s = g_open(2, 1, name);			// rwSTREAMFILENAME, rwSTREAMREAD
	if(!s) return NULL;
	if(g_read(s, magic, 4) != 4 || (magic[0] | magic[1] << 8) != 0x4d42){ g_close(s, NULL); return NULL; }
	// the next 14 bytes land at hdr + 2: file size high half, reserved, bfOffBits (+8), biSize (+0xc); then biSize - 4 bytes from hdr + 0
	if(g_read(s, hdr + 2, 14) != 14){ g_close(s, NULL); return NULL; }
	u32 offBits = (u32)s32(hdr + 8);
	int32_t biSize = s32(hdr + 0xc);
	u32 n = (u32)(biSize - 4);
	if(g_read(s, hdr, n) != n){ g_close(s, NULL); return NULL; }
	u32 used = n + 0x12;
	int32_t w, h, compression, bpp, ncol;
	int16_t bitCount;
	if(biSize == 12){
		w = s16(hdr); h = s16(hdr + 2); bitCount = s16(hdr + 6); compression = 0;
		goto fullPalette;
	}
	w = s32(hdr); h = s32(hdr + 4); bitCount = s16(hdr + 0xa); compression = s32(hdr + 0xc);
	ncol = s32(hdr + 0x1c);			// biClrUsed
	if(compression == 2){ err((int32_t)0x80000009); g_close(s, NULL); return NULL; }
	if(ncol > 0){
		bpp = bitCount;
		if(ncol <= 1 << (bpp & 31)) goto depth;
	}
fullPalette:
	bpp = bitCount;
	ncol = 1 << (bpp & 31);
depth:
	{
	int32_t depth = bpp == 1 || bpp == 4 ? 4 : bpp == 8 ? 8 : 32;
	Image *im = g_create(w, h, depth);
	if(!im){ g_close(s, NULL); return NULL; }
	if(!g_allocPixels(im)) goto destroyClose;
	if(bitCount <= 8){
		u8 *pal = im->palette;
		if(biSize == 12){
			u32 k = (u32)(ncol * 3);
			if(g_read(s, hdr, k) != k){ g_close(s, NULL); return NULL; }
			used += k;
			for(int32_t i = 0; i < ncol; i++){ pal[4 * i] = hdr[3 * i + 2]; pal[4 * i + 1] = hdr[3 * i + 1]; pal[4 * i + 2] = hdr[3 * i]; pal[4 * i + 3] = 0xff; }
		}else if(biSize == 0x28){
			u32 k = (u32)(ncol * 4);
			if(g_read(s, hdr, k) != k){ g_close(s, NULL); return NULL; }
			used += k;
			for(int32_t i = 0; i < ncol; i++){ pal[4 * i] = hdr[4 * i + 2]; pal[4 * i + 1] = hdr[4 * i + 1]; pal[4 * i + 2] = hdr[4 * i]; pal[4 * i + 3] = 0xff; }
		}else{ err((int32_t)0x80000009); g_close(s, NULL); return NULL; }
	}
	if(!g_skip(s, offBits - used)) goto destroyClose;
	{
	int32_t rowBytes = ((bpp * w + 7) / 8 + 3) & ~3;
	int32_t pixBytes = (bpp + 7) / 8;
	u8 *buf = (u8 *)rwMalloc((u32)(((w + 7) & ~7) * pixBytes), 0x10000);
	if(!buf){ err((int32_t)0x80000013); g_destroy(im); g_close(s, NULL); return NULL; }
	if(compression == 0 || bitCount == 24 || bitCount == 32){
		for(int32_t y = 0, row = h - 1; y < h; y++, row--){
			if(g_read(s, buf, (u32)rowBytes) != (u32)rowBytes) goto freeFail;
			g_span(im, row, buf, bpp, 0);
		}
	}else if(compression == 1){		// RLE8
		for(int32_t y = 0; y < h; y++){
			int32_t x = 0;
			for(;;){
				if(g_read(s, rle, 2) != 2) goto freeFail;
				if(rle[0]){ memset(buf + x, rle[1], rle[0]); x += rle[0]; continue; }
				if(rle[1] > 2){
					u32 k = rle[1];
					if(g_read(s, buf + x, k) != k) goto freeFail;
					x += rle[1];
					if(rle[1] & 1){ if(g_read(s, rle, 1) != 1) goto freeFail; }
					continue;
				}
				if(rle[1] == 0) break;			// end of line
				if(rle[1] != 1) goto freeFail;		// delta: not supported
				y = h - 1;				// end of bitmap
				break;
			}
			g_span(im, h - y - 1, buf, bpp, 1);
		}
	}else{
		err((int32_t)0x80000009);
		goto freeFail;
	}
	rwFree(buf);
	g_close(s, NULL);
	return im;
freeFail:
	rwFree(buf);
	g_destroy(im);
	g_close(s, NULL);
	return NULL;
	}
destroyClose:
	g_destroy(im);
	g_close(s, NULL);
	return NULL;
	}
}

}
