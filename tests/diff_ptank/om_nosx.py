#!/usr/bin/env python3
# Control-flow model of the original MFL.exe PTank (D3D9 build) from the decompile/disassembly:
# create normalisation 0x545170, allocation 0x545940 / 0x545310 (interleaved), lock 0x545d10, unlock 0x545f20,
# instance 0x546ca0 -> vb lock/axes 0x545f60, handler selection 0x546d90, position handlers 0x546f50..0x547fd0,
# colour 0x548130/0x548190/0x548250 (+ TFACTOR = const colour when not prelit, 0x5462c0), uv 0x548380/0x5483f0/
# 0x548470/0x5484e0, normals 0x548550. Reads fork.txt (inputs + fork outputs), recomputes, compares.
# usage: orig_model.py fork.txt
import sys, struct, math
import numpy as np
F = np.float32
H = F(0.5)

def f(u): return F(struct.unpack('<f', struct.pack('<I', u))[0])
def fbits(x): return struct.unpack('<I', struct.pack('<f', float(x)))[0]

def normalize(p):
    o = p
    pos, size, mat, rot = p & 1, p & 4, p & 8, p & 0x20
    if mat and (o & 0x8000): p = 0x8000                 # mov edi,eax (eax = o & 0x8000)
    if rot and (o & 0x20000): p &= 0x20000                # tests use the ORIGINAL word (sete before any masking)
    if (o & 0x40) and (o & 0x40000): p &= 0x40000
    if (o & 0x80) and (o & 0x80000): p &= 0x80000
    if (o & 0x100) and (o & 0x100000): p &= 0x100000
    if mat:
        if pos: p &= ~1
        if size: p &= ~4
        if rot: p &= ~0x20
    if not mat and not pos: p |= 1
    if (p & 0x30000000) == 0: p |= 0x10000000
    return p & 0xffffffff

ORDER = [(1, 12), (8, 64), (0x10, 12), (4, 8), (2, 4), (0x40, 16), (0x20, 4), (0x80, 16), (0x100, 32)]

def layout(p, n):
    inter = (p & 0x20000000) == 0x20000000
    fmt = (p & ~0x1ff) & 0xffffffff
    num = 0; tot = 0
    for b, s in ORDER:
        if p & b: num += 1; tot += s; fmt |= b
    arr = {}; off = 0
    for b, s in ORDER:
        if p & b:
            arr[b] = (off, tot if inter else s)
            off += s if inter else n * s
    return fmt, num, (tot if inter else 0), arr

def add3(a, b): return [F(a[i] + b[i]) for i in range(3)]
def sub3(a, b): return [F(a[i] - b[i]) for i in range(3)]
def neg3(a): return [F(-a[i]) for i in range(3)]
def mul3(a, s): return [F(a[i] * s) for i in range(3)]

def corners(a, b, p, off=None):
    # v0 = (-a-b)+p, v1 = (a-b)+p, v2 = (b+a)+p, v3 = (b-a)+p (+off when given)
    vs = [sub3(neg3(a), b), sub3(a, b), add3(b, a), sub3(b, a)]
    vs = [add3(v, p) for v in vs]
    if off is not None: vs = [add3(v, off) for v in vs]
    return vs

def quads(fmt, data, K, M, act):
    cx, cy, sx, sy, crot = K['cx'], K['cy'], K['sx'], K['sy'], K['rot']
    R = [F(-M[0]), F(-M[1]), F(-M[2])]; U = [M[4], M[5], M[6]]
    if fmt & 0x8000:
        R = K['m'][0:3]; U = K['m'][4:7]
    elif fmt & 0x20000:
        c = F(math.cos(float(crot))); s = F(math.sin(float(crot)))
        R2 = [F(F(c * R[i]) + F(U[i] * F(-s))) for i in range(3)]
        U2 = [F(F(s * R[i]) + F(c * U[i])) for i in range(3)]
        R, U = R2, U2
    center = (fmt & 0x1000000) != 0
    out = []
    for k in range(act):
        if fmt & 8:
            m = data[8][k]
            r = m[0:3]; u = m[4:7]; pos = m[12:15]
            if center:
                s_ = [F(F(u[i] + r[i]) * H) for i in range(3)]
                d_ = [F(F(r[i] - u[i]) * H) for i in range(3)]
                P = [F(F(F(cx * r[i]) + F(cy * u[i])) + pos[i]) for i in range(3)]
                vs = [add3(P, d_), sub3(P, s_), sub3(P, d_), add3(P, s_)]
            else:
                s_ = [F(F(u[i] * H) + F(r[i] * H)) for i in range(3)]
                d_ = [F(F(r[i] * H) - F(u[i] * H)) for i in range(3)]
                vs = [add3(d_, pos), sub3(pos, s_), sub3(pos, d_), add3(s_, pos)]
        else:
            p = data[1][k]
            rotA = (fmt & 0x8000) == 0 and (fmt & 0x20)
            sizeA = (fmt & 0x8000) == 0 and (fmt & 4)
            if rotA:
                ang = data[0x20][k][0]
                c = F(math.cos(float(ang))); s = F(math.sin(float(ang)))
                r = [F(F(c * R[i]) + F(F(-s) * U[i])) for i in range(3)]
                u = [F(F(c * U[i]) + F(s * R[i])) for i in range(3)]
            else:
                r, u = R, U
            if sizeA:
                w, h = data[4][k][0], data[4][k][1]
            else:
                w, h = sx, sy
            if rotA and sizeA:
                a = [F(F(r[i] * w) * H) for i in range(3)]; b = [F(F(u[i] * h) * H) for i in range(3)]
            elif rotA:
                a = mul3(r, F(w * H)); b = mul3(u, F(h * H))
            else:
                a = mul3(r, F(w * H))
                b = mul3(u, F(h * H))   # 0x5470e0 reads size.x twice
            off = None
            if center:
                off = [F(F(u[i] * cy) + F(cx * r[i])) for i in range(3)]
            vs = corners(a, b, p, off)
        # colour
        if fmt & 2: col = [data[2][k][0]] * 4
        elif fmt & 0x40: col = data[0x40][k][0:4]
        elif fmt & 0x40000: col = K['vc']
        else: col = [K['col']] * 4                     # TFACTOR (not prelit)
        uv = None
        if fmt & 0x80:
            t = data[0x80][k]; uv = [(t[0], t[3]), (t[2], t[3]), (t[2], t[1]), (t[0], t[1])]
        elif fmt & 0x100:
            t = data[0x100][k]; uv = [(t[0], t[1]), (t[2], t[3]), (t[4], t[5]), (t[6], t[7])]
        elif fmt & 0x80000:
            t = K['tc']; uv = [(t[0], t[3]), (t[2], t[3]), (t[2], t[1]), (t[0], t[1])]
        elif fmt & 0x100000:
            t = K['tc']; uv = [(t[0], t[1]), (t[2], t[3]), (t[4], t[5]), (t[6], t[7])]
        nrm = data[0x10][k][0:3] if fmt & 0x10 else None
        for v in range(4):
            out.append((vs[v], col[v], uv[v] if uv else None, nrm))
    return out

def main():
    lines = open(sys.argv[1]).read().split('\n')
    i = 0
    st = dict(cases=0, norm=0, layout=0, lock=0, upd=0, quads=0, cnt=0, posx=0, posmax=0.0, col=0, uv=0, nrm=0, skip=0)
    ex = {}
    def note(k, msg):
        st[k] += 1
        ex.setdefault(k, []).append(msg) if len(ex.get(k, [])) < 4 else None
    while i < len(lines):
        L = lines[i].split()
        if not L or L[0] != 'C': i += 1; continue
        st['cases'] += 1
        raw = int(L[1], 16); fn = int(L[2], 16)
        if normalize(raw) != fn: note('norm', '%08x orig %08x fork %08x' % (raw, normalize(raw), fn))
        if L[3] == 'nil': i += 1; continue
        ffmt, fnum, ftot, n = int(L[3], 16), int(L[4]), int(L[5]), int(L[6])
        p = normalize(raw)
        fmt, num, tot, arr = layout(p, n)
        if (fmt, num, tot) != (ffmt, fnum, ftot): note('layout', '%08x orig %08x/%d/%d fork %08x/%d/%d' % (raw, fmt, num, tot, ffmt, fnum, ftot))
        i += 1
        data = {}; locked = 0
        while lines[i].startswith('L') or lines[i].startswith('D'):
            T = lines[i].split()
            if T[0] == 'L':
                b, r, o, s = int(T[1], 16), int(T[2]), int(T[3]), int(T[4])
                exp = (1, arr[b][0], arr[b][1]) if b in arr else (0, -1, 0)
                if (r, o, s) != exp: note('lock', '%08x bit %x orig %s fork %s' % (raw, b, exp, (r, o, s)))
                if b in arr: locked |= b | 0x40000000
            else:
                b, k = int(T[1], 16), int(T[2])
                ws = [int(x, 16) for x in T[3:]]
                data.setdefault(b, {})[k] = ws if b in (2, 0x40) else [f(w) for w in ws]
            i += 1
        U1 = lines[i].split(); U2 = lines[i+1].split(); i += 2
        if (int(U1[1], 16), int(U1[2], 16)) != (locked, 0xffffffff) or (int(U2[1], 16), int(U2[2], 16)) != (0, 0xffffffff):
            note('upd', '%08x %s %s' % (raw, U1, U2))
        Kl = lines[i].split()[1:]; i += 1
        kw = [int(x, 16) for x in Kl[:-1]]
        K = dict(cx=f(kw[0]), cy=f(kw[1]), sx=f(kw[2]), sy=f(kw[3]), rot=f(kw[4]), col=kw[5], vc=kw[6:10],
                 tc=[f(x) for x in kw[10:18]], m=[f(x) for x in kw[18:34]])
        act = int(Kl[-1])
        M = [f(int(x, 16)) for x in lines[i].split()[1:]]; i += 1
        Q = lines[i].split(); i += 1
        if Q[1] == 'skip':
            st['skip'] += 1
            continue
        got = int(Q[1])
        fv = []
        for _ in range(4 * got):
            V = lines[i].split(); i += 1
            fv.append(V)
        if normalize(raw) != fn: st['normskip'] = st.get('normskip', 0) + 1; continue
        if got != act: note('cnt', '%08x orig %d fork %d' % (raw, act, got)); continue
        st['quads'] += 1
        om = quads(fmt, data, K, M, act)
        for j, (vs, col, uv, nrm) in enumerate(om):
            V = fv[j]
            fp = [f(int(V[1 + c], 16)) for c in range(3)]
            dmax = max(abs(float(fp[c]) - float(vs[c])) for c in range(3))
            if dmax > 0: st['posmax'] = max(st['posmax'], dmax)
            if dmax > 2e-4: note('posx', '%08x fmt %08x v%d orig %s fork %s' % (raw, fmt, j, [float(x) for x in vs], [float(x) for x in fp]))
            # fork prints r g b a bytes; original colour dword = bytes r,g,b,a in memory (little endian)
            cb = struct.pack('<I', col)
            fc = bytes.fromhex(V[7])
            if cb != fc: note('col', '%08x v%d orig %s fork %s' % (raw, j, cb.hex(), fc.hex()))
            if uv is not None:
                fu = (f(int(V[8], 16)), f(int(V[9], 16)))
                if fu != uv: note('uv', '%08x v%d orig %s fork %s' % (raw, j, uv, fu))
            if nrm is not None:
                fnm = [f(int(V[4 + c], 16)) for c in range(3)]
                if fnm != list(nrm): note('nrm', '%08x v%d' % (raw, j))
    for k, v in st.items(): print(k, v)
    for k, v in ex.items():
        print('--', k)
        for m in v: print('  ', m)

main()
