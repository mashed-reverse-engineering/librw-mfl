#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwextra.h"

#define PLUGIN_ID 0

namespace rw {
namespace extra {

// ---- RpRandom -------------------------------------------------------------

static uint32 rndBlock[31];
static uint32 *rndFront, *rndRear;
static bool rndInit;

static uint32
rndStep(void)
{
	*rndFront += *rndRear;
	uint32 r = *rndFront >> 1;
	rndFront++;
	if(rndFront < rndBlock+31){
		rndRear++;
		if(rndRear >= rndBlock+31)
			rndRear = rndBlock;
	}else{
		rndFront = rndBlock;
		rndRear++;
	}
	return r;
}

void
randomSeed(uint32 seed)
{
	rndBlock[0] = seed;
	for(int32 i = 1; i < 31; i++)
		rndBlock[i] = rndBlock[i-1]*0x41C64E6Du + 0x3039u;
	rndFront = rndBlock+3;
	rndRear = rndBlock;
	rndInit = true;
	for(int32 i = 0; i < 310; i++)	// discard the warm-up
		rndStep();
}

uint32
random(void)
{
	if(!rndInit)
		randomSeed(0x9A319039u);
	return rndStep();
}

// ---- RpSpline -------------------------------------------------------------

// Solves for the Hermite derivatives D of an interpolating C2 cubic spline.
// Open: natural ends. Closed: periodic. Per component.
// ponytail: closed loop uses Gauss-Seidel (diagonally dominant, halves the error per sweep);
// unverified numerically against the original spline (it is the same C2 spline in theory).
static void
solveDerivatives(const V3d *p, int32 n, bool closed, V3d *d)
{
	for(int32 c = 0; c < 3; c++){
		const float32 *P = &p[0].x + c;
		double *mem = rwNewT(double, 4*n, MEMDUR_FUNCTION | ID_NAOBJECT);
		double *rhs = mem, *D = mem+n, *cp = mem+2*n, *dp = mem+3*n;
		#define PV(i) ((double)P[3*(((i)%n+n)%n)])
		if(closed){
			for(int32 i = 0; i < n; i++){
				rhs[i] = 3.0*(PV(i+1) - PV(i-1));
				D[i] = rhs[i]/4.0;
			}
			for(int32 it = 0; it < 64; it++)
				for(int32 i = 0; i < n; i++)
					D[i] = (rhs[i] - D[(i+n-1)%n] - D[(i+1)%n])/4.0;
		}else if(n == 2){
			D[0] = D[1] = PV(1) - PV(0);
		}else{
			for(int32 i = 0; i < n; i++){
				double a = i == 0 ? 0.0 : 1.0;
				double b = (i == 0 || i == n-1) ? 2.0 : 4.0;
				double cc = i == n-1 ? 0.0 : 1.0;
				rhs[i] = i == 0 ? 3.0*(PV(1)-PV(0)) :
				         i == n-1 ? 3.0*(PV(n-1)-PV(n-2)) :
				         3.0*(PV(i+1)-PV(i-1));
				double m = b - a*(i ? cp[i-1] : 0.0);
				cp[i] = cc/m;
				dp[i] = (rhs[i] - a*(i ? dp[i-1] : 0.0))/m;
			}
			D[n-1] = dp[n-1];
			for(int32 i = n-2; i >= 0; i--)
				D[i] = dp[i] - cp[i]*D[i+1];
		}
		#undef PV
		for(int32 i = 0; i < n; i++)
			(&d[i].x)[c] = (float32)D[i];
		rwFree(mem);
	}
}

Spline*
Spline::create(int32 numCtrlPoints, int32 type, const V3d *ctrlPoints)
{
	if(numCtrlPoints < 2 || (type != SPLINE_OPENLOOP && type != SPLINE_CLOSEDLOOP) ||
	   numCtrlPoints > 0x100000){
		RWERROR((ERR_GENERAL, "bad spline"));
		return nil;
	}
	Spline *s = rwNewT(Spline, 1, MEMDUR_EVENT | ID_NAOBJECT);
	if(s == nil)
		return nil;
	int32 n = numCtrlPoints;
	s->numCtrlPoints = n;
	s->type = type;
	memset(s->name, 0, sizeof(s->name));
	s->ctrlPoints = rwNewT(V3d, n, MEMDUR_EVENT | ID_NAOBJECT);
	s->bezier = rwNewT(V3d, 2*n, MEMDUR_EVENT | ID_NAOBJECT);
	V3d *d = rwNewT(V3d, n, MEMDUR_FUNCTION | ID_NAOBJECT);
	if(!s->ctrlPoints || !s->bezier || !d){
		rwFree(d);
		s->destroy();
		return nil;
	}
	memcpy(s->ctrlPoints, ctrlPoints, n*sizeof(V3d));
	bool closed = type == SPLINE_CLOSEDLOOP;
	solveDerivatives(ctrlPoints, n, closed, d);
	for(int32 i = 0; i < s->numSegments(); i++){
		int32 j = (i+1)%n;
		s->bezier[2*i]   = add(ctrlPoints[i], scale(d[i], 1.0f/3.0f));
		s->bezier[2*i+1] = sub(ctrlPoints[j], scale(d[j], 1.0f/3.0f));
	}
	rwFree(d);
	return s;
}

void
Spline::destroy(void)
{
	rwFree(this->ctrlPoints);
	rwFree(this->bezier);
	rwFree(this);
}

Spline*
Spline::clone(void) const
{
	Spline *s = create(this->numCtrlPoints, this->type, this->ctrlPoints);
	if(s)
		memcpy(s->name, this->name, sizeof(s->name));
	return s;
}

V3d*
Spline::findPosition(int32 path, float32 where, V3d *pos, V3d *tangent)
{
	if(where < 0.0f) where = 0.0f;
	if(where > 1.0f) where = 1.0f;
	if(path == SPLINE_PATHNICEENDS)		// unverified shaping, ease in/out
		where = where*where*(3.0f - 2.0f*where);
	int32 nseg = this->numSegments();
	float32 u = where*nseg;
	int32 i = (int32)u;
	if(i >= nseg) i = nseg-1;
	float32 t = u - i, it = 1.0f - t;
	V3d p0 = this->ctrlPoints[i], p3 = this->ctrlPoints[(i+1)%this->numCtrlPoints];
	V3d p1 = this->bezier[2*i], p2 = this->bezier[2*i+1];
	if(pos){
		float32 a = it*it*it, b = 3*it*it*t, c = 3*it*t*t, d = t*t*t;
		*pos = makeV3d(a*p0.x + b*p1.x + c*p2.x + d*p3.x,
		               a*p0.y + b*p1.y + c*p2.y + d*p3.y,
		               a*p0.z + b*p1.z + c*p2.z + d*p3.z);
	}
	if(tangent){
		// d/d(where) of the position, unnormalised (unverified: scale of the original)
		float32 a = 3*it*it, b = 6*it*t, c = 3*t*t;
		V3d d0 = sub(p1, p0), d1 = sub(p2, p1), d2 = sub(p3, p2);
		*tangent = scale(makeV3d(a*d0.x + b*d1.x + c*d2.x,
		                         a*d0.y + b*d1.y + c*d2.y,
		                         a*d0.z + b*d1.z + c*d2.z), (float32)nseg);
	}
	return pos;
}

float32
Spline::findMatrix(int32 path, float32 where, const V3d *up, Matrix *m)
{
	V3d pos, tan;
	findPosition(path, where, &pos, &tan);
	float32 len = length(tan);
	V3d at = len > 1e-12f ? scale(tan, 1.0f/len) : makeV3d(0.0f, 0.0f, 1.0f);
	V3d right = cross(*up, at);
	len = length(right);
	if(len < 1e-6f)		// up parallel to the path: any perpendicular
		right = fabsf(at.x) < 0.9f ? cross(makeV3d(1.0f, 0.0f, 0.0f), at) :
		                             cross(makeV3d(0.0f, 1.0f, 0.0f), at);
	right = normalize(right);
	m->right = right;
	m->at = at;
	m->up = cross(at, right);
	m->pos = pos;
	m->flags = Matrix::TYPEORTHONORMAL;
	m->pad1 = m->pad2 = 0;
	m->pad3 = 0x3F800000;
	return where;
}

Spline*
Spline::streamRead(Stream *stream)
{
	char name[SPLINE_NAMELENGTH];
	stream->read8(name, sizeof(name));
	int32 n = stream->readI32();
	int32 type = stream->readI32();
	if(n < 2 || n > 0x100000 || (type != SPLINE_OPENLOOP && type != SPLINE_CLOSEDLOOP))
		return nil;
	V3d *pts = rwNewT(V3d, n, MEMDUR_FUNCTION | ID_NAOBJECT);
	if(pts == nil)
		return nil;
	stream->read32(pts, 12*n);
	Spline *s = create(n, type, pts);
	rwFree(pts);
	if(s)
		memcpy(s->name, name, sizeof(name));
	return s;
}

bool
Spline::streamWrite(Stream *stream) const
{
	stream->write8(this->name, sizeof(this->name));
	stream->writeI32(this->numCtrlPoints);
	stream->writeI32(this->type);
	stream->write32(this->ctrlPoints, 12*this->numCtrlPoints);
	return true;
}

// ---- RpLODAtomic ----------------------------------------------------------

struct LODData
{
	Geometry *geos[LOD_MAXLOD];
	int32 current;
	float32 range;
	LODCallBack callback;
	Atomic::RenderCB prevRender;
	bool32 hooked;
};

static int32 lodOffset = -1;
static Camera *lodCamera;

static LODData* lodData(Atomic *a) { return PLUGINOFFSET(LODData, a, lodOffset); }

static void*
lodCtor(void *object, int32 offset, int32)
{
	LODData *d = PLUGINOFFSET(LODData, object, offset);
	memset(d, 0, sizeof(*d));
	d->current = -1;
	d->range = 1.0f;
	return object;
}

static void*
lodDtor(void *object, int32 offset, int32)
{
	LODData *d = PLUGINOFFSET(LODData, object, offset);
	for(int32 i = 0; i < LOD_MAXLOD; i++)
		if(d->geos[i])
			d->geos[i]->destroy();
	return object;
}

static void*
lodCopy(void *dst, void *src, int32 offset, int32)
{
	LODData *d = PLUGINOFFSET(LODData, dst, offset);
	LODData *s = PLUGINOFFSET(LODData, src, offset);
	*d = *s;
	for(int32 i = 0; i < LOD_MAXLOD; i++)
		if(d->geos[i])
			d->geos[i]->addRef();
	return dst;
}

void
registerLODPlugin(void)
{
	lodOffset = Atomic::registerPlugin(sizeof(LODData), MAKEPLUGINID(VEND_CRITERIONTK, 0x12),
	                                   lodCtor, lodDtor, lodCopy);
}

Atomic*
lodSetGeometry(Atomic *atomic, int32 idx, Geometry *geo)
{
	if(lodOffset < 0 || idx < 0 || idx >= LOD_MAXLOD)
		return nil;
	LODData *d = lodData(atomic);
	if(geo)
		geo->addRef();
	if(d->geos[idx])
		d->geos[idx]->destroy();
	d->geos[idx] = geo;
	if(d->current < 0 || d->current == idx){
		// first geometry becomes the current one, a replaced current one is re-applied
		d->current = idx;
		atomic->setGeometry(geo, 0);
	}
	return atomic;
}

Geometry*
lodGetGeometry(Atomic *atomic, int32 idx)
{
	if(lodOffset < 0 || idx < 0 || idx >= LOD_MAXLOD)
		return nil;
	return lodData(atomic)->geos[idx];
}

Atomic*
lodSetCurrentLOD(Atomic *atomic, int32 idx)
{
	if(lodOffset < 0 || idx < 0 || idx >= LOD_MAXLOD)
		return nil;
	LODData *d = lodData(atomic);
	if(d->geos[idx] == nil)
		return nil;
	if(d->current != idx){
		d->current = idx;
		atomic->setGeometry(d->geos[idx], 0);
	}
	return atomic;
}

int32
lodGetCurrentLOD(Atomic *atomic)
{
	return lodOffset < 0 ? -1 : lodData(atomic)->current;
}

Atomic*
lodSetRange(Atomic *atomic, float32 farRange)
{
	lodData(atomic)->range = farRange;
	return atomic;
}

float32
lodGetRange(Atomic *atomic)
{
	return lodData(atomic)->range;
}

void
lodSetCamera(Camera *cam)
{
	lodCamera = cam;
}

Atomic*
lodSetCallBack(Atomic *atomic, LODCallBack cb)
{
	lodData(atomic)->callback = cb;
	return atomic;
}

// Default selection (unverified against the original): one LOD step per `range`
// of camera distance, clamped to the finest/coarsest geometry that exists.
Atomic*
lodSelectLOD(Atomic *atomic)
{
	LODData *d = lodData(atomic);
	int32 idx;
	if(d->callback)
		idx = d->callback(atomic);
	else{
		Camera *cam = lodCamera ? lodCamera : (Camera*)engine->currentCamera;
		if(cam == nil || cam->getFrame() == nil || atomic->getFrame() == nil || d->range <= 0.0f)
			return atomic;
		V3d diff = sub(atomic->getFrame()->getLTM()->pos, cam->getFrame()->getLTM()->pos);
		idx = (int32)(length(diff)/d->range);
	}
	if(idx >= LOD_MAXLOD) idx = LOD_MAXLOD-1;
	while(idx > 0 && d->geos[idx] == nil)
		idx--;
	if(idx < 0) idx = 0;
	lodSetCurrentLOD(atomic, idx);
	return atomic;
}

static void
lodRenderCB(Atomic *atomic)
{
	LODData *d = lodData(atomic);
	lodSelectLOD(atomic);
	Atomic::RenderCB prev = d->prevRender ? d->prevRender : Atomic::defaultRenderCB;
	prev(atomic);
}

Atomic*
lodHookRender(Atomic *atomic)
{
	LODData *d = lodData(atomic);
	if(!d->hooked){
		d->prevRender = atomic->renderCB;
		d->hooked = 1;
		atomic->setRenderCB(lodRenderCB);
	}
	return atomic;
}

Atomic*
lodUnHookRender(Atomic *atomic)
{
	LODData *d = lodData(atomic);
	if(d->hooked){
		atomic->setRenderCB(d->prevRender);
		d->hooked = 0;
	}
	return atomic;
}

// ---- RpMorph --------------------------------------------------------------

struct MorphGeoData
{
	MorphInterpolator *interps;
	int32 numInterps;
	MorphCallBack callback;
};
struct MorphAtomicData
{
	int32 current;
	float32 time;
};
static int32 morphGeoOffset = -1, morphAtomOffset = -1;

static MorphGeoData* geoData(Geometry *g) { return PLUGINOFFSET(MorphGeoData, g, morphGeoOffset); }
static MorphAtomicData* atomData(Atomic *a) { return PLUGINOFFSET(MorphAtomicData, a, morphAtomOffset); }

static void*
mgCtor(void *o, int32 off, int32){
	memset(PLUGINOFFSET(MorphGeoData, o, off), 0, sizeof(MorphGeoData)); return o; }
static void*
mgDtor(void *o, int32 off, int32){
	rwFree(PLUGINOFFSET(MorphGeoData, o, off)->interps); return o; }
static void*
mgCopy(void *dst, void *src, int32 off, int32)
{
	MorphGeoData *d = PLUGINOFFSET(MorphGeoData, dst, off);
	MorphGeoData *s = PLUGINOFFSET(MorphGeoData, src, off);
	*d = *s;
	d->interps = nil;
	if(s->numInterps){
		d->interps = rwNewT(MorphInterpolator, s->numInterps, MEMDUR_EVENT | ID_GEOMETRY);
		memcpy(d->interps, s->interps, s->numInterps*sizeof(MorphInterpolator));
		for(int32 i = 0; i < d->numInterps; i++)
			if(s->interps[i].next)
				d->interps[i].next = d->interps + (s->interps[i].next - s->interps);
	}
	return dst;
}
static void*
maCtor(void *o, int32 off, int32){
	MorphAtomicData *d = PLUGINOFFSET(MorphAtomicData, o, off); d->current = 0; d->time = 0.0f; return o; }
static void*
maCopy(void *dst, void *src, int32 off, int32){
	*PLUGINOFFSET(MorphAtomicData, dst, off) = *PLUGINOFFSET(MorphAtomicData, src, off); return dst; }

void
registerMorphPlugin(void)
{
	uint32 id = MAKEPLUGINID(VEND_CRITERIONTK, 0x05);
	morphGeoOffset = Geometry::registerPlugin(sizeof(MorphGeoData), id, mgCtor, mgDtor, mgCopy);
	morphAtomOffset = Atomic::registerPlugin(sizeof(MorphAtomicData), id, maCtor, nil, maCopy);
}

Geometry*
morphCreateInterpolators(Geometry *geo, int32 n)
{
	if(morphGeoOffset < 0 || n < 0)
		return nil;
	MorphGeoData *d = geoData(geo);
	rwFree(d->interps);
	d->interps = nil;
	d->numInterps = 0;
	if(n){
		d->interps = rwNewT(MorphInterpolator, n, MEMDUR_EVENT | ID_GEOMETRY);
		if(d->interps == nil)
			return nil;
		memset(d->interps, 0, n*sizeof(MorphInterpolator));
		d->numInterps = n;
	}
	return geo;
}

MorphInterpolator*
morphGetInterpolator(Geometry *geo, int32 i)
{
	if(morphGeoOffset < 0)
		return nil;
	MorphGeoData *d = geoData(geo);
	return i >= 0 && i < d->numInterps ? &d->interps[i] : nil;
}

Geometry*
morphSetInterpolator(Geometry *geo, int32 i, int32 start, int32 end, float32 time)
{
	MorphInterpolator *m = morphGetInterpolator(geo, i);
	if(m == nil || start < 0 || end < 0 || start >= geo->numMorphTargets || end >= geo->numMorphTargets)
		return nil;
	m->startMorphTarget = (int16)start;
	m->endMorphTarget = (int16)end;
	m->time = time;
	m->recipTime = time > 0.0f ? 1.0f/time : 0.0f;
	return geo;
}

Geometry*
morphSetNextInterpolator(Geometry *geo, int32 i, int32 next)
{
	MorphInterpolator *m = morphGetInterpolator(geo, i), *n = morphGetInterpolator(geo, next);
	if(m == nil || n == nil)
		return nil;
	m->next = n;
	return geo;
}

Geometry*
morphSetCallBack(Geometry *geo, MorphCallBack cb)
{
	if(morphGeoOffset < 0)
		return nil;
	geoData(geo)->callback = cb;
	return geo;
}

MorphCallBack
morphGetCallBack(Geometry *geo)
{
	return morphGeoOffset < 0 ? nil : geoData(geo)->callback;
}

Atomic*
morphSetCurrentInterpolator(Atomic *atomic, int32 i)
{
	if(morphAtomOffset < 0 || atomic->geometry == nil || morphGetInterpolator(atomic->geometry, i) == nil)
		return nil;
	MorphAtomicData *d = atomData(atomic);
	d->current = i;
	d->time = 0.0f;
	return atomic;
}

int32
morphGetCurrentInterpolator(Atomic *atomic)
{
	return morphAtomOffset < 0 ? -1 : atomData(atomic)->current;
}

Atomic*
morphSetTime(Atomic *atomic, float32 time)
{
	if(morphAtomOffset < 0 || atomic->geometry == nil)
		return nil;
	MorphAtomicData *d = atomData(atomic);
	MorphInterpolator *m = morphGetInterpolator(atomic->geometry, d->current);
	if(m == nil)
		return nil;
	// unverified: the original may wrap through m->next when time passes the end;
	// here the time is clamped.
	d->time = time < 0.0f ? 0.0f : time > m->time ? m->time : time;
	return atomic;
}

Atomic*
morphAddTime(Atomic *atomic, float32 time)
{
	if(morphAtomOffset < 0)
		return nil;
	return morphSetTime(atomic, atomData(atomic)->time + time);
}

float32
morphGetTime(Atomic *atomic)
{
	return atomData(atomic)->time;
}

float32
morphGetBlend(Atomic *atomic)
{
	if(morphAtomOffset < 0 || atomic->geometry == nil)
		return 0.0f;
	MorphAtomicData *d = atomData(atomic);
	MorphInterpolator *m = morphGetInterpolator(atomic->geometry, d->current);
	return m ? d->time*m->recipTime : 0.0f;
}

// ---- RpPVS ----------------------------------------------------------------

bool
pvsQuery(World *world)
{
	(void)world;
	return false;
}

}
}
