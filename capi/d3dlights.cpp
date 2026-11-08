// D3DLIGHTS (LIB_RW group d3dlights, D3 of plan_engine_switch_window2_2026-11-08): the RW 3.7 D3D9 world-lights module (d3d9lights.obj of rpworld,
// 0x505180..0x505c30 of MFL.exe) and the device light setters of d3d9device.obj (0x4db740..0x4db980), written op for op from the disassembly ("kernel on exe
// state": the two D3DLIGHT9 templates, the light index free list, the active / previous light index lists stay in the exe's bss 0x7d53d0..0x7d54d0, the light
// cache in MaxNumLights 0x7d269c / LightsCache 0x7d26a0, the ambient colour at 0x62e738).
//   rpD3D9LightDestructor 0x505180 (exe LAB_00505180), rpD3D9LightCopy 0x505270 (LAB_00505270), _rwD3D9LightsOpen 0x5052b0, _rwD3D9LightsClose 0x505510,
//   rwD3D9LightsGlobalEnable 0x5055a0, _rwD3D9LightDirectionalEnable 0x505780, _rwD3D9LightLocalEnable 0x5058b0, _rwD3D9LightsEnable 0x505b20,
//   RwD3D9SetLight 0x4db740, RwD3D9EnableLight 0x4db830, _rwD3D9LightsEqual 0x4db880.
// The device is the exe's (*0x7d2240): SetLight 51, LightEnable 53.  Arrays come from the exe's RW memory table (RwEngineInstance +0x108 malloc, +0x10c free,
// +0x110 realloc); no CRT heap.  Exe callees: RwD3D9GetCaps 0x4dbca0, RwFrameGetLTM 0x4d0ff0, RpLightGetConeAngle 0x4f4a70, RwD3D9SetRenderState 0x4e3c00.
// Exe-layout adapter: mflrw_d3dlights_bind(n, table) (rw_engine_init, recomp/automatch_near/an_hk_612.c), table layout = enum B below.
// Check: ghidra tools/delink/rw_d3dlights_check.sh (fake device tools/delink/fakedev.c; both sides run the same op sequences natively under Wine).
#include <stdint.h>
#include <string.h>
#include "rwmfl.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef long double X;

// bind table layout (an_hk_612.c builds it in this order)
enum B { B_DEVVAR, B_ENGINEVAR, B_LIGHTOFFSET, B_AMBIENT, B_NUMLIGHTS, B_CACHE, B_GETCAPS, B_GETLTM, B_CONEANGLE, B_SETRS, B_N };
static void *const *g;

typedef u32 __attribute__((may_alias)) UA;
// the module block, addressed from D3D9LightOffset 0x7d54a0 (the only symbol of the block in the overlay)
#define A(va) (*(UA *)((u8 *)g[B_LIGHTOFFSET] + ((va) - 0x7d54a0u)))
#define LOCAL 0x7d53d0u		// D3DLIGHT9 template for point / spot lights
#define DIR 0x7d5438u		// D3DLIGHT9 template for directional lights (Type 3)
#define LIGHTOFF A(0x7d54a0)	// light plugin offset of the {index, attenuation[3]} extension
#define MAXLIGHTS A(0x7d54a4)	// D3DCAPS9.MaxActiveLights, -1 if 0
#define FREEN A(0x7d54a8)	// free light indices: count, capacity, array; next new index
#define FREECAP A(0x7d54ac)
#define NEXTIDX A(0x7d54b0)
#define FREEARR A(0x7d54b4)
#define ACT A(0x7d54b8)		// light indices enabled this frame: array, count, capacity
#define ACTN A(0x7d54bc)
#define ACTCAP A(0x7d54c0)
#define PREV A(0x7d54c4)	// last frame's: array, count, capacity
#define PREVN A(0x7d54c8)
#define PREVCAP A(0x7d54cc)
#define AMB(i) (((UA *)g[B_AMBIENT])[i])
#define NUMLIGHTS (*(i32 *)g[B_NUMLIGHTS])
#define CACHE (*(u8 **)g[B_CACHE])	// MaxNumLights entries of {D3DLIGHT9 (0x68), enabled}
#define W(p, off) (*(UA *)((u8 *)(p) + (off)))
enum { HINT_CACHE = 0x1030411, HINT_LIGHTS = 0x103050d };

static inline u8 *DEV(void){ return *(u8 **)g[B_DEVVAR]; }
static inline void **VT(void *o){ return *(void ***)o; }
static inline i32 devSetLight(u32 i, const void *l){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, const void *))VT(d)[51])(d, i, l); }
static inline i32 devLightEnable(u32 i, i32 e){ u8 *d = DEV(); return ((i32 (__stdcall *)(void *, u32, i32))VT(d)[53])(d, i, e); }
static inline u8 *ENG(void){ return *(u8 **)g[B_ENGINEVAR]; }
static inline u32 memAlloc(u32 size, u32 hint){ return (u32)(uintptr_t)(*(void *(__cdecl **)(u32, u32))(ENG() + 0x108))(size, hint); }
static inline void memFree(u32 p){ (*(void (__cdecl **)(u32))(ENG() + 0x10c))(p); }
static inline u32 memRealloc(u32 p, u32 size, u32 hint){ return (u32)(uintptr_t)(*(void *(__cdecl **)(u32, u32, u32))(ENG() + 0x110))(p, size, hint); }
static inline u8 *getLTM(const u8 *light){ return ((u8 *(__cdecl *)(u32))g[B_GETLTM])(W(light, 4)); }	// RwFrameGetLTM(RpLightGetFrame(light))
static inline X coneAngle(u8 *light){ return ((X (__cdecl *)(u8 *))g[B_CONEANGLE])(light); }	// st0 taken as it comes (no rounding)
static inline u32 setRS(u32 s, u32 v){ return ((u32 (__cdecl *)(u32, u32))g[B_SETRS])(s, v); }

// a value through the x87 stack (fld / fstp: an sNaN comes out quiet, as in the original's fst)
static inline u32 x87w(u32 w){ u32 r; __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(w)); return r; }
// fld a / fadd b / fstp, with b as the memory operand (NaN operand choice as the original)
static inline u32 faddm(u32 a, u32 b){ u32 r; __asm__ volatile("flds %1\n\tfadds %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b)); return r; }

extern "C" {

i32 __cdecl mflrw_d3dlights_bind(i32 n, void *const *table){
	if (n != B_N) return 0;
	g = table;
	return 1;
}

// _rwD3D9LightsEqual 0x4db880: type and diffuse r, g, b, then per type: point position + range, spot position, direction, range, theta, phi,
// directional direction; any other type compares equal
i32 __cdecl rw37__rwD3D9LightsEqual(const u32 *a, const u32 *b){
	for (int i = 0; i < 4; i++) if (a[i] != b[i]) return 0;
	static const u8 point[] = { 0x34, 0x38, 0x3c, 0x4c, 0 }, spot[] = { 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x60, 0x64, 0 }, dir[] = { 0x40, 0x44, 0x48, 0 };
	const u8 *f = a[0] == 1 ? point : a[0] == 2 ? spot : a[0] == 3 ? dir : 0;
	if (f) for (; *f; f++) if (W(a, *f) != W(b, *f)) return 0;
	return 1;
}

// RwD3D9SetLight 0x4db740: the cache grows to index + 1 entries (new ones disabled); an unchanged light is not sent again
i32 __cdecl rw37_RwD3D9SetLight(i32 index, const u32 *light){
	i32 old = NUMLIGHTS;
	if (index >= old) {
		NUMLIGHTS = index + 1;
		u32 sz = (u32)(index + 1) * 0x6c;
		CACHE = (u8 *)(uintptr_t)(CACHE ? memRealloc((u32)(uintptr_t)CACHE, sz, HINT_CACHE) : memAlloc(sz, HINT_CACHE));
		for (i32 i = old; i < NUMLIGHTS; i++) W(CACHE, i * 0x6c + 0x68) = 0;
	} else if (rw37__rwD3D9LightsEqual(light, (const u32 *)(CACHE + index * 0x6c))) return 1;
	memcpy(CACHE + index * 0x6c, light, 0x68);
	return devSetLight((u32)index, light) >= 0;
}

// RwD3D9EnableLight 0x4db830: only known lights, only on a change
i32 __cdecl rw37_RwD3D9EnableLight(i32 index, i32 enable){
	if (index < NUMLIGHTS) {
		UA *e = &W(CACHE, index * 0x6c + 0x68);
		if ((i32)*e != enable) { *e = (u32)enable; return devLightEnable((u32)index, enable) >= 0; }
	}
	return 1;
}

// rpD3D9LightDestructor 0x505180 (plugin destructor): the light's index goes back to the free list; when every index handed out is free, the list is dropped
u8 *__cdecl rw37_rpD3D9LightDestructor(u8 *light, i32 offset, i32 size){
	if (light) {
		UA *ext = &W(light, LIGHTOFF);
		if ((i32)*ext >= 0) {
			if ((i32)FREEN >= (i32)NEXTIDX - 1) {
				FREEN = 0; FREECAP = 0; NEXTIDX = 0;
				if (FREEARR) { memFree(FREEARR); FREEARR = 0; }
			} else {
				if ((i32)FREEN >= (i32)FREECAP) {
					FREECAP += 0x100;
					FREEARR = FREEARR ? memRealloc(FREEARR, FREECAP * 4, HINT_LIGHTS) : memAlloc(FREECAP * 4, HINT_LIGHTS);
				}
				W(FREEARR, FREEN * 4) = *ext;
				FREEN++;
			}
		}
	}
	return light;
}

// rpD3D9LightCopy 0x505270 (plugin copy): the 16-byte extension
u8 *__cdecl rw37_rpD3D9LightCopy(u8 *dst, const u8 *src, i32 offset, i32 size){
	if (src && dst) memcpy(dst + LIGHTOFF, src + LIGHTOFF, 16);
	return dst;
}

static void freeLists(void){
	if (FREEARR) { memFree(FREEARR); FREEARR = 0; }
	FREEN = 0; FREECAP = 0; NEXTIDX = 0;
	if (ACT) { memFree(ACT); ACT = 0; }
	ACTN = 0; ACTCAP = 0;
	if (PREV) { memFree(PREV); PREV = 0; }
	PREVN = 0; PREVCAP = 0;
}

// _rwD3D9LightsOpen 0x5052b0: lists dropped, MaxActiveLights from the caps, ambient black, the two templates reset
i32 __cdecl rw37__rwD3D9LightsOpen(void){
	freeLists();
	u32 m = W(((u8 *(__cdecl *)(void))g[B_GETCAPS])(), 0xa0);
	MAXLIGHTS = m ? m : 0xffffffffu;
	AMB(0) = 0; AMB(1) = 0; AMB(2) = 0; AMB(3) = 0x3f800000;
	for (u32 a = DIR; a < DIR + 0x68; a += 4) A(a) = 0;
	A(DIR) = 3; A(DIR + 0x10) = 0x3f800000; A(DIR + 0x20) = 0x3f800000; A(DIR + 0x30) = 0x3f800000;
	for (u32 a = LOCAL + 4; a < LOCAL + 0x34; a += 4) A(a) = 0;
	A(LOCAL + 0x10) = 0x3f800000; A(LOCAL + 0x20) = 0x3f800000; A(LOCAL + 0x30) = 0x3f800000; A(LOCAL + 0x50) = 0x3f800000;
	return 1;
}

// _rwD3D9LightsClose 0x505510
void __cdecl rw37__rwD3D9LightsClose(void){ freeLists(); }

// the light's index (from the free list, else a new one) and its D3DLIGHT9 sent; the index joins this frame's list (grown 8 at a time)
static inline UA *lightIndex(u8 *light){
	UA *ext = &W(light, LIGHTOFF);
	if ((i32)*ext < 0) {
		if (FREEN) { FREEN--; *ext = W(FREEARR, FREEN * 4); }
		else { *ext = NEXTIDX; NEXTIDX++; }
	}
	return ext;
}
static inline void activate(u32 idx){
	if (ACTN >= ACTCAP) {
		u32 old = ACTCAP;
		ACTCAP += 8;
		ACT = ACT ? memRealloc(ACT, ACTCAP * 4, HINT_LIGHTS) : memAlloc(ACTCAP * 4, HINT_LIGHTS);
		for (u32 i = old; i < ACTCAP; i++) W(ACT, i * 4) = 0;
	}
	W(ACT, ACTN * 4) = idx;
	ACTN++;
}

// _rwD3D9LightDirectionalEnable 0x505780
i32 __cdecl rw37__rwD3D9LightDirectionalEnable(u8 *light){
	UA *ext = lightIndex(light);
	A(DIR + 4) = W(light, 0x18); A(DIR + 8) = W(light, 0x1c); A(DIR + 0xc) = W(light, 0x20);
	u8 *ltm = getLTM(light);
	A(DIR + 0x40) = W(ltm, 0x20); A(DIR + 0x44) = W(ltm, 0x24); A(DIR + 0x48) = W(ltm, 0x28);	// the frame's at vector
	rw37_RwD3D9SetLight((i32)*ext, &A(DIR));
	activate(*ext);
	return 1;
}

// rwD3D9LightsGlobalEnable 0x5055a0: the current world's directional lights whose flags match are sent, the others add to the ambient colour;
// none at all: D3DRS_AMBIENT 0.  Returns 1 if any light matched.
i32 __cdecl rw37__rwD3D9LightsGlobalEnable(u32 flags){
	i32 any = 0;
	AMB(3) = 0x3f800000; AMB(0) = 0; AMB(1) = 0; AMB(2) = 0;
	u8 *world = *(u8 **)(ENG() + 4);
	u8 *head = world + 0x3c;
	for (u8 *n = *(u8 **)head; n != head; n = *(u8 **)n) {
		u8 *light = n - 0x34;
		if (!light || !((u8)flags & light[2])) continue;
		if (light[1] == 1) rw37__rwD3D9LightDirectionalEnable(light);
		else { AMB(0) = faddm(AMB(0), W(light, 0x18)); AMB(1) = faddm(AMB(1), W(light, 0x1c)); AMB(2) = faddm(AMB(2), W(light, 0x20)); }
		any = 1;
	}
	if (!any) setRS(0x8b, 0);
	return any;
}

// _rwD3D9LightLocalEnable 0x5058b0: point (0x80), spot (0x81), soft spot (0x82); radius 0 or another type: 0 (index, colour, position, range already set)
i32 __cdecl rw37__rwD3D9LightLocalEnable(u8 *light){
	UA *ext = lightIndex(light);
	A(LOCAL + 4) = W(light, 0x18); A(LOCAL + 8) = W(light, 0x1c); A(LOCAL + 0xc) = W(light, 0x20);
	u8 *ltm = getLTM(light);
	A(LOCAL + 0x34) = W(ltm, 0x30); A(LOCAL + 0x38) = W(ltm, 0x34); A(LOCAL + 0x3c) = W(ltm, 0x38);	// the frame's position
	u32 r = W(light, 0x14);
	A(LOCAL + 0x4c) = x87w(r);
	float rf; memcpy(&rf, &r, 4);
	if (rf == 0.0f) return 0;
	A(LOCAL + 0x54) = ext[1];
	{	// attenuation 1 / radius, 2 / radius^2 (x87 at the caller's precision)
		u32 a1, a2;
		__asm__ volatile("flds %2\n\tflds %3\n\tfdiv %%st(1), %%st\n\tfstps %0\n\tfld %%st(0)\n\tfmul %%st(1), %%st\n\tfdivrs %4\n\tfstps %1\n\tfstp %%st(0)"
		                 : "=m"(a1), "=m"(a2) : "m"(r), "m"(ext[2]), "m"(ext[3]));
		A(LOCAL + 0x58) = a1; A(LOCAL + 0x5c) = a2;
	}
	u32 type = light[1];
	if (type == 0x80) {
		A(LOCAL) = 1;
		A(LOCAL + 0x40) = 0; A(LOCAL + 0x44) = 0; A(LOCAL + 0x48) = 0;
		A(LOCAL + 0x60) = 0; A(LOCAL + 0x64) = 0;
	} else if (type == 0x81 || type == 0x82) {
		A(LOCAL) = 2;
		A(LOCAL + 0x40) = W(ltm, 0x20); A(LOCAL + 0x44) = W(ltm, 0x24); A(LOCAL + 0x48) = W(ltm, 0x28);
		float th, ph;
		if (type == 0x81) {
			X c = coneAngle(light); c = c + c;
			if (c < (X)3.14159274101257324219f) th = (float)c;		// 0x40490fdb
			else th = (float)(c - (X)0.00100000004749745131f);		// 0x3a83126f
			ph = (float)c;
		} else {
			A(LOCAL + 0x60) = 0;
			X c = coneAngle(light); c = c + c;
			ph = (float)c;
		}
		if (type == 0x81) memcpy(&A(LOCAL + 0x60), &th, 4);
		memcpy(&A(LOCAL + 0x64), &ph, 4);
	} else return 0;
	rw37_RwD3D9SetLight((i32)*ext, &A(LOCAL));
	activate(*ext);
	return 1;
}

// _rwD3D9LightsEnable 0x505b20: last frame's lights not in this frame's list are disabled, this frame's enabled (list cut to MaxActiveLights), the lists
// swap; then D3DRS_LIGHTING (tail call, its eax is the return).  enable 0: lighting off only.
u32 __cdecl rw37__rwD3D9LightsEnable(i32 enable){
	if (!enable) return setRS(0x89, 0);
	if (ACTN > MAXLIGHTS) ACTN = MAXLIGHTS;
	for (u32 i = 0; i < PREVN; i++) {
		u32 idx = W(PREV, i * 4), j;
		for (j = 0; j < ACTN && W(ACT, j * 4) != idx; j++);
		if (j >= ACTN) rw37_RwD3D9EnableLight((i32)idx, 0);
	}
	if (ACTN) {
		for (u32 i = 0; i < ACTN; i++) rw37_RwD3D9EnableLight((i32)W(ACT, i * 4), 1);
		u32 t = PREV; PREV = ACT; ACT = t;
		t = PREVCAP; PREVCAP = ACTCAP; ACTCAP = t;
		PREVN = ACTN;
	} else PREVN = 0;
	ACTN = 0;
	return setRS(0x89, 1);
}

}
