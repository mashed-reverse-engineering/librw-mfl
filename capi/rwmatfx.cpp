// MATFX (group effectpipesd3d9, lane K round 2 step 0): the device free rows of the rpmatfx / D3D9 effect pipe code of MFL.exe, written op for op from the disassembly.
//   FUN_00554140 0x554140 (user data array add), FUN_00554120 0x554120 / FUN_00554320 0x554320 (its geometry / world sector wrappers),
//   rpMatFXSetupBumpMapTexture 0x5540c0, D3D9AtomicMatFXInstanceCallback 0x553ee0, D3D9WorldSectorMatFXInstanceCallback 0x553f60.
// Exe binding: mflrw_matfx_bind(&RwEngineInstance, table), table order = enum M below.  Unbound: abort.  Check: tools/delink/rw_matfx_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

enum M { M_UDGEOMOFF, M_UDSECTOROFF, M_MATOFF, M_TANGENTS, M_DEFATOMIC, M_DEFSECTOR, M_GEOMGET, M_GEOMSET, M_SECTORGET, M_SECTORSET, M_ISCOMPRESSED, M_MASKCREATE,
         M_CLIPPING, M_GETRS, M_STATE55D0, M_SETRS, M_SETTSS, M_SETSTREAMS, M_VERTEXALPHA, M_SURFPROPS, M_BUMPRENDER, M_ENVRENDER, M_DUALRENDER, M_DEFRENDER, M_BLACKRENDER,
         M_LASTIB, M_LASTDECL, M_LASTFVF, M_DEVICE,
         M_SETTRANSFORM, M_FLUSHCACHE, M_LASTPS, M_LASTVS, M_MATSCRATCH,
         M_CUBE, M_GETLTM, M_MATINVERT, M_MATMUL, M_IDMATRIX,
         M_PIPECREATE, M_PIPELOCK, M_FRAGATOMIC, M_FRAGSECTOR, M_ADDFRAG, M_PIPEUNLOCK, M_PIPEDESTROY, M_FINDNODE, M_GETINSTCB, M_SETINSTCB, M_SETRENDERCB, M_GETCAPS, M_CREATEPS,
         M_PIPEATOMIC, M_PIPESECTOR, M_MAXSTAGES, M_MULTITEX, M_MULADD, M_MOD2X, M_DIFFUSE0, M_VALID2X, M_VALIDDIFF, M_PSBASE, M_CBINSTATOMIC, M_CBINSTSECTOR, M_CBRENDER,
         M_PSDATA0, M_PSDATA1, M_PSDATA2, M_PSDATA3, M_PSDATA4, M_PSDATA5,
         M_TEXHASALPHA, M_DYNLOCK, M_DYNUNLOCK, M_CALCUV, M_SETSTREAMSOURCE, M_FUN4E5950, M_SRCBLEND, M_DESTBLEND, M_CONVNONPAL, M_N };
static u8 **g_engine;
static void *g_m[M_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] matfx called before mflrw_matfx_bind\n"); abort(); }
	return *g_engine;
}
#define ENG(off, T) (*(T *)(eng() + (off)))
typedef void *(__cdecl *MallocFn)(u32 n, u32 hint);
typedef void (__cdecl *FreeFn)(void *p);
typedef u32 (__cdecl *StrLenFn)(const char *s);
typedef char *(__cdecl *StrCpyFn)(char *d, const char *s);
typedef u32 (__cdecl *Get1Fn)(void *obj);
typedef void (__cdecl *Set2Fn)(void *obj, u32 flags);
typedef int (__cdecl *IsCompressedFn)(void *raster);
typedef void *(__cdecl *Mask2Fn)(void *a, void *b);
typedef void *(__cdecl *InstFn)(void *a, void *b, void *c);

extern "C" {

void __cdecl mflrw_matfx_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < M_N; i++) g_m[i] = tbl[i];
}

// FUN_00554140 0x554140: adds a user data array {name, format, numElements, data} (16 byte entries behind sect = {count, entries}) in the first free entry (data == NULL) or a
// new one (the entry table grows by one); data = zeroed elements of 4 bytes for formats 1..3 (0 otherwise); returns the entry index, -1 when memory runs out.
int32_t __cdecl rw37_UserDataArrayAdd(int32_t *sect, const char *name, int32_t format, int32_t numElements){
	int32_t idx = 0, n = sect[0];
	u8 *entries;
	if(n > 0){
		u32 *p = (u32 *)((u8 *)sect[1] + 0xc);
		while(*p != 0){
			idx++;
			p += 4;
			if(idx >= n) goto grow;
		}
		goto found;
	}
grow:
	{
		u32 bytes = (u32)(n + 1) << 4;
		if(sect[1]){
			void *nw = ENG(0x108, MallocFn)(bytes, 0x3011f);
			if(!nw) return -1;
			memcpy(nw, (void *)sect[1], (u32)sect[0] << 4);
			ENG(0x10c, FreeFn)((void *)sect[1]);
			sect[1] = (int32_t)(uintptr_t)nw;
		}else{
			void *nw = ENG(0x108, MallocFn)(bytes, 0x3011f);
			sect[1] = (int32_t)(uintptr_t)nw;
			if(!nw) return -1;
		}
		idx = sect[0];
		sect[0] = idx + 1;
	}
found:
	entries = (u8 *)sect[1];
	{
		u32 *e = (u32 *)(entries + ((u32)idx << 4));
		u32 elem = (format == 1 || format == 2 || format == 3) ? 4 : 0;
		void *data = ENG(0x108, MallocFn)(elem * (u32)numElements, 0x3011f);
		e[3] = (u32)(uintptr_t)data;
		if(!data) return -1;
		memset(data, 0, elem * (u32)numElements);
		e[0] = 0;
		if(name){
			u8 *engine = eng();
			MallocFn mal = *(MallocFn *)(engine + 0x108);
			char *copy = (char *)mal(((StrLenFn)*(void **)(engine + 0xf4))(name) + 1, 0x30002);
			e[0] = (u32)(uintptr_t)copy;
			if(copy) ENG(0xcc, StrCpyFn)(copy, name);
		}
		e[1] = (u32)format;
		e[2] = (u32)numElements;
	}
	return idx;
}

// FUN_00554120 0x554120 / FUN_00554320 0x554320: the geometry / world sector user data section lives at plugin offset userDataGeometryOffset / userDataWorldSectorOffset
int32_t __cdecl rw37_UserDataGeometryArrayAdd(int32_t *obj, const char *name, int32_t format, int32_t n){
	return rw37_UserDataArrayAdd((int32_t *)((u8 *)obj + *(int32_t *)g_m[M_UDGEOMOFF]), name, format, n);
}
int32_t __cdecl rw37_UserDataWorldSectorArrayAdd(int32_t *obj, const char *name, int32_t format, int32_t n){
	return rw37_UserDataArrayAdd((int32_t *)((u8 *)obj + *(int32_t *)g_m[M_UDSECTOROFF]), name, format, n);
}

// rpMatFXSetupBumpMapTexture 0x5540c0 (texture, bump texture): neither may have a compressed raster, then _rpMatFXTextureMaskCreate; a NULL bump texture returns the texture
void *__cdecl rw37_rpMatFXSetupBumpMapTexture(void **tex, void **bump){
	if(!bump) return tex;
	if(tex){
		if(((IsCompressedFn)g_m[M_ISCOMPRESSED])(*tex)) return NULL;
		if(((IsCompressedFn)g_m[M_ISCOMPRESSED])(*bump)) return NULL;
	}else if(((IsCompressedFn)g_m[M_ISCOMPRESSED])(*bump)) return NULL;
	return ((Mask2Fn)g_m[M_MASKCREATE])(tex, bump);
}

// rwD3D9AtomicMatFXRenderCallback 0x553a90 (resentry header, object, type, flags): sets the clipping for the object and (unless render state 0x89 is on, or mesh flag 8) the fixed colour
// pass state, the index buffer / streams / vertex declaration of the instanced header (the device's SetIndices slot 104 and SetVertexDeclaration slot 87 through the exe's
// device global), then renders every mesh by its MatFX material type (1 / 3 bump map, 2 environment map, 4 / 6 dual pass, else default), or black in the fixed colour mode.
typedef void (__cdecl *Cl2Fn)(void *a, u32 b);
typedef void (__cdecl *GetRSFn)(u32 state, u32 *out);
typedef void (__cdecl *V2Fn)(u32 a, u32 b);
typedef void (__cdecl *V3Fn)(u32 a, u32 b, u32 c);
typedef void (__cdecl *SetStreamsFn)(void *a, u32 b);
typedef void (__cdecl *V1Fn)(u32 a);
typedef void (__cdecl *SurfFn)(void *a, void *b, u32 flags);
typedef void (__cdecl *R6Fn)(void *a, void *b, u32 c, void *d, void *e, void *f);
typedef void (__cdecl *R5Fn)(void *a, void *b, u32 c, void *d, void *e);
typedef void (__cdecl *R4Fn)(void *a, void *b, u32 c, void *d);
typedef void (__cdecl *R2Fn)(void *a, void *b);
typedef long (__stdcall *DevIdxFn)(void *dev, u32 v);
void __cdecl rw37_rwD3D9AtomicMatFXRenderCallback(u8 *a1, void *object, u32 type, u32 flags){
	((Cl2Fn)g_m[M_CLIPPING])(object, type & 0xff);
	u32 rs89;
	((GetRSFn)g_m[M_GETRS])(0x89, &rs89);
	int black;
	if(rs89 == 0 && !(flags & 8)){
		black = 1;
		((V2Fn)g_m[M_STATE55D0])(0, 0);
		((V2Fn)g_m[M_SETRS])(0x3c, 0xff000000u);
		((V3Fn)g_m[M_SETTSS])(0, 1, 3);
		((V3Fn)g_m[M_SETTSS])(0, 3, 3);
		((V3Fn)g_m[M_SETTSS])(0, 4, 3);
		((V3Fn)g_m[M_SETTSS])(0, 6, 3);
	}else black = 0;
	u8 *edi = a1 + 0x18, *esi = edi + 0x40;
	u32 ib = *(u32 *)(a1 + 0x20);
	if(ib && *(u32 *)g_m[M_LASTIB] != ib){
		*(u32 *)g_m[M_LASTIB] = ib;
		void *dev = *(void **)g_m[M_DEVICE];
		(*(DevIdxFn *)((u8 *)*(void **)dev + 0x1a0))(dev, *(u32 *)(edi + 8));
	}
	((SetStreamsFn)g_m[M_SETSTREAMS])(edi + 0x10, *(u32 *)(edi + 0x30));
	u32 decl = *(u32 *)(edi + 0x34);
	if(*(u32 *)g_m[M_LASTDECL] != decl){
		*(u32 *)g_m[M_LASTDECL] = decl;
		void *dev = *(void **)g_m[M_DEVICE];
		*(u32 *)g_m[M_LASTFVF] = 0xffffffffu;
		(*(DevIdxFn *)((u8 *)*(void **)dev + 0x15c))(dev, *(u32 *)(edi + 0x34));
	}
	int32_t n = *(int32_t *)(edi + 4);
	if(!n) return;
	do{
		((V1Fn)g_m[M_VERTEXALPHA])((*(u32 *)(esi + 0xc) == 0 && (*(u8 **)(esi + 8))[7] == 0xff) ? 0 : 1);
		if(!black){
			u8 *mat = *(u8 **)(esi + 8);
			if(rs89) ((SurfFn)g_m[M_SURFPROPS])(mat + 0xc, mat + 4, flags);
			mat = *(u8 **)(esi + 8);
			u8 *md = *(u8 **)(*(int32_t *)g_m[M_MATOFF] + (u8 *)mat);
			u32 type = md ? *(u32 *)(md + 0x30) : 0;
			void *tex = *(void **)mat;
			switch(type - 1){
			case 0: ((R6Fn)g_m[M_BUMPRENDER])(edi, esi, flags, tex, *(void **)(md + 4), NULL); break;
			case 1: ((R6Fn)g_m[M_ENVRENDER])(edi, esi, flags, NULL, tex, *(void **)(md + 4)); break;
			case 2: ((R6Fn)g_m[M_BUMPRENDER])(edi, esi, flags, tex, *(void **)(md + 4), *(void **)(md + 0x1c)); break;
			case 3: ((R5Fn)g_m[M_DUALRENDER])(edi, esi, flags, tex, *(void **)md); break;
			case 5: ((R5Fn)g_m[M_DUALRENDER])(edi, esi, flags, tex, *(void **)(md + 0x18)); break;
			default: ((R4Fn)g_m[M_DEFRENDER])(edi, esi, flags, tex); break;
			}
		}else ((R2Fn)g_m[M_BLACKRENDER])(edi, esi);
		esi += 0x24;
	}while(--n);
}

// rpMatFXD3D9AtomicMatFXDefaultRender 0x5508d0 (resentry header, mesh, flags, texture; batch 3): plain textured mesh.  Flags 0x84 select the lit pass states (and, for MatFX
// type 5 materials with an uncompressed texture, texture transform stage 0 from the material's matrix through the exe's scratch matrix at 0x63a2d0), otherwise the plain
// states; then the pixel / vertex shader caches, a DrawIndexedPrimitive (slot 82) or DrawPrimitive (slot 81) on the device.
typedef long (__stdcall *DevDrawIdxFn)(void *dev, u32 type, u32 base, u32 zero, u32 nv, u32 start, u32 count);
typedef long (__stdcall *DevDrawFn)(void *dev, u32 type, u32 start, u32 count);
typedef long (__stdcall *DevSetFn)(void *dev, u32 v);
typedef void (__cdecl *SetXformFn)(u32 state, void *m);
typedef void (__cdecl *V0Fn)(void);
void __cdecl rw37_rpMatFXD3D9AtomicMatFXDefaultRender(u8 *hdr, u8 *mesh, u32 flags, void *tex){
	int tswap = 0;
	if(flags & 0x84){
		((V2Fn)g_m[M_STATE55D0])((u32)(uintptr_t)tex, 0);
		u8 *md = *(u8 **)(*(int32_t *)g_m[M_MATOFF] + *(u8 **)(mesh + 8));
		if(md && *(int32_t *)(md + 0x30) == 5){
			tswap = 1;
			u32 *s = *(u32 **)md;
			if(s && !(s[3] & 0x20000)){
				((V3Fn)g_m[M_SETTSS])(0, 0x18, 2);
				u32 *g = (u32 *)g_m[M_MATSCRATCH];
				g[0] = s[0]; g[1] = s[1]; g[4] = s[4]; g[5] = s[5]; g[8] = s[12]; g[9] = s[13];
				((SetXformFn)g_m[M_SETTRANSFORM])(0x10, g);
			}
		}
		((V3Fn)g_m[M_SETTSS])(0, 1, 4);
		((V3Fn)g_m[M_SETTSS])(0, 2, 2);
		((V3Fn)g_m[M_SETTSS])(0, 3, 0);
		((V3Fn)g_m[M_SETTSS])(0, 4, 4);
		((V3Fn)g_m[M_SETTSS])(0, 5, 2);
		((V3Fn)g_m[M_SETTSS])(0, 6, 0);
	}else{
		((V2Fn)g_m[M_STATE55D0])(0, 0);
		((V3Fn)g_m[M_SETTSS])(0, 1, 3);
		((V3Fn)g_m[M_SETTSS])(0, 3, 0);
		((V3Fn)g_m[M_SETTSS])(0, 4, 3);
		((V3Fn)g_m[M_SETTSS])(0, 6, 0);
	}
	void *dev;
	if(*(u32 *)g_m[M_LASTPS]){
		dev = *(void **)g_m[M_DEVICE];
		*(u32 *)g_m[M_LASTPS] = 0;
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, 0);
	}
	u32 vs = *(u32 *)(mesh + 0x10);
	if(*(u32 *)g_m[M_LASTVS] != vs){
		*(u32 *)g_m[M_LASTVS] = vs;
		dev = *(void **)g_m[M_DEVICE];
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x170))(dev, *(u32 *)(mesh + 0x10));
	}
	if(*(u32 *)(hdr + 8)){
		((V0Fn)g_m[M_FLUSHCACHE])();
		dev = *(void **)g_m[M_DEVICE];
		(*(DevDrawIdxFn *)((u8 *)*(void **)dev + 0x148))(dev, *(u32 *)(hdr + 0xc), *(u32 *)(mesh + 0x14), 0, *(u32 *)(mesh + 0x18), *(u32 *)(mesh + 0x1c), *(u32 *)(mesh + 0x20));
	}else{
		((V0Fn)g_m[M_FLUSHCACHE])();
		dev = *(void **)g_m[M_DEVICE];
		(*(DevDrawFn *)((u8 *)*(void **)dev + 0x144))(dev, *(u32 *)(hdr + 0xc), *(u32 *)(mesh + 0x14), *(u32 *)(mesh + 0x20));
	}
	if(tswap) ((V3Fn)g_m[M_SETTSS])(0, 0x18, 0);
}

// ApplyEnvMapTextureMatrix 0x5520e0 (texture, stage, frame or NULL; batch 3): sets the texture coordinate generation of a stage for an environment map and its texture
// transform.  Cube rasters: camera space reflection (generation 3, projected 0x30000) with the camera frame's matrix (right vector negated, translation 0, w 1).  Others:
// sphere map style (generation 2, 0x10000): without a frame the identity matrix 0x5f5d48, else  (camera LTM * inverse(frame LTM)) with the right vector negated and the
// translation zeroed, times 0x5f5d48.  Returns 1.
typedef int (__cdecl *CubeFn)(void *raster);
typedef void *(__cdecl *GetLtmFn)(void *frame);
typedef void (__cdecl *Mat2Fn)(void *dst, void *src);
typedef void (__cdecl *Mat3Fn)(void *dst, void *a, void *b);
int32_t __cdecl rw37_ApplyEnvMapTextureMatrix(void **tex, u32 stage, void *frame){
	((V2Fn)g_m[M_STATE55D0])((u32)(uintptr_t)tex, stage);
	if(((CubeFn)g_m[M_CUBE])(*tex)){
		((V3Fn)g_m[M_SETTSS])(stage, 0x18, 3);
		((V3Fn)g_m[M_SETTSS])(stage, 0xb, 0x30000);
		u8 *camFrame = *(u8 **)(*(u8 **)eng() + 4);
		float *l = (float *)((GetLtmFn)g_m[M_GETLTM])(camFrame);
		volatile float M[16];
		M[0] = -l[0]; M[1] = -l[1]; M[2] = -l[2];
		M[4] = l[4]; M[5] = l[5]; M[6] = l[6];
		M[8] = l[8]; M[9] = l[9]; M[10] = l[10];
		u32 *Mi = (u32 *)M;
		Mi[12] = 0; Mi[13] = 0; Mi[14] = 0; Mi[3] = 0; Mi[7] = 0; Mi[11] = 0; Mi[15] = 0x3f800000;
		((SetXformFn)g_m[M_SETTRANSFORM])(stage + 0x10, (void *)M);
		return 1;
	}
	((V3Fn)g_m[M_SETTSS])(stage, 0x18, 2);
	((V3Fn)g_m[M_SETTSS])(stage, 0xb, 0x10000);
	if(!frame){
		((SetXformFn)g_m[M_SETTRANSFORM])(stage + 0x10, g_m[M_IDMATRIX]);
		return 1;
	}
	u8 *camFrame = *(u8 **)(*(u8 **)eng() + 4);
	void *ltmCam = ((GetLtmFn)g_m[M_GETLTM])(camFrame);
	void *ltmObj = ((GetLtmFn)g_m[M_GETLTM])(frame);
	u8 I[0x40], T[0x40], R[0x40];
	((Mat2Fn)g_m[M_MATINVERT])(I, ltmObj);
	((Mat3Fn)g_m[M_MATMUL])(T, ltmCam, I);
	{ volatile float *f = (volatile float *)T; f[0] = -f[0]; f[1] = -f[1]; f[2] = -f[2]; }
	*(u32 *)(T + 0xc) = 0; *(u32 *)(T + 0x30) = 0; *(u32 *)(T + 0x34) = 0; *(u32 *)(T + 0x38) = 0;
	((Mat3Fn)g_m[M_MATMUL])(R, T, g_m[M_IDMATRIX]);
	((SetXformFn)g_m[M_SETTRANSFORM])(stage + 0x10, R);
	return 1;
}

// _rpMatFXD3D9AtomicMatFXBumpMapRender 0x5522d0 (hdr, mesh, flags, texture, bump texture, env texture; batch 3): the perturbed UVs of the mesh go into a dynamic vertex buffer
// (CalculatePerturbedUVs), then the bump map pass: with the BumpEnvMap / BumpMap pixel shaders in one pass (env map through ApplyEnvMapTextureMatrix on stage 2 and the material's
// coefficient as pixel shader constant 0), without pixel shaders in a fixed function multi pass (modulate, additive blend, then a blend pass with texture transform).
// Without any bump pixel shader and with an env texture the EnvRender pass follows (flags unchanged).
typedef int (__cdecl *HasAlphaFn)(void *tex);
typedef int (__cdecl *DynLockFn)(u32 stride, u32 count, void **vb, void **data, u32 *baseIdx);
typedef void (__cdecl *DynUnlockFn)(void *vb);
typedef void (__cdecl *CalcUVFn)(u8 *hdr, u8 *mesh, void *data);
typedef void (__cdecl *SetStreamSrcFn)(u32 stream, void *vb, u32 off, u32 stride);
typedef int (__cdecl *V0RFn)(void);
typedef void (__cdecl *BlendFn)(u32 v);
typedef long (__stdcall *DevPSConstFn)(void *dev, u32 reg, const float *c, u32 n);
typedef void (__cdecl *RSGetFn)(u32 state, u32 *out);
typedef void (__cdecl *RSSetFn)(u32 state, u32 v);
static void bumpDraw(u8 *hdr, u8 *mesh, u32 baseIdx){
	((V0Fn)g_m[M_FLUSHCACHE])();
	void *dev = *(void **)g_m[M_DEVICE];
	if(*(u32 *)(hdr + 8))
		(*(DevDrawIdxFn *)((u8 *)*(void **)dev + 0x148))(dev, *(u32 *)(hdr + 0xc), baseIdx, 0, *(u32 *)(mesh + 0x18), *(u32 *)(mesh + 0x1c), *(u32 *)(mesh + 0x20));
	else
		(*(DevDrawFn *)((u8 *)*(void **)dev + 0x144))(dev, *(u32 *)(hdr + 0xc), baseIdx, *(u32 *)(mesh + 0x20));
}
static void setPS(u32 ps){
	if(*(u32 *)g_m[M_LASTPS] != ps){
		void *dev = *(void **)g_m[M_DEVICE];
		*(u32 *)g_m[M_LASTPS] = ps;
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, ps);
	}
}
void __cdecl rw37_rpMatFXD3D9AtomicMatFXBumpMapRender(u8 *hdr, u8 *mesh, u32 flags, void *tex, void *bump, void *env){
	void *bt = bump;
	if(!bt){
		bt = tex;
		if(!bt || !((HasAlphaFn)g_m[M_TEXHASALPHA])(bt)){
			((R4Fn)g_m[M_DEFRENDER])(hdr, mesh, flags, tex);
			return;
		}
	}
	void *vb; void *data; u32 baseIdx;
	if(!((DynLockFn)g_m[M_DYNLOCK])(0x2c, *(u32 *)(mesh + 0x18), &vb, &data, &baseIdx)) return;
	((CalcUVFn)g_m[M_CALCUV])(hdr, mesh, data);
	((DynUnlockFn)g_m[M_DYNUNLOCK])(vb);
	((V2Fn)g_m[M_STATE55D0])((u32)(uintptr_t)bt, 0);
	void *dev;
	if(*(u32 *)g_m[M_LASTFVF] != 0x252){
		dev = *(void **)g_m[M_DEVICE];
		*(u32 *)g_m[M_LASTFVF] = 0x252;
		*(u32 *)g_m[M_LASTDECL] = 0xffffffffu;
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x164))(dev, 0x252);
	}
	((SetStreamSrcFn)g_m[M_SETSTREAMSOURCE])(0, vb, 0, 0x2c);
	u32 vs = *(u32 *)(mesh + 0x10);
	if(*(u32 *)g_m[M_LASTVS] != vs){
		*(u32 *)g_m[M_LASTVS] = vs;
		dev = *(void **)g_m[M_DEVICE];
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x170))(dev, *(u32 *)(mesh + 0x10));
	}
	u32 *ps = (u32 *)g_m[M_PSBASE];		// [0] BumpMap, [3] BumpEnvMap, [4] BumpModulatedEnvMap, [5] BumpModulatedEnvMapNoAlpha
	u32 flagsForEnv = flags;
	if((env && ps[3]) || ps[0]){
		((BlendFn)g_m[M_VERTEXALPHA])(0);
		((V2Fn)g_m[M_STATE55D0])((u32)(uintptr_t)bt, 1);
		u32 s1, s2;
		((RSGetFn)g_m[M_GETRS])(0x1b, &s1);
		((RSGetFn)g_m[M_GETRS])(0xf, &s2);
		((RSSetFn)g_m[M_SETRS])(0x1b, 0);
		((RSSetFn)g_m[M_SETRS])(0xf, 0);
		int done = 0;
		if(env && ps[3]){
			u8 *md = *(u8 **)(*(int32_t *)g_m[M_MATOFF] + *(u8 **)(mesh + 8));
			float *e = (float *)(md + 0x18);
			volatile float v;
			int t;
			v = e[2] * 255.0f;
			__asm__ volatile("flds %1\n\tfistpl %0" : "=m"(t) : "m"(v));
			if((u8)t != 0){
				volatile float c[4];
				float ev = e[2];
				__asm__ volatile("flds %4\n\tfsts %0\n\tfsts %1\n\tfsts %2\n\tfstps %3" : "=m"(c[0]), "=m"(c[1]), "=m"(c[2]), "=m"(c[3]) : "m"(ev));
				rw37_ApplyEnvMapTextureMatrix((void **)env, 2, *(void **)e);
				u32 sh;
				if(*(u32 *)(e + 3)) sh = ((HasAlphaFn)g_m[M_TEXHASALPHA])(env) ? ps[4] : ps[5];
				else sh = ps[3];
				setPS(sh);
				dev = *(void **)g_m[M_DEVICE];
				(*(DevPSConstFn *)((u8 *)*(void **)dev + 0x1b4))(dev, 0, (const float *)c, 1);
				bumpDraw(hdr, mesh, baseIdx);
				((V2Fn)g_m[M_STATE55D0])(0, 2);
				((V3Fn)g_m[M_SETTSS])(2, 0x18, 0);
				((V3Fn)g_m[M_SETTSS])(2, 0xb, 2);
				done = 1;
			}
		}
		if(!done){
			setPS(ps[0]);
			bumpDraw(hdr, mesh, baseIdx);
		}
		((RSSetFn)g_m[M_SETRS])(0x1b, s1);
		((RSSetFn)g_m[M_SETRS])(0xf, s2);
		((V2Fn)g_m[M_STATE55D0])(0, 1);
	}else{
		((V3Fn)g_m[M_SETTSS])(0, 1, 4);
		((V3Fn)g_m[M_SETTSS])(0, 2, 2);
		((V3Fn)g_m[M_SETTSS])(0, 3, 0);
		((V3Fn)g_m[M_SETTSS])(0, 4, 4);
		((V3Fn)g_m[M_SETTSS])(0, 5, 2);
		((V3Fn)g_m[M_SETTSS])(0, 6, 0);
		if(*(u32 *)g_m[M_LASTPS]){
			dev = *(void **)g_m[M_DEVICE];
			*(u32 *)g_m[M_LASTPS] = 0;
			(*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, 0);
		}
		if(!((V0RFn)g_m[M_FUN4E5950])()) ((BlendFn)g_m[M_VERTEXALPHA])(1);
		((RSSetFn)g_m[M_SETRS])(0xf, 0);
		u32 x1, x2;
		typedef void (__cdecl *EngRSFn)(u32 id, u32 *p);
		ENG(0x24, EngRSFn)(0xa, &x1);
		ENG(0x24, EngRSFn)(0xb, &x2);
		((BlendFn)g_m[M_SRCBLEND])(6);
		((BlendFn)g_m[M_DESTBLEND])(1);
		bumpDraw(hdr, mesh, baseIdx);
		((RSSetFn)g_m[M_SETRS])(0xf, 1);
		((BlendFn)g_m[M_SRCBLEND])(5);
		((BlendFn)g_m[M_DESTBLEND])(2);
		u32 s3;
		((RSGetFn)g_m[M_GETRS])(0xe, &s3);
		((RSSetFn)g_m[M_SETRS])(0xe, 0);
		((V3Fn)g_m[M_SETTSS])(0, 0xb, 1);
		bumpDraw(hdr, mesh, baseIdx);
		((V3Fn)g_m[M_SETTSS])(0, 0xb, 0);
		((RSSetFn)g_m[M_SETRS])(0xe, s3);
		((BlendFn)g_m[M_VERTEXALPHA])(0);
		typedef void (__cdecl *EngRSSetFn)(u32 id, u32 v);
		ENG(0x20, EngRSSetFn)(0xa, x1);
		ENG(0x20, EngRSSetFn)(0xb, x2);
	}
	((SetStreamsFn)g_m[M_SETSTREAMS])(hdr + 0x10, *(u32 *)(hdr + 0x30));
	u32 decl = *(u32 *)(hdr + 0x34);
	if(*(u32 *)g_m[M_LASTDECL] != decl){
		*(u32 *)g_m[M_LASTDECL] = decl;
		dev = *(void **)g_m[M_DEVICE];
		*(u32 *)g_m[M_LASTFVF] = 0xffffffffu;
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x15c))(dev, *(u32 *)(hdr + 0x34));
	}
	if(env && !ps[3]) ((R6Fn)g_m[M_ENVRENDER])(hdr, mesh, flagsForEnv, (void *)1, tex, env);
}

// _rpMatFXD3D9AtomicMatFXEnvRender 0x551860 (hdr, mesh, flags, index, texture, env texture; batch 3): environment map pass of mesh with the material's env entry `index`
// (24 byte entries at the MatFX material data: frame +0, coefficient float +8, second texture flag +0xc).  Coefficient 0 or no env texture: plain default render (index 0 only).
// Index 0: base pass (default states, lit or not) and the env map blended on; with the EnvMap pixel shaders a single pass with the coefficient as shader constant 0; with
// multiply-add / diffuse-in-stage-0 hardware the env map goes into texture stage 1.  Index != 0, or after the base pass: the additive env blend pass (coefficient as texture
// factor 0x3c).  In the original the saved render states 0x89 / 0xe / 0x1c live in argument slots (flags, texture, env texture): only the values matter.
typedef long (__stdcall *DevValidateFn)(void *dev, u32 *passes);
typedef void (__cdecl *Conv1Fn)(void *raster);
void __cdecl rw37_rpMatFXD3D9AtomicMatFXEnvRender(u8 *hdr, u8 *mesh, u32 flags, int32_t idx, void **tex, void **env){
	u8 *ent = *(u8 **)(*(int32_t *)g_m[M_MATOFF] + *(u8 **)(mesh + 8)) + 24 * idx;
	volatile float v = *(float *)(ent + 8) * 255.0f;
	int t;
	__asm__ volatile("flds %1\n\tfistpl %0" : "=m"(t) : "m"(v));
	u32 coef = (u32)t & 0xff;
	if(coef == 0 || !env || !*env){
		if(idx == 0) ((R4Fn)g_m[M_DEFRENDER])(hdr, mesh, flags, tex);
		return;
	}
	u32 *ps = (u32 *)g_m[M_PSBASE];		// [1] EnvMap, [2] EnvMapNoBaseTexture
	int hasBase = 0, need2 = 0;
	void *dev;
	u32 vs = *(u32 *)(mesh + 0x10);
	if(*(u32 *)g_m[M_LASTVS] != vs){
		*(u32 *)g_m[M_LASTVS] = vs;
		dev = *(void **)g_m[M_DEVICE];
		(*(DevSetFn *)((u8 *)*(void **)dev + 0x170))(dev, *(u32 *)(mesh + 0x10));
	}
	if(idx == 0){
		if((flags & 0x84) && tex){
			((V2Fn)g_m[M_STATE55D0])((u32)(uintptr_t)tex, 0);
			hasBase = 1;
			if((*(u8 **)tex)[0x23] & 0x20){
				if((*(u8 **)env)[0x23] & 0x20 && tex != env) ((Conv1Fn)g_m[M_CONVNONPAL])(*env);
			}
		}else ((V2Fn)g_m[M_STATE55D0])(0, 0);
		int psA = 0;		// 1: EnvMap pixel shader pass
		int fixed = 0;		// 1: go to the fixed function states
		if(*(u32 *)(ent + 0xc) == 0){
			if(*(u32 *)(mesh + 0xc) != 0 || (*(u8 **)(mesh + 8))[7] != 0xff) fixed = 1;
			else if(hasBase && ((HasAlphaFn)g_m[M_TEXHASALPHA])(tex)) fixed = 1;
		}
		if(!fixed){
			if(((HasAlphaFn)g_m[M_TEXHASALPHA])(env)){
				if(ps[1]) psA = 1; else fixed = 1;
			}else{
				if(*(u32 *)g_m[M_MULADD]) need2 = 1;
				else if(coef != 0xff) { }
				else if(!hasBase) need2 = 1;
				else if(*(u32 *)g_m[M_DIFFUSE0]) need2 = 1;
				else if(*(u32 *)g_m[M_VALIDDIFF]) need2 = 1;
			}
		}
		if(psA){
			volatile float c[4];
			float ev = *(float *)(ent + 8);
			__asm__ volatile("flds %4\n\tfsts %0\n\tfsts %1\n\tfsts %2\n\tfstps %3" : "=m"(c[0]), "=m"(c[1]), "=m"(c[2]), "=m"(c[3]) : "m"(ev));
			dev = *(void **)g_m[M_DEVICE];
			(*(DevPSConstFn *)((u8 *)*(void **)dev + 0x1b4))(dev, 0, (const float *)c, 1);
			if(hasBase){
				rw37_ApplyEnvMapTextureMatrix(env, 1, *(void **)ent);
				if(*(u32 *)g_m[M_LASTPS] != ps[1]){ dev = *(void **)g_m[M_DEVICE]; *(u32 *)g_m[M_LASTPS] = ps[1]; (*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, ps[1]); }
				bumpDraw(hdr, mesh, *(u32 *)(mesh + 0x14));
				((V2Fn)g_m[M_STATE55D0])(0, 1);
				((V3Fn)g_m[M_SETTSS])(1, 0x18, 0);
				((V3Fn)g_m[M_SETTSS])(1, 0xb, 1);
			}else{
				rw37_ApplyEnvMapTextureMatrix(env, 0, *(void **)ent);
				if(*(u32 *)g_m[M_LASTPS] != ps[2]){ dev = *(void **)g_m[M_DEVICE]; *(u32 *)g_m[M_LASTPS] = ps[2]; (*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, ps[2]); }
				bumpDraw(hdr, mesh, *(u32 *)(mesh + 0x14));
				((V2Fn)g_m[M_STATE55D0])(0, 0);
				((V3Fn)g_m[M_SETTSS])(0, 0x18, 0);
				((V3Fn)g_m[M_SETTSS])(0, 0xb, 0);
			}
			return;
		}
		if(*(u32 *)g_m[M_LASTPS]){
			dev = *(void **)g_m[M_DEVICE];
			*(u32 *)g_m[M_LASTPS] = 0;
			(*(DevSetFn *)((u8 *)*(void **)dev + 0x1ac))(dev, 0);
		}
		if(hasBase){
			((V3Fn)g_m[M_SETTSS])(0, 1, 4); ((V3Fn)g_m[M_SETTSS])(0, 2, 2); ((V3Fn)g_m[M_SETTSS])(0, 3, 0);
			((V3Fn)g_m[M_SETTSS])(0, 4, 4); ((V3Fn)g_m[M_SETTSS])(0, 5, 2); ((V3Fn)g_m[M_SETTSS])(0, 6, 0);
		}else{
			((V3Fn)g_m[M_SETTSS])(0, 1, 3); ((V3Fn)g_m[M_SETTSS])(0, 3, 0); ((V3Fn)g_m[M_SETTSS])(0, 4, 3); ((V3Fn)g_m[M_SETTSS])(0, 6, 0);
		}
		if(need2){
			u32 c4 = (((coef << 8 | coef) << 8 | coef) << 8) | coef;
			if(coef == 0xff && !hasBase){
				rw37_ApplyEnvMapTextureMatrix(env, 0, *(void **)ent);
				((V3Fn)g_m[M_SETTSS])(0, 1, 7);
				bumpDraw(hdr, mesh, *(u32 *)(mesh + 0x14));
				((V2Fn)g_m[M_STATE55D0])(0, 0);
				((V3Fn)g_m[M_SETTSS])(0, 0x18, 0);
				((V3Fn)g_m[M_SETTSS])(0, 0xb, 0);
				return;
			}
			rw37_ApplyEnvMapTextureMatrix(env, 1, *(void **)ent);
			if(coef != 0xff){
				((RSSetFn)g_m[M_SETRS])(0x3c, c4);
				((V3Fn)g_m[M_SETTSS])(1, 1, 0x19); ((V3Fn)g_m[M_SETTSS])(1, 0x1a, 1); ((V3Fn)g_m[M_SETTSS])(1, 2, 2); ((V3Fn)g_m[M_SETTSS])(1, 3, 3);
			}else{
				((V3Fn)g_m[M_SETTSS])(1, 1, 7); ((V3Fn)g_m[M_SETTSS])(1, 2, 2); ((V3Fn)g_m[M_SETTSS])(1, 3, 1);
				if(*(u32 *)g_m[M_VALIDDIFF]){
					u32 passes = 0;
					((V0Fn)g_m[M_FLUSHCACHE])();
					dev = *(void **)g_m[M_DEVICE];
					long hr = (*(DevValidateFn *)((u8 *)*(void **)dev + 0x118))(dev, &passes);
					*(u32 *)g_m[M_DIFFUSE0] = (hr >= 0 && passes == 1) ? 1 : 0;
					*(u32 *)g_m[M_VALIDDIFF] = 0;
				}
			}
			bumpDraw(hdr, mesh, *(u32 *)(mesh + 0x14));
			((V2Fn)g_m[M_STATE55D0])(0, 1);
			((V3Fn)g_m[M_SETTSS])(1, 1, 1); ((V3Fn)g_m[M_SETTSS])(1, 4, 1); ((V3Fn)g_m[M_SETTSS])(1, 0x18, 0); ((V3Fn)g_m[M_SETTSS])(1, 0xb, 1);
			return;
		}
		bumpDraw(hdr, mesh, *(u32 *)(mesh + 0x14));
	}
	// second pass: additive env map blend
	rw37_ApplyEnvMapTextureMatrix(env, 0, *(void **)ent);
	if(!((V0RFn)g_m[M_FUN4E5950])()) ((BlendFn)g_m[M_VERTEXALPHA])(1);
	typedef void (__cdecl *EngRSFn)(u32 id, u32 *p);
	typedef void (__cdecl *EngRSSetFn)(u32 id, u32 v);
	u32 sA, sB, sC, sD, sE, sF = 0;
	ENG(0x24, EngRSFn)(0xa, &sA);
	ENG(0x24, EngRSFn)(0xb, &sB);
	((BlendFn)g_m[M_SRCBLEND])(5);
	((BlendFn)g_m[M_DESTBLEND])(2);
	u32 c4 = (((coef << 8 | coef) << 8 | coef) << 8) | coef;
	if(c4 < 0xffffffffu){
		((RSSetFn)g_m[M_SETRS])(0x3c, c4);
		((V3Fn)g_m[M_SETTSS])(0, 1, 4); ((V3Fn)g_m[M_SETTSS])(0, 2, 2); ((V3Fn)g_m[M_SETTSS])(0, 3, 3);
		((V3Fn)g_m[M_SETTSS])(0, 4, 2); ((V3Fn)g_m[M_SETTSS])(0, 5, 2); ((V3Fn)g_m[M_SETTSS])(0, 6, 3);
	}else{
		((V3Fn)g_m[M_SETTSS])(0, 1, 2); ((V3Fn)g_m[M_SETTSS])(0, 2, 2); ((V3Fn)g_m[M_SETTSS])(0, 4, 2); ((V3Fn)g_m[M_SETTSS])(0, 5, 2);
	}
	((RSGetFn)g_m[M_GETRS])(0x89, &sC);
	((RSGetFn)g_m[M_GETRS])(0xe, &sD);
	((RSGetFn)g_m[M_GETRS])(0x1c, &sE);
	((RSSetFn)g_m[M_SETRS])(0x89, 0);
	((RSSetFn)g_m[M_SETRS])(0xe, 0);
	if(sE){
		((RSGetFn)g_m[M_GETRS])(0x22, &sF);
		((RSSetFn)g_m[M_SETRS])(0x22, 0);
	}
	bumpDraw(hdr, mesh, *(u32 *)(mesh + 0x14));
	if(sE) ((RSSetFn)g_m[M_SETRS])(0x22, sF);
	((RSSetFn)g_m[M_SETRS])(0xe, sD);
	((RSSetFn)g_m[M_SETRS])(0x89, sC);
	if(c4 < 0xffffffffu){
		((V3Fn)g_m[M_SETTSS])(0, 3, 0);
		((V3Fn)g_m[M_SETTSS])(0, 6, 0);
	}else{
		((V3Fn)g_m[M_SETTSS])(0, 1, 4);
		((V3Fn)g_m[M_SETTSS])(0, 4, 4);
	}
	((BlendFn)g_m[M_VERTEXALPHA])(0);
	ENG(0x20, EngRSSetFn)(0xa, sA);
	ENG(0x20, EngRSSetFn)(0xb, sB);
	((V3Fn)g_m[M_SETTSS])(0, 0x18, 0);
	((V3Fn)g_m[M_SETTSS])(0, 0xb, 0);
}

// rpMatFXPipelinesCreate 0x553ca0 (batch 3): builds the atomic and world sector MatFX pipelines (one fragment each, pipeline flags 0x120), keeps the default instance
// callbacks and installs this file's instance callbacks (0x553ee0 / 0x553f60) and the render callback 0x553a90, reads the card caps (blend stages, multitexture, multiply-add,
// modulate 2x, pixel shader version >= 1.1 -> tangents + the six MatFX pixel shaders) into the MatFX globals.  Always returns 1.
typedef void *(__cdecl *PipeCreateFn)(void);
typedef void *(__cdecl *PipeLockFn)(void *p);
typedef void *(__cdecl *FragFn)(void);
typedef void *(__cdecl *AddFragFn)(void *locked, u32 a, void *frag, u32 b);
typedef void (__cdecl *Unlock1Fn)(void *p);
typedef void *(__cdecl *FindNodeFn)(void *pipe, void *name, u32 a, u32 b);
typedef void *(__cdecl *GetCbFn)(void *node);
typedef void (__cdecl *SetCbFn)(void *node, void *cb);
typedef u8 *(__cdecl *CapsFn)(void);
typedef void (__cdecl *CreatePSFn)(void *data, void *slot);
static void *buildPipe(int fragSlot, int pipeVar, int defVar, int instCb){
	void *pipe = ((PipeCreateFn)g_m[M_PIPECREATE])();
	if(pipe){
		*(u32 *)((u8 *)pipe + 0x2c) = 0x120;
		void *locked = ((PipeLockFn)g_m[M_PIPELOCK])(pipe);
		if(locked){
			void *frag = ((FragFn)g_m[fragSlot])();
			((Unlock1Fn)g_m[M_PIPEUNLOCK])(((AddFragFn)g_m[M_ADDFRAG])(locked, 0, frag, 0));
		}else{
			((Unlock1Fn)g_m[M_PIPEDESTROY])(pipe);
			pipe = NULL;
		}
	}
	*(void **)g_m[pipeVar] = pipe;
	void **name = (void **)((FragFn)g_m[fragSlot])();
	void *node = ((FindNodeFn)g_m[M_FINDNODE])(*(void **)g_m[pipeVar], *name, 0, 0);
	*(void **)g_m[defVar] = ((GetCbFn)g_m[M_GETINSTCB])(node);
	((SetCbFn)g_m[M_SETINSTCB])(node, g_m[instCb]);
	((SetCbFn)g_m[M_SETRENDERCB])(node, g_m[M_CBRENDER]);
	return node;
}
int32_t __cdecl rw37_rpMatFXPipelinesCreate(void){
	buildPipe(M_FRAGATOMIC, M_PIPEATOMIC, M_DEFATOMIC, M_CBINSTATOMIC);
	buildPipe(M_FRAGSECTOR, M_PIPESECTOR, M_DEFSECTOR, M_CBINSTSECTOR);
	u8 *caps = ((CapsFn)g_m[M_GETCAPS])();
	u32 stages = *(u32 *)(caps + 0x94);
	*(u32 *)g_m[M_MAXSTAGES] = stages;
	if(stages >= 2){
		*(u32 *)g_m[M_MULTITEX] = *(u32 *)(caps + 0x98) >= 2 ? 1 : 0;
		*(u32 *)g_m[M_MULADD] = *(u32 *)(caps + 0x90) & 0x1000000;
		*(u32 *)g_m[M_MOD2X] = *(u32 *)(caps + 0x90) & 0x10;
	}else{
		*(u32 *)g_m[M_MULTITEX] = 0; *(u32 *)g_m[M_MULADD] = 0; *(u32 *)g_m[M_MOD2X] = 0;
		*(u32 *)g_m[M_DIFFUSE0] = 0; *(u32 *)g_m[M_VALID2X] = 0; *(u32 *)g_m[M_VALIDDIFF] = 0;
	}
	u32 *ps = (u32 *)g_m[M_PSBASE];
	if((*(u32 *)(caps + 0xcc) & 0xffff) >= 0x101){
		*(u32 *)g_m[M_TANGENTS] = 1;
		for(int i = 0; i < 6; i++) ((CreatePSFn)g_m[M_CREATEPS])(g_m[M_PSDATA0 + i], ps + i);
	}else{
		*(u32 *)g_m[M_TANGENTS] = 0;
		for(int i = 0; i < 6; i++) ps[i] = 0;
	}
	return 1;
}

// D3D9AtomicMatFXInstanceCallback 0x553ee0 (atomic, header, flags) / D3D9WorldSectorMatFXInstanceCallback 0x553f60 (sector, header, flags): when the card supports tangents,
// every mesh (0x24 byte entries from header + 0x48, count at header + 4) whose MatFX material type is 1 or 3 marks the geometry / sector with usage flag 0x10000; then the
// default instance callback runs.
void *__cdecl rw37_D3D9AtomicMatFXInstanceCallback(u8 *atomic, u8 *header, void *flags){
	if(!flags){
		int32_t cnt = *(int32_t *)(header + 4);
		u8 *m = header + 0x48;
		do{
			u8 *data = *(u8 **)(*(int32_t *)g_m[M_MATOFF] + *(int32_t *)m);
			if(data){
				int32_t t = *(int32_t *)(data + 0x30);
				if((t == 1 || t == 3) && *(int32_t *)g_m[M_TANGENTS]){
					void *geom = *(void **)(atomic + 0x18);
					((Set2Fn)g_m[M_GEOMSET])(geom, ((Get1Fn)g_m[M_GEOMGET])(geom) | 0x10000);
				}
			}
			m += 0x24;
		}while(--cnt);
	}
	return (*(InstFn *)g_m[M_DEFATOMIC])(atomic, header, flags);
}
void *__cdecl rw37_D3D9WorldSectorMatFXInstanceCallback(u8 *sector, u8 *header, void *flags){
	int32_t cnt = *(int32_t *)(header + 4);
	u8 *m = header + 0x48;
	do{
		u8 *data = *(u8 **)(*(int32_t *)g_m[M_MATOFF] + *(int32_t *)m);
		if(data){
			int32_t t = *(int32_t *)(data + 0x30);
			if((t == 1 || t == 3) && *(int32_t *)g_m[M_TANGENTS])
				((Set2Fn)g_m[M_SECTORSET])(sector, ((Get1Fn)g_m[M_SECTORGET])(sector) | 0x10000);
		}
		m += 0x24;
	}while(--cnt);
	return (*(InstFn *)g_m[M_DEFSECTOR])(sector, header, flags);
}

}
