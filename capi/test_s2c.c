/* Wine/Windows test exe for mflrw_null.dll, S2-c group 1: engine start through the C ABI, RwStream memory + FILENAME (stdio and through a registered mock file
   system = the shape of the game's piz hooks), RtFS manager entries, chunk search, texture dictionary entries, x87 control word.  usage: test_s2c.exe [mflrw_null.dll]  (exit 0 = pass) */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "rwmfl.h"

static int fails;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static unsigned short getcw(void){ unsigned short c; __asm__ volatile("fnstcw %0" : "=m"(c)); return c; }
static void setcw(unsigned short c){ __asm__ volatile("fldcw %0" : : "m"(c)); }

/* RtFile / RtFileSystem as the game's piz and the Windows file system see them (rtfsyst.h, 32 bit) */
typedef struct File { uint32_t len[2], pos[2], init[2]; int isAsync, priority, status, error, op; uint32_t osize; void *ocb, *ocbd; struct FS *fs; } File;
typedef struct FS { struct FS *next; int maxFiles, status; char *deviceName; int defAsync; File *(*getObject)(struct FS *, unsigned);
	void (*close)(struct FS *); char *(*getDev)(struct FS *); void *regCB, *unregCB;
	int (*open)(struct FS *, File *, const char *, int); void (*fclose)(File *); unsigned (*read)(File *, void *, unsigned); unsigned (*write)(File *, const void *, unsigned);
	unsigned long long (*setPos)(File *, int, int); int (*sync)(File *, int); int (*abort_)(File *); int (*isEOF)(File *); int (*getStatus)(File *);
	int (*exists)(struct FS *, const char *); char name[8]; } FS;

/* mock file system: the file "mock:a.bin" is a fixed buffer */
static unsigned char blob[64]; static File mockFile; static unsigned mpos; static int opens, closes, lastAccess;
static File *m_get(FS *fs, unsigned i){ (void)fs; (void)i; return &mockFile; }
static void m_close(FS *fs){ (void)fs; }
static int m_open(FS *fs, File *f, const char *name, int acc){ (void)fs; (void)f; lastAccess = acc; if(strcmp(name, "mock:a.bin")) return 3; opens++; mpos = 0; return 1; }
static void m_fclose(File *f){ (void)f; closes++; }
static unsigned m_read(File *f, void *d, unsigned n){ (void)f; if(mpos + n > sizeof blob) n = sizeof blob - mpos; memcpy(d, blob + mpos, n); mpos += n; return n; }
static unsigned m_write(File *f, const void *d, unsigned n){ (void)f; (void)d; return n; }
static unsigned long long m_seek(File *f, int off, int m){ (void)f; if(m == 1) mpos = off; else if(m == 2) mpos += off; else mpos = sizeof blob - off; return mpos; }
static int m_eof(File *f){ (void)f; return mpos >= sizeof blob; }
static int m_exists(FS *fs, const char *n){ (void)fs; return !strcmp(n, "mock:a.bin"); }
static int m_sync(File *f, int b){ (void)f; (void)b; return 0; }

int main(int argc, char **argv){
	setvbuf(stdout, NULL, _IONBF, 0); setcw(0x7f);
	unsigned short before = getcw();
	HMODULE h = LoadLibraryA(argc > 1 ? argv[1] : "mflrw_null.dll");
	if(!h){ printf("FAIL LoadLibrary %lu\n", GetLastError()); return 2; }
#define P(n) __typeof__(&n) n##_p = (__typeof__(&n))GetProcAddress(h, #n); if(!n##_p){ printf("FAIL missing export %s\n", #n); return 2; }
	P(RwEngineInit) P(RwEngineOpen) P(RwEngineStart) P(RwEngineStop) P(RwEngineClose) P(RwEngineTerm)
	P(RwStreamOpen) P(RwStreamClose) P(RwStreamRead) P(RwStreamSkip) P(RwStreamFindChunk) P(RwStreamReadReal) P(RwStreamWrite)
	P(RtFSManagerOpen) P(RtFSManagerRegister) P(RtFSManagerSetDefaultFileSystem) P(RtFSManagerGetFileSystemFromName) P(rtFSInit) P(RwFopen) P(RwFread) P(RwFseek) P(RwFclose) P(RwFeof) P(FUN_00561060)
	P(RwFrameSetIdentity) P(RwFrameCreate) P(RwFrameDestroy) P(RwFrameTranslate) P(RwFrameGetLTM) P(RpMaterialCreate) P(RpMaterialDestroy) P(RwTexDictionaryCreate) P(RwTexDictionarySetCurrent) P(RwTexDictionaryFindNamedTexture) P(RwTexDictionaryDestroy)
	P(RwMalloc) P(RwFree) P(rwmfl_engine)
	void *shadow = *(void **)GetProcAddress(h, "RwEngineInstance");
	CHECK(shadow != NULL);
	{ char **sf = (char **)((char *)shadow + 0xc4); CHECK(((size_t(*)(const char *))sf[12])("abcd") == 4); CHECK(((int(*)(const char *, const char *))sf[11])("AbC", "aBc") == 0);
	  void *(*mal)(size_t, unsigned) = *(void *(**)(size_t, unsigned))((char *)shadow + 0x108); void (*fre)(void *) = *(void (**)(void *))((char *)shadow + 0x10c);
	  void *q = mal(16, 0); CHECK(q != NULL); fre(q); }

	printf("shadow ok\n"); CHECK(RwEngineInit_p(NULL, 0, 0)); printf("init ok\n"); CHECK(RwEngineOpen_p(NULL)); CHECK(RwEngineStart_p());
	CHECK(rwmfl_engine_p() != NULL);

	/* a chunk stream in memory: [0x99 len 4][4 bytes] [0x10 len 8 id 0x1803ffff][8 bytes] */
	uint32_t buf[32] = { 0x99, 4, 0x1803ffff, 0x11111111,  0x10, 8, 0x1803ffff, 1, 2 };
	RwStream *s;
	struct { uint8_t *start; uint32_t length; } mem = { (uint8_t *)buf, 9 * 4 };
	s = RwStreamOpen_p(3, 1, &mem); CHECK(s != NULL);
	uint32_t len = 0, ver = 0;
	CHECK(RwStreamFindChunk_p(s, 0x10, &len, &ver) && len == 8 && ver == 0x36003);
	float f[2]; uint32_t w[2] = { 0x3f800000, 0x40000000 }; (void)w;
	CHECK(RwStreamRead_p(s, f, 4) == 4);
	CHECK(RwStreamSkip_p(s, 4) == s);
	CHECK(!RwStreamFindChunk_p(s, 0x10, &len, &ver));			/* end of stream */
	struct { uint8_t *start; uint32_t length; } back = { 0, 0 };
	CHECK(RwStreamClose_p(s, &back) && back.start == (uint8_t *)buf && back.length == 36);

	/* FILENAME through stdio (no file system registered yet) */
	char tmp[MAX_PATH]; GetTempPathA(sizeof tmp, tmp); strcat(tmp, "s2c_test.bin");
	s = RwStreamOpen_p(2, 2, tmp); CHECK(s != NULL);
	CHECK(RwStreamWrite_p(s, buf, 36) == s); CHECK(RwStreamClose_p(s, NULL));
	s = RwStreamOpen_p(2, 1, tmp); CHECK(s != NULL);
	CHECK(RwStreamFindChunk_p(s, 0x10, &len, &ver) && len == 8); CHECK(RwStreamReadReal_p(s, f, 8) == s); CHECK(RwStreamClose_p(s, NULL));
	DeleteFileA(tmp);
	CHECK(RwStreamOpen_p(2, 1, "no\\such\\file") == NULL);

	/* the mock file system, registered like the game's piz */
	for(int i = 0; i < 64; i++) blob[i] = (unsigned char)(i + 1);
	CHECK(RtFSManagerOpen_p(4));
	FS fs; memset(&fs, 0, sizeof fs);
	fs.getObject = m_get; fs.close = m_close; fs.open = m_open; fs.fclose = m_fclose; fs.read = m_read; fs.write = m_write; fs.setPos = m_seek; fs.isEOF = m_eof; fs.exists = m_exists; fs.sync = m_sync;
	mockFile.fs = &fs;							/* every file object points back to its file system (FUN_005616f0 sets this for the Windows one) */
	CHECK(rtFSInit_p((RtFileSystem *)&fs, 1, "mock", "mock:") == 1);
	CHECK(strcmp(fs.name, "mock") == 0 && fs.status == 2 && mockFile.status == 1);
	CHECK(RtFSManagerRegister_p((RtFileSystem *)&fs));
	CHECK(RtFSManagerGetFileSystemFromName_p("mock") == (RtFileSystem *)&fs);
	CHECK(FUN_00561060_p("mock:a.bin") && !FUN_00561060_p("mock:b.bin"));
	s = RwStreamOpen_p(2, 1, "mock:a.bin"); CHECK(s != NULL && opens == 1 && lastAccess == 1);
	unsigned char rd[8]; CHECK(RwStreamRead_p(s, rd, 4) == 4 && rd[0] == 1 && rd[3] == 4);
	CHECK(RwStreamSkip_p(s, 2) == s);
	CHECK(RwStreamRead_p(s, rd, 2) == 2 && rd[0] == 7);
	CHECK(RwStreamClose_p(s, NULL) && closes == 1 && mockFile.status == 1);
	CHECK(RwStreamOpen_p(2, 1, "mock:nothere") == NULL);
	/* no device prefix: the default file system takes it */
	CHECK(RwStreamOpen_p(2, 1, "a.bin") == NULL);
	RtFSManagerSetDefaultFileSystem_p((RtFileSystem *)&fs);
	void *rf = RwFopen_p("mock:a.bin", "rb"); CHECK(rf != NULL && ((File *)rf)->status == 5);
	CHECK(RwFseek_p(rf, 10, 0) == 0 && mpos == 10); CHECK(RwFread_p(rd, 1, 4, rf) == 4 && rd[0] == 11); CHECK(RwFseek_p(rf, 0, 2) == 0 && RwFeof_p(rf)); CHECK(RwFclose_p(rf) == 0);

	/* objects: frame create / translate / LTM, material, texture dictionary entries */
	RwFrame *fr = RwFrameCreate_p(); CHECK(fr != NULL);
	RwV3d t = { 1, 2, 3 }; CHECK(RwFrameTranslate_p(fr, &t, 1) == fr);
	RwMatrix *ltm = RwFrameGetLTM_p(fr); CHECK(ltm != NULL && ltm->pos.x == 1 && ltm->pos.z == 3);
	CHECK(*(float *)((char *)fr + 0x10 + 0x30) == 1.0f);			/* RwFrame modelling at 0x10, pos.x at +0x30 */
	CHECK(RwFrameSetIdentity_p(fr) == fr && *(unsigned *)((char *)fr + 0x10 + 0x0c) == 0x20003 && *(float *)((char *)fr + 0x10 + 0x30) == 0.0f);
	CHECK(RwFrameDestroy_p(fr));
	RpMaterial *mat = RpMaterialCreate_p(); CHECK(mat != NULL && *(unsigned *)((char *)mat + 4) == 0xffffffffu); CHECK(RpMaterialDestroy_p(mat));
	RwTexDictionary *d = RwTexDictionaryCreate_p(); CHECK(d != NULL && RwTexDictionarySetCurrent_p(d) == d && RwTexDictionaryFindNamedTexture_p(d, "nothing") == NULL); CHECK(RwTexDictionaryDestroy_p(d));

	printf("CW before 0x%04x after 0x%04x\n", before, getcw());
	CHECK(getcw() == before);
	CHECK(RwEngineStop_p() && RwEngineClose_p() && RwEngineTerm_p());
	printf(fails ? "RESULT: %d FAILED\n" : "RESULT: PASS\n", fails);
	return fails != 0;
}
