// RW 3.7 (MFL.exe, D3D9 driver) byte layouts of the raster / texture / texture dictionary objects and of the D3D9 raster extension, for DLL code that
// shares these objects with the exe's original D3D9 driver (texdict spike S6, plan_texdict_2026-11-03.md). Every offset is read from the exe's code:
//   RwRaster        RwRasterCreate 0x4d7950 (0x1c/0x1e int16, 0x21/0x22 bytes, parent = self), _rwD3D9RasterCreate 0x4e20f0 (cType = flags & 7 at 0x20,
//                   cFlags = flags & 0xf8 at 0x21), _rwD3D9CameraBeginUpdate 0x4da2a0 (originalWidth/Height 0x28/0x2c), SDK rwcore.h:1365 (same order)
//   ext             _rwD3D9RasterCreate (0, 4, 8, 9, 0xa, 0xb = 0xff, 0xc, 0x18), rwD3D9RasterCtor (0x1c), camera begin (0x1c swap chain, 0x20 HWND)
//   RwTexture       RwTextureCreate 0x4d5b90 / Destroy 0x4d5bf0 / FindNamedTexture 0x4d5d90 / name setters 0x4d5c70 / 0x4d5ce0
// The exe's raster plugin area starts at sizeof(RwRaster37); the D3D9 driver is the only raster plugin (RwRasterRegisterPlugin has one caller), so
// _RwD3D9RasterExtOffset (0x947144) is 0x34 at run time and the registry's sizeOfStruct (rasterTKList 0x62e718 + 0) is 0x58.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct RwRaster37 {
	RwRaster37 *parent;		// 0x00  top level raster, self when not a sub raster
	uint8_t *cpPixels;		// 0x04  locked pixels
	uint8_t *palette;		// 0x08
	int32_t width, height, depth;	// 0x0c 0x10 0x14
	int32_t stride;			// 0x18
	int16_t nOffsetX, nOffsetY;	// 0x1c 0x1e  sub raster offset
	uint8_t cType;			// 0x20  type (flags & 7: 0 normal, 1 z buffer, 2 camera, 4 texture, 5 camera texture)
	uint8_t cFlags;			// 0x21  flags & 0xf8 (0x80 dont allocate)
	uint8_t privateFlags;		// 0x22  lock state
	uint8_t cFormat;		// 0x23  format >> 8 (RwRasterGetFormat = cFormat << 8)
	uint8_t *originalPixels;	// 0x24
	int32_t originalWidth;		// 0x28
	int32_t originalHeight;		// 0x2c
	int32_t originalStride;		// 0x30
};
static_assert(offsetof(RwRaster37, width) == 0x0c && offsetof(RwRaster37, stride) == 0x18, "RwRaster37 dims");
static_assert(offsetof(RwRaster37, nOffsetX) == 0x1c && offsetof(RwRaster37, nOffsetY) == 0x1e, "RwRaster37 offsets");
static_assert(offsetof(RwRaster37, cType) == 0x20 && offsetof(RwRaster37, cFlags) == 0x21, "RwRaster37 type bytes");
static_assert(offsetof(RwRaster37, privateFlags) == 0x22 && offsetof(RwRaster37, cFormat) == 0x23, "RwRaster37 format bytes");
static_assert(offsetof(RwRaster37, originalWidth) == 0x28 && offsetof(RwRaster37, originalHeight) == 0x2c, "RwRaster37 original size");
static_assert(sizeof(RwRaster37) == 0x34, "RwRaster37 size = start of the plugin area");

// _rwD3D9RasterExt, at raster + _RwD3D9RasterExtOffset
struct RwD3D9RasterExt37 {
	void *texture;			// 0x00  IDirect3DTexture9 / surface for camera rasters
	void *palette;			// 0x04
	uint8_t alpha;			// 0x08
	uint8_t cubeFace;		// 0x09  cube : 4, face : 4
	uint8_t autoMipCompressed;	// 0x0a  automipmapgen : 4, compressed : 4
	uint8_t lockedMipLevel;		// 0x0b  0xff = not locked
	void *lockedSurface;		// 0x0c
	int32_t lockedPitch;		// 0x10  D3DLOCKED_RECT
	void *lockedBits;		// 0x14
	uint32_t d3dFormat;		// 0x18
	void *swapChain;		// 0x1c  camera rasters of a window (camera begin tests it first)
	void *window;			// 0x20  HWND
};
static_assert(offsetof(RwD3D9RasterExt37, d3dFormat) == 0x18 && offsetof(RwD3D9RasterExt37, swapChain) == 0x1c, "ext");
static_assert(sizeof(RwD3D9RasterExt37) == 0x24, "ext size");

struct RwLLLink37 { RwLLLink37 *next, *prev; };
struct RwTexture37 {
	RwRaster37 *raster;		// 0x00
	void *dict;			// 0x04
	RwLLLink37 lInDictionary;	// 0x08
	char name[32];			// 0x10
	char mask[32];			// 0x30
	uint8_t filter;			// 0x50  filterAddressing low byte
	uint8_t addressing;		// 0x51  (U | V << 4, created as 0x11)
	uint16_t pad;
	int32_t refCount;		// 0x54
};
static_assert(offsetof(RwTexture37, name) == 0x10 && offsetof(RwTexture37, filter) == 0x50 && offsetof(RwTexture37, refCount) == 0x54, "RwTexture37");
struct RwTexDictionary37 {
	uint8_t type, subType, flags, privateFlags; void *parent;	// RwObject
	RwLLLink37 texturesInDict;	// 0x08  list head
	RwLLLink37 lInInstance;		// 0x10
};
static_assert(offsetof(RwTexDictionary37, texturesInDict) == 8, "RwTexDictionary37");

// RwGlobals (RwEngineInstance) offsets the bridge uses: stdFunc[] at 0x48, memory functions 0x108..
enum { RW37_STD_CAMERABEGIN = 0x4c, RW37_STD_RASTERCREATE = 0x58, RW37_STD_RASTERDESTROY = 0x5c, RW37_STD_CAMERAEND = 0x70, RW37_STD_RASTERLOCK = 0x84,
       RW37_FREELISTALLOC = 0x118, RW37_FREELISTFREE = 0x11c };
