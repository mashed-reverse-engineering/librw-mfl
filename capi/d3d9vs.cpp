// D3D9VS (group d3d9vs, lane K3 window 7): the vertex shader cache management of MFL.exe (rpworld d3d9vertexshader), written op for op from the disassembly, on the
// exe's own VertexShaderCache (0x7d54e0, 256 entries of 0x14 bytes {key, frameStamp, shader, flag0, flag1}), count (NumShadersInCache 0x7d79e0), the last-used entry
// pointer (LastVertexShaderDesc 0x7d79e4), the version / constant globals set by CacheOpen, the device caps (_RwD3D9DeviceCaps 0x947600) and these callees:
//   _rpD3D9VertexShaderCachePurge 0x509620, rpD3D9GetVertexShader 0x509690, rxD3D9VertexShaderDefaultGetMaterialShaderCallBack 0x50b5f0, _rpD3D9VertexShaderCacheOpen 0x50b4e0
// rpD3D9GenerateVertexShader 0x509870 (the shader generator) is NOT ported here (K3b, deferred to W8); the cache calls it by its exe address through the bind table.
// Not here: rxD3D9VertexShaderDefaultMeshRenderCallBack 0x50b610 and LightingCallBack 0x50b750 (render callbacks; deferred, stay native-only original).
// Exe binding: mflrw_d3d9vs_bind(table), table order = enum V below.  Unbound: abort.  Check: tools/delink/rw_d3d9vs_check.sh.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

enum V { V_CACHE, V_NUMEFFECT, V_COUNT, V_LASTHEAD, V_USEVER, V_MAXCONST, V_ENGINE, V_CAPS, V_GENERATE, V_DELETESHADER, V_MEMMOVE, V_N };
static void *g_v[V_N];
static void *fn(int i){
	if(!g_v[V_N - 1]){ fprintf(stderr, "[mflrw] d3d9vs called before mflrw_d3d9vs_bind\n"); abort(); }
	return g_v[i];
}
#define VAR(i) (*(u32 *)fn(i))
#define P(p) ((u32)(uintptr_t)(p))
struct Entry { u32 key, frameStamp, shader, flag0, flag1; };			// 0x14 bytes
static Entry *cache(void){ return (Entry *)fn(V_CACHE); }
static u32 *numEffect(void){ return (u32 *)fn(V_NUMEFFECT); }			// D3D9NumConstantsEffect 0x7d78e0: 64 dwords
static u32 *caps(void){ return (u32 *)fn(V_CAPS); }				// _RwD3D9DeviceCaps 0x947600 (+0xc4 VS version, +0xc8 MaxVSConst, +0xec, +0xfc)
typedef u32 (__cdecl *GenFn)(const void *desc, void *flags);			// rpD3D9GenerateVertexShader
typedef void (__cdecl *DelFn)(u32 shader);					// RwD3D9DeletePixelShader 0x4dbc40
typedef void *(__cdecl *MoveFn)(void *dst, const void *src, u32 n);		// _memmove 0x4bdba0
static u16 frameWord(void){ return *(u16 *)(uintptr_t)(VAR(V_ENGINE) + 8); }	// engine global 0x7d2128 -> word at +8

extern "C" {

void __cdecl mflrw_d3d9vs_bind(void *const *tbl){
	for(int i = 0; i < V_N; i++) g_v[i] = tbl[i];
}

// _rpD3D9VertexShaderCachePurge 0x509620 (): frees every cached shader, resets the cache to one entry of all 0xffffffff and count 0, LastHead -> cache[0].
void __cdecl rw37__rpD3D9VertexShaderCachePurge(void){
	i32 n = (i32)VAR(V_COUNT);
	Entry *c = cache();
	for(i32 i = 0; i < n; i++){						// loop over the shader slot (cache[i].shader = 0x7d54e8 + i*0x14)
		if(c[i].shader){ ((DelFn)fn(V_DELETESHADER))(c[i].shader); c[i].shader = 0; }
		n = (i32)VAR(V_COUNT);						// the original re-reads the count each iteration
	}
	VAR(V_COUNT) = 0;
	VAR(V_LASTHEAD) = P(c);
	c[0].key = 0xffffffffu; c[0].frameStamp = 0xffffffffu; c[0].shader = 0xffffffffu; c[0].flag0 = 0xffffffffu; c[0].flag1 = 0xffffffffu;
}

// _rpD3D9VertexShaderCacheOpen 0x50b4e0 (): resets the cache, zeroes the 64-dword constants-per-effect table (then presets a few entries), and from the VS version /
// max-constant device caps sets D3D9UseVersion (0x7d79e8) and _rwD3D9VertexShaderMaxConstants (0x7d79ec).  Writes caps[0xec] back for the 1.x case.
void __cdecl rw37__rpD3D9VertexShaderCacheOpen(void){
	Entry *c = cache();
	c[0].key = 0xffffffffu; c[0].frameStamp = 0xffffffffu; c[0].shader = 0xffffffffu; c[0].flag0 = 0xffffffffu; c[0].flag1 = 0xffffffffu;
	u32 *t = numEffect();
	for(int i = 0; i < 0x40; i++) t[i] = 0;					// rep stos 0x40 dwords at 0x7d78e0
	VAR(V_COUNT) = 0;
	t[1] = 2; t[2] = 2; t[6] = 2; t[7] = 2; t[9] = 2;			// 0x7d78e4 / e8 / f8 / fc / 7904
	u32 ver = caps()[0x31] & 0xffff;					// 0x947600 + 0xc4 = index 0x31: VS version low word
	VAR(V_LASTHEAD) = P(c);
	t[3] = 4; t[10] = 4; t[11] = 6;						// 0x7d78ec / 7908 / 790c
	if(ver >= 0x300){
		VAR(V_USEVER) = 0x19;
	}else if(ver >= 0x200){
		i32 vp = (i32)caps()[0x3f];					// 0x947600 + 0xfc; signed compare (setle)
		VAR(V_USEVER) = ((vp <= 0 ? 0u : ~0u) & 5) + 0x14;		// vp<=0 -> 0x14, else 0x19
	}else if(ver >= 0x100){
		u32 cv = caps()[0x3b];						// 0x947600 + 0xec
		VAR(V_USEVER) = 0xa;
		caps()[0x3b] = cv & 0xffffffc1u;				// and al,0xc1: low byte keeps bits 0/6/7, upper 3 bytes kept
	}else{
		VAR(V_USEVER) = 0;
		return;								// ver < 0x100: no MaxConstants write
	}
	u32 mc = caps()[0x32];							// 0x947600 + 0xc8: MaxVSConst (the >=0x100 / >=0x200 / >=0x300 paths all fall through to here)
	VAR(V_MAXCONST) = mc < 0x100 ? mc : 0x100;
}

// rpD3D9GetVertexShader 0x509690 (desc, flags): returns a vertex shader for desc[0] (the sort key), creating one through the generator and inserting it into the sorted
// cache on a miss.  Fast path: the last-used entry's key matches.  On reuse / found, the cached flags are written to *flags; on insert, *flags (2 dwords) are stored
// in the new entry.  frameStamp = the engine's frame word.  Evicts the oldest entry when the cache is full (256).
u32 __cdecl rw37_rpD3D9GetVertexShader(const u32 *desc, u32 *flags){
	Entry *c = cache();
	u32 key = desc[0];
	Entry *last = (Entry *)(uintptr_t)VAR(V_LASTHEAD);
	Entry *e;
	if(last->key == key){ e = last; }					// fast path: reuse the last entry
	else{
		// the original's hand-rolled binary search (0x5096a6..0x509758), probe for probe: it must stay in step even once eviction has broken the sort order.
		// lo = edi, hi = ebx, range = ecx; mid = (range & 1) + lo + range / 2 - 1; a 1-element range is decided separately (0x50973f).
		i32 n = (i32)VAR(V_COUNT), lo = 0, found = -1;
		if(n > 0){
			i32 hi = n - 1, range = n;
			for(;;){
				if(range <= 1){						// 0x50973f: single element at lo
					u32 k = c[lo].key;
					if(key == k) found = lo;
					else if(!(key < k)) lo = lo + 1;		// key > k: insert after it (esi = -2); key < k: insert at lo
					break;
				}
				i32 mid = (range & 1) + lo + range / 2 - 1;
				u32 k = c[mid].key;
				if(key == k){ found = mid; break; }
				if(key >= k) lo = mid + 1; else hi = mid - 1;		// jae (unsigned)
				range = hi - lo + 1;
				if(range <= 0) break;					// jg not taken: not found, insert at lo
			}
		}
		if(found >= 0){ e = &c[found]; VAR(V_LASTHEAD) = P(e); }
		else{
			u32 shader = ((GenFn)fn(V_GENERATE))(desc, flags);
			n = (i32)VAR(V_COUNT);
			if(n >= 0x100){						// cache full: evict the entry with the smallest frameStamp
				Entry *best = &c[0];
				for(i32 i = 1; i < n; i++) if(c[i].frameStamp >= best->frameStamp) {} else best = &c[i];
				e = best;
				e->key = key; e->frameStamp = frameWord();
				if(e->shader) ((DelFn)fn(V_DELETESHADER))(e->shader);
				e->shader = shader; e->flag0 = flags[0]; e->flag1 = flags[1];
				VAR(V_LASTHEAD) = P(e);
				return shader;
			}
			if(lo < n) ((MoveFn)fn(V_MEMMOVE))(&c[lo + 1], &c[lo], (u32)(n - lo) * 0x14);
			e = &c[lo];
			VAR(V_LASTHEAD) = P(e);
			e->key = key; e->frameStamp = frameWord();
			e->shader = shader; e->flag0 = flags[0]; e->flag1 = flags[1];
			VAR(V_COUNT) = (u32)(n + 1);
			return shader;
		}
	}
	e->frameStamp = frameWord();
	flags[0] = e->flag0; flags[1] = e->flag1;
	return e->shader;
}

// rxD3D9VertexShaderDefaultGetMaterialShaderCallBack 0x50b5f0 (self, desc, flags): a thin wrapper that calls rpD3D9GetVertexShader(desc, flags).
u32 __cdecl rw37_rxD3D9VertexShaderDefaultGetMaterialShaderCallBack(u32 self, const u32 *desc, u32 *flags){
	(void)self;
	return rw37_rpD3D9GetVertexShader(desc, flags);
}

}
