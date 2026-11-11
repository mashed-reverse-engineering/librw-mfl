// D3D9SYS (group d3d9sys, lane K window 5 batch 6): D3D9System 0x4d7c00, the device system request dispatcher of MFL.exe's D3D9 driver (rwcore d3d9device bucket, 3040 B incl. its
// jump tables), written op for op from the disassembly, on the exe's own globals and its IDirect3D9 / IDirect3DDevice9 objects.
// Exe binding: mflrw_d3d9sys_bind(&RwEngineInstance, table), table order = enum E below.  Unbound: abort.  Check: tools/delink/rw_d3d9sys_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum E { E_SYSSTARTED, E_NUMDISPMODES, E_DISPMODES, E_CURMODE, E_MODE, E_DEPTH, E_MODEFLAGS,
         E_MAXMS, E_SELMS, E_MAXMSNM, E_SELMSNM, E_D3DOBJ, E_ADAPTERTYPE, E_ADAPTER, E_CREATEMODES,
         E_DEVICE, E_FLAG947760, E_WINDOWHANDLE, E_SHOWWINDOW, E_REGTABLE, E_G9473C4,
         E_D3DCREATE, E_ADAPTERINFO, E_STR62E830, E_CAPS, E_MAXMODES, E_BBFORMATS, E_DESKTOP, E_CALCMS,
         E_HAVEMMX, E_HAVESSE, E_HAVESSE2, E_HAVE3DNOW, E_MMX, E_SSE, E_SSE2, E_3DNOW,
         E_PRESENT, E_SETPP, E_G7D226C, E_G7D2264, E_G7D2268, E_GETCLIENTRECT, E_RENDERSURF, E_DEPTHSURF, E_CURDEPTH, E_CURSURF,
         E_CLEARSHADERS, E_CLEARMATRIX, E_LIGHTSCACHE, E_MAXLIGHTS, E_RASTEROPEN, E_IM2DOPEN, E_RSOPEN, E_VBMOPEN,
         E_SYSSTOP, E_STANDARDS, E_FPINIT, E_MATMULT, E_SETMATMULT, E_SETVECMULT, E_VECFNS,
         E_N };
static u8 **g_engine;
static void *g_m[E_N];
static u8 *eng(void){
	if(!g_engine){ fprintf(stderr, "[mflrw] d3d9sys called before mflrw_d3d9sys_bind\n"); abort(); }
	return *g_engine;
}
#define GU(i) ((u32 *)g_m[i])
#define VT(obj, off, Fn) (*(Fn *)((u8 *)*(void **)(obj) + (off)))
#define ENGFN(off, Fn) (*(Fn *)(eng() + (off)))
typedef void (__cdecl *V0)(void);
typedef u32 (__cdecl *F0)(void);
typedef u32 (__cdecl *F3)(u32, u32, u32);
typedef void (__cdecl *V1)(void *);
typedef void (__cdecl *V4)(void *, void *, void *, void *);
typedef void (__cdecl *StrCpyFn)(void *dst, const void *src);
typedef void (__cdecl *FreeFn)(void *p);
typedef void *(__stdcall *D3DCreateFn)(u32 version);
typedef int (__stdcall *ShowWindowFn)(u32 hwnd, u32 cmd);
typedef int (__stdcall *GetClientRectFn)(u32 hwnd, int32_t *rc);
typedef u32 (__stdcall *ObjU32Fn)(void *obj);
typedef long (__stdcall *GetIdentFn)(void *d3d, u32 adapter, u32 flags, void *ident);
typedef u32 (__stdcall *ModeCountFn)(void *d3d, u32 adapter, u32 fmt);
typedef long (__stdcall *GetModeFn)(void *d3d, u32 adapter, void *mode);
typedef long (__stdcall *CheckMSFn)(void *d3d, u32 adapter, u32 type, u32 fmt, u32 windowed, u32 ms, u32 *quality);
typedef long (__stdcall *GetCapsFn)(void *d3d, u32 adapter, u32 type, void *caps);
typedef long (__stdcall *CreateDevFn)(void *d3d, u32 adapter, u32 type, u32 hwnd, u32 flags, void *pp, void *dev);
typedef long (__stdcall *DialogFn)(void *dev, u32 enable);
typedef long (__stdcall *GetRTFn)(void *dev, u32 i, void *pp);
typedef long (__stdcall *GetDSFn)(void *dev, void *pp);

// depth of a D3DFORMAT (the original's byte tables 0x4d8764 / 0x4d8780 / 0x4d87c4): 21 22 35 -> 32, 23 24 25 -> 16, else 0
static u32 fmtDepth(u32 f){
	u32 i = f - 0x15;
	if(i > 0xe) return 0;
	if(i <= 1 || i == 0xe) return 0x20;
	if(i <= 4) return 0x10;
	return 0;
}
// RenderWare raster format of a D3DFORMAT (byte table 0x4d87a8): 21 35 -> 8888, 22 -> 888, 23 -> 565, 24 -> 555, 25 -> 1555, else 0
static u32 fmtRw(u32 f){
	switch(f){
	case 0x15: case 0x23: return 0x500;
	case 0x16: return 0x600;
	case 0x17: return 0x200;
	case 0x18: return 0xa00;
	case 0x19: return 0x100;
	default: return 0;
	}
}
// multisampling levels of the current mode format: maskable types 2..16, then the quality count of NONMASKABLE (+1); inlined twice in the original (requests 7 and 16)
static void msLevels(u32 windowed){
	u32 l;
	*GU(E_MAXMS) = 1; *GU(E_SELMS) = 1; *GU(E_MAXMSNM) = 1; *GU(E_SELMSNM) = 1;
	for(l = 2; l <= 0x10; l++){
		void *d3d = *(void **)g_m[E_D3DOBJ];
		if(VT(d3d, 0x2c, CheckMSFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), GU(E_MODE)[3], windowed, l, 0) >= 0) *GU(E_MAXMS) = l;
	}
	void *d3d = *(void **)g_m[E_D3DOBJ];
	if(VT(d3d, 0x2c, CheckMSFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), GU(E_MODE)[3], windowed, 1, &l) >= 0){
		l++;
		*GU(E_MAXMSNM) = l;
		if(l > *GU(E_MAXMS)) *GU(E_MAXMS) = l;
	}
}
// sum of the adapter mode counts of the 3 back buffer formats, then the adapter's display mode into the current mode block
static void adapterModes(void){
	*GU(E_MAXMODES) = 0;
	for(u32 i = 0; i < 3; i++){
		void *d3d = *(void **)g_m[E_D3DOBJ];
		u32 n = VT(d3d, 0x18, ModeCountFn)(d3d, *GU(E_ADAPTER), GU(E_BBFORMATS)[i]);
		*GU(E_MAXMODES) += n;
	}
	void *d3d = *(void **)g_m[E_D3DOBJ];
	VT(d3d, 0x20, GetModeFn)(d3d, *GU(E_ADAPTER), g_m[E_MODE]);
}

extern "C" {

void __cdecl mflrw_d3d9sys_bind(u8 **engine, void *const *tbl){
	g_engine = engine;
	for(int i = 0; i < E_N; i++) g_m[i] = tbl[i];
}

// D3D9System 0x4d7c00 (request, out, inOut, numIn): 0 open (Direct3DCreate9, first adapter with HAL caps, mode counts, desktop mode, CPU flags), 1 close, 2 start
// (present parameters, caps, ATI 0x1002:0x5157 quirk, behaviour flags, CreateDevice, back buffer surfaces, caches, driver modules open), 3 stop, 4 register (copies the
// 14 word device block), 5 mode count, 6 mode info, 7 use mode (+ multisampling levels), 8 focus (ShowWindow), 10 current mode, 11 standards, 12 texture memory,
// 13 adapter count, 14 adapter name, 15 current adapter, 16 set adapter, 17 finalize start (x87 / SSE setup, matrix and vector functions), 19 max texture size,
// 22 writes the 16 bit value 2.  Everything else returns 0.
int32_t __cdecl rw37_D3D9System(u32 request, void *out, void *inOut, int32_t numIn){
	u8 *e = eng();
	void *d3d, *dev;
	u32 *m;
	switch(request){
	case 0: {
		*GU(E_WINDOWHANDLE) = *(u32 *)inOut;
		d3d = ((D3DCreateFn)g_m[E_D3DCREATE])(0x1f);
		*(void **)g_m[E_D3DOBJ] = d3d;
		if(!d3d) return 0;
		ENGFN(0xcc, StrCpyFn)(g_m[E_ADAPTERINFO], g_m[E_STR62E830]);
		d3d = *(void **)g_m[E_D3DOBJ];
		u32 n = VT(d3d, 0x10, ObjU32Fn)(d3d), a = 0;
		int found = 0;
		*GU(E_ADAPTER) = 0;
		*GU(E_ADAPTERTYPE) = 1;
		if(n > 0){
			for(;;){
				d3d = *(void **)g_m[E_D3DOBJ];
				long hr = VT(d3d, 0x38, GetCapsFn)(d3d, a, *GU(E_ADAPTERTYPE), g_m[E_CAPS]);
				a = *GU(E_ADAPTER);
				if(hr >= 0){ found = a < n; break; }
				a++;
				*GU(E_ADAPTER) = a;
				if(a >= n) break;
			}
		}
		if(!found){
			d3d = *(void **)g_m[E_D3DOBJ];
			VT(d3d, 8, ObjU32Fn)(d3d);
			*GU(E_D3DOBJ) = 0;
			return 0;
		}
		adapterModes();
		m = GU(E_MODE);
		u32 fmt = m[3];
		*GU(E_DEPTH) = fmtDepth(fmt);
		*GU(E_MODEFLAGS) = 0;
		u32 *desk = GU(E_DESKTOP);
		desk[0] = m[0]; desk[1] = m[1]; desk[2] = m[2]; desk[3] = fmt;
		*GU(E_CURMODE) = 0;
		((V0)g_m[E_CALCMS])();
		*GU(E_MMX) = ((F0)g_m[E_HAVEMMX])();
		*GU(E_SSE) = ((F0)g_m[E_HAVESSE])();
		*GU(E_SSE2) = ((F0)g_m[E_HAVESSE2])();
		*GU(E_3DNOW) = ((F0)g_m[E_HAVE3DNOW])();
		return 1; }
	case 1: {
		void *dm = *(void **)g_m[E_DISPMODES];
		if(dm){
			ENGFN(0x10c, FreeFn)(dm);
			*GU(E_DISPMODES) = 0;
			*GU(E_NUMDISPMODES) = 0;
		}
		d3d = *(void **)g_m[E_D3DOBJ];
		if(d3d){
			VT(d3d, 8, ObjU32Fn)(d3d);
			*GU(E_D3DOBJ) = 0;
		}
		return 1; }
	case 2: {
		u32 mode[4];
		u8 ident[0x44c];
		int32_t rc[4];
		d3d = *(void **)g_m[E_D3DOBJ];
		VT(d3d, 0x20, GetModeFn)(d3d, *GU(E_ADAPTER), mode);
		u32 *pp = GU(E_PRESENT);
		memset(pp, 0, 14 * 4);
		((F3)g_m[E_SETPP])((u32)(uintptr_t)g_m[E_MODE], *GU(E_MODEFLAGS), mode[3]);
		pp[7] = *GU(E_WINDOWHANDLE);
		pp[11] = *GU(E_G7D226C) != 0;
		u8 *caps = (u8 *)g_m[E_CAPS];
		d3d = *(void **)g_m[E_D3DOBJ];
		VT(d3d, 0x38, GetCapsFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), caps);
		d3d = *(void **)g_m[E_D3DOBJ];
		VT(d3d, 0x14, GetIdentFn)(d3d, *GU(E_ADAPTER), 0, ident);
		if(*(u32 *)(ident + 0x428) == 0x1002 && *(u32 *)(ident + 0x42c) == 0x5157)
			*(u32 *)(caps + 0xd4) &= ~1u;
		u32 flags = *GU(E_G7D2264) ? 4 : 0;
		u32 devcaps = *(u32 *)(caps + 0x1c);
		if(*GU(E_G7D2268)){
			flags |= 0x20;
			*(u32 *)(caps + 0x1c) = devcaps & 0xfffeffff;
			*(u32 *)(caps + 0xec) = 0;
		}else if(devcaps & 0x10000){
			if(devcaps & 0x100000) flags |= 0x50;
			else flags |= 0x40;
		}else flags |= 0x20;
		d3d = *(void **)g_m[E_D3DOBJ];
		if(VT(d3d, 0x40, CreateDevFn)(d3d, *GU(E_ADAPTER), *GU(E_ADAPTERTYPE), *GU(E_WINDOWHANDLE), flags, pp, g_m[E_DEVICE]) < 0){
			*GU(E_SYSSTARTED) = 0;
			return 0;
		}
		if(*GU(E_G7D226C)){
			dev = *(void **)g_m[E_DEVICE];
			VT(dev, 0x50, DialogFn)(dev, 1);
		}
		u32 hwnd = *GU(E_WINDOWHANDLE);
		if(hwnd){
			(*(GetClientRectFn *)g_m[E_GETCLIENTRECT])(hwnd, rc);
			m = GU(E_MODE);
			m[0] = (u32)rc[2];
			m[1] = (u32)rc[3];
		}
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0x98, GetRTFn)(dev, 0, g_m[E_RENDERSURF]);
		void *s = *(void **)g_m[E_RENDERSURF];
		VT(s, 8, ObjU32Fn)(s);
		dev = *(void **)g_m[E_DEVICE];
		VT(dev, 0xa0, GetDSFn)(dev, g_m[E_DEPTHSURF]);
		s = *(void **)g_m[E_DEPTHSURF];
		VT(s, 8, ObjU32Fn)(s);
		*GU(E_CURDEPTH) = 0;
		u32 *cs = GU(E_CURSURF);
		cs[0] = 0; cs[1] = 0; cs[2] = 0; cs[3] = 0;
		((V0)g_m[E_CLEARSHADERS])();
		((V0)g_m[E_CLEARMATRIX])();
		void *lc = *(void **)g_m[E_LIGHTSCACHE];
		*GU(E_MAXLIGHTS) = 0;
		if(lc){
			ENGFN(0x10c, FreeFn)(lc);
			*GU(E_LIGHTSCACHE) = 0;
		}
		((V0)g_m[E_RASTEROPEN])();
		((V0)g_m[E_IM2DOPEN])();
		((V0)g_m[E_RSOPEN])();
		((V0)g_m[E_VBMOPEN])();
		*GU(E_SYSSTARTED) = 1;
		return 1; }
	case 3:
		return (int32_t)((F3)g_m[E_SYSSTOP])((u32)(uintptr_t)out, (u32)(uintptr_t)inOut, (u32)numIn);
	case 4:
		memcpy(out, g_m[E_REGTABLE], 14 * 4);
		*GU(E_G9473C4) = (u32)(uintptr_t)inOut;
		return 1;
	case 5:
		if(!*GU(E_DISPMODES)) ((V0)g_m[E_CREATEMODES])();
		*(u32 *)out = *GU(E_NUMDISPMODES);
		return 1;
	case 6: {
		if(!*GU(E_DISPMODES)) ((V0)g_m[E_CREATEMODES])();
		if(numIn < 0 || numIn >= *(int32_t *)g_m[E_NUMDISPMODES]) return 0;
		m = *(u32 **)g_m[E_DISPMODES] + 5 * numIn;
		u32 *o = (u32 *)out;
		o[0] = m[0];
		o[1] = m[1];
		o[2] = fmtDepth(m[3]);
		o[3] = m[4];
		o[4] = m[2];
		o[5] = fmtRw(m[3]);
		return 1; }
	case 7: {
		if(*GU(E_SYSSTARTED)) return 0;
		if(numIn < 0 || numIn >= *(int32_t *)g_m[E_NUMDISPMODES]) return 0;
		*GU(E_CURMODE) = (u32)numIn;
		m = *(u32 **)g_m[E_DISPMODES] + 5 * numIn;
		u32 *cur = GU(E_MODE);
		cur[0] = m[0]; cur[1] = m[1]; cur[2] = m[2]; cur[3] = m[3];
		u32 fl = m[4];
		*GU(E_MODEFLAGS) = fl;
		*GU(E_DEPTH) = fmtDepth(m[3]);
		msLevels(~fl & 1);
		return 1; }
	case 8:
		if(!*GU(E_DEVICE)) return 0;
		if(*GU(E_FLAG947760)) return 1;
		(*(ShowWindowFn *)g_m[E_SHOWWINDOW])(*GU(E_WINDOWHANDLE), numIn ? 9 : 0);
		return 1;
	case 10:
		*(u32 *)out = *GU(E_CURMODE);
		return 1;
	case 11:
		return (int32_t)((F3)g_m[E_STANDARDS])((u32)(uintptr_t)out, (u32)(uintptr_t)inOut, (u32)numIn);
	case 12:
		dev = *(void **)g_m[E_DEVICE];
		*(u32 *)out = VT(dev, 0x10, ObjU32Fn)(dev);
		return 1;
	case 13:
		d3d = *(void **)g_m[E_D3DOBJ];
		*(u32 *)out = VT(d3d, 0x10, ObjU32Fn)(d3d);
		return 1;
	case 14: {
		u8 ident[0x44c];
		d3d = *(void **)g_m[E_D3DOBJ];
		VT(d3d, 0x14, GetIdentFn)(d3d, (u32)numIn, 0, ident);
		ENGFN(0xcc, StrCpyFn)(out, ident + 0x200);
		return 1; }
	case 15:
		*(u32 *)out = *GU(E_ADAPTER);
		return 1;
	case 16:
		*GU(E_ADAPTER) = (u32)numIn;
		adapterModes();
		((V0)g_m[E_CREATEMODES])();
		msLevels(~*GU(E_MODEFLAGS) & 1);
		return 1;
	case 17: {
		((V0)g_m[E_FPINIT])();
		if(*GU(E_SSE)){
			u32 x;
			__asm__ volatile("stmxcsr %0" : "=m"(x));
			x |= 0x8000;
			__asm__ volatile("ldmxcsr %0" :: "m"(x));
		}
		((V1)g_m[E_SETMATMULT])(g_m[E_MATMULT]);
		void *const *v = (void *const *)g_m[E_VECFNS];      // 3 sets of {point, points, vector, vectors}: SSE P4, SSE, x86
		if(*GU(E_SSE)){
			if(*GU(E_SSE2) && !*GU(E_3DNOW)) ((V4)g_m[E_SETVECMULT])(v[0], v[1], v[2], v[3]);
			else ((V4)g_m[E_SETVECMULT])(v[4], v[5], v[6], v[7]);
		}else ((V4)g_m[E_SETVECMULT])(v[8], v[9], v[10], v[11]);
		return 1; }
	case 19: {
		u8 *caps = (u8 *)g_m[E_CAPS];
		u32 w = *(u32 *)(caps + 0x58), h = *(u32 *)(caps + 0x5c);
		*(u32 *)out = w >= h ? w : h;
		return 1; }
	case 22:
		*(u16 *)out = 2;
		return 1;
	default:
		return 0;
	}
}

}
