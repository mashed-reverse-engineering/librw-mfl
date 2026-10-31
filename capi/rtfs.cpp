// S2-c group 1: the RenderWare file system manager (rtfsmgr, the part of the toolkit that the game's piz archive file system hooks into) and
// the RwFopen / RwFread / RwFwrite / RwFseek / RwFclose / RwFeof entries, written from the decompile of MFL.exe 0x560980..0x561120
// (ghidra/output/named/MFL_rtfs_manager.c) and the RW 3.7 SDK header rtfsyst.h (layouts).  The file systems themselves (the Windows one at
// 0x5616f0, the game's "piz") stay game code: they only see these entries and the RtFileSystem / RtFile structs, exactly like in the original.
// No librw types here (windows.h is included).  The stream layer (stream.cpp) opens FILENAME streams through mflrw_fs_*.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION cs;
#define LOCK() EnterCriticalSection(&cs)
#define UNLOCK() LeaveCriticalSection(&cs)
#else
#define LOCK()
#define UNLOCK()
#endif
#include "rwmfl.h"
#include "fsys.h"

// RtFile (rtfsyst.h, 32 bit): the game's piz and the Windows file system read / write these fields by offset
struct File {
	uint32_t lenLo, lenHi;       // 0x00 length
	uint32_t posLo, posHi;       // 0x08 position
	uint32_t initLo, initHi;     // 0x10 initialPos
	int32_t isAsync;             // 0x18
	int32_t priority;            // 0x1c
	int32_t status;              // 0x20  RTFILE_STATUS_*: CLOSED 1, READY 2, BUSY 3, ERROR 4, OPENING 5
	int32_t error;               // 0x24
	int32_t outstandingOp;       // 0x28  RTFS_FILEOP_*: NONE 1, OPEN 2, CLOSE 3, SEEK 4, READ 5
	uint32_t outstandingSize;    // 0x2c
	void *outstandingCB;         // 0x30
	void *outstandingCBData;     // 0x34
	struct FileSys *fs;          // 0x38
};
struct FileSys {
	struct FileSys *next;        // 0x00
	int32_t maxNbOpenFiles;      // 0x04
	int32_t status;              // 0x08
	char *deviceName;            // 0x0c
	int32_t defaultToAsync;      // 0x10
	File *(*getObject)(FileSys *, uint32_t index);                       // 0x14
	void (*close)(FileSys *);                                            // 0x18
	char *(*getDeviceName)(FileSys *);                                   // 0x1c
	void (*registerCB)(char *);  // 0x20
	void (*unregisterCB)(char *);// 0x24
	int32_t (*open)(FileSys *, File *, const char *, int32_t access);    // 0x28 returns RtFileSystemError
	void (*fclose)(File *);                                              // 0x2c
	uint32_t (*read)(File *, void *, uint32_t);                          // 0x30
	uint32_t (*write)(File *, const void *, uint32_t);                   // 0x34
	unsigned long long (*setPosition)(File *, int32_t, int32_t);         // 0x38
	int32_t (*sync)(File *, int32_t);                                    // 0x3c
	int32_t (*abort)(File *);                                            // 0x40
	int32_t (*isEOF)(File *);                                            // 0x44
	int32_t (*getStatus)(File *);                                        // 0x48
	int32_t (*exists)(FileSys *, const char *);                          // 0x4c
	char name[8];                // 0x50
};
typedef char s0[offsetof(File, fs) == 0x38 ? 1 : -1];
typedef char s1[offsetof(FileSys, name) == 0x50 && offsetof(FileSys, exists) == 0x4c && offsetof(FileSys, read) == 0x30 ? 1 : -1];

enum { ACC_READ = 1, ACC_WRITE = 2, ACC_CREATE = 4, ACC_APPEND = 8, ACC_ASYNC = 0x10 };
enum { ERR_NOERROR = 1, ERR_TOOMANY = 2, ERR_NAMEINUSE = 6, ERR_DEVNAMEREG = 7 };

// manager state (DAT_007da894.. in the exe)
static FileSys *g_head, *g_default;
static int g_count, g_max, g_opened;
static void (*g_errCB)(int);
static int g_n; 
#define TR(...) do{ static int on = -1; if(on < 0){ const char *e = getenv("MFLRW_TRACE"); on = e && *e; } if(on && g_n++ < 400000) fprintf(stderr, __VA_ARGS__); }while(0)
static void err(int code){ if(g_errCB) g_errCB(code); }

extern "C" {

// L2:impl  0x560980 FUN_00560980
RWAPI RwBool RtFSManagerOpen(RwInt32 maxNbFS){
	TR("mflrw: RtFSManagerOpen(%d)\n", (int)maxNbFS);
#ifdef _WIN32
	InitializeCriticalSection(&cs);
#endif
	g_count = 0; g_max = maxNbFS; g_errCB = 0; g_default = 0; g_head = 0; g_opened = 1;
	return 1;
}

// L2:impl  0x5609c0 (the exe has it unnamed): closes and frees every file system the way the original does (fsClose, free deviceName, unregister callback, free)
RWAPI void FUN_005609c0(void){
	if(!g_opened) return;
	for(FileSys *fs = g_head, *n; fs; fs = n){
		n = fs->next;
		fs->close(fs);
		free(fs->deviceName); fs->deviceName = 0;
		if(fs->unregisterCB) fs->unregisterCB(fs->name);
		// the FS object itself was allocated by the exe's engine allocator (RwEngineInstance+0x108, game side 0x5616f0): not freed here (shutdown leak)
	}
#ifdef _WIN32
	DeleteCriticalSection(&cs);
#endif
	g_head = 0; g_opened = 0; g_count = 0;
}

// L2:impl  0x560a60
RWAPI RwBool RtFSManagerRegister(RtFileSystem *f){
	FileSys *fs = (FileSys*)f;
	TR("mflrw: Register(%p \"%s\")\n", (void*)f, fs->name);
	if(g_max > 0 && g_max < g_count + 1){ err(5); return 0; }
	for(FileSys *p = g_head; p; p = p->next)
		if(strcmp(p->name, fs->name) == 0) return 0;	// the original leaves through this return when the name is registered already
	err(6);												// (the original reports 6 = not found when the list was walked without a hit, then registers)
	if(fs->registerCB) fs->registerCB(fs->name);
	fs->next = g_head; g_head = fs; g_count++;
	return 1;
}

// L2:impl  0x560b00
RWAPI RtFileSystem * RtFSManagerGetFileSystemFromName(RwChar *fsName){
	for(FileSys *p = g_head; p; p = p->next)
		if(strcmp(p->name, fsName) == 0) return (RtFileSystem*)p;
	err(6);
	return 0;
}

// L2:impl  0x560cf0
RWAPI void RtFSManagerSetDefaultFileSystem(RtFileSystem *fs){ TR("mflrw: SetDefault(%p)\n", (void*)fs); g_default = (FileSys*)fs; }
// L2:impl  0x560d00
RWAPI RtFileSystem * RtFSManagerGetDefaultFileSystem(void){ return (RtFileSystem*)g_default; }

// L2:impl  0x561120
RWAPI void RtFSManagerSyncAllFiles(void){
	for(FileSys *p = g_head; p; p = p->next)
		for(int i = 0; i < p->maxNbOpenFiles; i++){
			File *f = p->getObject(p, (uint32_t)i);
			if(f && f->status != 1) f->fs->sync(f, 0);
		}
}

static int nameInUse(const char *n){
	for(FileSys *p = g_head; p; p = p->next) if(strcmp(p->name, n) == 0){ err(7); return 1; }
	err(6);
	return 0;
}
static FileSys *byDevice(const char *dev){
	for(FileSys *p = g_head; p; p = p->next) if(p->deviceName && _stricmp(dev, p->deviceName) == 0) return p;
	return 0;
}

// L2:impl  0x561900: the generic part of a file system's creation (name, device name, file objects closed)
RWAPI RtFileSystemError rtFSInit(RtFileSystem *f, RwInt32 maxNbFiles, RwChar *fsName, RwChar *deviceName){
	FileSys *fs = (FileSys*)f;
	if(g_max > 0 && g_max < g_count + 1){ err(5); return ERR_TOOMANY; }
	if(nameInUse(fsName)) return ERR_NAMEINUSE;
	strcpy(fs->name, fsName);
	fs->maxNbOpenFiles = maxNbFiles;
	fs->defaultToAsync = 0;
	for(int i = 0; i < maxNbFiles; i++){
		File *o = fs->getObject(fs, (uint32_t)i);
		o->status = 1;
		o->isAsync = fs->defaultToAsync;
	}
	if(byDevice(deviceName)) return ERR_DEVNAMEREG;
	fs->deviceName = (char*)malloc(strlen(deviceName) + 1);
	strcpy(fs->deviceName, deviceName);
	fs->status = 2;
	fs->registerCB = 0; fs->unregisterCB = 0;
	return ERR_NOERROR;
}
// L2:impl  the Ghidra name of the same function
RWAPI RtFileSystemError _rtFSInit(RtFileSystem *f, RwInt32 n, RwChar *fsName, RwChar *dev){ return rtFSInit(f, n, fsName, dev); }

static File *openOnFS(FileSys *fs, const char *name, int access){
	File *file = 0;
	if(!fs){ err(4); return 0; }
	LOCK();
	for(int i = 0; i < fs->maxNbOpenFiles; i++){
		File *o = fs->getObject(fs, (uint32_t)i);
		if(o->status == 1){ o->status = 5; file = o; break; }
	}
	UNLOCK();
	if(!file){ err(8); return 0; }
	if((access & ACC_ASYNC) || file->isAsync){ file->outstandingOp = 2; file->outstandingCB = 0; file->outstandingCBData = 0; }
	if(fs->defaultToAsync){ access |= ACC_ASYNC; file->outstandingOp = 2; file->outstandingCB = 0; file->outstandingCBData = 0; }
	if(fs->open(fs, file, name, access) == ERR_NOERROR){ file->priority = 2; return file; }
	file->status = 1;
	return 0;
}

// the file system a name belongs to: "dev:" prefix (device names include the colon) or the default file system
static FileSys *pick(const char *name){
	if(g_count == 0) return 0;
	size_t n = strlen(name);
	for(size_t i = 0; i < n; i++)
		if(name[i] == ':'){
			char dev[100];
			size_t l = i + 1 < sizeof dev ? i + 1 : sizeof dev - 1;
			memcpy(dev, name, l); dev[l] = 0;
			FileSys *p = byDevice(dev);
			if(p) return p;
		}
	return g_default;
}

// L2:impl  0x560d10 (FUN_00560d10): mode string -> access flags, then the file system of the name
RWAPI void * RwFopen(const RwChar *name, const RwChar *mode){
	TR("mflrw: RwFopen(\"%s\", \"%s\")\n", name, mode);
	// the flag logic of 0x560d10: 'r' READ (with '+': APPEND|WRITE|READ), 'w' |= CREATE|WRITE (+READ), 'a' |= APPEND|WRITE (+READ) and CREATE when the mode has a 'b'
	int plus = strchr(mode, '+') != 0, bin = strchr(mode, 'b') != 0, access = 0;
	if(strchr(mode, 'r')) access = plus ? (ACC_APPEND | ACC_WRITE | ACC_READ) : ACC_READ;
	if(strchr(mode, 'w')) access |= plus ? (ACC_CREATE | ACC_WRITE | ACC_READ) : (ACC_CREATE | ACC_WRITE);
	if(strchr(mode, 'a')){ access |= plus ? (ACC_APPEND | ACC_WRITE | ACC_READ) : (ACC_APPEND | ACC_WRITE); if(bin) access |= ACC_CREATE; }
	FileSys *fs = pick(name);
	if(!fs){ err(6); static int n1; if(n1++ < 20) fprintf(stderr, "mflrw: RwFopen(\"%s\"): no file system (count %d, default %p)\n", name, g_count, (void*)g_default); return 0; }
	File *r = openOnFS(fs, name, access);
	if(!r){ static int n2; if(n2++ < 20) fprintf(stderr, "mflrw: RwFopen(\"%s\", access %d) failed on fs %s dev %s\n", name, access, fs->name, fs->deviceName ? fs->deviceName : "?"); }
	return r;
}

// L2:impl  0x560e70
RWAPI RwInt32 RwFclose(void *fptr){
	File *f = (File*)fptr;
	FileSys *fs = f->fs;
	TR("mflrw: Fclose(%p)\n", fptr);
	LOCK();
	f->status = 1;
	fs->fclose(f);
	UNLOCK();
	return 0;
}
// L2:impl  0x560eb0 (api_used.csv calls this exe function RwFwrite; it reads)
RWAPI size_t RwFread(void *addr, size_t size, size_t count, void *fptr){
	File *f = (File*)fptr;
	size_t r = f->fs->read(f, addr, (uint32_t)(size * count)) / size;
	TR("mflrw: Fread(%p, %u x %u) = %u\n", fptr, (unsigned)size, (unsigned)count, (unsigned)r);
	return r;
}
// L2:impl  0x560ee0
RWAPI size_t RwFwrite(const void *addr, size_t size, size_t count, void *fptr){
	File *f = (File*)fptr;
	return f->fs->write(f, addr, (uint32_t)(size * count)) / size;
}
// L2:impl  0x560f10
RWAPI RwInt32 RwFseek(void *fptr, long offset, RwInt32 origin){
	File *f = (File*)fptr;
	TR("mflrw: Fseek(%p, %ld, %d)\n", fptr, offset, (int)origin);
	if(f->isAsync){ f->outstandingOp = 4; f->status = 3; }
	if(origin < 0 || origin > 2) return -1;
	f->fs->setPosition(f, (int32_t)offset, origin + 1);
	return 0;
}
// L2:impl  0x561050
RWAPI RwInt32 RwFeof(void *fptr){ File *f = (File*)fptr; return f->fs->isEOF(f); }

// L2:impl  0x561060: does the named file exist (file system of the name, else the default one)
RWAPI RwBool FUN_00561060(const RwChar *name){
	FileSys *fs = pick(name);
	if(!fs){ err(6); return 0; }
	return fs->exists(fs, name);
}

// ---- stream layer hooks (fsys.h): stdio when the manager is not open (offline tools)
struct HFile { File *rt; FILE *f; unsigned long long pos; };
int mflrw_fs_active(void){ return g_count != 0; }
void *mflrw_fs_open(const char *name, const char *mode){
	HFile *h = (HFile*)calloc(1, sizeof *h);
	if(mflrw_fs_active()){ h->rt = (File*)RwFopen(name, mode); if(!h->rt){ free(h); return 0; } }
	else { h->f = fopen(name, mode); if(!h->f){ free(h); return 0; } }
	return h;
}
int mflrw_fs_close(void *p){ HFile *h = (HFile*)p; if(h->rt) RwFclose(h->rt); else fclose(h->f); free(h); return 0; }
int mflrw_fs_seek(void *p, long off, int whence){
	HFile *h = (HFile*)p;
	if(!h->rt) return fseek(h->f, off, whence);
	File *f = h->rt;
	if(f->isAsync){ f->outstandingOp = 4; f->status = 3; }
	h->pos = f->fs->setPosition(f, (int32_t)off, whence + 1);
	return 0;
}
long mflrw_fs_tell(void *p){ HFile *h = (HFile*)p; return h->rt ? (long)h->pos : ftell(h->f); }
size_t mflrw_fs_read(void *buf, size_t sz, size_t n, void *p){
	HFile *h = (HFile*)p;
	if(!h->rt) return fread(buf, sz, n, h->f);
	size_t r = RwFread(buf, sz, n, h->rt); h->pos += r * sz; return r;
}
size_t mflrw_fs_write(const void *buf, size_t sz, size_t n, void *p){
	HFile *h = (HFile*)p;
	if(!h->rt) return fwrite(buf, sz, n, h->f);
	size_t r = RwFwrite(buf, sz, n, h->rt); h->pos += r * sz; return r;
}
int mflrw_fs_eof(void *p){ HFile *h = (HFile*)p; return h->rt ? RwFeof(h->rt) : feof(h->f); }

}
