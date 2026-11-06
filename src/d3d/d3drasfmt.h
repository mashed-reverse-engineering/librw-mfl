// E3 (F1 rung): raster format choice of the RW 3.7 D3D9 driver, _rwD3D9FindCorrectRasterFormat (MFL.exe 0x4e13f0), as a pure function of the
// raster type, the requested flags and the device answers it asks for.  No D3D9 headers here: the native unit check
// (ghidra fmtoracle.py vectors) builds this file alone.
#ifndef D3DRASFMT_H
#define D3DRASFMT_H
#include <stdint.h>
namespace rw {
namespace d3d {

// bit i of tex / rt / ds = the device accepts RASFMT_D3D[i]: CheckDeviceFormat(usage 0, TEXTURE) / CheckDeviceFormat(usage RENDERTARGET, TEXTURE) /
// CheckDepthStencilMatch(render target = that format, depth = the device's depth-stencil format)
enum { RF_P8, RF_A8R8G8B8, RF_X8R8G8B8, RF_A4R4G4B4, RF_L8, RF_A1R5G5B5 };
struct RasFmtCaps
{
	uint32_t tex, rt, ds;
	int32_t displayDepth;	// 0x9475f4: bits per pixel of the adapter format
	int32_t zDepth;		// 0x9475fc: bits of the depth buffer
	uint8_t textureCaps;	// 0x94763c: low byte of D3DCAPS9.TextureCaps (bit 7 D3DPTEXTURECAPS_ALPHAPALETTE)
	uint16_t modeFormat;	// 0x947162 + 4 * mode: raster format of the current video mode (camera rasters)
};
int32_t findCorrectRasterFormat(int32_t type, int32_t flags, const RasFmtCaps *caps);

}
}
#endif
