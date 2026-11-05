// RESAMPLE (LIB_RW group resample; output/rw_next_family_2026-11-05.md row 6, the resampler): RwImageResample 0x4eacf0 and
// ImageResampleGetSpan 0x4eb110 of MFL.exe, written op for op from the disassembly like hanimcam.cpp (X = a value the original keeps on the
// x87 stack, R() = its store to a float slot; valid at the game's x87 control word, the same operations in the same order).
// Box filter over 32 bit images in 16.16 fixed point: every destination pixel is the area weighted mean of the source pixels it covers.
// No exe state, no bind.  Check: tools/delink/rw_resample_check.sh (the original code and these exports natively under Wine).
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
typedef uint8_t u8;

struct Image { int32_t flags, width, height, depth, stride; u8 *pixels, *palette; };

static inline float R(X x){ volatile float f = (float)x; return f; }		// fstp to a float slot
// the original's float constants (0x5ea288 1/65536, 0x5dff00 1/255, 0x5dfd84 65536, 0x5dd9ac 255, 0x5dd340 0.5, 0x5dd330 1): float objects
static const float kFix = 1.52587890625e-05f, kInv255 = 0.0039215688593685627f, k65536 = 65536.0f, k255 = 255.0f, kHalf = 0.5f, kOne = 1.0f;

extern "C" {

// 0x4eb110: mean colour (0..1 floats) of source row y >> 16 between the 16.16 x positions x0 and x1, the end pixels weighted by their cover
void __cdecl rw37_ImageResampleGetSpan(const Image *src, int32_t x0, int32_t x1, int32_t y, float *out){
	X w = (X)(int32_t)(x1 - x0) * kFix;
	float wf = R(w);
	const u8 *p = src->pixels + src->stride * (y >> 16) + (x0 >> 16) * 4;
	int32_t xe = x1 >> 16;
	if((x0 >> 16) == xe){
		for(int c = 0; c < 4; c++) out[c] = R((X)p[c] * kInv255);
		for(int c = 0; c < 4; c++) out[c] = R(w * out[c]);
	}else{
		for(int c = 0; c < 4; c++) out[c] = R((X)p[c] * kInv255);
		int32_t b = (int32_t)((x0 & 0xffff0000u) + 0x10000u);
		p += 4;
		X w0 = (X)(int32_t)(b - x0) * kFix;
		for(int c = 0; c < 4; c++) out[c] = R(w0 * out[c]);
		if((b >> 16) != xe) do{
			b += 0x10000;
			X v0 = (X)p[0] * kInv255, v1 = (X)p[1] * kInv255;
			float v2 = R((X)p[2] * kInv255), v3 = R((X)p[3] * kInv255);
			out[0] = R(v0 + out[0]);
			out[1] = R((X)out[1] + v1);
			out[2] = R((X)out[2] + v2);
			out[3] = R((X)out[3] + v3);
			p += 4;
		}while((b >> 16) != xe);
		X q0 = (X)p[0] * kInv255, q1 = (X)p[1] * kInv255;
		float q2 = R((X)p[2] * kInv255), q3 = R((X)p[3] * kInv255);
		float wl = R((X)(int32_t)(x1 - b) * kFix);
		float q0w = R(q0 * wl);
		X q1w = q1 * wl, q2w = (X)q2 * wl;
		float q3w = R((X)q3 * wl);
		out[0] = R((X)q0w + out[0]);
		out[1] = R((X)out[1] + q1w);
		out[2] = R((X)out[2] + q2w);
		out[3] = R((X)out[3] + q3w);
	}
	X inv = (X)kOne / wf;
	for(int c = 0; c < 4; c++) out[c] = R(inv * out[c]);
}

// 0x4eacf0: dst (32 bit, its own size) = src (32 bit) box filtered; dst takes over src's gamma flag
Image *__cdecl rw37_RwImageResample(Image *dst, const Image *src){
	int32_t dW = dst->width, sW = src->width, sH = src->height, dH = dst->height;
	X t = (X)sW / (X)dW;
	dst->flags = dst->flags | (src->flags & 2);
	int32_t stepX = (int32_t)(X)R(t * k65536);
	int32_t stepY = (int32_t)(X)R((X)sH / (X)dH * k65536);
	int32_t yf = 0;
	for(int32_t y = 0; y < dH; y++, yf += stepY){
		u8 *d = dst->pixels + dst->stride * y;
		int32_t xf = 0;
		if(dW <= 0) continue;
		int32_t yEnd = stepY + yf - 1;
		float yw = R((X)(int32_t)(yEnd - yf) * kFix);
		int32_t y0 = yf >> 16, y1 = yEnd >> 16;
		for(int32_t n = dW; n; n--, xf += stepX, d += 4){
			int32_t xEnd = stepX + xf - 1;
			float acc[8];
			X a0, a1;
			if(y0 == y1){
				rw37_ImageResampleGetSpan(src, xf, xEnd, yf, acc);
				a0 = (X)yw * acc[0];
				a1 = (X)acc[1] * yw;
				acc[2] = R((X)acc[2] * yw);
				acc[3] = R((X)acc[3] * yw);
			}else{
				int32_t b = (int32_t)((yf & 0xffff0000u) + 0x10000u);
				rw37_ImageResampleGetSpan(src, xf, xEnd, yf, acc);
				X w0 = (X)(int32_t)(b - yf) * kFix;
				for(int c = 0; c < 4; c++) acc[c] = R((X)acc[c] * w0);
				if((b >> 16) != y1) do{
					rw37_ImageResampleGetSpan(src, xf, xEnd, b, acc + 4);
					b += 0x10000;
					acc[0] = R((X)acc[4] + acc[0]);
					acc[1] = R((X)acc[1] + acc[5]);
					acc[2] = R((X)acc[2] + acc[6]);
					acc[3] = R((X)acc[3] + acc[7]);
				}while((b >> 16) != y1);
				rw37_ImageResampleGetSpan(src, xf, xEnd, b, acc + 4);
				X wl = (X)(int32_t)(stepY + yf - 1 - b) * kFix;
				for(int c = 4; c < 8; c++) acc[c] = R((X)acc[c] * wl);
				a0 = (X)acc[4] + acc[0];
				a1 = (X)acc[1] + acc[5];
				acc[2] = R((X)acc[2] + acc[6]);
				acc[3] = R((X)acc[3] + acc[7]);
			}
			X inv = (X)kOne / yw;
			float invf = R(inv);
			acc[0] = R(inv * a0);
			acc[1] = R(a1 * invf);
			acc[2] = R((X)acc[2] * invf);
			acc[3] = R((X)acc[3] * invf);
			for(int c = 0; c < 4; c++) d[c] = (u8)(int32_t)(X)R((X)acc[c] * k255 + kHalf);
		}
	}
	return dst;
}

} // extern "C"
