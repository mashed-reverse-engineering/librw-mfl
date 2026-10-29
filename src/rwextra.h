// Small RenderWare toolkit plugins used by Mashed: RpRandom, RpSpline, RpLODAtomic, RpMorph, RpPVS.
// Written from the documented API (plugin headers) and the observed behaviour of the game's
// statically linked routines; no SDK source copied. See MFL_NOTES.md "Small plugins" for what is
// verified and what is not. Not included from rw.h: #include "rwextra.h" after rw.h. Namespace rw::extra.
#ifndef RW_EXTRA_H
#define RW_EXTRA_H

namespace rw {
namespace extra {

// ---- RpRandom -------------------------------------------------------------
// Additive lagged generator (31 words, lag 3) seeded by an LCG, as the game's RpRandom behaves:
// bit-exact for RpRandom/RpRandomSeed (the MT variant is not implemented, the game never calls it).
// ponytail: state is a plain global instead of an engine plugin slot.
void randomSeed(uint32 seed);
uint32 random(void);		// 31 bit value; the default state equals randomSeed(0x9A319039)

// ---- RpSpline -------------------------------------------------------------
enum {
	SPLINE_NAMELENGTH = 32,
	SPLINE_OPENLOOP   = 1,
	SPLINE_CLOSEDLOOP = 2,
	SPLINE_PATHSMOOTH   = 10,
	SPLINE_PATHNICEENDS = 11
};
struct Spline
{
	int32 numCtrlPoints;
	int32 type;
	char name[SPLINE_NAMELENGTH];
	V3d *ctrlPoints;
	V3d *bezier;		// per segment i: inner Bezier points [2*i], [2*i+1] (C2 interpolating spline)

	int32 numSegments(void) const {
		return type == SPLINE_CLOSEDLOOP ? numCtrlPoints : numCtrlPoints-1; }
	static Spline *create(int32 numCtrlPoints, int32 type, const V3d *ctrlPoints);
	void destroy(void);
	Spline *clone(void) const;
	// position (and tangent if non-nil) at parameter where in [0,1]
	V3d *findPosition(int32 path, float32 where, V3d *pos, V3d *tangent);
	// right/up/at/pos matrix at the parameter; returns where (unverified: the original's return value)
	float32 findMatrix(int32 path, float32 where, const V3d *up, Matrix *matrix);
	// chunk-less file format: char name[32]; int32 numCtrl; int32 type; V3d ctrl[numCtrl]
	static Spline *streamRead(Stream *stream);
	bool streamWrite(Stream *stream) const;
	uint32 streamGetSize(void) const { return 32 + 8 + 12*numCtrlPoints; }
};

// ---- RpLODAtomic ----------------------------------------------------------
enum { LOD_MAXLOD = 10 };
typedef int32 (*LODCallBack)(Atomic *atomic);
void registerLODPlugin(void);		// RpLODAtomicPluginAttach; before any Atomic is created
Atomic *lodSetGeometry(Atomic *atomic, int32 lodIdx, Geometry *geo);
Geometry *lodGetGeometry(Atomic *atomic, int32 lodIdx);
Atomic *lodSetCurrentLOD(Atomic *atomic, int32 lodIdx);
int32 lodGetCurrentLOD(Atomic *atomic);	// -1 if the atomic has no LOD data
Atomic *lodSetRange(Atomic *atomic, float32 farRange);
float32 lodGetRange(Atomic *atomic);
void lodSetCamera(Camera *cam);
Atomic *lodSetCallBack(Atomic *atomic, LODCallBack cb);
Atomic *lodSelectLOD(Atomic *atomic);
Atomic *lodHookRender(Atomic *atomic);
Atomic *lodUnHookRender(Atomic *atomic);

// ---- RpMorph --------------------------------------------------------------
// Data model and API only. unverified: stream layout of the plugin chunks, and the
// render path does not blend morph targets yet (no librw pipeline does).
struct MorphInterpolator
{
	int32 flags;
	int16 startMorphTarget, endMorphTarget;
	float32 time, recipTime;
	MorphInterpolator *next;
};
typedef float32 (*MorphCallBack)(Atomic *atomic, float32 position);
void registerMorphPlugin(void);		// RpMorphPluginAttach; before any Geometry/Atomic is created
Geometry *morphCreateInterpolators(Geometry *geo, int32 numInterps);
Geometry *morphSetInterpolator(Geometry *geo, int32 interpNum, int32 startKey, int32 endKey, float32 time);
Geometry *morphSetNextInterpolator(Geometry *geo, int32 interpNum, int32 interpNumNext);
Geometry *morphSetCallBack(Geometry *geo, MorphCallBack cb);
MorphCallBack morphGetCallBack(Geometry *geo);
MorphInterpolator *morphGetInterpolator(Geometry *geo, int32 interpNum);
Atomic *morphSetCurrentInterpolator(Atomic *atomic, int32 interpNum);
int32 morphGetCurrentInterpolator(Atomic *atomic);
Atomic *morphSetTime(Atomic *atomic, float32 time);	// clamps into the interpolator's [0,time]
Atomic *morphAddTime(Atomic *atomic, float32 time);
float32 morphGetTime(Atomic *atomic);
// blend factor (0..1) between start and end target for the current interpolator and time
float32 morphGetBlend(Atomic *atomic);

// ---- RpPVS ----------------------------------------------------------------
// ponytail: no potentially-visible-set data is read (not verified to exist in the game's BSPs);
// the query reports "no PVS" so callers fall back to normal culling.
bool pvsQuery(World *world);

}
}
#endif
