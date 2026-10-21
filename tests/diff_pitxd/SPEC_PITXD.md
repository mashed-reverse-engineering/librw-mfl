# PI texture dictionary (chunk 0x23): format and original semantics

Derived from the behaviour of MFL.exe (RenderWare 3.7.0.2, D3D9 driver) and checked on all 103 shipped PI files.
Behavioural description only; no SDK code. All values little endian. Chunk header = u32 type, u32 size, u32 stamp.
Version of a stamp: `((stamp >> 14) & 0x3FF00) + 0x30000 | ((stamp >> 16) & 0x3F)` (0x1C02000A -> 0x37002, 0x1803FFFF -> 0x36003).

## 1. Container
The game opens a file or memory stream, finds chunk 0x23 (skipping any other chunks in front) and reads its body:
```
u32 word: numTextures = word & 0xFFFF, deviceId = word >> 16
deviceId != 0 -> current layout (section 2); deviceId == 0 -> legacy layout (section 4). All shipped files: 1.
```
The new dictionary is made "current" while loading (previous current restored at the end); each finished texture is added
to it in file order. On any failure the original returns NULL (and leaks); a reimplementation should destroy and return NULL.

## 2. Per texture (current layout)
```
u32 numImages                      original: signed, used as array size; must be >= 1 (0 crashes the original)
numImages x:
   find chunk 0x18 (IMAGE)         other chunk types in between are skipped
      find chunk 0x01 (STRUCT), 16 bytes: u32 width, height, depth, fileStride   (no version check)
      pixel rows, then palette      see 2.1
find chunk 0x06 (TEXTURE)
   find chunk 0x01 (STRUCT): version must be in [0x35000, 0x37002] else error; payload u32 flags
   string: name                    see 2.2
   string: mask
   chunk 0x03 (EXTENSION)          texture plugin data, read after the raster is attached (empty in all files)
```

### 2.1 Image payload (one mip level)
* In-memory image: bytesPerPixel = (depth + 7) >> 3, i.e. **1 for depth 4**, 1 for depth 8, 4 for depth 32;
  stride = roundup4(width * bytesPerPixel).
* Pixels: `height` rows of `fileStride` bytes. Row y of the image = first min(fileStride, stride) bytes of file row y
  (excess skipped; if the file row is shorter the rest is undefined). In all files fileStride == stride.
* **Depth 4: one palette index per byte (values 0..15), NOT nibble packed.** Verified: all 2759 4-bit images have
  fileStride == roundup4(width) and every byte inside the row width is < 16; padding bytes past `width` may be garbage.
* Depth 32: 4 bytes per pixel in R, G, B, A order.
* Palette: only when depth <= 8: (1 << depth) entries x 4 bytes (R, G, B, A) **after** the pixels (64 bytes for depth 4,
  1024 for depth 8). Other depths <= 8 than 4/8 are invalid (original would write through a NULL palette).
* Chunk size in the files is exactly 28 + h*fileStride + palette (the original does not check it).
* Gamma: the original gamma-corrects R, G, B (pixels for 32 bit, palette for 4/8 bit) of every image, but the engine gamma
  is 1.0 and the table is the identity, so the bytes are unchanged. Implement as no-op.

### 2.2 Strings (name, mask)
Search forward for chunk 0x02 (bytes as-is) or 0x13 (UTF-16: take the low byte of every 16-bit unit), skipping other
chunk types; each chunk header's version must be in [0x35000, 0x37002]. The original reads into a 128 byte buffer
(longer strings overflow it: reject > 128). Stored as a C string cut at the first NUL and truncated to 31 characters
(32 byte field incl. terminator). Files: NUL padded to 4 bytes, always chunk 0x02.

### 2.3 flags (texture STRUCT)
```
bits 0..7   filter mode (1 NEAREST, 2 LINEAR, 3 MIPNEAREST, 4 MIPLINEAR, 5 LINEARMIPNEAREST, 6 LINEARMIPLINEAR)
bits 8..11  U addressing, bits 12..15 V addressing (1 = WRAP)
bit 16      set on all single-level textures in the data: "no auto mipmaps" hint (see 3.2)
```
If V (bits 12..15) is 0, V := U. texture.filterAddressing = flags & 0xFFFF. (U == 0 stays 0 in this layout.)
Data (low 16 bits): 0x1104 x1744, 0x1102 x744, 0x1101 x7; bit 16 on 749 textures.

## 3. Raster creation (per texture, after the texture chunk's string reads)
### 3.1 Raster type flags
```
numImages >= 2                                   TEXTURE | MIPMAP                 (0x8004)
numImages == 1 and filter in {1, 2}              TEXTURE                          (0x0004)
numImages == 1 and filter anything else          TEXTURE | MIPMAP | AUTOMIPMAP    (0x9004)
```
Data: 1746 textures 0x8004 (filters 4 x1744, 2 x2), 749 textures 0x0004 (filters 2 x742, 1 x7); 0x9004 never occurs.

### 3.2 Global mipmap state
The texture pre-read toggles the global "mipmapping / auto mipmapping" switches (filter 3..6 -> on, auto = !(flags bit 16))
but only around creating the empty texture and restores them before returning. It has no effect on the result; do not model it.

### 3.3 Size
Raster size from level 0 (driver may clamp to max texture size / round down to pow2 / force square, then all levels are
resampled; 8-bit images are then recreated as 32-bit 8888, or 16-bit 1555 if that fails). All shipped textures are pow2
and well below limits, so no resampling happens with real hardware caps.

### 3.4 Colour format (D3D9 driver, 32-bit display) - decided from LEVEL 0 only
* depth 4/8: if all palette entries (16 / 256) have alpha 0xFF -> PAL4/PAL8 | 888. Otherwise scan the pixels: if any pixel's
  palette entry has alpha != 0xFF -> PAL | 8888, else PAL | 888. (Equivalent: alpha iff a USED index has alpha != 0xFF;
  librw `Image::hasAlpha()` computes exactly this.)
* depth 32: any pixel alpha != 0xFF -> 8888, else 888.
* Data: PAL4|888 650, PAL4|8888 71, PAL8|888 1291, PAL8|8888 303, 888 41, 8888 139.
The format's alpha bit is what the renderer uses to decide alpha blending, so "opaque" palettised textures must not be
reported as having alpha. (librw d3d backend with P8: keep storing PAL|C8888 if needed, but set the native hasAlpha from
`img->hasAlpha()`.)

### 3.5 Levels
For k = 0 .. min(rasterLevelCount, numImages) - 1: lock level k for write, set it from image k, unlock (the AUTOMIPMAP bit is
masked during this when numImages >= 2 so unlock does not regenerate). Level k size = max(1, w0 >> k) x max(1, h0 >> k);
an image of a different size is resampled (files: always exact). Images beyond the raster's level count are ignored.
Raster levels beyond numImages are left undefined (or generated when AUTOMIPMAP, single image case). Each level write also
writes the palette, so the last written level's palette wins (identical in all files). All multi-level textures in the data
have the full chain down to 1x1 (log2(max(w,h)) + 1 levels).

Then the raster is attached to the texture, the EXTENSION chunk is read with the texture plugin registry, and the texture
is added to the dictionary.

## 4. Legacy layout (deviceId == 0; not present in the data)
Per texture, no TEXTURE chunk and no extension:
```
72 raw bytes: char name[32], char mask[32], i32 numImages, u32 flags
numImages x IMAGE chunk (as 2.1, found with chunk skipping)
```
Raster flags as 3.1 using flags & 0xFF; texture created with defaults, name/mask set (31 chars), filter = flags & 0xFF,
U = bits 8..11, V = bits 12..15 with: U == 0 and V == 0 -> both 1 (WRAP); U == 0 -> U := V; V == 0 -> V := U.
Bits above 15 are not stored. The legacy loop ignores per-texture failures and continues.

## 5. Corrections for librw-mfl src/pitexd.cpp
1. **(High)** depth 4: treat exactly like depth 8 at the byte level: copy `w` bytes per row (librw's depth-4 Image is one
   index per byte already). Minimum stride for depth 4 = w (not (w+1)/2). Indices >= 16 do not occur; reject or `& 15`.
2. **(Medium)** Raster alpha per 3.4 (d3d backend currently marks all palettised rasters alpha).
3. **(Low)** numImages == 1 and filter not in {1,2}: request MIPMAP | AUTOMIPMAP.
4. **(Low, optional)** deviceId == 0 legacy record (section 4); chunk 0x13 strings; version window on the texture struct and
   string chunks.
5. Fix the test's reference walk (pitxd_test.cpp, 4-bit branch) and MashedFormats pitexd.py / FORMATS.md the same way, else
   the self-check keeps confirming the wrong decoding.
Everything else (header, counts, names, masks, flags incl. V:=U, dims, mip chain, palette position/size, 8/32-bit pixels,
extension handling) matches the original byte for byte on all 103 files.
