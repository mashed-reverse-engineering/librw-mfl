// RW 3.7 32-bit layout of every struct field the game reads by offset (output/rwsurf/fields_used.csv, SDK headers).
// librw-mfl's structs are reordered to these offsets in S2; until then the mismatches are expected (see the table).
//   -DLAYOUT_STRICT : static_assert per field, the build fails on any drift (S2 acceptance)
//   -DLAYOUT_MAIN   : prints the table "ok/MISMATCH name got want" and exits 1 on any mismatch
//   (neither)       : exports rwmfl_layout_check(), the number of mismatches
#include <stddef.h>
#include <stdio.h>
#include "../rw.h"
using namespace rw;
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

#define FIELDS(X) \
	X(Object_type, offsetof(Object, type), 0x0) \
	X(Object_flags, offsetof(Object, flags), 0x2) \
	X(Object_parent, offsetof(Object, parent), 0x4) \
	X(Frame_object_parent, offsetof(Frame, object) + offsetof(Object, parent), 0x4) \
	X(Frame_modelling, offsetof(Frame, matrix), 0x10) \
	X(Atomic_object_type, offsetof(Atomic, object) + offsetof(Object, type), 0x0) \
	X(Atomic_object_parent, offsetof(Atomic, object) + offsetof(Object, parent), 0x4) \
	X(Atomic_geometry, offsetof(Atomic, geometry), 0x18) \
	X(Atomic_boundingSphere, offsetof(Atomic, boundingSphere), 0x1c) \
	X(Atomic_renderCB, offsetof(Atomic, renderCB), 0x48) \
	X(Atomic_interpolator_flags, offsetof(Atomic, interpolatorFlags), 0x4c) \
	X(Atomic_pipeline, offsetof(Atomic, pipeline), 0x6c) \
	X(HAnimHierarchy_currentAnim, offsetof(HAnimHierarchy, interpolator), 0x20) \
	X(Clump_object_parent, offsetof(Clump, object) + offsetof(Object, parent), 0x4) \
	X(Light_object_flags, offsetof(Light, object) + offsetof(Object, flags), 0x2) \
	X(Light_object_parent, offsetof(Light, object) + offsetof(Object, parent), 0x4) \
	X(Geometry_flags, offsetof(Geometry, flags), 0x8) \
	X(Geometry_numVertices, offsetof(Geometry, numVertices), 0x14) \
	X(Geometry_matList_materials, offsetof(Geometry, matList) + offsetof(MaterialList, materials), 0x20) \
	X(Geometry_triangles, offsetof(Geometry, triangles), 0x2c) \
	X(Geometry_preLitLum, offsetof(Geometry, colors), 0x30) \
	X(Geometry_texCoords, offsetof(Geometry, texCoords), 0x34) \
	X(Geometry_morphTarget, offsetof(Geometry, morphTargets), 0x5c) \
	X(MorphTarget_boundingSphere, offsetof(MorphTarget, boundingSphere), 0x4) \
	X(MorphTarget_verts, offsetof(MorphTarget, vertices), 0x14) \
	X(MorphTarget_normals, offsetof(MorphTarget, normals), 0x18) \
	X(Material_texture, offsetof(Material, texture), 0x0) \
	X(Material_color, offsetof(Material, color), 0x4) \
	X(Material_surfaceProps, offsetof(Material, surfaceProps), 0xc) \
	X(Raster_width, offsetof(Raster, width), 0xc) \
	X(Raster_height, offsetof(Raster, height), 0x10) \
	X(Texture_filterAddressing, offsetof(Texture, filterAddressing), 0x50) \
	X(Camera_object_parent, offsetof(Camera, object) + offsetof(Object, parent), 0x4) \
	X(Camera_projectionType, offsetof(Camera, projection), 0x14) \
	X(Camera_frameBuffer, offsetof(Camera, frameBuffer), 0x60) \
	X(Camera_zBuffer, offsetof(Camera, zBuffer), 0x64) \
	X(Camera_viewWindow, offsetof(Camera, viewWindow), 0x68) \
	X(Camera_fogPlane, offsetof(Camera, fogPlane), 0x88) \
	X(Matrix_size, sizeof(Matrix), 0x40) \
	X(Matrix_pos, offsetof(Matrix, pos), 0x30)

#ifdef LAYOUT_STRICT
#define X(n, got, want) static_assert((got) == (want), #n);
FIELDS(X)
#else
#undef X
struct Row { const char *name; size_t got, want; };
#define X(n, got, want) { #n, (size_t)(got), (size_t)(want) },
static const Row rows[] = { FIELDS(X) };
#undef X
static int check(int print){
	int bad = 0;
	for(size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++){
		bad += rows[i].got != rows[i].want;
		if(print) printf("%s %s got 0x%x want 0x%x\n", rows[i].got == rows[i].want ? "ok      " : "MISMATCH", rows[i].name, (unsigned)rows[i].got, (unsigned)rows[i].want);
	}
	return bad;
}
#ifdef LAYOUT_MAIN
int main(void){ int b = check(1); printf("%d mismatches of %d\n", b, (int)(sizeof(rows)/sizeof(rows[0]))); return b != 0; }
#else
extern "C" int rwmfl_layout_check(void){ return check(0); }
#endif
#endif
