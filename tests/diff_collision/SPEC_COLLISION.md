# SPEC_COLLISION: RpCollision / RtIntersection / RtBary as MFL.exe does them

Source: Ghidra decompile of MFL.exe (statically linked RW library code; FID names), read-only scratch project.
Addresses are MFL.exe VAs. "float" means IEEE single; the original is x87 code that stores every intermediate to a
float slot, so a float32 port in the same operation order reproduces it to within an ulp. A double precision port is
fine for the set of hits except on boundaries. Notation: `RwBBox = {sup, inf}` (sup first, as in librw `BBox`).
`RpIntersection = {union t (24 bytes); int type}`; type 1 line, 2 point, 3 sphere, 4 box, 5 atomic.
`RpCollisionTriangle = {V3d normal; V3d point; int index; V3d *vertices[3]}`.

## 1. Tree in memory and in the stream

In memory (`_rpCollTreeCreate` 0x54dbc0): `{u32 flags; RwBBox bbox (sup,inf); u16 numEntries @0x1C; u16 numSplits
@0x1E; split *splits @0x20 (16 byte aligned, NULL if numSplits==0); u16 *map @0x24 (NULL unless flags&1)}`.
numEntries and numSplits are truncated to u16. Split = 16 bytes = two 8 byte nodes `{u8 type; u8 contents; u16 index;
float value}`: left node at +0, right node at +8.

Stream (`_CollisionDataStreamRead` 0x54d8d0 / `_rpCollTreeStreamRead` 0x54de40), payload of plugin chunk 0x11D:
- u32 version. `< 0x36001`: pre-3.6 layout (`_rpCollTreeStreamReadPre36001` 0x54df40, not in the game data).
  Otherwise `RwStreamFindChunk(0x2C)`, `RwStreamFindChunk(STRUCT)` (other chunks are skipped), read 0x24 bytes:
  `u32 flags; f32 inf[3]; f32 sup[3]; u32 numEntries; u32 numSplits` (file has inf first, memory sup first), then
  numSplits*16 bytes of splits verbatim, then (flags&1) numEntries u16 map.
- No validation at all in the original (no range check, no entries==triangles check). Rejecting garbage is fine.
- **numSplits == 0 is valid**: the tree is a single leaf (see 2.1). The original builder produces such trees
  (fewer than 4 triangles, or no usable split and < 240 triangles).
- Write (`_rpCollTreeStreamWrite` 0x54dce0): leading u32 = `RwEngineGetVersion()`; chunk 0x2C size
  `(numSplits+3)*16 + (flags&1 ? 2*numEntries : 0)`, STRUCT size = that - 12, both headers via
  `_rwStreamWriteVersionedChunkHeader(.., 0x37002, build 0x24)` = stamp 0x1C020024. (The shipped data has 0x1C02000A /
  0x1803FFFF from the exporter; byte-exact round trip of the data only needs stamps carried through.)

Node semantics (both the queries and the builder): the axis of a split is `left.type & 0xC` (0 x, 4 y, 8 z); the
right node's type is never read by the queries. The left child covers `coord <= left.value`, the right child
`coord >= right.value`. Builder writes `left.type = axis|1`, `right.type = axis`. `contents == 0xFF`: child is a
split, `index` = split index. Else leaf: `contents` entries starting at entry position `index`; entry k means triangle
`map ? map[k] : k`.

Verified: the fork's parse of all 217 trees in the 126 BSPs is field-for-field identical to a reader following the
original control flow (orig_coll.py vs `colldiff dump`). Keep it; add the numSplits==0 case.

## 2. World queries (`RpCollisionWorldForAllIntersections` 0x549290)

Dispatch on type: 1 line (0x549370), 3 sphere (0x54a4d0), 4 box (0x549f10), 5 atomic = sphere query with
`RpAtomicGetWorldBoundingSphere(atomic)` as the sphere (callbacks still get the caller's RpIntersection), anything
else: nothing. Always returns the world (no error for unsupported types).

**Sectors without collision data (no 0x11D chunk / NULL tree) are skipped entirely: no triangle of them is ever
reported.** (All 44 graphics/MAIN/TREES BSPs have no trees; all 82 collision BSPs have a tree in every sector with
triangles. The game queries the collision worlds.)

Callback: `RpCollisionTriangle *cb(RpIntersection *is, RpWorldSector *sector, RpCollisionTriangle *tri, RwReal
distance, void *data)`; returning NULL stops the whole query immediately. The triangle struct handed over:
- `normal` = normalize((v1-v0) x (v2-v0)) (computed with the 12 bit `_rwInvSqrt` table; |n| = 1 to ~1e-4; zero
  vector for a degenerate triangle, `_rwInvSqrt(0) = 0`),
- **`point` = v0 (the first vertex), for every query type** (NOT a hit/closest point),
- `index` = triangle index in the sector (after the map), `vertices` = pointers into sector->vertices.

### 2.1 Line (type 1, WorldForAllTriIntsecLine 0x549370)
Setup: `s = start, d = end-start, inv[a] = d[a]==0 ? 0 : 1/d[a]`, `pad[a] = 1e-6 * max(|start[a]|, |end[a]|)`.
Traversal keeps a stack of `(node, t0, t1)`; plane tree first, root `(world root, 0, 1)`:
```
pop (n,t0,t1); while n is a plane sector (a = type/4):
  p0 = d[a]*t0 + s[a]; p1 = d[a]*t1 + s[a]; lhi = leftValue + pad[a]; rlo = rightValue - pad[a]
  if d[a] <= 0:                                   # moving towards -axis: right side is near
     if p1 <= lhi:
        if rlo <= p0:  near=(right,t0, p1<rlo ? t1+(rlo-p1)*inv : t1); far=(left, lhi<p0 ? t0+(lhi-p0)*inv : t0, t1)
                       push far; (n,t0,t1)=near
        else n = left
     else n = right
  else:                                           # left side is near
     if rlo <= p1:
        if p0 <= lhi:  near=(left,t0, lhi<p1 ? t1+(lhi-p1)*inv : t1); far=(right, p0<rlo ? t0+(rlo-p0)*inv : t0, t1)
                       push far; (n,t0,t1)=near
        else n = right
     else n = left
visit sector with (t0,t1); continue until the stack is empty
```
In a sector with a tree: same algorithm on the kd tree, starting from a pseudo root node
`{contents = numSplits ? 0xFF : (u8)numEntries, index 0}` with the sector's `(t0,t1)`; split = `splits[node.index]`,
`a = left.type & 0xC`, `lhi = left.value + pad`, `rlo = right.value - pad`, children = the two node structs. At a leaf
test entries `index .. index+contents-1` in order. So hits come **near-to-far along the segment** (front to back in
both trees), not in stream order. The interval is only used for culling; every triangle is tested against the full
segment.

Triangle test (inline, identical to RtIntersectionLineTriangle, 2.5), **one sided**: only triangles with
`det = (d x e2)·e1 > 1e-8` (i.e. `d · ((v1-v0)x(v2-v0)) < 0`, the segment enters the front face) can hit; inclusive
borders with a relative slack: with `eps = -1e-5*det`, `hi = det*(1+1e-5)`: `eps <= u <= hi`, `eps <= v`,
`u+v <= hi`, `eps <= t' <= hi`. Distance passed to the callback = `t'/det` = fraction along the segment, in
`[-1e-5, 1+1e-5]`. Degenerate triangles never hit (det <= 1e-8 absolute; note this also drops tiny
triangles/very short segments).

### 2.2 Sphere (type 3, WorldForAllTriIntsecSphere 0x54a4d0)
Outer box `O = {sup c+r, inf c-r}`, inner box `I = {sup c+0.57r, inf c-0.57r}`, `ir = 1/r`.
Plane tree with O (box rule 2.4), left child first. Per sector: skip if no tree; skip unless the tree bbox is within
r of c (`sum(dist_a^2) <= r^2`, per axis distance to [inf,sup]); skip if the tree bbox is disjoint from O
(`tree.inf[a] > O.sup[a] || tree.sup[a] < O.inf[a]`). kd walk as 2.4 with the outer box for culling and the inner box
for the "fully inside" flags. For each entry: if the node's flags are 0 (node region inside I, hence inside the
sphere) the triangle is reported **without a test**; otherwise `RtIntersectionSphereTriangle` (2.6) decides.
**distance = ((c - v0) · n̂) / r** (signed plane distance of the centre, in units of the radius, in [-1,1];
positive on the front side). Two sided. Degenerate triangles: reported (normal 0, distance 0) when in a fully
inside node, otherwise rejected by the sphere test.

### 2.3 Box (type 4, WorldForAllTriIntsecBBox 0x549f10)
Same as the sphere with O = I = the box; per sector: skip if no tree or the tree bbox is disjoint from the box.
Fully inside nodes: reported without test; else `RtIntersectionBBoxTriangle` (2.7). **distance = 0** always.
(The game never issues box queries; only line and sphere.)

### 2.4 Box style traversal (shared by sphere and box)
Plane tree: `if rightValue <= O.sup[a] { if O.inf[a] <= leftValue: visit left then right; else right only } else
left only`. kd tree: stack of (node, flags); flags bit `(1<<2a)` = the query's inf face at axis a cuts the region,
`(2<<2a)` = the sup face cuts it. Initial flags from the tree bbox vs I: bit `1<<2a` if `tree.inf[a] < I.inf[a]`,
bit `2<<2a` if `tree.sup[a] > I.sup[a]`. Node with flags 0 and a split: report the contiguous entry range from the
leftmost leaf's `index` to the rightmost leaf's `index+contents` (u16 arithmetic) untested. Split with flags != 0:
```
if right.value <= O.sup[a]:
    if O.inf[a] <= left.value:      # both, left first
        lf = rf = flags
        if left.value  <= I.sup[a]: lf &= ~(2<<2a)
        if I.inf[a] <= right.value: rf &= ~(1<<2a)
        visit (left,lf) then (right,rf)
    else visit (right, flags)
else visit (left, flags)
```
Leaf: entries in order; test only if flags != 0. Order = plane tree left first, kd left first = the sector stream
order of librw (`World::sectors`) and left-first kd order; the fork already matches this order exactly.

### 2.5 RtIntersectionLineTriangle (0x557860)
`RwBool RtIntersectionLineTriangle(RwV3d *lineStart, RwV3d *lineDelta, RwV3d *v0, RwV3d *v1, RwV3d *v2, RwReal *dist)`
exactly the test of 2.1 (one sided, slack, `*dist = t'/det`). Side effect: when u and v pass but t' fails, `*dist`
is left holding the raw t' (not divided). Game use: course_point_in_water_box_test (0x458170, short upward segment
against water quads), shadow_project_to_ground (0x499dc0).

### 2.6 RtIntersectionSphereTriangle (0x557a80) (sphere, v0, v1, v2, RwV3d *normal, RwReal *dist)
A = v0-c, B = v1-c, C = v2-c (floats). Per axis: if all three of A,B,C have a clear sign bit and all `X-r` have a
clear sign bit (>= +0) -> false; if all three have the sign bit set and all `r+X` have it set -> false.
n = (v1-v0)x(v2-v0); `|n|^2 <= 0` -> false; normal = n̂; pd = n̂·A; `pd < -r || pd > r` -> false; `*dist = -pd`.
Then true if `|A|^2 <= r^2` or `|B|^2 <= r^2` or `|C|^2 <= r^2`. Nearest vertex X (ties: B over A if |B|<=|A|, C if
strictly nearer): separated (false) if X's Voronoi test holds strictly (`X·X < X·Y` for both others, written as in the
decompile: e.g. B nearest: `bb < A·B && bb < C·B`; C nearest: `!(A·C <= cc) && !(B·C <= cc)`; A nearest:
`!(A·B <= aa) && !(A·C <= aa)`). Then for each edge (AB with C, BC with A, CA with B): Q = P0 - e*(e·P0)/(e·e)
(foot of the perpendicular from the centre to the edge line), false if `r^2 < Q·Q && Q·Q < Q·opposite`. Else true.
This is an exact sphere/triangle overlap test up to boundary ties (verified against the fork's exact closest point
test: identical sets on 164000 world sphere queries except degenerate triangles).

### 2.7 RtIntersectionBBoxTriangle (0x558220) (RwBBox *box, v0, v1, v2)
Outcodes per vertex: bit `1<<2a` if `v[a] > sup[a]`, else bit `2<<2a` if `v[a] < inf[a]` (inclusive inside).
Any vertex code 0 -> true. `c0&c1&c2 != 0` -> false. Edges (v0v1 if `c0&c1==0`, v0v2 if `c0&c2==0`, v1v2 if
`c1&c2==0`) are intersected with each face in `ci^cj` (strict inequalities, see colldiff.cpp `edgeFaces`) -> true.
Finally one box diagonal (chosen by the signs of the normal components) against the triangle (strict) -> true.
Else false. Up to boundary ties this is exact; it agrees with the fork's SAT on all 164000 world box queries except
degenerate triangles (a degenerate triangle with a vertex inside the box is reported by the original).

## 3. Geometry / atomic queries

`RpCollisionGeometryForAllIntersections` (0x54acd0): same dispatch, geometry local space, morph target 0 vertices,
callback `cb(is, tri, distance, data)`, same triangle struct / distance semantics as 2. **A geometry without a tree is
brute forced over all triangles** (unlike world sectors). No plane tree; the kd walk starts at the pseudo root.

`RpAtomicForAllIntersections` (0x54cbe0): type 1: line transformed by `RwMatrixInvert(LTM)`; type 3: centre
transformed, radius *= |inv.at| (length of the inverted matrix's at row); type 5: atomic's world bounding sphere,
then as 3; **type 4 (box) and others: no-op**, returns the atomic. When `numMorphTargets > 1` the interpolated morph
variants (MorphGeomForAllTriIntsecLine/Sphere) are used. The callback receives the caller's original (world space)
RpIntersection pointer, not the transformed copy.

## 4. Plugin data API as the game calls it

- 0x54d6a0 is **RpCollisionGeometryQueryData** (reads `_rpCollisionGeometryDataOffset`; returns RwBool: tree != NULL)
  and 0x54d670 is **RpCollisionGeometryDestroyData** (frees the geometry's tree, returns the geometry).
  `api_used.csv` labels them RpCollisionWorldSector* - wrong. Game: vehicle_load 0x423320:
  `if (!RpCollisionGeometryQueryData(geo)) RpCollisionGeometryBuildData(geo, NULL)` for vehicle parts 0x3B..0x3E and
  0x40; vehicle_prepare_parts 0x421d50 calls DestroyData on the same parts; vehicle_part_set_effect -> atomic line
  query (nearest hit). The world sector QueryData/DestroyData are never called.
- RpCollisionBuildParam is `{RwInt32 flags; callback; void *data}` (BuildTriListCollTree 0x54da80): flags bit 0 =
  sort the geometry's triangles into tree order (then the tree has no map, flags 0) and call
  `callback(geometry, tree, remap u16[], data)`. NULL params = all zero (no sort, tree with map, flags 1).
  The 0x5d09d0 "RpCollisionBuildParamInitDefaults" FID hit is a byte-identical 3-dword zeroing helper of a COM-style
  object (vtable 0x64bfb0, refcount at +4) - not a collision call; the game never initialises build params.
- Builder (`_rpCollTreeBuild` 0x54e1c0, `BuildTreeGenerate` 0x54e4c0): leaf when fewer than 4 triangles; otherwise per
  axis sort the used vertices, examine up to 50 candidate planes between the quartiles around the median, score
  `DivisionScore * FuzzyExtentScore`, classify triangles by vertex sides (GetClipStats), left value = max coordinate
  of the left triangles, right value = min of the right ones; no valid split: single leaf if < 240 triangles else an
  even split; depth limit 32. Different trees only change the callback order, not the hit set.
- Plugin registration (0x54d6c0): engine, atomic (4 bytes), world sector (4 bytes + stream 0x11D), geometry (4 bytes +
  stream 0x11D); copy constructors are NULL (a cloned geometry has no tree).

## 5. Other Rt functions

`RtLineSphereIntersectionTest(RwLine *line, RwSphere *sphere, RwReal *dist)` (0x569110):
```
d = end - start; il = _rwV3dNormalize(&d,&d)   # NB: _rwV3dNormalize returns 1/|d| (table rsqrt)
m = c - start; b = m·d̂; m2 = m·m
if m2 < r^2:                      { *dist = b*il; return TRUE }     # start strictly inside (b may be < 0)
if b > 0 && r^2 - (m2 - b^2) >= 0: { *dist = b*il; return TRUE }
return FALSE
```
It is a **ray** test from the start (no upper bound at the segment end) and `*dist` is the fraction (b/|d|) of the
**closest approach** to the centre, not of the entry point. The three game callers (hudpanel_cursor_update 0x41c7c0,
weapon_find_targets_in_cone 0x465470, weapon_shotgun_fire_shot 0x467200) only use the boolean.

`RtBaryGetTransform(RwMatrix *m, RwReal *scale, RwV3d *v0, RwV3d *v1, RwV3d *v2)` (0x569610): n = (v1-v2)x(v2-v0)
(= (v1-v0)x(v2-v0)); `|n|^2 <= 2.4414062e-4` -> degenerate: `*scale = 0`, everything below computed with factor 0
(all zero except m[14] = 1), return FALSE. Else `*scale = |n|`, k = 1/|n|, n̂ = n*k. The 16 floats of the RwMatrix
(right, flags, up, pad1, at, pad2, pos, pad3 = m[0..15]) are a 4x4 matrix for row vector `[x y z 1] * M =
[w0 w1 w2 h]`: column 0 = barycentric weight of v0, column 1 = weight of v1, column 2 = 1 - w0 - w1 = weight of v2,
column 3 = signed distance to the plane (`n̂·q - n̂·centroid`). Exactly:
`m0 = ((v1-v2) x n̂).x*k, m4 = .y*k, m8 = .z*k, m12 = -(v2·(m0,m4,m8))`;
`m1 = ((v2-v0) x n̂).x*k, m5, m9 likewise, m13 = -(v0·(m1,m5,m9))`;
`m2 = -(m0+m1), m6 = -(m4+m5), m10 = -(m8+m9), m14 = 1-(m12+m13)`;
`m3,m7,m11 = n̂`, `m15 = -(centroid·n̂)` (centroid = (v0+v1+v2)*0.33333334). Note the flags word (m3) receives n̂.x.
Game use: course_world_ray_hit_light_sample 0x4891b0 computes `w_i = m[i]*x + m[4+i]*y + m[8+i]*z + m[12+i]` for
i = 0,1,2 at the line hit point and blends the three vertex prelight values with them; `*scale` is ignored.

## 6. Game call sites (for weighting)
Line (type 1, world): car_probe_ground_distance 0x47b480 (vertical 6 unit probe, min distance), shadow_project
0x499dc0 (callback returns 0: FIRST reported hit only), FUN_004c4d90/4c4de0 nearest hit (line_of_sight_between,
camera_update, trail_update, hudpanel_marker_ray_cast, mortar/drum/rocket/oil/mine/flamethrower weapons,
course_world_ray_hit_light_sample), FUN_004c4b50 capped collector (FUN_004c4ba0/4c30).
Sphere (type 3, world): car_world_collision_query 0x47bb00 and car_wheel_contact_update 0x47d840 (car physics;
callback 0x477170 stores normal, vertices, material via index), player_time_update 0x409a70 (r 0.6, last hit wins),
FUN_00573a00 (per triangle sphere, uses normal and vertex identity), FUN_004c4e20 (nearest by distance),
FUN_004c4be0/4c70 (collector storing distance).
Atomic (type 1): vehicle_part_set_effect -> FUN_004c4f40 nearest distance.
