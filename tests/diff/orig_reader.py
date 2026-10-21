#!/usr/bin/env python3
"""Reader that follows the ORIGINAL MFL.exe control flow (RpWorldStreamRead 0x4f9bd0,
WorldSectorStreamRead 0x4fa060, PlaneSectorStreamRead 0x4fa440,
_rpWorldSetupSectorBoundingBoxes 0x4f5300, RpWorldUnlock 0x4f5420, normal decode 0x506ad0).
usage: orig_reader.py dump FILE      -> canonical dump (same format as bspdump.cpp)
       orig_reader.py facts FILE...  -> per-file facts"""
import struct, sys, hashlib

ID_STRUCT, ID_EXT, ID_MATLIST, ID_ATOMIC, ID_PLANE, ID_WORLD = 1, 3, 8, 9, 10, 0xB


def unpack_ver(lib):
    if lib & 0xFFFF0000:
        return (((lib >> 14) & 0x3FF00) + 0x30000) | ((lib >> 16) & 0x3F)
    return lib << 8


class S:
    def __init__(s, d): s.d, s.p = d, 0
    def hdr(s):
        t, l, v = struct.unpack_from('<III', s.d, s.p); s.p += 12
        return t, l, unpack_ver(v)
    def find(s, want):
        # RwStreamFindChunk: skip chunks until type matches
        while s.p + 12 <= len(s.d):
            t, l, v = s.hdr()
            if t == want:
                return l, v
            s.p += l
        raise EOFError('chunk %x not found' % want)
    def read(s, n):
        b = s.d[s.p:s.p + n]
        if len(b) != n: raise EOFError('short read')
        s.p += n; return b


class Ctx:
    pass


def ver_ok(v):
    return 0x35000 <= v <= 0x37002


def read_ext(s, ctx, owner):
    l, v = s.find(ID_EXT)
    if not ver_ok(v): raise ValueError('ext version')
    end = s.p + l
    ids = []
    while l:
        t, cl, cv = s.hdr()
        ids.append(t)
        s.p += cl
        l -= 12 + cl
    ctx.extids.setdefault(owner, set()).update(ids)
    return ids


def world_sector(s, ctx):
    l, v = s.find(ID_STRUCT)
    if not ver_ok(v): raise ValueError('sector version')
    h = s.read(0x2c)
    base, ntri, nvert = struct.unpack_from('<iii', h, 0)
    f = struct.unpack_from('<6f', h, 12)
    w9, w10 = struct.unpack_from('<ii', h, 36)
    sec = dict(leaf=True, base=base & 0xFFFF, ntri=ntri & 0xFFFF, nvert=nvert & 0xFFFF,
               tight_inf=f[0:3], tight_sup=f[3:6], w9=w9, w10=w10, ver=v)
    fmt = ctx.fmt
    if not (ctx.worldflags & 0x1000000):
        if nvert:
            sec['verts'] = s.read(12 * nvert)
            if fmt & 0x10: sec['normals'] = s.read(4 * nvert)
            if fmt & 8: sec['colors'] = s.read(4 * nvert)
            sec['uvs'] = [s.read(8 * (nvert & 0xFFFF)) for _ in range(ctx.nsets)]
        if ntri:
            raw = s.read(8 * ntri)
            tris = list(struct.iter_unpack('<4H', raw))
            if v < 0x36002:
                tris = [(w1, w2, w3, w0) for (w0, w1, w2, w3) in tris]
            sec['tris'] = tris  # {v0,v1,v2,mat}
    sec['ext'] = read_ext(s, ctx, 'sector')
    ctx.sectors.append(sec)
    ctx.consumed_after_struct = None
    return sec


def plane_sector(s, ctx):
    l, v = s.find(ID_STRUCT)
    if not ver_ok(v): raise ValueError('plane version')
    b = s.read(l) + bytes(max(0, 24 - l))
    typ, val, lw, rw, lv, rv = struct.unpack_from('<ifiiff', b)
    if not (ctx.fmt & 0x40000000):
        lv = rv = val
    p = dict(leaf=False, type=typ, value=val, lv=lv, rv=rv, lw=lw, rw=rw)
    ctx.planes.append(p)
    for side, isw in (('left', lw), ('right', rw)):
        if isw:
            cl, cv = s.find(ID_ATOMIC)
            if not ver_ok(cv): raise ValueError
            p[side] = world_sector(s, ctx)
        else:
            cl, cv = s.find(ID_PLANE)
            if not ver_ok(cv): raise ValueError
            p[side] = plane_sector(s, ctx)
    # note: original does NOT look for an extension after a plane sector
    return p


def setup_boxes(root, wsup, winf):
    # _rpWorldSetupSectorBoundingBoxes: left child sup[axis]=leftValue, right child inf[axis]=rightValue
    def rec(n, sup, inf):
        if n['leaf']:
            n['bsup'], n['binf'] = tuple(sup), tuple(inf)
            return
        a = n['type'] // 4
        ls = list(sup); ls[a] = n['lv']
        ri = list(inf); ri[a] = n['rv']
        rec(n['left'], ls, list(inf))
        rec(n['right'], list(sup), ri)
    rec(root, list(wsup), list(winf))


def read_world(data):
    s = S(data)
    t, l, v = s.hdr()
    assert t == ID_WORLD
    l, v = s.find(ID_STRUCT)
    if not ver_ok(v): raise ValueError('world version %x' % v)
    b = s.read(l) + bytes(max(0, 64 - l))
    w = struct.unpack_from('<i3fiiiiiI6f', b)
    ctx = Ctx()
    ctx.ver = v
    ctx.root_is_ws, ctx.inv = w[0], w[1:4]
    ctx.ntri, ctx.nvert, ctx.nplane, ctx.nsect, ctx.colsize, ctx.fmt = w[4:10]
    ctx.sup, ctx.inf = w[10:13], w[13:16]
    if ctx.colsize >= 1: raise ValueError('colSectorSize >= 1 rejected by original')
    fmt = ctx.fmt
    ctx.worldflags = fmt
    if fmt & 0xFF0000: ctx.nsets = (fmt >> 16) & 0xFF
    elif fmt & 0x80: ctx.nsets = 2
    else: ctx.nsets = (fmt >> 2) & 1
    ctx.sectors, ctx.planes, ctx.extids = [], [], {}
    l, v = s.find(ID_MATLIST)
    if not ver_ok(v): raise ValueError
    # material list: skip whole chunk (we only need the count)
    ml_start = s.p
    l2, v2 = s.find(ID_STRUCT)
    ctx.nmat = struct.unpack_from('<i', data, s.p)[0]
    s.p = ml_start + l
    if ctx.root_is_ws:
        cl, cv = s.find(ID_ATOMIC); ctx.root = world_sector(s, ctx)
    else:
        cl, cv = s.find(ID_PLANE); ctx.root = plane_sector(s, ctx)
    setup_boxes(ctx.root, ctx.sup, ctx.inf)
    ctx.worldext = read_ext(s, ctx, 'world')
    ctx.end = s.p
    return ctx


def h(b):
    x = 0xcbf29ce484222325
    for c in b:
        x = ((x ^ c) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % x


def dump(ctx):
    out = []
    out.append('world fmt %08X nsets %d tri %d vert %d plane %d sect %d nmat %d' % (
        ctx.fmt, ctx.nsets, ctx.ntri, ctx.nvert, ctx.nplane, ctx.nsect, ctx.nmat))
    out.append('bbox inf %.6g %.6g %.6g sup %.6g %.6g %.6g' % (*ctx.inf, *ctx.sup))
    out.append('invorigin %.6g %.6g %.6g' % ctx.inv)

    def rec(n):
        if n['leaf']:
            return
        out.append('plane %d %.6g %.6g %.6g %s %s' % (n['type'], n['value'], n['lv'], n['rv'],
                   'S' if n['left']['leaf'] else 'P', 'S' if n['right']['leaf'] else 'P'))
        rec(n['left']); rec(n['right'])
    rec(ctx.root)
    for i, sc in enumerate(ctx.sectors):
        nv, nt = sc['nvert'], sc['ntri']
        verts = sc.get('verts', b'')
        nrm = sc.get('normals')
        nrm_s = '-'
        if nrm is not None:
            # decode as original: int8 * (1/128)
            fl = b''.join(struct.pack('<3f', *(struct.unpack_from('<3b', nrm, 4 * k)[j] * 0.0078125 for j in range(3))) for k in range(nv))
            nrm_s = h(fl)
        col = h(sc['colors']) if 'colors' in sc else '-'
        uvs = ','.join(h(u) for u in sc.get('uvs', [])) or '-'
        tris = sc.get('tris', [])
        tri_s = h(b''.join(struct.pack('<4H', t[0], t[1], t[2], sc['base'] + t[3]) for t in tris))
        out.append('sector %d base %d tri %d vert %d tight %.6g %.6g %.6g %.6g %.6g %.6g' % (
            i, sc['base'], nt, nv, *sc['tight_inf'], *sc['tight_sup']))
        out.append('  plbox %.6g %.6g %.6g %.6g %.6g %.6g' % (*sc['binf'], *sc['bsup']))
        out.append('  v %s n %s c %s uv %s t %s' % (h(verts), nrm_s, col, uvs, tri_s))
    return '\n'.join(out) + '\n'


if __name__ == '__main__':
    if sys.argv[1] == 'dump':
        sys.stdout.write(dump(read_world(open(sys.argv[2], 'rb').read())))
    else:
        for fn in sys.argv[2:]:
            d = open(fn, 'rb').read()
            try:
                c = read_world(d)
            except Exception as e:
                print('ERR', fn, e); continue
            types = sorted({p['type'] for p in c.planes})
            ovl = sum(1 for p in c.planes if not (p['lv'] == p['value'] == p['rv']))
            w9 = sorted({sc['w9'] for sc in c.sectors}); w10 = sorted({sc['w10'] for sc in c.sectors})
            sv = sorted({sc['ver'] for sc in c.sectors})
            notight = sum(1 for sc in c.sectors if any(sc['tight_inf'][k] < sc['binf'][k] - 1e-3 or sc['tight_sup'][k] > sc['bsup'][k] + 1e-3 for k in range(3)))
            maxtri = max([sc['ntri'] for sc in c.sectors] or [0])
            print('%s ver %x fmt %08X col %d nsets %d types %s overlapPlanes %d/%d w9 %s w10 %s secver %s sectExt %s worldExt %s planeExt? end %d/%d tightOutsidePlbox %d maxtri %d rootWS %d' % (
                fn.split('extracted/')[-1], c.ver, c.fmt, c.colsize, c.nsets, types, ovl, len(c.planes), w9[:4], w10[:4],
                [hex(x) for x in sv], sorted(hex(x) for x in c.extids.get('sector', [])), sorted(hex(x) for x in c.extids.get('world', [])),
                c.end, len(d), notight, maxtri, c.root_is_ws))
