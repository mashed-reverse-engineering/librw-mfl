// PROPOSED replacement of librw-mfl/capi/layout_check.cpp (A0 raw-offset audit, ghidra/output/rw_offset_audit_2026-11-03.md).
// RW 3.7 32-bit layout of every struct field the game reads by offset (output/rw_offset_audit_2026-11-03.csv, output/rwsurf/fields_used.csv,
// SDK headers via DWARF: ghidra/tools/rwaudit/dwarf_layout.py).
//   FIELDS / WIDTHS   rows librw-mfl already satisfies: enforced by -DLAYOUT_STRICT (add it to tools/build_mflrw.sh now)
//   TODO_FIELDS / TODO_WIDTHS   rows librw-mfl does NOT satisfy yet (S2 reorder targets): enforced only by -DLAYOUT_STRICT_ALL
//   -DLAYOUT_MAIN : prints both tables "ok/MISMATCH name got want" and exits 1 on any mismatch
//   (neither)     : exports rwmfl_layout_check(), the number of mismatches (TODO rows included)
#include <stddef.h>
#include <stdio.h>
#include "../rw.h"
using namespace rw;
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#define MSIZE(T, m) sizeof(((T *)0)->m)

#define FIELDS(X) \
	X(Object_type, offsetof(Object, type), 0x0) \
	X(Object_flags, offsetof(Object, flags), 0x2) \
	X(Object_parent, offsetof(Object, parent), 0x4) \
	X(Frame_object_parent, offsetof(Frame, object) + offsetof(Object, parent), 0x4) \
	X(Frame_modelling, offsetof(Frame, matrix), 0x10) \
	X(Frame_ltm, offsetof(Frame, ltm), 0x50) \
	X(Frame_child, offsetof(Frame, child), 0x98) \
	X(Frame_root, offsetof(Frame, root), 0xa0) \
	X(Atomic_object_type, offsetof(Atomic, object) + offsetof(Object, type), 0x0) \
	X(Atomic_object_parent, offsetof(Atomic, object) + offsetof(Object, parent), 0x4) \
	X(Atomic_geometry, offsetof(Atomic, geometry), 0x18) \
	X(Atomic_boundingSphere, offsetof(Atomic, boundingSphere), 0x1c) \
	X(Atomic_clump, offsetof(Atomic, clump), 0x3c) \
	X(Atomic_inClumpLink, offsetof(Atomic, inClump), 0x40) \
	X(Atomic_renderCB, offsetof(Atomic, renderCB), 0x48) \
	X(Atomic_interpolator_flags, offsetof(Atomic, interpolatorFlags), 0x4c) \
	X(Atomic_interpolator_time, offsetof(Atomic, interpTime), 0x54) \
	X(Atomic_pipeline, offsetof(Atomic, pipeline), 0x6c) \
	X(HAnimHierarchy_pMatrixArray, offsetof(HAnimHierarchy, matrices), 0x8) \
	X(HAnimHierarchy_currentAnim, offsetof(HAnimHierarchy, interpolator), 0x20) \
	X(Clump_object_parent, offsetof(Clump, object) + offsetof(Object, parent), 0x4) \
	X(Clump_atomicList, offsetof(Clump, atomics), 0x8) \
	X(Clump_lightList, offsetof(Clump, lights), 0x10) \
	X(Clump_cameraList, offsetof(Clump, cameras), 0x18) \
	X(Clump_inWorldLink, offsetof(Clump, inWorld), 0x20) \
	X(Clump_size, sizeof(Clump), 0x2c) \
	X(World_matList_materials, offsetof(World, matList) + offsetof(MaterialList, materials), 0x10) \
	X(World_matList_numMaterials, offsetof(World, matList) + offsetof(MaterialList, numMaterials), 0x14) \
	X(World_renderCallBack, offsetof(World, sectorRenderCB), 0x68) \
	X(WorldSector_triangles, offsetof(WorldSector, triangles), 0x4) \
	X(WorldSector_vertices, offsetof(WorldSector, vertices), 0x8) \
	X(WorldSector_normals, offsetof(WorldSector, normals), 0xc) \
	X(WorldSector_texCoords, offsetof(WorldSector, texCoords), 0x10) \
	X(WorldSector_preLitLum, offsetof(WorldSector, colors), 0x30) \
	X(WorldSector_boundingBox, offsetof(WorldSector, planeBBox), 0x48) \
	X(WorldSector_tightBoundingBox, offsetof(WorldSector, bbox), 0x60) \
	X(WorldSector_mesh, offsetof(WorldSector, mesh), 0x78) \
	X(WorldSector_pipeline, offsetof(WorldSector, pipeline), 0x7c) \
	X(WorldSector_matListWindowBase, offsetof(WorldSector, matIdBase), 0x80) \
	X(WorldSector_numVertices, offsetof(WorldSector, numVertices), 0x82) \
	X(WorldSector_numTriangles, offsetof(WorldSector, numTriangles), 0x84) \
	X(WorldSector_size, sizeof(WorldSector), 0x88) \
	X(Light_object_flags, offsetof(Light, object) + offsetof(Object, flags), 0x2) \
	X(Light_object_parent, offsetof(Light, object) + offsetof(Object, parent), 0x4) \
	X(Light_radius, offsetof(Light, radius), 0x14) \
	X(Light_color, offsetof(Light, color), 0x18) \
	X(Light_minusCosAngle, offsetof(Light, minusCosAngle), 0x28) \
	X(Geometry_flags, offsetof(Geometry, flags), 0x8) \
	X(Geometry_lockedSinceLastInst, offsetof(Geometry, lockedSinceInst), 0xc) \
	X(Geometry_numVertices, offsetof(Geometry, numVertices), 0x14) \
	X(Geometry_matList_materials, offsetof(Geometry, matList) + offsetof(MaterialList, materials), 0x20) \
	X(Geometry_matList_numMaterials, offsetof(Geometry, matList) + offsetof(MaterialList, numMaterials), 0x24) \
	X(Geometry_triangles, offsetof(Geometry, triangles), 0x2c) \
	X(Geometry_preLitLum, offsetof(Geometry, colors), 0x30) \
	X(Geometry_texCoords, offsetof(Geometry, texCoords), 0x34) \
	X(Geometry_mesh, offsetof(Geometry, meshHeader), 0x54) \
	X(Geometry_morphTarget, offsetof(Geometry, morphTargets), 0x5c) \
	X(MorphTarget_boundingSphere, offsetof(MorphTarget, boundingSphere), 0x4) \
	X(MorphTarget_verts, offsetof(MorphTarget, vertices), 0x14) \
	X(MorphTarget_normals, offsetof(MorphTarget, normals), 0x18) \
	X(Material_texture, offsetof(Material, texture), 0x0) \
	X(Material_color, offsetof(Material, color), 0x4) \
	X(Material_pipeline, offsetof(Material, pipeline), 0x8) \
	X(Material_surfaceProps, offsetof(Material, surfaceProps), 0xc) \
	X(Raster_parent, offsetof(Raster, parent), 0x0) \
	X(Raster_width, offsetof(Raster, width), 0xc) \
	X(Raster_height, offsetof(Raster, height), 0x10) \
	X(Raster_depth, offsetof(Raster, depth), 0x14) \
	X(Raster_stride, offsetof(Raster, stride), 0x18) \
	X(Raster_cType, offsetof(Raster, type), 0x20) \
	X(Texture_raster, offsetof(Texture, raster), 0x0) \
	X(Texture_dict, offsetof(Texture, dict), 0x4) \
	X(Texture_lInDictionary, offsetof(Texture, inDict), 0x8) \
	X(Texture_name, offsetof(Texture, name), 0x10) \
	X(Texture_mask, offsetof(Texture, mask), 0x30) \
	X(Texture_filterAddressing, offsetof(Texture, filterAddressing), 0x50) \
	X(Texture_refCount, offsetof(Texture, refCount), 0x54) \
	X(Camera_object_parent, offsetof(Camera, object) + offsetof(Object, parent), 0x4) \
	X(Camera_projectionType, offsetof(Camera, projection), 0x14) \
	X(Camera_viewMatrix, offsetof(Camera, viewMatrix), 0x20) \
	X(Camera_frameBuffer, offsetof(Camera, frameBuffer), 0x60) \
	X(Camera_zBuffer, offsetof(Camera, zBuffer), 0x64) \
	X(Camera_viewWindow, offsetof(Camera, viewWindow), 0x68) \
	X(Camera_nearPlane, offsetof(Camera, nearPlane), 0x80) \
	X(Camera_farPlane, offsetof(Camera, farPlane), 0x84) \
	X(Camera_fogPlane, offsetof(Camera, fogPlane), 0x88) \
	X(Matrix_size, sizeof(Matrix), 0x40) \
	X(Matrix_up, offsetof(Matrix, up), 0x10) \
	X(Matrix_at, offsetof(Matrix, at), 0x20) \
	X(Matrix_pos, offsetof(Matrix, pos), 0x30) \
	/* rows from output/rw_offset_audit_2026-11-03.csv: typed game/rwp keys at the same librw offset, not asserted before (L3b 2026-11-05) */ \
	X(Atomic_object_flags, offsetof(Atomic, object) + offsetof(Object, flags), 0x2) \
	X(Geometry_object_type, offsetof(Geometry, object) + offsetof(Object, type), 0x0) \
	X(Material_color_alpha, offsetof(Material, color) + offsetof(RGBA, alpha), 0x7) \
	X(Material_surfaceProps_specular, offsetof(Material, surfaceProps) + offsetof(SurfaceProperties, specular), 0x10) \
	X(Material_surfaceProps_diffuse, offsetof(Material, surfaceProps) + offsetof(SurfaceProperties, diffuse), 0x14) \
	X(Matrix_right_y, offsetof(Matrix, right) + offsetof(V3d, y), 0x4) \
	X(Matrix_right_z, offsetof(Matrix, right) + offsetof(V3d, z), 0x8) \
	X(Matrix_flags, offsetof(Matrix, flags), 0xc) \
	X(Matrix_up_y, offsetof(Matrix, up) + offsetof(V3d, y), 0x14) \
	X(Matrix_up_z, offsetof(Matrix, up) + offsetof(V3d, z), 0x18) \
	X(Matrix_at_y, offsetof(Matrix, at) + offsetof(V3d, y), 0x24) \
	X(Matrix_at_z, offsetof(Matrix, at) + offsetof(V3d, z), 0x28) \
	X(Matrix_pos_y, offsetof(Matrix, pos) + offsetof(V3d, y), 0x34) \
	X(Matrix_pos_z, offsetof(Matrix, pos) + offsetof(V3d, z), 0x38) \
	/* E4 2026-11-06: the remaining "librw same offset / has <field> at" rows of the audit (closure table: ghidra/output/rw_offset_audit_closure_2026-11-06.csv) */ \
	X(Atomic_object_privateFlags, offsetof(Atomic, object) + offsetof(Object, privateFlags), 0x3) \
	X(Atomic_boundingSphere_radius, offsetof(Atomic, boundingSphere) + offsetof(Sphere, radius), 0x28) \
	X(Atomic_inClumpLink_prev, offsetof(Atomic, inClump) + offsetof(LLLink, prev), 0x44) \
	X(LLLink_next, offsetof(LLLink, next), 0x0) \
	X(LinkList_link_next, offsetof(LinkList, link) + offsetof(LLLink, next), 0x0) \
	X(V3d_x, offsetof(V3d, x), 0x0) \
	X(Matrix_right, offsetof(Matrix, right), 0x0) \
	X(Camera_beginUpdate, offsetof(Camera, beginUpdateCB), 0x18) \
	X(Camera_endUpdate, offsetof(Camera, endUpdateCB), 0x1c) \
	X(Geometry_refCount, offsetof(Geometry, refCount), 0xe) \
	X(Image_flags, offsetof(Image, flags), 0x0) \
	X(TexDictionary_texturesInDict, offsetof(TexDictionary, textures), 0x8)

// field widths the game reads / writes with a fixed size (RW 3.7 width)
#define WIDTHS(Y) \
	Y(Object_flags_w, MSIZE(Object, flags), 1) \
	Y(Matrix_flags_w, MSIZE(Matrix, flags), 4) \
	Y(Atomic_interpolator_flags_w, MSIZE(Atomic, interpolatorFlags), 4) \
	Y(Geometry_lockedSinceLastInst_w, MSIZE(Geometry, lockedSinceInst), 2) \
	Y(Geometry_refCount_w, MSIZE(Geometry, refCount), 2) \
	Y(Material_color_w, MSIZE(Material, color), 4) \
	Y(Texture_filterAddressing_w, MSIZE(Texture, filterAddressing), 4) \
	Y(Texture_refCount_w, MSIZE(Texture, refCount), 4) \
	Y(Raster_width_w, MSIZE(Raster, width), 4) \
	Y(Camera_viewWindow_w, MSIZE(Camera, viewWindow), 8) \
	Y(WorldSector_numTriangles_w, MSIZE(WorldSector, numTriangles), 2) \
	Y(WorldSector_matListWindowBase_w, MSIZE(WorldSector, matIdBase), 2) \
	Y(Light_color_w, MSIZE(Light, color), 16) \
	Y(Camera_viewMatrix_w, MSIZE(Camera, viewMatrix), 64)

// NOT satisfied by librw-mfl today (each found by the A0 audit or required by the exe's D3D9 driver / RtFS code sharing the object;
// World / WorldSector / Clump.inWorldLink were reordered to RW 3.7 and are enforced above):
//   Raster: the exe D3D9 driver and rw37.h share rasters byte for byte (cType/cFlags/privateFlags/cFormat are bytes at 0x20..0x23)
//   Camera: RwCameraCalcPixelRay (tier2 C) reads frustumCorners at 0x124 / 0x154; recipViewWindow/viewOffset are swapped in librw
//   Stream: T-L7 lesson, the original keeps RtFile / memory position at stream+0xc; librw's Stream is a C++ object (vtable at +0)
#define TODO_FIELDS(X) \
	X(Raster_nOffsetX, offsetof(Raster, offsetX), 0x1c) \
	X(Raster_nOffsetY, offsetof(Raster, offsetY), 0x1e) \
	X(Raster_cFlags, offsetof(Raster, flags), 0x21) \
	X(Raster_privateFlags, offsetof(Raster, privateFlags), 0x22) \
	X(Raster_cFormat, offsetof(Raster, format), 0x23) \
	X(Raster_originalPixels, offsetof(Raster, originalPixels), 0x24) \
	X(Raster_originalWidth, offsetof(Raster, originalWidth), 0x28) \
	X(Raster_originalHeight, offsetof(Raster, originalHeight), 0x2c) \
	X(Raster_originalStride, offsetof(Raster, originalStride), 0x30) \
	X(Raster_size_extOffset, sizeof(Raster), 0x34) \
	X(Texture_size, sizeof(Texture), 0x58) \
	X(Atomic_size, sizeof(Atomic), 0x70) \
	X(Light_size, sizeof(Light), 0x40) \
	X(Camera_recipViewWindow, offsetof(Camera, recipViewWindow), 0x70) \
	X(Camera_viewOffset, offsetof(Camera, viewOffset), 0x78) \
	X(Camera_frustumBoundBox, offsetof(Camera, frustumBoundBox), 0x10c) \
	X(Camera_frustumCorners, offsetof(Camera, frustumCorners), 0x124) \
	X(Camera_size, sizeof(Camera), 0x184) \
	X(Stream_memory_position, offsetof(StreamMemory, position), 0xc) \
	X(Stream_file_fpFile, offsetof(StreamFile, file), 0xc)

#define TODO_WIDTHS(Y) \
	Y(Raster_cType_w, MSIZE(Raster, type), 1) \
	Y(Raster_nOffsetX_w, MSIZE(Raster, offsetX), 2) \
	Y(Material_refCount_w, MSIZE(Material, refCount), 2)
// no librw member at all: RpPTankAtomicExtPrv (game writes actPCount 0x4, instFlags 0x40, srcBlend/dstBlend/vertexAlphaBlend 0xa4..0xac, cSize 0xb8/0xbc through
// g_ptank_plugin_offset): librw has no PTank plugin.

#if defined(LAYOUT_STRICT) || defined(LAYOUT_STRICT_ALL)
#define X(n, got, want) static_assert((got) == (want), #n);
#define Y(n, got, want) static_assert((got) == (want), #n);
FIELDS(X)
WIDTHS(Y)
#ifdef LAYOUT_PLANT	// negative fixture (tools/layout_fixture.sh): one wrong row must fail the build
X(Planted_Matrix_pos, offsetof(Matrix, pos), 0x34)
#endif
#ifdef LAYOUT_STRICT_ALL
TODO_FIELDS(X)
TODO_WIDTHS(Y)
#endif
#undef X
#undef Y
extern "C" int rwmfl_layout_check(void){ return 0; }	// rwmfl.def exports it: strict builds must define it too (link error otherwise)
#else
struct Row { const char *name; size_t got, want; int todo; };
#define X(n, got, want) { #n, (size_t)(got), (size_t)(want), 0 },
#define XT(n, got, want) { #n, (size_t)(got), (size_t)(want), 1 },
static const Row rows[] = { FIELDS(X) WIDTHS(X) TODO_FIELDS(XT) TODO_WIDTHS(XT) };
#undef X
#undef XT
static int check(int print){
	int bad = 0;
	for(size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++){
		bad += rows[i].got != rows[i].want;
		if(print) printf("%s %s%s got 0x%x want 0x%x\n", rows[i].got == rows[i].want ? "ok      " : "MISMATCH", rows[i].todo ? "[todo] " : "", rows[i].name, (unsigned)rows[i].got, (unsigned)rows[i].want);
	}
	return bad;
}
#ifdef LAYOUT_MAIN
int main(void){ int b = check(1); printf("%d mismatches of %d\n", b, (int)(sizeof(rows)/sizeof(rows[0]))); return b != 0; }
#else
extern "C" int rwmfl_layout_check(void){ return check(0); }
#endif
#endif
