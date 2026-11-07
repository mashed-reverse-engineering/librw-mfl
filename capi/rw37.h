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
enum { RW37_STD_CAMERABEGIN = 0x4c, RW37_STD_RASTERCREATE = 0x58, RW37_STD_RASTERDESTROY = 0x5c, RW37_STD_CAMERAEND = 0x70, RW37_STD_RASTERSUBRASTER = 0x78,
       RW37_STD_RASTERLOCK = 0x84, RW37_STD_RASTERUNLOCK = 0x88, RW37_STD_RASTERLOCKPALETTE = 0xa4, RW37_STD_RASTERUNLOCKPALETTE = 0xa8,
       RW37_STD_RASTERGETMIPLEVELS = 0xb8, RW37_STRNCPY = 0xd0, RW37_STRLEN = 0xf4, RW37_FREELISTALLOC = 0x118, RW37_FREELISTFREE = 0x11c };
// RwPluginRegistry (sizeOfStruct at 0, first / last entry 0x10 / 0x14) and its entries (offset 0, size 4, ctor 0x20, dtor 0x24, next 0x30, prev 0x34):
// rwPluginRegistryInitObject 0x4e9ce0 / DeInitObject 0x4e9d40
struct RwPluginRegEntry37 {
	int32_t offset, size;		// 0x00 0x04
	uint8_t pad08[0x18];
	void *(__cdecl *ctor)(void *object, int32_t offset, int32_t size);	// 0x20
	void *(__cdecl *dtor)(void *object, int32_t offset, int32_t size);	// 0x24
	uint8_t pad28[8];
	RwPluginRegEntry37 *next, *prev;	// 0x30 0x34
};
static_assert(offsetof(RwPluginRegEntry37, ctor) == 0x20 && offsetof(RwPluginRegEntry37, next) == 0x30, "RwPluginRegEntry37");
struct RwPluginRegistry37 { int32_t sizeOfStruct; uint8_t pad04[0xc]; RwPluginRegEntry37 *first, *last; };
static_assert(offsetof(RwPluginRegistry37, first) == 0x10, "RwPluginRegistry37");

// ---- P2 (engine switch 2026-11-07): layout asserts for the objects the D/X kernels and the flip touch. Asserts only, no code uses these types yet.
// RwRaster37 / ext: the remaining fields (_rwD3D9RasterCreate, rwD3D9RasterCtor, _rwD3D9RasterLock 0x4dfcd0)
static_assert(offsetof(RwRaster37, cpPixels) == 0x04 && offsetof(RwRaster37, palette) == 0x08 && offsetof(RwRaster37, depth) == 0x14, "RwRaster37 pixels");
static_assert(offsetof(RwRaster37, originalPixels) == 0x24 && offsetof(RwRaster37, originalStride) == 0x30, "RwRaster37 original pixels");
static_assert(offsetof(RwD3D9RasterExt37, palette) == 0x04 && offsetof(RwD3D9RasterExt37, alpha) == 0x08 && offsetof(RwD3D9RasterExt37, lockedMipLevel) == 0x0b, "ext bytes");
static_assert(offsetof(RwD3D9RasterExt37, lockedSurface) == 0x0c && offsetof(RwD3D9RasterExt37, lockedPitch) == 0x10 && offsetof(RwD3D9RasterExt37, lockedBits) == 0x14, "ext lock");
static_assert(offsetof(RwD3D9RasterExt37, window) == 0x20 && sizeof(RwTexture37) == 0x58, "ext window / texture size");

// RwImage (d3d9convrt reads / writes images): RwImageCreate 0x4dd550 (width 4, height 8, depth 0xc, cpPixels 0x14 = palette 0x18 = flags 0 = 0),
// RwImageAllocatePixels 0x4dd610 (stride 0x10 = ((depth + 7) >> 3) * width + 3 & ~3, palette behind the pixels for depth 4 / 8, flags |= 1 owns pixels)
struct RwImage37 {
	int32_t flags;			// 0x00
	int32_t width, height, depth;	// 0x04 0x08 0x0c
	int32_t stride;			// 0x10
	uint8_t *cpPixels;		// 0x14
	uint8_t *palette;		// 0x18  RGBA entries
};
static_assert(offsetof(RwImage37, depth) == 0x0c && offsetof(RwImage37, stride) == 0x10 && offsetof(RwImage37, palette) == 0x18 && sizeof(RwImage37) == 0x1c, "RwImage37");

// RwGlobals (RwEngineInstance, capi/core.cpp shadow of 0x140 bytes): curCamera 0, curWorld 4 (42 / 1 game reads), dOpenDevice 0x10 (copied as 56 bytes,
// fpRenderStateSet 0x20: 604 game reads; filled by mflrw_shadow_device, capi/d3d9dev.cpp), stdFunc 0x48, stringFuncs 0xc4, memoryFuncs 0x108.. (audit
// ghidra/output/rw_offset_audit_2026-11-03.csv rows RwEngineInstance)
struct RwDevice37 {
	float gammaCorrection;		// 0x00 (global 0x10)
	void *fpSystem;			// 0x04
	float zBufferNear, zBufferFar;	// 0x08 0x0c (global 0x18 / 0x1c)
	void *fpRenderStateSet, *fpRenderStateGet;	// 0x10 0x14 (global 0x20 / 0x24)
	void *fpIm2DRenderLine, *fpIm2DRenderTriangle, *fpIm2DRenderPrimitive, *fpIm2DRenderIndexedPrimitive;	// 0x18..0x24 (global 0x28..0x34)
	void *fpIm3D[4];		// 0x28..0x34
};
static_assert(sizeof(RwDevice37) == 56 && offsetof(RwDevice37, fpRenderStateSet) == 0x10 && offsetof(RwDevice37, fpIm2DRenderIndexedPrimitive) == 0x24, "RwDevice37");
struct RwGlobals37 {
	void *curCamera, *curWorld;	// 0x00 0x04
	uint8_t pad08[8];
	RwDevice37 dOpenDevice;		// 0x10
	void *stdFunc[29];		// 0x48
	uint8_t padbc[8];
	void *stringFuncs[17];		// 0xc4  vecSprintf .. (strcpy +8, strcat +0x10, strcmp +0x24, strlen +0x30)
	void *memoryFuncs[4];		// 0x108 rwmalloc, rwfree, rwrealloc, rwcalloc
	void *freeListAlloc, *freeListFree;	// 0x118 0x11c
};
static_assert(offsetof(RwGlobals37, dOpenDevice) == 0x10 && offsetof(RwGlobals37, dOpenDevice.fpRenderStateSet) == 0x20, "RwGlobals37 device");
static_assert(offsetof(RwGlobals37, dOpenDevice.zBufferNear) == 0x18 && offsetof(RwGlobals37, dOpenDevice.fpIm2DRenderIndexedPrimitive) == 0x34, "RwGlobals37 device fields");
static_assert(offsetof(RwGlobals37, stdFunc) == 0x48 && offsetof(RwGlobals37, stdFunc[RW37_STD_RASTERCREATE / 4 - 0x12]) == RW37_STD_RASTERCREATE, "RwGlobals37 stdFunc");
static_assert(offsetof(RwGlobals37, stringFuncs) == 0xc4 && offsetof(RwGlobals37, stringFuncs[RW37_STRLEN / 4 - 0x31]) == RW37_STRLEN, "RwGlobals37 stringFuncs");
static_assert(offsetof(RwGlobals37, memoryFuncs) == 0x108 && offsetof(RwGlobals37, freeListAlloc) == RW37_FREELISTALLOC && offsetof(RwGlobals37, freeListFree) == RW37_FREELISTFREE, "RwGlobals37 memory");
static_assert(sizeof(RwGlobals37) <= 0x140, "RwGlobals37 fits the capi/core.cpp shadow");

// RwCamera: fields the game / tier-2 C read by offset. frustumCorners[8] (V3d) 0x124..0x183: RwCameraCalcPixelRay 0x5698c0 reads 0x124..0x180
// (and frameBuffer 0x60 -> raster width 0xc / height 0x10); librw's Camera has them elsewhere (layout_check.cpp TODO rows: a flip blocker).
struct RwV3d37 { float x, y, z; };
struct RwCamera37 {
	uint8_t object[0x14];		// 0x00  RwObjectHasFrame
	int32_t projectionType;		// 0x14
	void *beginUpdate, *endUpdate;	// 0x18 0x1c
	float viewMatrix[16];		// 0x20
	RwRaster37 *frameBuffer, *zBuffer;	// 0x60 0x64
	float viewWindow[2];		// 0x68
	float recipViewWindow[2];	// 0x70
	float viewOffset[2];		// 0x78
	float nearPlane, farPlane, fogPlane;	// 0x80 0x84 0x88
	uint8_t pad8c[0x80];		// 0x8c  zScale / zShift / frustum planes (not read by the game)
	float frustumBoundBox[6];	// 0x10c
	RwV3d37 frustumCorners[8];	// 0x124
};
static_assert(offsetof(RwCamera37, frameBuffer) == 0x60 && offsetof(RwCamera37, viewWindow) == 0x68 && offsetof(RwCamera37, nearPlane) == 0x80, "RwCamera37");
static_assert(offsetof(RwCamera37, fogPlane) == 0x88 && offsetof(RwCamera37, frustumBoundBox) == 0x10c, "RwCamera37 fog / bbox");
static_assert(offsetof(RwCamera37, frustumCorners) == 0x124 && offsetof(RwCamera37, frustumCorners[4]) == 0x154 && sizeof(RwCamera37) == 0x184, "RwCamera37 frustumCorners");

// RpPTank atomic extension (the block behind *(atomic + __rpPTankAtomicDataOffset)): the 12 fields the game C writes by offset (audit rows PTankExt;
// the full 0x138 B table is src/rwptank.h ptank::Data, asserted in src/ptank.cpp). No game write hits the gaps.
struct RpPTankExt37 {
	int32_t maxCount;		// 0x00
	int32_t actPCount;		// 0x04  game writes the active count
	uint8_t pad08[0x38];
	uint32_t instFlags;		// 0x40  update flags (game ORs 0x800000 etc.)
	uint8_t pad44[0x60];
	int32_t srcBlend, dstBlend;	// 0xa4 0xa8
	int32_t vertexAlphaBlend;	// 0xac
	float cCenter[2];		// 0xb0
	float cSize[2];			// 0xb8 0xbc
	float cRotate;			// 0xc0
	uint8_t cColor[4];		// 0xc4
	uint8_t cVtxColor[4][4];	// 0xc8
	float cUV[8];			// 0xd8 (u0 0xd8, v0 0xdc, u1 0xe0, v1 0xe4, ...)
	float cMatrix[16];		// 0xf8  RwMatrix
};
static_assert(offsetof(RpPTankExt37, actPCount) == 0x04 && offsetof(RpPTankExt37, instFlags) == 0x40, "RpPTankExt37 counts");
static_assert(offsetof(RpPTankExt37, srcBlend) == 0xa4 && offsetof(RpPTankExt37, dstBlend) == 0xa8 && offsetof(RpPTankExt37, vertexAlphaBlend) == 0xac, "RpPTankExt37 blend");
static_assert(offsetof(RpPTankExt37, cSize) == 0xb8 && offsetof(RpPTankExt37, cSize[1]) == 0xbc, "RpPTankExt37 size");
static_assert(offsetof(RpPTankExt37, cUV) == 0xd8 && offsetof(RpPTankExt37, cUV[1]) == 0xdc && offsetof(RpPTankExt37, cUV[2]) == 0xe0 && offsetof(RpPTankExt37, cUV[3]) == 0xe4, "RpPTankExt37 uv");
static_assert(offsetof(RpPTankExt37, cMatrix) == 0xf8 && sizeof(RpPTankExt37) == 0x138, "RpPTankExt37 matrix / size");
#ifdef RW37_PLANT	// negative fixture (tools/layout_fixture.sh): one wrong row must fail the build
static_assert(offsetof(RwCamera37, frustumCorners) == 0x128, "Planted_rw37_frustumCorners");
#endif
