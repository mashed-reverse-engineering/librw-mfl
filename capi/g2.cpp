// G2 (plan_librw_d3dx S4): the collision code's triangle tests and the RwMatrix allocator, written op for op from the disassembly of
// MFL.exe like g1.cpp (same conventions: X = a value the original keeps on the x87 stack, float = a value it stores; CW 0x7f).
// The intersection tests only read their inputs (the sphere test also writes normal / distance in the original's order).
// Check: tools/delink/rw_g2_check.sh.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"

typedef long double X;
static inline uint32_t SB(float f){ uint32_t b; memcpy(&b, &f, 4); return b; }

extern "C" {

// ---- RwMatrixCreate 0x4d4e70 / RwMatrixDestroy 0x4d4e40.  The originals take 64 byte entries from the matrix module's free list
// (static RwFreeList 0x7d212c, created by the module open 0x4d3b40, align 4; engine + [0x7d2150] holds its pointer; the engine
// functions +0x118 / +0x11c alloc / free).  Only these two use that list, so after the swap the DLL owns matrix memory (malloc,
// 8 byte aligned) and the exe's list stays created and empty.  The pad words 0x1c/0x2c/0x3c are left unset like the original.
// ponytail: plain malloc instead of a pool; the game creates about ten matrices, add a free list if a profile ever shows them.
// L1:exact 0x4d4e70
RWAPI RwMatrix *RwMatrixCreate(void){
	uint32_t *m = (uint32_t *)malloc(64);
	if(!m) return NULL;
	m[0] = 0x3f800000; m[1] = 0; m[2] = 0; m[3] = 0x20003;
	m[4] = 0; m[5] = 0x3f800000; m[6] = 0;
	m[8] = 0; m[9] = 0; m[10] = 0x3f800000;
	m[12] = 0; m[13] = 0; m[14] = 0;
	return (RwMatrix *)m;
}
// L1:exact 0x4d4e40
RWAPI RwBool RwMatrixDestroy(RwMatrix *mpMat){ free(mpMat); return 1; }

// ---- RtIntersectionSphereTriangle 0x557a80
// one axis of the bounding test: the sign bits of the three vertex offsets; all on one side and all farther than r -> separated
static inline int sphereAxisOut(float a, float b, float c, float r){
	uint32_t n = (SB(a) >> 31) + (SB(b) >> 31) + (SB(c) >> 31);
	if(n == 0){ float p = a - r, q = b - r, s = c - r; return !((SB(p) | SB(q) | SB(s)) >> 31); }
	if(n == 3){ float p = a + r, q = b + r, s = c + r; return (SB(p) & SB(q) & SB(s)) >> 31; }
	return 0;
}
// closest point of the edge line P + t (Q - P) to the centre (offsets are centre relative); 0 = the sphere misses on this edge
static inline int sphereEdgeOut(const float *P, const float *Q, const float *O, float r2){
	X ex = Q[0] - P[0], ey = Q[1] - P[1]; float ez = Q[2] - P[2];
	X dot = P[1]*ey + ex*P[0]; dot = dot + ez*P[2];
	X len = ey*ey + ex*ex; len = len + ez*ez;
	float t = dot/len;
	float ext = ex*t; float px = P[0] - ext;
	float py = P[1] - ey*t;
	X pz = P[2] - ez*t;
	X d2 = py*py + px*px; d2 = d2 + pz*pz;
	if(!(d2 > r2)) return 0;
	X q = py*O[1] + px*O[0]; q = q + pz*O[2];
	return d2 < q;
}
static inline X dot3(const float *a, const float *b){ X s = a[1]*b[1] + a[0]*b[0]; return s + a[2]*b[2]; }
// L1:exact 0x557a80: *normal = unit face normal, *distance = signed distance of the centre (written before the vertex / edge tests)
RWAPI RwBool RtIntersectionSphereTriangle(const RwSphere *sphere, const RwV3d *v0, const RwV3d *v1, const RwV3d *v2, RwV3d *normal, RwReal *distance){
	const float *c = (const float *)sphere, *p0 = (const float *)v0, *p1 = (const float *)v1, *p2 = (const float *)v2;
	float *n = (float *)normal;
	float A[3] = {p0[0] - c[0], p0[1] - c[1], p0[2] - c[2]};
	float B[3] = {p1[0] - c[0], p1[1] - c[1], p1[2] - c[2]};
	float C[3] = {p2[0] - c[0], p2[1] - c[1], p2[2] - c[2]};
	if(sphereAxisOut(A[0], B[0], C[0], c[3]) || sphereAxisOut(A[1], B[1], C[1], c[3]) || sphereAxisOut(A[2], B[2], C[2], c[3])) return 0;
	X d1x = p1[0] - p0[0], d1y = p1[1] - p0[1], d1z = p1[2] - p0[2];
	float d2x = p2[0] - p0[0], d2y = p2[1] - p0[1]; X d2z = p2[2] - p0[2];
	float nx = d2z*d1y - d2y*d1z; n[0] = nx;
	float ny = d1z*d2x - d2z*d1x; n[1] = ny;
	X nz = d1x*d2y - d2x*d1y; n[2] = nz;
	X s = ny*ny + nx*nx; s = s + nz*nz;
	if(s <= 0.0f) return 0;
	float sf = s;
	X i = _rwInvSqrt(sf);
	X a = i*n[0]; n[0] = a;
	X b = n[1]*i; n[1] = b;
	X e = n[2]*i; n[2] = e;
	X dd = b*A[1] + a*A[0]; dd = dd + e*A[2];
	float d = dd;
	if(-c[3] > d) return 0;
	if(d > c[3]) return 0;
	*distance = -d;
	float r2 = c[3]*c[3];
	X sa = dot3(A, A); float SA = sa; if(sa <= r2) return 1;		// a vertex inside the sphere
	X sb = dot3(B, B); float SB_ = sb; if(sb <= r2) return 1;
	X sc = dot3(C, C); float SC = sc; if(sc <= r2) return 1;
	// the nearest vertex: if both its edges point away from the centre the sphere touches neither edge nor face
	int edges;
	if(SA < SB_){
		if(SC < SA) edges = !(dot3(C, A) > SC) || !(dot3(C, B) > SC);
		else edges = !(dot3(B, A) > SA) || !(dot3(C, A) > SA);
	}else if(SC < SB_) edges = !(dot3(C, A) > SC) || !(dot3(C, B) > SC);
	else edges = !(dot3(B, A) > SB_) || !(dot3(C, B) > SB_);
	if(!edges) return 0;
	if(sphereEdgeOut(A, B, C, r2)) return 0;
	if(sphereEdgeOut(B, C, A, r2)) return 0;
	if(sphereEdgeOut(C, A, B, r2)) return 0;
	return 1;
}

// ---- RtIntersectionBBoxTriangle 0x558220 (box = {sup, inf})
static inline int outcode(const float *p, const float *bb){
	int c = p[0] > bb[0] ? 1 : p[0] < bb[3] ? 2 : 0;
	c |= p[1] > bb[1] ? 4 : p[1] < bb[4] ? 8 : 0;
	c |= p[2] > bb[2] ? 0x10 : p[2] < bb[5] ? 0x20 : 0;
	return c;
}
// the edge crosses the plane of axis k (o = P - that plane's corner) inside the face rectangle (axes a, b); products compared, no divide
static inline int faceHit(const float *o, const float *S, const float *I, const float *d, int k, int a, int b){
	X P = o[k]*d[a], Q = S[a]*d[k], R = I[a]*d[k];
	X P2 = o[k]*d[b], Q2 = S[b]*d[k], R2 = I[b]*d[k];
	if(d[k] < 0.0f) return P < Q && P > R && P2 < Q2 && P2 > R2;
	return P > Q && P < R && P2 > Q2 && P2 < R2;
}
// edge P -> Q, bits = the outcode bits that differ between its ends
static int edgeHit(const float *P, const float *Q, const float *bb, int bits){
	float d[3] = {Q[0] - P[0], Q[1] - P[1], Q[2] - P[2]};
	float S[3] = {P[0] - bb[0], P[1] - bb[1], P[2] - bb[2]}, I[3] = {P[0] - bb[3], P[1] - bb[4], P[2] - bb[5]};
	return ((bits & 1) && faceHit(S, S, I, d, 0, 1, 2)) || ((bits & 2) && faceHit(I, S, I, d, 0, 1, 2))
		|| ((bits & 4) && faceHit(S, S, I, d, 1, 2, 0)) || ((bits & 8) && faceHit(I, S, I, d, 1, 2, 0))
		|| ((bits & 0x10) && faceHit(S, S, I, d, 2, 0, 1)) || ((bits & 0x20) && faceHit(I, S, I, d, 2, 0, 1));
}
// L1:exact 0x558220: outcodes, then the triangle edges against the box faces, then the box diagonal (chosen by the normal's
// signs) against the triangle
RWAPI RwBool RtIntersectionBBoxTriangle(const RwBBox *bbox, const RwV3d *v0, const RwV3d *v1, const RwV3d *v2){
	const float *bb = (const float *)bbox, *p0 = (const float *)v0, *p1 = (const float *)v1, *p2 = (const float *)v2;
	int o0 = outcode(p0, bb); if(!o0) return 1;
	int o1 = outcode(p1, bb); if(!o1) return 1;
	int o2 = outcode(p2, bb); if(!o2) return 1;
	if(o0 & o1 & o2) return 0;
	if(!(o0 & o1) && edgeHit(p0, p1, bb, o0 ^ o1)) return 1;
	if(!(o0 & o2) && edgeHit(p0, p2, bb, o0 ^ o2)) return 1;
	if(!(o1 & o2) && edgeHit(p1, p2, bb, o1 ^ o2)) return 1;
	float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
	float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], 0}; X e2z = p2[2] - p0[2]; e2[2] = e2z;
	float nx = e2z*e1[1] - e2[1]*e1[2];
	float ny = e2[0]*e1[2] - e2[2]*e1[0];
	float nz = e2[1]*e1[0] - e2[0]*e1[1];
	float D[3], K[3];		// box diagonal and its start corner (relative to v0)
	D[0] = bb[0] - bb[3]; K[0] = bb[3] - p0[0];
	if((int32_t)(SB(ny) ^ SB(nx)) < 0){ D[1] = bb[4] - bb[1]; K[1] = bb[1] - p0[1]; } else { D[1] = bb[1] - bb[4]; K[1] = bb[4] - p0[1]; }
	if((int32_t)(SB(nz) ^ SB(nx)) < 0){ D[2] = bb[5] - bb[2]; K[2] = bb[2] - p0[2]; } else { D[2] = bb[2] - bb[5]; K[2] = bb[5] - p0[2]; }
	X dn = D[1]*ny + D[0]*nx; dn = dn + D[2]*nz;
	float L = -dn;
	X s = K[1]*ny + K[0]*nx; s = s + K[2]*nz;
	float m0 = D[2]*K[1] - K[2]*D[1], m1 = K[2]*D[0] - D[2]*K[0], m2 = D[1]*K[0] - K[1]*D[0];
	if(L < 0.0f){
		if(!(s < 0.0f) || !(s > L)) return 0;
		X tt = m1*e2[1] + m0*e2[0]; tt = tt + m2*e2[2]; float t = tt;
		if(!(t < 0.0f) || !(t > L)) return 0;
		X u = m1*e1[1] + m0*e1[0]; u = u + m2*e1[2]; u = -u;
		if(!(u < 0.0f)) return 0;
		return t + u > L;
	}
	if(!(s > 0.0f) || !(s < L)) return 0;
	X tt = m1*e2[1] + m0*e2[0]; tt = tt + m2*e2[2]; float t = tt;
	if(!(t > 0.0f) || !(t < L)) return 0;
	X u = m1*e1[1] + m0*e1[0]; u = u + m2*e1[2]; u = -u;
	if(!(u > 0.0f)) return 0;
	return t + u < L;
}

}
