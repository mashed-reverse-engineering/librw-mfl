import re, collections, sys
def parse(fn):
    out = []
    for l in open(fn):
        l = l.rstrip('\n')
        if l.startswith('tex'): out.append([l, None, []])
        elif l.startswith('  pal'): out[-1][1] = l
        elif l.startswith('  lvl'): out[-1][2].append(l)
    return out
c = collections.Counter(); ex = {}
for i in range(1, 200):
    try: L, O = parse('d/%d.lib' % i), parse('d/%d.orig' % i)
    except FileNotFoundError: continue
    if not O: continue
    if len(L) != len(O): c['texcount'] += 1; continue
    for a, b in zip(L, O):
        d = int(re.search(r' d(\d+) ', b[0]).group(1))
        if a[0] != b[0]: c['hdr d%d' % d] += 1; ex.setdefault('hdr', (i, a[0], b[0]))
        if a[1] != b[1]: c['pal d%d' % d] += 1
        if len(a[2]) != len(b[2]): c['nlev d%d' % d] += 1; ex.setdefault('nlev', (i, a[0], len(a[2]), len(b[2]), a[2][-2:], b[2][-2:]))
        for x, y in zip(a[2], b[2]):
            c[('lvl same' if x == y else 'lvl DIFF') + ' d%d' % d] += 1
for k, v in sorted(c.items()): print(k, v)
for k, v in ex.items(): print(k, v)
