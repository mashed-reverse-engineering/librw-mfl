#!/usr/bin/env python3
"""Collision tree reader following the ORIGINAL MFL.exe control flow:
_CollisionDataStreamRead 0x54d8d0 (u32 version; < 0x36001 -> pre36001 reader, else FindChunk 0x2C),
_rpCollTreeStreamRead 0x54de40 (FindChunk STRUCT, 0x24 byte header, splits, map) and _rpCollTreeCreate 0x54dbc0
(numEntries / numSplits truncated to u16). BSP walk reused from tests/diff/orig_reader.py.
usage: orig_coll.py FILE...  -> canonical dump on stdout (same format as colldump)"""
import struct, sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'diff'))
import orig_reader as R

payloads = []


def read_ext(s, ctx, owner):
    l, v = s.find(R.ID_EXT)
    coll = None
    while l:
        t, cl, cv = s.hdr()
        if t == 0x11D and owner == 'sector':
            coll = s.d[s.p:s.p + cl]
        s.p += cl
        l -= 12 + cl
    if owner == 'sector':
        payloads.append(coll)
    return []


R.read_ext = read_ext


def find(d, p, want):
    while p + 12 <= len(d):
        t, l, v = struct.unpack_from('<III', d, p); p += 12
        if t == want:
            return p, l
        p += l
    raise EOFError


def parse(b):
    if b is None or len(b) == 0:
        return None          # constructor sets NULL; zero length chunk: plugin read not called
    ver = struct.unpack_from('<I', b, 0)[0]
    if ver < 0x36001:
        return 'PRE36001'
    p, l = find(b, 4, 0x2C)
    p, l = find(b, p, 1)
    h = struct.unpack_from('<I6fII', b, p); p += 0x24
    flags, f, nent, nsplit = h[0], h[1:7], h[7] & 0xFFFF, h[8] & 0xFFFF
    nodes = []
    for i in range(nsplit):
        for k in range(2):
            ty, co, ix, va = struct.unpack_from('<BBHI', b, p); p += 8
            nodes.append((ty, co, ix, va))
    m = list(struct.unpack_from('<%dH' % nent, b, p)) if flags & 1 else None
    return dict(flags=flags, inf=f[0:3], sup=f[3:6], nent=nent, nsplit=nsplit, nodes=nodes, map=m)


def fh(x):
    return '%08x' % struct.unpack('<I', struct.pack('<f', x))[0]


def dump(path):
    payloads.clear()
    ctx = R.read_world(open(path, 'rb').read())
    out = []
    for i, (sec, b) in enumerate(zip(ctx.sectors, payloads)):
        t = parse(b)
        if t is None:
            out.append('s %d ntri %d notree' % (i, sec['ntri']))
            continue
        if t == 'PRE36001':
            out.append('s %d ntri %d pre36001' % (i, sec['ntri']))
            continue
        out.append('s %d ntri %d flags %x inf %s sup %s nent %d nsplit %d' % (
            i, sec['ntri'], t['flags'], ' '.join(map(fh, t['inf'])), ' '.join(map(fh, t['sup'])), t['nent'], t['nsplit']))
        out.append(' n ' + ' '.join('%x,%x,%x,%08x' % n for n in t['nodes']))
        out.append(' m ' + (' '.join('%x' % x for x in t['map']) if t['map'] is not None else '-'))
    return out


if __name__ == '__main__':
    for f in sys.argv[1:]:
        print('F', f)
        for line in dump(f):
            print(line)
