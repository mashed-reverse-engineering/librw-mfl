/* Wine/Windows test exe for mflrw.dll: loads the DLL, calls G0 functions, checks that the x87 control word is unchanged
   across LoadLibrary and the calls.  usage: test_g0.exe [mflrw.dll]   (exit 0 = pass) */
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "rwmfl.h"

static int fails;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static unsigned short getcw(void){ unsigned short c; __asm__ volatile("fnstcw %0" : "=m"(c)); return c; }
static void setcw(unsigned short c){ __asm__ volatile("fldcw %0" : : "m"(c)); }
static int near_(float a, float b, float tol){ return fabsf(a - b) <= tol; }

#define LIST X(RwV3dNormalize) X(RwV3dLength) X(RwV2dLength) X(RwV2dNormalize) X(_rwSqrt) X(_rwInvSqrt) X(RwV3dTransformPoint) X(RwV3dTransformVectors) X(RwMatrixTranslate) X(RwMatrixScale) X(RwMatrixUpdate) X(RwMatrixMultiply) X(RtIntersectionLineTriangle) X(RpRandomSeed) X(RpRandom) X(RwCameraEndUpdate) X(rwmfl_stub_count) X(rwmfl_stub_info) X(rwmfl_get_fpucw) X(rwmfl_layout_check)
#define X(n) static __typeof__(&n) n##_p;
LIST
#undef X
#define X(n) n##_p = (__typeof__(&n))GetProcAddress(h, #n); if(!n##_p){ printf("FAIL missing export %s\n", #n); return 2; }

int main(int argc, char **argv){
	setcw(0x7f);						/* the game's x87 state */
	unsigned short before = getcw();
	HMODULE h = LoadLibraryA(argc > 1 ? argv[1] : "mflrw.dll");
	if(!h){ printf("FAIL LoadLibrary %lu\n", GetLastError()); return 2; }
	unsigned short after = getcw();
	LIST
#undef X
#define RwV3dNormalize RwV3dNormalize_p
#define RwV3dLength RwV3dLength_p
#define RwV2dLength RwV2dLength_p
#define RwV2dNormalize RwV2dNormalize_p
#define _rwSqrt _rwSqrt_p
#define _rwInvSqrt _rwInvSqrt_p
#define RwV3dTransformPoint RwV3dTransformPoint_p
#define RwV3dTransformVectors RwV3dTransformVectors_p
#define RwMatrixTranslate RwMatrixTranslate_p
#define RwMatrixScale RwMatrixScale_p
#define RwMatrixUpdate RwMatrixUpdate_p
#define RwMatrixMultiply RwMatrixMultiply_p
#define RtIntersectionLineTriangle RtIntersectionLineTriangle_p
#define RpRandomSeed RpRandomSeed_p
#define RpRandom RpRandom_p
#define RwCameraEndUpdate RwCameraEndUpdate_p
#define rwmfl_stub_count rwmfl_stub_count_p
#define rwmfl_stub_info rwmfl_stub_info_p
#define rwmfl_get_fpucw rwmfl_get_fpucw_p
#define rwmfl_layout_check rwmfl_layout_check_p

	printf("CW before LoadLibrary 0x%04x after 0x%04x\n", before, after);
	CHECK(before == after);


	RwV3d v = {3, 4, 0}, o;
	float len = RwV3dNormalize(&o, &v);
	CHECK(near_(len, 5, 5e-3f) && near_(o.x, 0.6f, 1e-3f) && near_(o.y, 0.8f, 1e-3f) && o.z == 0);
	RwV3d w = {1, 2, 2}; printf("len(1,2,2)=%.9g sqrt(4)=%.9g sqrt(9)=%.9g\n", RwV3dLength(&w), _rwSqrt(4.0f), _rwSqrt(9.0f)); CHECK(near_(RwV3dLength(&w), 3, 3e-3f));
	RwV2d p = {3, 4}, q; CHECK(near_(RwV2dLength(&p), 5, 5e-3f)); CHECK(near_(RwV2dNormalize(&q, &p), 5, 5e-3f) && near_(q.y, 0.8f, 1e-3f));
	CHECK(near_(_rwSqrt(4.0f), 2, 2e-3f)); CHECK(near_(_rwInvSqrt(4.0f), 0.5f, 1e-3f));
	CHECK(_rwSqrt(0.0f) == 0.0f);

	RwMatrix m; memset(&m, 0, sizeof m); m.right.x = m.up.y = m.at.z = 1; m.flags = 0x20003;
	RwV3d t = {1, 2, 3};
	CHECK(RwMatrixTranslate(&m, &t, 2) == &m && m.pos.x == 1 && m.pos.y == 2 && m.pos.z == 3 && (m.flags & 0x20000) == 0);
	RwV3d s = {2, 3, 4}; RwMatrixScale(&m, &s, 1);
	CHECK(m.right.x == 2 && m.up.y == 3 && m.at.z == 4 && (m.flags & 3) == 0 && m.pos.x == 1);
	RwV3d a = {1, 1, 1}, r; RwV3dTransformPoint(&r, &a, &m);
	CHECK(r.x == 3 && r.y == 5 && r.z == 7);
	RwV3d arr[2] = {{1, 0, 0}, {0, 0, 1}}, out2[2]; RwV3dTransformVectors(out2, arr, 2, &m);
	CHECK(out2[0].x == 2 && out2[1].z == 4 && out2[1].x == 0);
	RwMatrixUpdate(&m); CHECK((m.flags & 0x20003) == 0);
	RwMatrix id; memset(&id, 0, sizeof id); id.right.x = id.up.y = id.at.z = 1; id.flags = 0x20003;
	RwMatrix mm; RwMatrixMultiply(&mm, &m, &id); CHECK(mm.right.x == 2 && mm.pos.z == 3);

	RwV3d tv0 = {0, 0, 0}, tv1 = {1, 0, 0}, tv2 = {0, 1, 0}, ls = {0.25f, 0.25f, 1}, ld = {0, 0, -2};
	RwReal dist = -1; RwBool hit = RtIntersectionLineTriangle(&ls, &ld, &tv0, &tv1, &tv2, &dist);
	RwV3d ld2 = {0, 0, 2}; RwBool hit2 = RtIntersectionLineTriangle(&ls, &ld2, &tv0, &tv1, &tv2, &dist);
	printf("line-triangle hit=%d (reverse %d)\n", hit, hit2);
	CHECK(hit != hit2);					/* one sided: exactly one direction hits */

	RpRandomSeed(1234); RwUInt32 r1 = RpRandom(), r2 = RpRandom();
	RpRandomSeed(1234); CHECK(RpRandom() == r1 && RpRandom() == r2 && r1 != r2);

	CHECK(rwmfl_get_fpucw() == getcw());
	void *f = RwCameraEndUpdate(NULL); CHECK(f == NULL);		/* an unimplemented entry is a counting stub */
	int n = rwmfl_stub_count(), called = 0; const char *nm; unsigned hits;
	for(int i = 0; i < n; i++) if(rwmfl_stub_info(i, &nm, &hits) && hits){ called++; printf("stub hit: %s x%u\n", nm, hits); }
	CHECK(called == 1);
	printf("stubs %d, layout mismatches (expected until S2) %d\n", n, rwmfl_layout_check());

	unsigned short end = getcw();
	printf("CW at end 0x%04x\n", end);
	CHECK(end == before);
	printf(fails ? "RESULT: %d FAILED\n" : "RESULT: PASS\n", fails);
	return fails != 0;
}
