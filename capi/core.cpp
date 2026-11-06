// S2-c group 1: engine life cycle, memory entries, the RwEngineInstance shadow, RwStream.  Thin C-to-rw:: wrappers; RW 3.7 signatures from rwmfl.h.
// Tags: "L2:oracle" = compared against the original through the asset-parity oracle (output/librw_asset_parity_*.csv, tests/asset_dump_dll.cpp);
// "L2:impl" = implemented from the RW 3.7 semantics / the decompile, not compared against the original.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "rwmfl.h"
#include "fsys.h"
#include "../rw.h"
#include <windows.h>

using namespace rw;
static int g_tr; static int tr_on(void){ static int on = -1; if(on < 0){ const char *e = getenv("MFLRW_TRACE"); on = e && *e; }	/* opt-in debug trace to stderr (Wine: WINEPREFIX env must carry MFLRW_TRACE) */ return on && g_tr++ < 400000; }
#define TR(...) do{ if(tr_on()) fprintf(stderr, __VA_ARGS__); }while(0)

// ---- RwEngineInstance shadow: the RW 3.7 RwGlobals layout, the parts the game reads by offset (fields_used.csv): dOpenDevice 0x10 (zBufferNear 0x18,
// fpRenderStateSet 0x20), stringFuncs 0xc4 (strcpy +8, strcat +0x10, strcmp +0x24, stricmp +0x2c, strlen +0x30), memoryFuncs 0x108..0x114.
// fpRenderStateSet / Get and the Im2D entries (0x20..0x34) are filled by mflrw_shadow_device (capi/d3d9dev.cpp) at engine start; curCamera / curWorld mirror librw's engine (NULL until Camera / World are swapped).
struct RwGlobalsShadow { uint8_t raw[0x140]; };
static RwGlobalsShadow g_shadow;
extern "C" {
RwGlobalsShadow *RwEngineInstance = &g_shadow;
}
extern "C" void mflrw_shadow_device(uint8_t *raw);
extern "C" void mflrw_d3d9_prestart(void);
static int shadowInit(void);
static int g_shadowDone = shadowInit();

// ---- single heap (plan_librw_engine_switch v2 decision 3, L3a): the original runs RwMalloc = the exe's CRT heap (memFuncs = 0 -> RwEngineInit
// 0x4e96e0 fills engine+0x108 with 0x4e97b0 malloc / 0x4b45fd free / 0x4e97c0 realloc / calloc).  mflrw_mem_bind(RwOsGetMemoryInterface()) hands
// that table over once, before any DLL allocation; from then RwMalloc/RwFree/RwRealloc/RwCalloc, the shadow table (+0x108..+0x114) and librw's
// Engine::memfuncs all allocate on the exe heap.  Without the bind (test exes, older exe) they use the DLL's msvcrt heap as before.
// Foreign-free counter (log only): RwFree / RwRealloc of a pointer that did not come from RwMalloc/RwCalloc/RwRealloc is counted and the first 20
// are printed; behaviour is unchanged (the pointer is still freed).  Allocations that are not 8-byte aligned are counted the same way.
static void *__cdecl crtMalloc(size_t n, uint32_t){ return malloc(n); }
static void __cdecl crtFree(void *p){ free(p); }
static void *__cdecl crtRealloc(void *p, size_t n, uint32_t){ return realloc(p, n); }
static void *__cdecl crtCalloc(size_t n, size_t s, uint32_t){ return calloc(n, s); }
static void *(__cdecl *xMalloc)(size_t, uint32_t) = crtMalloc;
static void (__cdecl *xFree)(void *) = crtFree;
static void *(__cdecl *xRealloc)(void *, size_t, uint32_t) = crtRealloc;
static void *(__cdecl *xCalloc)(size_t, size_t, uint32_t) = crtCalloc;
static int g_heapBound;
static unsigned g_allocs, g_frees, g_foreign, g_misaligned;
// live set of DLL allocations: open addressing, linear probe, backward-shift delete; its own storage is the DLL's msvcrt heap
// ponytail: one spin lock around the set (RWA's thread allocates through the table too); per-bucket locks if it ever shows in a profile
static uintptr_t *g_set; static uint32_t g_cap, g_live;
static volatile int g_lock;
static void lock(void){ while(__sync_lock_test_and_set(&g_lock, 1)) while(g_lock) ; }
static void unlock(void){ __sync_lock_release(&g_lock); }
static uint32_t slot(uintptr_t p){ return (uint32_t)((p >> 3) * 2654435761u) & (g_cap - 1); }
static void setAdd(uintptr_t p){
	if(2 * (g_live + 1) > g_cap){
		uint32_t oc = g_cap; uintptr_t *os = g_set;
		g_cap = oc ? 2 * oc : 4096; g_set = (uintptr_t *)calloc(g_cap, sizeof *g_set); g_live = 0;
		for(uint32_t i = 0; i < oc; i++) if(os[i]) setAdd(os[i]);
		free(os);
	}
	uint32_t i = slot(p);
	while(g_set[i]) i = (i + 1) & (g_cap - 1);
	g_set[i] = p; g_live++;
}
static int setDel(uintptr_t p){
	if(!g_cap) return 0;
	uint32_t i = slot(p);
	while(g_set[i] != p){ if(!g_set[i]) return 0; i = (i + 1) & (g_cap - 1); }
	g_set[i] = 0; g_live--;
	for(uint32_t j = (i + 1) & (g_cap - 1); g_set[j]; j = (j + 1) & (g_cap - 1)){	// backward shift: re-place the rest of the cluster
		uintptr_t q = g_set[j]; g_set[j] = 0; uint32_t k = slot(q);
		while(g_set[k]) k = (k + 1) & (g_cap - 1);
		g_set[k] = q;
	}
	return 1;
}
static void heapReport(const char *when);
static void *track(void *p){
	if(!p) return p;
	lock(); ++g_allocs; setAdd((uintptr_t)p); unlock();
	if((uintptr_t)p & 7){ g_misaligned++; fprintf(stderr, "mflrw: heap: allocation %p not 8-byte aligned\n", p); }
	return p;
}
static int untrack(void *p, const char *what, void *caller){
	lock(); int mine = setDel((uintptr_t)p); unsigned n = mine ? (g_frees++, 0) : ++g_foreign; unlock();
	if(n && n <= 20) fprintf(stderr, "mflrw: heap: foreign %s %p (#%u) caller %p\n", what, p, n, caller);	// caller = return address of RwFree / RwRealloc
	return mine;
}
static void heapReport(const char *when){
	fprintf(stderr, "mflrw: heap %s: bound %d allocs %u frees %u live %u foreign %u misaligned %u\n", when, g_heapBound, g_allocs, g_frees, g_live, g_foreign, g_misaligned);
}
static struct HeapAtExit { ~HeapAtExit(){ heapReport("exit"); } } g_heapAtExit;
// games are killed, not exited: once bound, a reporter thread prints "heap total" every second while the counters move, so the last such line of a
// game's log is that process's total (at most 1 s stale).  ponytail: 1 s polling thread; a per-frame hook if a gate ever needs exact totals
static DWORD WINAPI heapReporter(void *){
	unsigned last[4] = { ~0u, 0, 0, 0 };
	for(;;){
		unsigned now[4] = { g_allocs, g_frees, g_foreign, g_misaligned };
		if(memcmp(now, last, sizeof now)){ memcpy(last, now, sizeof now); heapReport("total"); }
		Sleep(1000);
	}
}

extern "C" {
// L2:impl
RWAPI void * RwMalloc(size_t size, RwUInt32 hint){ return track(xMalloc(size, hint)); }
// L2:impl
RWAPI void RwFree(void *mem){ if(!mem) return; untrack(mem, "free", __builtin_return_address(0)); xFree(mem); }
// L2:impl  (failure keeps the old block; size 0 frees it like the CRT)
RWAPI void * RwRealloc(void *mem, size_t size, RwUInt32 hint){
	if(!mem) return RwMalloc(size, hint);
	int mine = untrack(mem, "realloc", __builtin_return_address(0));
	void *r = xRealloc(mem, size, hint);
	if(r) return track(r);
	if(size && mine){ lock(); setAdd((uintptr_t)mem); unlock(); }
	return r;
}
// L2:impl
RWAPI void * RwCalloc(size_t n, size_t size, RwUInt32 hint){ return track(xCalloc(n, size, hint)); }
// L2:impl  RW 3.7: &RwEngineInstance->memoryFuncs (0x4e9940); the shadow's table forwards to the four entries above
RWAPI RwMemoryFunctions * RwOsGetMemoryInterface(void){ return (RwMemoryFunctions *)(g_shadow.raw + 0x108); }
// exe table = RW 3.7 RwMemoryFunctions {malloc(size, hint), free(p), realloc(p, size, hint), calloc(n, size, hint)}; refused once the DLL heap holds blocks
void __cdecl mflrw_mem_bind(void *const *exeTable){
	if(g_live){ fprintf(stderr, "mflrw: heap: mem_bind refused, %u live DLL-heap blocks\n", g_live); return; }
	xMalloc = (void *(__cdecl *)(size_t, uint32_t))exeTable[0]; xFree = (void (__cdecl *)(void *))exeTable[1];
	xRealloc = (void *(__cdecl *)(void *, size_t, uint32_t))exeTable[2]; xCalloc = (void *(__cdecl *)(size_t, size_t, uint32_t))exeTable[3];
	if(!g_heapBound){		// close the handle at once: an open handle shifts the numbers of every later kernel handle (the audio library stores them) against the original
		HANDLE t = CreateThread(nil, 0, heapReporter, nil, 0, nil);
		if(t) CloseHandle(t);
	}
	g_heapBound = 1;
	heapReport("bind");
}
// out[0..4] = bound, allocs, live, foreign, misaligned; print != 0 also writes the log line.  Returns foreign.
unsigned __cdecl mflrw_heap_stats(unsigned *out, int print){
	if(out){ out[0] = g_heapBound; out[1] = g_allocs; out[2] = g_live; out[3] = g_foreign; out[4] = g_misaligned; }
	if(print) heapReport("stats");
	return g_foreign;
}
}
// librw's own allocations (Engine::memfuncs) go through the same tracked entries
static void *lwMalloc(size_t n, uint32 hint){ return n ? RwMalloc(n, hint) : nil; }
static void *lwRealloc(void *p, size_t n, uint32 hint){ return RwRealloc(p, n, hint); }
static MemoryFunctions g_lwMem = { lwMalloc, lwRealloc, RwFree, nil, nil };
static int ci_cmp(const char *a, const char *b){
	for(;; a++, b++){ int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b); if(x != y) return x < y ? -1 : 1; if(!x) return 0; }
}
static char *up(char *s){ for(char *p = s; *p; p++) *p = (char)toupper((unsigned char)*p); return s; }
static char *lo(char *s){ for(char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p); return s; }
static int shadowInit(void){
	void **sf = (void**)(g_shadow.raw + 0xc4);
	sf[0] = (void*)sprintf; sf[1] = (void*)vsprintf; sf[2] = (void*)strcpy; sf[3] = (void*)strncpy; sf[4] = (void*)strcat; sf[5] = (void*)strncat;
	sf[6] = (void*)(char*(*)(char*, int))strrchr; sf[7] = (void*)(char*(*)(char*, int))strchr; sf[8] = (void*)(char*(*)(char*, const char*))strstr;
	sf[9] = (void*)strcmp; sf[10] = (void*)strncmp; sf[11] = (void*)ci_cmp; sf[12] = (void*)strlen; sf[13] = (void*)up; sf[14] = (void*)lo;
	sf[15] = (void*)strtok; sf[16] = (void*)sscanf;
	void **mf = (void**)(g_shadow.raw + 0x108);
	mf[0] = (void*)RwMalloc; mf[1] = (void*)RwFree; mf[2] = (void*)RwRealloc; mf[3] = (void*)RwCalloc;
	return 1;
}

namespace rw { extern bool32 engineHeadless; }
static void setFileFuncs(void){
	// FILENAME streams go through the file system manager (RtFS / the game's piz) when one is registered, else stdio
	engine->filefuncs.rwfopen = (void *(*)(const char*, const char*))mflrw_fs_open;
	engine->filefuncs.rwfclose = (int (*)(void*))mflrw_fs_close;
	engine->filefuncs.rwfseek = (int (*)(void*, long, int))mflrw_fs_seek;
	engine->filefuncs.rwftell = (long (*)(void*))mflrw_fs_tell;
	engine->filefuncs.rwfread = (size_t (*)(void*, size_t, size_t, void*))mflrw_fs_read;
	engine->filefuncs.rwfwrite = (size_t (*)(const void*, size_t, size_t, void*))mflrw_fs_write;
	engine->filefuncs.rwfeof = (int (*)(void*))mflrw_fs_eof;
}
// S2-d: the game's own RwEngineInit/Open (not swapped yet) never reach the DLL, so the first swapped call (RwStreamOpen, RtFSManagerOpen ...) creates a headless engine
// (no render device: rasters are the null driver's until the D3D9 layer is swapped).  Every swapped entry that touches `engine` calls this first.
extern "C" void mflrw_ensure_engine(void){
	if(engine) return;
	engineHeadless = 1;
	if(Engine::state == Engine::Dead) Engine::init(&g_lwMem);
	Engine::open(nil);
	engineHeadless = 0;
	setFileFuncs();
	mflrw_shadow_device(g_shadow.raw);
}

extern "C" {

RWAPI void * rwmfl_engine(void){ return engine; }

// ---- engine life cycle (RwEngine*): librw allocates through RwMalloc/RwFree (single heap above); the game passes memFuncs = 0, the exe heap comes from mflrw_mem_bind
// L2:impl
RWAPI RwBool RwEngineInit(const RwMemoryFunctions *memFuncs, RwUInt32 initFlags, RwUInt32 resArenaSize){ (void)memFuncs; (void)initFlags; (void)resArenaSize; return Engine::init(&g_lwMem); }
// L2:impl
RWAPI RwBool RwEngineOpen(RwEngineOpenParams *initParams){
	if(!Engine::open((EngineOpenParams*)initParams)) return 0;
	setFileFuncs();
	return 1;
}
// L2:impl
RWAPI RwBool RwEngineStart(void){ mflrw_d3d9_prestart(); RwBool r = Engine::start(); mflrw_shadow_device(g_shadow.raw); return r; }
// L2:impl
RWAPI RwBool RwEngineStop(void){ Engine::stop(); return 1; }
// L2:impl
RWAPI RwBool RwEngineClose(void){ Engine::close(); return 1; }
// L2:impl
RWAPI RwBool RwEngineTerm(void){ Engine::term(); heapReport("term"); return 1; }
// L2:impl
RWAPI RwInt32 RwEngineGetNumSubSystems(void){ return Engine::getNumSubSystems(); }
// L2:impl
RWAPI RwInt32 RwEngineGetCurrentSubSystem(void){ return Engine::getCurrentSubSystem(); }
// L2:impl
RWAPI RwBool RwEngineSetSubSystem(RwInt32 i){ return Engine::setSubSystem(i); }
// L2:impl
RWAPI RwSubSystemInfo * RwEngineGetSubSystemInfo(RwSubSystemInfo *info, RwInt32 i){ return (RwSubSystemInfo*)Engine::getSubSystemInfo((SubSystemInfo*)info, i); }
// L2:impl
RWAPI RwInt32 RwEngineGetNumVideoModes(void){ return Engine::getNumVideoModes(); }
// L2:impl
RWAPI RwInt32 RwEngineGetCurrentVideoMode(void){ return Engine::getCurrentVideoMode(); }
// L2:impl
RWAPI RwBool RwEngineSetVideoMode(RwInt32 mode){ return Engine::setVideoMode(mode); }
// RW 3.7 RwVideoMode has six fields (librw four): refRate and format are filled with 0 / the raster format of the depth
struct RwVideoModeRW { RwInt32 width, height, depth; RwUInt32 flags; RwInt32 refRate; RwUInt32 format; };
// L2:impl
RWAPI RwVideoMode * RwEngineGetVideoModeInfo(RwVideoMode *modeinfo, RwInt32 modeIndex){
	VideoMode vm;
	if(!Engine::getVideoModeInfo(&vm, modeIndex)) return nil;
	RwVideoModeRW *o = (RwVideoModeRW*)modeinfo;
	o->width = vm.width; o->height = vm.height; o->depth = vm.depth; o->flags = vm.flags; o->refRate = 0;
	o->format = vm.depth == 16 ? 0x0200 : vm.depth == 32 ? 0x0500 : vm.depth == 24 ? 0x0600 : 0;	// rwRASTERFORMAT565 / 8888 / 888
	return modeinfo;
}
// L2:impl  (the RW engine plugin list: constructors run in RwEngineStart)
RWAPI RwInt32 RwEngineRegisterPlugin(RwInt32 size, RwUInt32 pluginID, void *initCB, void *termCB){ return Engine::registerPlugin(size, pluginID, (Constructor)initCB, (Destructor)termCB); }

// ---- RwStream
struct RwMemoryRW { uint8 *start; uint32 length; };
enum { SF_FILE = 1, SF_FILENAME = 2, SF_MEMORY = 3, SF_CUSTOM = 4 };
enum { SA_READ = 1, SA_WRITE = 2, SA_APPEND = 3 };
// closing a memory stream hands the buffer back through pData: tell the two kinds apart by their own subclass
struct MemStream : StreamMemory { };
struct FileStream : StreamFile { };
static inline bool isMem(Stream *s){ static MemStream proto; return *(void**)s == *(void**)&proto; }

// ponytail: FILE (1) and CUSTOM (4) streams, and memory write / append, return NULL (not used by the game); add when a caller appears.
// L2:oracle  FILENAME (read, write, append) through the file system manager / stdio, MEMORY (read only: the game never writes a memory stream).
RWAPI RwStream * RwStreamOpen(RwStreamType type, RwStreamAccessType access, const void *pData){
	mflrw_ensure_engine();
	if(type == SF_FILENAME || type == SF_MEMORY) TR("mflrw: Open type %d name/ptr %s\n", (int)type, type == SF_FILENAME ? (const char*)pData : "mem");
	if(type == SF_FILENAME){
		FileStream *s = new FileStream;
		if(!s->open((const char*)pData, access == SA_READ ? "rb" : access == SA_WRITE ? "wb" : "ab")){
			static int n; if(n++ < 40) fprintf(stderr, "mflrw: RwStreamOpen(\"%s\", access %d) failed (rtfs %d)\n", (const char*)pData, (int)access, mflrw_fs_active());	// ponytail: stderr -> wine.log, first 40
			delete s; return nil; }
		return (RwStream*)static_cast<Stream*>(s);
	}
	if(type == SF_MEMORY && access == SA_READ){
		const RwMemoryRW *m = (const RwMemoryRW*)pData;
		MemStream *s = new MemStream;
		s->open(m->start, m->length);
		return (RwStream*)static_cast<Stream*>(s);
	}
	fprintf(stderr, "mflrw: RwStreamOpen type %d access %d unsupported\n", (int)type, (int)access);
	return nil;
}
void *mflrw_chunkStream; uint32 mflrw_chunkType, mflrw_chunkSize;	// the last chunk RwStreamFindChunk found (E3)
// L2:oracle
RWAPI RwBool RwStreamClose(RwStream *stream, void *pData){
	TR("mflrw: Close(%p)\n", (void*)stream);
	Stream *s = (Stream*)stream;
	if(stream == mflrw_chunkStream) mflrw_chunkStream = nil;
	if(isMem(s)){
		// 0x4dc2a0: pData is written back only for WRITE / APPEND memory streams (not supported here); the game passes junk pointers for READ streams (stack smash found in S2-d)
	}else
		s->close();
	delete s;
	return 1;
}
// L2:oracle  RwStreamFindChunk returns the unpacked version like the original (disasm 0x4dc540/0x4dc720); NAOBJECT ends the search
RWAPI RwBool RwStreamFindChunk(RwStream *stream, RwUInt32 type, RwUInt32 *lengthOut, RwUInt32 *versionOut){
	Stream *s = (Stream*)stream;
	struct { int32 type, size; uint32 id; } h;
	for(;;){
		if(s->read32(&h, 12) != 12){ TR("mflrw: FindChunk(%p, 0x%x) short read\n", (void*)stream, type); return 0; }
		if(h.type == ID_NAOBJECT) return 0;
		if((uint32)h.type == type){
			int ver = libraryIDUnpackVersion(h.id);
			if((uint32)ver < 0x35000 || (uint32)ver > 0x37002) return 0;	// 0x4dc7be: version outside 3.5.0.0 .. 3.7.0.2 (the original also reports RwError 0x80000004)
			if(lengthOut) *lengthOut = h.size;
			if(versionOut) *versionOut = (uint32)ver;	// 0x4dc540: the UNPACKED version (0x37002), T-L6 was wrong about raw
			mflrw_chunkStream = stream; mflrw_chunkType = type; mflrw_chunkSize = (uint32)h.size;	// for readers that need the size (RtPITexDictionaryStreamRead)
			TR("mflrw: FindChunk(%p, 0x%x) = len %d ver %08x\n", (void*)stream, type, h.size, h.id);
			return 1;
		}
		s->seek(h.size);
	}
}
// L2:impl
RWAPI RwUInt32 RwStreamRead(RwStream *stream, void *buffer, RwUInt32 length){ RwUInt32 r = ((Stream*)stream)->read8(buffer, length); TR("mflrw: Read(%p, %u) = %u\n", (void*)stream, length, r); return r; }
// L2:impl
RWAPI RwStream * RwStreamWrite(RwStream *stream, const void *buffer, RwUInt32 length){ return ((Stream*)stream)->write8(buffer, length) == length ? stream : nil; }
// L2:impl
RWAPI RwStream * RwStreamSkip(RwStream *stream, RwUInt32 offset){ TR("mflrw: Skip(%p, %u)\n", (void*)stream, offset); ((Stream*)stream)->seek((int32)offset, 1); return stream; }
// L2:impl
RWAPI RwStream * RwStreamReadReal(RwStream *stream, RwReal *reals, RwUInt32 numBytes){ RwUInt32 r = ((Stream*)stream)->read32(reals, numBytes); TR("mflrw: ReadReal(%p, %u) = %u\n", (void*)stream, numBytes, r); return r == numBytes ? stream : nil; }
// L2:impl  (_rwStreamWriteVersionedChunkHeader)
RWAPI RwStream * rwStreamWriteVersionedChunkHeader(RwStream *stream, RwInt32 type, RwInt32 size, RwUInt32 version, RwUInt32 buildNum){
	struct { int32 type, size; uint32 id; } b = { type, size, libraryIDPack((int)version, (int)buildNum) };
	return ((Stream*)stream)->write32(&b, 12) == 12 ? stream : nil;
}

}
