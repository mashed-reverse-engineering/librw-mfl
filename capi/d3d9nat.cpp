// D3D9NAT (group d3d9nat, lane K4 window 8): small rpworld d3d9 rows of MFL.exe, written op for op from the disassembly, on the exe's globals and callees:
//   _rpD3D9UsageFlagPluginAttach 0x509470: registers the usage-flag engine plugin and the geometry / world sector plugin (id 0x50f), stores both offsets
//   _rpGeometryNativeSize 0x501a30, _rpWorldSectorNativeSize 0x502130: size of the native (instanced) data, one COM call (vertex declaration GetDeclaration)
//   RpBuildMeshGenerateDefaultTriStrip 0x5021c0: TriStripMeshGenerate(buildMesh, 0, 1)
// The plugin callbacks (0x509510 / 0x509520 engine ctor / dtor, 0x54d840 object ctor / dtor, 0x5094f0 copy) stay exe code, passed by address.
// Exe binding: mflrw_d3d9nat_bind(table), table order = enum V below.  Unbound: abort.  Check: tools/delink/rw_d3d9nat_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;

enum V { V_ENGREG, V_GEOREG, V_SECREG, V_ENGCTOR, V_ENGDTOR, V_OBJCTOR, V_OBJCOPY, V_GEOOFS, V_SECOFS, V_GETWORLD, V_TRISTRIP, V_N };
static void *g_v[V_N];
static void *fn(int i){
	if(!g_v[V_N - 1]){ fprintf(stderr, "[mflrw] d3d9nat called before mflrw_d3d9nat_bind\n"); abort(); }
	return g_v[i];
}
#define R32(p) (*(u32 *)(uintptr_t)(p))
typedef i32 (__cdecl *EngRegFn)(i32 size, u32 id, void *ctor, void *dtor);			// RwEngineRegisterPlugin 0x4d2f20
typedef i32 (__cdecl *ObjRegFn)(i32 size, u32 id, void *ctor, void *dtor, void *copy);	// RpGeometryRegisterPlugin 0x4f9110 / RpWorldSectorRegisterPlugin 0x500b40
typedef u32 (__cdecl *GetWorldFn)(u32 sector);							// RpWorldSectorGetWorld 0x4f56f0
typedef u32 (__cdecl *TriStripFn)(u32 buildMesh, u32 a, u32 b);				// TriStripMeshGenerate 0x5021e0
typedef long (__stdcall *GetDeclFn)(u32 self, void *elements, u32 *num);			// IDirect3DVertexDeclaration9::GetDeclaration (vtbl +0x10)

// shared tail of both native sizes (0x501a46..0x501a91 / 0x50215c..0x5021a6): h = the instance header (resEntry data)
static u32 nativeSize(u32 h){
	u32 num;
	u8 elems[0xa0];								// the original's 0xa4 frame: count at +0, elements at +4
	u32 decl = R32(h + 0x4c);
	u32 sz = R32(h + 8) + 0x14;						// edi, read before the call
	((GetDeclFn)(uintptr_t)R32(R32(decl) + 0x10))(decl, elems, &num);
	sz = sz + 2 * (R32(h + 0x50) + num * 4) + 4;
	for(u32 i = 0; i < 2; i++){						// two vertex streams: 0x10 each, plus stride * count when the stream has a buffer
		sz += 0x10;
		if(R32(h + 0x28 + 0x10 * i)) sz += R32(h + 0x54) * R32(h + 0x30 + 0x10 * i);
	}
	return sz;
}

extern "C" {

void __cdecl mflrw_d3d9nat_bind(void *const *tbl){
	for(int i = 0; i < V_N; i++) g_v[i] = tbl[i];
}

// _rpD3D9UsageFlagPluginAttach 0x509470 (): RwBool
i32 __cdecl rw37__rpD3D9UsageFlagPluginAttach(void){
	if(((EngRegFn)fn(V_ENGREG))(0, 0x50f, fn(V_ENGCTOR), fn(V_ENGDTOR)) < 0) return 0;
	i32 o = ((ObjRegFn)fn(V_GEOREG))(4, 0x50f, fn(V_OBJCTOR), fn(V_OBJCTOR), fn(V_OBJCOPY));
	*(i32 *)fn(V_GEOOFS) = o;						// _rpD3D9GeometryUsageFlagOffset 0x7d54d8 (stored before the test)
	if(o < 0) return 0;
	o = ((ObjRegFn)fn(V_SECREG))(4, 0x50f, fn(V_OBJCTOR), fn(V_OBJCTOR), fn(V_OBJCOPY));
	*(i32 *)fn(V_SECOFS) = o;						// _rpD3D9WorldSectorUsageFlagOffset 0x7d54dc
	return o >= 0;
}

// _rpGeometryNativeSize 0x501a30 (geometry): 0 unless rpGEOMETRYNATIVE (flags +8 bit 24); header at +0x58
u32 __cdecl rw37__rpGeometryNativeSize(u32 geom){
	if(!(R32(geom + 8) & 0x1000000)) return 0;
	return nativeSize(R32(geom + 0x58));
}

// _rpWorldSectorNativeSize 0x502130 (sector): 0 unless the sector has a header (+0x34) and its world is native (world flags +8 bit 24)
u32 __cdecl rw37__rpWorldSectorNativeSize(u32 sector){
	if(!R32(sector + 0x34)) return 0;
	if(!(R32(((GetWorldFn)fn(V_GETWORLD))(sector) + 8) & 0x1000000)) return 0;
	return nativeSize(R32(sector + 0x34));
}

// RpBuildMeshGenerateDefaultTriStrip 0x5021c0 (buildMesh, data): data unused
u32 __cdecl rw37_RpBuildMeshGenerateDefaultTriStrip(u32 buildMesh, u32 data){
	(void)data;
	return ((TriStripFn)fn(V_TRISTRIP))(buildMesh, 0, 1);
}

}
