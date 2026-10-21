# Rt2d subset used by MFL - behavioural spec (from the game's own routines)

Written by the reviewer from the decompile/disassembly of MFL.exe (0x562d20-0x5677f0) and the 13 shipped RWF files. No SDK
source. Section 9 lists the exact changes the fork needs; everything else in the fork already behaves as described here
(verified by the differential in REVIEW.md).

## 0. Arithmetic model
The game creates its D3D9 device without D3DCREATE_FPU_PRESERVE, so the x87 runs with 24-bit precision control: every
add/sub/mul/div result is rounded to float (the exponent range is wider, irrelevant here). Model all Rt2d arithmetic as plain
IEEE float32 ops, one rounding per operation, in the operand order given below. On x86-64/SSE2 builds that is what
`float` code does; on 32-bit x87 builds (mingw i686 default) use `-mfpmath=sse -msse2` or force stores (volatile) at each step.
float -> byte colour conversion truncates toward zero.

## 1. RWF stream (Rt2dFontStreamRead; stream positioned after the 0x199 chunk header)
| Field | Type | Notes |
|---|---|---|
| version | u32 | fail if `version >> 24 == 0` |
| type | i32 | 0 = texture font (all shipped). Non-zero = outline font: per glyph i32, f32 advance, i32 kind; kind 5 is followed by a 0x1A1 path chunk; then i32. Not needed for MFL |
| size | f32 | default 1.0 before read; not used by width/show/flow |
| baseline | f32 | text is drawn shifted down by baseline (in units of height) |
| intergap | f32 | added after every mapped glyph in width, flow and show |
| flags | i32 | bit 1 (value 2) = 16-bit chars (all shipped fonts); else 8-bit |
| highBase | i32 | first char code of the high map (128; INGAMEFONT 141) |
| numGlyphs | i32 | |
| numHigh | i32 | |
| highMap | i16[numHigh] | present only when numHigh > 0 |
| lowMap | i16[128] | codes 0..127 |
| glyph x numGlyphs | f32 u0, v0, u1, v1; f32 advance; i8 texIndex | v0 = top edge, v1 = bottom edge |
| numTextures | i32 | game has 4 slots, no check |
| name x numTextures | char[32] | texture looked up with the font texture dictionary made current (dictionary, then normal texture read path) |
Every glyph's texture must exist and have a raster, else the read fails. Hardening (bounds on counts, map entries, texIndex,
numTextures <= 4) is fine and does not change accepted shipped data.

Character decode: 16-bit fonts read `p[0] | p[1] << 8` and step 2 bytes; 8-bit fonts read one byte. NUL ends the string.
Glyph lookup: `c < 0x80 ? lowMap[c] : (0 <= c-highBase < numHigh ? highMap[c-highBase] : none)`; negative value = none.
Unmapped characters contribute nothing anywhere (no width, no quad, no gap).

## 2. Rt2dFontGetStringWidth(font, text, h)
`w = 0; for each mapped glyph g: w = float(float(g.advance + intergap) + w); return float(w * h)`.

## 3. Rt2dFontShow(font, text, h, pos, brush) - texture fonts
1. CTM push; translate(pos.x, pos.y); scale(h, h); translate(0, -baseline). (If push fails the game carries on and corrupts the
   stack; a port may skip drawing.)
2. `W = width(text, 1.0)` (section 2 with h = 1, i.e. the plain float sum); `invW = float(1 / W)`.
3. `x = 0; acc = 0`. For each mapped glyph g (adv = g.advance):
   - batch: if the glyph's texture differs from the current one, or 64 quads are already batched, flush; on a texture change
     set TEXTUREFILTER = texture.filterAddressing & 0xFF and TEXTURERASTER = texture.raster.
   - vertices (Rt2d space, before the CTM): v0 = (x, 0) uv (u0, v1); v1 = (x, 1) uv (u0, v0);
     v2 = (xr, 0) uv (u1, v1); v3 = (xr, 1) uv (u1, v0), with `xr = float(x + adv)`. Indices (0,2,3), (0,3,1).
   - colours: flat brush -> every vertex = the brush's flat colour (4th SetRGBA argument). Gradient brush:
     `tl = float(invW * acc)`, `tr = float(invW * float(acc + adv))`; v0 = bottom(tl), v1 = top(tl), v2 = bottom(tr),
     v3 = top(tr); per channel `bottom(t) = trunc(float(B0 + float(dB * t)))` with B0 = 1st argument, dB = 2nd - 1st;
     `top(t) = trunc(float(T0 + float(dT * t)))` with T0 = 4th argument, dT = 3rd - 4th (deltas are exact float
     differences of the byte values). Result & 0xFF.
   - `x = float(float(x + intergap) + adv)`; `acc` the same.
4. Flush; `pos.x = float(float(x * h) + pos.x)` (pos.y unchanged); CTM pop.
W = 0 (only zero-advance glyphs) gives invW = inf and NaN t in the game (colour bytes 0); not reachable with shipped fonts.

## 4. Rt2dFontFlow(font, text, h, rect{x,y,w,h}, format, brush)
```
lineW = float(rect.w / h)
align = format==1 ? float(h*0.5) : format==2 ? h : 0
ly    = float(float(rect.h + rect.y) - h)
first = true; w = 0; lastW = 0; cur = start = brk = 0
loop:
  c = text[cur]
  if c == 0: show(text[start..end], h, (float(float(float(lineW - w) * align) + rect.x), ly)); return font
  if first and c == ' ': first = false; brk = cur; lastW = w
  else: first = true; if c == '\n': brk = cur+1; lastW = w
  if mapped(c): w = float(float(adv + intergap) + w)
  if lineW < w or c == '\n':
      show(text[start..brk), h, (float(float(float(lineW - lastW) * align) + rect.x), ly))   # '\n' itself is inside the span
      cur = brk; skip ' ' at cur
      ly = float(ly - h); first = true; brk = start = cur; w = 0
      if ly < rect.y: return NULL           # after drawing the line
  else: cur++
```
Notes: only the first space of a run of spaces (then every second one) becomes a break candidate; a word wider than the box
never makes progress and repeats until ly drops below rect.y (port may bound this). The game writes temporary NULs into the
caller's buffer; a port uses spans instead. The width used for the gradient t of each line is that line's span.
Formats: 0 left, 1 centre, 2 right.

## 5. Brush
Create: white, flat, refcount 1, no texture. SetRGBA(c0, c1, c2, c3) = (bottom-left, bottom-right, top-right, top-left);
gradient flag cleared only when all four are equal. Destroy: decrement; free and return 1 when the count is < 1, else 0.

## 6. CTM and device
- Stack of 32 matrices; Push fails (returns false, no change) when depth is already 31; Pop is a no-op at depth 0.
  SetIdentity resets the top. Translate/Scale/Rotate pre-concatenate (the new op is applied to points first).
  Rotate(deg) is a counter-clockwise rotation in Rt2d space (RwMatrixRotate about +z).
- SetCamera(cam): cam == nil -> clear the device camera, return 0. cam without frame buffer -> return 0, keep everything.
  Else viewW = 2*viewWindow.x, viewH = 2*viewWindow.y, raster size = frame buffer size, return 1.
- Mapping: unit = min(viewW, viewH); Rt2d point (x, y) after the CTM lands at camera-space
  (-unit*x + viewW/2, unit*y - viewH/2, 1) i.e. screen pixel `sx = x*unit/viewW*rasterW`, `sy = (1 - y*unit/viewH)*rasterH`.
  The layer depth is always 1.0 (only initialised), and the parallel-projection factor is also 1.0.
- Drawing in the game: Im3D, transform flags VERTEXUV|NOCLIP (vertex alpha honoured), z = 1 in camera space; render states
  set by Rt2d: TEXTUREFILTER, TEXTURERASTER only. HUD/menu callers disable ZTEST and ZWRITE around text.
  Port (Im2D): draw at near z with VERTEXALPHA on; disable ZTEST and ZWRITE (or leave both to the caller) and restore.
- GetClippath(path): empty the path; corners (viewW/unit, 0), (viewW/unit, viewH/unit), (0, viewH/unit), (0, 0) mapped through
  the inverse CTM; moveto, 3 linetos, close.

## 7. Path
- Create: empty. Moveto starts a new subpath (reuses the current one if it has no points). Lineto appends.
- Close: if the last point differs from the first, append a lineto to the first point; mark closed.
- Curveto(p1, p2, p3) from the current point p0: stored as a curve; flattened on demand by recursive midpoint subdivision,
  at most 7 levels: a segment is flat when both control points have squared perpendicular distance to the chord p0-p3 below
  tol (test: `(|pi-p0|^2 - tol) * |p3-p0|^2 < dot(pi-p0, p3-p0)^2`); flat or depth exhausted -> emit lineto p3.
  tol = squared length of the vector (viewW/rasterW * 0.5, 0) mapped from camera space into CTM space (half a pixel).
- GetBBox: start from the first point of the path, extend over all points (curves flattened first), return
  {minX, minY, maxX-minX, maxY-minY}.

## 8. Not Rt2d
api_used rows Rt2dFontSetIntergapSpacing@0x56b2f0, Rt2dObjectSetDepth@0x571840, Rt2dAnimSetDeltaTimeScale@0x56e7d0,
rt2dAnimPropsStreamGetSize@0x56bf00 are byte-matched generic setters/destructors in the rwp/anim range; nothing to implement.

## 9. Corrections for the fork (validated: with exactly these the differential is 13/13 identical)
1. `lerpc` (colour): `d = float((float)b - (float)a) * t; v = float((float)a + d); return (uint8)((int32)v & 0xFF);` -
   truncation, not lrintf.
2. `showSpan`: `xr = acc + adv` (float), `ts = { float(acc*invW), float(xr*invW) }`; advance with
   `acc = float(acc + intergap); acc = float(acc + adv)`.
3. `spanWidth`: accumulate in float: `a = float(adv + intergap); w = float(a + w)`; return w (getStringWidth then returns
   `float(w * height)`, which it already does).
4. `deviceSetCamera`: return 0 for nil (still clear the camera); check the frame buffer before touching `devCamera`.
5. Im2D sink: set VERTEXALPHA = 1 while drawing (restore after); treat ZWRITEENABLE like ZTESTENABLE.
6. Optional: Path::close appends the closing point; rwrt2d.h comment "<= 63 quads" -> 64.
7. Library (src/base.cpp StreamMemory::read8/write8): return 0 early when the clamped length is 0 (no memcpy with a null
   buffer), and bound with `len > length - position` instead of `position + len > length` (uint32 wrap).
