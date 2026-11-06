// RWPNGREAD (LIB_RW group pngread, R3 2026-11-06): RtPNGImageRead of MFL.exe (rtpng.obj 0x524880) and its stream / memory callbacks
// (0x524820 malloc, 0x524840 free, 0x524cb0 read), written from the disassembly on top of the same libpng 1.0.12 + zlib 1.1.3 the exe links
// (capi/png/, built with PNG_NO_STDIO PNG_USER_MEM_SUPPORTED PNG_NO_CONSOLE_IO as the exe's copy).  libpng, its setjmp and every longjmp stay
// inside this DLL; the exe only supplies the stream (RwStreamOpen / Read / Close) and the image (RwImageCreate / AllocatePixels / FreePixels /
// Destroy); all memory comes from the exe engine (malloc +0x108 / free +0x10c, hint 0x3010d), so a corrupt file costs no foreign free.
// Kept quirks of the original: the read callback ignores short reads (no "Read Error"); after a libpng error the image is not destroyed (only the
// row table and the png structs go); colour type 1 / 5 (invalid) get an image of depth 0.
// Exe-layout adapter: mflrw_pngread_bind (rw_engine_init, recomp/automatch_near/an_hk_612.c).
// Check: tools/delink/rw_pngread_check.sh (the original code and this export natively under Wine on every PNG of the game + corrupt variants).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define PNG_NO_STDIO				// the same configuration as capi/png/*.c (tools/build_mflrw.sh): png_struct's layout depends on it
#define PNG_USER_MEM_SUPPORTED
#define PNG_NO_CONSOLE_IO
extern "C" {
#include "png/png.h"
}
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef void *(__cdecl *MallocFn)(u32 size, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__cdecl *StreamOpenFn)(int32_t type, int32_t access, const void *data);
typedef u32 (__cdecl *StreamReadFn)(void *stream, void *buffer, u32 length);
typedef int32_t (__cdecl *StreamCloseFn)(void *stream, void *data);
typedef u8 *(__cdecl *ImageCreateFn)(int32_t width, int32_t height, int32_t depth);
typedef u8 *(__cdecl *ImagePtrFn)(u8 *image);
typedef int32_t (__cdecl *ImageBoolFn)(u8 *image);
typedef void *(__cdecl *ErrorSetFn)(void *error);
typedef int32_t (__cdecl *ErrorCodeFn)(int32_t code, ...);

static char **g_engine;
static StreamOpenFn g_open; static StreamReadFn g_read; static StreamCloseFn g_close;
static ImageCreateFn g_imageCreate; static ImagePtrFn g_allocPixels, g_freePixels; static ImageBoolFn g_imageDestroy;
static ErrorSetFn g_errorSet; static ErrorCodeFn g_errorCode;

enum { rwSTREAMFILENAME = 2, rwSTREAMREAD = 1, HINT = 0x3010d, rwID_PNGIMAGEPLUGIN = 0x10d, E_RW_NULLP = 0x80000016 };
static void *emalloc(u32 n){ return (*(MallocFn *)(*g_engine + 0x108))(n, HINT); }
static void efree(void *p){ (*(FreeFn *)(*g_engine + 0x10c))(p); }

static png_voidp pngMalloc(png_structp, png_size_t size){ return emalloc((u32)size); }		// 0x524820
static void pngFree(png_structp, png_voidp p){ efree(p); }					// 0x524840
static void pngRead(png_structp png, png_bytep data, png_size_t length){			// 0x524cb0: the stream, else png_error
	void *stream = png_get_io_ptr(png);
	if(stream) g_read(stream, data, (u32)length);
	else png_error(png, 0);
}

extern "C" {

void __cdecl mflrw_pngread_bind(char **engine, void *streamOpen, void *streamRead, void *streamClose, void *imageCreate, void *allocPixels, void *freePixels,
                                void *imageDestroy, void *errorSet, void *errorCode){
	g_engine = engine; g_open = (StreamOpenFn)streamOpen; g_read = (StreamReadFn)streamRead; g_close = (StreamCloseFn)streamClose;
	g_imageCreate = (ImageCreateFn)imageCreate; g_allocPixels = (ImagePtrFn)allocPixels; g_freePixels = (ImagePtrFn)freePixels;
	g_imageDestroy = (ImageBoolFn)imageDestroy; g_errorSet = (ErrorSetFn)errorSet; g_errorCode = (ErrorCodeFn)errorCode;
}

// 0x524880: the PNG file `name` as an RwImage {+0x10 stride, +0x14 pixels, +0x18 palette}: 8 bit palette (4 bit for palettes under 8 bit, packed one
// index per byte), 8 bit grey (expanded, grey ramp palette), else 32 bit RGBA (RGB + 0xff filler, grey + alpha to RGBA); 16 bit channels stripped.
// NULL for a NULL name (RwError), a missing file, or a libpng error.
u8 *__cdecl rw37_RtPNGImageRead(const char *name){
	if(!name){
		int32_t err[2] = {rwID_PNGIMAGEPLUGIN, g_errorCode(E_RW_NULLP)};
		g_errorSet(err);
		return 0;
	}
	void *stream = g_open(rwSTREAMFILENAME, rwSTREAMREAD, name);
	if(!stream) return 0;
	// volatile: they are read after a longjmp back into this frame
	png_structp volatile png = 0;
	png_infop volatile info = 0;
	u8 **volatile rows = 0;
	void *volatile streamV = stream;
	int32_t depth = 0;
	png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, 0, 0, 0, 0, pngMalloc, pngFree);
	if(!png){ g_close(stream, 0); return 0; }
	info = png_create_info_struct(png);
	if(!info){
		g_close(stream, 0);
		png_structp p = png; png_destroy_read_struct(&p, 0, 0);
		return 0;
	}
	if(setjmp(png->jmpbuf)){
		if(rows){ efree(rows); rows = 0; }
		g_close(streamV, 0); streamV = 0;
		png_structp p = png; png_infop i = info;
		png_destroy_read_struct(&p, &i, 0);
		png = 0; info = 0;
		return 0;
	}
	png_set_read_fn(png, stream, pngRead);
	png_read_info(png, info);
	png_uint_32 width, height; int bitDepth, colorType;
	png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, 0, 0, 0);
	png_set_strip_16(png);
	switch(colorType){
	case PNG_COLOR_TYPE_GRAY:
		depth = 8;
		if(bitDepth < 8) png_set_gray_1_2_4_to_8(png);
		break;
	case PNG_COLOR_TYPE_RGB:
		depth = 32;
		png_set_filler(png, 0xff, PNG_FILLER_AFTER);
		break;
	case PNG_COLOR_TYPE_PALETTE:
		depth = 8;
		if(bitDepth < 8){ png_set_packing(png); depth = 4; }
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		depth = 32;
		png_set_gray_to_rgb(png);
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		depth = 32;
		break;
	}
	u8 *image = g_imageCreate((int32_t)width, (int32_t)height, depth);
	if(!image){ g_close(stream, 0); return 0; }		// as the original: the png structs are not destroyed here
	if(!g_allocPixels(image)){
		g_imageDestroy(image);
		g_close(stream, 0);
		return 0;
	}
	u8 *palette = *(u8 **)(image + 0x18), *pixels = *(u8 **)(image + 0x14);
	int32_t stride = *(int32_t *)(image + 0x10);
	rows = (u8 **)emalloc((u32)height * 4);
	if(!rows){
		g_freePixels(image);
		g_imageDestroy(image);
		g_close(streamV, 0);
		return 0;
	}
	for(int32_t y = 0; y < (int32_t)height; y++) rows[y] = pixels + stride * y;
	if((depth == 4 || depth == 8) && palette){
		int32_t n = 1 << depth;
		for(int32_t k = 0; k < n; k++){ palette[4*k] = palette[4*k + 1] = palette[4*k + 2] = (u8)k; palette[4*k + 3] = 0xff; }
		if(png_get_valid(png, info, PNG_INFO_PLTE)){
			png_colorp pal; int numPal;
			png_get_PLTE(png, info, &pal, &numPal);
			if(n < numPal) numPal = n;
			for(int32_t k = 0; k < numPal; k++){ palette[4*k] = pal[k].red; palette[4*k + 1] = pal[k].green; palette[4*k + 2] = pal[k].blue; }
		}
		if(png_get_valid(png, info, PNG_INFO_tRNS)){
			png_bytep trans; int numTrans; png_color_16p tc;
			png_get_tRNS(png, info, &trans, &numTrans, &tc);
			for(int32_t k = 0; k < numTrans; k++) palette[4*k + 3] = trans[k];
		}
	}
	png_read_image(png, (png_bytepp)rows);
	png_read_end(png, info);
	{ png_structp p = png; png_infop i = info; png_destroy_read_struct(&p, &i, 0); }
	png = 0; info = 0;
	efree(rows); rows = 0;
	g_close(streamV, 0);
	return image;
}

}
