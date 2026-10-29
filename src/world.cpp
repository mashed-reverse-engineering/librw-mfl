#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwanim.h"
#include "rwplugins.h"
#include "rwcollision.h"

#define PLUGIN_ID ID_WORLD

namespace rw {

int32 World::numAllocated = 0;

PluginList World::s_plglist(sizeof(World));
PluginList WorldSector::s_plglist(sizeof(WorldSector));

static void defaultSectorRender(WorldSector *sector){ sector->renderDefault(); }

World*
World::create(BBox *bbox)
{
	World *world = (World*)rwMalloc(s_plglist.size, MEMDUR_EVENT | ID_WORLD);
	if(world == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	numAllocated++;
	world->object.init(World::ID, 0);
	world->localLights.init();
	world->globalLights.init();
	world->clumps.init();
	world->matList.init();
	world->root = nil;
	world->sectors = nil;
	world->numSectors = world->numPlaneSectors = 0;
	world->numTriangles = world->numVertices = 0;
	world->colSectorSize = 0;
	world->format = 0;
	world->invOrigin.set(0.0f, 0.0f, 0.0f);
	world->sectorRenderCB = defaultSectorRender;
	if(bbox)
		world->bbox = *bbox;
	else{
		world->bbox.inf.set(0.0f, 0.0f, 0.0f);
		world->bbox.sup.set(0.0f, 0.0f, 0.0f);
	}
	s_plglist.construct(world);
	return world;
}

static void
destroyNode(BspNode *n)
{
	if(n == nil)
		return;
	if(n->isLeaf){
		WorldSector *s = (WorldSector*)n;
		if(s->atomic){
			Frame *f = s->atomic->getFrame();
			s->atomic->destroy();	// also drops the geometry
			if(f)
				f->destroy();
		}else if(s->geometry)
			s->geometry->destroy();
		// triangles block also holds colors and texcoords only for Geometry;
		// here every array is separate
		rwFree(s->vertices);
		rwFree(s->normals);
		rwFree(s->colors);
		for(int32 i = 0; i < 8; i++)
			rwFree(s->texCoords[i]);
		rwFree(s->triangles);
		rwFree(s->collision);
		if(s->collTree)
			s->collTree->destroy();
		WorldSector::s_plglist.destruct(s);
	}else{
		PlaneSector *p = (PlaneSector*)n;
		destroyNode(p->left);
		destroyNode(p->right);
	}
	rwFree(n);
}

void
World::destroy(void)
{
	destroyNode(this->root);
	rwFree(this->sectors);
	this->matList.deinit();
	s_plglist.destruct(this);
	rwFree(this);
	numAllocated--;
}

void
World::addLight(Light *light)
{
	assert(light->world == nil);
	light->world = this;
	if(light->getType() < Light::POINT){
		this->globalLights.append(&light->inWorld);
	}else{
		this->localLights.append(&light->inWorld);
		if(light->getFrame())
			light->getFrame()->updateObjects();
	}
}

void
World::removeLight(Light *light)
{
	assert(light->world == this);
	light->inWorld.remove();
	light->world = nil;
}

void
World::addCamera(Camera *cam)
{
	assert(cam->world == nil);
	cam->world = this;
	if(cam->getFrame())
		cam->getFrame()->updateObjects();
}

void
World::removeCamera(Camera *cam)
{
	assert(cam->world == this);
	cam->world = nil;
}

void
World::addAtomic(Atomic *atomic)
{
	assert(atomic->world == nil);
	atomic->world = this;
	if(atomic->getFrame())
		atomic->getFrame()->updateObjects();
}

void
World::removeAtomic(Atomic *atomic)
{
	assert(atomic->world == this);
	atomic->world = nil;
}

void
World::addClump(Clump *clump)
{
	assert(clump->world == nil);
	clump->world = this;
	this->clumps.add(&clump->inWorld);
	FORLIST(lnk, clump->atomics)
		this->addAtomic(Atomic::fromClump(lnk));
	FORLIST(lnk, clump->lights)
		this->addLight(Light::fromClump(lnk));
	FORLIST(lnk, clump->cameras)
		this->addCamera(Camera::fromClump(lnk));

	if(clump->getFrame()){
		clump->getFrame()->matrix.optimize();
		clump->getFrame()->updateObjects();
	}
}

void
World::removeClump(Clump *clump)
{
	assert(clump->world == this);
	clump->inWorld.remove();
	FORLIST(lnk, clump->atomics)
		this->removeAtomic(Atomic::fromClump(lnk));
	FORLIST(lnk, clump->lights)
		this->removeLight(Light::fromClump(lnk));
	FORLIST(lnk, clump->cameras)
		this->removeCamera(Camera::fromClump(lnk));
	clump->world = nil;
}

void
World::render(void)
{
	// BSP sectors need a camera; use renderSectors(cam) for them
	FORLIST(lnk, this->clumps)
		Clump::fromWorld(lnk)->render();
}

// Find lights that illuminate an atomic
void
World::enumerateLights(Atomic *atomic, WorldLights *lightData)
{
	int32 maxDirectionals, maxLocals;

	maxDirectionals = lightData->numDirectionals;
	maxLocals = lightData->numLocals;

	lightData->numDirectionals = 0;
	lightData->numLocals = 0;
	lightData->numAmbients = 0;
	lightData->ambient.red = 0.0f;
	lightData->ambient.green = 0.0f;
	lightData->ambient.blue = 0.0f;
	lightData->ambient.alpha = 1.0f;

	bool32 normals = atomic->geometry->flags & Geometry::NORMALS;

	FORLIST(lnk, this->globalLights){
		Light *l = Light::fromWorld(lnk);
		if((l->getFlags() & Light::LIGHTATOMICS) == 0)
			continue;
		if(l->getType() == Light::AMBIENT){
			lightData->ambient.red   += l->color.red;
			lightData->ambient.green += l->color.green;
			lightData->ambient.blue  += l->color.blue;
			lightData->numAmbients++;
		}else if(normals && l->getType() == Light::DIRECTIONAL){
			if(lightData->numDirectionals < maxDirectionals)
				lightData->directionals[lightData->numDirectionals++] = l;
		}
	}

	if(atomic->world != this)
		return;

	if(!normals)
		return;

	// TODO: for this we would use an atomic's world sectors, but we don't have those yet
	FORLIST(lnk, this->localLights){
		if(lightData->numLocals >= maxLocals)
			return;

		Light *l = Light::fromWorld(lnk);
		if((l->getFlags() & Light::LIGHTATOMICS) == 0)
			continue;

		// check if spheres are intersecting
		Sphere *atomsphere = atomic->getWorldBoundingSphere();
		V3d dist = sub(l->getFrame()->getLTM()->pos, atomsphere->center);
		if(length(dist) < atomsphere->radius + l->radius)
			lightData->locals[lightData->numLocals++] = l;
	}
}

// Find all lights, for im3d lighting extension
// missing flags and bounding spheres so more primitive than above
void
World::enumerateLights(WorldLights *lightData)
{
	int32 maxDirectionals, maxLocals;

	maxDirectionals = lightData->numDirectionals;
	maxLocals = lightData->numLocals;

	lightData->numDirectionals = 0;
	lightData->numLocals = 0;
	lightData->numAmbients = 0;
	lightData->ambient.red = 0.0f;
	lightData->ambient.green = 0.0f;
	lightData->ambient.blue = 0.0f;
	lightData->ambient.alpha = 1.0f;

	FORLIST(lnk, this->globalLights){
		Light *l = Light::fromWorld(lnk);
		if(l->getType() == Light::AMBIENT){
			lightData->ambient.red   += l->color.red;
			lightData->ambient.green += l->color.green;
			lightData->ambient.blue  += l->color.blue;
			lightData->numAmbients++;
		}else if(l->getType() == Light::DIRECTIONAL){
			if(lightData->numDirectionals < maxDirectionals)
				lightData->directionals[lightData->numDirectionals++] = l;
		}
	}

	FORLIST(lnk, this->localLights){
		if(lightData->numLocals >= maxLocals)
			return;

		Light *l = Light::fromWorld(lnk);
		lightData->locals[lightData->numLocals++] = l;
	}
}


// ---- BSP world stream reading -------------------------------------------
// Stream layout (documented in MashedFormats/FORMATS.md, observed in the
// game's .BSP files):
//  WORLD { STRUCT(64), MATLIST, (PLANESECT|ATOMICSECT), EXTENSION }
//  PLANESECT { STRUCT(24: type, value, leftIsWorldSector, rightIsWorldSector,
//                      leftValue, rightValue), left, right [, EXTENSION] }
//  ATOMICSECT { STRUCT(matIdBase, numTriangles, numVertices, inf, sup,
//                      unused, unused, vertices, [normals int8x4],
//                      [colors], [uvs per set], triangles {v0,v1,v2,mat}),
//               EXTENSION }

struct BspReadCtx
{
	World *world;
	int32 planesLeft;
	int32 sectorsRead;
};

static const uint32 SECTORHDR = 44;

// findChunk that also rejects chunk versions the original does not accept
static bool
findChunkV(Stream *stream, uint32 type, uint32 *length, uint32 *version)
{
	uint32 v;
	if(!findChunk(stream, type, length, &v))
		return false;
	if(v < 0x35000 || v > 0x37002){
		RWERROR((ERR_GENERAL, "unsupported chunk version"));
		return false;
	}
	if(version)
		*version = v;
	return true;
}

static bool readNode(Stream *stream, BspReadCtx *ctx, BspNode **slot, int32 depth, bool isSector);

static bool
readSectorExtension(Stream *stream, WorldSector *sec, int32 length)
{
	ChunkHeaderInfo h;
	while(length >= 12){
		if(!readChunkHeaderInfo(stream, &h))
			return false;
		length -= 12;
		if((int32)h.length > length)
			return false;
		if(h.type == 0x11D && h.length){
			sec->collision = rwNewT(uint8, h.length, MEMDUR_EVENT | ID_WORLD);
			sec->collisionSize = h.length;
			stream->read8(sec->collision, h.length);
			sec->collTree = CollisionTree::fromBlob(sec->collision, h.length, sec->numTriangles);
			if(sec->collTree == nil)
				return false;
		}else if(h.type == 0x120 && h.length >= 4){
			sec->matFX = stream->readI32();
			stream->seek(h.length-4);
		}else if(WorldSector::s_plglist.streamReadChunk(stream, h.type, h.length, sec)){
			// registered plugin consumed its chunk; resync in case it read less
			// (plugins must read exactly h.length bytes, as everywhere in librw)
		}else
			stream->seek(h.length);
		length -= h.length;
	}
	return length == 0;
}

static bool
readAtomicSector(Stream *stream, BspReadCtx *ctx, BspNode **slot, uint32 end)
{
	World *world = ctx->world;
	uint32 length;
	uint32 sver;
	if(!findChunkV(stream, ID_STRUCT, &length, &sver)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return false;
	}
	if(length < SECTORHDR || ctx->sectorsRead >= world->numSectors){
		RWERROR((ERR_GENERAL, "bad atomic sector"));
		return false;
	}
	WorldSector *sec = WorldSector::create();
	if(sec == nil)
		return false;
	*slot = (BspNode*)sec;
	world->sectors[ctx->sectorsRead++] = sec;
	sec->world = world;

	int32 hdr[11];
	stream->read32(hdr, SECTORHDR);
	sec->matIdBase = hdr[0];
	sec->numTriangles = hdr[1];
	sec->numVertices = hdr[2];
	float32 box[6];
	memcpy(box, &hdr[3], sizeof(box));
	sec->bbox.inf.set(box[0], box[1], box[2]);
	sec->bbox.sup.set(box[3], box[4], box[5]);
	sec->unusedHdr = hdr[9];	// hdr[10] is unused too

	uint32 fmt = world->format;
	if(fmt & Geometry::NATIVE){
		RWERROR((ERR_GENERAL, "native (platform specific) world sectors are not supported"));
		return false;
	}
	if(sec->numTriangles < 0 || sec->numVertices < 0 || sec->numTriangles > 0xFFFF*4 || sec->numVertices > 0xFFFF){
		RWERROR((ERR_GENERAL, "bad sector counts"));
		return false;
	}
	int32 nv = sec->numVertices, nt = sec->numTriangles;
	int32 nsets = (fmt & 0xFF0000) >> 16;
	if(nsets == 0)
		nsets = (fmt & Geometry::TEXTURED2) ? 2 : (fmt & Geometry::TEXTURED) ? 1 : 0;
	if(nsets > 8){
		RWERROR((ERR_GENERAL, "too many texcoord sets"));
		return false;
	}
	sec->numTexCoordSets = nsets;
	uint32 expect = SECTORHDR + 12*nv + ((fmt & Geometry::NORMALS) ? 4*nv : 0) +
	                ((fmt & Geometry::PRELIT) ? 4*nv : 0) + 8*nv*nsets + 8*nt;
	if(expect != length){
		RWERROR((ERR_GENERAL, "sector struct size mismatch"));
		return false;
	}
	if(nv){
		sec->vertices = rwNewT(V3d, nv, MEMDUR_EVENT | ID_WORLD);
		stream->read32(sec->vertices, 12*nv);
		if(fmt & Geometry::NORMALS){
			int8 *tmp = rwNewT(int8, 4*nv, MEMDUR_FUNCTION | ID_WORLD);
			stream->read8(tmp, 4*nv);
			sec->normals = rwNewT(V3d, nv, MEMDUR_EVENT | ID_WORLD);
			for(int32 i = 0; i < nv; i++)
				sec->normals[i].set(tmp[4*i]/128.0f, tmp[4*i+1]/128.0f, tmp[4*i+2]/128.0f);
			rwFree(tmp);
		}
		if(fmt & Geometry::PRELIT){
			sec->colors = rwNewT(RGBA, nv, MEMDUR_EVENT | ID_WORLD);
			stream->read8(sec->colors, 4*nv);
		}
		for(int32 i = 0; i < nsets; i++){
			sec->texCoords[i] = rwNewT(TexCoords, nv, MEMDUR_EVENT | ID_WORLD);
			stream->read32(sec->texCoords[i], 8*nv);
		}
	}
	if(nt){
		sec->triangles = rwNewT(Triangle, nt, MEMDUR_EVENT | ID_WORLD);
		// BSP word order is {v0, v1, v2, material}, which is what Triangle holds
		stream->read16(sec->triangles, 8*nt);
		if(sver < 0x36002)	// old files store {mat, v0, v1, v2}
			for(int32 i = 0; i < nt; i++){
				uint16 *w = (uint16*)&sec->triangles[i];
				uint16 m = w[0];
				w[0] = w[1]; w[1] = w[2]; w[2] = w[3]; w[3] = m;
			}
		for(int32 i = 0; i < nt; i++){
			Triangle *t = &sec->triangles[i];
			int32 m = sec->matIdBase + t->matId;
			if(t->v[0] >= nv || t->v[1] >= nv || t->v[2] >= nv ||
			   m < 0 || m >= world->matList.numMaterials){
				RWERROR((ERR_GENERAL, "sector triangle index out of range"));
				return false;
			}
		}
	}
	// extension: collision tree (kept raw), everything else skipped
	uint32 extLen;
	if(!findChunkV(stream, ID_EXTENSION, &extLen, nil)){
		RWERROR((ERR_CHUNK, "EXTENSION"));
		return false;
	}
	if(!readSectorExtension(stream, sec, extLen))
		return false;
	(void)end;
	return true;
}

static bool
readPlaneSector(Stream *stream, BspReadCtx *ctx, BspNode **slot, int32 depth)
{
	uint32 length;
	if(!findChunkV(stream, ID_STRUCT, &length, nil) || length != 24){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return false;
	}
	if(ctx->planesLeft <= 0){
		RWERROR((ERR_GENERAL, "more plane sectors than the world header says"));
		return false;
	}
	ctx->planesLeft--;
	PlaneSector *p = rwNewT(PlaneSector, 1, MEMDUR_EVENT | ID_WORLD);
	memset(p, 0, sizeof(*p));
	*slot = (BspNode*)p;
	int32 buf[6];
	stream->read32(buf, 24);
	p->type = buf[0];
	memcpy(&p->value, &buf[1], 4);
	if(p->type != 0 && p->type != 4 && p->type != 8){
		RWERROR((ERR_GENERAL, "bad plane type"));
		return false;
	}
	if(ctx->world->format & 0x40000000){	// sectors overlap
		memcpy(&p->leftValue, &buf[4], 4);
		memcpy(&p->rightValue, &buf[5], 4);
	}else
		p->leftValue = p->rightValue = p->value;
	// buf[2], buf[3] select the child chunk kind; other chunks in between are skipped
	return readNode(stream, ctx, &p->left, depth+1, buf[2] != 0) &&
	       readNode(stream, ctx, &p->right, depth+1, buf[3] != 0);
}

static bool
readNode(Stream *stream, BspReadCtx *ctx, BspNode **slot, int32 depth, bool isSector)
{
	ChunkHeaderInfo h;
	if(depth > 512)
		return false;
	// like the original: find the wanted chunk kind, skipping anything else
	uint32 want = isSector ? ID_ATOMICSECTOR : ID_PLANESECTOR;
	do{
		if(!readChunkHeaderInfo(stream, &h) || h.type == ID_NAOBJECT)
			return false;
		if(h.type != want)
			stream->seek(h.length);
	}while(h.type != want);
	if(h.version < 0x35000 || h.version > 0x37002){
		RWERROR((ERR_GENERAL, "unsupported chunk version"));
		return false;
	}
	uint32 end = stream->tell() + h.length;
	bool ok;
	if(isSector)
		ok = readAtomicSector(stream, ctx, slot, end);
	else
		ok = readPlaneSector(stream, ctx, slot, depth);
	if(!ok)
		return false;
	// skip anything we did not consume (e.g. a plane sector extension)
	int32 rest = (int32)end - (int32)stream->tell();
	if(rest < 0)
		return false;
	if(rest)
		stream->seek(rest);
	return true;
}

// plane box of every sector: world box cut along the plane path
static void
setPlaneBoxes(BspNode *n, BBox box)
{
	if(n->isLeaf){
		((WorldSector*)n)->planeBBox = box;
		return;
	}
	PlaneSector *p = (PlaneSector*)n;
	int32 a = p->type/4;
	BBox l = box, r = box;
	(&l.sup.x)[a] = p->leftValue;
	(&r.inf.x)[a] = p->rightValue;
	setPlaneBoxes(p->left, l);
	setPlaneBoxes(p->right, r);
}

World*
World::streamRead(Stream *stream)
{
	uint32 length;
	if(!findChunkV(stream, ID_STRUCT, &length, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	if(length != 64){
		RWERROR((ERR_GENERAL, "unexpected world struct size"));
		return nil;
	}
	int32 b[16];
	stream->read32(b, 64);
	float32 f[16];
	memcpy(f, b, sizeof(f));
	// rootIsWorldSector, invOrigin[3], numTri, numVert, numPlane, numAtomic,
	// colSectorSize, format, bbox[6]
	if(b[8] > 0 || b[4] < 0 || b[5] < 0 || b[6] < 0 || b[7] < 0 || b[7] > 0x100000 || b[6] > 0x100000){
		RWERROR((ERR_GENERAL, "bad world counts"));
		return nil;
	}
	World *world = World::create(nil);
	if(world == nil)
		return nil;
	world->invOrigin.set(f[1], f[2], f[3]);
	world->numTriangles = b[4];
	world->numVertices = b[5];
	world->numPlaneSectors = b[6];
	world->numSectors = b[7];
	world->colSectorSize = b[8];
	world->format = b[9];
	// the world struct stores the box as sup then inf (observed: first triple is the larger)
	world->bbox.sup.set(f[10], f[11], f[12]);
	world->bbox.inf.set(f[13], f[14], f[15]);
	if(world->numSectors)
		world->sectors = rwNewT(WorldSector*, world->numSectors, MEMDUR_EVENT | ID_WORLD);
	for(int32 i = 0; i < world->numSectors; i++)
		world->sectors[i] = nil;

	if(!findChunkV(stream, ID_MATLIST, nil, nil)){
		RWERROR((ERR_CHUNK, "MATLIST"));
		goto fail;
	}
	if(MaterialList::streamRead(stream, &world->matList) == nil)
		goto fail;

	BspReadCtx ctx;
	ctx.world = world;
	ctx.planesLeft = world->numPlaneSectors;
	ctx.sectorsRead = 0;
	if(!readNode(stream, &ctx, &world->root, 0, b[0] != 0))
		goto fail;
	if(ctx.sectorsRead != world->numSectors || ctx.planesLeft != 0){
		RWERROR((ERR_GENERAL, "sector counts differ from world header"));
		goto fail;
	}
	if(!s_plglist.streamRead(stream, world))
		goto fail;
	setPlaneBoxes(world->root, world->bbox);
	return world;
fail:
	// sectors not read stay nil; destroyNode only walks the tree
	world->destroy();
	return nil;
}

World*
World::forAllWorldSectors(WorldSectorCallBack cb, void *data)
{
	for(int32 i = 0; i < this->numSectors; i++)
		if(this->sectors[i] && cb(this->sectors[i], data) == nil)
			break;
	return this;
}

World*
World::forAllMaterials(WorldMaterialCallBack cb, void *data)
{
	for(int32 i = 0; i < this->matList.numMaterials; i++)
		if(cb(this->matList.materials[i], data) == nil)
			break;
	return this;
}

World*
World::forAllClumps(WorldClumpCallBack cb, void *data)
{
	FORLIST(lnk, this->clumps)
		if(cb(Clump::fromWorld(lnk), data) == nil)
			break;
	return this;
}

World*
World::setSectorRenderCallBack(WorldSectorRenderCallBack cb)
{
	this->sectorRenderCB = cb ? cb : defaultSectorRender;
	return this;
}

static Atomic *ensureAtomic(WorldSector *s);

World*
World::instance(void)
{
	// ponytail: only builds geometry + atomic; the device instance still happens on first draw
	for(int32 i = 0; i < this->numSectors; i++)
		if(this->sectors[i] && this->sectors[i]->numTriangles)
			ensureAtomic(this->sectors[i]);
	return this;
}

// ---- minimal render path ---------------------------------------------------
// Each sector becomes an ordinary Geometry + Atomic (identity frame) the first
// time it is drawn, so all of librw's material/pipeline code is reused.
// ponytail: whole world material list is attached to every sector geometry
// (empty meshes cost nothing); trim per sector if instancing cost shows up.

Geometry*
WorldSector::getGeometry(void)
{
	if(this->geometry)
		return this->geometry;
	World *w = this->world;
	uint32 flags = ((w->format & 0xFC) | Geometry::POSITIONS) & ~Geometry::TRISTRIP;
	flags |= (uint32)this->numTexCoordSets << 16;
	Geometry *g = Geometry::create(this->numVertices, this->numTriangles, flags);
	if(g == nil)
		return nil;
	MorphTarget *mt = &g->morphTargets[0];
	if(this->numVertices){
		memcpy(mt->vertices, this->vertices, this->numVertices*sizeof(V3d));
		if(this->normals && mt->normals)
			memcpy(mt->normals, this->normals, this->numVertices*sizeof(V3d));
		if(this->colors && g->colors)
			memcpy(g->colors, this->colors, this->numVertices*sizeof(RGBA));
		for(int32 i = 0; i < this->numTexCoordSets; i++)
			memcpy(g->texCoords[i], this->texCoords[i], this->numVertices*sizeof(TexCoords));
	}
	for(int32 i = 0; i < w->matList.numMaterials; i++)
		g->matList.appendMaterial(w->matList.materials[i]);
	for(int32 i = 0; i < this->numTriangles; i++){
		g->triangles[i] = this->triangles[i];
		g->triangles[i].matId = this->matIdBase + this->triangles[i].matId;
	}
	mt->boundingSphere = mt->calculateBoundingSphere();
	g->buildMeshes();
	this->geometry = g;
	return g;
}

WorldSector*
WorldSector::create(void)
{
	WorldSector *sec = (WorldSector*)rwMalloc(s_plglist.size, MEMDUR_EVENT | ID_WORLD);
	if(sec == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	memset(sec, 0, sizeof(*sec));
	sec->isLeaf = 1;
	s_plglist.construct(sec);
	return sec;
}

void
WorldSector::destroy(void)
{
	s_plglist.destruct(this);
	rwFree(this);
}

void
WorldSector::enableMatFX(void)
{
	this->matFX = 1;
	if(this->atomic)
		MatFX::enableEffects(this->atomic);
}

void
WorldSector::render(void)
{
	World *w = this->world;
	if(w && w->sectorRenderCB)
		w->sectorRenderCB(this);
	else
		this->renderDefault();
}

static Atomic*
ensureAtomic(WorldSector *s)
{
	if(s->atomic)
		return s->atomic;
	Geometry *g = s->getGeometry();
	if(g == nil)
		return nil;
	Atomic *a = Atomic::create();
	a->setFrame(Frame::create());
	a->setGeometry(g, 0);
	g->destroy();	// atomic now owns the only reference
	if(s->matFX)
		MatFX::enableEffects(a);
	return s->atomic = a;
}

void
WorldSector::renderDefault(void)
{
	if(Atomic *a = ensureAtomic(this))
		a->render();
}

static void
renderNode(BspNode *n, Camera *cam, const V3d *eye)
{
	if(n == nil)
		return;
	if(n->isLeaf){
		WorldSector *s = (WorldSector*)n;
		if(s->numTriangles == 0)
			return;
		Sphere sp;
		sp.center = scale(add(s->bbox.inf, s->bbox.sup), 0.5f);
		sp.radius = length(sub(s->bbox.sup, sp.center));
		if(cam && cam->frustumTestSphere(&sp) == Camera::SPHEREOUTSIDE)
			return;
		s->render();
	}else{
		// back to front: the child on the far side of the plane first
		PlaneSector *p = (PlaneSector*)n;
		bool eyeLeft = eye && (&eye->x)[p->type/4] < p->value;
		renderNode(eyeLeft ? p->right : p->left, cam, eye);
		renderNode(eyeLeft ? p->left : p->right, cam, eye);
	}
}

void
World::renderSectors(Camera *cam)
{
	V3d eye;
	bool haveEye = cam && cam->getFrame();
	if(haveEye)
		eye = cam->getFrame()->getLTM()->pos;
	renderNode(this->root, cam, haveEye ? &eye : nil);
}

}
