// S2-c groups 2 and 3: texture dictionary / texture / raster, and clump / atomic / geometry / material / frame create, stream read, accessors.
// Thin wrappers over rw:: with the RW 3.7 signatures of rwmfl.h; the object layouts are RW 3.7 since T-L4 (layout_check.cpp).
// Tags as in core.cpp: L2:oracle = compared with the original through the asset-parity oracle, L2:impl = implemented, not compared.
#include <string.h>
#include "rwmfl.h"
#include "../rw.h"

using namespace rw;

typedef void *(*cbp)(void *, void *);	// RW callbacks all have the shape  T *cb(T *item, void *data)

extern "C" {

// ---- Raster
// L2:impl
extern "C" void mflrw_ensure_engine(void);
RWAPI RwRaster * RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags){ mflrw_ensure_engine(); return (RwRaster*)Raster::create(width, height, depth, flags); }
// L2:impl
RWAPI RwBool RwRasterDestroy(RwRaster *raster){ ((Raster*)raster)->destroy(); return 1; }
// L2:impl
RWAPI RwRaster * RwRasterGetOffset(RwRaster *raster, RwInt16 *xOffset, RwInt16 *yOffset){
	Raster *r = (Raster*)raster;
	if(xOffset) *xOffset = (RwInt16)r->offsetX;
	if(yOffset) *yOffset = (RwInt16)r->offsetY;
	return raster;
}

// ---- TexDictionary / Texture
// L2:impl  (native chunk 0x16 stream)
RWAPI RwTexDictionary * RwTexDictionaryStreamRead(RwStream *stream){ mflrw_ensure_engine(); return (RwTexDictionary*)TexDictionary::streamRead((Stream*)stream); }
// L2:oracle  (platform independent TXD, chunk 0x23: the form the game's course / car TXDs are in)
RWAPI RwTexDictionary * RtPITexDictionaryStreamRead(RwStream *stream){ mflrw_ensure_engine(); return (RwTexDictionary*)TexDictionary::streamReadPI((Stream*)stream); }
// L2:impl
RWAPI RwTexDictionary * RwTexDictionaryCreate(void){ mflrw_ensure_engine(); return (RwTexDictionary*)TexDictionary::create(); }
// L2:impl
RWAPI RwTexture * RwTexDictionaryAddTexture(RwTexDictionary *dict, RwTexture *texture){ ((TexDictionary*)dict)->add((Texture*)texture); return texture; }
// L2:impl
RWAPI RwTexDictionary * RwTexDictionarySetCurrent(RwTexDictionary *dict){ mflrw_ensure_engine(); TexDictionary::setCurrent((TexDictionary*)dict); return dict; }
// L2:impl
RWAPI RwBool RwTexDictionaryDestroy(RwTexDictionary *dict){ ((TexDictionary*)dict)->destroy(); return 1; }
// L2:oracle  (the original compares case-insensitively, 0x4d5d90)
RWAPI RwTexture * RwTexDictionaryFindNamedTexture(RwTexDictionary *dict, const RwChar *name){ return (RwTexture*)((TexDictionary*)dict)->find(name); }
// L2:impl
RWAPI const RwTexDictionary * RwTexDictionaryForAllTextures(const RwTexDictionary *dict, RwTextureCallBack fpCallBack, void *pData){
	TexDictionary *d = (TexDictionary*)dict;
	FORLIST(lnk, d->textures){
		Texture *t = Texture::fromDict(lnk);
		if(!((cbp)fpCallBack)(t, pData)) break;
	}
	return dict;
}
// L2:impl
RWAPI RwTexture * RwTextureRead(const RwChar *name, const RwChar *maskName){ mflrw_ensure_engine(); return (RwTexture*)Texture::read(name, maskName); }
// L2:impl
RWAPI RwTexture * RwTextureCreate(RwRaster *raster){ mflrw_ensure_engine(); return (RwTexture*)Texture::create((Raster*)raster); }
// L2:impl
RWAPI RwBool RwTextureDestroy(RwTexture *texture){ ((Texture*)texture)->destroy(); return 1; }
// L2:impl  (RW stores the raster pointer, it does not release the old one)
RWAPI RwTexture * RwTextureSetRaster(RwTexture *texture, RwRaster *raster){ ((Texture*)texture)->raster = (Raster*)raster; return texture; }

// ---- Material
// L2:impl
RWAPI RpMaterial * RpMaterialCreate(void){ return (RpMaterial*)Material::create(); }
// L2:impl
RWAPI RwBool RpMaterialDestroy(RpMaterial *material){ ((Material*)material)->destroy(); return 1; }
// L2:impl
RWAPI RpMaterial * RpMaterialSetTexture(RpMaterial *material, RwTexture *texture){ ((Material*)material)->setTexture((Texture*)texture); return material; }
// L2:impl  (plugin offsets are the ones librw hands out; the game keeps what it is given)
RWAPI RwInt32 RpMaterialRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB){
	return Material::registerPlugin(size, pluginID, (Constructor)constructCB, (Destructor)destructCB, (CopyConstructor)copyCB); }
// L2:impl
RWAPI RwInt32 RpMaterialRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB){
	return Material::registerPluginStream(pluginID, (StreamRead)readCB, (StreamWrite)writeCB, (StreamGetSize)getSizeCB); }

// ---- Geometry
// L2:impl
RWAPI RpGeometry * RpGeometryCreate(RwInt32 numVert, RwInt32 numTriangles, RwUInt32 format){ return (RpGeometry*)Geometry::create(numVert, numTriangles, format); }
// L2:impl
RWAPI RwBool RpGeometryDestroy(RpGeometry *geometry){ ((Geometry*)geometry)->destroy(); return 1; }
// L2:impl
RWAPI RpGeometry * RpGeometryLock(RpGeometry *geometry, RwInt32 lockMode){ ((Geometry*)geometry)->lock(lockMode); return geometry; }
// L2:impl
RWAPI RpGeometry * RpGeometryUnlock(RpGeometry *geometry){ ((Geometry*)geometry)->unlock(); return geometry; }
// L2:impl
RWAPI RpGeometry * RpGeometryForAllMaterials(RpGeometry *geometry, RpMaterialCallBack fpCallBack, void *pData){
	Geometry *g = (Geometry*)geometry;
	for(int32 i = 0; i < g->matList.numMaterials; i++)
		if(!((cbp)fpCallBack)(g->matList.materials[i], pData)) break;
	return geometry;
}
// L2:impl
RWAPI const RpGeometry * RpGeometryTriangleSetVertexIndices(const RpGeometry *geometry, RpTriangle *triangle, RwUInt16 vert1, RwUInt16 vert2, RwUInt16 vert3){
	Triangle *t = (Triangle*)triangle;
	t->v[0] = vert1; t->v[1] = vert2; t->v[2] = vert3;
	return geometry;
}
// L2:impl  (RW: the material joins the geometry's list when it is not in it yet)
RWAPI RpGeometry * RpGeometryTriangleSetMaterial(RpGeometry *geometry, RpTriangle *triangle, RpMaterial *material){
	Geometry *g = (Geometry*)geometry;
	Material *m = (Material*)material;
	int32 idx = g->matList.findIndex(m);
	if(idx < 0) idx = g->matList.appendMaterial(m);
	((Triangle*)triangle)->matId = (uint16)idx;
	return geometry;
}

// ---- Frame
// L2:impl
RWAPI RwFrame * RwFrameCreate(void){ return (RwFrame*)Frame::create(); }
// L2:impl
RWAPI RwBool RwFrameDestroy(RwFrame *frame){ ((Frame*)frame)->destroy(); return 1; }
// L2:impl
RWAPI RwFrame * RwFrameAddChild(RwFrame *parent, RwFrame *child){ ((Frame*)parent)->addChild((Frame*)child); return parent; }
// L2:impl
RWAPI RwFrame * RwFrameRemoveChild(RwFrame *child){ ((Frame*)child)->removeChild(); return child; }
// L2:impl
RWAPI RwFrame * RwFrameForAllChildren(RwFrame *frame, RwFrameCallBack callBack, void *data){ return (RwFrame*)((Frame*)frame)->forAllChildren((Frame::Callback)callBack, data); }
// L2:impl
RWAPI RwMatrix * RwFrameGetLTM(RwFrame *frame){ return (RwMatrix*)((Frame*)frame)->getLTM(); }
// L2:impl
RWAPI RwFrame * RwFrameUpdateObjects(RwFrame *frame){ ((Frame*)frame)->updateObjects(); return frame; }
// L2:impl  (identity matrix with the type flags 0x20003 RW's RwMatrixSetIdentity gives, then dirty)
RWAPI RwFrame * RwFrameSetIdentity(RwFrame *frame){ Frame *f = (Frame*)frame; f->matrix.setIdentity(); f->updateObjects(); return frame; }
// L2:impl
RWAPI RwFrame * RwFrameTranslate(RwFrame *frame, const RwV3d *v, RwOpCombineType combine){ ((Frame*)frame)->translate((const V3d*)v, (CombineOp)combine); return frame; }
// L2:impl
RWAPI RwFrame * RwFrameRotate(RwFrame *frame, const RwV3d *axis, RwReal angle, RwOpCombineType combine){ ((Frame*)frame)->rotate((const V3d*)axis, angle, (CombineOp)combine); return frame; }
// L2:impl
RWAPI RwFrame * RwFrameScale(RwFrame *frame, const RwV3d *v, RwOpCombineType combine){ ((Frame*)frame)->scale((const V3d*)v, (CombineOp)combine); return frame; }
// L2:impl
RWAPI RwFrame * RwFrameTransform(RwFrame *frame, const RwMatrix *m, RwOpCombineType combine){ ((Frame*)frame)->transform((const Matrix*)m, (CombineOp)combine); return frame; }

// ---- Atomic
// L2:impl
RWAPI RpAtomic * RpAtomicCreate(void){ return (RpAtomic*)Atomic::create(); }
// L2:impl  (RW detaches the atomic from its clump / world first; librw asserts)
RWAPI RwBool RpAtomicDestroy(RpAtomic *atomic){
	Atomic *a = (Atomic*)atomic;
	if(a->world) a->world->removeAtomic(a);
	if(a->clump) a->clump->removeAtomic(a);
	a->destroy();
	return 1;
}
// L2:impl
RWAPI RpAtomic * RpAtomicClone(RpAtomic *atomic){ return (RpAtomic*)((Atomic*)atomic)->clone(); }
// L2:impl
RWAPI RpAtomic * RpAtomicSetFrame(RpAtomic *atomic, RwFrame *frame){ ((Atomic*)atomic)->setFrame((Frame*)frame); return atomic; }
// L2:impl
RWAPI RpAtomic * RpAtomicSetGeometry(RpAtomic *atomic, RpGeometry *geometry, RwUInt32 flags){ ((Atomic*)atomic)->setGeometry((Geometry*)geometry, flags); return atomic; }
// L2:impl
RWAPI const RwSphere * RpAtomicGetWorldBoundingSphere(RpAtomic *atomic){ return (const RwSphere*)((Atomic*)atomic)->getWorldBoundingSphere(); }
// L2:impl
RWAPI RpWorld * RpAtomicGetWorld(const RpAtomic *atomic){ return (RpWorld*)((const Atomic*)atomic)->world; }
// L2:impl  0x4f4920: the exe's RpAtomicGetWorld and RpClumpGetWorld are one ICF-folded function (`*(obj + world plugin offset)`); the object type byte tells them apart
RWAPI RpWorld * FUN_004f4920(void *object){
	Object *o = (Object*)object;
	return (RpWorld*)(o->type == Atomic::ID ? ((Atomic*)object)->world : o->type == Light::ID ? ((Light*)object)->world : ((Clump*)object)->world);
}
// L2:impl
RWAPI RwBool RpAtomicInstance(RpAtomic *atomic){ ((Atomic*)atomic)->instance(); return 1; }
// L2:impl
RWAPI RwInt32 RpAtomicRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB){
	return Atomic::registerPlugin(size, pluginID, (Constructor)constructCB, (Destructor)destructCB, (CopyConstructor)copyCB); }
// L2:impl
RWAPI RwInt32 RpAtomicRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB){
	return Atomic::registerPluginStream(pluginID, (StreamRead)readCB, (StreamWrite)writeCB, (StreamGetSize)getSizeCB); }

// ---- Clump
// L2:oracle  (the caller has found the clump chunk, as in clump_load_logged)
RWAPI RpClump * RpClumpStreamRead(RwStream *stream){ return (RpClump*)Clump::streamRead((Stream*)stream); }
// L2:impl
RWAPI RpClump * RpClumpCreate(void){ return (RpClump*)Clump::create(); }
// L2:impl  (RW also leaves the world)
RWAPI RwBool RpClumpDestroy(RpClump *clump){
	Clump *c = (Clump*)clump;
	if(c->world) c->world->removeClump(c);
	c->destroy();
	return 1;
}
// L2:impl
RWAPI RpClump * RpClumpClone(RpClump *clump){ return (RpClump*)((Clump*)clump)->clone(); }
// L2:impl
RWAPI RpClump * RpClumpAddAtomic(RpClump *clump, RpAtomic *atomic){ ((Clump*)clump)->addAtomic((Atomic*)atomic); return clump; }
// L2:impl
RWAPI RpClump * RpClumpRemoveAtomic(RpClump *clump, RpAtomic *atomic){ ((Clump*)clump)->removeAtomic((Atomic*)atomic); return clump; }
// L2:impl  (the callback may remove the atomic it is given: the next link is read first)
RWAPI RpClump * RpClumpForAllAtomics(RpClump *clump, RpAtomicCallBack callback, void *pData){
	Clump *c = (Clump*)clump;
	LLLink *next;
	for(LLLink *l = c->atomics.link.next; l != c->atomics.end(); l = next){
		next = l->next;
		if(!((cbp)callback)(Atomic::fromClump(l), pData)) break;
	}
	return clump;
}
// L2:impl
RWAPI RpClump * RpClumpForAllLights(RpClump *clump, RpLightCallBack callback, void *pData){
	Clump *c = (Clump*)clump;
	LLLink *next;
	for(LLLink *l = c->lights.link.next; l != c->lights.end(); l = next){
		next = l->next;
		if(!((cbp)callback)(Light::fromClump(l), pData)) break;
	}
	return clump;
}
// L2:impl
RWAPI RwInt32 RpClumpGetNumLights(RpClump *clump){ return ((Clump*)clump)->countLights(); }
// L2:impl  (needs the render backend and a current camera)
RWAPI RpClump * RpClumpRender(RpClump *clump){ ((Clump*)clump)->render(); return clump; }

// ---- World (BSP read only; the rest of RpWorld is the sector / collision group)
// L2:oracle
RWAPI RpWorld * RpWorldStreamRead(RwStream *stream){ return (RpWorld*)World::streamRead((Stream*)stream); }

}
