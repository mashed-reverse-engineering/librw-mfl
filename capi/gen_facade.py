#!/usr/bin/env python3
"""gen_facade.py <api_used.csv> <rw sdk hdr dir> <coverage.csv out>
Generates capi/rwmfl.h (C prototypes in RW 3.7 names), capi/rwmfl.def, capi/stubs_gen.cpp (call-counting stubs for every
name no hand-written capi/*.cpp defines) and the coverage csv.  Prototypes come from the SDK headers (interface only),
fallback ghidra output/decls.h, else `int NAME(void)` (cdecl: the caller cleans the stack, so a wrong arg count is safe).
A hand-written definition is `RWAPI <ret> Name(` in capi/*.cpp (not stubs_gen/ rwmfl.h); the comment `// L1:exact`
(disassembly-exact) or `// L1:librw` (librw semantics, not bit-checked) on the line above sets the coverage status."""
import csv, glob, os, re, sys
api, sdk, covout = sys.argv[1:4]
here = os.path.dirname(os.path.abspath(__file__))
decls_h = os.path.join(os.path.dirname(os.path.abspath(api)), '..', 'decls.h')

txt = ''
for f in sorted(glob.glob(sdk + '/*.h')):
    txt += open(f, errors='replace').read() + '\n'
txt = re.sub(r'/\*.*?\*/', '', txt, flags=re.S)
txt = re.sub(r'//[^\n]*', '', txt)
txt = re.sub(r'^[ \t]*#(?:[^\n]*\\\n)*[^\n]*', '', txt, flags=re.M)
txt = re.sub(r'\s+', ' ', txt)
enums = set(re.findall(r'typedef enum [^;{]*\{[^}]*\} ?(\w+) ?;', txt))
fptrs = set(re.findall(r'typedef [^;]*?\( ?\* ?(\w+) ?\) ?\(', txt))
dec = open(decls_h, errors='replace').read() if os.path.exists(decls_h) else ''

BASE = {'RwReal', 'RwBool', 'RwInt8', 'RwUInt8', 'RwInt16', 'RwUInt16', 'RwInt32', 'RwUInt32', 'RwChar', 'RwUInt64', 'RwInt64',
        'RtBaryTransform', 'RwV3d', 'RwV2d', 'RwMatrix', 'RwRGBA', 'RwSphere', 'RwLine', 'RwBBox', 'RwPlane'}
KW = {'const', 'unsigned', 'signed', 'int', 'float', 'double', 'char', 'void', 'struct', 'short', 'long', 'enum', 'extern',
      'static', 'volatile', 'size_t', 'va_list', 'FILE', 'RWAPI', 'bool'}
C = {'int': 'int', 'undefined': 'int', 'undefined1': 'unsigned char', 'undefined2': 'unsigned short', 'undefined4': 'int',
     'uint': 'unsigned', 'ushort': 'unsigned short', 'byte': 'unsigned char', 'bool': 'int'}

def sdk_proto(n):
    m = re.search(r'\bextern ([^;(){}]*?)\b' + re.escape(n) + r' ?\(((?:[^()]|\([^()]*\))*)\) ?;', txt)
    return (m.group(1).strip(), m.group(2).strip()) if m else None

def dec_proto(n):
    m = re.search(r'extern ([^;(]*?)\b' + re.escape(n) + r'\(([^;]*)\);', dec)
    if not m: return None
    ret = re.sub(r'\b(__cdecl|__stdcall|__fastcall|struct|enum)\b ?', '', m.group(1)).strip()
    args = re.sub(r'\b(struct|enum)\b ?', '', m.group(2)).strip()
    return ret, args

rows = [r for r in csv.DictReader(open(api)) if r['module'] != 'rwp physics']
protos = {}; src = {}
for r in rows:
    n = r['name']
    p = sdk_proto(n); s = 'sdk'
    if not p: p = dec_proto(n); s = 'decls'
    if p and (p[0] == '' or 'undefined' in p[0] and not p[1]) and s == 'decls' and not p[1]:
        p = None                                    # `undefined name()`: no information
    if not p: p = ('int', 'void'); s = 'generic'
    ret, args = p
    ret = re.sub(r'\bundefined4?\b', 'int', ret)
    if re.fullmatch(r'\s*', ret): ret = 'int'
    args = re.sub(r'\bundefined4?\b', 'int', args)
    protos[n] = (ret, args or 'void'); src[n] = s
protos['rwSqrt'] = ('RwReal', 'RwReal x'); src['rwSqrt'] = 'override'      # 0x4d5410, same function as _rwSqrt
# aliases the game calls by another name (Ghidra name of the same address)
ALIAS = {'_rwSqrt': 'rwSqrt', '_rwInvSqrt': None}
# S2-c: exact prototypes for entries the Ghidra names only know generically (the exe function is the RW 3.7 one named in the note)
PROTO_FIX = {'FUN_004f4920': ('RpWorld *', 'void *object'),                           # RpAtomicGetWorld / RpClumpGetWorld (ICF-folded: *(obj + plugin offset 0x7d52a4))
             'FUN_005609c0': ('void', 'void'),                                       # RtFSManagerClose
             'FUN_00561060': ('RwBool', 'const RwChar *name'),                       # RwFexist (fsFileFunc.fExists on the file system of the name)
             'rtFSInit': ('RtFileSystemError', 'RtFileSystem *fs, RwInt32 maxNbFiles, RwChar *fsName, RwChar *deviceName'),
             'rwStreamWriteVersionedChunkHeader': ('RwStream *', 'RwStream *stream, RwInt32 type, RwInt32 size, RwUInt32 version, RwUInt32 buildNum'),
             'RwFwrite': ('size_t', 'const void *addr, size_t size, size_t count, void *fptr')}
for n, p in PROTO_FIX.items():
    if n in protos: protos[n] = p; src[n] = 'fix'
extra = []                                          # (name, ret, args) not in api_used but needed (G0 group, S2-c memory / file entries)
for n, rt, ar, mod, note in [('_rwSqrt', 'RwReal', 'RwReal x', 'core', 'alias of the same exe function'),
                             ('_rwInvSqrt', 'RwReal', 'RwReal x', 'core', 'alias of the same exe function'),
                             ('_rtFSInit', 'RtFileSystemError', 'RtFileSystem *fs, RwInt32 maxNbFiles, RwChar *fsName, RwChar *deviceName', 'toolkit', 'Ghidra name of rtFSInit (0x561900)'),
                             ('RwFread', 'size_t', 'void *addr, size_t size, size_t count, void *fptr', 'core', 'the exe function api_used calls RwFwrite at 0x560eb0 is this read (fsFileFunc.read)'),
                             ('RwFeof', 'RwInt32', 'void *fptr', 'core', 'RwFeof 0x561050'),
                             ('RwTexDictionaryCreate', 'RwTexDictionary *', 'void', 'core', 'RW 3.7 API the game does not call (0 sites); the asset oracle builds its merged dictionary with it'),
                             ('RwTexDictionaryAddTexture', 'RwTexture *', 'RwTexDictionary *dict, RwTexture *texture', 'core', 'RW 3.7 API the game does not call (0 sites); asset oracle'),
                             ('RwMalloc', 'void *', 'size_t size, RwUInt32 hint', 'core', 'engine memoryFuncs.rwmalloc (the game reads it from RwEngineInstance+0x108)'),
                             ('RwFree', 'void', 'void *mem', 'core', 'memoryFuncs.rwfree (+0x10c)'),
                             ('RwRealloc', 'void *', 'void *mem, size_t size, RwUInt32 hint', 'core', 'memoryFuncs.rwrealloc (+0x110)'),
                             ('RwRenderStateSet', 'RwBool', 'RwInt32 state, void *value', 'core-d3d9', 'device table RwEngineInstance+0x20 (exe 0x4e5be0); RW enum mapped to librw states'),
                             ('RwRenderStateGet', 'RwBool', 'RwInt32 state, void *value', 'core-d3d9', 'device table +0x24 (exe 0x4e5070)'),
                             ('RwIm2DRenderLine', 'RwBool', 'void *vertices, RwInt32 numVertices, RwInt32 vert1, RwInt32 vert2', 'core-d3d9', 'device table +0x28 (exe 0x4eb6d0)'),
                             ('RwIm2DRenderTriangle', 'RwBool', 'void *vertices, RwInt32 numVertices, RwInt32 vert1, RwInt32 vert2, RwInt32 vert3', 'core-d3d9', 'device table +0x2c (exe 0x4eb960)'),
                             ('RwIm2DRenderPrimitive', 'RwBool', 'RwInt32 primType, void *vertices, RwInt32 numVertices', 'core-d3d9', 'device table +0x30 (exe 0x4ebb80)'),
                             ('RwIm2DRenderIndexedPrimitive', 'RwBool', 'RwInt32 primType, void *vertices, RwInt32 numVertices, void *indices, RwInt32 numIndices', 'core-d3d9', 'device table +0x34 (exe 0x4ebdb0)'),
                             ('RwCalloc', 'void *', 'size_t numObj, size_t sizeObj, RwUInt32 hint', 'core', 'memoryFuncs.rwcalloc (+0x114)')]:
    if n not in protos: protos[n] = (rt, ar); src[n] = 'extra'; extra.append((n, mod, note))

idents = set()
for n, (ret, args) in protos.items():
    for t in re.findall(r'[A-Za-z_]\w*', ret + ' ' + args):
        if t not in KW and t != n: idents.add(t)
# drop parameter names: an ident that is lowercase-start and not a type
def is_type(t): return re.match(r'^(Rw|Rp|Rt|Rx|rw|rp|rt|rx|_rw|_rp)[A-Z0-9]', t) is not None
tys = sorted(t for t in idents if is_type(t) and t not in BASE and t not in protos)
cb = [t for t in tys if t in fptrs or re.search(r'(CallBack|Callback)$', t) and t not in enums]
alltxt = ' '.join(r + ' ' + a for r, a in protos.values())
byval = lambda t: re.search(r'\b' + t + r'\b(?! ?\*)', alltxt) is not None
en = [t for t in tys if t not in cb and (t in enums or byval(t))]      # enums and by-value flag typedefs: plain int
op = [t for t in tys if t not in cb and t not in en]

# hand-written definitions
defs = {}
for f in sorted(glob.glob(here + '/*.cpp')):
    if os.path.basename(f) in ('stubs_gen.cpp', 'layout_check.cpp'): continue
    lines = open(f, errors='replace').read().split('\n')
    for i, l in enumerate(lines):
        m = re.match(r'RWAPI [^(]*?\b(\w+)\(', l)
        if m:
            pl = lines[i - 1] if i else ''
            tag = 'exact' if 'L1:exact' in pl else 'librw' if 'L1:librw' in pl else 'oracle' if 'L2:oracle' in pl else 'impl' if 'L2:impl' in pl else 'untagged'
            defs[m.group(1)] = (os.path.basename(f), tag)
known = set(protos)
for d in defs:
    if d not in known and not d.startswith('rwmfl_'): print('WARNING: %s defined but not in the api list' % d, file=sys.stderr)

H = ['/* GENERATED by capi/gen_facade.py from output/rwsurf/api_used.csv + RW 3.7 SDK headers (interface only). Do not edit.',
     '   RenderWare 3.7 C ABI of mflrw.dll: cdecl, undecorated names.  Opaque handles stay opaque; callbacks are void*. */',
     '#ifndef RWMFL_H', '#define RWMFL_H', '#include <stddef.h>', '#ifdef __cplusplus', 'extern "C" {', '#endif',
     '#define RWAPI /* cdecl, exported through rwmfl.def */',
     'typedef float RwReal; typedef int RwBool; typedef signed char RwInt8; typedef unsigned char RwUInt8; typedef short RwInt16;',
     'typedef unsigned short RwUInt16; typedef int RwInt32; typedef unsigned int RwUInt32; typedef char RwChar;',
     'typedef long long RwInt64; typedef unsigned long long RwUInt64;',
     'typedef struct RwV2d { RwReal x, y; } RwV2d;', 'typedef struct RwV3d { RwReal x, y, z; } RwV3d;',
     'typedef struct RwMatrix { RwV3d right; RwUInt32 flags; RwV3d up; RwUInt32 pad1; RwV3d at; RwUInt32 pad2; RwV3d pos; RwUInt32 pad3; } RwMatrix;',
     'typedef RwReal RtBaryTransform[16]; /* RtBaryV4d[4]; decays to a pointer as a parameter */',
     'typedef struct RwRGBA { RwUInt8 red, green, blue, alpha; } RwRGBA;',
     'typedef struct RwSphere { RwV3d center; RwReal radius; } RwSphere;', 'typedef struct RwLine { RwV3d start, end; } RwLine;',
     'typedef struct RwBBox { RwV3d sup, inf; } RwBBox;', 'typedef struct RwPlane { RwV3d normal; RwReal distance; } RwPlane;']
H += ['typedef struct %s %s;' % (t, t) for t in op]
H += ['typedef int %s;' % t for t in en]
H += ['typedef void *%s;' % t for t in cb]
H += ['', '/* unimplemented entries are call-counting stubs; rwmfl_stub_count/info report them */',
      'int RWAPI rwmfl_stub_count(void);', 'int RWAPI rwmfl_stub_info(int idx, const char **name, unsigned *hits);',
      'RwUInt32 RWAPI rwmfl_get_fpucw(void);', 'struct RwGlobalsShadow; extern struct RwGlobalsShadow * RWAPI RwEngineInstance; /* RW 3.7 RwGlobals layout shadow (capi/core.cpp) */',
      'void * RWAPI rwmfl_engine(void); /* test hook: the librw rw::engine pointer */', 'int RWAPI rwmfl_layout_check(void); /* number of RW 3.7 layout mismatches (layout_check.cpp) */', '']
for n in sorted(protos):
    ret, args = protos[n]
    H.append('%s RWAPI %s(%s);' % (ret, n, args))
H += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
open(here + '/rwmfl.h', 'w').write('\n'.join(H))

D = ['LIBRARY mflrw', 'EXPORTS']
D += ['    %s' % n for n in sorted(protos)] + ['    RwEngineInstance DATA'] + ['    rwmfl_stub_count', '    rwmfl_stub_info', '    rwmfl_get_fpucw', '    rwmfl_layout_check', '    rwmfl_engine']
open(here + '/rwmfl.def', 'w').write('\n'.join(D) + '\n')

S = ['// GENERATED by capi/gen_facade.py: call-counting stubs for the entries without a hand-written definition.',
     '#include <stdio.h>', '#include "rwmfl.h"', '']
stubs = [n for n in sorted(protos) if n not in defs]
S.append('static unsigned hits[%d]; static const char *const names[%d] = {' % (max(len(stubs), 1), max(len(stubs), 1)))
S += ['\t"%s",' % n for n in stubs] + ['};', 'static void hit(int i){ if (++hits[i] == 1) fprintf(stderr, "[mflrw] stub called: %s\\n", names[i]); }',
      'extern "C" {', 'int RWAPI rwmfl_stub_count(void){ return %d; }' % len(stubs),
      'int RWAPI rwmfl_stub_info(int i, const char **n, unsigned *h){ if (i < 0 || i >= %d) return 0; if (n) *n = names[i]; if (h) *h = hits[i]; return 1; }' % len(stubs)]
for i, n in enumerate(stubs):
    ret, args = protos[n]
    # unnamed parameters are legal in a C++ definition; copy the prototype with the names stripped is not needed: keep them
    pa = args
    if ret.strip() == 'void': body = '{ hit(%d); }' % i
    elif ret.strip() in ('RwReal', 'float'): body = '{ hit(%d); return 0.0f; }' % i
    elif '*' in ret or ret.strip() in ('int', 'RwBool', 'RwInt32', 'RwUInt32', 'RwInt8', 'RwUInt8', 'RwInt16', 'RwUInt16', 'RwChar', 'unsigned', 'size_t') or ret.strip() in en + cb: body = '{ hit(%d); return (%s)0; }' % (i, ret)
    else: body = '{ hit(%d); static %s z; return z; }' % (i, ret)
    S.append('%s RWAPI %s(%s) %s' % (ret, n, pa, body))
S += ['}', '']
open(here + '/stubs_gen.cpp', 'w').write('\n'.join(S))

with open(covout, 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['api', 'module', 'orig_call_sites', 'status', 'verified', 'proto_source', 'librw_has', 'note'])
    for r in rows + [dict(name=n, module=m, call_sites='0', in_librw='y', librw_equivalent_or_note=nt) for n, m, nt in extra]:
        n = r['name']
        if n in defs:
            tag = defs[n][1]
            st, ver = 'Y', {'exact': 'disasm-exact', 'librw': 'librw semantics, bit check pending (L2)', 'oracle': 'asset-parity oracle through mflrw.dll',
                         'impl': 'implemented, not compared against the original', 'untagged': 'untagged'}[tag]
        else:
            st, ver = 'stub', ''
        note = {'RwFwrite': 'api_used.csv calls the exe function at 0x560eb0 RwFwrite; that one READS (= RwFread below); this RwFwrite is 0x560ee0'}.get(n, r['librw_equivalent_or_note'].replace('\n', ' '))
        w.writerow([n, r['module'], r['call_sites'], st, ver, src[n], r['in_librw'], note])
print('protos', len(protos), 'hand-written', len(defs), 'stubs', len(stubs), 'opaque types', len(op), 'enums', len(en), 'callbacks', len(cb))
