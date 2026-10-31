// internal: stream layer <-> rtfs.cpp (no windows.h / librw types)
#ifndef MFLRW_FSYS_H
#define MFLRW_FSYS_H
#include <stddef.h>
extern "C" {
int mflrw_fs_active(void);
void *mflrw_fs_open(const char *name, const char *mode);
int mflrw_fs_close(void *h);
int mflrw_fs_seek(void *h, long off, int whence);
long mflrw_fs_tell(void *h);
size_t mflrw_fs_read(void *buf, size_t sz, size_t n, void *h);
size_t mflrw_fs_write(const void *buf, size_t sz, size_t n, void *h);
int mflrw_fs_eof(void *h);
}
#endif
