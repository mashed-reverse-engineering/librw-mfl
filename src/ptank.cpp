// RpPTank subset for Mashed: atomic extension holding the particle arrays, Lock/Unlock with the game's stride semantics,
// CPU billboard instancing and an Im3D render path (backend independent: D3D9 / GL3 / anything with Im3D).
// Decompilation-informed (the game's statically linked PTank code: create 0x545170/0x545310/0x545940, lock 0x545d10,
// unlock 0x545f20, instancing 0x545f60 .. 0x548910); no SDK source or headers were copied. Guesses are marked "GUESS",
// deviations from the original "DEVIATION"; all collected in MFL_NOTES.md.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwrender.h"
#include "rwptank.h"

#include "ps2/rwps2.h"
#include "d3d/rwd3d.h"
#include "gl/rwgl3.h"

#define PLUGIN_ID ID_PTANK

namespace rw {
namespace ptank {

int32 atomicDataOffset = -1;

// layout of the original structure (32 bit builds only; on 64 bit the pointer members shift the later offsets)
#define OFF(f, v) static_assert(sizeof(void*) != 4 || offsetof(Data, f) == (v), "PTank Data layout: " #f)
OFF(activeCount, 0x04); OFF(block, 0x0c); OFF(origRenderCB, 0x10); OFF(callbacks, 0x14); OFF(lockedFlags, 0x3c);
OFF(updateFlags, 0x40); OFF(platformFlags, 0x44); OFF(arrays, 0x4c); OFF(numArrays, 0x98); OFF(totalStride, 0x9c);
OFF(formatFlags, 0xa0); OFF(srcBlend, 0xa4); OFF(dstBlend, 0xa8); OFF(blendEnable, 0xac); OFF(cnsCenter, 0xb0);
OFF(cnsSize, 0xb8); OFF(cnsRotate, 0xc0); OFF(cnsColor, 0xc4); OFF(cnsVtxColor, 0xc8); OFF(cnsTexCoords, 0xd8);
OFF(cnsMatrix, 0xf8);
static_assert(sizeof(void*) != 4 || sizeof(Data) == 0x138, "PTank Data size");

static QuadSink userSink;
static void *userSinkData;

void setQuadSink(QuadSink s, void *user) { userSink = s; userSinkData = user; }

// array order (index into Data::arrays), bit and element size
static const uint32 arrBit[NUMARRAYS]  = { DPOSITION, DMATRIX, DNORMALS, DSIZE, DCOLOR, DVTXCOLOR, DROTATE2D, DVTX2TEXCOORDS, DVTX4TEXCOORDS };
static const int32  arrSize[NUMARRAYS] = { 12, 64, 12, 8, 4, 16, 4, 16, 32 };

static int
arrIndex(uint32 bit)
{
	for(int i = 0; i < NUMARRAYS; i++)
		if(arrBit[i] == bit)
			return i;
	return -1;
}

uint32
normalizeFlags(uint32 f)
{
	// Reproduces the compiled behaviour of the original, including the lines that look like bugs:
	// "constant AND array requested" masks the whole word down to the constant bit (instead of just dropping the array).
	// all tests read the caller's original word (SPEC F1); only the result is masked
	const uint32 o = f;
	bool pos = (f & DPOSITION) != 0, size = (f & DSIZE) != 0, mat = (f & DMATRIX) != 0, rot = (f & DROTATE2D) != 0;
	bool vc = (f & DVTXCOLOR) != 0, t2 = (f & DVTX2TEXCOORDS) != 0, t4 = (f & DVTX4TEXCOORDS) != 0;
	if(mat && (o & DCNSMATRIX)) f &= DCNSMATRIX;
	if(rot && (o & DCNSROTATE2D)) f &= DCNSROTATE2D;
	if(vc && (o & DCNSVTXCOLOR)) f &= DCNSVTXCOLOR;
	if(t2 && (o & DCNSVTX2TEXCOORDS)) f &= DCNSVTX2TEXCOORDS;
	if(t4 && (o & DCNSVTX4TEXCOORDS)) f &= DCNSVTX4TEXCOORDS;
	if(mat){	// per particle matrix replaces position, size and 2D rotation
		if(pos) f &= ~DPOSITION;
		if(size) f &= ~DSIZE;
		if(rot) f &= ~DROTATE2D;
	}
	if(!mat && !pos)
		f |= DPOSITION;
	if((f & DLAYOUTMASK) == 0)
		f |= DLAYOUT_ARRAYS;
	return f;
}

// ---- plugin
static void*
ptankCtor(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(Data*, object, offset) = nil;
	return object;
}

static void*
ptankDtor(void *object, int32 offset, int32)
{
	Atomic *a = (Atomic*)object;
	Data *d = *PLUGINOFFSET(Data*, object, offset);
	if(d){
		// the original drops the texture of material 0 here; Atomic::destroy would do it through the geometry anyway
		if(a->geometry && a->geometry->matList.numMaterials > 0 && a->geometry->matList.materials[0])
			a->geometry->matList.materials[0]->setTexture(nil);
		if(d->block)
			rwFree(d->block);
		rwFree(d);
		*PLUGINOFFSET(Data*, object, offset) = nil;
	}
	return object;
}

static void*
ptankCopy(void *dst, void *, int32 offset, int32)
{
	// the original registers no copy constructor: a cloned atomic has no tank (render callback must cope)
	*PLUGINOFFSET(Data*, dst, offset) = nil;
	return dst;
}

int32
registerPlugin(void)
{
	atomicDataOffset = Atomic::registerPlugin(sizeof(Data*), PLUGIN_ID, ptankCtor, ptankDtor, ptankCopy);
	return atomicDataOffset;
}

// ---- create
static bool
allocArrays(Data *d, uint32 flags)
{
	int64 n = d->maxCount;
	bool interleaved = (flags & DLAYOUT_INTERLEAVED) != 0;
	int32 stride = 0;
	d->numArrays = 0;
	d->formatFlags = 0;
	for(int i = 0; i < NUMARRAYS; i++)
		if(flags & arrBit[i]){
			d->numArrays++;
			d->formatFlags |= arrBit[i];
			stride += arrSize[i];
		}
	// the constant bits and layout bits of the request stay in the format word (the original keeps only array bits here and
	// ORs the rest back later is not observable: Lock/render look at array bits and the CNS bits of the normalised word)
	d->formatFlags |= flags & ~(uint32)(DPOSITION|DCOLOR|DSIZE|DMATRIX|DNORMALS|DROTATE2D|DVTXCOLOR|DVTX2TEXCOORDS|DVTX4TEXCOORDS);
	int64 bytes = n*stride + 16;
	if(n < 0 || n > MAXPARTICLES || bytes > (int64)0x7FFFFFF0){
		RWERROR((ERR_ALLOC, (int32)bytes));
		return false;
	}
	// DEVIATION: the original does not clear the block (rwmalloc); we do, so reads of unwritten arrays are deterministic.
	uint8 *raw = (uint8*)rwMalloc((size_t)bytes, MEMDUR_EVENT | PLUGIN_ID);
	if(raw == nil){
		RWERROR((ERR_ALLOC, (int32)bytes));
		return false;
	}
	memset(raw, 0, (size_t)bytes);
	d->block = raw;
	uint8 *base = (uint8*)(((uintptr)raw + 15) & ~(uintptr)15);
	int32 off = 0;
	for(int i = 0; i < NUMARRAYS; i++){
		if(!(flags & arrBit[i]))
			continue;
		if(interleaved){
			d->arrays[i].data = base + off;
			d->arrays[i].stride = stride;
			off += arrSize[i];
		}else{
			d->arrays[i].data = base + (size_t)(n*off);
			d->arrays[i].stride = arrSize[i];
			off += arrSize[i];
		}
	}
	d->totalStride = interleaved ? stride : 0;
	return true;
}

static void
setDefaults(Data *d)
{
	d->cnsCenter[0] = d->cnsCenter[1] = 0.0f;
	d->cnsSize[0] = d->cnsSize[1] = 1.0f;
	d->cnsRotate = 0.0f;
	// 0xFF000000 as a little endian dword: r = g = b = 0, a = 255
	for(int i = 0; i < 4; i++)
		d->cnsColor[i] = i == 3 ? 0xFF : 0;
	for(int v = 0; v < 4; v++)
		for(int i = 0; i < 4; i++)
			d->cnsVtxColor[v][i] = i == 3 ? 0xFF : 0;
	// 8 floats 0,0,0,1,1,1,1,0 (u,v pairs (0,0) (0,1) (1,1) (1,0)); also read as {u0,v1,u1,v0} by the 2 coordinate variant
	static const float32 tc[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };
	memcpy(d->cnsTexCoords, tc, sizeof(tc));
	d->cnsMatrix.setIdentity();
	d->cnsMatrix.flags = 0x20003;	// identity | orthonormal
}

Atomic*
atomicCreate(int32 maxCount, uint32 flags, uint32 platformFlags)
{
	if(atomicDataOffset < 0){
		RWERROR((ERR_GENERAL, "PTank plugin not registered"));
		return nil;
	}
	flags = normalizeFlags(flags);
	Atomic *a = Atomic::create();
	if(a == nil)
		return nil;
	Data *d = (Data*)rwMalloc(sizeof(Data), MEMDUR_EVENT | PLUGIN_ID);
	if(d == nil){
		RWERROR((ERR_ALLOC, (int32)sizeof(Data)));
		a->destroy();
		return nil;
	}
	memset(d, 0, sizeof(*d));
	d->maxCount = maxCount;
	d->activeCount = 0;
	d->lockedFlags = 0;
	d->updateFlags = 0xFFFFFFFF;	// everything changed
	d->platformFlags = platformFlags;
	d->origRenderCB = a->renderCB;
	a->renderCB = render;
	*PLUGINOFFSET(Data*, a, atomicDataOffset) = d;	// from here the plugin destructor owns d
	if(!allocArrays(d, flags)){
		a->destroy();
		return nil;
	}
	// geometry with one material (the game binds the particle texture to it) and one morph target
	Geometry *geo = Geometry::create(0, 0, 0);
	Material *mat = geo ? Material::create() : nil;
	if(geo == nil || mat == nil){
		if(geo) geo->destroy();
		a->destroy();
		return nil;
	}
	mat->surfaceProps.ambient = 0.3f;	// original values (0x3e99999a, 1.0, 1.0)
	mat->surfaceProps.specular = 1.0f;
	mat->surfaceProps.diffuse = 1.0f;
	geo->matList.appendMaterial(mat);
	mat->destroy();	// the list holds its own reference
	a->setGeometry(geo, 0);
	geo->destroy();
	setDefaults(d);
	geo->morphTargets[0].boundingSphere.radius = 1.0f;	// original default (SPEC F8); center 0
	syncBoundingSphere(a);
	// blend state: the original samples the device state at create time
	d->srcBlend = d->dstBlend = d->blendEnable = 0;
	if(engine && engine->device.getRenderState){
		d->srcBlend = (int32)GetRenderState(SRCBLEND);
		d->dstBlend = (int32)GetRenderState(DESTBLEND);
		d->blendEnable = (int32)GetRenderState(VERTEXALPHA);
	}
	return a;
}

// ---- lock / unlock
bool32
atomicLock(Atomic *a, LockArray *out, uint32 which, uint32 lockMode)
{
	out->data = nil;
	out->stride = 0;
	Data *d = a ? getData(a) : nil;
	if(d == nil)
		return 0;
	int idx = arrIndex(which);	// DEVIATION: the original returns 1 with the out struct untouched for non single array bits
	if(idx < 0 || (d->formatFlags & which) == 0)
		return 0;
	out->data = d->arrays[idx].data;
	out->stride = d->arrays[idx].stride;
	if(d->totalStride != 0)
		out->stride = d->totalStride;
	if(out->data != nil && (lockMode & DLOCKWRITE) == DLOCKWRITE)
		d->lockedFlags |= which | lockMode;
	return 1;
}

void
atomicUnlock(Atomic *a)
{
	Data *d = a ? getData(a) : nil;
	if(d == nil)
		return;
	uint32 l = d->lockedFlags;
	d->lockedFlags = 0;
	d->updateFlags |= l;
}

void
syncBoundingSphere(Atomic *a)
{
	if(a->geometry && a->geometry->numMorphTargets > 0){
		a->boundingSphere = a->geometry->morphTargets[0].boundingSphere;
		a->object.object.privateFlags |= Atomic::WORLDBOUNDDIRTY;
		if(a->getFrame())
			a->getFrame()->updateObjects();
	}
}

// ---- instancing
static inline float32 rd(const uint8 *p, int i) { float32 f; memcpy(&f, p + 4*i, 4); return f; }

int32
buildQuads(Atomic *a, const Matrix *cam, int32 first, int32 n, QuadVertex *out)
{
	Data *d = a ? getData(a) : nil;
	if(d == nil || first < 0 || n <= 0)
		return 0;
	if(!(d->formatFlags & (DPOSITION|DMATRIX)))	// SPEC F3: the original reads an uninitialised pointer here
		return 0;
	int32 active = d->activeCount < d->maxCount ? d->activeCount : d->maxCount;	// DEVIATION: the original trusts the count
	if(first >= active)
		return 0;
	if(n > active - first)
		n = active - first;
	uint32 f = d->formatFlags;
	const ArrayDesc *A = d->arrays;
	enum { P, M, N, S, C, VC, R, T2, T4 };

	// billboard axes (game: R = -camera right, U = camera up; constant matrix replaces both; constant rotation turns them)
	V3d R_, U_;
	if(f & DCNSMATRIX){
		R_ = d->cnsMatrix.right;
		U_ = d->cnsMatrix.up;
	}else{
		R_.set(-cam->right.x, -cam->right.y, -cam->right.z);
		U_ = cam->up;
		if(f & DCNSROTATE2D){
			float32 c = cosf(d->cnsRotate), s = sinf(d->cnsRotate);
			V3d r2, u2;
			r2.set(c*R_.x - s*U_.x, c*R_.y - s*U_.y, c*R_.z - s*U_.z);
			u2.set(s*R_.x + c*U_.x, s*R_.y + c*U_.y, s*R_.z + c*U_.z);
			R_ = r2; U_ = u2;
		}
	}
	// handler selection as in the original: matrix; else (not CNSMATRIX) rotate/size arrays; else constant size
	bool matrix = (f & DMATRIX) != 0;
	bool rotArr = !matrix && !(f & DCNSMATRIX) && (f & DROTATE2D);
	bool sizeArr = !matrix && !(f & DCNSMATRIX) && (f & DSIZE);
	bool center = (f & DCENTER) != 0;

	for(int32 i = 0; i < n; i++){
		int32 k = first + i;
		QuadVertex *q = out + 4*i;
		float32 x[4], y[4], z[4];
		if(matrix){
			const uint8 *m = A[M].data + (size_t)k*A[M].stride;
			V3d right, up, pos;
			right.set(rd(m,0), rd(m,1), rd(m,2));
			up.set(rd(m,4), rd(m,5), rd(m,6));
			pos.set(rd(m,12), rd(m,13), rd(m,14));
			if(center){
				pos.x += d->cnsCenter[0]*right.x + d->cnsCenter[1]*up.x;
				pos.y += d->cnsCenter[0]*right.y + d->cnsCenter[1]*up.y;
				pos.z += d->cnsCenter[0]*right.z + d->cnsCenter[1]*up.z;
			}
			float32 hx = (right.x + up.x)*0.5f, hy = (right.y + up.y)*0.5f, hz = (right.z + up.z)*0.5f;
			float32 dx = (right.x - up.x)*0.5f, dy = (right.y - up.y)*0.5f, dz = (right.z - up.z)*0.5f;
			x[0] = pos.x + dx; y[0] = pos.y + dy; z[0] = pos.z + dz;
			x[1] = pos.x - hx; y[1] = pos.y - hy; z[1] = pos.z - hz;
			x[2] = pos.x - dx; y[2] = pos.y - dy; z[2] = pos.z - dz;
			x[3] = pos.x + hx; y[3] = pos.y + hy; z[3] = pos.z + hz;
		}else{
			const uint8 *p = A[P].data + (size_t)k*A[P].stride;
			float32 w, h;
			if(sizeArr){
				const uint8 *s = A[S].data + (size_t)k*A[S].stride;
				w = rd(s,0); h = rd(s,1);
			}else{
				w = d->cnsSize[0]; h = d->cnsSize[1];
				if(center && !rotArr)	// original quirk (SPEC F2): the constant size DCENTER handler uses size[0] for the height too
					h = w;
			}
			V3d r = R_, u = U_;
			if(rotArr){
				float32 ang = rd(A[R].data + (size_t)k*A[R].stride, 0);
				float32 c = cosf(ang), s = sinf(ang);
				V3d r2, u2;
				r2.set(c*R_.x - s*U_.x, c*R_.y - s*U_.y, c*R_.z - s*U_.z);
				u2.set(c*U_.x + s*R_.x, c*U_.y + s*R_.y, c*U_.z + s*R_.z);
				r = r2; u = u2;
			}
			float32 px = rd(p,0), py = rd(p,1), pz = rd(p,2);
			if(center){	// GUESS for the rotated/size array variants (only the plain one was read): offset in the (rotated) axes
				px += d->cnsCenter[0]*r.x + d->cnsCenter[1]*u.x;
				py += d->cnsCenter[0]*r.y + d->cnsCenter[1]*u.y;
				pz += d->cnsCenter[0]*r.z + d->cnsCenter[1]*u.z;
			}
			float32 ax = r.x*w*0.5f, ay = r.y*w*0.5f, az = r.z*w*0.5f;
			float32 bx = u.x*h*0.5f, by = u.y*h*0.5f, bz = u.z*h*0.5f;
			x[0] = px - ax - bx; y[0] = py - ay - by; z[0] = pz - az - bz;
			x[1] = px + ax - bx; y[1] = py + ay - by; z[1] = pz + az - bz;
			x[2] = px + ax + bx; y[2] = py + ay + by; z[2] = pz + az + bz;
			x[3] = px - ax + bx; y[3] = py - ay + by; z[3] = pz - az + bz;
		}
		// colour: per particle, per corner, constant per corner, else constant
		uint8 col[4][4];
		if(f & DCOLOR){
			const uint8 *c = A[C].data + (size_t)k*A[C].stride;
			for(int v = 0; v < 4; v++) memcpy(col[v], c, 4);
		}else if(f & DVTXCOLOR){
			const uint8 *c = A[VC].data + (size_t)k*A[VC].stride;
			memcpy(col, c, 16);
		}else if(f & DCNSVTXCOLOR){
			memcpy(col, d->cnsVtxColor, 16);
		}else{
			for(int v = 0; v < 4; v++) memcpy(col[v], d->cnsColor, 4);
		}
		// texture coordinates
		float32 tu[4], tv[4];
		if(f & DVTX2TEXCOORDS){
			const uint8 *t = A[T2].data + (size_t)k*A[T2].stride;
			float32 u0 = rd(t,0), v1 = rd(t,1), u1 = rd(t,2), v0 = rd(t,3);	// {uLeft, vTop, uRight, vBottom}
			tu[0] = u0; tv[0] = v0; tu[1] = u1; tv[1] = v0; tu[2] = u1; tv[2] = v1; tu[3] = u0; tv[3] = v1;
		}else if(f & DVTX4TEXCOORDS){
			const uint8 *t = A[T4].data + (size_t)k*A[T4].stride;
			for(int v = 0; v < 4; v++){ tu[v] = rd(t, 2*v); tv[v] = rd(t, 2*v+1); }
		}else if(f & DCNSVTX2TEXCOORDS){
			const float32 *t = d->cnsTexCoords;
			tu[0] = t[0]; tv[0] = t[3]; tu[1] = t[2]; tv[1] = t[3]; tu[2] = t[2]; tv[2] = t[1]; tu[3] = t[0]; tv[3] = t[1];
		}else if(f & DCNSVTX4TEXCOORDS){
			for(int v = 0; v < 4; v++){ tu[v] = d->cnsTexCoords[2*v]; tv[v] = d->cnsTexCoords[2*v+1]; }
		}else{	// DEVIATION: the original leaves the coordinates unwritten; use the whole texture
			tu[0] = 0; tv[0] = 1; tu[1] = 1; tv[1] = 1; tu[2] = 1; tv[2] = 0; tu[3] = 0; tv[3] = 0;
		}
		float32 nx = 0, ny = 0, nz = 0;
		if(f & DNORMALS){
			const uint8 *nn = A[N].data + (size_t)k*A[N].stride;
			nx = rd(nn,0); ny = rd(nn,1); nz = rd(nn,2);
		}
		for(int v = 0; v < 4; v++){
			QuadVertex *o = &q[v];
			o->x = x[v]; o->y = y[v]; o->z = z[v];
			o->nx = nx; o->ny = ny; o->nz = nz;
			o->r = col[v][0]; o->g = col[v][1]; o->b = col[v][2]; o->a = col[v][3];
			o->u = tu[v]; o->v = tv[v];
		}
	}
	return n;
}

// ---- render
enum { BATCH = 256 };

#ifdef RWDEVICE
static void
im3dDraw(Texture *tex, const QuadVertex *q, int32 nq, bool blend)
{
	static RWDEVICE::Im3DVertex verts[BATCH*4];
	static uint16 idx[BATCH*6];
	static bool init;
	if(!init){
		for(int i = 0; i < BATCH; i++){	// same triangles as the game's index buffer: (0,1,2) (0,2,3)
			uint16 b = i*4, *x = &idx[i*6];
			x[0] = b; x[1] = b+1; x[2] = b+2; x[3] = b; x[4] = b+2; x[5] = b+3;
		}
		init = true;
	}
	for(int i = 0; i < nq*4; i++){
		RWDEVICE::Im3DVertex *v = &verts[i];
		v->setX(q[i].x); v->setY(q[i].y); v->setZ(q[i].z);
		v->setColor(q[i].r, q[i].g, q[i].b, q[i].a);
		v->setU(q[i].u); v->setV(q[i].v);
	}
	uint32 flags = im3d::VERTEXUV | im3d::VERTEXXYZ | im3d::VERTEXRGBA | im3d::NOCLIP;
	if(!blend)
		flags |= im3d::ALLOPAQUE;
	im3d::Transform(verts, nq*4, nil, flags);	// world transform = identity: particle positions are world space
	im3d::RenderIndexedPrimitive(PRIMTYPETRILIST, idx, nq*6);
	im3d::End();
}
#endif

void
render(Atomic *a)
{
	Data *d = atomicDataOffset >= 0 ? getData(a) : nil;
	if(d == nil || d->activeCount <= 0)	// the original does nothing for count <= 0 (and leaves updateFlags alone)
		return;
	Camera *cam = (Camera*)engine->currentCamera;
	if(cam == nil || cam->getFrame() == nil)
		return;
	Matrix *ltm = cam->getFrame()->getLTM();
	Texture *tex = nil;
	if(a->geometry && a->geometry->matList.numMaterials > 0 && a->geometry->matList.materials[0])
		tex = a->geometry->matList.materials[0]->texture;
	// the update step of the original: everything is (re)instanced each draw here, so only the bookkeeping remains
	d->updateFlags = 0;

	static QuadVertex quads[BATCH*4];
	int32 total = d->activeCount < d->maxCount ? d->activeCount : d->maxCount;
	uint32 saveSrc = 0, saveDst = 0;
	bool state = userSink == nil && engine->device.getRenderState != nil;
#ifdef RWDEVICE
	if(state){
		saveSrc = GetRenderState(SRCBLEND);
		saveDst = GetRenderState(DESTBLEND);
		SetRenderStatePtr(TEXTURERASTER, tex ? tex->raster : nil);
		if(tex){
			SetRenderState(TEXTUREFILTER, tex->getFilter());
			SetRenderState(TEXTUREADDRESSU, tex->getAddressU());
			SetRenderState(TEXTUREADDRESSV, tex->getAddressV());
		}
		if(d->blendEnable){
			SetRenderState(SRCBLEND, d->srcBlend);
			SetRenderState(DESTBLEND, d->dstBlend);
			SetRenderState(VERTEXALPHA, 1);
		}else
			SetRenderState(VERTEXALPHA, 0);
	}
#endif
	for(int32 first = 0; first < total; first += BATCH){
		int32 n = buildQuads(a, ltm, first, BATCH, quads);
		if(n <= 0)
			break;
		if(userSink)
			userSink(userSinkData, tex, quads, n);
#ifdef RWDEVICE
		else
			im3dDraw(tex, quads, n, d->blendEnable != 0);
#endif
	}
#ifdef RWDEVICE
	if(state){
		if(d->blendEnable){
			SetRenderState(SRCBLEND, saveSrc);
			SetRenderState(DESTBLEND, saveDst);
		}
		// VERTEXALPHA is deliberately not restored (SPEC F4: the original leaks it); cull/Z state is left alone (F5)
	}
#endif
	(void)saveSrc; (void)saveDst; (void)state;
}

}
}
