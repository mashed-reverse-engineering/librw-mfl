// B9: replay half of the RW-API oracle.  Feeds a command script made by ghidra/tools/rwtap/rwreplay_gen.py (from a recorded rwtap.log) into the librw-mfl
// D3D9 fixed-function layer (RwRenderStateSet / RwIm2D* of capi/d3d9dev.cpp, rw::Camera for clear / begin / show) in a 640x480 window.  Run under Wine with the
// d3dtrace proxy renamed d3dp.dll (tools/rwtap/rwreplay.sh) so the same per-draw D3D trace as the original game is produced.  usage: rwreplay.exe replay.txt
#define WITH_D3D
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "rw.h"
using namespace rw;
extern "C" {
int RwRenderStateSet(int, void *);
int RwIm2DRenderPrimitive(int, void *, int);
int RwIm2DRenderIndexedPrimitive(int, void *, int, void *, int);
int RwIm2DRenderTriangle(void *, int, int, int, int);
int RwIm2DRenderLine(void *, int, int, int);
void RwD3D9SetRenderState(unsigned, unsigned);
void RwD3D9SetTextureStageState(unsigned, unsigned, unsigned);
void RwD3D9SetSamplerState(unsigned, unsigned, unsigned);
}
static std::vector<unsigned char> unhex(const char *s){ std::vector<unsigned char> v; for(; s[0] && s[1]; s += 2){ char b[3] = { s[0], s[1], 0 }; v.push_back((unsigned char)strtoul(b, NULL, 16)); } return v; }

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	if(argc < 2){ fprintf(stderr, "usage: rwreplay replay.txt\n"); return 2; }
	FILE *f = fopen(argv[1], "r"); if(!f){ printf("FAIL open %s\n", argv[1]); return 2; }
	WNDCLASSA wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "rwrp"; RegisterClassA(&wc);
	HWND win = CreateWindowA("rwrp", "rwreplay", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, wc.hInstance, NULL);
	EngineOpenParams op; op.window = win;
	d3d::fixedFunction = 1;
	if(!Engine::init() || !Engine::open(&op) || !Engine::start()){ printf("FAIL engine\n"); return 1; }
	Camera *cam = Camera::create(); cam->setFrame(Frame::create());
	cam->frameBuffer = Raster::create(640, 480, 0, Raster::CAMERA);
	cam->zBuffer = Raster::create(640, 480, 0, Raster::ZBUFFER);
	Raster *ras[64] = { 0 };
	bool inUpdate = false; int n = 0, frames = 0;
	static char line[1 << 20];
	while(fgets(line, sizeof line, f)){
		char c[16]; int a, b, d, e; unsigned u; float x, y; char h1[1 << 19], h2[1 << 15];
		if(sscanf(line, "%15s", c) != 1) continue;
		const char *r = line + strlen(c);
		n++;
		if(!strcmp(c, "viewwindow")){ sscanf(r, "%f %f", &x, &y); cam->viewWindow.x = x; cam->viewWindow.y = y; }
		else if(!strcmp(c, "near")){ sscanf(r, "%f", &x); cam->setNearPlane(x); }
		else if(!strcmp(c, "far")){ sscanf(r, "%f", &x); cam->setFarPlane(x); }
		else if(!strcmp(c, "clear")){ RGBA bg = { 0, 0, 0, 255 }; if(inUpdate){ cam->endUpdate(); inUpdate = false; } cam->clear(&bg, Camera::CLEARIMAGE | Camera::CLEARZ); }
		else if(!strcmp(c, "show")){ if(inUpdate){ cam->endUpdate(); inUpdate = false; } cam->showRaster(0); frames++; }
		else if(!strcmp(c, "raster")){ sscanf(r, "%d %d %d %d", &a, &b, &d, &e); ras[a] = Raster::create(b, d, 32, Raster::C8888 | Raster::TEXTURE); if(!ras[a]) printf("raster %d failed\n", a); }
		else if(!strcmp(c, "rs")){ char v[32]; sscanf(r, "%d %31s", &a, v); RwRenderStateSet(a, v[0] == 'R' ? (void*)ras[atoi(v + 1)] : (void*)strtoul(v, NULL, 16)); }
		else if(!strcmp(c, "d3drs")){ sscanf(r, "%d %x", &a, &u); RwD3D9SetRenderState(a, u); }
		else if(!strcmp(c, "tss")){ sscanf(r, "%d %d %x", &a, &b, &u); RwD3D9SetTextureStageState(a, b, u); }
		else if(!strcmp(c, "ss")){ sscanf(r, "%d %d %x", &a, &b, &u); RwD3D9SetSamplerState(a, b, u); }
		else{
			if(!inUpdate){ cam->beginUpdate(); inUpdate = true; }
			if(!strcmp(c, "prim")){ sscanf(r, "%d %d %s", &a, &b, h1); std::vector<unsigned char> v = unhex(h1); RwIm2DRenderPrimitive(a, v.data(), b); }
			else if(!strcmp(c, "iprim")){ sscanf(r, "%d %d %d %s %s", &a, &b, &d, h1, h2); std::vector<unsigned char> v = unhex(h1), i = unhex(h2); RwIm2DRenderIndexedPrimitive(a, v.data(), b, i.data(), d); }
			else if(!strcmp(c, "tri")){ sscanf(r, "%d %d %d %d %s", &a, &b, &d, &e, h1); std::vector<unsigned char> v = unhex(h1); RwIm2DRenderTriangle(v.data(), a, b, d, e); }
			else if(!strcmp(c, "line")){ sscanf(r, "%d %d %d %s", &a, &b, &d, h1); std::vector<unsigned char> v = unhex(h1); RwIm2DRenderLine(v.data(), a, b, d); }
			else printf("unknown command %s\n", c);
		}
	}
	printf("RESULT: %d commands, %d frames\n", n, frames);
	return 0;
}
