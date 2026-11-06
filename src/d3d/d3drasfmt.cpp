// _rwD3D9FindCorrectRasterFormat (MFL.exe 0x4e13f0), op for op from the disassembly; see d3drasfmt.h.  Verified against the original bytes
// under Unicorn: ghidra E3 fmtoracle.py (random caps profiles) and tests/rwreplay_f1.cpp (the Wine device through mflrw.dll).
#include "d3drasfmt.h"

namespace rw {
namespace d3d {

// bits per pixel by raster format nibble: the byte at +4 of the 8-byte entries at 0x5ea0f0 (entries 11..15 run into the next table; kept as read)
static const uint8_t fmtDepth[16] = { 0, 16, 16, 16, 8, 32, 32, 16, 32, 32, 16, 1, 0, 1, 0, 2 };

int32_t
findCorrectRasterFormat(int32_t type, int32_t flags, const RasFmtCaps *c)
{
#define TEX(i) ((c->tex >> (i)) & 1)
#define RTDS(i) (((c->rt >> (i)) & 1) && ((c->ds >> (i)) & 1))
	uint32_t f = (uint32_t)flags & 0xff00;
	uint32_t fmt;
	switch(type){
	case 0:		// rwRASTERTYPENORMAL
	case 4:		// rwRASTERTYPETEXTURE
		if((f & 0xf00) == 0){
			if(f & 0x2000){		// palettised, no colour format given
				if(TEX(RF_P8))
					f |= (c->textureCaps & 0x80) ? 0x500 : 0x600;
				else
					f &= ~0x2000u;
				if(f & 0x2000)
					return f;
			}
			if(TEX(RF_A8R8G8B8))
				return f | 0x500;
			goto try4444;
		}
		if(f & 0x4000)		// PAL4 is created as PAL8
			f = (f & ~0x4000u) | 0x2000;
		if(f & 0x2000){
			if(TEX(RF_P8)){
				if((f & 0xf00) != 0x500 || (c->textureCaps & 0x80))
					return f;
			}
			f &= ~0x2000u;
			if((f & 0xf00) == 0x600){
				if(c->displayDepth > 16 && TEX(RF_X8R8G8B8))
					return f | 0x600;
				goto to565;
			}
			if(c->displayDepth > 16){
				if(TEX(RF_A8R8G8B8))
					return f | 0x500;
				goto drop8888;
			}
			f &= ~0x500u;
			if(TEX(RF_A4R4G4B4))
				return f | 0x300;
			goto to1555;
		}
		fmt = f & 0xf00;
		if(fmt == 0x500){
			if(TEX(RF_A8R8G8B8))
				return f;
		drop8888:
			f &= ~0x500u;
		try4444:
			if(TEX(RF_A4R4G4B4))
				return f | 0x300;
			goto to1555;
		}
		if(fmt == 0x600){
			if(TEX(RF_X8R8G8B8))
				return f;
		to565:
			return (f & ~0x400u) | 0x200;
		}
		if(fmt == 0x300){
			if(TEX(RF_A4R4G4B4))
				return f;
			f &= ~0x200u;
		to1555:
			return f | 0x100;
		}
		if(fmt == 0x400){
			if(TEX(RF_L8))
				return f;
			f &= ~0x400u;
			if(TEX(RF_X8R8G8B8))
				return f | 0x16;	// sic: the original ors 0x16, not 0x600
			return f | 0x200;
		}
		return f;

	case 5:		// rwRASTERTYPECAMERATEXTURE
		fmt = f & 0xf00;
		if(fmt == 0){
			if(c->zDepth == 16)
				return f | 0x200;
			if(RTDS(RF_X8R8G8B8))
				return f | 0x600;
			return f | 0x200;
		}
		if(c->zDepth == 16){
			if(fmtDepth[fmt >> 8] == 32){
				if(fmt == 0x500) f = (f & ~0x400u) | 0x300;
				else if(fmt == 0x600) f = (f & ~0x400u) | 0x200;
				else f = 0x200;
			}
			if((f & 0xf00) == 0x300 && !RTDS(RF_A4R4G4B4))
				f = (f & ~0x200u) | 0x100;
			if((f & 0xf00) != 0x100 || RTDS(RF_A1R5G5B5))
				return f;
			return (f & ~0x100u) | 0x200;
		}
		if(fmtDepth[fmt >> 8] == 16){
			if(fmt == 0x300) f = (f & ~0x200u) | 0x500;
			else if(fmt == 0x100) f |= 0x500;
			else if(fmt == 0x200) f |= 0x600;
			else f = 0x600;
		}
		if((f & 0xf00) != 0x500 || RTDS(RF_A8R8G8B8))
			return f;
		return (f & ~0x100u) | 0x600;

	case 2:		// rwRASTERTYPECAMERA: the current video mode's format, only when none is given
		return (f & 0xf00) ? f : c->modeFormat;

	case 1:		// rwRASTERTYPEZBUFFER
		if(f & 0xf00)
			return f;
		if(c->zDepth == 16) return 0x700;
		if(c->zDepth == 32) return 0x900;
		return f;

	default:	// rwRASTERTYPE 3 and out of range: E_RW_INVRASTERFORMAT, flags unchanged
		return f;
	}
#undef TEX
#undef RTDS
}

}
}
