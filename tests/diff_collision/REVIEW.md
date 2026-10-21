# REVIEW: librw-mfl collision component (patch 0014) vs MFL.exe

Reviewer method: Ghidra decompile (read-only scratch project) of the statically linked RpCollision/RtIntersection/
RtBary code (0x549290-0x54ef00, 0x557860-0x558220, 0x569110, 0x569610) and of all 23 game functions calling them plus
their callbacks; a float32 port of the original traversal and tests (`colldiff.cpp`, `orig*` functions) linked
against the fork's objects; an original-control-flow tree reader in Python (`orig_coll.py`). Everything in
/tmp/claude-1000/collreview (`run.sh` reproduces; decompiles in coll.c, rt.c, callers.c, callers2.c, cbs.c).
Precise semantics for the fixer: SPEC_COLLISION.md.

## Verified correct (no action)
- Tree parse: fork dump == original-flow reader dump for all 126 BSPs, 1080 sectors, 217 trees (flags, bbox
  inf/sup order, numEntries, numSplits, every node byte/value, map). Node encoding (axis = type&0xC, left `<=`
  left.value / right `>=` right.value, 0xFF = split, leaf = map range) matches the original queries and builder
  (builder writes left.type = axis|1, right.type = axis).
- Callback order for sphere and box queries: original = plane tree left first, kd left first (incl. the untested
  contiguous range of fully contained nodes); fork's stream order + left-first kd gives the identical sequence in
  164000/164000 sphere and 164000/164000 box queries.
- Hit sets for sphere and box: identical to the original's RtIntersectionSphereTriangle / RtIntersectionBBoxTriangle
  + fully-contained shortcut on 163858/164000 and 163619/164000 queries; every remaining difference is a degenerate
  triangle (finding 9). The kd/plane culling therefore loses nothing.
- Line distance = fraction along the segment (agrees on 78494/78778 common hits; the 284 others are grazing lines,
  cos(angle) ~1e-5, float32 vs double).
- Normal = normalize((v1-v0)x(v2-v0)): 0 mismatches. Index, vertex pointers, early stop on NULL: match.
- Geometry without a tree: brute force over all triangles - matches the original.
- RtIntersectionLineTriangle argument list (start, delta, v0, v1, v2, &dist) and the dist meaning (fraction);
  RtLineSphereIntersectionTest arg list (line, sphere, &dist); RtBaryGetTransform arg list (matrix, &scalar, v0, v1, v2).

## Findings

| # | Sev | Area | Expected (original) | Actual (fork) | Evidence |
|---|-----|------|---------------------|---------------|----------|
| 1 | HIGH | line queries + RtIntersectionLineTriangle are two sided | One sided: hit only if `det=(d x e2)·e1 > 1e-8`, i.e. the segment enters the FRONT face (d·((v1-v0)x(v2-v0)) < 0) | Moeller-Trumbore with `det != 0`: back faces hit too | 0x549370 inline test / 0x557860 `if (1e-8 < det)`. World: fork reports 157195 hits vs 79556; 78416 extra hits, all back faces; set differs in 60723 of 107806 hit queries (56%); nearest hit differs in 55442 of them (51%). Rt: 5604/1e6 random cases fork-only, all back faces. Game: ground probe (car_probe_ground_distance), shadow first hit, line of sight, camera, weapons, light sampling all pick the nearest/first hit -> backfaces (undersides, inner walls) would now block/ground the car. |
| 2 | HIGH | RtBaryGetTransform result | 4x4 use of the RwMatrix: columns 0,1,2 = barycentric weights of v0,v1,v2 (row vector [x y z 1]*M), column 3 (m3,m7,m11,m15 = flags/pad words) = plane distance; `*scalar = |n|` (0 if degenerate, threshold |n|^2 <= 2.44e-4); returns FALSE but still writes the matrix when degenerate | 3x3 + pos giving (u,v,w) = (weight of v1, weight of v2, normal offset), flags = TYPENORMAL, `*det = |n|^2`, exact `== 0` degeneracy | 0x569610; course_world_ray_hit_light_sample 0x4891b0 blends vertex prelight with m[i]x+m[4+i]y+m[8+i]z+m[12+i], i=0..2. Differential: game-style weights wrong in 1000000/1000000 random triangles, scalar differs in 999991. (The orig model reproduces the true barycentrics in all cases.) The car light factor would use the wrong vertex colours. |
| 3 | HIGH | sphere query distance | `((c - v0)·n̂) / r`: signed plane distance in radius units, [-1,1] | `|c - closest point|`, absolute units, >= 0 | 0x54a4d0 `local_2c4 * local_288` (1/r), RtIntersectionSphereTriangle writes `-n̂·(v0-c)`. 7617249/7617318 common hits differ (max 31). Used by FUN_004c4e20 (nearest sphere hit) and the collectors FUN_004c4be0/4c70 that store it. |
| 4 | MED | CollisionTriangle.point | v0 (first vertex) for every query type | line: hit point; sphere/box: closest point | all three world walkers and the geometry ones copy `*v0` into the point field (e.g. 0x549370 local_544..53c). 78778/78778 line, 5850403 sphere, 3801807 box hits differ. Game callbacks copy the struct (shadow 0x499cc0 copies 10 floats) but compute hit points from the distance, so visible impact is low; fidelity of the struct still wrong. |
| 5 | MED | line callback order | near-to-far: plane tree and kd tree both visit the child on the start side first, with the t interval clipped per child (`pad = 1e-6*max(|start|,|end|)` per axis) | sectors in stream order, kd always left then right | 0x549370. Even ignoring back faces (finding 1), the first reported common hit differs in 9621/60333 hit queries and the common-hit sequence in 9558/164000 queries. shadow_project's callback returns 0 at the FIRST hit, FUN_004c4b50 collectors are capped, so order is observable. Original first hit is the nearest one in 96.6% of hit queries. |
| 6 | MED | world sectors without a collision tree | skipped: no triangle ever reported | brute force over all sector triangles | 0x549370/0x54a4d0/0x549f10 `if (sectorColl != 0)`. All 44 GRAPH/GRAPHICS/MAIN/TREES BSPs (595702 triangles, 426 sectors) have no tree: fork reports 40495 line / 7.1M sphere / 4.8M box hits there, original 0. The collision BSPs have a tree in every non-empty sector, and the game's queries go to the collision worlds (they carry the prelight the light sampler reads), so this is latent for the shipped game but a behaviour change for any query on a render world. |
| 7 | MED | RtLineSphereIntersectionTest | ray from start (no end bound): TRUE if start strictly inside (`m2 < r^2`) or (`b > 0` and closest approach <= r); `*dist = b/|d|` = fraction of the CLOSEST APPROACH (`_rwV3dNormalize` returns 1/length) | segment test (rejects entry t > 1), start inside inclusive, `*dist` = entry fraction | 0x569110 + 0x4d51f0. 1e6 random: 5009 original-only TRUE (all beyond the segment end), dist differs in 44423 of agreeing hits. Game callers use only the boolean (hud cursor lock, shotgun, cone targeting): targets beyond the line end are missed by the fork. |
| 8 | MED | box query distance | always 0 | `|centre - closest point|` | 0x549f10 / 0x54bdc0 `(*cb)(isec, sector, &tri, 0, data)`. The game issues no box queries (only types 1 and 3; FUN_004c4ba0..4c70, 4c4de0, 4c4e20, 4c4f90 set the type), so impact nil. |
| 9 | LOW | degenerate triangles | sphere: reported (normal 0, dist 0) when in a fully contained kd node; box: reported when a vertex is inside (or fully contained node); line: never (det <= 1e-8) | always rejected (`ddot(n,n) == 0`) | 285 sphere / 824 box original-only hits, all zero-area triangles (checked). Car sphere callback 0x477170 would receive them. Header comment says "zero for degenerate triangles" but they never reach the callback. |
| 10 | LOW | numSplits == 0 trees | valid: root is a leaf `{contents = (u8)numEntries, index 0}`; produced by the original builder (< 4 triangles, or no split and < 240) | `fromBlob` rejects `nent && nsplit == 0` (a geometry/sector with such a tree fails to load); `queryTree` returns nothing for such a tree; `build` never makes one | 0x549370 etc. `if (numSplits == 0) contents = numEntries`; 0x54e4c0. No such tree in the data (latent). |
| 11 | LOW | line triangle tolerance | inclusive with relative slack: u, v, t >= -1e-5*det, u+v and t <= det*(1+1e-5); distance may be slightly < 0 or > 1 | exact [0,1] | 778 original-only line hits (non degenerate, borderline) in 164000 queries. |
| 12 | LOW | atomic queries | box (type 4): no-op; type 5 supported; radius *= |inv.at|; morph targets > 1 use the interpolated geometry; callback gets the caller's (world space) RpIntersection pointer | box via AABB of transformed corners; type 5 rejected (nil + error); |inv.right|; morph target 0; callback gets the local copy | 0x54cbe0, 0x54d2d0. Game uses only type 1 on vehicle parts (callback ignores the isec arg). |
| 13 | LOW | unsupported types | world/geometry: type 5 = sphere of the atomic's world bounding sphere; types 0/2: nothing, return world/geometry, no error | type 2/5 rejected with RWERROR and nil | 0x549290 / 0x54acd0. Not used by the game. |
| 14 | LOW | API naming for the game shim | game calls 0x54d6a0 = RpCollisionGeometryQueryData (RwBool) and 0x54d670 = RpCollisionGeometryDestroyData (returns geometry) | MFL_NOTES/api_used treat them as WorldSector Query/DestroyData | decompile reads `_rpCollisionGeometryDataOffset` (0x7da6f0); call sites vehicle_load / vehicle_prepare_parts / vehicle_part_set_effect pass `atomic->geometry`. The fork already has the geometry functions; only the mapping (and api_used.csv) is wrong. |
| 15 | LOW | build params | `RpCollisionBuildParam = {RwInt32 flags (bit0 sort triangles); callback(geo, tree, remap, data); data}`, all zero = default; the game passes NULL; leaf < 4 triangles, scored split search (SPEC 4) | invented `{maxEntriesPerLeaf}` default 2, median split | 0x54da20/0x54da80/0x54e4c0. The FID "RpCollisionBuildParamInitDefaults" at 0x5d09d0 is a COM refcount object helper, not a collision call. Tree shape only affects callback order of geometry queries; the game's atomic callback is a min-distance one, so impact nil. |
| 16 | INFO | stream write/version | leading u32 = engine version; headers stamp 0x37002/build 0x24 (0x1C020024); reader accepts any version >= 0x36001 (older -> pre-3.6 converter) and uses FindChunk | writes 0x37002 + 0x1C02000A; reader requires exactly 0x37002 and fixed nesting | 0x54d870, 0x54dce0, 0x54d8d0. Data only has 0x37002; fine for the game. |
| 17 | INFO | precision | float32 x87 code; normals via 12 bit `_rwInvSqrt` table (~1e-4) | double internally | boundary cases only (grazing line distances, ties). |

## Answers to the implementer's guesses (MFL_NOTES "Collision" 1-10)
1. Sphere distance: signed plane distance / radius (F3); box distance 0 (F8); point = v0 always (F4).
2. Lines are ONE sided (F1) with inclusive, slightly widened borders (F11).
3. RtLineSphereIntersectionTest(line, sphere, &dist): arg list right, semantics wrong (F7). RtBaryGetTransform(matrix,
   &scalar, v0, v1, v2): arg list right, contents wrong (F2); the 4th "vertex" is not used - column 3 is the plane
   distance and the scalar is |n|.
4. Order: sphere/box = fork order (verified); line = near-to-far (F5); not sorted by distance.
5. Precision: borderline only (F17).
6. No tree: world sector skipped (F6); geometry brute forced (fork correct).
7. Types: game uses line and sphere on worlds, line on atomics only; atomic box is a no-op in the original (F12).
8. Game calls the GEOMETRY QueryData/DestroyData (F14); the world sector ones are unused.
9. Builder differs (F15), valid; numSplits==0 trees must be accepted (F10).
10. Stamps: see F16; masking in the round trip test is fine.

## Fixer priorities
F1, F2, F3 (gameplay/visual), then F5, F7, F4, F6, F10, F9; the rest is cosmetic or unused by the game.
For F1/F5/F3 the simplest faithful fix is to port the original walkers as in SPEC 2.1-2.4 (float32), keeping the
fork's validation; re-run `colldiff diff` (expect setEqual/seqEqual/firstEqual = queries, distBad ~0) and
`colldiff rt` (expect only-fork/only-orig 0 and weights differ 0).
