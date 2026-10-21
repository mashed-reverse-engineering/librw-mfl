# Review: librw-mfl PI texture dictionary reader (src/pitexd.cpp, patch 0009)

Reviewer: independent, against the ORIGINAL MFL.exe (Ghidra read-only decompile of a scratch project copy, no edits to the fork).
Routines checked (MFL.exe VAs): RtPITexDictionaryStreamRead 0x55ff00, rtpitexdImage2TextureReadLegacy 0x560390,
rtpitexdTextureStreamReadPre 0x560760, RwImageStreamRead 0x4de740, RwImageAllocatePixels 0x4dd610, RwImageGammaCorrect 0x4de880,
RwImageSetGamma 0x4dea10 (+ device gamma float at 0x62e7b8 = 1.0), _rwStringStreamFindAndRead 0x4ea4b0, RwTextureSetName 0x4d5c70,
RwTextureCreate 0x4d5b90, _rwD3D9ImageFindRasterFormat 0x4ef150, _rwD3D9ImageFindFormat 0x4eef60, _rwD3D9CheckRasterSize 0x4eddd0,
_rwD3D9RasterSetFromImage 0x4ef240, _rwD3D9RasterLock/Unlock 0x4dfcd0/0x4e0040, callers 0x4c3d80 (file) / 0x4c3eb0 (memory).

## Method
* `orig_reader.py` - Python reader following the original control flow (FindChunk skipping, image allocation stride rule,
  palette after pixels, gamma, texture-struct pre-read, mip/automip raster flag rule, level loop min(rasterLevels, numImages),
  palette rewritten per level, D3D9 colour format choice).
* `pitxddump.cpp` -> `pitxddump`: links the fork's objects (built with the fork's own build script into `obj/`), reuses the
  test's in-memory raster driver, dumps per texture name/mask/filterAddressing/dims/depth/MIPMAP/AUTOMIPMAP, palette CRC and
  per-level pixel CRC. `run_diff.sh files.txt` + `cmp.py` compare it with `orig_reader.py dump` on all 139 .TXD/.CMP files
  (103 PI, 36 native skipped).

## Result over all 103 PI files (2495 textures, 13714 images)
| Field | Match |
|---|---|
| texture count, order, names, masks | 2495/2495 |
| filterAddressing (incl. V:=U copy) | 2495/2495 |
| dims, depth, MIPMAP, AUTOMIPMAP flags | 2495/2495 |
| palettes (4 and 8 bit) | 2315/2315 |
| pixels, 8 bit levels | 9749/9749 |
| pixels, 32 bit levels | 1203/1203 |
| **pixels, 4 bit levels** | **442/2759 equal (only by accident: 1-pixel-wide levels / uniform rows), 2317 differ** |
(3 extra "level count" diffs are an artefact of the dump: a 1x1 level whose only index is 0xCD looks unwritten to the dump heuristic.)

## Findings
| # | Severity | Finding | Evidence | Expected (original) | Actual (fork) |
|---|---|---|---|---|---|
| 1 | **High** | 4 bit images decoded as nibble-packed; the files (and the original) store **one palette index per byte** | RwImageAllocatePixels: bytes/pixel = (depth+7)>>3 = 1 for depth 4, stride = roundup4(w); RwImageStreamRead copies file rows straight into that buffer. Data: in all 2759 4-bit images file stride == roundup4(width) (never ~w/2) and every in-row byte is < 16. Rendering `SideAds` (TRACKS/AngelS/TEXTURES.TXD) both ways: `SideAds_orig.png` shows the "EMPIRE / supersonic" banner, `SideAds_fork.png` shows the left half smeared over the full width. | index(x,y) = row[y][x] (byte, 0..15) | index = nibble of row[y][x/2] (low = even x); 2317 of 2759 levels wrong, all 721 4-bit textures affected; minStride check (w+1)/2 too lax |
| 2 | Medium (backend, not data) | Opaque palettised textures get an alpha raster format on the librw D3D backends | _rwD3D9ImageFindFormat: palettised -> PAL\|888 unless a pixel actually used references a palette entry with alpha != 0xFF (then PAL\|8888); 32 bit -> 888 unless any alpha != 0xFF. librw d3d imageFindRasterFormat always returns PAL4/PAL8\|C8888 when P8 is supported -> natras->hasAlpha = 1 -> alpha blending enabled (d3ddevice.cpp). Original on the data: PAL4\|888 650, PAL4\|8888 71, PAL8\|888 1291, PAL8\|8888 303, 888 41, 8888 139. librw GL3 and d3d-without-P8 use Image::hasAlpha, which matches the original rule. | 1941 palettised textures opaque | all 2315 palettised marked alpha on d3d with P8 |
| 3 | Low (latent, not in data) | Single-level texture with a filter mode other than NEAREST(1)/LINEAR(2) | Raster type flags in the original: numImages>=2 -> TEXTURE\|MIPMAP; numImages==1 and filter in {1,2} -> TEXTURE; numImages==1 otherwise (0, 3..6, ...) -> TEXTURE\|MIPMAP\|AUTOMIPMAP (driver generates levels on unlock). Data: all 749 single-level textures use filter 1 (7) or 2 (742), so no difference today. | MIPMAP\|AUTOMIPMAP | no mip flags |
| 4 | Low (latent) | deviceId (high u16 of the header word) ignored | deviceId == 0 selects a legacy layout (fixed 0x48-byte record, see SPEC section 4). All files have deviceId 1. | legacy path | misparse -> clean failure |
| 5 | Low | Unicode name/mask strings (chunk 0x13) not accepted; stamp version not checked | _rwStringStreamFindAndRead accepts 0x02 and 0x13 (takes low bytes), skips other chunks; texture struct and strings rejected unless version in [0x35000, 0x37002]. Data: only 0x02, versions 0x37002 / 0x36003. | | fork fails on 0x13; accepts any stamp |
| 6 | Info | Fork is stricter than the original (fine, no fix needed) | Original uses FindChunk (skips unknown chunks), resamples mip images whose size differs from the level, ignores images beyond the raster's level count, does not cross-check chunk sizes, never validates depth or palette equality, leaks the half-built dictionary on failure; with numImages 0 it dereferences an empty array. Fork rejects all these cases, destroys on failure. | | |
| 7 | Info | Gamma correction: no-op, fork correct to skip it | RwImageGammaCorrect runs on every image (StreamRead never sets the "gamma corrected" flag), but the table comes from RwImageSetGamma(device gamma 1.0; only callers are engine start / image module open). Table computed with truncation (FPU control OR 0x0C00) of pow(i*0.0039215689f)*255+0.5 -> identity for all 256 entries (checked numerically). Alpha is never touched. | identity | none |
| 8 | Info | Filter/mipmap global state in the texture pre-read is irrelevant | rtpitexdTextureStreamReadPre sets RwTextureSetMipmapping/AutoMipmapping (filter 3..6 -> mip on, auto = !(flags & 0x10000)) only around RwTextureCreate(NULL) and restores both before returning; the raster is created later with explicit flags (finding 3). Bit 16 (set on all 749 single-level textures) is therefore only a hint; texture->filterAddressing = flags & 0xFFFF exactly like the fork. | | matches |

Related (outside the fork, same wrong assumption): `tests/pitxd_test.cpp` line 214 reference walk, `MashedFormats/pitexd.py`
`Image.rgba()` (4-bit branch) and `FORMATS.md` ("4 bit: two pixels per byte, low nibble = left pixel") - this is why the
self-validation passed. They need the same fix (index = byte).

## Verified correct (field by field)
header u16 count / u16 deviceId; per texture u32 numImages then IMAGE chunks; image struct w,h,depth,stride; pixels h*stride
then palette (4<<depth bytes, depth<=8) AFTER the pixels; TEXTURE chunk: struct u32 flags, V addressing 0 -> copy U,
filterAddressing = flags & 0xFFFF; name/mask truncated to 31 chars at the first NUL; extension -> texture plugin data;
MIPMAP iff numImages > 1 (for the data); mip level dims max(1, prev>>1) from level 0; palette of level 0 (all levels
identical in the data, the original effectively keeps the last level's); 8-bit and 32-bit pixels byte-exact.

## Fix summary (details in SPEC_PITXD.md)
1. depth 4: copy `w` bytes per row like depth 8 (optionally mask `& 15` or reject bytes >= 16); minStride for depth 4 = w.
   Fix the test's reference walk and MashedFormats the same way.
2. Choose the raster alpha the original way (rule in SPEC 3.4); for librw-d3d either store PAL\|C888 support or set the
   native hasAlpha from Image::hasAlpha() after creation.
3. numImages == 1 and (flags & 0xFF) not in {1,2}: request MIPMAP|AUTOMIPMAP.
4. Optional: deviceId 0 legacy record, chunk 0x13 strings, version range check.

Files: `orig_reader.py`, `pitxddump.cpp`, `run_diff.sh`, `cmp.py`, `stats.py`, `nib.py`, `d/` (per-file dumps), `diff.txt`,
`pitexd.c` `img.c` `d3d.c` `caller.c` (decompiler output, reviewer-only), `SideAds_orig.png` / `SideAds_fork.png`.
