// NODED3D9 (group noded3d9, lane K window 5 batch 6): the small rows of the D3D9 all-in-one pipeline nodes of MFL.exe (rpworld noded3d9worldsectorallinone /
// noded3d9atomicallinone buckets), written op for op from the disassembly, on the exe's own engine, resource arena list and callbacks:
//   D3D9WorldSectorAllInOneNode 0x5003c0, D3D9WorldSectorAllInOnePipelineInit 0x500510, D3D9WorldSectorDefaultLightingCallback 0x500540,
//   D3D9AtomicAllInOnePipelineInit 0x4ff950, D3D9VertexElementSortCB 0x500310 (LAB_00500310), D3D9AtomicDefaultReinstanceCallback 0x500340
// Not here (still original): D3D9AtomicDefaultInstanceCallback 0x4ff980, D3D9WorldSectorDefaultInstanceCallback 0x5005b0, D3D9AtomicAllInOneNode 0x4ff6b0,
// rwD3D9AtomicDefaultLightingCallback 0x4ff4c0: the rows here call / install them through the bind table.
// Exe binding: mflrw_noded3d9_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_noded3d9_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_WSINSTANCECB, E_WSLIGHTCB, E_RENDERCB, E_ATINSTANCECB, E_ATREINSTANCECB, E_ATLIGHTCB,
         E_LIGHTSGLOBAL, E_LIGHTLOCAL, E_LIGHTSENABLE, E_FREERESENTRY, E_INSTANCE, E_RESMODULE, E_SETXFORMWORLD, E_SETRS,
         E_AMBIENT, E_LIGHTDIR, E_FRAMEGETLTM, E_ATOMICSPHERE, E_GETRS,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] noded3d9 called before mflrw_noded3d9_bind\n"); abort(); }
	return *g_engine;
}
#define U32AT(p) (*(u32 *)(p))
#define U16AT(p) (*(u16 *)(p))
typedef u32 (__cdecl *F1)(u32);
typedef u32 (__cdecl *F2)(u32, u32);
typedef u32 (__cdecl *F3)(u32, u32, u32);
typedef u32 (__cdecl *F4)(u32, u32, u32, u32);
typedef u32 (__cdecl *InstanceFn)(u32 object, u32 owner, u32 type, u32 pentry, u32 meshHeader, u32 cb, u32 native);

extern "C" {

void __cdecl mflrw_noded3d9_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// D3D9WorldSectorAllInOnePipelineInit 0x500510 (node): the node's 4 callbacks (+0x14): instance, reinstance (none), lighting, render.  Returns 1.
int32_t __cdecl rw37_D3D9WorldSectorAllInOnePipelineInit(u8 *node){
	eng();
	void **cb = *(void ***)(node + 0x14);
	cb[0] = g_m[E_WSINSTANCECB];
	cb[1] = 0;
	cb[2] = g_m[E_WSLIGHTCB];
	cb[3] = g_m[E_RENDERCB];
	return 1;
}

// D3D9AtomicAllInOnePipelineInit 0x4ff950 (node): the same for the atomic node, with its reinstance callback.  Returns 1.
int32_t __cdecl rw37_D3D9AtomicAllInOnePipelineInit(u8 *node){
	eng();
	void **cb = *(void ***)(node + 0x14);
	cb[0] = g_m[E_ATINSTANCECB];
	cb[1] = g_m[E_ATREINSTANCECB];
	cb[2] = g_m[E_ATLIGHTCB];
	cb[3] = g_m[E_RENDERCB];
	return 1;
}

// D3D9VertexElementSortCB 0x500310 (a, b): qsort compare of two D3DVERTEXELEMENT9: stream (u16 +0), then offset (u16 +2).
int32_t __cdecl rw37_D3D9VertexElementSortCB(const u16 *a, const u16 *b){
	int32_t r = (int32_t)a[0] - (int32_t)b[0];
	if(r == 0) r = (int32_t)a[1] - (int32_t)b[1];
	return r;
}

// D3D9WorldSectorDefaultLightingCallback 0x500540 (sector): world without the light flag (0x20 of the current world's flags byte): _rwD3D9LightsEnable(0, -1).  Else the
// global lights (type 2), the engine's light frame counter (u16 +0xa) + 1, every light of the sector's light list (+0x40, entries {next, prev, light}) that has flag 2 enabled
// locally, the results ored, then _rwD3D9LightsEnable(any, -1).  Returns what _rwD3D9LightsEnable returns (the original leaves it in eax).
u32 __cdecl rw37_D3D9WorldSectorDefaultLightingCallback(u8 *sector){
	u8 *world = *(u8 **)(eng() + 4);
	if(!(world[8] & 0x20)) return ((F2)g_m[E_LIGHTSENABLE])(0, 0xffffffff);
	u32 any = ((F1)g_m[E_LIGHTSGLOBAL])(2);
	u8 *head = sector + 0x40;
	U16AT(eng() + 0xa) += 1;
	for(u8 *n = *(u8 **)head; n != head; n = *(u8 **)n){
		u8 *light = *(u8 **)(n + 8);
		if(light && (light[2] & 2)) any |= ((F1)g_m[E_LIGHTLOCAL])((u32)(uintptr_t)light);
	}
	return ((F2)g_m[E_LIGHTSENABLE])(any, 0xffffffff);
}

// D3D9AtomicDefaultReinstanceCallback 0x500340 (atomic, resEntry, instanceCB): geometry (+0x18) with one morph target: instanceCB(atomic, resEntry + 0x18, 1) when there is
// one (returns whether it gave non zero).  Else, when the atomic's interpolator flag (+0x4c bit 0) is set or the geometry lock word (u16 +0xc) is not 0: lock word | 6 for
// the call of the default instance callback (atomic, resEntry + 0x18, 1), then the old lock word back.  Returns 1.
int32_t __cdecl rw37_D3D9AtomicDefaultReinstanceCallback(u8 *atomic, u8 *resEntry, void *instanceCB){
	eng();
	u8 *geom = *(u8 **)(atomic + 0x18);
	if(U32AT(geom + 0x18) == 1){
		if(!instanceCB) return 1;
		return ((F3)instanceCB)((u32)(uintptr_t)atomic, (u32)(uintptr_t)(resEntry + 0x18), 1) != 0;
	}
	if((atomic[0x4c] & 1) || U16AT(geom + 0xc) != 0){
		u16 lock = U16AT(geom + 0xc);
		U16AT(geom + 0xc) = lock | 6;
		((F3)g_m[E_ATINSTANCECB])((u32)(uintptr_t)atomic, (u32)(uintptr_t)(resEntry + 0x18), 1);
		U16AT(geom + 0xc) = lock;
	}
	return 1;
}

// D3D9WorldSectorAllInOneNode 0x5003c0 (node, params): params[0] = world sector.  Nothing to do (1) without vertices (u16 +0x82) or meshes (mesh header +0x78, u16 +4).
// Unless the current world's flags (+8) have 0x1000000: a resource entry (+0x34) whose serial (+0x18) is the mesh header's (u16 +6) moves to the front of the engine's used
// resource list (when it is linked), any other is freed and the sector is instanced again (rxD3D9Instance; 0 = failure, returns 0).  Unless flag 0x2000000: lighting
// callback, world transform, render state 0x8f = 0, render callback (resEntry, sector, 0xff, flags).  Returns 1.
int32_t __cdecl rw37_D3D9WorldSectorAllInOneNode(u8 *node, u8 **params){
	void **cb = *(void ***)(node + 0x14);
	u8 *sector = params[0];
	if(U16AT(sector + 0x82) == 0) return 1;
	u8 *mh = *(u8 **)(sector + 0x78);
	if(U16AT(mh + 4) == 0) return 1;
	u8 *res = *(u8 **)(sector + 0x34);
	u32 flags = U32AT(*(u8 **)(eng() + 4) + 8);
	if(!(flags & 0x1000000)){
		if(res && U32AT(res + 0x18) == (u32)U16AT(mh + 6)){
			if(U32AT(res)){
				U32AT(*(u8 **)(res + 4)) = U32AT(res);
				U32AT(*(u8 **)res + 4) = U32AT(res + 4);
				u32 mod = *(u32 *)g_m[E_RESMODULE];
				U32AT(res) = U32AT(*(u8 **)(eng() + mod + 0x24));
				U32AT(res + 4) = U32AT(eng() + mod + 0x24);
				U32AT(*(u8 **)*(u8 **)(eng() + mod + 0x24) + 4) = (u32)(uintptr_t)res;
				U32AT(*(u8 **)(eng() + mod + 0x24)) = (u32)(uintptr_t)res;
			}
		}else{
			if(res) ((F1)g_m[E_FREERESENTRY])((u32)(uintptr_t)res);
			res = (u8 *)(uintptr_t)((InstanceFn)g_m[E_INSTANCE])((u32)(uintptr_t)sector, (u32)(uintptr_t)sector, 0xff, (u32)(uintptr_t)(sector + 0x34), (u32)(uintptr_t)mh,
				(u32)(uintptr_t)cb[0], (flags >> 0x19) & 1);
			if(!res) return 0;
		}
	}
	if(flags & 0x2000000) return 1;
	if(cb[2]) ((F1)cb[2])((u32)(uintptr_t)sector);
	((F1)g_m[E_SETXFORMWORLD])(0);
	((F2)g_m[E_SETRS])(0x8f, 0);
	if(cb[3]) ((F4)cb[3])((u32)(uintptr_t)res, (u32)(uintptr_t)sector, 0xff, flags);
	return 1;
}

// the used resource list of the engine's resources module: unlink the entry and put it at the front (same instruction sequence in both node functions)
static void resEntryToFront(u8 *res){
	U32AT(*(u8 **)(res + 4)) = U32AT(res);
	U32AT(*(u8 **)res + 4) = U32AT(res + 4);
	u32 mod = *(u32 *)g_m[E_RESMODULE];
	U32AT(res) = U32AT(*(u8 **)(eng() + mod + 0x24));
	U32AT(res + 4) = U32AT(eng() + mod + 0x24);
	U32AT(*(u8 **)*(u8 **)(eng() + mod + 0x24) + 4) = (u32)(uintptr_t)res;
	U32AT(*(u8 **)(eng() + mod + 0x24)) = (u32)(uintptr_t)res;
}
// squared length of a matrix row as the original computes it (x87 register, stored as float), outside [0.9, 1.1] by the signed compare of the float's bits
static int rowNotUnit(const float *r){
	volatile float len = (float)((double)r[0] * (double)r[0] + (double)r[1] * (double)r[1] + (double)r[2] * (double)r[2]);
	union { float f; int32_t i; } u;
	u.f = len;
	return u.i > 0x3f8ccccd || u.i < 0x3f666666;
}

// D3D9AtomicAllInOneNode 0x4ff6b0 (node, params): params[0] = atomic, geometry at +0x18.  Nothing to do (1) without vertices (int +0x14 <= 0) or meshes.  Unless the
// geometry flags (+8) have 0x1000000: the resource entry (geometry +0x58 for one morph target, else atomic +0x14) with the mesh header's serial is kept: reinstanced
// through callback 1 when the geometry is locked (u16 +0xc) or morphs (failure: entry freed, returns 0; then atomic +0x4c bit 0 and the lock word are cleared) and moved to
// the front of the used list; any other entry is freed and the atomic instanced again (0 = failure, returns 0; lock word cleared).  Unless flag 0x2000000: lighting
// callback, world transform = frame LTM, render state 0x8f (normalize normals) = 1 when render state 0x89 is on, the LTM is not flagged 0x20001 and one of its 3 rows has
// a squared length outside [0.9, 1.1]; render callback (resEntry, atomic, 1, flags).  Returns 1.
int32_t __cdecl rw37_D3D9AtomicAllInOneNode(u8 *node, u8 **params){
	u8 *atomic = params[0];
	void **cb = *(void ***)(node + 0x14);
	u8 *geom = *(u8 **)(atomic + 0x18);
	if(*(int32_t *)(geom + 0x14) <= 0) return 1;
	u8 *mh = *(u8 **)(geom + 0x54);
	if(U16AT(mh + 4) == 0) return 1;
	u32 flags = U32AT(geom + 8);
	u8 *res;
	if(!(flags & 0x1000000)){
		u32 nmt = U32AT(geom + 0x18);
		res = nmt == 1 ? *(u8 **)(geom + 0x58) : *(u8 **)(atomic + 0x14);
		if(res && U32AT(res + 0x18) == (u32)U16AT(mh + 6)){
			if(U16AT(geom + 0xc) != 0 || nmt != 1){
				if(cb[1] && !((F3)cb[1])((u32)(uintptr_t)atomic, (u32)(uintptr_t)res, (u32)(uintptr_t)cb[0])){
					((F1)g_m[E_FREERESENTRY])((u32)(uintptr_t)res);
					return 0;
				}
				U32AT(atomic + 0x4c) &= ~1u;
				U16AT(geom + 0xc) = 0;
			}
			if(U32AT(res)) resEntryToFront(res);
		}else{
			if(res) ((F1)g_m[E_FREERESENTRY])((u32)(uintptr_t)res);
			u8 *owner, *pentry;
			mh = *(u8 **)(geom + 0x54);
			if(U32AT(geom + 0x18) == 1){ owner = geom; pentry = geom + 0x58; }
			else{ owner = atomic; pentry = atomic + 0x14; }
			res = (u8 *)(uintptr_t)((InstanceFn)g_m[E_INSTANCE])((u32)(uintptr_t)atomic, (u32)(uintptr_t)owner, 1, (u32)(uintptr_t)pentry, (u32)(uintptr_t)mh,
				(u32)(uintptr_t)cb[0], (flags >> 0x19) & 1);
			if(!res) return 0;
			U16AT(geom + 0xc) = 0;
		}
	}else res = *(u8 **)(geom + 0x58);
	if(flags & 0x2000000) return 1;
	if(cb[2]) ((F1)cb[2])((u32)(uintptr_t)atomic);
	u8 *ltm = (u8 *)(uintptr_t)((F1)g_m[E_FRAMEGETLTM])(U32AT(atomic + 4));
	((F1)g_m[E_SETXFORMWORLD])((u32)(uintptr_t)ltm);
	u32 normalize = 0, on = 0;
	((F2)g_m[E_GETRS])(0x89, (u32)(uintptr_t)&on);
	if(on && !(U32AT(ltm + 0xc) & 0x20001))
		normalize = rowNotUnit((float *)ltm) || rowNotUnit((float *)(ltm + 0x10)) || rowNotUnit((float *)(ltm + 0x20));
	((F2)g_m[E_SETRS])(0x8f, normalize);
	if(cb[3]) ((F4)cb[3])((u32)(uintptr_t)res, (u32)(uintptr_t)atomic, 1, flags);
	return 1;
}

// rwD3D9AtomicDefaultLightingCallback 0x4ff4c0 (atomic): geometry without the light flag (0x20) or no current world: _rwD3D9LightsEnable(0, 1).  Else the ambient sum
// (AmbientSaturated, 3 floats + 1.0) is rebuilt from the world's light list (+0x3c, link at light +0x34): lights with flag 1; directional (subtype 1) are enabled when
// the geometry has normals (0x10), everything else adds its colour (+0x18).  No such light: render state 0x8b = 0.  With normals: light frame counter + 1, then every
// light (flag 1, not yet seen in this frame: u16 +0x3c) of every world sector the atomic is in (+0x64, sector at +8, its lights at +0x40) is enabled locally when
// (radius + sphere radius)^2 > squared distance of its frame position to the atomic's world bounding sphere (compared in x87 registers).  _rwD3D9LightsEnable(any, 1);
// returns its result (the original leaves it in eax).
u32 __cdecl rw37_rwD3D9AtomicDefaultLightingCallback(u8 *atomic){
	u32 gflags = U32AT(*(u8 **)(atomic + 0x18) + 8);
	u8 *world;
	if(!(gflags & 0x20) || !(world = *(u8 **)(eng() + 4))) return ((F2)g_m[E_LIGHTSENABLE])(0, 1);
	float *amb = (float *)g_m[E_AMBIENT];
	amb[0] = 0; amb[1] = 0; amb[2] = 0; amb[3] = 1.0f;
	world = *(u8 **)(eng() + 4);
	u32 any = 0;
	u8 *head = world + 0x3c;
	for(u8 *n = *(u8 **)head; n != head; n = *(u8 **)n){
		u8 *light = n - 0x34;
		if(!(light[2] & 1)) continue;
		if(light[1] == 1){
			if(!(gflags & 0x10)) continue;
			((F1)g_m[E_LIGHTDIR])((u32)(uintptr_t)light);
		}else{
			amb[0] = (float)((double)amb[0] + (double)*(float *)(light + 0x18));
			amb[1] = (float)((double)amb[1] + (double)*(float *)(light + 0x1c));
			amb[2] = (float)((double)amb[2] + (double)*(float *)(light + 0x20));
		}
		any = 1;
	}
	if(!any) ((F2)g_m[E_SETRS])(0x8b, 0);
	if(gflags & 0x10){
		u8 *ties = atomic + 0x64;
		U16AT(eng() + 0xa) += 1;
		for(u8 *t = *(u8 **)ties; t != ties; t = *(u8 **)t){
			u8 *lhead = *(u8 **)(t + 8) + 0x40;
			for(u8 *n = *(u8 **)lhead; n != lhead; n = *(u8 **)n){
				u8 *light = *(u8 **)(n + 8);
				if(!light) continue;
				u16 frame = U16AT(eng() + 0xa);
				if(U16AT(light + 0x3c) == frame) continue;
				if(!(light[2] & 1)) continue;
				U16AT(light + 0x3c) = frame;
				const float *pos = (const float *)((u8 *)(uintptr_t)((F1)g_m[E_FRAMEGETLTM])(U32AT(*(u8 **)(n + 8) + 4)) + 0x30);
				const float *sph = (const float *)(uintptr_t)((F1)g_m[E_ATOMICSPHERE])((u32)(uintptr_t)atomic);
				double dx = (double)sph[0] - (double)pos[0];
				double dy = (double)sph[1] - (double)pos[1];
				light = *(u8 **)(n + 8);
				double dz = (double)sph[2] - (double)pos[2];
				double rr = (double)*(float *)(light + 0x14) + (double)sph[3];
				if(rr * rr > dy * dy + dx * dx + dz * dz)
					any |= ((F1)g_m[E_LIGHTLOCAL])((u32)(uintptr_t)light);
			}
		}
	}
	return ((F2)g_m[E_LIGHTSENABLE])(any, 1);
}

}
