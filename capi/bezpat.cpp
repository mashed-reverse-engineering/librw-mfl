// BEZPAT (D1c): leaf kernels of the RtBezPat toolkit and the RpPatch refiners of MFL.exe, written op for op from its disassembly like
// collis.cpp / splanim.cpp (X = a value the original keeps on the x87 stack, float = a value it stores; valid at the game's x87 CW 0x7f).
//   RtBezierQuadBernsteinWeight3d 0x55db00, RtBezierQuadBernsteinWeight4d 0x55e200, RtBezierQuadOriginDifference3d 0x55eb50,
//   RtBezierQuadOriginDifference4d 0x55f1c0 (straight-line code: tools/x87/lift.py output, offsets kept), _rpPatchRefineQuadVector 0x55cb20,
//   _rpPatchRefineQuadColor 0x55cea0, _rpPatchRefineQuadTexCoords 0x55d4e0, _rpPatchGenerateQuadIndex 0x55d6c0.
// Objects: RtBezierMatrix = 4 x 4 RtBezierV4d {x, y, z, w} (256 bytes; the 3d kernels leave w alone); the refiners write (n+1)^2 vertices
// (RwV3d / RwRGBA / RwTexCoords) at a byte stride, row by row, by forward differencing of the origin difference matrix with step 1/n;
// GenerateQuadIndex writes the n strips of 2(n+1) 16-bit indices, every second strip reversed.
// No exe state, no adapter: the kernels only touch their arguments.
// Check: tools/delink/rw_bezpat_check.sh (the original code and these exports natively under Wine on the same random inputs, outputs and the
// untouched bytes around them compared word for word).
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// field access with may_alias types (integer moves and float reads of the same words)
typedef float __attribute__((may_alias)) FA;
typedef u32 __attribute__((may_alias)) UA;
#define F(p, off)  (*(FA *)((u8 *)(p) + (off)))
#define U(p, off)  (*(UA *)((u8 *)(p) + (off)))

// fistp with the rounding mode set to chop around it, as the original does (fnstcw / or ah,0xc / fldcw / fistp / fldcw)
static inline int32_t fistpChop(X v){
	int32_t r; u16 cw, t;
	__asm__ volatile("fnstcw %1\n\tmovw %1, %%ax\n\torb $0xc, %%ah\n\tmovw %%ax, %2\n\tfldcw %2\n\tfistpl %0\n\tfldcw %1"
		: "=m"(r), "=m"(cw), "=m"(t) : "t"(v) : "st", "eax");
	return r;
}

extern "C" {

void __cdecl rw37_RtBezierQuadBernsteinWeight3d(u8 *a1, const u8 *a2)
{
  U(a1, 0xf0) = U(a2, 0);
  U(a1, 0xf4) = U(a2, 0x4);
  U(a1, 0xf8) = U(a2, 0x8);
  F(a1, 0xe0) = (F(a2, 0x10) - F(a2, 0)) * 3.0f;
  F(a1, 0xe4) = (F(a2, 0x14) - F(a2, 0x4)) * 3.0f;
  F(a1, 0xe8) = (F(a2, 0x18) - F(a2, 0x8)) * 3.0f;
  F(a1, 0xd0) = (F(a2, 0x20) - F(a2, 0x10)) * 3.0f;
  F(a1, 0xd4) = (F(a2, 0x24) - F(a2, 0x14)) * 3.0f;
  F(a1, 0xd8) = (F(a2, 0x28) - F(a2, 0x18)) * 3.0f;
  F(a1, 0xc0) = F(a2, 0x30) - F(a2, 0) - F(a1, 0xd0);
  F(a1, 0xc4) = F(a2, 0x34) - F(a2, 0x4) - F(a1, 0xd4);
  F(a1, 0xc8) = F(a2, 0x38) - F(a2, 0x8) - F(a1, 0xd8);
  F(a1, 0xd0) = F(a1, 0xd0) - F(a1, 0xe0);
  F(a1, 0xd4) = F(a1, 0xd4) - F(a1, 0xe4);
  F(a1, 0xd8) = F(a1, 0xd8) - F(a1, 0xe8);
  U(a1, 0xb0) = U(a2, 0x40);
  U(a1, 0xb4) = U(a2, 0x44);
  U(a1, 0xb8) = U(a2, 0x48);
  F(a1, 0xa0) = (F(a2, 0x50) - F(a2, 0x40)) * 3.0f;
  F(a1, 0xa4) = (F(a2, 0x54) - F(a2, 0x44)) * 3.0f;
  F(a1, 0xa8) = (F(a2, 0x58) - F(a2, 0x48)) * 3.0f;
  F(a1, 0x90) = (F(a2, 0x60) - F(a2, 0x50)) * 3.0f;
  F(a1, 0x94) = (F(a2, 0x64) - F(a2, 0x54)) * 3.0f;
  F(a1, 0x98) = (F(a2, 0x68) - F(a2, 0x58)) * 3.0f;
  F(a1, 0x80) = F(a2, 0x70) - F(a2, 0x40) - F(a1, 0x90);
  F(a1, 0x84) = F(a2, 0x74) - F(a2, 0x44) - F(a1, 0x94);
  F(a1, 0x88) = F(a2, 0x78) - F(a2, 0x48) - F(a1, 0x98);
  F(a1, 0x90) = F(a1, 0x90) - F(a1, 0xa0);
  F(a1, 0x94) = F(a1, 0x94) - F(a1, 0xa4);
  F(a1, 0x98) = F(a1, 0x98) - F(a1, 0xa8);
  U(a1, 0x70) = U(a2, 0x80);
  U(a1, 0x74) = U(a2, 0x84);
  U(a1, 0x78) = U(a2, 0x88);
  F(a1, 0x60) = (F(a2, 0x90) - F(a2, 0x80)) * 3.0f;
  F(a1, 0x64) = (F(a2, 0x94) - F(a2, 0x84)) * 3.0f;
  F(a1, 0x68) = (F(a2, 0x98) - F(a2, 0x88)) * 3.0f;
  F(a1, 0x50) = (F(a2, 0xa0) - F(a2, 0x90)) * 3.0f;
  F(a1, 0x54) = (F(a2, 0xa4) - F(a2, 0x94)) * 3.0f;
  F(a1, 0x58) = (F(a2, 0xa8) - F(a2, 0x98)) * 3.0f;
  F(a1, 0x40) = F(a2, 0xb0) - F(a2, 0x80) - F(a1, 0x50);
  F(a1, 0x44) = F(a2, 0xb4) - F(a2, 0x84) - F(a1, 0x54);
  F(a1, 0x48) = F(a2, 0xb8) - F(a2, 0x88) - F(a1, 0x58);
  F(a1, 0x50) = F(a1, 0x50) - F(a1, 0x60);
  F(a1, 0x54) = F(a1, 0x54) - F(a1, 0x64);
  F(a1, 0x58) = F(a1, 0x58) - F(a1, 0x68);
  U(a1, 0x30) = U(a2, 0xc0);
  U(a1, 0x34) = U(a2, 0xc4);
  U(a1, 0x38) = U(a2, 0xc8);
  F(a1, 0x20) = (F(a2, 0xd0) - F(a2, 0xc0)) * 3.0f;
  F(a1, 0x24) = (F(a2, 0xd4) - F(a2, 0xc4)) * 3.0f;
  F(a1, 0x28) = (F(a2, 0xd8) - F(a2, 0xc8)) * 3.0f;
  F(a1, 0x10) = (F(a2, 0xe0) - F(a2, 0xd0)) * 3.0f;
  F(a1, 0x14) = (F(a2, 0xe4) - F(a2, 0xd4)) * 3.0f;
  F(a1, 0x18) = (F(a2, 0xe8) - F(a2, 0xd8)) * 3.0f;
  F(a1, 0) = F(a2, 0xf0) - F(a2, 0xc0) - F(a1, 0x10);
  F(a1, 0x4) = F(a2, 0xf4) - F(a2, 0xc4) - F(a1, 0x14);
  F(a1, 0x8) = F(a2, 0xf8) - F(a2, 0xc8) - F(a1, 0x18);
  F(a1, 0x10) = F(a1, 0x10) - F(a1, 0x20);
  F(a1, 0x14) = F(a1, 0x14) - F(a1, 0x24);
  F(a1, 0x18) = F(a1, 0x18) - F(a1, 0x28);
  F(a1, 0x40) = (F(a1, 0x40) - F(a1, 0x80)) * 3.0f;
  F(a1, 0x44) = (F(a1, 0x44) - F(a1, 0x84)) * 3.0f;
  F(a1, 0x48) = (F(a1, 0x48) - F(a1, 0x88)) * 3.0f;
  F(a1, 0x50) = (F(a1, 0x50) - F(a1, 0x90)) * 3.0f;
  F(a1, 0x54) = (F(a1, 0x54) - F(a1, 0x94)) * 3.0f;
  F(a1, 0x58) = (F(a1, 0x58) - F(a1, 0x98)) * 3.0f;
  F(a1, 0x60) = (F(a1, 0x60) - F(a1, 0xa0)) * 3.0f;
  F(a1, 0x64) = (F(a1, 0x64) - F(a1, 0xa4)) * 3.0f;
  F(a1, 0x68) = (F(a1, 0x68) - F(a1, 0xa8)) * 3.0f;
  F(a1, 0x70) = (F(a1, 0x70) - F(a1, 0xb0)) * 3.0f;
  F(a1, 0x74) = (F(a1, 0x74) - F(a1, 0xb4)) * 3.0f;
  F(a1, 0x78) = (F(a1, 0x78) - F(a1, 0xb8)) * 3.0f;
  F(a1, 0x80) = (F(a1, 0x80) - F(a1, 0xc0)) * 3.0f;
  F(a1, 0x84) = (F(a1, 0x84) - F(a1, 0xc4)) * 3.0f;
  F(a1, 0x88) = (F(a1, 0x88) - F(a1, 0xc8)) * 3.0f;
  F(a1, 0x90) = (F(a1, 0x90) - F(a1, 0xd0)) * 3.0f;
  F(a1, 0x94) = (F(a1, 0x94) - F(a1, 0xd4)) * 3.0f;
  F(a1, 0x98) = (F(a1, 0x98) - F(a1, 0xd8)) * 3.0f;
  F(a1, 0xa0) = (F(a1, 0xa0) - F(a1, 0xe0)) * 3.0f;
  F(a1, 0xa4) = (F(a1, 0xa4) - F(a1, 0xe4)) * 3.0f;
  F(a1, 0xa8) = (F(a1, 0xa8) - F(a1, 0xe8)) * 3.0f;
  F(a1, 0xb0) = (F(a1, 0xb0) - F(a1, 0xf0)) * 3.0f;
  F(a1, 0xb4) = (F(a1, 0xb4) - F(a1, 0xf4)) * 3.0f;
  F(a1, 0xb8) = (F(a1, 0xb8) - F(a1, 0xf8)) * 3.0f;
  F(a1, 0) = F(a1, 0) - F(a1, 0xc0) - F(a1, 0x40);
  F(a1, 0x4) = F(a1, 0x4) - F(a1, 0xc4) - F(a1, 0x44);
  F(a1, 0x8) = F(a1, 0x8) - F(a1, 0xc8) - F(a1, 0x48);
  F(a1, 0x10) = F(a1, 0x10) - F(a1, 0xd0) - F(a1, 0x50);
  F(a1, 0x14) = F(a1, 0x14) - F(a1, 0xd4) - F(a1, 0x54);
  F(a1, 0x18) = F(a1, 0x18) - F(a1, 0xd8) - F(a1, 0x58);
  F(a1, 0x20) = F(a1, 0x20) - F(a1, 0xe0) - F(a1, 0x60);
  F(a1, 0x24) = F(a1, 0x24) - F(a1, 0xe4) - F(a1, 0x64);
  F(a1, 0x28) = F(a1, 0x28) - F(a1, 0xe8) - F(a1, 0x68);
  F(a1, 0x30) = F(a1, 0x30) - F(a1, 0xf0) - F(a1, 0x70);
  F(a1, 0x34) = F(a1, 0x34) - F(a1, 0xf4) - F(a1, 0x74);
  F(a1, 0x38) = F(a1, 0x38) - F(a1, 0xf8) - F(a1, 0x78);
  F(a1, 0x40) = F(a1, 0x40) - F(a1, 0x80);
  F(a1, 0x44) = F(a1, 0x44) - F(a1, 0x84);
  F(a1, 0x48) = F(a1, 0x48) - F(a1, 0x88);
  F(a1, 0x50) = F(a1, 0x50) - F(a1, 0x90);
  F(a1, 0x54) = F(a1, 0x54) - F(a1, 0x94);
  F(a1, 0x58) = F(a1, 0x58) - F(a1, 0x98);
  F(a1, 0x60) = F(a1, 0x60) - F(a1, 0xa0);
  F(a1, 0x64) = F(a1, 0x64) - F(a1, 0xa4);
  F(a1, 0x68) = F(a1, 0x68) - F(a1, 0xa8);
  F(a1, 0x70) = F(a1, 0x70) - F(a1, 0xb0);
  F(a1, 0x74) = F(a1, 0x74) - F(a1, 0xb4);
  F(a1, 0x78) = F(a1, 0x78) - F(a1, 0xb8);
}

void __cdecl rw37_RtBezierQuadBernsteinWeight4d(u8 *a1, const u8 *a2)
{
  U(a1, 0xf0) = U(a2, 0);
  U(a1, 0xf4) = U(a2, 0x4);
  U(a1, 0xf8) = U(a2, 0x8);
  U(a1, 0xfc) = U(a2, 0xc);
  F(a1, 0xe0) = (F(a2, 0x10) - F(a2, 0)) * 3.0f;
  F(a1, 0xe4) = (F(a2, 0x14) - F(a2, 0x4)) * 3.0f;
  F(a1, 0xe8) = (F(a2, 0x18) - F(a2, 0x8)) * 3.0f;
  F(a1, 0xec) = (F(a2, 0x1c) - F(a2, 0xc)) * 3.0f;
  F(a1, 0xd0) = (F(a2, 0x20) - F(a2, 0x10)) * 3.0f;
  F(a1, 0xd4) = (F(a2, 0x24) - F(a2, 0x14)) * 3.0f;
  F(a1, 0xd8) = (F(a2, 0x28) - F(a2, 0x18)) * 3.0f;
  F(a1, 0xdc) = (F(a2, 0x2c) - F(a2, 0x1c)) * 3.0f;
  F(a1, 0xc0) = F(a2, 0x30) - F(a2, 0) - F(a1, 0xd0);
  F(a1, 0xc4) = F(a2, 0x34) - F(a2, 0x4) - F(a1, 0xd4);
  F(a1, 0xc8) = F(a2, 0x38) - F(a2, 0x8) - F(a1, 0xd8);
  F(a1, 0xcc) = F(a2, 0x3c) - F(a2, 0xc) - F(a1, 0xdc);
  F(a1, 0xd0) = F(a1, 0xd0) - F(a1, 0xe0);
  F(a1, 0xd4) = F(a1, 0xd4) - F(a1, 0xe4);
  F(a1, 0xd8) = F(a1, 0xd8) - F(a1, 0xe8);
  F(a1, 0xdc) = F(a1, 0xdc) - F(a1, 0xec);
  U(a1, 0xb0) = U(a2, 0x40);
  U(a1, 0xb4) = U(a2, 0x44);
  U(a1, 0xb8) = U(a2, 0x48);
  U(a1, 0xbc) = U(a2, 0x4c);
  F(a1, 0xa0) = (F(a2, 0x50) - F(a2, 0x40)) * 3.0f;
  F(a1, 0xa4) = (F(a2, 0x54) - F(a2, 0x44)) * 3.0f;
  F(a1, 0xa8) = (F(a2, 0x58) - F(a2, 0x48)) * 3.0f;
  F(a1, 0xac) = (F(a2, 0x5c) - F(a2, 0x4c)) * 3.0f;
  F(a1, 0x90) = (F(a2, 0x60) - F(a2, 0x50)) * 3.0f;
  F(a1, 0x94) = (F(a2, 0x64) - F(a2, 0x54)) * 3.0f;
  F(a1, 0x98) = (F(a2, 0x68) - F(a2, 0x58)) * 3.0f;
  F(a1, 0x9c) = (F(a2, 0x6c) - F(a2, 0x5c)) * 3.0f;
  F(a1, 0x80) = F(a2, 0x70) - F(a2, 0x40) - F(a1, 0x90);
  F(a1, 0x84) = F(a2, 0x74) - F(a2, 0x44) - F(a1, 0x94);
  F(a1, 0x88) = F(a2, 0x78) - F(a2, 0x48) - F(a1, 0x98);
  F(a1, 0x8c) = F(a2, 0x7c) - F(a2, 0x4c) - F(a1, 0x9c);
  F(a1, 0x90) = F(a1, 0x90) - F(a1, 0xa0);
  F(a1, 0x94) = F(a1, 0x94) - F(a1, 0xa4);
  F(a1, 0x98) = F(a1, 0x98) - F(a1, 0xa8);
  F(a1, 0x9c) = F(a1, 0x9c) - F(a1, 0xac);
  U(a1, 0x70) = U(a2, 0x80);
  U(a1, 0x74) = U(a2, 0x84);
  U(a1, 0x78) = U(a2, 0x88);
  U(a1, 0x7c) = U(a2, 0x8c);
  F(a1, 0x60) = (F(a2, 0x90) - F(a2, 0x80)) * 3.0f;
  F(a1, 0x64) = (F(a2, 0x94) - F(a2, 0x84)) * 3.0f;
  F(a1, 0x68) = (F(a2, 0x98) - F(a2, 0x88)) * 3.0f;
  F(a1, 0x6c) = (F(a2, 0x9c) - F(a2, 0x8c)) * 3.0f;
  F(a1, 0x50) = (F(a2, 0xa0) - F(a2, 0x90)) * 3.0f;
  F(a1, 0x54) = (F(a2, 0xa4) - F(a2, 0x94)) * 3.0f;
  F(a1, 0x58) = (F(a2, 0xa8) - F(a2, 0x98)) * 3.0f;
  F(a1, 0x5c) = (F(a2, 0xac) - F(a2, 0x9c)) * 3.0f;
  F(a1, 0x40) = F(a2, 0xb0) - F(a2, 0x80) - F(a1, 0x50);
  F(a1, 0x44) = F(a2, 0xb4) - F(a2, 0x84) - F(a1, 0x54);
  F(a1, 0x48) = F(a2, 0xb8) - F(a2, 0x88) - F(a1, 0x58);
  F(a1, 0x4c) = F(a2, 0xbc) - F(a2, 0x8c) - F(a1, 0x5c);
  F(a1, 0x50) = F(a1, 0x50) - F(a1, 0x60);
  F(a1, 0x54) = F(a1, 0x54) - F(a1, 0x64);
  F(a1, 0x58) = F(a1, 0x58) - F(a1, 0x68);
  F(a1, 0x5c) = F(a1, 0x5c) - F(a1, 0x6c);
  U(a1, 0x30) = U(a2, 0xc0);
  U(a1, 0x34) = U(a2, 0xc4);
  U(a1, 0x38) = U(a2, 0xc8);
  U(a1, 0x3c) = U(a2, 0xcc);
  F(a1, 0x20) = (F(a2, 0xd0) - F(a2, 0xc0)) * 3.0f;
  F(a1, 0x24) = (F(a2, 0xd4) - F(a2, 0xc4)) * 3.0f;
  F(a1, 0x28) = (F(a2, 0xd8) - F(a2, 0xc8)) * 3.0f;
  F(a1, 0x2c) = (F(a2, 0xdc) - F(a2, 0xcc)) * 3.0f;
  F(a1, 0x10) = (F(a2, 0xe0) - F(a2, 0xd0)) * 3.0f;
  F(a1, 0x14) = (F(a2, 0xe4) - F(a2, 0xd4)) * 3.0f;
  F(a1, 0x18) = (F(a2, 0xe8) - F(a2, 0xd8)) * 3.0f;
  F(a1, 0x1c) = (F(a2, 0xec) - F(a2, 0xdc)) * 3.0f;
  F(a1, 0) = F(a2, 0xf0) - F(a2, 0xc0) - F(a1, 0x10);
  F(a1, 0x4) = F(a2, 0xf4) - F(a2, 0xc4) - F(a1, 0x14);
  F(a1, 0x8) = F(a2, 0xf8) - F(a2, 0xc8) - F(a1, 0x18);
  F(a1, 0xc) = F(a2, 0xfc) - F(a2, 0xcc) - F(a1, 0x1c);
  F(a1, 0x10) = F(a1, 0x10) - F(a1, 0x20);
  F(a1, 0x14) = F(a1, 0x14) - F(a1, 0x24);
  F(a1, 0x18) = F(a1, 0x18) - F(a1, 0x28);
  F(a1, 0x1c) = F(a1, 0x1c) - F(a1, 0x2c);
  F(a1, 0x40) = (F(a1, 0x40) - F(a1, 0x80)) * 3.0f;
  F(a1, 0x44) = (F(a1, 0x44) - F(a1, 0x84)) * 3.0f;
  F(a1, 0x48) = (F(a1, 0x48) - F(a1, 0x88)) * 3.0f;
  F(a1, 0x4c) = (F(a1, 0x4c) - F(a1, 0x8c)) * 3.0f;
  F(a1, 0x50) = (F(a1, 0x50) - F(a1, 0x90)) * 3.0f;
  F(a1, 0x54) = (F(a1, 0x54) - F(a1, 0x94)) * 3.0f;
  F(a1, 0x58) = (F(a1, 0x58) - F(a1, 0x98)) * 3.0f;
  F(a1, 0x5c) = (F(a1, 0x5c) - F(a1, 0x9c)) * 3.0f;
  F(a1, 0x60) = (F(a1, 0x60) - F(a1, 0xa0)) * 3.0f;
  F(a1, 0x64) = (F(a1, 0x64) - F(a1, 0xa4)) * 3.0f;
  F(a1, 0x68) = (F(a1, 0x68) - F(a1, 0xa8)) * 3.0f;
  F(a1, 0x6c) = (F(a1, 0x6c) - F(a1, 0xac)) * 3.0f;
  F(a1, 0x70) = (F(a1, 0x70) - F(a1, 0xb0)) * 3.0f;
  F(a1, 0x74) = (F(a1, 0x74) - F(a1, 0xb4)) * 3.0f;
  F(a1, 0x78) = (F(a1, 0x78) - F(a1, 0xb8)) * 3.0f;
  F(a1, 0x7c) = (F(a1, 0x7c) - F(a1, 0xbc)) * 3.0f;
  F(a1, 0x80) = (F(a1, 0x80) - F(a1, 0xc0)) * 3.0f;
  F(a1, 0x84) = (F(a1, 0x84) - F(a1, 0xc4)) * 3.0f;
  F(a1, 0x88) = (F(a1, 0x88) - F(a1, 0xc8)) * 3.0f;
  F(a1, 0x8c) = (F(a1, 0x8c) - F(a1, 0xcc)) * 3.0f;
  F(a1, 0x90) = (F(a1, 0x90) - F(a1, 0xd0)) * 3.0f;
  F(a1, 0x94) = (F(a1, 0x94) - F(a1, 0xd4)) * 3.0f;
  F(a1, 0x98) = (F(a1, 0x98) - F(a1, 0xd8)) * 3.0f;
  F(a1, 0x9c) = (F(a1, 0x9c) - F(a1, 0xdc)) * 3.0f;
  F(a1, 0xa0) = (F(a1, 0xa0) - F(a1, 0xe0)) * 3.0f;
  F(a1, 0xa4) = (F(a1, 0xa4) - F(a1, 0xe4)) * 3.0f;
  F(a1, 0xa8) = (F(a1, 0xa8) - F(a1, 0xe8)) * 3.0f;
  F(a1, 0xac) = (F(a1, 0xac) - F(a1, 0xec)) * 3.0f;
  F(a1, 0xb0) = (F(a1, 0xb0) - F(a1, 0xf0)) * 3.0f;
  F(a1, 0xb4) = (F(a1, 0xb4) - F(a1, 0xf4)) * 3.0f;
  F(a1, 0xb8) = (F(a1, 0xb8) - F(a1, 0xf8)) * 3.0f;
  F(a1, 0xbc) = (F(a1, 0xbc) - F(a1, 0xfc)) * 3.0f;
  F(a1, 0) = F(a1, 0) - F(a1, 0xc0) - F(a1, 0x40);
  F(a1, 0x4) = F(a1, 0x4) - F(a1, 0xc4) - F(a1, 0x44);
  F(a1, 0x8) = F(a1, 0x8) - F(a1, 0xc8) - F(a1, 0x48);
  F(a1, 0xc) = F(a1, 0xc) - F(a1, 0xcc) - F(a1, 0x4c);
  F(a1, 0x10) = F(a1, 0x10) - F(a1, 0xd0) - F(a1, 0x50);
  F(a1, 0x14) = F(a1, 0x14) - F(a1, 0xd4) - F(a1, 0x54);
  F(a1, 0x18) = F(a1, 0x18) - F(a1, 0xd8) - F(a1, 0x58);
  F(a1, 0x1c) = F(a1, 0x1c) - F(a1, 0xdc) - F(a1, 0x5c);
  F(a1, 0x20) = F(a1, 0x20) - F(a1, 0xe0) - F(a1, 0x60);
  F(a1, 0x24) = F(a1, 0x24) - F(a1, 0xe4) - F(a1, 0x64);
  F(a1, 0x28) = F(a1, 0x28) - F(a1, 0xe8) - F(a1, 0x68);
  F(a1, 0x2c) = F(a1, 0x2c) - F(a1, 0xec) - F(a1, 0x6c);
  F(a1, 0x30) = F(a1, 0x30) - F(a1, 0xf0) - F(a1, 0x70);
  F(a1, 0x34) = F(a1, 0x34) - F(a1, 0xf4) - F(a1, 0x74);
  F(a1, 0x38) = F(a1, 0x38) - F(a1, 0xf8) - F(a1, 0x78);
  F(a1, 0x3c) = F(a1, 0x3c) - F(a1, 0xfc) - F(a1, 0x7c);
  F(a1, 0x40) = F(a1, 0x40) - F(a1, 0x80);
  F(a1, 0x44) = F(a1, 0x44) - F(a1, 0x84);
  F(a1, 0x48) = F(a1, 0x48) - F(a1, 0x88);
  F(a1, 0x4c) = F(a1, 0x4c) - F(a1, 0x8c);
  F(a1, 0x50) = F(a1, 0x50) - F(a1, 0x90);
  F(a1, 0x54) = F(a1, 0x54) - F(a1, 0x94);
  F(a1, 0x58) = F(a1, 0x58) - F(a1, 0x98);
  F(a1, 0x5c) = F(a1, 0x5c) - F(a1, 0x9c);
  F(a1, 0x60) = F(a1, 0x60) - F(a1, 0xa0);
  F(a1, 0x64) = F(a1, 0x64) - F(a1, 0xa4);
  F(a1, 0x68) = F(a1, 0x68) - F(a1, 0xa8);
  F(a1, 0x6c) = F(a1, 0x6c) - F(a1, 0xac);
  F(a1, 0x70) = F(a1, 0x70) - F(a1, 0xb0);
  F(a1, 0x74) = F(a1, 0x74) - F(a1, 0xb4);
  F(a1, 0x78) = F(a1, 0x78) - F(a1, 0xb8);
  F(a1, 0x7c) = F(a1, 0x7c) - F(a1, 0xbc);
}

void __cdecl rw37_RtBezierQuadOriginDifference3d(u8 *a1, const u8 *a2, float a3, float a4)
{
  float fv[6];
  X f1;
  X f2;
  X f3;
  X f4;
  X f5;
  X f6;
  X f7;

  f1 = a3 * a3;
  f2 = a3 * f1;
  f3 = a3 * a4;
  f4 = f1 * a4;
  f5 = f2 * a4;
  f6 = a4 * a4;
  f7 = a3 * f6;
  fv[5] = f6 * f1;
  fv[4] = f6 * f2;
  fv[0] = f6 * a4;
  fv[3] = f6 * a4 * a3;
  fv[2] = fv[0] * f1;
  fv[1] = fv[0] * f2;
  F(a1, 0x40) = fv[0] * F(a2, 0xc0);
  F(a1, 0x44) = fv[0] * F(a2, 0xc4);
  F(a1, 0x48) = fv[0] * F(a2, 0xc8);
  F(a1, 0x50) = fv[1] * F(a2, 0);
  F(a1, 0x54) = fv[1] * F(a2, 0x4);
  F(a1, 0x58) = fv[1] * F(a2, 0x8);
  F(a1, 0x60) = fv[2] * F(a2, 0x40);
  F(a1, 0x64) = fv[2] * F(a2, 0x44);
  F(a1, 0x68) = fv[2] * F(a2, 0x48);
  F(a1, 0x70) = F(a1, 0x50) * 6.0f;
  F(a1, 0x74) = F(a1, 0x54) * 6.0f;
  F(a1, 0x78) = F(a1, 0x58) * 6.0f;
  F(a1, 0x50) = fv[3] * F(a2, 0x80) + F(a1, 0x50) + F(a1, 0x60);
  F(a1, 0x54) = fv[3] * F(a2, 0x84) + F(a1, 0x64) + F(a1, 0x54);
  F(a1, 0x58) = fv[3] * F(a2, 0x88) + F(a1, 0x58) + F(a1, 0x68);
  F(a1, 0x60) = F(a1, 0x60) + F(a1, 0x60) + F(a1, 0x70);
  F(a1, 0x64) = F(a1, 0x64) + F(a1, 0x64) + F(a1, 0x74);
  F(a1, 0x68) = F(a1, 0x68) + F(a1, 0x68) + F(a1, 0x78);
  F(a1, 0x80) = f6 * F(a2, 0xd0);
  F(a1, 0x84) = f6 * F(a2, 0xd4);
  F(a1, 0x88) = f6 * F(a2, 0xd8);
  F(a1, 0x90) = fv[4] * F(a2, 0x10);
  F(a1, 0x94) = fv[4] * F(a2, 0x14);
  F(a1, 0x98) = fv[4] * F(a2, 0x18);
  F(a1, 0xa0) = fv[5] * F(a2, 0x50);
  F(a1, 0xa4) = fv[5] * F(a2, 0x54);
  F(a1, 0xa8) = fv[5] * F(a2, 0x58);
  F(a1, 0xb0) = F(a1, 0x90) * 6.0f;
  F(a1, 0xb4) = F(a1, 0x94) * 6.0f;
  F(a1, 0xb8) = F(a1, 0x98) * 6.0f;
  F(a1, 0x90) = f7 * F(a2, 0x90) + F(a1, 0xa0) + F(a1, 0x90);
  F(a1, 0x94) = f7 * F(a2, 0x94) + F(a1, 0x94) + F(a1, 0xa4);
  F(a1, 0x98) = f7 * F(a2, 0x98) + F(a1, 0xa8) + F(a1, 0x98);
  F(a1, 0xa0) = F(a1, 0xa0) + F(a1, 0xa0) + F(a1, 0xb0);
  F(a1, 0xa4) = F(a1, 0xa4) + F(a1, 0xa4) + F(a1, 0xb4);
  F(a1, 0xa8) = F(a1, 0xa8) + F(a1, 0xa8) + F(a1, 0xb8);
  F(a1, 0xc0) = F(a1, 0x40) * 6.0f;
  F(a1, 0xc4) = F(a1, 0x44) * 6.0f;
  F(a1, 0xc8) = F(a1, 0x48) * 6.0f;
  F(a1, 0xd0) = F(a1, 0x50) * 6.0f;
  F(a1, 0xd4) = F(a1, 0x54) * 6.0f;
  F(a1, 0xd8) = F(a1, 0x58) * 6.0f;
  F(a1, 0xe0) = F(a1, 0x60) * 6.0f;
  F(a1, 0xe4) = F(a1, 0x64) * 6.0f;
  F(a1, 0xe8) = F(a1, 0x68) * 6.0f;
  F(a1, 0xf0) = F(a1, 0x70) * 6.0f;
  F(a1, 0xf4) = F(a1, 0x74) * 6.0f;
  F(a1, 0xf8) = F(a1, 0x78) * 6.0f;
  F(a1, 0) = a4 * F(a2, 0xe0);
  F(a1, 0x4) = a4 * F(a2, 0xe4);
  F(a1, 0x8) = a4 * F(a2, 0xe8);
  F(a1, 0x10) = f5 * F(a2, 0x20);
  F(a1, 0x14) = f5 * F(a2, 0x24);
  F(a1, 0x18) = f5 * F(a2, 0x28);
  F(a1, 0x20) = f4 * F(a2, 0x60);
  F(a1, 0x24) = f4 * F(a2, 0x64);
  F(a1, 0x28) = f4 * F(a2, 0x68);
  F(a1, 0x30) = F(a1, 0x10) * 6.0f;
  F(a1, 0x34) = F(a1, 0x14) * 6.0f;
  F(a1, 0x38) = F(a1, 0x18) * 6.0f;
  F(a1, 0x10) = f3 * F(a2, 0xa0) + F(a1, 0x20) + F(a1, 0x10);
  F(a1, 0x14) = f3 * F(a2, 0xa4) + F(a1, 0x24) + F(a1, 0x14);
  F(a1, 0x18) = f3 * F(a2, 0xa8) + F(a1, 0x28) + F(a1, 0x18);
  F(a1, 0x20) = F(a1, 0x20) + F(a1, 0x20) + F(a1, 0x30);
  F(a1, 0x24) = F(a1, 0x24) + F(a1, 0x24) + F(a1, 0x34);
  F(a1, 0x28) = F(a1, 0x28) + F(a1, 0x28) + F(a1, 0x38);
  F(a1, 0x40) = F(a1, 0x80) + F(a1, 0x40) + F(a1, 0);
  F(a1, 0x44) = F(a1, 0x84) + F(a1, 0x44) + F(a1, 0x4);
  F(a1, 0x48) = F(a1, 0x88) + F(a1, 0x48) + F(a1, 0x8);
  F(a1, 0x50) = F(a1, 0x50) + F(a1, 0x10) + F(a1, 0x90);
  F(a1, 0x54) = F(a1, 0x94) + F(a1, 0x54) + F(a1, 0x14);
  F(a1, 0x58) = F(a1, 0x58) + F(a1, 0x18) + F(a1, 0x98);
  F(a1, 0x60) = F(a1, 0x20) + F(a1, 0xa0) + F(a1, 0x60);
  F(a1, 0x64) = F(a1, 0x24) + F(a1, 0x64) + F(a1, 0xa4);
  F(a1, 0x68) = F(a1, 0x28) + F(a1, 0xa8) + F(a1, 0x68);
  F(a1, 0x70) = F(a1, 0x30) + F(a1, 0xb0) + F(a1, 0x70);
  F(a1, 0x74) = F(a1, 0x34) + F(a1, 0xb4) + F(a1, 0x74);
  F(a1, 0x78) = F(a1, 0x38) + F(a1, 0xb8) + F(a1, 0x78);
  F(a1, 0x80) = F(a1, 0x80) + F(a1, 0x80) + F(a1, 0xc0);
  F(a1, 0x84) = F(a1, 0x84) + F(a1, 0x84) + F(a1, 0xc4);
  F(a1, 0x88) = F(a1, 0x88) + F(a1, 0x88) + F(a1, 0xc8);
  F(a1, 0x90) = F(a1, 0x90) + F(a1, 0x90) + F(a1, 0xd0);
  F(a1, 0x94) = F(a1, 0x94) + F(a1, 0x94) + F(a1, 0xd4);
  F(a1, 0x98) = F(a1, 0x98) + F(a1, 0x98) + F(a1, 0xd8);
  F(a1, 0xa0) = F(a1, 0xa0) + F(a1, 0xa0) + F(a1, 0xe0);
  F(a1, 0xa4) = F(a1, 0xa4) + F(a1, 0xa4) + F(a1, 0xe4);
  F(a1, 0xa8) = F(a1, 0xa8) + F(a1, 0xa8) + F(a1, 0xe8);
  F(a1, 0xb0) = F(a1, 0xb0) + F(a1, 0xb0) + F(a1, 0xf0);
  F(a1, 0xb4) = F(a1, 0xb4) + F(a1, 0xb4) + F(a1, 0xf4);
  F(a1, 0xb8) = F(a1, 0xb8) + F(a1, 0xb8) + F(a1, 0xf8);
  U(a1, 0) = U(a2, 0xf0);
  U(a1, 0x4) = U(a2, 0xf4);
  U(a1, 0x8) = U(a2, 0xf8);
  F(a1, 0x10) = f2 * F(a2, 0x30);
  F(a1, 0x14) = f2 * F(a2, 0x34);
  F(a1, 0x18) = f2 * F(a2, 0x38);
  F(a1, 0x20) = f1 * F(a2, 0x70);
  F(a1, 0x24) = f1 * F(a2, 0x74);
  F(a1, 0x28) = f1 * F(a2, 0x78);
  F(a1, 0x30) = F(a1, 0x10) * 6.0f;
  F(a1, 0x34) = F(a1, 0x14) * 6.0f;
  F(a1, 0x38) = F(a1, 0x18) * 6.0f;
  F(a1, 0x10) = a3 * F(a2, 0xb0) + F(a1, 0x20) + F(a1, 0x10);
  F(a1, 0x14) = a3 * F(a2, 0xb4) + F(a1, 0x24) + F(a1, 0x14);
  F(a1, 0x18) = a3 * F(a2, 0xb8) + F(a1, 0x28) + F(a1, 0x18);
  F(a1, 0x20) = F(a1, 0x20) + F(a1, 0x20) + F(a1, 0x30);
  F(a1, 0x24) = F(a1, 0x24) + F(a1, 0x24) + F(a1, 0x34);
  F(a1, 0x28) = F(a1, 0x28) + F(a1, 0x28) + F(a1, 0x38);
}

void __cdecl rw37_RtBezierQuadOriginDifference4d(u8 *a1, const u8 *a2, float a3, float a4)
{
  float fv[6];
  X f1;
  X f2;
  X f3;
  X f4;
  X f5;
  X f6;
  X f7;

  f1 = a3 * a3;
  f2 = a3 * f1;
  f3 = a3 * a4;
  f4 = f1 * a4;
  f5 = f2 * a4;
  f6 = a4 * a4;
  f7 = a3 * f6;
  fv[5] = f6 * f1;
  fv[4] = f6 * f2;
  fv[0] = f6 * a4;
  fv[3] = f6 * a4 * a3;
  fv[2] = fv[0] * f1;
  fv[1] = fv[0] * f2;
  F(a1, 0x40) = fv[0] * F(a2, 0xc0);
  F(a1, 0x44) = fv[0] * F(a2, 0xc4);
  F(a1, 0x48) = fv[0] * F(a2, 0xc8);
  F(a1, 0x4c) = fv[0] * F(a2, 0xcc);
  F(a1, 0x50) = fv[1] * F(a2, 0);
  F(a1, 0x54) = fv[1] * F(a2, 0x4);
  F(a1, 0x58) = fv[1] * F(a2, 0x8);
  F(a1, 0x5c) = fv[1] * F(a2, 0xc);
  F(a1, 0x60) = fv[2] * F(a2, 0x40);
  F(a1, 0x64) = fv[2] * F(a2, 0x44);
  F(a1, 0x68) = fv[2] * F(a2, 0x48);
  F(a1, 0x6c) = fv[2] * F(a2, 0x4c);
  F(a1, 0x70) = F(a1, 0x50) * 6.0f;
  F(a1, 0x74) = F(a1, 0x54) * 6.0f;
  F(a1, 0x78) = F(a1, 0x58) * 6.0f;
  F(a1, 0x7c) = F(a1, 0x5c) * 6.0f;
  F(a1, 0x50) = fv[3] * F(a2, 0x80) + F(a1, 0x50) + F(a1, 0x60);
  F(a1, 0x54) = fv[3] * F(a2, 0x84) + F(a1, 0x54) + F(a1, 0x64);
  F(a1, 0x58) = fv[3] * F(a2, 0x88) + F(a1, 0x58) + F(a1, 0x68);
  F(a1, 0x5c) = fv[3] * F(a2, 0x8c) + F(a1, 0x5c) + F(a1, 0x6c);
  F(a1, 0x60) = F(a1, 0x60) + F(a1, 0x60) + F(a1, 0x70);
  F(a1, 0x64) = F(a1, 0x64) + F(a1, 0x64) + F(a1, 0x74);
  F(a1, 0x68) = F(a1, 0x68) + F(a1, 0x68) + F(a1, 0x78);
  F(a1, 0x6c) = F(a1, 0x6c) + F(a1, 0x6c) + F(a1, 0x7c);
  F(a1, 0x80) = f6 * F(a2, 0xd0);
  F(a1, 0x84) = f6 * F(a2, 0xd4);
  F(a1, 0x88) = f6 * F(a2, 0xd8);
  F(a1, 0x8c) = f6 * F(a2, 0xdc);
  F(a1, 0x90) = fv[4] * F(a2, 0x10);
  F(a1, 0x94) = fv[4] * F(a2, 0x14);
  F(a1, 0x98) = fv[4] * F(a2, 0x18);
  F(a1, 0x9c) = fv[4] * F(a2, 0x1c);
  F(a1, 0xa0) = fv[5] * F(a2, 0x50);
  F(a1, 0xa4) = fv[5] * F(a2, 0x54);
  F(a1, 0xa8) = fv[5] * F(a2, 0x58);
  F(a1, 0xac) = fv[5] * F(a2, 0x5c);
  F(a1, 0xb0) = F(a1, 0x90) * 6.0f;
  F(a1, 0xb4) = F(a1, 0x94) * 6.0f;
  F(a1, 0xb8) = F(a1, 0x98) * 6.0f;
  F(a1, 0xbc) = F(a1, 0x9c) * 6.0f;
  F(a1, 0x90) = f7 * F(a2, 0x90) + F(a1, 0xa0) + F(a1, 0x90);
  F(a1, 0x94) = f7 * F(a2, 0x94) + F(a1, 0xa4) + F(a1, 0x94);
  F(a1, 0x98) = f7 * F(a2, 0x98) + F(a1, 0xa8) + F(a1, 0x98);
  F(a1, 0x9c) = f7 * F(a2, 0x9c) + F(a1, 0xac) + F(a1, 0x9c);
  F(a1, 0xa0) = F(a1, 0xa0) + F(a1, 0xa0) + F(a1, 0xb0);
  F(a1, 0xa4) = F(a1, 0xa4) + F(a1, 0xa4) + F(a1, 0xb4);
  F(a1, 0xa8) = F(a1, 0xa8) + F(a1, 0xa8) + F(a1, 0xb8);
  F(a1, 0xac) = F(a1, 0xac) + F(a1, 0xac) + F(a1, 0xbc);
  F(a1, 0xc0) = F(a1, 0x40) * 6.0f;
  F(a1, 0xc4) = F(a1, 0x44) * 6.0f;
  F(a1, 0xc8) = F(a1, 0x48) * 6.0f;
  F(a1, 0xcc) = F(a1, 0x4c) * 6.0f;
  F(a1, 0xd0) = F(a1, 0x50) * 6.0f;
  F(a1, 0xd4) = F(a1, 0x54) * 6.0f;
  F(a1, 0xd8) = F(a1, 0x58) * 6.0f;
  F(a1, 0xdc) = F(a1, 0x5c) * 6.0f;
  F(a1, 0xe0) = F(a1, 0x60) * 6.0f;
  F(a1, 0xe4) = F(a1, 0x64) * 6.0f;
  F(a1, 0xe8) = F(a1, 0x68) * 6.0f;
  F(a1, 0xec) = F(a1, 0x6c) * 6.0f;
  F(a1, 0xf0) = F(a1, 0x70) * 6.0f;
  F(a1, 0xf4) = F(a1, 0x74) * 6.0f;
  F(a1, 0xf8) = F(a1, 0x78) * 6.0f;
  F(a1, 0xfc) = F(a1, 0x7c) * 6.0f;
  F(a1, 0) = a4 * F(a2, 0xe0);
  F(a1, 0x4) = a4 * F(a2, 0xe4);
  F(a1, 0x8) = a4 * F(a2, 0xe8);
  F(a1, 0xc) = a4 * F(a2, 0xec);
  F(a1, 0x10) = f5 * F(a2, 0x20);
  F(a1, 0x14) = f5 * F(a2, 0x24);
  F(a1, 0x18) = f5 * F(a2, 0x28);
  F(a1, 0x1c) = f5 * F(a2, 0x2c);
  F(a1, 0x20) = f4 * F(a2, 0x60);
  F(a1, 0x24) = f4 * F(a2, 0x64);
  F(a1, 0x28) = f4 * F(a2, 0x68);
  F(a1, 0x2c) = f4 * F(a2, 0x6c);
  F(a1, 0x30) = F(a1, 0x10) * 6.0f;
  F(a1, 0x34) = F(a1, 0x14) * 6.0f;
  F(a1, 0x38) = F(a1, 0x18) * 6.0f;
  F(a1, 0x3c) = F(a1, 0x1c) * 6.0f;
  F(a1, 0x10) = f3 * F(a2, 0xa0) + F(a1, 0x10) + F(a1, 0x20);
  F(a1, 0x14) = f3 * F(a2, 0xa4) + F(a1, 0x14) + F(a1, 0x24);
  F(a1, 0x18) = f3 * F(a2, 0xa8) + F(a1, 0x18) + F(a1, 0x28);
  F(a1, 0x1c) = f3 * F(a2, 0xac) + F(a1, 0x2c) + F(a1, 0x1c);
  F(a1, 0x20) = F(a1, 0x20) + F(a1, 0x20) + F(a1, 0x30);
  F(a1, 0x24) = F(a1, 0x24) + F(a1, 0x24) + F(a1, 0x34);
  F(a1, 0x28) = F(a1, 0x28) + F(a1, 0x28) + F(a1, 0x38);
  F(a1, 0x2c) = F(a1, 0x2c) + F(a1, 0x2c) + F(a1, 0x3c);
  F(a1, 0x40) = F(a1, 0x80) + F(a1, 0x40) + F(a1, 0);
  F(a1, 0x44) = F(a1, 0x84) + F(a1, 0x44) + F(a1, 0x4);
  F(a1, 0x48) = F(a1, 0x88) + F(a1, 0x48) + F(a1, 0x8);
  F(a1, 0x4c) = F(a1, 0x8c) + F(a1, 0x4c) + F(a1, 0xc);
  F(a1, 0x50) = F(a1, 0x10) + F(a1, 0x90) + F(a1, 0x50);
  F(a1, 0x54) = F(a1, 0x54) + F(a1, 0x14) + F(a1, 0x94);
  F(a1, 0x58) = F(a1, 0x98) + F(a1, 0x58) + F(a1, 0x18);
  F(a1, 0x5c) = F(a1, 0x5c) + F(a1, 0x9c) + F(a1, 0x1c);
  F(a1, 0x60) = F(a1, 0xa0) + F(a1, 0x60) + F(a1, 0x20);
  F(a1, 0x64) = F(a1, 0x64) + F(a1, 0xa4) + F(a1, 0x24);
  F(a1, 0x68) = F(a1, 0xa8) + F(a1, 0x68) + F(a1, 0x28);
  F(a1, 0x6c) = F(a1, 0x6c) + F(a1, 0xac) + F(a1, 0x2c);
  F(a1, 0x70) = F(a1, 0x70) + F(a1, 0xb0) + F(a1, 0x30);
  F(a1, 0x74) = F(a1, 0x74) + F(a1, 0xb4) + F(a1, 0x34);
  F(a1, 0x78) = F(a1, 0x78) + F(a1, 0xb8) + F(a1, 0x38);
  F(a1, 0x7c) = F(a1, 0x7c) + F(a1, 0xbc) + F(a1, 0x3c);
  F(a1, 0x80) = F(a1, 0x80) + F(a1, 0x80) + F(a1, 0xc0);
  F(a1, 0x84) = F(a1, 0x84) + F(a1, 0x84) + F(a1, 0xc4);
  F(a1, 0x88) = F(a1, 0x88) + F(a1, 0x88) + F(a1, 0xc8);
  F(a1, 0x8c) = F(a1, 0x8c) + F(a1, 0x8c) + F(a1, 0xcc);
  F(a1, 0x90) = F(a1, 0x90) + F(a1, 0x90) + F(a1, 0xd0);
  F(a1, 0x94) = F(a1, 0x94) + F(a1, 0x94) + F(a1, 0xd4);
  F(a1, 0x98) = F(a1, 0x98) + F(a1, 0x98) + F(a1, 0xd8);
  F(a1, 0x9c) = F(a1, 0x9c) + F(a1, 0x9c) + F(a1, 0xdc);
  F(a1, 0xa0) = F(a1, 0xa0) + F(a1, 0xa0) + F(a1, 0xe0);
  F(a1, 0xa4) = F(a1, 0xa4) + F(a1, 0xa4) + F(a1, 0xe4);
  F(a1, 0xa8) = F(a1, 0xa8) + F(a1, 0xa8) + F(a1, 0xe8);
  F(a1, 0xac) = F(a1, 0xac) + F(a1, 0xac) + F(a1, 0xec);
  F(a1, 0xb0) = F(a1, 0xb0) + F(a1, 0xb0) + F(a1, 0xf0);
  F(a1, 0xb4) = F(a1, 0xb4) + F(a1, 0xb4) + F(a1, 0xf4);
  F(a1, 0xb8) = F(a1, 0xb8) + F(a1, 0xb8) + F(a1, 0xf8);
  F(a1, 0xbc) = F(a1, 0xbc) + F(a1, 0xbc) + F(a1, 0xfc);
  U(a1, 0) = U(a2, 0xf0);
  U(a1, 0x4) = U(a2, 0xf4);
  U(a1, 0x8) = U(a2, 0xf8);
  U(a1, 0xc) = U(a2, 0xfc);
  F(a1, 0x10) = f2 * F(a2, 0x30);
  F(a1, 0x14) = f2 * F(a2, 0x34);
  F(a1, 0x18) = f2 * F(a2, 0x38);
  F(a1, 0x1c) = f2 * F(a2, 0x3c);
  F(a1, 0x20) = f1 * F(a2, 0x70);
  F(a1, 0x24) = f1 * F(a2, 0x74);
  F(a1, 0x28) = f1 * F(a2, 0x78);
  F(a1, 0x2c) = f1 * F(a2, 0x7c);
  F(a1, 0x30) = F(a1, 0x10) * 6.0f;
  F(a1, 0x34) = F(a1, 0x14) * 6.0f;
  F(a1, 0x38) = F(a1, 0x18) * 6.0f;
  F(a1, 0x3c) = F(a1, 0x1c) * 6.0f;
  F(a1, 0x10) = a3 * F(a2, 0xb0) + F(a1, 0x10) + F(a1, 0x20);
  F(a1, 0x14) = a3 * F(a2, 0xb4) + F(a1, 0x14) + F(a1, 0x24);
  F(a1, 0x18) = a3 * F(a2, 0xb8) + F(a1, 0x18) + F(a1, 0x28);
  F(a1, 0x1c) = a3 * F(a2, 0xbc) + F(a1, 0x2c) + F(a1, 0x1c);
  F(a1, 0x20) = F(a1, 0x20) + F(a1, 0x20) + F(a1, 0x30);
  F(a1, 0x24) = F(a1, 0x24) + F(a1, 0x24) + F(a1, 0x34);
  F(a1, 0x28) = F(a1, 0x28) + F(a1, 0x28) + F(a1, 0x38);
  F(a1, 0x2c) = F(a1, 0x2c) + F(a1, 0x2c) + F(a1, 0x3c);
}


// _rpPatchRefineQuadVector 0x55cb20: (n+1)^2 RwV3d at out (byte stride), positions of the cubic patch ctrl (16 RtBezierV4d) on the u/v grid
// with step 1/n.  Returns out.  The original keeps D[0][0].xyz, D[0][1].xyz and D[0][2].x on the x87 stack through the u loop and
// D[0][2].yz in two stack temporaries; no check of n (n < 0 runs ~2^32 iterations, as in the original).
u8 *__cdecl rw37__rpPatchRefineQuadVector(u8 *out, int32_t stride, const u8 *ctrl, int32_t n){
	float step, W[64], D[64];
	step = 1.0f / (X)n;
	rw37_RtBezierQuadBernsteinWeight3d((u8 *)W, ctrl);
	rw37_RtBezierQuadOriginDifference3d((u8 *)D, (u8 *)W, step, step);
	u8 *p = out;
	X a = D[0];
	for(u32 v = (u32)n + 1;;){
		X b = D[1], c = D[2], d = D[4], e = D[5], f = D[6], g = D[8];
		float t9, t10;
		U(&t9, 0) = U(D, 9*4); U(&t10, 0) = U(D, 10*4);
		for(u32 u = (u32)n + 1;;){
			F(p, 0) = a; F(p, 4) = b; F(p, 8) = c;
			p += stride;
			if(--u == 0) break;
			a = a + d; b = b + e; c = c + f;
			d = d + g; e = e + t9; f = f + t10;
			g = g + D[12];
			t9 = D[13] + t9;
			t10 = D[14] + t10;
		}
		if(--v == 0) break;
		a = D[16] + D[0]; D[0] = a;
		for(int k = 1; k < 48; k++) if((k & 3) != 3) D[k] = D[k + 16] + D[k];
	}
	return out;
}

// _rpPatchRefineQuadColor 0x55cea0: (n+1)^2 RwRGBA at out (byte stride) from the 4d patch ctrl (16 RtBezierV4d = r, g, b, a); each channel
// is the float truncated to int32 (chop) and its low byte stored.  Returns out; n < 0 writes nothing.
u8 *__cdecl rw37__rpPatchRefineQuadColor(u8 *out, int32_t stride, const u8 *ctrl, int32_t n){
	float step, W[64], D[64], R[16];
	step = 1.0f / (X)n;
	rw37_RtBezierQuadBernsteinWeight4d((u8 *)W, ctrl);
	rw37_RtBezierQuadOriginDifference4d((u8 *)D, (u8 *)W, step, step);
	if(n < 0) return out;
	u8 *p = out;
	for(int32_t v = n + 1; v; v--){
		memcpy(R, D, sizeof R);		// integer copy of the first difference row
		for(int32_t u = n + 1; u; u--){
			p[0] = (u8)fistpChop(R[0]);
			p[1] = (u8)fistpChop(R[1]);
			p[2] = (u8)fistpChop(R[2]);
			p[3] = (u8)fistpChop(R[3]);
			p += stride;
			for(int k = 0; k < 12; k++) R[k] = R[k + 4] + R[k];
		}
		for(int k = 0; k < 48; k++) D[k] = D[k + 16] + D[k];
	}
	return out;
}

// _rpPatchRefineQuadTexCoords 0x55d4e0: (n+1)^2 RwTexCoords at out (byte stride), bilinear between the 4 corner uvs c[0..3] with step 1/n.
// Returns out; n < 0 writes nothing.
u8 *__cdecl rw37__rpPatchRefineQuadTexCoords(u8 *out, int32_t stride, const float *c, int32_t n){
	float s = 1.0f / (X)n;
	float dv0 = (c[5] - c[1]) * s, dv1 = (c[7] - c[3]) * s, du0 = (c[4] - c[0]) * s, du1 = (c[6] - c[2]) * s;
	u8 *p = out;
	for(int32_t i = 0; i <= n; i++){
		X xi = i;
		float A = du0 * xi + c[0];
		float B = dv0 * xi + c[1];
		float C = (dv1 * xi + c[3] - B) * s;
		X Dd = (xi * du1 + c[2] - A) * s;
		for(int32_t j = 0; j <= n; j++){
			X xj = j;
			F(p, 0) = xj * Dd + A;
			F(p, 4) = xj * C + B;
			p += stride;
		}
	}
	return out;
}

// _rpPatchGenerateQuadIndex 0x55d6c0: n triangle strips of the (n+1) x (n+1) vertex grid starting at vertex base, 2(n+1) indices each
// (v, v+n+1 pairs), the odd strips written backwards from their end.  Returns out; n <= 0 writes nothing.
u16 *__cdecl rw37__rpPatchGenerateQuadIndex(u16 *out, int32_t base, int32_t n){
	if(n <= 0) return out;
	u16 *row = out, *p = out;
	int32_t dir = 1, flag = 1, v = base;
	for(int32_t r = n; r; r--){
		for(int32_t k = n + 1; k; k--){
			*p = (u16)v; p += dir;
			*p = (u16)(n + v + 1); p += dir;
			v++;
		}
		dir = -dir;
		row += 2 * (n + 1);
		p = row + (2 * (n + 1) + dir) * flag;
		flag = flag == 0;
	}
	return out;
}

} // extern "C"
