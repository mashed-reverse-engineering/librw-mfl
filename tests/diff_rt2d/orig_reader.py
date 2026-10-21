#!/usr/bin/env python3
# Model of the ORIGINAL MFL.exe Rt2d routines (Rt2dFontStreamRead 0x564E40, GetStringWidth 0x565F80,
# texture show 0x5653F0, Flow 0x566060, BrushSetRGBA 0x564B10, _rt2dCTMGet 0x563410) written from the
# decompile/disassembly; emits the same text format as rt2ddump.cpp.  usage: orig_reader.py strings.txt FILE.RWF
import struct, sys, math
from fractions import Fraction as Fr

def f32(x):
    return struct.unpack('<f', struct.pack('<f', float(x)))[0]
def fb(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]

class Font: pass

def read_font(data):
    p = 0
    typ, size, ver = struct.unpack_from('<III', data, p); p += 12
    assert typ == 0x199
    version, = struct.unpack_from('<I', data, p); p += 4
    assert version >> 24 != 0
    f = Font()
    f.type, f.f1, f.baseline, f.gap, f.flags = struct.unpack_from('<ifffi', data, p); p += 20
    f.highBase, f.numGlyphs, f.numHigh = struct.unpack_from('<iii', data, p); p += 12   # +0x124, +0x130, +0x128
    f.high = []
    if f.numHigh > 0:
        f.high = list(struct.unpack_from('<%dh' % f.numHigh, data, p)); p += 2 * f.numHigh
    f.low = list(struct.unpack_from('<128h', data, p)); p += 256
    assert f.type == 0
    f.glyphs = []
    for i in range(f.numGlyphs):
        u0, v0, u1, v1 = struct.unpack_from('<4f', data, p); p += 16      # -> glyph+8
        adv, = struct.unpack_from('<f', data, p); p += 4                  # -> glyph+0
        ti, = struct.unpack_from('<b', data, p); p += 1                   # signed char at glyph+0x1c
        f.glyphs.append((adv, u0, v0, u1, v1, ti))
    n, = struct.unpack_from('<i', data, p); p += 4
    f.tex = []
    for i in range(n):
        f.tex.append(data[p:p+32].split(b'\0')[0].decode('latin1')); p += 32
    return f

def gidx(f, c):
    if c < 0x80:
        g = f.low[c]
    else:
        d = c - f.highBase
        if d < 0 or d >= f.numHigh: return -1
        g = f.high[d]
    return g if g > -1 else -1

def width_ext(f, s):
    w = 0.0
    for c in s:
        if c == 0: break
        g = gidx(f, c)
        if g > -1: w = f32(f32(f.glyphs[g][0] + f.gap) + w)
    return Fr(w)

def get_string_width(f, s, h):
    return f32(float(width_ext(f, s)) * h)

# ---- device / CTM (4:3 view 2 x 1.5, 640x480, layer depth 1)
VW, VH, RW_, RH = 2.0, 1.5, 640, 480
def to_screen(x, y):
    m = min(VW, VH)
    return x * m / VW * RW_, (1.0 - y * m / VH) * RH

def trunc_byte(x):
    return int(math.trunc(x)) & 0xff

class Brush:
    def __init__(s, c0, c1, c2, c3):   # Rt2dBrushSetRGBA(brush, c0, c1, c2, c3)
        s.top = [float(v) for v in c3]; s.dtop = [float(c2[i]) - float(c3[i]) for i in range(4)]
        s.bot = [float(v) for v in c0]; s.dbot = [float(c1[i]) - float(c0[i]) for i in range(4)]
        s.flat = tuple(c3)
        s.grad = not (c0 == c1 == c2 == c3)

def corner_colour(b, top, t):
    if not b.grad: return b.flat
    base, d = (b.top, b.dtop) if top else (b.bot, b.dbot)
    out = []
    for i in range(4):
        v = f32(Fr(base[i]) + Fr(f32(Fr(d[i]) * t)))   # PC24: every op rounds to float
        out.append(trunc_byte(v))   # fistp with RC = chop
    return tuple(out)

def show(f, s, h, pos, b, out):
    W = width_ext(f, s)
    invW = f32(Fr(1) / W) if W != 0 else float('inf')
    x = 0.0; acc = 0.0
    for c in s:
        if c == 0: break
        g = gidx(f, c)
        if g <= -1: continue
        adv, u0, v0, u1, v1, ti = f.glyphs[g]
        tl = Fr(f32(Fr(invW) * Fr(acc))) if W != 0 else Fr(0)
        tr = Fr(f32(Fr(invW) * Fr(f32(acc + adv)))) if W != 0 else Fr(0)
        for (vx, vy, u, v, top, t) in ((x, 0.0, u0, v1, False, tl), (x, 1.0, u0, v0, True, tl),
                                       (x + adv, 0.0, u1, v1, False, tr), (x + adv, 1.0, u1, v0, True, tr)):
            px = pos[0] + h * vx
            py = pos[1] + h * (vy - f.baseline)
            sx, sy = to_screen(px, py)
            out.append((sx, sy, u, v) + corner_colour(b, top, t))
        x = f32(f32(x + f.gap) + adv)
        acc = f32(f32(acc + f.gap) + adv)
    return f32(f32(x * h) + pos[0])

def flow(f, s, h, rect, fmt, b, out, maxlines=None):
    lineW = f32(Fr(rect[2]) / Fr(h))
    align = f32(h * 0.5) if fmt == 1 else (h if fmt == 2 else 0.0)
    ly = f32(f32(rect[3] + rect[1]) - h)
    first = True
    w = Fr(0); lastW = 0.0
    cur = start = brk = 0
    lines = 0
    while True:
        ch = s[cur]
        if ch == 0:
            px = f32(f32(f32(lineW - float(w)) * align) + rect[0])
            lines += 1
            if out is not None: show(f, s[start:] , h, (px, ly), b, out)
            return True, lines
        g = gidx(f, ch)
        if first and ch == 0x20:
            first = False; brk = cur; lastW = f32(w)
        else:
            first = True
            if ch == 10: brk = cur + 1; lastW = f32(w)
        if g > -1: w = Fr(f32(f32(f.glyphs[g][0] + f.gap) + float(w)))
        if Fr(lineW) < w or ch == 10:
            px = f32(f32(f32(lineW - lastW) * align) + rect[0])
            lines += 1
            if out is not None: show(f, s[start:brk] + [0], h, (px, ly), b, out)
            noprog = (brk == start)
            cur = brk
            while s[cur] == 0x20: cur += 1
            ly = f32(ly - h); lyx = Fr(ly)
            first = True; brk = start = cur; w = Fr(0)
            if lyx < Fr(rect[1]): return False, lines
            if maxlines and noprog and s[cur] != 0: return None, lines
        else:
            cur += 1

def main():
    strs = []
    for line in open(sys.argv[1]):
        strs.append([int(t, 16) for t in line.split()] + [0])
    f = read_font(open(sys.argv[2], 'rb').read())
    o = sys.stdout.write
    o("H %d %08x %08x %08x %d %d %d %d %d\n" % (f.type, fb(f.f1), fb(f.baseline), fb(f.gap), f.flags, f.highBase, f.numGlyphs, f.numHigh, len(f.tex)))
    for i, t in enumerate(f.tex): o("T %d %s\n" % (i, t))
    for i, (adv, u0, v0, u1, v1, ti) in enumerate(f.glyphs):
        o("G %d %08x %08x %08x %08x %08x %d\n" % (i, fb(adv), fb(u0), fb(v0), fb(u1), fb(v1), ti))
    for c in list(range(0x300)): o("M %x %d\n" % (c, gidx(f, c)))
    o("M ffff %d\n" % gidx(f, 0xffff))
    hs = [1.0, f32(0.0708), f32(f32(0.0020833334) * 24), f32(0.05)]
    for i, s in enumerate(strs):
        for k, h in enumerate(hs): o("W %d %d %08x\n" % (i, k, fb(get_string_width(f, s, h))))
    def dq(q):
        for (x, y, u, v, r, g, b, a) in q: o(" V %.3f %.3f %08x %08x %d %d %d %d\n" % (x, y, fb(u), fb(v), r, g, b, a))
    br = Brush((255, 0, 10, 255), (0, 255, 200, 128), (7, 99, 255, 0), (250, 3, 128, 77))
    for i, s in enumerate(strs):
        q = []
        px = show(f, s, f32(0.05), (f32(0.2), f32(0.4)), br, q)
        o("S %d %08x\n" % (i, fb(px))); dq(q)
    flat = Brush(*([(200, 100, 50, 25)] * 4))
    h = f32(f32(0.0708) * f32(0.8))
    for i, s in enumerate(strs):
        for k, wd in enumerate([0.3, 0.6, 1.2]):
            for fmt in range(3):
                q = []
                ok, _ = flow(f, s, h, (f32(0.1), f32(0.1), f32(wd), f32(0.8)), fmt, flat, q)
                o("F %d %d %d %d\n" % (i, k, fmt, ok)); dq(q)
                ok2, n = flow(f, s, h, (0.0, 0.0, f32(wd), f32(1.0e6)), 0, None, None, maxlines=65536)
                o("N %d %d %d\n" % (i, k, n if ok2 is not None else 65537))

main()
