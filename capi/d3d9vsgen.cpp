// D3D9VSGEN (group d3d9vsgen, lane K3b window 8): rpD3D9GenerateVertexShader 0x509870 of MFL.exe (rpworld d3d9vertexshader), written from the disassembly.
// The generator writes D3D9 vertex shader assembly text into the exe's source buffer (0x7d68e0) through the engine's sprintf (RwEngineInstance + 0xc4),
// assembles it with D3DXAssembleShader (0x50cf85, D3DX code linked into MFL.exe) and creates the shader with RwD3D9CreateVertexShader (0x4dbc10).
// The text is built line for line in the original's order, so the assembled token stream is the original's.
// desc (4 bytes):  [0] lo = directional lights, hi = point lights;  [1] lo = spot lights, hi = texcoord sets;
//                  [2] bit0 morph, bits1-3 skin weights, bit4 vertex colour, bit5 normals, bit6 normalise, bit7 modulate;  [3] bits0-1 fog, bits2-7 effect.
// flags (8 bytes, out): constant register bases: [0] modulate colour, [1] fog, [2] effect, [3] morph weights, [4] skin matrices (0xff = unused).
// Exe binding: mflrw_d3d9vsgen_bind(table), table order = enum V below.  Unbound: abort.  Check: tools/delink/rw_d3d9vsgen_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;

enum V { V_ENGINE, V_TEXT, V_NUMEFFECT, V_USEVER, V_MAXCONST, V_CAPS, V_ASSEMBLE, V_CREATE, V_N };
static void *g_v[V_N];
static void *fn(int i){
	if(!g_v[V_N - 1]){ fprintf(stderr, "[mflrw] d3d9vsgen called before mflrw_d3d9vsgen_bind\n"); abort(); }
	return g_v[i];
}
#define VAR(i) (*(u32 *)fn(i))
typedef int (__cdecl *SprintfFn)(char *dst, const char *fmt, ...);		// RwEngineInstance->stringFuncs sprintf (+0xc4)
typedef i32 (__stdcall *AssembleFn)(const char *src, u32 len, const void *defines, void *include, u32 flags, void **shader, void **errors);
typedef void (__cdecl *CreateFn)(const void *function, u32 *shader);		// RwD3D9CreateVertexShader
static SprintfFn rwsprintf(void){ return *(SprintfFn *)(uintptr_t)(VAR(V_ENGINE) + 0xc4); }	// re-read before every line, as the original does
#define E(...) (g.p += rwsprintf()(g.p, __VA_ARGS__))

// Swizzle table at 0x5ec3e8: the bytes "xyzw" followed by char pointers.  Byte reads past index 3 (skin weights 5..7) see the low bytes of the first pointer
// (0x0062efe8): exact copies, so the text matches for every descriptor.  k_mask[n] = the n-component write mask (dword at 0x5ec3e8 + 4n; n >= 1),
// k_splat[i] = the replicated component (0x5ec3fc + 4i).
static const signed char k_swz[8] = {'x', 'y', 'z', 'w', (signed char)0xe8, (signed char)0xef, 0x62, 0};
static const char *const k_mask[8] = {0, "x", "xy", "xyz", "xyzw", "xxx", "yyy", "zzz"};
static const char *const k_splat[4] = {"xxx", "yyy", "zzz", "www"};

struct Gen {
	char *p;			// write position in the text buffer
	const u8 *d;			// descriptor
	u8 *fl;				// flag bytes
	u32 normal, tangent, normalise, modulate;
	u32 nw;				// skin weights (0..7)
	u32 eff, fog;
	u32 reg;			// first constant register after the light / flag allocation
	u32 pointBase, spotBase;	// first constants of the point / spot light blocks
	const char *pos, *nrm;		// current position / normal source registers (nrm may be NULL)
};

static u32 min4(u32 n){ return n < 4 ? n : 4; }

// Declarations (0x509a51..0x509bec).
static void emitDecls(Gen &g){
	E("dcl_position0 v0\n");
	if(g.nw){
		if(g.nw > 1) E("dcl_blendweight0 v1\n");
		E("dcl_blendindices0 v2\n");
	}
	if(g.d[2] & 0x10) E("dcl_color0 v4\n");
	if(g.normal) E("dcl_normal0 v3\n");
	if(g.tangent) E("dcl_tangent0 v13\n");
	switch(g.eff){							// jump table 0x50b498 / index bytes 0x50b4a4
	case 1: case 2: case 3: case 5: case 6: case 10: case 11:
		if(g.d[1] & 0xf0) E("dcl_texcoord0 v5\n");
		break;
	case 4: case 7:
		if((g.d[1] & 0xf0) > 0x10) E("dcl_texcoord1 v6\n");
		break;
	default:							// 0, 8, 9, 12..63
		for(u32 i = 0; i < (u32)(g.d[1] >> 4); i++) E("dcl_texcoord%u v%u\n", i, i + 5);
	}
	if(g.d[2] & 1){
		E("dcl_position1 v14\n");
		if(g.normal) E("dcl_normal1 v15\n");
	}
}

// Position: morph blend (v0/v14, v3/v15), then matrix palette skinning (0x509bec..0x50a110).
static void emitPosition(Gen &g){
	g.pos = "v0";
	g.nrm = g.normal ? "v3" : 0;
	if(!(g.d[2] & 1) && !g.nw){
		E("m4x4 oPos.xyzw, v0, c[0]\n");
		return;
	}
	if(g.d[2] & 1){
		u32 c = g.fl[3];
		if(!g.nw){
			E("mul r3, v0, c[%d].x\n", c);
			E("mad r3, v14, c[%d].y, r3\n", c);
			g.pos = "r3";
			if(g.normal){
				E("mul r5, v3, c[%d].x\n", c);
				E("mad r5, v15, c[%d].y, r5\n", c);
				g.nrm = "r5";
			}
			goto out;
		}
		E("mul r2, v0, c[%d].x\n", c);
		E("mad r2, v14, c[%d].y, r2\n", c);
		g.pos = "r2";
		if(g.normal){
			E("mul r4, v3, c[%d].x\n", c);
			E("mad r4, v15, c[%d].y, r4\n", c);
			g.nrm = "r4";
		}
	}
	if(g.nw == 1){
		E(VAR(V_USEVER) == 0xa ? "mov a0.x, v2.x\n" : "mova a0.x, v2.x\n");
		E("m4x3 r3.xyz, %s, c[a0.x + %d]\n", g.pos, g.reg);
		if(g.normal) E("m3x3 r5.xyz, %s, c[a0.x + %d]\n", g.nrm, g.reg);
	}else if(g.normal){							// blend the matrices into r8..r10 first
		if(VAR(V_USEVER) >= 0x14){
			E("mova a0.%s, v2\n", k_mask[g.nw]);
			for(u32 i = 0; i < g.nw; i++){
				int s = k_swz[i];
				if(i){
					E("mad r8, v1.%c, c[a0.%c + %d], r8\n", s, s, g.reg);
					E("mad r9, v1.%c, c[a0.%c + %d], r9\n", s, s, g.reg + 1);
					E("mad r10, v1.%c, c[a0.%c + %d], r10\n", s, s, g.reg + 2);
				}else{
					E("mul r8, v1.%c, c[a0.%c + %d]\n", s, s, g.reg);
					E("mul r9, v1.%c, c[a0.%c + %d]\n", s, s, g.reg + 1);
					E("mul r10, v1.%c, c[a0.%c + %d]\n", s, s, g.reg + 2);
				}
			}
		}else{
			for(u32 i = 0; i < g.nw; i++){
				int s = k_swz[i];
				E("mov a0.x, v2.%c\n", s);
				if(i){
					E("mad r8, v1.%c, c[a0.x + %d], r8\n", s, g.reg);
					E("mad r9, v1.%c, c[a0.x + %d], r9\n", s, g.reg + 1);
					E("mad r10, v1.%c, c[a0.x + %d], r10\n", s, g.reg + 2);
				}else{
					E("mul r8, v1.%c, c[a0.x + %d]\n", s, g.reg);
					E("mul r9, v1.%c, c[a0.x + %d]\n", s, g.reg + 1);
					E("mul r10, v1.%c, c[a0.x + %d]\n", s, g.reg + 2);
				}
			}
		}
		E("m4x3 r3.xyz, %s, r8\n", g.pos);
		E("m3x3 r5.xyz, %s, r8\n", g.nrm);
	}else{									// no normals: blend the transformed positions
		u32 v2 = VAR(V_USEVER) >= 0x14;
		if(v2) E("mova a0.%s, v2\n", k_mask[g.nw]);
		for(u32 i = 0; i < g.nw; i++){
			int s = k_swz[i];
			if(v2) E("m4x3 r5.xyz, %s, c[a0.%c + %d]\n", g.pos, s, g.reg);
			else{
				E("mov a0.x, v2.%c\n", s);
				E("m4x3 r5.xyz, %s, c[a0.x + %d]\n", g.pos, g.reg);
			}
			if(i) E("mad r3.xyz, v1.%c, r5.xyz, r3.xyz\n", s);
			else E("mul r3.xyz, v1.%c, r5.xyz\n", s);
		}
	}
	g.pos = "r3";
	if(g.normal) g.nrm = "r5";
out:
	E("mov r3.w, v0.w\n");
	E("m4x4 oPos.xyzw, r3, c[0]\n");
}

// Fog (0x50a110..0x50a23d): WFOG devices take the depth directly; else linear / exp / exp2 from r0.w with the constants at flags[1].
static void emitFog(Gen &g){
	if((i32)g.fog <= 0) return;
	if(((u32 *)fn(V_CAPS))[0x24 / 4] & 0x100000){				// _RwD3D9DeviceCaps.RasterCaps & D3DPRASTERCAPS_WFOG
		E("dp4 oFog, %s, c[3]\n", g.pos);
		return;
	}
	u32 c = g.fl[1];
	E("dp4 r0.w, %s, c[3]\n", g.pos);
	switch(g.fog){
	case 1:
		E("mad oFog, r0.w, c[%d].x, c[%d].y\n", c, c);
		break;
	case 2:
		E("mul r0.w, r0.w, c[%d].x\n", c);
		E("expp r0.w, r0.w\n");
		E("rcp oFog, r0.w\n");
		break;
	case 3:
		E("mul r0.w, r0.w, c[%d].x\n", c);
		E("mul r0.w, r0.w, r0.w\n");
		E("expp r0.w, r0.w\n");
		E("rcp oFog, r0.w\n");
		break;
	}
}

// One light's diffuse term: the last light writes oD0 directly unless the result is modulated (0x50a3a8 / 0x50a56c / 0x50a7bd).
static void lightOut(Gen &g, u32 &left, u32 c, u32 i){
	if(left <= 1 && !g.modulate) E("mad oD0, c[%d], r6.%c, r11\n", c, (int)k_swz[i]);
	else E("mad r11.xyz, c[%d].xyz, r6.%s, r11.xyz\n", c, k_splat[i]);
	left--;
}

// Vertex lighting (0x50a2a3..0x50a8fe): ambient (+ vertex colour) in r11, then directional, point and spot lights in groups of four.
static void emitLighting(Gen &g){
	const u8 *d = g.d;
	u32 left = (d[0] & 0xf) + (d[1] & 0xf) + (d[0] >> 4);
	if(!g.normal || !left){
		if(g.modulate){
			E(d[2] & 0x10 ? "add r11, v4, c[4]\n" : "mov r11, c[4]\n");
			E("mul oD0, r11, c[%d]\n", g.fl[0]);
		}else E(d[2] & 0x10 ? "add oD0, v4, c[4]\n" : "mov oD0, c[4]\n");
		return;
	}
	E(d[2] & 0x10 ? "add r11, v4, c[4]\n" : "mov r11, c[4]\n");
	if(u32 n = d[0] & 0xf){							// directional: c[6 + 8k ...], direction / colour pairs
		u32 c0 = 6;
		for(u32 base = 0; base < n; base += 4, c0 += 8){
			u32 cnt = min4(n - base);
			for(u32 i = 0; i < cnt; i++) E("dp3 r6.%c, c[%d], %s\n", (int)k_swz[i], c0 - 1 + 2 * i, g.nrm);
			E("max r6.%s, c[5].w, -r6.%s\n", k_mask[cnt], k_mask[cnt]);
			for(u32 i = 0; i < cnt; i++) lightOut(g, left, c0 + 2 * i, i);
		}
	}
	if(u32 n = d[0] >> 4){							// point: position / colour pairs plus one range constant per group
		for(u32 base = 0; base < n; base += 4){
			u32 cnt = min4(n - base), pb = g.pointBase;
			for(u32 i = 0; i < cnt; i++){
				int s = k_swz[i];
				E("add r7.xyz, -c[%d].xyz, %s.xyz\n", pb + 1 + 2 * i, g.pos);
				E("dp3 r1.%c, r7, r7\n", s);
				E("rsq r1.%c, r1.%c\n", s, s);
				E("dp3 r6.%c, %s, r7\n", s, g.nrm);
			}
			const char *m = k_mask[cnt];
			E("max r8.%s, c[%d].%s, r1.%s\n", m, pb, m, m);
			E("max r6.%s, c[%d].wwww, -r6.%s\n", m, pb + 1, m);
			E("add r8.%s, -c[%d].%s, r8.%s\n", m, pb, m, m);
			E("mul r6.%s, r6.%s, r8.%s\n", m, m, m);
			for(u32 i = 0; i < cnt; i++) lightOut(g, left, pb + 2 + 2 * i, i);
			g.pointBase = pb + cnt * 2 + 1;
		}
	}
	if(u32 n = d[1] & 0xf){							// spot: four constants per light plus one per group
		for(u32 base = 0; base < n; base += 4){
			u32 cnt = min4(n - base), sb = g.spotBase;
			for(u32 i = 0; i < cnt; i++){
				int s = k_swz[i];
				u32 c = sb + 3 + 4 * i;
				E("add r7.xyz, -c[%d].xyz, %s.xyz\n", c - 2, g.pos);
				E("dp3 r1.%c, r7, r7\n", s);
				E("rsq r1.%c, r1.%c\n", s, s);
				E("dp3 r6.%c, %s, r7\n", s, g.nrm);
				E("mul r7.xyz, r7.xyz, r1.%c\n", s);
				E("dp3 r7.w, r7, c[%d]\n", c - 1);
				E("mad r9.%c, r7.w, c[%d].x, c[%d].y\n", s, c, c);
			}
			const char *m = k_mask[cnt];
			E("max r8.%s, c[%d].%s, r1.%s\n", m, sb, m, m);
			E("max r9.%s, c[%d].wwww, r9.%s\n", m, sb + 1, m);
			E("max r6.%s, c[%d].wwww, -r6.%s\n", m, sb + 1, m);
			E("add r8.%s, -c[%d].%s, r8.%s\n", m, sb, m, m);
			E("mul r6.%s, r6.%s, r8.%s\n", m, m, m);
			E("mul r6.%s, r6.%s, r9.%s\n", m, m, m);
			for(u32 i = 0; i < cnt; i++) lightOut(g, left, sb + 4 + 4 * i, i);
			g.spotBase = sb + cnt * 4 + 1;
		}
	}
	if(g.modulate) E("mul oD0, r11, c[%d]\n", g.fl[0]);
}

// Tangent space (effects 8 / 9; 0x50a8fe..0x50ad42): tangent r4, binormal r2, the first light's vector into tangent space in r1.
static void emitTangent(Gen &g){
	const u8 *d = g.d;
	if(g.nw){
		if(g.nw == 1) E("m3x3 r4.xyz, v13, c[a0.x + %d]\n", g.reg);
		else E("m3x3 r4.xyz, v13, r8\n");
		if(g.normalise){
			E("dp3 r4.w, r4, r4\n");
			E("rsq r4.w, r4.w\n");
			E("mul r4.xyz, r4, r4.w\n");
		}
		E("slt r4.w, v13.w, v13.w\n");
	}else E("mov r4, v13\n");
	if((d[2] & 1) || g.nw) E("mov %s.w, v3.w\n", g.nrm);
	E("mul r2, r4.zxyw, %s.yzxw\n", g.nrm);
	E("mad r2, r4.yzxw, %s.zxyw, -r2\n", g.nrm);
	if(g.normalise){
		E("dp3 r2.w, r2, r2\n");
		E("rsq r2.w, r2.w\n");
		E("mul r2.xyz, r2, r2.w\n");
	}
	if(d[2] & 0x10) E("mov oD0, v4\n");
	if(d[0] & 0xf){								// directional light 0
		E("dp3 r1.x, c[5], r4\n");
		E("dp3 r1.y, c[5], r2\n");
		E("dp3 r1.z, c[5], %s\n", g.nrm);
		E("dp3 r1.w, r1, r1\n");
		E("rsq r1.w, r1.w\n");
		E("mul r1.xyz, r1, r1.w\n");
		if(g.eff != 9) E("mad oT2.xyz, r1, c[5].w, c[5].w\n");
		else E(d[2] & 0x10 ? "mad oD1.xyz, r1, c[5].w, c[5].w\n" : "mad oD0.xyz, r1, c[5].w, c[5].w\n");
	}else if(d[0] & 0xf0 || d[1] & 0xf){					// point light 0, or spot light 0 (adds the cone term r9.w)
		u32 spot = !(d[0] & 0xf0);
		E("add r7.xyz, c[1 + 5].xyz, -%s.xyz\n", g.pos);
		E("dp3 r7.w, r7, r7\n");
		E("rsq r7.w, r7.w\n");
		E("mul r7.xyz, r7, r7.w\n");
		if(spot){
			E("dp3 r9.w, r7, c[2 + 5]\n");
			E("mad r9.w, r9.w, c[3 + 5].x, c[3 + 5].y\n");
		}
		E("rcp r7.w, r7.w\n");
		E("mad r7.w, r7.w, c[5].x, c[5].y\n");
		E("max r7.w, r7.w, c[5].w\n");
		if(spot) E("max r9.w, r9.w, c[5].w\n");
		E("dp3 r1.x, r7, r4\n");
		E("dp3 r1.y, r7, r2\n");
		E("dp3 r1.z, r7, %s\n", g.nrm);
		E("dp3 r1.w, r1, r1\n");
		E("rsq r1.w, r1.w\n");
		E("mul r1.xyz, r1, r1.w\n");
		E("mul r1.xyz, r1, r7.w\n");
		if(spot) E("mul r1.xyz, r1, r9.w\n");
		if(g.eff != 9) E("mad oT2.xyz, r1, c[5].z, c[5].z\n");
		else E(d[2] & 0x10 ? "mad oD1.xyz, r1, c[5].z, c[5].z\n" : "mad oD0.xyz, r1, c[5].z, c[5].z\n");
	}
	if(g.eff == 9){								// tangent-to-object matrix rows into oT2 / oT3
		u32 c = g.fl[2];
		E("dp3 oT2.x, c[%d], r4\n", c);
		E("dp3 oT2.y, c[%d], r2\n", c);
		E("dp3 oT2.z, c[%d], %s\n", c, g.nrm);
		E("dp3 oT3.x, c[%d], r4\n", c + 1);
		E("dp3 oT3.y, c[%d], r2\n", c + 1);
		E("dp3 oT3.z, c[%d], %s\n", c + 1, g.nrm);
	}
	E("mov oT0.xy, v%d\n", (d[1] >> 4) + 4);
	E("mov oT1.xy, v5\n");
}

// Reflection vector r7 = 2 (e . n) n - e for the eye position c[c] (0x50b144 / 0x50b2f3).
static void emitReflect(Gen &g, u32 c){
	E("add r7.xyz, c[%d].xyz, -%s.xyz\n", c, g.pos);
	E("dp3 r7.w, r7, r7\n");
	E("rsq r7.w, r7.w\n");
	E("mul r7.xyz, r7, r7.w\n");
	E("dp3 r7.w, r7, %s\n", g.nrm);
	E("add r7.w, r7.w, r7.w\n");
	E("mad r7, r7.w, %s, -r7\n", g.nrm);
}

// Environment-map texcoords from the view-space normal (0x50b035).
static void emitEnvMap(Gen &g, u32 c){
	E("dp3 r9.x, %s, c[%d]\n", g.nrm, c);
	E("dp3 r9.y, %s, c[%d]\n", g.nrm, c + 1);
	E("mov oT0.xy, v5\n");
	E("add oT1.xy, r9.xy, v5.xy\n");
}

// Texture coordinates per effect (0x50ad44; jump table 0x50b4b0, constants from flags[2]).
static void emitEffect(Gen &g){
	u32 c = g.fl[2];
	switch(g.eff){
	case 1:
		emitEnvMap(g, c);
		break;
	case 2:
		if(g.d[1] & 0xf0){
			E("mov oT0.xy, v5\n");
			E("dp4 oT1.x, %s, c[%d]\n", g.nrm, c);
			E("dp4 oT1.y, %s, c[%d]\n", g.nrm, c + 1);
		}else{
			E("dp4 oT0.x, %s, c[%d]\n", g.nrm, c);
			E("dp4 oT0.y, %s, c[%d]\n", g.nrm, c + 1);
		}
		break;
	case 3:
		E("dp4 oT2.x, %s, c[%d]\n", g.nrm, c + 2);
		E("dp4 oT2.y, %s, c[%d]\n", g.nrm, c + 3);
		emitEnvMap(g, c);
		break;
	case 4:
		E("mov oT0.xy, v6\n");
		break;
	case 5:
		E("mov oT0.xy, v5\n");
		E("mov oT1.xy, v5\n");
		break;
	case 6:
		E("dp4 oT0.x, v5, c[%d]\n", c);
		E("dp4 oT0.y, v5, c[%d]\n", c + 1);
		break;
	case 7:
		E("dp4 oT0.x, v6, c[%d]\n", c);
		E("dp4 oT0.y, v6, c[%d]\n", c + 1);
		break;
	case 8: case 9:								// tangent space: texcoords written by emitTangent
		break;
	case 10:
		emitReflect(g, c);
		if(g.d[1] & 0xf0){
			E("mov oT0.xy, v5\n");
			E("dp3 oT1.x, r7, c[%d]\n", c + 1);
			E("dp3 oT1.y, r7, c[%d]\n", c + 2);
			E("dp3 oT1.z, r7, c[%d]\n", c + 3);
		}else{
			E("dp3 oT0.x, r7, c[%d]\n", c + 1);
			E("dp3 oT0.y, r7, c[%d]\n", c + 2);
			E("dp3 oT0.z, r7, c[%d]\n", c + 3);
		}
		break;
	case 11:
		emitEnvMap(g, c);
		emitReflect(g, c + 2);
		E("dp3 oT2.x, r7, c[%d]\n", c + 3);
		E("dp3 oT2.y, r7, c[%d]\n", c + 4);
		E("dp3 oT2.z, r7, c[%d]\n", c + 5);
		break;
	default:								// 0, 12..63: pass the texcoord sets through
		for(u32 i = 0; i < (u32)(g.d[1] >> 4); i++) E("mov oT%u.xy, v%u\n", i, i + 5);
	}
}

struct ID3DXBufferVtbl { void *qi, *addRef; u32 (__stdcall *Release)(void *self); void *(__stdcall *GetBufferPointer)(void *self); };
struct ID3DXBufferObj { const ID3DXBufferVtbl *vt; };

extern "C" {

void __cdecl mflrw_d3d9vsgen_bind(void *const *tbl){
	for(int i = 0; i < V_N; i++) g_v[i] = tbl[i];
}

// rpD3D9GenerateVertexShader 0x509870 (desc, flags): returns the created vertex shader (0 when the version is unknown or assembly fails).
u32 __cdecl rw37_rpD3D9GenerateVertexShader(const u8 *desc, u8 *flags){
	Gen g;
	g.d = desc;
	g.fl = flags;
	const u8 *d = desc;
	g.normalise = (d[2] >> 6) & 1;
	g.eff = d[3] >> 2;
	g.nw = (d[2] >> 1) & 7;
	g.modulate = d[2] >> 7;
	g.fog = d[3] & 3;
	g.normal = g.tangent = 0;
	g.pointBase = g.spotBase = 0xffffffffu;
	if(d[0] || (d[1] & 0xf) || g.eff == 1 || g.eff == 2 || g.eff == 3 || g.eff == 8 || g.eff == 9 || g.eff == 10 || g.eff == 11){
		if(d[2] & 0x20){
			g.normal = 1;
			if(g.eff == 8 || g.eff == 9) g.tangent = 1;
		}
	}
	((u32 *)flags)[0] = 0xffffffffu;
	((u32 *)flags)[1] = 0xffffffffu;
	// constant registers from c[5]: two per directional light, then the point and spot light blocks, then the flag-addressed constants
	u32 reg = 5;
	if(d[0] & 0xf) reg = (d[0] & 0xf) * 2 + 5;
	if(d[0] & 0xf0){
		u32 n = d[0] >> 4;
		g.pointBase = reg;
		reg = (n + 3) / 4 + reg + n * 2;
	}
	if(d[1] & 0xf){
		u32 n = d[1] & 0xf;
		g.spotBase = reg;
		reg = (n + 3) / 4 + reg + n * 4;
	}
	if(g.modulate) flags[0] = (u8)reg++;
	if((i32)g.fog > 0 && !(((u32 *)fn(V_CAPS))[0x24 / 4] & 0x100000)) flags[1] = (u8)reg++;
	if(d[2] & 1) flags[3] = (u8)reg++;
	if(g.nw) flags[4] = (u8)reg;
	if(g.eff){
		u32 k = ((u32 *)fn(V_NUMEFFECT))[g.eff];
		if(k){
			if(g.nw) flags[2] = (u8)((u8)VAR(V_MAXCONST) - (u8)k);
			else{ flags[2] = (u8)reg; reg += k; }
		}
	}
	g.reg = reg;
	char *text = (char *)fn(V_TEXT);
	g.p = text;
	switch(VAR(V_USEVER)){
	case 0xa: E("vs_1_1\n"); break;
	case 0x14: E("vs_2_0\n"); break;
	case 0x19: E("vs_2_x\n"); break;
	default: return 0;
	}
	emitDecls(g);
	emitPosition(g);
	emitFog(g);
	if(g.normal && g.normalise){
		E("dp3 r5.w, %s, %s\n", g.nrm, g.nrm);
		E("rsq r5.w, r5.w\n");
		E("mul r5.xyz, %s, r5.w\n", g.nrm);
		g.nrm = "r5";
	}
	if(g.tangent) emitTangent(g);
	else emitLighting(g);
	emitEffect(g);
	ID3DXBufferObj *code = 0;
	u32 shader = 0;
	if(((AssembleFn)fn(V_ASSEMBLE))(text, (u32)(g.p - text), 0, 0, 4, (void **)&code, 0) >= 0)
		((CreateFn)fn(V_CREATE))(code->vt->GetBufferPointer(code), &shader);
	if(code) code->vt->Release(code);
	return shader;
}

}
