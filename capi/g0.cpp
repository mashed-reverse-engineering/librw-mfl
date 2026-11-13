// G0: stateless RenderWare functions on caller-owned RW-layout data (plan_librw_integration_2026-10-31.md section 2.3).
// "L1:exact" = written op for op from the disassembly of MFL.exe (address in the comment): operation order, the 12 bit
// _rwSqrt/_rwInvSqrt table and float rounding follow the original, valid when the caller runs the x87 at the game's
// 24 bit precision (CW 0x7f).  "L1:librw" = librw semantics, not yet bit-compared (L2 does that before the gate).
// The DLL never touches the FPU control word.
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"
#include "../rw.h"
#include "../src/rwextra.h"

// ---- the 12 bit sqrt / invsqrt tables: the original's math open routine 0x4d5770 builds them in the engine instance.
static uint32_t sqrtTab[4096], invSqrtTab[4096];
static struct TabInit {
	TabInit(void){
		uint32_t xb = 0x3f800000;
		for(int pass = 0; pass < 2; pass++){
			// pass 0: x = 1.0 + i*2^-11 (fills the upper half), pass 1 continues at 2.0 (lower half)
			for(int i = 0; i < 0x800; i++, xb += 0x1000){
				float x; memcpy(&x, &xb, 4);
				float s = (float)sqrt((double)x);		// fsqrt at 24 bit precision == correctly rounded float
				float r = (float)(1.0 / (double)s);		// fdivr [1.0], one rounding on the register value
				uint32_t sb, rb; memcpy(&sb, &s, 4); memcpy(&rb, &r, 4);
				if(pass == 0){ sqrtTab[0x800+i] = sb - 0x1fc00000; invSqrtTab[0x800+i] = rb - 0x20000000; }
				else         { sqrtTab[i]       = sb - 0x20000000; invSqrtTab[i]       = rb - 0x1fc00000; }
			}
		}
	}
} tabInit;

static inline float tabSqrt(float f){				// 0x4d5410
	uint32_t b; memcpy(&b, &f, 4);
	if(b){ b += 0x800; b = sqrtTab[(b>>12)&0xfff] + ((b>>1)&0x3fc00000); }
	memcpy(&f, &b, 4); return f;
}
static inline float tabInvSqrt(float f){			// 0x4d5470
	uint32_t b; memcpy(&b, &f, 4);
	if(b){ b += 0x800; b = invSqrtTab[(b>>12)&0xfff] + (((~b)>>1)&0x3fc00000); }
	memcpy(&f, &b, 4); return f;
}
static inline float dot3(float x, float y, float z){ float s = x*x; float t = y*y; s = s + t; t = z*z; return s + t; }
static inline float dot2(float x, float y){ float s = x*x; float t = y*y; return s + t; }

extern "C" {

// L1:exact 0x4d5410 (the game calls it _rwSqrt; the api list names the same address rwSqrt)
RWAPI RwReal rwSqrt(RwReal x){ return tabSqrt(x); }
// L1:exact 0x4d5410
RWAPI RwReal _rwSqrt(RwReal x){ return tabSqrt(x); }
// L1:exact 0x4d5470
RWAPI RwReal _rwInvSqrt(RwReal x){ return tabInvSqrt(x); }

// ponytail: the original reports RwError 0x19 when the length is <= 0; not reproduced (needs the error layer, S2).
// L1:exact 0x4d5290
RWAPI RwReal RwV3dNormalize(RwV3d *out, const RwV3d *in){
	float l2 = dot3(in->x, in->y, in->z);
	float len = tabSqrt(l2), recip = tabInvSqrt(l2);
	float a = recip*in->x; out->x = a;
	float b = in->y*recip; out->y = b;
	float c = in->z*recip; out->z = c;
	return len;
}
// L1:exact 0x4d53a0
RWAPI RwReal RwV3dLength(const RwV3d *in){ return tabSqrt(dot3(in->x, in->y, in->z)); }
// L1:exact 0x4d54d0
RWAPI RwReal RwV2dLength(const RwV2d *in){ return tabSqrt(dot2(in->x, in->y)); }
// L1:exact 0x4d5540
RWAPI RwReal RwV2dNormalize(RwV2d *out, const RwV2d *in){
	float l2 = dot2(in->x, in->y);
	float len = tabSqrt(l2), recip = tabInvSqrt(l2);
	float a = recip*in->x; out->x = a;
	float b = in->y*recip; out->y = b;
	return len;
}

// transforms: ((x*right + y*up) + z*at) + pos, each product and sum rounded to float (0x4d5010 / 0x4d4f50 / 0x4d5160 / 0x4d50b0)
static inline void xformPoint(RwV3d *out, const RwV3d *in, const RwMatrix *m, bool pos){
	float x = in->x, y = in->y, z = in->z;
	float x1 = x*m->right.x, y1 = x*m->right.y, z1 = x*m->right.z, t;
	t = y*m->up.x; x1 = t + x1; t = y*m->up.y; y1 = t + y1; t = y*m->up.z; z1 = t + z1;
	t = z*m->at.x; x1 = t + x1; t = z*m->at.y; y1 = t + y1; t = z*m->at.z; z1 = t + z1;
	if(pos){ x1 = x1 + m->pos.x; y1 = y1 + m->pos.y; z1 = z1 + m->pos.z; }
	out->x = x1; out->y = y1; out->z = z1;
}
// L1:exact 0x4d5640 -> 0x4d5010
RWAPI RwV3d *RwV3dTransformPoint(RwV3d *out, const RwV3d *in, const RwMatrix *m){ xformPoint(out, in, m, true); return out; }
// L1:exact 0x4d5670 -> 0x4d4f50
RWAPI RwV3d *RwV3dTransformPoints(RwV3d *out, const RwV3d *in, RwInt32 n, const RwMatrix *m){
	for(RwInt32 i = 0; i < n; i++) xformPoint(&out[i], &in[i], m, true);
	return out;
}
// L1:exact 0x4d5160 (via the 0x4d56a0 dispatcher)
RWAPI RwV3d *RwV3dTransformVector(RwV3d *out, const RwV3d *in, const RwMatrix *m){ xformPoint(out, in, m, false); return out; }
// L1:exact 0x4d50b0 (via the 0x4d56d0 dispatcher)
RWAPI RwV3d *RwV3dTransformVectors(RwV3d *out, const RwV3d *in, RwInt32 n, const RwMatrix *m){
	for(RwInt32 i = 0; i < n; i++) xformPoint(&out[i], &in[i], m, false);
	return out;
}

// ---- matrices (flags: 0x20000 identity, 3 type mask)
// L1:exact 0x4d3cc0
RWAPI RwMatrix *RwMatrixUpdate(RwMatrix *m){ m->flags &= 0xfffdfffc; return m; }

static void identityWith(RwMatrix *m){
	memset(&m->right, 0, 12); memset(&m->up, 0, 12); memset(&m->at, 0, 12); memset(&m->pos, 0, 12);
	m->right.x = m->up.y = m->at.z = 1.0f;
	m->flags |= 0x20003;
}
// ponytail: an invalid combine op returns NULL; the original reports RwError 0x80000003 and then dereferences NULL.
// L1:exact 0x4d4870
RWAPI RwMatrix *RwMatrixTranslate(RwMatrix *m, const RwV3d *t, RwOpCombineType op){
	switch(op){
	case 0: identityWith(m); m->pos = *t; break;
	case 1: {
		float tx = t->x, ty = t->y, tz = t->z, a, b;
		a = m->at.x*tz; b = m->up.x*ty; a = a + b; b = m->right.x*tx; a = a + b; m->pos.x = a + m->pos.x;
		a = m->at.y*tz; b = m->up.y*ty; a = a + b; b = m->right.y*tx; a = a + b; m->pos.y = a + m->pos.y;
		a = m->at.z*tz; b = m->up.z*ty; a = a + b; b = m->right.z*tx; a = a + b; m->pos.z = a + m->pos.z;
		break; }
	case 2: m->pos.x = m->pos.x + t->x; m->pos.y = t->y + m->pos.y; m->pos.z = t->z + m->pos.z; break;
	default: return NULL;
	}
	m->flags &= 0xfffdffff;
	return m;
}
// L1:exact 0x4d46e0
RWAPI RwMatrix *RwMatrixScale(RwMatrix *m, const RwV3d *s, RwOpCombineType op){
	switch(op){
	case 0: identityWith(m); m->right.x = s->x; m->up.y = s->y; m->at.z = s->z; break;
	case 1:
		m->right.x *= s->x; m->right.y *= s->x; m->right.z *= s->x;
		m->up.x = s->y*m->up.x; m->up.y = s->y*m->up.y; m->up.z = s->y*m->up.z;
		m->at.x = s->z*m->at.x; m->at.y = s->z*m->at.y; m->at.z = s->z*m->at.z;
		break;
	case 2:
		m->right.x *= s->x; m->right.y *= s->y; m->right.z *= s->z;
		m->up.x *= s->x; m->up.y = s->y*m->up.y; m->up.z *= s->z;
		m->at.x *= s->x; m->at.y *= s->y; m->at.z *= s->z;
		m->pos.x *= s->x; m->pos.y = s->y*m->pos.y; m->pos.z *= s->z;
		break;
	default: return NULL;
	}
	m->flags &= 0xfffdfffc;
	return m;
}

// RwMatrixMultiply / Invert / Rotate / Transform, RtIntersectionLineTriangle, RpRandom / RpRandomSeed: disasm-exact in g1.cpp (S4)

// ---- Rt intersection / barycentric (K3 W7: were librw implementations, DIFF in rw_g0_check.sh; now op for op)
// L1:exact 0x4d51f0 (_rwV3dNormalize: returns the reciprocal length from the 12 bit table, 0 for a 0 length; out may alias in)
static inline float v3dNormalizeRecip(RwV3d *out, const RwV3d *in){
	float r = tabInvSqrt(dot3(in->x, in->y, in->z));
	out->x = r*in->x; out->y = in->y*r; out->z = in->z*r;
	return r;
}
// L1:exact 0x569110.  The original also overwrites its own argument slots (line, sphere) with temporaries: not reproduced (callee-owned).
RWAPI RwBool RtLineSphereIntersectionTest(RwLine *line, RwSphere *sphere, RwReal *dist){
	RwV3d dir;
	dir.x = line->end.x - line->start.x; dir.y = line->end.y - line->start.y; dir.z = line->end.z - line->start.z;
	float inv = v3dNormalizeRecip(&dir, &dir);
	float r = sphere->radius, r2 = r*r;
	float dx = sphere->center.x - line->start.x, dy = sphere->center.y - line->start.y, dz = sphere->center.z - line->start.z;
	float t, proj, dist2;
	proj = dy*dir.y; t = dx*dir.x; proj = proj + t; t = dz*dir.z; proj = proj + t;
	dist2 = dy*dy; t = dx*dx; dist2 = dist2 + t; t = dz*dz; dist2 = dist2 + t;
	if(dist2 < r2){ *dist = proj*inv; return 1; }			// fcomp; test ah,5; jp: unordered takes the outside path
	if(!(proj > 0.0f)) return 0;					// C3|C0: <= 0 or unordered
	t = proj*proj; t = dist2 - t; t = r2 - t;
	if(!(t >= 0.0f)) return 0;					// C0: < 0 or unordered
	*dist = proj*inv; return 1;
}
// L1:exact 0x569610 (m = 16 floats; eps 0x5f6328 = 2^-12, third 0x5ddc1c)
RWAPI RwBool RtBaryGetTransform(RtBaryTransform m, RwReal *scale, const RwV3d *v0, const RwV3d *v1, const RwV3d *v2){
	const float eps = 0.000244140625f, third = 1.0f/3.0f;
	float e1x = v1->x - v2->x, e1y = v1->y - v2->y, e1z = v1->z - v2->z;
	float e2x = v2->x - v0->x, e2y = v2->y - v0->y, e2z = v2->z - v0->z;
	float nx = e2z*e1y - e2y*e1z;
	float ny = e2x*e1z - e2z*e1x;
	float nz = e2y*e1x - e2x*e1y;
	float d = ny*ny; d = d + nx*nx; d = d + nz*nz;
	float inv; RwBool ret;
	if(eps < d){ float s = __builtin_sqrtf(d); *scale = s; inv = 1.0f/s; ret = 1; }	// fsqrt; fdivr [1.0]
	else { ret = 0; *scale = 0.0f; inv = 0.0f; }
	float A = nx*inv, B = inv*ny, C = inv*nz;
	float P = e1y*C - B*e1z, Q = A*e1z - C*e1x, R = B*e1x - A*e1y;
	P = P*inv; Q = Q*inv; R = R*inv;
	float U = C*e2y - B*e2z, V = A*e2z - C*e2x, W = B*e2x - A*e2y;
	U = U*inv; V = V*inv; W = W*inv;
	float sx = v0->x + v1->x, sy = v0->y + v1->y, sz = v0->z + v1->z;
	sx = sx + v2->x; sy = v2->y + sy; sz = v2->z + sz;
	m[1] = U; m[5] = V; m[9] = W;					// store order of the original (5697c9..)
	sx = sx*third; sy = sy*third; sz = sz*third;
	m[0] = P; m[2] = -(U + P); m[3] = A; m[4] = Q; m[6] = -(V + Q); m[7] = B; m[8] = R; m[10] = -(W + R); m[11] = C;
	float t = v2->y*Q; t = t + P*v2->x; t = t + v2->z*R; m[12] = -t;
	t = v0->y*V; t = t + U*v0->x; t = t + v0->z*W; t = -t; m[13] = t;
	m[14] = 1.0f - (t + m[12]);
	t = sy*B; t = t + sx*A; t = t + sz*C; m[15] = -t;
	return ret;
}

RWAPI RwUInt32 rwmfl_get_fpucw(void){ unsigned short cw; __asm__ volatile("fnstcw %0" : "=m"(cw)); return cw; }

}
