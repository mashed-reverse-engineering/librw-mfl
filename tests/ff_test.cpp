// S3 piece 2 acceptance: renders a clump (TXD + DFF given on the command line, e.g. a course prop or a car) through the d3d9 backend with the FIXED FUNCTION
// callback (d3d::fixedFunction = 1: defaultRenderCB_Fix, D3D lights / material, NULL texture = NULL) in a Windows window; the process stays up for ~8 s so the
// screen can be grabbed (tests/build_ff_test.sh, run under Wine + Xvfb).  usage: ff_test.exe file.dff [file.txd]   exit 0 = rendered
#define WITH_D3D
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "rw.h"
using namespace rw;

// backbuffer -> 24 bit BMP (the Xvfb root grab does not show Wine's D3D window reliably)
static void saveBackbuffer(const char *path)
{
	IDirect3DDevice9 *dev = d3d::d3ddevice;
	IDirect3DSurface9 *rt = nil, *sys = nil;
	D3DSURFACE_DESC desc;
	if(dev->GetRenderTarget(0, &rt) != D3D_OK) return;
	rt->GetDesc(&desc);
	if(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nil) == D3D_OK &&
	   dev->GetRenderTargetData(rt, sys) == D3D_OK){
		D3DLOCKED_RECT lr;
		if(sys->LockRect(&lr, nil, D3DLOCK_READONLY) == D3D_OK){
			FILE *f = fopen(path, "wb");
			uint32 sz = desc.Width*desc.Height*3, hdr[] = { 54 + sz, 0, 54, 40, desc.Width, desc.Height, 0x180001, 0, sz, 2835, 2835, 0, 0 };
			fwrite("BM", 2, 1, f); fwrite(hdr, 52, 1, f);
			for(int y = (int)desc.Height - 1; y >= 0; y--)
				for(uint32 x = 0; x < desc.Width; x++) fwrite((uint8*)lr.pBits + y*lr.Pitch + x*4, 3, 1, f);
			fclose(f);
			sys->UnlockRect();
		}
	}
	if(sys) sys->Release();
	rt->Release();
}

static void *readAll(const char *path, uint32 *len){ return getFileContents(path, len); }

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	if(argc < 2){ fprintf(stderr, "usage: ff_test file.dff [file.txd]\n"); return 2; }
	WNDCLASSA wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "ffwin"; RegisterClassA(&wc);
	HWND win = CreateWindowA("ffwin", "ff", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, wc.hInstance, NULL);
	EngineOpenParams op; op.window = win;
	d3d::fixedFunction = 1;
	if(!Engine::init()){ printf("FAIL engine init\n"); return 1; }
	// the plugins DFF loading needs (the game attaches its own set through RpMatFXPluginAttach etc.; the facade does not map those yet)
	registerMeshPlugin(); registerNativeDataPlugin(); registerAtomicRightsPlugin(); registerMaterialRightsPlugin(); xbox::registerVertexFormatPlugin();
	registerSkinPlugin(); registerUserDataPlugin(); registerHAnimPlugin(); registerMatFXPlugin(); registerUVAnimPlugin();
	if(!Engine::open(&op) || !Engine::start()){ printf("FAIL engine start\n"); return 1; }
	Image::setSearchPath("./");
	uint32 len = 0;
	if(argc > 2){
		uint8 *d = (uint8*)readAll(argv[2], &len);
		StreamMemory in; in.open(d, len);
		TexDictionary *t = nil;
		if(d && len >= 4 && *(uint32*)d == ID_PITEXDICTIONARY) t = TexDictionary::streamReadPI(&in);	// the game's platform independent TXD
		else if(d && findChunk(&in, ID_TEXDICTIONARY, nil, nil)) t = TexDictionary::streamRead(&in);
		if(t) TexDictionary::setCurrent(t); else printf("no TXD loaded\n");
		in.close();
	}
	uint8 *d = (uint8*)readAll(argv[1], &len);
	if(!d){ printf("FAIL read %s\n", argv[1]); return 1; }
	StreamMemory in; in.open(d, len);
	Clump *clump = findChunk(&in, ID_CLUMP, nil, nil) ? Clump::streamRead(&in) : nil;
	in.close();
	if(!clump){ printf("FAIL clump\n"); return 1; }
	World *world = World::create();
	Light *l = Light::create(Light::AMBIENT); l->setColor(0.4f, 0.4f, 0.4f); world->addLight(l);
	l = Light::create(Light::DIRECTIONAL); l->setColor(0.8f, 0.8f, 0.8f); l->setFrame(Frame::create());
	V3d xaxis = { 1.0f, 0.0f, 0.0f }; l->getFrame()->rotate(&xaxis, 150.0f, COMBINEREPLACE); world->addLight(l);
	{ int nm = 0, nt = 0, nr = 0; FORLIST(lnk, clump->atomics){ Geometry *g = Atomic::fromClump(lnk)->geometry; for(int i = 0; i < g->matList.numMaterials; i++){ nm++; Texture *t = g->matList.materials[i]->texture; if(t){ nt++; if(t->raster) nr++; } } }
	  printf("materials %d, with texture %d, with raster %d\n", nm, nt, nr); }
	world->addClump(clump);
	Camera *cam = Camera::create(); cam->setFrame(Frame::create());
	cam->frameBuffer = Raster::create(640, 480, 0, Raster::CAMERA);
	cam->zBuffer = Raster::create(640, 480, 0, Raster::ZBUFFER);
	world->addCamera(cam);
	// fit: bounding sphere of the atomics
	V3d c = { 0, 0, 0 }; int n = 0; float r = 0;
	FORLIST(lnk, clump->atomics){ Sphere *s = Atomic::fromClump(lnk)->getWorldBoundingSphere(); c = add(c, s->center); n++; }
	if(n) c = scale(c, 1.0f/n);
	FORLIST(lnk, clump->atomics){ Sphere *s = Atomic::fromClump(lnk)->getWorldBoundingSphere(); float dd = length(sub(s->center, c)) + s->radius; if(dd > r) r = dd; }
	if(r < 0.001f) r = 1.0f;
	cam->setNearPlane(r*0.01f > 0.05f ? r*0.01f : 0.05f); cam->setFarPlane(r*20.0f > 100.0f ? r*20.0f : 100.0f);
	cam->setFOV(60.0f, 640.0f/480.0f);
	Frame *f = cam->getFrame();
	// look from -Y toward the centre, up = +Z
	V3d pos = add(c, V3d{ 0.0f, -r*2.5f, r*0.7f });
	V3d at = normalize(sub(c, pos)), up = { 0, 0, 1 }, right = normalize(cross(at, up)); up = cross(right, at);
	Matrix *m = &f->matrix; m->right = right; m->up = up; m->at = at; m->pos = pos; m->flags = 0; m->optimize(); f->updateObjects();
	RGBA bg = { 64, 64, 96, 255 };
	DWORD t0 = GetTickCount();
	int frames = 0;
	while(GetTickCount() - t0 < 8000){
		MSG msg; while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
		cam->clear(&bg, Camera::CLEARIMAGE | Camera::CLEARZ);
		cam->beginUpdate();
		world->render();
		cam->endUpdate();
		if(frames == 30) saveBackbuffer("ff.bmp");
		cam->showRaster(0);
		frames++;
	}
	printf("RESULT: rendered %d frames, %d atomics\n", frames, n);
	return 0;
}
