// RpPTank (particle tank) subset used by Mashed (Fully Loaded / Drive to Survive).
// Written from the game's call sites and the observed behaviour of the game's statically linked PTank routines
// (decompilation-informed, no SDK source/headers copied). See MFL_NOTES.md "PTank" for provenance, offsets, unknowns.
// Not included from rw.h: #include "rwptank.h" after rw.h. Namespace rw::ptank.
#ifndef RW_PTANK_H
#define RW_PTANK_H

namespace rw {
namespace ptank {

enum { ID_PTANK = 0x12F };	// atomic extension plugin id (original: vendor 0, id 0x12f, no stream support)

// Create flags = array bits (also the Lock "which array" argument and, with CNS bits, the update-flag bits).
enum DFlags {
	DPOSITION       = 0x00000001,	// RwV3d per particle (12)
	DCOLOR          = 0x00000002,	// RGBA per particle (4)
	DSIZE           = 0x00000004,	// RwV2d per particle (8): billboard width/height
	DMATRIX         = 0x00000008,	// Matrix per particle (64): quad from right/up/pos, replaces POSITION|SIZE|ROTATE2D
	DNORMALS        = 0x00000010,	// RwV3d per particle (12)
	DROTATE2D       = 0x00000020,	// float per particle (4): radians, rotates the billboard axes
	DVTXCOLOR       = 0x00000040,	// 4 x RGBA per particle (16): one colour per corner
	DVTX2TEXCOORDS  = 0x00000080,	// 4 floats per particle (16): {u0, v1, u1, v0}, see QuadVertex docs in ptank.cpp
	DVTX4TEXCOORDS  = 0x00000100,	// 8 floats per particle (32): u,v for each of the 4 corners
	// constant (per tank) versions, stored in the plugin data and not allocated per particle
	DCNSCOLOR       = 0x00002000,	// (update flag only, constant colour exists always)
	DCNSSIZE        = 0x00004000,	// (update flag only)
	DCNSMATRIX      = 0x00008000,	// use cnsMatrix right/up as the billboard axes instead of the camera
	DCNSROTATE2D    = 0x00020000,	// rotate the camera axes by cnsRotate (radians)
	DCNSVTXCOLOR    = 0x00040000,
	DCNSVTX2TEXCOORDS = 0x00080000,
	DCNSVTX4TEXCOORDS = 0x00100000,
	DCENTER         = 0x01000000,	// shift billboards by cnsCenter[0]*right + cnsCenter[1]*up (variant selector, game never sets it)
	DACTIVE         = 0x00800000,	// update flag: active count changed (game ORs it into updateFlags)
	DLAYOUT_ARRAYS  = 0x10000000,	// separate arrays, contiguous (default when neither layout bit is given)
	DLAYOUT_INTERLEAVED = 0x20000000,	// one interleaved structure per particle; Lock returns the total stride
	DLAYOUTMASK     = 0x30000000,
	DLOCKWRITE      = 0x40000000	// Lock mode: the caller will write, mark the array changed
};

enum { NUMARRAYS = 9 };
enum { MAXPARTICLES = 1<<22 };	// DEVIATION (hardening): allocation guard

struct LockArray { uint8 *data; int32 stride; };	// the two words the game's lock struct holds (pointer, stride in bytes)

struct ArrayDesc { uint8 *data; int32 stride; };

// Per-atomic plugin data. Field order mirrors the game's 0x138 byte structure; the offsets in the comments are those of the
// original (32 bit). The game pokes some of these directly (activeCount, updateFlags, blend, cns*), so they are public.
struct Data
{
	int32 maxCount;			// 0x00 particle capacity given to create
	int32 activeCount;		// 0x04 number of particles drawn (first N); the game writes it directly
	int32 unused08;			// 0x08 never written by the original
	uint8 *block;			// 0x0c raw allocation (original: rwmalloc, 16 aligned inside), freed by the destructor
	Atomic::RenderCB origRenderCB;	// 0x10 render callback the atomic had before (unused here)
	void *callbacks[10];		// 0x14..0x38 original: create/instance/update/render function pointers (not reproduced)
	uint32 lockedFlags;		// 0x3c arrays locked for write since the last unlock
	uint32 updateFlags;		// 0x40 changed bits; Unlock ORs lockedFlags in, render clears it (create: ~0)
	uint32 platformFlags;		// 0x44 third argument of create
	int32 unused48;			// 0x48
	ArrayDesc arrays[NUMARRAYS];	// 0x4c position, matrix, normals, size, color, vtxcolor, rotate2d, vtx2tc, vtx4tc
	int32 unused94;			// 0x94
	int32 numArrays;		// 0x98
	int32 totalStride;		// 0x9c interleaved layout only (0 otherwise); Lock returns it as the stride
	uint32 formatFlags;		// 0xa0 normalised create flags
	int32 srcBlend;			// 0xa4 rw::BlendFunction, default = device state at create
	int32 dstBlend;			// 0xa8
	int32 blendEnable;		// 0xac vertex alpha / blending on (game sets 1)
	float32 cnsCenter[2];		// 0xb0
	float32 cnsSize[2];		// 0xb8 default 1,1 (game sets 0.1,0.1 and ORs DCNSSIZE)
	float32 cnsRotate;		// 0xc0
	uint8 cnsColor[4];		// 0xc4 RGBA, default 0,0,0,255
	uint8 cnsVtxColor[4][4];	// 0xc8
	float32 cnsTexCoords[8];	// 0xd8 DCNSVTX4TEXCOORDS: 4 u,v pairs; DCNSVTX2TEXCOORDS reads the first 4 floats {u0,v1,u1,v0}
	Matrix cnsMatrix;		// 0xf8 default identity
};

extern int32 atomicDataOffset;	// original __rpPTankAtomicDataOffset (offset of the Data* inside the atomic)
inline Data *getData(Atomic *a) { return *PLUGINOFFSET(Data*, a, atomicDataOffset); }

// RpPTankPluginAttach: register the atomic extension (call between Engine::init and Engine::open, like the other plugins).
// Returns the plugin offset (<0 on failure).
int32 registerPlugin(void);

// Normalisation done by RpPTankAtomicCreate before the allocation (reproduces the original incl. its quirks).
uint32 normalizeFlags(uint32 flags);

// RpPTankAtomicCreate(maxCount, flags, platformFlags); nil on failure. The atomic has a geometry with one material
// (the game binds the particle texture to material 0), one morph target whose bounding sphere the game sets.
Atomic *atomicCreate(int32 maxCount, uint32 flags, uint32 platformFlags);
// RpPTankAtomicLock: array bit in `which` (exactly one of DPOSITION..DVTX4TEXCOORDS), lockMode DLOCKWRITE to mark it changed.
// Returns 1 and fills out, or 0 with out = {nil,0} when the array does not exist.
bool32 atomicLock(Atomic *a, LockArray *out, uint32 which, uint32 lockMode);
// RpPTankAtomicUnlock
void atomicUnlock(Atomic *a);
// RpPTankAtomicDestroy is Atomic::destroy (the plugin destructor frees the data and drops the material texture).

// The atomic's bounding sphere is copied from the geometry at setGeometry time only; the game writes the morph target sphere
// after create. Call this (or write Atomic::boundingSphere yourself) before culling with Atomic::getWorldBoundingSphere.
void syncBoundingSphere(Atomic *a);

// ---- instancing (CPU), the backend independent part of rendering
struct QuadVertex { float32 x, y, z, nx, ny, nz; uint8 r, g, b, a; float32 u, v; };
// Builds the 4 corners (bottom-left, bottom-right, top-right, top-left in billboard space) of particles [first, first+n)
// into out[0..4n). camLTM is the camera frame LTM (the game's R = -camera right, U = camera up). Returns n (clamped to the
// active count), 0 if there is no data.
int32 buildQuads(Atomic *a, const Matrix *camLTM, int32 first, int32 n, QuadVertex *out);
// Atomic render callback installed by atomicCreate (Im3D when RWDEVICE is a backend with Im3D, else only the sink).
void render(Atomic *a);
// Test hook: receive the quads instead of drawing them (quads are batches of 4 vertices, tex may be nil).
typedef void (*QuadSink)(void *user, Texture *tex, const QuadVertex *quads, int32 numQuads);
void setQuadSink(QuadSink sink, void *user);

}
}

#endif
