// D3D9VS2 (group d3d9vs2, lane K4 window 8): the two default vertex shader render callbacks of MFL.exe (rpworld d3d9vertexshader), written op for op from the
// disassembly, on the exe's device, last-shader globals, engine (current world, light frame word at +0xa), world / sector / atomic light lists and these callees:
//   rxD3D9VertexShaderDefaultMeshRenderCallBack 0x50b610: pixel shader off, stage-0 texture + texture stage states, vertex shader, DrawIndexedPrimitive / DrawPrimitive
//   rxD3D9VertexShaderDefaultLightingCallBack 0x50b750: collects ambient / directional / point / spot lights (15 of each at most) of the world, and for an atomic the
//     local lights of its sectors that touch its bounding sphere, and writes the vertex shader light constants (local-space direction / position, colour, inverse
//     local radius, spot cone terms); counts go into the descriptor nibbles.  x87 parts (sphere test, ambient sums, radius inverse, cone terms incl. fcos) are inline
//     asm in the original's operand order.
// The three _rwD3D9VSGet*InLocalSpace rows (group d3d9vsu) are called through their exe addresses (the link turns them into the DLL thunks).
// Exe binding: mflrw_d3d9vs2_bind(table), table order = enum V below.  Unbound: abort.  Check: tools/delink/rw_d3d9vs2_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum V { V_LASTPS, V_LASTVS, V_DEVICE, V_SETTEX, V_SETTSS, V_FLUSH, V_ENGINE, V_GETLTM, V_BSPHERE, V_CONE, V_VSNORMAL, V_VSPOINT, V_VSRADIUS, V_N };
static void *g_v[V_N];
static void *fn(int i){
	if(!g_v[V_N - 1]){ fprintf(stderr, "[mflrw] d3d9vs2 called before mflrw_d3d9vs2_bind\n"); abort(); }
	return g_v[i];
}
#define VAR(i) (*(u32 *)fn(i))
#define B(p, o) (*(u8 *)(uintptr_t)((p) + (o)))
#define H(p, o) (*(u16 *)(uintptr_t)((p) + (o)))
#define D(p, o) (*(u32 *)(uintptr_t)((p) + (o)))
typedef u32 (__stdcall *Com1)(u32 dev, u32 a);								// SetPixelShader +0x1ac / SetVertexShader +0x170
typedef u32 (__stdcall *ComDP)(u32 dev, u32 type, u32 start, u32 count);					// DrawPrimitive +0x144
typedef u32 (__stdcall *ComDIP)(u32 dev, u32 type, u32 base, u32 minIdx, u32 nv, u32 start, u32 count);	// DrawIndexedPrimitive +0x148
typedef u32 (__cdecl *SetTexFn)(u32 tex, u32 stage);							// RwD3D9SetTexture 0x4e55d0
typedef u32 (__cdecl *SetTssFn)(u32 stage, u32 type, u32 value);					// RwD3D9SetTextureStageState 0x4e3c70
typedef u32 (__cdecl *VoidFn)(void);									// _rwD3D9RenderStateFlushCache 0x4e3b30
typedef u32 (__cdecl *PtrFn)(u32 p);									// RwFrameGetLTM 0x4d0ff0 / RpAtomicGetWorldBoundingSphere 0x4f6240
typedef long double (__cdecl *ConeFn)(u32 light);							// RpLightGetConeAngle 0x4f4a70 (float in st0, taken unrounded)
typedef void (__cdecl *VecFn)(u32 in, u32 out);								// _rwD3D9VSGetNormalInLocalSpace 0x50c1a0 / GetPointInLocalSpace 0x50c2e0
typedef void (__cdecl *RadFn)(float r, u32 out);							// _rwD3D9VSGetRadiusInLocalSpace 0x50c3d0
static const float k_one = 1.0f, k_0_999 = 0.999f;							// 0x5dd330, 0x5dfda8
static const u32 k_999_99994 = 0x4479ffffu;								// 0x5ec450

static void tss(u32 type, u32 value){ ((SetTssFn)fn(V_SETTSS))(0, type, value); }
static u32 dev(void){ return VAR(V_DEVICE); }
static u32 ltm(u32 light){ return ((PtrFn)fn(V_GETLTM))(D(light, 4)); }
// ambient: fld light colour; fadd out; fstp out (0x50b808 / 0x50b9ca)
static void ambientAdd(u32 out, u32 light){
	__asm__ volatile("flds 0x18(%1)\n\tfadds (%0)\n\tfstps (%0)\n\tflds 0x1c(%1)\n\tfadds 4(%0)\n\tfstps 4(%0)\n\tflds 0x20(%1)\n\tfadds 8(%0)\n\tfstps 8(%0)"
	                 : : "r"(out), "r"(light) : "memory", "st");
}
// 0x50b8db..0x50b91a: nonzero = the light's sphere does NOT reach the atomic's (r*r <= d*d, or unordered)
static u32 sphereMiss(u32 sph, u32 pos, u32 light){
	u32 sw;
	__asm__ volatile("flds (%1)\n\tfsubs (%2)\n\tflds 4(%1)\n\tfsubs 4(%2)\n\tflds 8(%1)\n\tfsubs 8(%2)\n\tflds 0x14(%3)\n\tfadds 12(%1)\n\t"
	                 "fld %%st(2)\n\tfmul %%st(3), %%st\n\tfld %%st(4)\n\tfmul %%st(5), %%st\n\tfaddp %%st, %%st(1)\n\t"
	                 "fld %%st(2)\n\tfmul %%st(3), %%st\n\tfaddp %%st, %%st(1)\n\tfld %%st(1)\n\tfmul %%st(2), %%st\n\t"
	                 "fcompp\n\tfstp %%st(0)\n\tfstp %%st(0)\n\tfnstsw %%ax\n\tfstp %%st(0)\n\tandl $0x4100, %%eax\n\tfstp %%st(0)"
	                 : "=a"(sw) : "r"(sph), "r"(pos), "r"(light)
	                 : "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)", "cc", "memory");
	return sw;
}
// light radius through fld / fstp (a signalling NaN comes out quiet) to the local-space radius; then *rp = 1 / *rp
static void radius(u32 light, u32 rp){
	float r;
	__asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(*(float *)(uintptr_t)(light + 0x14)));
	((RadFn)fn(V_VSRADIUS))(r, rp);
	__asm__ volatile("flds %1\n\tfdivs (%0)\n\tfstps (%0)" : : "r"(rp), "m"(k_one) : "memory", "st");
}
static void colour(u32 o, u32 light){
	D(o, 0) = D(light, 0x18); D(o, 4) = D(light, 0x1c); D(o, 0xc) = 0; D(o, 8) = D(light, 0x20);
}
// 0x50bc26..0x50bc64: cone terms from the cone angle (hard spot 0x81, or cos >= 0.999 / unordered: 999.99994 and -cos * 999.99994; else 1 / (1 - cos), -cos / (1 - cos))
static void cone(long double ang, int hard, u32 o){
	__asm__ volatile("fldt %0\n\tfcos\n\ttestl %1, %1\n\tjnz 1f\n\tfcoms %3\n\tfnstsw %%ax\n\ttestl $0x100, %%eax\n\tje 1f\n\t"
	                 "flds %4\n\tfsub %%st(1), %%st\n\tfdivrs %4\n\tfsts (%2)\n\tfxch %%st(1)\n\tfchs\n\tfmul %%st(1), %%st\n\tfstps 4(%2)\n\tfstp %%st(0)\n\tjmp 2f\n"
	                 "1:\n\tfchs\n\tfmuls %5\n\tmovl $0x4479ffff, (%2)\n\tfstps 4(%2)\n2:"
	                 : : "m"(ang), "r"(hard), "r"(o), "m"(k_0_999), "m"(k_one), "m"(k_999_99994)
	                 : "eax", "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)", "cc", "memory");
}

extern "C" {

void __cdecl mflrw_d3d9vs2_bind(void *const *tbl){
	for(int i = 0; i < V_N; i++) g_v[i] = tbl[i];
}

// rxD3D9VertexShaderDefaultMeshRenderCallBack 0x50b610 (resEntryHeader, instancedData): returns the draw call's HRESULT (eax of the original; callers ignore it)
u32 __cdecl rw37_rxD3D9VertexShaderDefaultMeshRenderCallBack(u32 hdr, u32 inst){
	if(VAR(V_LASTPS)){
		u32 d = dev();
		VAR(V_LASTPS) = 0;
		((Com1)D(D(d, 0), 0x1ac))(d, 0);
	}
	u32 tex = D(D(inst, 8), 0);						// material -> texture
	if(tex){
		((SetTexFn)fn(V_SETTEX))(tex, 0);
		tss(1, 4); tss(2, 2); tss(3, 0); tss(4, 4); tss(5, 2); tss(6, 0);
	}else{
		((SetTexFn)fn(V_SETTEX))(0, 0);
		tss(1, 3); tss(3, 0); tss(4, 3); tss(6, 0);
	}
	if(VAR(V_LASTVS) != D(inst, 0x10)){
		VAR(V_LASTVS) = D(inst, 0x10);
		u32 d = dev();
		((Com1)D(D(d, 0), 0x170))(d, D(inst, 0x10));
	}
	if(D(hdr, 8)){								// index buffer
		((VoidFn)fn(V_FLUSH))();
		u32 d = dev();
		return ((ComDIP)D(D(d, 0), 0x148))(d, D(hdr, 0xc), D(inst, 0x14), 0, D(inst, 0x18), D(inst, 0x1c), D(inst, 0x20));
	}
	((VoidFn)fn(V_FLUSH))();
	u32 d = dev();
	return ((ComDP)D(D(d, 0), 0x144))(d, D(hdr, 0xc), D(inst, 0x14), D(inst, 0x20));
}

// rxD3D9VertexShaderDefaultLightingCallBack 0x50b750 (object, type: 1 = atomic else world sector, shaderConstantPtr, descriptor): returns the constant pointer
// after the written constants
u32 __cdecl rw37_rxD3D9VertexShaderDefaultLightingCallBack(u32 obj, u32 type, u32 out, u8 *desc){
	u8 c1 = desc[1];
	desc[0] = 0; desc[1] = c1 & 0xf0;
	u32 geo = type == 1 ? D(obj, 0x18) : D(VAR(V_ENGINE), 4);		// atomic -> geometry, else the current world
	u32 flags = D(geo, 8);
	u32 esi = out;
	D(esi, 0) = 0; D(esi, 4) = 0; D(esi, 8) = 0;
	D(esi, 0xc) = flags & 8 ? 0 : 0x3f800000u;
	if(!(flags & 0x20)) return esi + 0x10;
	u32 world = D(VAR(V_ENGINE), 4);
	if(!world) return esi + 0x10;
	u32 dir[15], pnt[15], spt[15], nd = 0, np = 0, ns = 0, found = 0;
	if(type == 1){
		u32 normals = (flags >> 4) & 1;
		for(u32 l = D(world, 0x3c); l != world + 0x3c; l = D(l, 0)){	// world directional / ambient lights (link at light + 0x34)
			u32 light = l - 0x34;
			if(!light || !(B(light, 2) & 1)) continue;		// rpLIGHTLIGHTATOMICS
			u8 t = B(light, 1);
			if(t == 1){ if(normals && nd < 15){ dir[nd++] = light; found = 1; } }
			else if(t == 2) ambientAdd(esi, light);
		}
		if(normals){
			H(VAR(V_ENGINE), 0xa)++;
			for(u32 tie = D(obj, 0x64); tie != obj + 0x64; tie = D(tie, 0)){		// the atomic's sectors
				u32 sec = D(tie, 8);
				for(u32 e = D(sec, 0x40); e != sec + 0x40; e = D(e, 0)){	// the sector's local lights
					u32 light = D(e, 8);
					if(!light || !(B(light, 2) & 1)) continue;
					u16 fr = H(VAR(V_ENGINE), 0xa);
					if(H(light, 0x3c) == fr) continue;
					H(light, 0x3c) = fr;
					u32 pos = ltm(light) + 0x30;
					u32 sph = ((PtrFn)fn(V_BSPHERE))(obj);
					if(sphereMiss(sph, pos, light)) continue;
					u8 t = B(light, 1);
					if(t == 0x80){ if(np < 15){ pnt[np++] = light; found = 1; } }
					else if(t == 0x81 || t == 0x82){ if(ns < 15){ spt[ns++] = light; found = 1; } }
				}
			}
		}
	}else{
		for(u32 l = D(world, 0x3c); l != world + 0x3c; l = D(l, 0)){
			u32 light = l - 0x34;
			if(!light || !(B(light, 2) & 2)) continue;		// rpLIGHTLIGHTWORLD
			u8 t = B(light, 1);
			if(t == 1){ if(nd < 15){ dir[nd++] = light; found = 1; } }
			else if(t == 2) ambientAdd(esi, light);
		}
		H(VAR(V_ENGINE), 0xa)++;
		for(u32 e = D(obj, 0x40); e != obj + 0x40; e = D(e, 0)){		// the sector's local lights
			u32 light = D(e, 8);
			if(!light || !(B(light, 2) & 2)) continue;
			u16 fr = H(VAR(V_ENGINE), 0xa);
			if(H(light, 0x3c) == fr) continue;
			H(light, 0x3c) = fr;
			u8 t = B(light, 1);
			if(t == 0x80){ if(np < 15){ pnt[np++] = light; found = 1; } }
			else if(t == 0x81 || t == 0x82){ if(ns < 15){ spt[ns++] = light; found = 1; } }
		}
	}
	esi += 0x10;
	if(!found) return esi;
	if(nd){
		for(u32 i = 0; i < nd; i++){
			u32 light = dir[i];
			((VecFn)fn(V_VSNORMAL))(ltm(light) + 0x20, esi);
			D(esi, 0xc) = 0; esi += 0x10;
			colour(esi, light); esi += 0x10;
		}
		desc[0] = (desc[0] & 0xf0) | (nd & 0xf);
	}
	u32 rp = 0;
	if(np){
		for(u32 i = 0; i < np; i++){
			u32 light = pnt[i];
			if(!(i & 3)){ rp = esi; esi += 0x10; }			// one vec4 of inverse radii per 4 lights
			((VecFn)fn(V_VSPOINT))(ltm(light) + 0x30, esi);
			D(esi, 0xc) = 0; esi += 0x10;
			colour(esi, light); esi += 0x10;
			radius(light, rp); rp += 4;
		}
		desc[0] = (desc[0] & 0xf) | (u8)(np << 4);
	}
	if(!ns) return esi;
	for(u32 i = 0; i < ns; i++){
		u32 light = spt[i];
		if(!(i & 3)){ rp = esi; esi += 0x10; }
		((VecFn)fn(V_VSPOINT))(ltm(light) + 0x30, esi);
		D(esi, 0xc) = 0; esi += 0x10;
		((VecFn)fn(V_VSNORMAL))(ltm(light) + 0x20, esi);
		D(esi, 0xc) = 0; esi += 0x10;
		long double a = ((ConeFn)fn(V_CONE))(light);
		cone(a, B(light, 1) == 0x81, esi);
		D(esi, 8) = 0x3f000000u; D(esi, 0xc) = 0; esi += 0x10;
		colour(esi, light); esi += 0x10;
		radius(light, rp); rp += 4;
	}
	desc[1] = (desc[1] & 0xf0) | (ns & 0xf);
	return esi;
}

}
