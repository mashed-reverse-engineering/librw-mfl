// S3 pieces 1-2: the D3D9 state layer and the device table of the RW 3.7 C ABI: RwRenderStateSet / Get and RwIm2D* (called by the game through
// RwEngineInstance+0x20..0x34, filled by mflrw_shadow_device), the RwD3D9* entries on d3ddevice.cpp, and the fixed-function switch (d3d::fixedFunction).
// Tags: "L2:impl" = implemented from the RW 3.7 semantics, not compared against the original (B7 replay: tools/rwtap, is the oracle).
// Without RW_D3D9 (mflrw_null.dll) every entry degrades to a no-op that returns 0.
#define WITH_D3D
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rwmfl.h"
#include "../rw.h"

using namespace rw;

// RW 3.7 RwRenderState values (rwplcore.h) that the game sets; librw's own enum has a different order
enum { RS_TEXTURERASTER = 1, RS_TEXTUREADDRESS, RS_TEXTUREADDRESSU, RS_TEXTUREADDRESSV, RS_PERSPECTIVE, RS_ZTESTENABLE, RS_SHADEMODE, RS_ZWRITEENABLE, RS_TEXTUREFILTER,
       RS_SRCBLEND, RS_DESTBLEND, RS_VERTEXALPHAENABLE, RS_BORDERCOLOR, RS_FOGENABLE, RS_FOGCOLOR, RS_FOGTYPE, RS_FOGDENSITY, RS_CULLMODE = 20,
       RS_STENCILENABLE, RS_STENCILFAIL, RS_STENCILZFAIL, RS_STENCILPASS, RS_STENCILFUNCTION, RS_STENCILFUNCTIONREF, RS_STENCILFUNCTIONMASK, RS_STENCILFUNCTIONWRITEMASK,
       RS_ALPHATESTFUNCTION, RS_ALPHATESTFUNCTIONREF, RS_MAX };
// states librw has no field for: kept here (defaults of RW: gouraud, linear fog, density 1.0f)
static uint32_t g_extra[RS_MAX] = { 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0x3f800000u };

static const int8_t rsMap[RS_MAX] = {	// RW state -> rw:: RenderState, -1 = handled by hand
	-1, TEXTURERASTER, TEXTUREADDRESS, TEXTUREADDRESSU, TEXTUREADDRESSV, -1, ZTESTENABLE, -1, ZWRITEENABLE, TEXTUREFILTER, SRCBLEND, DESTBLEND, VERTEXALPHA, -1,
	FOGENABLE, FOGCOLOR, -1, -1, -1, -1, CULLMODE, STENCILENABLE, STENCILFAIL, STENCILZFAIL, STENCILPASS, STENCILFUNCTION, STENCILFUNCTIONREF, STENCILFUNCTIONMASK,
	STENCILFUNCTIONWRITEMASK, ALPHATESTFUNC, ALPHATESTREF };
// fog colour: RW passes 0xAARRGGBB, librw's cache wants r | g<<8 | b<<16 | a<<24
static uint32_t fogToLib(uint32_t c){ return ((c >> 16) & 0xff) | (c & 0xff00) | ((c & 0xff) << 16) | (c & 0xff000000u); }

extern "C" {

// L2:impl
RWAPI RwBool RwRenderStateSet(RwInt32 state, void *value){
	if(!engine || state <= 0 || state >= RS_MAX) return 0;
	uint32_t v = (uint32_t)(uintptr_t)value;
	switch(state){
	case RS_PERSPECTIVE: return 1;
	case RS_SHADEMODE: g_extra[state] = v;
#ifdef RW_D3D9
		d3d::setRenderState(D3DRS_SHADEMODE, v);
#endif
		return 1;
	case RS_BORDERCOLOR: g_extra[state] = v;
#ifdef RW_D3D9
		d3d::setSamplerState(0, D3DSAMP_BORDERCOLOR, v);
#endif
		return 1;
	case RS_FOGTYPE: g_extra[state] = v;
#ifdef RW_D3D9
		d3d::setRenderState(D3DRS_FOGTABLEMODE, v == 1 ? D3DFOG_LINEAR : v == 2 ? D3DFOG_EXP : v == 3 ? D3DFOG_EXP2 : D3DFOG_NONE);
#endif
		return 1;
	case RS_FOGDENSITY: g_extra[state] = v;
#ifdef RW_D3D9
		d3d::setRenderState(D3DRS_FOGDENSITY, v);
#endif
		return 1;
	case RS_FOGCOLOR: v = fogToLib(v); break;
	case RS_ALPHATESTFUNCTION: v = v ? v - 1 : 0; break;	// RW: NA 0, ALWAYS 1, GREATEREQUAL 2, LESS 3
	}
	if(!engine->device.setRenderState) return 0;
	engine->device.setRenderState(rsMap[state], (void*)(uintptr_t)v);
	return 1;
}
// L2:impl  (value receives the 32 bit state, like fpRenderStateGet)
RWAPI RwBool RwRenderStateGet(RwInt32 state, void *value){
	if(!engine || state <= 0 || state >= RS_MAX || !value) return 0;
	uint32_t v;
	if(rsMap[state] < 0){
		if(state == RS_PERSPECTIVE) v = 1; else v = g_extra[state];
	}else{
		if(!engine->device.getRenderState) return 0;
		v = (uint32_t)(uintptr_t)engine->device.getRenderState(rsMap[state]);
		if(state == RS_FOGCOLOR) v = fogToLib(v);	// the same swap in reverse
		if(state == RS_ALPHATESTFUNCTION) v += 1;
	}
	*(uint32_t*)value = v;
	return 1;
}

// ---- Im2D: RW's RwIm2DVertex (x y z rhw color u v, 28 bytes) is librw's Im2DVertex byte for byte; fixed function draws it as XYZRHW.
// L2:impl
RWAPI RwBool RwIm2DRenderLine(void *verts, RwInt32 numVerts, RwInt32 v1, RwInt32 v2){
	if(!engine || !engine->device.im2DRenderLine) return 0;
	engine->device.im2DRenderLine(verts, numVerts, v1, v2); return 1; }
// L2:impl
RWAPI RwBool RwIm2DRenderTriangle(void *verts, RwInt32 numVerts, RwInt32 v1, RwInt32 v2, RwInt32 v3){
	if(!engine || !engine->device.im2DRenderTriangle) return 0;
	engine->device.im2DRenderTriangle(verts, numVerts, v1, v2, v3); return 1; }
// L2:impl
RWAPI RwBool RwIm2DRenderPrimitive(RwInt32 primType, void *verts, RwInt32 numVerts){
	if(!engine || !engine->device.im2DRenderPrimitive) return 0;
	engine->device.im2DRenderPrimitive((PrimitiveType)primType, verts, numVerts); return 1; }
// L2:impl
RWAPI RwBool RwIm2DRenderIndexedPrimitive(RwInt32 primType, void *verts, RwInt32 numVerts, void *indices, RwInt32 numIndices){
	if(!engine || !engine->device.im2DRenderIndexedPrimitive) return 0;
	engine->device.im2DRenderIndexedPrimitive((PrimitiveType)primType, verts, numVerts, indices, numIndices); return 1; }

}	// extern "C"

// The device table inside the RwEngineInstance shadow (RwGlobals.dOpenDevice at 0x10: gamma 0x10, system 0x14, zNear 0x18, zFar 0x1c, fpRenderStateSet 0x20, Get 0x24,
// Im2D Line 0x28, Triangle 0x2c, Primitive 0x30, IndexedPrimitive 0x34), plus curCamera 0 / curWorld 4 (copied from librw's engine; the game's own camera / world
// objects are still stubs, so these stay NULL until RwCamera / RwWorld are swapped).
extern "C" void mflrw_shadow_device(uint8_t *raw){
	void **t = (void**)(raw + 0x20);
	t[0] = (void*)RwRenderStateSet; t[1] = (void*)RwRenderStateGet;
	t[2] = (void*)RwIm2DRenderLine; t[3] = (void*)RwIm2DRenderTriangle; t[4] = (void*)RwIm2DRenderPrimitive; t[5] = (void*)RwIm2DRenderIndexedPrimitive;
	if(engine){
		((float*)raw)[6] = engine->device.zNear; ((float*)raw)[7] = engine->device.zFar;
		*(void**)raw = engine->currentCamera; *(void**)(raw + 4) = engine->currentWorld;
	}
}
// the fixed function path is the target (the exe renders through D3D9 fixed function); called before Engine::start creates the device
extern "C" void mflrw_d3d9_prestart(void){
#ifdef RW_D3D9
	d3d::fixedFunction = 1;
#endif
}

#ifdef RW_D3D9
extern "C" {
// L2:impl
RWAPI void * RwD3D9GetCurrentD3DDevice(void){ return d3d::d3ddevice; }
// L2:impl
RWAPI void RwD3D9SetRenderState(RwUInt32 state, RwUInt32 value){ if(d3d::d3ddevice) d3d::setRenderState(state, value); }
// L2:impl
RWAPI void RwD3D9GetRenderState(RwUInt32 state, void *value){ if(d3d::d3ddevice && value) d3d::getRenderState(state, (uint32*)value); }
// L2:impl
RWAPI void RwD3D9SetTextureStageState(RwUInt32 stage, RwUInt32 type, RwUInt32 value){ if(d3d::d3ddevice) d3d::setTextureStageState(stage, type, value); }
// L2:impl
RWAPI void RwD3D9SetSamplerState(RwUInt32 stage, RwUInt32 type, RwUInt32 value){ if(d3d::d3ddevice) d3d::setSamplerState(stage, type, value); }
// L2:impl
RWAPI RwBool RwD3D9SetTransform(RwUInt32 state, const void *matrix){ if(!d3d::d3ddevice) return 0; if(!matrix){ static const float id[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }; matrix = id; } return d3d::setTransform(state, matrix); }
// L2:impl
RWAPI void RwD3D9GetTransform(RwUInt32 state, void *matrix){ if(d3d::d3ddevice && matrix) d3d::getTransform(state, matrix); }
// L2:impl  (NULL texture = NULL D3D texture in fixed function mode)
RWAPI RwBool RwD3D9SetTexture(RwTexture *texture, RwUInt32 stage){ if(!d3d::d3ddevice || stage >= 8) return 0; d3d::setTexture(stage, (Texture*)texture); return 1; }
// L2:impl
RWAPI void RwD3D9SetStreamSource(RwUInt32 n, void *data, RwUInt32 offset, RwUInt32 stride){ if(d3d::d3ddevice && n < 3) d3d::setStreamSource(n, data, offset, stride); }
// L2:impl  (flags = geometry flags: MODULATE 0x40 uses the colour, PRELIT 0x08 takes the emissive part from the vertex colour)
RWAPI RwBool RwD3D9SetSurfaceProperties(const RwSurfaceProperties *surfaceProps, const RwRGBA *color, RwUInt32 flags){
	if(!d3d::d3ddevice || !surfaceProps || !color) return 0;
	static const RGBA white = { 255, 255, 255, 255 };
	d3d::setMaterial_fix((flags & 0x40) ? *(const RGBA*)color : white, *(const SurfaceProperties*)surfaceProps);
	d3d::setRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, (flags & 0x08) ? D3DMCS_COLOR1 : D3DMCS_MATERIAL);
	d3d::setRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
	return 1;
}
// L2:impl
RWAPI int rwD3D9RenderStateFlushCache(void){ if(d3d::d3ddevice) d3d::flushCache(); return 1; }
// L2:impl
RWAPI RwBool RwD3D9CreateVertexShader(const RwUInt32 *function, void **shader){ if(!d3d::d3ddevice || !shader) return 0; *shader = d3d::createVertexShader((void*)function); return *shader != NULL; }
// L2:impl
RWAPI RwBool RwD3D9CreatePixelShader(const RwUInt32 *function, void **shader){ if(!d3d::d3ddevice || !shader) return 0; *shader = d3d::createPixelShader((void*)function); return *shader != NULL; }
// L2:impl  (the exe's DeleteVertexShader at 0x4dbc40 also releases pixel shaders: both are IUnknown)
RWAPI void RwD3D9DeleteVertexShader(void *shader){ if(shader) ((IUnknown*)shader)->Release(); }
// L2:impl  ponytail: the refresh rate is applied at device creation by librw (video mode list); runtime change not supported
RWAPI void RwD3D9EngineSetRefreshRate(RwUInt32 refreshRate){ (void)refreshRate; }
}
#else
// no D3D9 backend (mflrw_null.dll): the entries exist so the export list is the same, they do nothing
extern "C" {
// L2:impl (null backend)
RWAPI void * RwD3D9GetCurrentD3DDevice(void){ return NULL; }
// L2:impl (null backend)
RWAPI void RwD3D9SetRenderState(RwUInt32, RwUInt32){}
// L2:impl (null backend)
RWAPI void RwD3D9GetRenderState(RwUInt32, void*){}
// L2:impl (null backend)
RWAPI void RwD3D9SetTextureStageState(RwUInt32, RwUInt32, RwUInt32){}
// L2:impl (null backend)
RWAPI void RwD3D9SetSamplerState(RwUInt32, RwUInt32, RwUInt32){}
// L2:impl (null backend)
RWAPI RwBool RwD3D9SetTransform(RwUInt32, const void*){ return 0; }
// L2:impl (null backend)
RWAPI void RwD3D9GetTransform(RwUInt32, void*){}
// L2:impl (null backend)
RWAPI RwBool RwD3D9SetTexture(RwTexture*, RwUInt32){ return 0; }
// L2:impl (null backend)
RWAPI void RwD3D9SetStreamSource(RwUInt32, void*, RwUInt32, RwUInt32){}
// L2:impl (null backend)
RWAPI RwBool RwD3D9SetSurfaceProperties(const RwSurfaceProperties*, const RwRGBA*, RwUInt32){ return 0; }
// L2:impl (null backend)
RWAPI int rwD3D9RenderStateFlushCache(void){ return 0; }
// L2:impl (null backend)
RWAPI RwBool RwD3D9CreateVertexShader(const RwUInt32*, void**){ return 0; }
// L2:impl (null backend)
RWAPI RwBool RwD3D9CreatePixelShader(const RwUInt32*, void**){ return 0; }
// L2:impl (null backend)
RWAPI void RwD3D9DeleteVertexShader(void*){}
// L2:impl (null backend)
RWAPI void RwD3D9EngineSetRefreshRate(RwUInt32){}
}
#endif
