# Rt2d review (librw-mfl patch 0012) against MFL.exe

Reviewer: independent, 2026-10-19. Implementation not edited. Read: src/rt2d.cpp, src/rwrt2d.h, MFL_NOTES.md (Rt2d section),
game callers in ghidra/recomp (ui_text_draw_a/b/wide_a, hud_label_draw_text, ui_rt2d_init, rt2d_font_load, ui_2d_begin,
app_state_menu_render). Original routines decompiled read-only from the scratch project copy (`rt2d.c`, range 0x5609c0-0x567800,
`misattr.c`, `xref.c`) plus objdump of MFL.exe for the FPU details (`flow.s`, `show.s`, `d3d.s`).

## Method
Differential, same as the PI-TXD/BSP reviews:
- `orig_reader.py`: Python model of the original control flow (Rt2dFontStreamRead 0x564E40, GetStringWidth 0x565F80,
  texture show 0x5653F0, Flow 0x566060, BrushSetRGBA 0x564B10, _rt2dCTMGet 0x563410 device mapping, flatten not modelled).
  It models the game's float arithmetic as x87 with precision control = 24 bit (see F2 for why).
- `rt2ddump.cpp`: links the fork's objects (built with the fork's own tests/build_rt2d_test.sh into `obj/`), same output format.
- Per font: header, all glyph records, texture names, glyphIndex for codes 0..0x2FF and 0xFFFF, widths of 14 strings x 4
  heights, Show quads (screen x/y, u/v bits, RGBA) with a 4-colour gradient brush, Flow quads for 14 strings x 3 box widths x
  3 formats (line breaks, alignment, line y, return value) with a flat brush, and measureFlow line counts.
  Strings cover spaces/double spaces/leading spaces, `\n`, empty lines, a word longer than the box (endless-repeat case),
  >64 glyphs (batch split), Latin-1 high chars, INGAMEFONT's 141+ range, unmapped codes >= 0x100, empty string, tab.
- `run_diff.sh` then `cmp.py d/N.lib d/N.orig` (screen positions compared with 0.01 px tolerance, everything else exact).
- `fix/rt2d.cpp` = copy of the fork with the corrections from SPEC_RT2D.md section 9; `rt2ddump_fix` built from it.

Result over all 13 shipped RWF files (2753 glyphs):

| Record | fork vs original model | fork + spec fixes vs model |
|---|---|---|
| header, glyph table (bit exact), texture names, char map 0..0x2FF + 0xFFFF | identical | identical |
| Flow: return value, number of lines, line positions (x incl. alignment, y), glyphs per line | identical | identical |
| measureFlow line counts (incl. endless-word case = 65537 in both) | identical | identical |
| Show: quad count, vertex order, screen x/y, u/v, pos->x advance | identical | identical |
| Show: gradient vertex colours | 802-2826 vertices per file off by exactly 1 | identical |
| GetStringWidth (56 values per file) | 20-40 values per file off by 1-5 ulp | identical |

## Findings

| # | Severity | Area | Expected (original) | Actual (fork) | Evidence |
|---|---|---|---|---|---|
| F1 | Medium-low | Show gradient colours | Each channel = `trunc(float(base + float(delta*t)))` - the game converts with fistp after setting RC=chop (`or ah,0x0C; fldcw; fistp` at 0x5656a0.. in 0x5653F0) | `lrintf(a + (b-a)*t)` = round-to-nearest. Up to +1 per channel on every gradient vertex | differential: all colour diffs are exactly 1, 0 after the fix. HUD labels use 4-colour brushes (hud_label_draw_text -> Rt2dBrushSetRGBA(c0..c3)), so it is reachable |
| F2 | Low | GetStringWidth / Show t / spanWidth precision | Game runs with x87 precision control 24 bit (D3D9 device created without D3DCREATE_FPU_PRESERVE: behaviour flags at 0x4d83d5-0x4d8420 are only 0x4 and 0x20/0x40/0x50), so every add/mul rounds to float: `w = float(float(adv+gap) + w)` per glyph, result `float(w*h)` | sum in double, rounded once, then times height | 20-40 of 56 widths per file differ by 1-5 ulp; 0 after fix. Visually negligible; it can only flip a wrap/centre decision at an exact boundary. (If precision control were 64 bit instead, the fork would still differ in 7/56 values, so it matches neither.) |
| F3 | Low | Show accumulation order | `x = float(float(x + intergap) + adv)` | `x += float(intergap + adv)` | asm 0x565bc7-0x565bda. Only differs when intergap != 0; the game never sets intergap (see F9) |
| F4 | Low | Rt2dDeviceSetCamera edge cases | `nil` -> device camera cleared, returns 0. Camera without frame buffer -> returns 0 and changes nothing (previous camera kept) | `nil` returns 1; no-frame-buffer case stores `devCamera = cam` before failing | 0x563030 |
| F5 | Low | Default Im2D sink render states | Game draws through Im3D with flags 5 (VERTEXUV + NOCLIP, *not* ALLOPAQUE) at camera-space z = 1.0 and sets only TEXTUREFILTER (texture filterAddressing & 0xFF) and TEXTURERASTER, only when the texture changes; its callers turn ZTEST *and* ZWRITE off around HUD/menu text (app_state_menu_render) | sink forces CULLNONE and ZTEST off, leaves ZWRITE and VERTEXALPHA as found. With a texture without alpha the brush alpha would not blend (game: Im3D without ALLOPAQUE blends vertex alpha) | 0x5653F0 RwIm3DTransform(...,5); xref.c |
| F6 | Info | Path close | Rt2dPathClose appends a closing lineto to the first point when the last point differs, then marks closed | only marks closed (clip path has 4 points instead of 5) | 0x5639a0. No effect on GetBBox, the only consumer in the game |
| F7 | Info | Curveto flattening (guess 2) | adaptive de Casteljau, max 7 levels, flat when both control points are within sqrt(tol) of the chord; tol = squared length of half a pixel mapped into CTM space | fixed 16 segments | 0x563e70, 0x562e60. Game never calls curveto; only GetBBox of curved paths would differ marginally |
| F8 | Info | Font texture lookup | RwTextureRead(name) with the font dictionary made current: dictionary hit, otherwise the normal texture read path (load from disk, add to the dictionary); Rt2dOpen creates a default font dictionary | `find` only, default dictionary nil | 0x564e40, 0x565e10. Fine for the port as long as the TXD is set first (documented) |
| F9 | Info | api_used "rwp physics" rows | 0x56b2f0 (byte-identical to Rt2dFontSetIntergapSpacing: `*(p+0xC)=v`) is called from rwp_world_create_once with the world's first field; 0x571840 is a `*(p+4)=v` setter called from rwp_world_create_once, 0x56e7d0 is the same `*(p+4)=v` body inside the rwp library range (no direct code xref found; api_used counts its 5 calls from one function); 0x56bf00 is a destructor thunk (-> 0x56c0a0) used by refcounted anim objects (0x58cbe0, 0x5b6420). The real Rt2d font/object/anim code lives in 0x562d20-0x5677f0 | not implemented | misattr.c, xref.c. Implementer's conclusion confirmed: none of the four is Rt2d; Rt2dObjectSetDepth/Rt2dAnimSetDeltaTimeScale/rt2dAnimPropsStreamGetSize need nothing; the real intergap setter is unused by the game |
| F10 | Info | rwrt2d.h comment | batches hold up to 64 quads | header says "<= 63 quads" (code correctly flushes at 64) | game flushes when numIndices > 378 before adding a quad -> 64 quads (0x2400-byte vertex buffer = 256 vertices, 384 indices) |
| F11 | Low (library) | StreamMemory::read8 / write8 | - | (a) confirmed: `memcpy(data, &this->data[pos], 0)` with `this->data == nullptr` (stream opened on an empty buffer) is UB, UBSan flags it; (b) also `position + len` is uint32 and can wrap for a huge `len`, so the bounds check passes and memcpy runs with a ~4 GB length (same in write8) | src/base.cpp:970-982, 952-968. Fix: return early when `l == 0`; compare with `len > length - position` (position <= length is guaranteed when not EOF) |

## Guesses list, verdicts
1. Rotation direction: **correct.** Rt2dCTMRotate = RwMatrixRotate(axis +z, degrees, PRECONCAT). The device mapping (x scaled by -unit
   into an RW camera, whose +x points left on screen; y by +unit) makes Rt2d space a plain x-right/y-up frame, so +angle is
   counter-clockwise on screen. Fork's `{c, s, -s, c}` pre-concatenated matches.
2. Curve flattening: differs (F7), unused.
3. Render states: see F5. Cull/ztest forcing is harmless for the game's own call sites (they disable ztest+zwrite), but
   VERTEXALPHA should be enabled while drawing and ZWRITE handled the same way as ZTEST.
4. Depth z: g_rt_948240 ("layer depth") is written only in _rt2dGstateOpen (= 1.0); nothing else writes it. Text sits at
   camera-space z = 1 in the game. Drawing Im2D at near z with ztest off is equivalent for the HUD/menu paths.
5. Frame-sync callback swap in SetCamera: only chains to the old callback (0x563120 jumps through the saved pointer);
   omitting it is fine.
6. Scale factor for parallel projection: local factor = 1.0 for parallel, else layer depth = 1.0, so always 1.0. **Correct.**
7. Intergap setter call from rwp_world_create_once: confirmed mis-attribution (F9).
8. 8-bit fonts: decode/width/flow/show follow the same code with a byte step and a zero high-byte mask; fork matches.
9. Special-char mapping: game code, correctly out of scope.

## Confirmed correct (path-by-path, besides the differential)
- Stream read: version top byte != 0; header type, f(+4), baseline(+8), intergap(+0xC), flags(+0x10); then highBase, numGlyphs,
  numHigh (file order); highMap (numHigh int16) only if numHigh > 0; lowMap 128 int16; glyph = 4 floats uv (u0,v0,u1,v1),
  float advance, 1 byte texture index (signed char in the game); int32 texture count; 32-byte names.
- Glyph lookup: code < 0x80 -> lowMap; else highMap[code-highBase] if in range; negative = none.
- Show: CTM push, translate(pos), scale(h,h), translate(0,-baseline); quad corners (x,0)/(x,1)/(x+adv,0)/(x+adv,1) with
  uv (u0,v1)/(u0,v0)/(u1,v1)/(u1,v0); indices (0,2,3),(0,3,1); new batch on texture change or at 64 quads; pos->x += x*h; pop.
- Brush: SetRGBA(bl, br, tr, tl); flat colour = 4th argument; gradient flag cleared only when all four equal; Create = white,
  refcount 1; Destroy frees when the count drops below 1 and returns 1 only then.
- Flow: lineW = w/h; alignment factor 0 / h/2 / h for formats 0/1/2; first line y = rect.y + rect.h - h; the alternating
  "first space of a run" break rule; '\n' breaks after itself (the newline char is part of the drawn span); leading spaces
  skipped after a break; no progress on an over-long word (repeats until it drops below rect.y, then returns NULL); the
  check happens after drawing the line.
- CTM: 32 matrices (push fails at depth 31), pop no-op at 0, all ops pre-concatenate; device mapping
  sx = x*unit/viewW*rasterW, sy = (1 - y*unit/viewH)*rasterH with unit = min(viewW, viewH), view = 2*viewWindow.
- Clip path corners: (W/u,0), (W/u,H/u), (0,H/u), (0,0) through the inverse CTM; bbox {minx, miny, w, h} initialised from
  the first point.

## Files
- /tmp/claude-1000/rt2dreview/orig_reader.py, rt2ddump.cpp, strings.txt, files.txt, run_diff.sh, cmp.py, d/ (outputs)
- /tmp/claude-1000/rt2dreview/fix/rt2d.cpp (validation copy only, not a deliverable patch), rt2ddump_fix
- /tmp/claude-1000/rt2dreview/rt2d.c, misattr.c, xref.c (decompiles), flow.s, show.s, d3d.s (disassembly)

Rerun: `cd /tmp/claude-1000/rt2dreview && ./run_diff.sh && for i in $(seq 1 13); do python3 -I cmp.py d/$i.lib d/$i.orig | head -1; done`
