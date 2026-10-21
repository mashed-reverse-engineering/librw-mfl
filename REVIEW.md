# Review: librw-mfl BSP world loader (src/world.cpp, patch 0008) against MFL.exe

Reviewer: independent review, 2026-10-19. Loader code not modified. Scratch tools are in this directory.

## Method

1. **Decompiled the original routines** (Ghidra, project `mashed/mfl`, read-only):
   `RpWorldStreamRead` 0x4f9bd0 (in `world_rd.c`), `WorldSectorStreamRead` 0x4fa060,
   `PlaneSectorStreamRead` 0x4fa440, `_rpWorldSetupSectorBoundingBoxes` 0x4f5300, `RpWorldUnlock` 0x4f5420,
   `RpWorldForAllWorldSectors` 0x4f5e10, `_rwPluginRegistryReadDataChunks` 0x4f1ea0, `readSectorMesh` 0x4f43f0,
   the D3D9 sector instance callback 0x5005b0, the normal decode helper 0x506ad0, MatFX sector read 0x550120,
   and the game's own sector callbacks 0x495e30 / 0x495ec0 / 0x4890e0 (to see which sector fields the game uses).
   Outputs: `world_rd.c`, `world_more.c`, `n.c`, `mfx.c`, `game_cb.c`.
2. **Differential (in place of the Unicorn run).** The Unicorn emulator (tools/diffemu) needs the docker
   environment (capstone/unicorn/pefile are not installed on the host) and is not allowed in this review.
   Instead, `orig_reader.py` re-implements the original control flow 1:1 from the decompilation (chunk
   lookup by FindChunk, version gates, 44-byte sector header, u16 truncations, stream order, triangle
   rotation for old versions, plane value fallback, int8 normal decode by 1/128, plane-derived boxes).
   `bspdump.cpp` (a copy of the bsp_test shell, linked against the fork's objects built into `obj/`) dumps
   the librw World in the same canonical form: header, world bbox, inverse origin, plane tree preorder
   (type, value, leftValue, rightValue, child kinds), and per sector: matIdBase, counts, box, and FNV-64
   hashes of vertices, decoded normals, prelit colours, each UV set, and triangles with absolute material
   index. `run_diff.sh` compares the two dumps file by file.
3. **Bin mesh check.** `binmesh_check.py` compares each sector's 0x50E mesh (what the original draws)
   with the sector triangle array (what librw rebuilds from), per material.

Inputs: all 126 game .BSP files (85 FullyLoaded + 41 DriveToSurvive) plus 32 RW SDK sample .BSP files
(as extra data only).

## Results

| Check | Files | Result |
|---|---|---|
| Header, bbox, inv origin, plane tree, sector counts/base/box, vertices, colours, UV sets, triangles + material index | 126 game | **122 identical; 4 differ only in normals** |
| Same | 32 SDK samples | **all 32 differ only in normals** (all have NORMALS); every other stream identical |
| Bin mesh 0x50E vs triangle array (per material, unordered, degenerate removed) | 126 game, 1080 sectors | 0 mismatches |
| Stream fully consumed by original-logic reader | 126 game | 126 |

## Findings

| # | Severity | Finding | Evidence | Expected (original) | Actual (librw-mfl) |
|---|---|---|---|---|---|
| 1 | Low | Normal decode scale | 0x506ad0 converts each `int8` component with `* k_f_0_0078125` (1/128). Differential: normals hash differs in Arctic/AI.BSP and Arctic/COLLISIONS.BSP (both editions, fmt 0x40000071 / 0x40000079) and in all 32 SDK samples; every other stream is identical | `n = c / 128.0f` (a component of 127 gives 0.9921875) | `n = c / 127.0f` (world.cpp line 393). Unit normals are 0.79% too long; only visible when the pipeline does not renormalise |
| 2 | Medium (render) | MatFX sector flag dropped | 46 sectors in 7 files have a 0x120 extension with value 1: SandSnow/GRAPHICS, Tierra_N/GRAPH, Warzone/GRAPHICS, dump/GRAPHICS, rouabout/GRAPH, sands/GRAPHICS, training/GRAPH (formats 0x400200C9 / 0x40020089, 2 UV sets). 0x550120 reads an int32 and, if it is nonzero, attaches the MatFX world-sector pipeline | Those sectors render through MatFX (material dual-texture / env effects active) | Skipped in `readSectorExtension`; `WorldSector::render` uses a plain atomic, so the second pass is lost. Fix: store the flag, then call `MatFX::enableEffects(atomic)` on the sector atomic. Already listed as a known gap in MFL_NOTES |
| 3 | Low (semantics) | Plane-derived sector bounding box is not computed | 0x4f5300 runs after loading: each sector gets a `boundingBox` made from the world box, cut along the plane path (left child: `sup[axis] = leftValue`, right child: `inf[axis] = rightValue`). 499 of the 954 game planes have leftValue/rightValue != value, so these boxes overlap and differ from the tight box | Two boxes per sector: the file's tight box (memory +0x60) and a plane box (+0x48) | Only the tight box (as `bbox`). That is the box the game itself reads (0x495ec0 `course_objects_test_bbox(sector+0x60)`, 0x4890e0), so no gameplay effect found. Add `planeBBox` only if a port needs RpWorldSectorGetBBox semantics. The tight box was inside the plane box for every sector (0 violations) |
| 4 | Low | No version gate and no old-triangle layout | Every chunk read checks `0x35000 <= version <= 0x37002`. 0x4fa060: if the sector STRUCT version is `< 0x36002`, each triangle is rotated from file order `{w0,w1,w2,w3}` to `{w1,w2,w3,w0}` (old files store the material first) | Pre-3.6.0.2 sectors give `{v0,v1,v2,mat}` after the rotation; out-of-range versions are rejected | Version ignored; old files would decode wrong triangles (the index check would catch most of them). All 158 files are 0x37002, so there is no current effect |
| 5 | Low | Plane bounds when format bit 0x40000000 is clear | 0x4fa440: without `rpWORLDSECTORSOVERLAP` (0x40000000) `leftValue = rightValue = value` | value used for both | Always uses stored words 4/5. All files have the bit set |
| 6 | Info | `collSectorPresent` is garbage | Sector header word 9 holds format-like values (13, 73, 77, 201 = low byte of the format), float bit patterns and pointer-like values (56816764). Word 10 is always 0. The original reads both words and ignores them | Ignore | Stored as `collSectorPresent`. Rename it (e.g. `unused0`) and never test it; collision presence is the 0x11D extension |
| 7 | Info | Child chunk selection | The original picks the child chunk from the flags (world `rootIsWorldSector`, plane `left/rightIsWorldSector`) and uses FindChunk, which skips any other chunk in between. No plane extension is read | Tolerant of extra chunks | `readNode` trusts the next header and fails on any other chunk. Identical on all files. Stricter, acceptable |
| 8 | Info | `colSectorSize` must be <= 0 | `RpWorldStreamRead` errors when header word 8 >= 1 | Reject | Accepted. All files have 0 |
| 9 | Info (render) | Sector draw order | `RpWorldStreamRead` sets `renderOrder = BACK2FRONT`, and drawing walks the camera's frustum sector list | Camera-dependent order | `renderSectors` draws depth-first, left before right. This affects only translucent world materials. Render path, not the loader |
| 10 | Info | 0x50E bin mesh ignored | The original uses the stored mesh (tristrips: bit 0 is set in every game format) and builds one only when it is missing (0x4f5420) | Stored strips | Rebuilt as lists from the triangles. Proven equivalent per material on all 1080 sectors. Draw order of meshes inside a sector may differ |
| 11 | Info | Width limits | The original keeps `matListWindowBase`, `numVertices` and `numTriangles` as u16. Its tree walks use 64-entry stacks (0x4f5300, 0x4f5420, 0x4f5e10) | <= 65535 per sector, about 64 pending right children | nt is allowed up to 0x3FFFC and depth up to 512. Data max: 4089 triangles per sector |
| 12 | Info (docs) | MFL_NOTES format list incomplete | Formats 0x400200C9 and 0x40020089 (TEXTURED2, 2 UV sets, MatFX) occur in 7 game files | - | Notes list only 0x4001000D/41/49/71/79/4D. Decoding them is correct (differential identical) |

No correctness bug was found in header field order, sup/inf order (world sup-then-inf, sector tight
inf-then-sup), material index base, triangle word order (3.7 files), UV set count and selection,
prelit colour order, stream order of the vertex arrays, or plane type (0/4/8 = byte offset of x/y/z).
The differential matched exactly on every one of these streams for 158 files.
