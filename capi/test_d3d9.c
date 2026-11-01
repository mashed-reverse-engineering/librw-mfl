/* S3 piece 1 test for mflrw.dll (D3D9 backend), Wine + Xvfb: engine start with a real window, the device table in the RwEngineInstance shadow (+0x20..0x34),
   RwRenderStateSet / Get round trips with RW 3.7 enum values, the ZTEST / ZWRITE coupling, RwD3D9 state entries.  usage: test_d3d9.exe [mflrw.dll]  (exit 0 = pass) */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

static int fails;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
typedef int (*SetFn)(int, void *); typedef int (*GetFn)(int, void *);
enum { RS_TEXTURERASTER = 1, RS_ZTEST = 6, RS_ZWRITE = 8, RS_SRC = 10, RS_DST = 11, RS_FOGENABLE = 14, RS_FOGCOLOR = 15, RS_FOGTYPE = 16, RS_CULL = 20, RS_ALPHAFUNC = 29, RS_ALPHAREF = 30 };
#define D3DRS_ZENABLE 7
#define D3DRS_ZWRITEENABLE 14
#define D3DRS_ZFUNC 23
#define D3DRS_FOGENABLE 28
#define D3DRS_FOGTABLEMODE 35
#define D3DRS_ALPHAFUNC 25
#define D3DRS_SRCBLEND 19

int main(int argc, char **argv){
	setvbuf(stdout, NULL, _IONBF, 0);
	HMODULE h = LoadLibraryA(argc > 1 ? argv[1] : "mflrw.dll");
	if(!h){ printf("FAIL LoadLibrary %lu\n", GetLastError()); return 2; }
#define P(n) __typeof__(&n) n##_p = (__typeof__(&n))GetProcAddress(h, #n); if(!n##_p){ printf("FAIL missing export %s\n", #n); return 2; }
	P(RwEngineInit) P(RwEngineOpen) P(RwEngineStart) P(RwEngineStop) P(RwEngineClose) P(RwEngineTerm)
	P(RwRenderStateSet) P(RwRenderStateGet) P(RwIm2DRenderTriangle) P(RwIm2DRenderPrimitive)
	P(RwD3D9GetCurrentD3DDevice) P(RwD3D9SetRenderState) P(RwD3D9GetRenderState) P(RwD3D9SetTransform) P(RwD3D9GetTransform) P(RwD3D9SetTexture) P(rwD3D9RenderStateFlushCache)
	P(RwD3D9SetSurfaceProperties)
	WNDCLASSA wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "ffwin"; RegisterClassA(&wc);
	HWND win = CreateWindowA("ffwin", "ff", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, wc.hInstance, NULL);
	CHECK(win != NULL);
	struct { HWND w; } op = { win };
	CHECK(RwEngineInit_p(NULL, 0, 0));
	CHECK(RwEngineOpen_p((RwEngineOpenParams *)&op));
	CHECK(RwEngineStart_p());
	CHECK(RwD3D9GetCurrentD3DDevice_p() != NULL);
	uint8_t *shadow = *(uint8_t **)GetProcAddress(h, "RwEngineInstance");
	SetFn set = *(SetFn *)(shadow + 0x20); GetFn get = *(GetFn *)(shadow + 0x24);
	CHECK(set == (SetFn)RwRenderStateSet_p && get == (GetFn)RwRenderStateGet_p);
	CHECK(*(void **)(shadow + 0x2c) == (void *)RwIm2DRenderTriangle_p && *(void **)(shadow + 0x30) == (void *)RwIm2DRenderPrimitive_p);
	uint32_t v, d;
	/* ZTEST / ZWRITE coupling (RW d3d9): write without test = ZENABLE on with ZFUNC ALWAYS; neither = ZENABLE off */
	set(RS_ZTEST, (void *)1); set(RS_ZWRITE, (void *)1);
	RwD3D9GetRenderState_p(D3DRS_ZENABLE, &d); CHECK(d == 1); RwD3D9GetRenderState_p(D3DRS_ZFUNC, &d); CHECK(d == 4);
	set(RS_ZTEST, (void *)0);
	RwD3D9GetRenderState_p(D3DRS_ZENABLE, &d); CHECK(d == 1); RwD3D9GetRenderState_p(D3DRS_ZFUNC, &d); CHECK(d == 8);
	set(RS_ZWRITE, (void *)0);
	RwD3D9GetRenderState_p(D3DRS_ZENABLE, &d); CHECK(d == 0); RwD3D9GetRenderState_p(D3DRS_ZWRITEENABLE, &d); CHECK(d == 0);
	set(RS_ZWRITE, (void *)1); set(RS_ZTEST, (void *)1);
	RwD3D9GetRenderState_p(D3DRS_ZENABLE, &d); CHECK(d == 1); RwD3D9GetRenderState_p(D3DRS_ZFUNC, &d); CHECK(d == 4);
	/* round trips of the RW enum values (blend 5 = SRCALPHA, cull 2 = NONE, alpha func 3 = LESS, fog type 2 = EXP) */
	set(RS_SRC, (void *)5); CHECK(get(RS_SRC, &v) && v == 5); RwD3D9GetRenderState_p(D3DRS_SRCBLEND, &d); CHECK(d == 5);
	set(RS_CULL, (void *)2); CHECK(get(RS_CULL, &v) && v == 2);
	set(RS_ALPHAFUNC, (void *)3); CHECK(get(RS_ALPHAFUNC, &v) && v == 3); RwD3D9GetRenderState_p(D3DRS_ALPHAFUNC, &d); CHECK(d == 2 /* D3DCMP_LESS */);
	set(RS_ALPHAREF, (void *)0x40); CHECK(get(RS_ALPHAREF, &v) && v == 0x40);
	set(RS_FOGCOLOR, (void *)0xff102030u); CHECK(get(RS_FOGCOLOR, &v) && v == 0xff102030u); RwD3D9GetRenderState_p(34 /* FOGCOLOR */, &d); CHECK(d == 0xff102030u);
	set(RS_FOGENABLE, (void *)1); CHECK(get(RS_FOGENABLE, &v) && v == 1); RwD3D9GetRenderState_p(D3DRS_FOGENABLE, &d); CHECK(d == 1);
	set(RS_FOGTYPE, (void *)2); CHECK(get(RS_FOGTYPE, &v) && v == 2); RwD3D9GetRenderState_p(D3DRS_FOGTABLEMODE, &d); CHECK(d == 1 /* D3DFOG_EXP */);
	set(RS_TEXTURERASTER, NULL); CHECK(get(RS_TEXTURERASTER, &v) && v == 0);
	CHECK(RwD3D9SetTexture_p(NULL, 0));
	/* transforms are cached and read back */
	float m[16] = { 1,0,0,0, 0,2,0,0, 0,0,3,0, 4,5,6,1 }, o[16];
	CHECK(RwD3D9SetTransform_p(2 /* VIEW */, m)); RwD3D9GetTransform_p(2, o); CHECK(!memcmp(m, o, 64));
	CHECK(RwD3D9SetTransform_p(256 /* WORLD */, m)); memset(o, 0, 64); RwD3D9GetTransform_p(256, o); CHECK(!memcmp(m, o, 64));
	CHECK(rwD3D9RenderStateFlushCache_p());
	/* an Im2D triangle outside a camera update must not crash (the draw itself fails) */
	{ float vt[3][7] = { { 10,10,0,1, 0,0,0 }, { 100,10,0,1, 0,0,0 }, { 10,100,0,1, 0,0,0 } }; ((int (*)(void *, int, int, int, int))*(void **)(shadow + 0x2c))(vt, 3, 0, 1, 2); }
	printf("RESULT: %s\n", fails ? "FAILED" : "PASS");
	return fails ? 1 : 0;
}
