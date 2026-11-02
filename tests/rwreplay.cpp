// B9: replay half of the RW-API oracle.  Feeds a command script made by ghidra/tools/rwtap/rwreplay_gen.py (from a recorded rwtap.log) into the librw-mfl
// D3D9 fixed-function layer (RwRenderStateSet / RwIm2D* of capi/d3d9dev.cpp, rw::Camera for clear / begin / show) in a 640x480 window.  Run under Wine with the
// d3dtrace proxy renamed d3dp.dll (tools/rwtap/rwreplay.sh) so the same per-draw D3D trace as the original game is produced.  usage: rwreplay.exe replay.txt
#define WITH_D3D
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <sstream>
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

// ---- course mode (rwreplay_gen.py --atoms): atomics rebuilt from the rwtap G / M / A records and rendered through the fixed-function callback
static std::map<unsigned, Material*> g_mat; static std::map<unsigned, Geometry*> g_geo; static std::map<unsigned, Atomic*> g_atom; static std::map<unsigned, unsigned> g_atomGeo;
static std::map<std::string, Raster*> g_ras;
static std::vector<std::string> split(const std::string &l){ std::vector<std::string> v; std::istringstream is(l); std::string t; while(is >> t) v.push_back(t); return v; }
static Raster *makeRaster(int w, int h, unsigned fmt, int levels)
{
	char key[64]; sprintf(key, "%d_%d_%u_%d", w, h, fmt, levels);
	Raster *&r = g_ras[key]; if(r) return r;
	int format = Raster::C8888 | (levels > 1 ? Raster::MIPMAP : 0);
	r = Raster::create(w, h, 32, format | Raster::TEXTURE | Raster::DONTALLOCATE, PLATFORM_D3D9);
	d3d::D3dRaster *ext = GETD3DRASTEREXT(r);
	ext->format = fmt; ext->hasAlpha = fmt != 22;
	ext->texture = d3d::createTexture(w, h, levels, 0, fmt); ext->customFormat = 1;
	r->flags &= ~Raster::DONTALLOCATE;
	return r;
}
static void cmdMat(const std::vector<std::string> &t)	// mat ptr col sp tex fa w h fmt levels
{
	Material *m = Material::create(); unsigned ptr = strtoul(t[1].c_str(), 0, 16), c = strtoul(t[2].c_str(), 0, 16);
	m->color.red = c >> 24; m->color.green = c >> 16; m->color.blue = c >> 8; m->color.alpha = c;
	std::vector<unsigned char> sp = unhex(t[3].c_str()); memcpy(&m->surfaceProps, sp.data(), 12);
	if(t[4] != "-"){
		Texture *tx = Texture::create(makeRaster(atoi(t[6].c_str()), atoi(t[7].c_str()), strtoul(t[8].c_str(), 0, 10), atoi(t[9].c_str())));
		strncpy(tx->name, t[4].c_str(), 31); tx->filterAddressing = strtoul(t[5].c_str(), 0, 16); m->setTexture(tx);
	}
	g_mat[ptr] = m;
}
static void cmdGeo(const std::vector<std::string> &t)	// geo ptr flags nt nv ntc nmat mats tri lit tc0 tc1 sph verts norm mesh
{
	unsigned ptr = strtoul(t[1].c_str(), 0, 16), fl = strtoul(t[2].c_str(), 0, 16); int nt = atoi(t[3].c_str()), nv = atoi(t[4].c_str()), ntc = atoi(t[5].c_str());
	Geometry *g = Geometry::create(nv, nt, fl | (ntc << 16));
	// materials
	std::vector<Material*> ml; { std::string s = t[7]; if(s != "-"){ std::istringstream is(s); std::string m; while(std::getline(is, m, ',')) ml.push_back(g_mat[strtoul(m.c_str(), 0, 16)]); } }
	for(size_t i = 0; i < ml.size(); i++) g->matList.appendMaterial(ml[i]);
	if(t[8] != "-"){ std::vector<unsigned char> v = unhex(t[8].c_str()); memcpy(g->triangles, v.data(), nt*8); }
	if(t[9] != "-" && g->colors){ std::vector<unsigned char> v = unhex(t[9].c_str()); memcpy(g->colors, v.data(), nv*4); }
	for(int s = 0; s < ntc && s < 2; s++) if(t[10 + s] != "-"){ std::vector<unsigned char> v = unhex(t[10 + s].c_str()); memcpy(g->texCoords[s], v.data(), nv*8); }
	if(t[12] != "-"){ std::vector<unsigned char> v = unhex(t[12].c_str()); memcpy(&g->morphTargets[0].boundingSphere, v.data(), 16); }
	if(t[13] != "-"){ std::vector<unsigned char> v = unhex(t[13].c_str()); memcpy(g->morphTargets[0].vertices, v.data(), nv*12); }
	if(t[14] != "-" && g->morphTargets[0].normals){ std::vector<unsigned char> v = unhex(t[14].c_str()); memcpy(g->morphTargets[0].normals, v.data(), nv*12); }
	// meshes: flags:n;mat:count:hex;...
	if(t[15] != "-"){
		std::vector<std::string> parts; { std::istringstream is(t[15]); std::string p; while(std::getline(is, p, ';')) parts.push_back(p); }
		unsigned mflags = strtoul(parts[0].c_str(), 0, 16); int nm = atoi(strchr(parts[0].c_str(), ':') + 1); unsigned total = 0;
		std::vector<std::vector<unsigned char> > idx; std::vector<unsigned> cnt, mat;
		for(int i = 0; i < nm; i++){
			const std::string &p = parts[1 + i]; size_t a = p.find(':'), b = p.find(':', a + 1);
			mat.push_back(strtoul(p.substr(0, a).c_str(), 0, 16)); cnt.push_back(atoi(p.substr(a + 1, b - a - 1).c_str())); idx.push_back(unhex(p.c_str() + b + 1)); total += cnt[i];
		}
		MeshHeader *mh = g->allocateMeshes(nm, total, 0); mh->flags = mflags;
		Mesh *m = mh->getMeshes(); uint16 *ip = (uint16*)&m[nm];
		for(int i = 0; i < nm; i++){ m[i].numIndices = cnt[i]; m[i].material = g_mat[mat[i]]; m[i].indices = ip; memcpy(ip, idx[i].data(), cnt[i]*2); ip += cnt[i]; }
	}
	// a changed geometry invalidates its atomics
	for(std::map<unsigned, unsigned>::iterator it = g_atomGeo.begin(); it != g_atomGeo.end(); ++it) if(it->second == ptr){ g_atom.erase(it->first); }
	g_geo[ptr] = g;
}
static void cmdSetTex(const std::vector<std::string> &t)	// settex stage fa|-  (RwD3D9RWSetTexture of a pass that is not replayed: its sampler effect and binding)
{
	int stage = atoi(t[1].c_str());
	if(t[2] == "-"){ d3d::setTexture(stage, nil); return; }
	unsigned fa = strtoul(t[2].c_str(), 0, 16); static std::map<unsigned, Texture*> cache;
	Texture *&tx = cache[fa];
	if(!tx){ tx = Texture::create(makeRaster(4, 4, 21, 1)); tx->filterAddressing = fa; }
	d3d::setTexture(stage, tx);
}
static World *g_world; static Light *g_amb; static std::vector<Light*> g_lights;
static void cmdLights(const std::vector<std::string> &t)	// lights ambient[3 floats hex] n hex104...
{
	for(size_t i = 0; i < g_lights.size(); i++){ g_world->removeLight(g_lights[i]); }
	if(g_amb) g_world->removeLight(g_amb);
	g_lights.clear(); g_amb = 0;
	std::vector<unsigned char> ab = unhex(t[1].c_str()); float amb[3]; memcpy(amb, ab.data(), 12); int n = atoi(t[2].c_str());
	if(amb[0] != 0.0f || amb[1] != 0.0f || amb[2] != 0.0f){ g_amb = Light::create(Light::AMBIENT); g_amb->setColor(amb[0], amb[1], amb[2]); g_world->addLight(g_amb); }
	for(int i = 0; i < n; i++){
		std::vector<unsigned char> v = unhex(t[3 + i].c_str()); float f[26]; memcpy(f, v.data(), 104); unsigned type = *(unsigned*)v.data();
		if(type != 3){ printf("light type %u not replayed\n", type); continue; }
		Light *l = Light::create(Light::DIRECTIONAL); l->setColor(f[1], f[2], f[3]); l->color.alpha = f[4]; l->setFrame(Frame::create());
		Matrix *m = &l->getFrame()->matrix; V3d at = { f[16], f[17], f[18] }, up = { 0, 0, 1 };
		if(fabsf(at.z) > 0.9f) up = { 1, 0, 0 };
		V3d right = normalize(cross(up, at)); up = cross(at, right);	// right-handed RW: right x up = at
		m->right = right; m->up = up; m->at = at; m->pos = { 0, 0, 0 }; m->flags = 0; l->getFrame()->updateObjects();
		g_world->addLight(l); g_lights.push_back(l);
	}
}
static void cmdAtom(const std::vector<std::string> &t, Camera *cam)	// atom ptr geo flags ltm
{
	unsigned ptr = strtoul(t[1].c_str(), 0, 16), gp = strtoul(t[2].c_str(), 0, 16);
	Atomic *&a = g_atom[ptr];
	if(!a){
		a = Atomic::create(); a->setFrame(Frame::create()); a->setGeometry(g_geo[gp], 0); g_atomGeo[ptr] = gp;
	}
	a->setFlags(strtoul(t[3].c_str(), 0, 16));
	std::vector<unsigned char> v = unhex(t[4].c_str()); memcpy(&a->getFrame()->matrix, v.data(), 64); a->getFrame()->updateObjects();
	if(t.size() > 5 && t[5] != "-"){ std::vector<unsigned char> b = unhex(t[5].c_str()); memcpy(&a->boundingSphere, b.data(), 16); }	// the atomic's own bounding sphere (frustum test)
	a->object.object.privateFlags |= Atomic::WORLDBOUNDDIRTY;
}
static void cmdBegin(const std::vector<std::string> &t, Camera *cam)	// begin view[64 B] viewwindow.x viewwindow.y near far
{
	std::vector<unsigned char> b = unhex(t[1].c_str()); float V[16]; memcpy(V, b.data(), 64);
	Matrix inv, ltm; inv.right = { -V[0], V[1], V[2] }; inv.up = { -V[4], V[5], V[6] }; inv.at = { -V[8], V[9], V[10] }; inv.pos = { -V[12], V[13], V[14] }; inv.flags = 0; inv.optimize();
	Matrix::invert(&ltm, &inv);
	cam->viewWindow.x = (float)atof(t[2].c_str()); cam->viewWindow.y = (float)atof(t[3].c_str()); cam->setNearPlane((float)atof(t[4].c_str())); cam->setFarPlane((float)atof(t[5].c_str()));
	Matrix *m = &cam->getFrame()->matrix; *m = ltm; m->flags = 0; m->optimize(); cam->getFrame()->updateObjects();
}

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
	Raster *ras[4096] = { 0 };
	g_world = World::create(); g_world->addCamera(cam);
	bool inUpdate = false; int n = 0, frames = 0;
	static char line[1 << 20], h1[1 << 19], h2[1 << 15];
	std::string big;
	for(;;){
		big.clear(); int ch; bool any = false;
		while((ch = fgetc(f)) != EOF){ any = true; if(ch == '\n') break; big += (char)ch; }
		if(!any) break;
		strncpy(line, big.c_str(), sizeof line - 1);
		char c[16]; int a, b, d, e; unsigned u; float x, y;
		if(sscanf(line, "%15s", c) != 1) continue;
		const char *r = line + strlen(c);
		n++;
		if(!strcmp(c, "mat") || !strcmp(c, "geo") || !strcmp(c, "lights") || !strcmp(c, "atom") || !strcmp(c, "begin") || !strcmp(c, "end") || !strcmp(c, "render") || !strcmp(c, "settex")){
			std::vector<std::string> t = split(big);
			if(!strcmp(c, "mat")) cmdMat(t);
			else if(!strcmp(c, "geo")) cmdGeo(t);
			else if(!strcmp(c, "lights")) cmdLights(t);
			else if(!strcmp(c, "settex")) cmdSetTex(t);
			else if(!strcmp(c, "atom")) cmdAtom(t, cam);
			else if(!strcmp(c, "begin")){ if(inUpdate) cam->endUpdate(); cmdBegin(t, cam); cam->beginUpdate(); inUpdate = true; }
			else if(!strcmp(c, "end")){ if(inUpdate) cam->endUpdate(); inUpdate = false; }
			else{ Atomic *at = g_atom[strtoul(t[1].c_str(), 0, 16)]; if(at && inUpdate) at->render(); else printf("render: no atomic / not in update\n"); }
		}
		else if(!strcmp(c, "viewwindow")){ sscanf(r, "%f %f", &x, &y); cam->viewWindow.x = x; cam->viewWindow.y = y; }
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
