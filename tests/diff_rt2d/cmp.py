#!/usr/bin/env python3
# usage: cmp.py lib orig -> counts of differing records by kind; V positions compared with 0.01 px tolerance
import sys, collections
a = open(sys.argv[1]).read().split('\n'); b = open(sys.argv[2]).read().split('\n')
if len(a) != len(b): print("LINECOUNT", len(a), len(b))
cnt = collections.Counter(); ex = {}
ctx = ''
for x, y in zip(a, b):
    if not x.startswith(' V'): ctx = x
    if x == y: continue
    p, q = x.split(), y.split()
    if p[0] == 'V' and q[0] == 'V':
        pos = abs(float(p[1]) - float(q[1])) > 0.01 or abs(float(p[2]) - float(q[2])) > 0.01
        uv = p[3:5] != q[3:5]
        col = p[5:] != q[5:]
        dmax = max(abs(int(i) - int(j)) for i, j in zip(p[5:], q[5:]))
        for k, v in (('Vpos', pos), ('Vuv', uv), ('Vcol', col)):
            if v:
                cnt[k] += 1; ex.setdefault(k, (ctx, x, y))
        if col: cnt['Vcol_maxdiff_%d' % dmax] += 1
    else:
        cnt[p[0]] += 1; ex.setdefault(p[0], (x, y))
print(dict(cnt))
for k, v in ex.items(): print(' ', k, '|'.join(v))
