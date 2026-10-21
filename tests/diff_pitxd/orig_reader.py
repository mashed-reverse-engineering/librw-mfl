#!/usr/bin/env python3
"""PI texture dictionary reader that follows the control flow of the ORIGINAL MFL.exe routines
(RtPITexDictionaryStreamRead 0x55ff00, rtpitexdTextureStreamReadPre 0x560760, RwImageStreamRead 0x4de740,
RwImageAllocatePixels 0x4dd610, RwImageGammaCorrect 0x4de880, _rwD3D9ImageFindFormat 0x4eef60), modern D3D9 caps
assumed (pow2 not required / data is pow2 anyway, no square-only, 32 bit display, A8R8G8B8/X8R8G8B8 valid).
usage: orig_reader.py dump FILE      (same line format as pitxddump)
       orig_reader.py stats FILE...  (D3D9 colour format choice etc.)"""
import struct, sys, zlib, collections

class S:  # RwStream over bytes
    def __init__(s, d): s.d, s.p = d, 0
    def read(s, n):
        b = s.d[s.p:s.p + n]; s.p += n
        return b
    def u32(s): return struct.unpack('<I', s.read(4))[0]
    def hdr(s):
        if s.p + 12 > len(s.d): return None
        return struct.unpack('<III', s.read(12))
    def find(s, want):  # RwStreamFindChunk: skip other chunks
        while True:
            h = s.hdr()
            if h is None: return None
            if h[0] == want: return h
            s.p += h[1]

def ver(stamp):  # library id stamp -> version
    return (((stamp >> 14) & 0x3ff00) + 0x30000) | ((stamp >> 16) & 0x3f) if stamp & 0xffff0000 else stamp << 8

def image_read(s):  # RwImageStreamRead (chunk 0x18 header already consumed by FindChunk)
    if s.find(1) is None: return None
    w, h, d, fstride = struct.unpack('<4I', s.read(16))
    bpp = (d + 7) >> 3                    # RwImageAllocatePixels: depth 4 -> ONE byte per pixel
    stride = (bpp * w + 3) & ~3
    rows = []
    for y in range(h):
        if fstride == stride: row = s.read(stride)
        elif fstride < stride: row = s.read(fstride) + b'\0' * (stride - fstride)  # rest uninitialised in the original
        else: row = s.read(stride); s.p += fstride - stride
        rows.append(row)
    pal = s.read((1 << d) * 4) if d < 9 else b''
    # RwImageGammaCorrect is applied (flag 2 never set by StreamRead) - with the engine gamma 1.0 the table is identity
    return dict(w=w, h=h, d=d, stride=stride, rows=rows, pal=pal)

def d3d9_find_format(img):  # _rwD3D9ImageFindFormat, 32 bit display
    w, h, d = img['w'], img['h'], img['d']
    if d in (4, 8):
        pal = img['pal']; n = 1 << d
        if all(pal[4 * i + 3] == 0xff for i in range(n)): return 'PAL%d|888' % d
        for y in range(h):
            for x in range(w):
                if pal[img['rows'][y][x] * 4 + 3] != 0xff: return 'PAL%d|8888' % d
        return 'PAL%d|888' % d
    a = False
    for y in range(h):
        r = img['rows'][y]
        for x in range(w):
            if r[4 * x + 3] != 0xff: a = True
    return '8888' if a else '888'

def read_pre(s):  # rtpitexdTextureStreamReadPre
    h = s.find(1)
    if h is None or not (0x35000 <= ver(h[2]) <= 0x37002): return None
    fa = struct.unpack('<I', s.read(h[1])[:4])[0]  # original reads h[1] bytes into a 4 byte local
    if (fa & 0xf000) == 0: fa |= (fa & 0xf00) << 4
    names = []
    for _ in range(2):  # _rwStringStreamFindAndRead: chunk 0x02 (ascii) or 0x13 (unicode, low bytes)
        while True:
            h = s.hdr()
            if h is None or not (0x35000 <= ver(h[2]) <= 0x37002): return None
            if h[0] == 2: raw = s.read(h[1]); break
            if h[0] == 0x13: raw = s.read(h[1])[::2]; break
            s.p += h[1]
        nm = raw.split(b'\0')[0][:31]  # RwTextureSetName: strncpy 32, truncate to 31
        names.append(nm.decode('latin1'))
    return dict(fa=fa & 0xffff, name=names[0], mask=names[1])

def read_dict(d):
    s = S(d)
    if s.find(0x23) is None: return None
    v = s.u32(); n, dev = v & 0xffff, v >> 16
    if dev == 0: raise NotImplementedError('legacy (deviceId 0) layout, not in data')
    out = []
    for _ in range(n):
        ni = s.u32()
        imgs = []
        for i in range(ni):
            if s.find(0x18) is None: return None
            imgs.append(image_read(s))
        if s.find(6) is None: return None
        t = read_pre(s)
        if t is None: return None
        if ni >= 2: mip, auto = 1, 0                        # 0x8004
        elif (t['fa'] & 0xff) in (1, 2): mip, auto = 0, 0   # 0x0004
        else: mip, auto = 1, 1                              # 0x9004
        i0 = imgs[0]
        nlev = (max(i0['w'], i0['h']).bit_length()) if mip else 1  # D3D9 full chain
        levels = []
        for k in range(min(nlev, ni)):                       # lock(k) + SetFromImage; extra images ignored
            im = imgs[k]
            lw, lh = max(1, i0['w'] >> k), max(1, i0['h'] >> k)
            assert (im['w'], im['h']) == (lw, lh)            # else the original resamples
            bpp = 4 if im['d'] == 32 else 1
            levels.append((lw, lh, b''.join(r[:lw * bpp] for r in im['rows'])))
        pal = imgs[len(levels) - 1]['pal']                   # each SetFromImage rewrites the palette
        h = s.find(3)                                        # _rwPluginRegistryReadDataChunks
        s.p += h[1]
        t.update(w=i0['w'], h=i0['h'], d=i0['d'], mip=mip, auto=auto, pal=pal, levels=levels, ni=ni,
                 fmt=d3d9_find_format(i0))
        out.append(t)
    return out

def load(fn):
    d = open(fn, 'rb').read()
    if d[:1] == b'\x78': d = zlib.decompress(d)
    return d

def main():
    if sys.argv[1] == 'dump':
        d = load(sys.argv[2])
        if struct.unpack_from('<I', d)[0] != 0x23: return
        ts = read_dict(d)
        print('dict %d' % len(ts))
        for t in ts:
            print("tex '%s' '%s' fa %04x %dx%d d%d mip %d auto %d" % (t['name'], t['mask'], t['fa'], t['w'], t['h'], t['d'], t['mip'], t['auto']))
            if t['d'] <= 8: print('  pal %08x' % zlib.crc32(t['pal']))
            for k, (w, h, px) in enumerate(t['levels']):
                print('  lvl %d %dx%d %08x' % (k, w, h, zlib.crc32(px)))
    else:
        c = collections.Counter()
        for fn in sys.argv[2:]:
            d = load(fn)
            if struct.unpack_from('<I', d)[0] != 0x23: continue
            for t in read_dict(d):
                c['fmt d%d %s' % (t['d'], t['fmt'])] += 1
                c['mip %d auto %d ni%s' % (t['mip'], t['auto'], '1' if t['ni'] == 1 else '>1')] += 1
        for k, v in sorted(c.items()): print(k, v)

if __name__ == '__main__':
    main()
