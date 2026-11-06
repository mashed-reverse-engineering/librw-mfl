/* L3a fixture: single-heap binding and the foreign-free counter of mflrw.dll (capi/core.cpp).
   run: docker run --rm -v $O:/t tianon/wine:latest wine /t/test_heap.exe /t/mflrw_null.dll   -> "PASS ..." / "FAIL line N" */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
static int fails;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
/* the "exe heap" of the test: CRT functions behind counters, RW 3.7 RwMemoryFunctions layout */
static int nm, nf, nr, nc;
static void *xm(size_t n, unsigned h){ (void)h; nm++; return malloc(n); }
static void xf(void *p){ nf++; free(p); }
static void *xr(void *p, size_t n, unsigned h){ (void)h; nr++; return realloc(p, n); }
static void *xc(size_t n, size_t s, unsigned h){ (void)h; nc++; return calloc(n, s); }
int main(int argc, char **argv){
	HMODULE h = LoadLibraryA(argc > 1 ? argv[1] : "mflrw_null.dll");
	if(!h){ printf("FAIL LoadLibrary %lu\n", GetLastError()); return 2; }
	void *(*RwMalloc)(size_t, unsigned) = (void *)GetProcAddress(h, "RwMalloc");
	void (*RwFree)(void *) = (void *)GetProcAddress(h, "RwFree");
	void *(*RwRealloc)(void *, size_t, unsigned) = (void *)GetProcAddress(h, "RwRealloc");
	void *(*RwCalloc)(size_t, size_t, unsigned) = (void *)GetProcAddress(h, "RwCalloc");
	void **(*RwOsGetMemoryInterface)(void) = (void *)GetProcAddress(h, "RwOsGetMemoryInterface");
	void (*bind)(void *const *) = (void *)GetProcAddress(h, "mflrw_mem_bind");
	unsigned (*stats)(unsigned *, int) = (void *)GetProcAddress(h, "mflrw_heap_stats");
	int (*RwEngineInit)(void *, unsigned, unsigned) = (void *)GetProcAddress(h, "RwEngineInit");
	int (*RwEngineTerm)(void) = (void *)GetProcAddress(h, "RwEngineTerm");
	if(!RwMalloc || !RwFree || !RwRealloc || !RwCalloc || !RwOsGetMemoryInterface || !bind || !stats || !RwEngineInit || !RwEngineTerm){ printf("FAIL missing export\n"); return 2; }
	unsigned s[5];
	/* RwOsGetMemoryInterface = shadow +0x108, entries malloc / free / realloc / calloc */
	void **mi = RwOsGetMemoryInterface();
	CHECK(mi == (void **)(*(char **)GetProcAddress(h, "RwEngineInstance") + 0x108));
	CHECK(mi[0] == (void *)RwMalloc && mi[1] == (void *)RwFree && mi[2] == (void *)RwRealloc && mi[3] == (void *)RwCalloc);
	/* unbound: DLL pairs are not foreign */
	void *p = RwMalloc(100, 0); p = RwRealloc(p, 5000, 0); RwFree(p); RwFree(RwCalloc(3, 7, 0));
	CHECK(stats(s, 0) == 0 && s[0] == 0 && s[2] == 0);
	/* negative fixture: a CRT pointer freed / reallocated through the DLL is counted */
	RwFree(malloc(64));
	CHECK(stats(s, 0) == 1);
	RwFree(RwRealloc(malloc(64), 128, 0));
	CHECK(stats(s, 0) == 2);
	/* bind refused while DLL-heap blocks are live */
	void *live = RwMalloc(8, 0);
	void *const table[4] = { (void *)xm, (void *)xf, (void *)xr, (void *)xc };
	bind(table); stats(s, 0); CHECK(s[0] == 0);
	RwFree(live);
	/* bind: every entry and librw's engine allocate on the bound heap */
	bind(table); stats(s, 0); CHECK(s[0] == 1);
	p = RwMalloc(32, 0); p = RwRealloc(p, 64, 0); RwFree(p); RwFree(RwCalloc(4, 4, 0));
	CHECK(nm == 1 && nr == 1 && nc == 1 && nf == 2);
	int m0 = nm + nc;
	CHECK(RwEngineInit(0, 0, 0));
	CHECK(nm + nc > m0);
	RwEngineTerm();
	stats(s, 1);
	CHECK(s[3] == 2 && s[4] == 0);
	if(!fails) printf("PASS heap fixture: foreign %u (2 planted), allocs %u, bound heap malloc %d calloc %d\n", s[3], s[1], nm, nc);
	return fails != 0;
}
