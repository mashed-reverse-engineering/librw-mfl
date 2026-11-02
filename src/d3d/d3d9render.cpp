#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "rwd3d.h"
#include "rwd3d9.h"

namespace rw {
namespace d3d9 {
using namespace d3d;

#ifndef RW_D3D9
void defaultRenderCB(Atomic*, InstanceDataHeader*) {}
void defaultRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header) {}
#else

void
drawInst_simple(d3d9::InstanceDataHeader *header, d3d9::InstanceData *inst)
{
	d3d::flushCache();
	d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)header->primType, inst->baseIndex,
	                                0, inst->numVertices,
	                                inst->startIndex, inst->numPrimitives);
}

// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void
drawInst_GSemu(d3d9::InstanceDataHeader *header, InstanceData *inst)
{
	uint32 hasAlpha;
	int alphafunc, alpharef, gsalpharef;
	int zwrite;
	d3d::getRenderState(D3DRS_ALPHABLENDENABLE, &hasAlpha);
	if(hasAlpha){
		zwrite = rw::GetRenderState(rw::ZWRITEENABLE);
		alphafunc = rw::GetRenderState(rw::ALPHATESTFUNC);
		if(zwrite){
			alpharef = rw::GetRenderState(rw::ALPHATESTREF);
			gsalpharef = rw::GetRenderState(rw::GSALPHATESTREF);

			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAGREATEREQUAL);
			SetRenderState(rw::ALPHATESTREF, gsalpharef);
			drawInst_simple(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHALESS);
			SetRenderState(rw::ZWRITEENABLE, 0);
			drawInst_simple(header, inst);
			SetRenderState(rw::ZWRITEENABLE, 1);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
			SetRenderState(rw::ALPHATESTREF, alpharef);
		}else{
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAALWAYS);
			drawInst_simple(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
		}
	}else
		drawInst_simple(header, inst);
}

void
drawInst(d3d9::InstanceDataHeader *header, d3d9::InstanceData *inst)
{
	if(rw::GetRenderState(rw::GSALPHATEST))
		drawInst_GSemu(header, inst);
	else
		drawInst_simple(header, inst);
}

// Fixed function default atomic callback: the exe's _rxD3D9DefaultRenderCallback (0x4faeb0, atomic case) + lighting callback (lightingCB_MFL) + RwD3D9SetSurfaceProperties
// (setSurfaceProperties_MFL).  Verified against the rwtap replay of course 4 (tools/rwtap/rwreplay.sh ATOMS=1, ROADMAP T-P9).
// ponytail: the black path (no lighting, not prelit) is transcribed from the disassembly but no course-4 draw takes it; world sectors (type 2, BBox test) are not handled.
void
defaultRenderCB_Fix(Atomic *atomic, InstanceDataHeader *header)
{
	RawMatrix world;
	Geometry *geo = atomic->geometry;
	uint32 gf = geo->flags;

	d3d::lightingCB_MFL(atomic);

	// D3DRS_CLIPPING = the bounding sphere is not fully inside the frustum (RwD3D9CameraIsSphereFullyInsideFrustum)
	Camera *cam = (Camera*)engine->currentCamera;
	d3d::setRenderState(D3DRS_CLIPPING, cam->frustumTestSphere(atomic->getWorldBoundingSphere()) == Camera::SPHEREINSIDE ? 0 : 1);

	Frame *f = atomic->getFrame();
	convMatrix(&world, f->getLTM());
	d3d::setTransform(D3DTS_WORLD, &world);

	setVertexShader(nil);
	setPixelShader(nil);
	setStreamSource(0, header->vertexStream[0].vertexBuffer, 0, header->vertexStream[0].stride);
	setIndices(header->indexBuffer);
	setVertexDeclaration(header->vertexDeclaration);

	uint32 lighting;
	d3d::getRenderState(D3DRS_LIGHTING, &lighting);
	InstanceData *inst = header->inst;

	if(!lighting && !(gf & Geometry::PRELIT)){
		// black: stage 0 outputs D3DRS_TEXTUREFACTOR (opaque black), only the alpha of an alpha texture is kept
		uint32 dither, shade;
		d3d::getRenderState(D3DRS_DITHERENABLE, &dither);
		d3d::getRenderState(D3DRS_SHADEMODE, &shade);
		SetRenderState(VERTEXALPHA, 0);
		d3d::setRenderState(D3DRS_TEXTUREFACTOR, 0xff000000);
		d3d::setRenderState(D3DRS_DITHERENABLE, 0);
		d3d::setRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
		d3d::setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
		d3d::setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		d3d::setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		d3d::setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		d3d::setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
		d3d::setTexture(0, nil);
		bool texAlpha = false;
		for(uint32 i = 0; i < header->numMeshes; i++, inst++){
			Material *m = inst->material;
			if((gf & (Geometry::TEXTURED | Geometry::TEXTURED2)) && m->texture && m->texture->raster && GETD3DRASTEREXT(m->texture->raster)->hasAlpha){
				d3d::setTexture(0, m->texture);
				if(!texAlpha){
					d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
					texAlpha = true;
				}
			}else if(texAlpha){
				texAlpha = false;
				d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
				d3d::setTexture(0, nil);
			}
			drawInst(header, inst);
		}
		d3d::setRenderState(D3DRS_DITHERENABLE, dither);
		d3d::setRenderState(D3DRS_SHADEMODE, shade);
		return;
	}

	// mode bit 0 = textured (stage 0 texture x diffuse), bit 1 = unlit material colour via D3DRS_TEXTUREFACTOR on stage 1 (stage 1 modulates CURRENT with it)
	uint32 prev = 0x80000000;
	for(uint32 i = 0; i < header->numMeshes; i++, inst++){
		Material *m = inst->material;
		uint32 mode = 0;
		SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 255);
		if(lighting)
			d3d::setSurfaceProperties_MFL(m->surfaceProps, m->color, gf);
		else if((gf & Geometry::MODULATE) && *(uint32*)&m->color != 0xffffffffu){
			d3d::setRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(m->color.alpha, m->color.red, m->color.green, m->color.blue));
			mode = 2;
		}
		if(m->texture && (gf & (Geometry::TEXTURED | Geometry::TEXTURED2))){
			mode |= 1;
			d3d::setTexture(0, m->texture);
		}else
			d3d::setTexture(0, nil);

		if(mode != prev){
			if(mode & 1){
				if(!(prev & 1)){
					d3d::setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
					d3d::setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
					d3d::setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
					d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
					d3d::setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
					d3d::setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				}
				if(mode & 2){
					d3d::setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
					d3d::setTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
					d3d::setTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
					d3d::setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
					d3d::setTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
					d3d::setTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
				}else if(prev == 3){
					d3d::setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
					d3d::setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				}
			}else{
				if(mode & 2){
					d3d::setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
					d3d::setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
					d3d::setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
					d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
					d3d::setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
					d3d::setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
				}else{
					d3d::setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
					d3d::setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
					d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
					d3d::setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				}
				if(prev == 3){
					d3d::setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
					d3d::setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				}
			}
			prev = mode;
		}
		drawInst(header, inst);
	}
	if(prev == 3){
		d3d::setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		d3d::setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}
}

void
defaultRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header)
{
	if(d3d::fixedFunction){ defaultRenderCB_Fix(atomic, header); return; }
	int vsBits;
	uint32 flags = atomic->geometry->flags;
	setStreamSource(0, header->vertexStream[0].vertexBuffer, 0, header->vertexStream[0].stride);
	setIndices(header->indexBuffer);
	setVertexDeclaration(header->vertexDeclaration);

	vsBits = lightingCB_Shader(atomic);
	uploadMatrices(atomic->getFrame()->getLTM());

	// Pick a shader
	if((vsBits & VSLIGHT_MASK) == 0)
		setVertexShader(default_amb_VS);
	else if((vsBits & VSLIGHT_MASK) == VSLIGHT_DIRECT)
		setVertexShader(default_amb_dir_VS);
	else
		setVertexShader(default_all_VS);

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		Material *m = inst->material;

		SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 255);

		setMaterial(flags, m->color, m->surfaceProps);

		if(m->texture){
			d3d::setTexture(0, m->texture);
			setPixelShader(default_tex_PS);
		}else
			setPixelShader(default_PS);

		drawInst(header, inst);
		inst++;
	}
}

#endif
}
}
