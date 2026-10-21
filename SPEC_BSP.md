# BSP world stream (RenderWare 3.7 RpWorld, chunk 0x0B): spec of the behaviour

Derived from how MFL.exe behaves (the read path at 0x4f9bd0 / 0x4fa060 / 0x4fa440 and the post-load
passes 0x4f5300 / 0x4f5420), and checked against 126 game files and 32 SDK sample files. All values
are little endian. "FindChunk(id)" means: read 12-byte chunk headers `{u32 type, u32 length,
u32 libraryId}` and skip `length` bytes until `type == id`. It fails at end of stream.
Version: decode libraryId (`if (id & 0xFFFF0000) ver = ((id>>14)&0x3FF00) + 0x30000 | ((id>>16)&0x3F); else ver = id<<8`).
Each FindChunk below also requires `0x35000 <= ver <= 0x37002`. Every game file uses 0x37002.

## 1. WORLD (0x0B)

```
WORLD
  STRUCT (0x01)      world header, normally 64 bytes
  MATLIST (0x08)     standard material list (same as in geometry)
  root node          ATOMICSECT (0x09) if header.rootIsWorldSector != 0 else PLANESECT (0x0A)
  EXTENSION (0x03)   world plugins (empty in all game files)
```

### World header (STRUCT payload; the reader zero-fills 64 bytes, then reads `length` bytes)

| off | type | field | semantics |
|---|---|---|---|
| 0x00 | i32 | rootIsWorldSector | 1 means the root is an atomic sector (36 single-sector game files, format 0x40000041) |
| 0x04 | f32[3] | invWorldOrigin | the world origin is `-invWorldOrigin`. It is stored only; vertices are already in world space |
| 0x10 | i32 | numTriangles | sum over all sectors |
| 0x14 | i32 | numVertices | sum over all sectors |
| 0x18 | i32 | numPlaneSectors | |
| 0x1C | i32 | numWorldSectors | atomic sectors |
| 0x20 | i32 | colSectorSize | must be <= 0, otherwise the original rejects the file (0 in all files) |
| 0x24 | u32 | format | flags, see section 4 |
| 0x28 | f32[3] | bbox.sup | **max corner first** |
| 0x34 | f32[3] | bbox.inf | min corner |

Texture coordinate set count: `n = (format >> 16) & 0xFF`. If it is 0, `n = (format & 0x80) ? 2 : (format & 0x04) ? 1 : 0`.

If `format & 0x01000000` (NATIVE) is set, the sectors carry no vertex or triangle arrays (see 2.2).
The platform data then comes from the sector extension (0x510 native data). No game file uses this.

## 2. Sector tree

The tree is serialised depth first, left subtree before right subtree. The atomic sectors appear in stream
order, and that order is also the enumeration order (RpWorldForAllWorldSectors walks the same order).

### 2.1 PLANESECT (0x0A)

```
PLANESECT
  STRUCT (24 bytes; the reader zero-fills 24 bytes, then reads `length`)
  left child      FindChunk(0x09) if leftIsWorldSector else FindChunk(0x0A)
  right child     FindChunk(0x09) if rightIsWorldSector else FindChunk(0x0A)
  (no extension is read for plane sectors)
```

| off | type | field | semantics |
|---|---|---|---|
| 0x00 | i32 | type | axis as a **byte offset** into a V3d: 0 = x, 4 = y, 8 = z |
| 0x04 | f32 | value | split plane |
| 0x08 | i32 | leftIsWorldSector | nonzero means the left child is an ATOMICSECT |
| 0x0C | i32 | rightIsWorldSector | |
| 0x10 | f32 | leftValue | upper bound of the left child along the axis (the left child is on the low side) |
| 0x14 | f32 | rightValue | lower bound of the right child along the axis (the right child is on the high side) |

If `format & 0x40000000` (sectors overlap) is clear, both leftValue and rightValue are replaced by `value`.
In the game data 499 of 954 planes have `leftValue > value` or `rightValue < value` (the children overlap).

### 2.2 ATOMICSECT (0x09)

```
ATOMICSECT
  STRUCT (0x01)   44-byte header, then the arrays below
  EXTENSION (0x03) sector plugins
```

Header (44 bytes):

| off | type | field | semantics |
|---|---|---|---|
| 0x00 | i32 | matListWindowBase | kept as u16. Added to every triangle's material index |
| 0x04 | i32 | numTriangles | kept as u16 in memory |
| 0x08 | i32 | numVertices | kept as u16 in memory |
| 0x0C | f32[3] | tightBBox.inf | **min corner first** (opposite of the world header) |
| 0x18 | f32[3] | tightBBox.sup | |
| 0x24 | i32 | unused | garbage in the files (format low byte, float bits, pointer-like values). Ignore it. It is NOT a collision flag |
| 0x28 | i32 | unused | 0 |

Arrays follow the header in this order (all skipped when NATIVE is set):

1. if numVertices: `numVertices * {f32 x,y,z}` positions
2. if format & 0x10 (NORMALS): `numVertices * {i8 x,y,z,pad}`. Decode as `component / 128.0f` (not 127)
3. if format & 0x08 (PRELIT): `numVertices * {u8 r,g,b,a}`
4. for each of the n texture sets (world-level count from section 1): `numVertices * {f32 u,v}`
5. if numTriangles: `numTriangles * 4 * u16`
   - sector STRUCT version >= 0x36002: `{v0, v1, v2, matIndex}`
   - version < 0x36002: the file stores `{matIndex, v0, v1, v2}`. Rotate it to the form above
   - material of a triangle = `world.matList[matListWindowBase + matIndex]`

Expected STRUCT length (useful as a strictness check; the original does not check it):
`44 + 12V + (NORMALS?4V:0) + (PRELIT?4V:0) + 8Vn + 8T`.

Sector EXTENSION: a list of plugin chunks. Unknown ids are skipped by length. Ids seen in the data:

| id | meaning | what the original does |
|---|---|---|
| 0x50E | bin mesh: `{u32 flags (1 = tristrip), u32 numMeshes, u32 totalIndices}`, then per mesh `{u32 numIndices, u32 matIndex (absolute index into world matList)}` and `numIndices * u32` | used as the sector's render mesh. If it is missing, a mesh is built from the triangles (strips when format bit 0 is set). For all 1080 game sectors this mesh holds exactly the sector's triangles per material, so rebuilding from the triangle array is equivalent |
| 0x11D | collision tree (RpCollTree) | collision plugin; see rwscene.py `read_colltree` |
| 0x120 | MatFX sector: one i32 | if nonzero, the sector uses the MatFX world-sector pipeline (needed for dual-texture / env materials). The value is 1 in 46 game sectors in 7 track graphics files |
| 0x11F | user data | named arrays. Not present in the files; the game adds a "BS" (bounding sphere) array to sectors at runtime |
| 0x510 | native sector data | only with NATIVE |

## 3. Post-load derived data

1. **Sector boundingBox (plane box).** Start with the world bbox. Walk the tree. At a plane on axis a:
   the left child gets `sup[a] = leftValue`, the right child gets `inf[a] = rightValue`, and every other
   component is inherited. A leaf stores the box it receives. This box is separate from the file's tight
   box. In all files the tight box lies inside the plane box. The game's own code reads the **tight** box
   (and numVertices / numTriangles / vertices), so the plane box is only needed for exact
   RpWorldSectorGetBBox semantics.
2. **Meshes:** see 0x50E above.
3. The world's render order defaults to back-to-front. Sectors are drawn in the camera's frustum sector order, and the
   per-sector render callback is replaceable (the game uses it to count and collect sectors).
4. Stack limits in the original walks: a 64-entry stack of pending right children. Real trees are shallow.

## 4. Format flags (world header 0x24)

| bit | name | effect on sector streams |
|---|---|---|
| 0x00000001 | TRISTRIP | meshes are strips (set in every game format); no change to the arrays |
| 0x00000002 | POSITIONS | informational (positions are always present) |
| 0x00000004 | TEXTURED | 1 UV set if bits 16-23 are 0 |
| 0x00000008 | PRELIT | colour array present |
| 0x00000010 | NORMALS | int8x4 normal array present |
| 0x00000020 | LIGHT | sectors are lit |
| 0x00000040 | MODULATEMATERIALCOLOR | material colour modulates |
| 0x00000080 | TEXTURED2 | 2 UV sets if bits 16-23 are 0 |
| 0x00FF0000 | numTexCoordSets | overrides the two bits above when nonzero |
| 0x01000000 | NATIVE | no arrays in the STRUCT |
| 0x40000000 | SECTORSOVERLAP | plane leftValue/rightValue are meaningful (otherwise = value) |

The D3D9 instancer chooses UV declarations from `format & 0x84` (set 0) and `format & 0x80` (set 1),
not from the count field. Keep the two consistent when building vertex formats.

Formats in the game files, with the streams per vertex after the position:

| format | files | normals | prelit | UV sets | sector ext |
|---|---|---|---|---|---|
| 0x40000041 | 39 | - | - | 0 | 0x11D, 0x50E |
| 0x40000049 | 39 | - | yes | 0 | 0x11D, 0x50E |
| 0x40000071 | 2 (Arctic) | yes | - | 0 | 0x11D, 0x50E |
| 0x40000079 | 2 (Arctic) | yes | yes | 0 | 0x11D, 0x50E |
| 0x4001000D | 12 | - | yes | 1 | 0x50E |
| 0x4001004D | 25 | - | yes | 1 | 0x50E (+0x120 in 2) |
| 0x400200C9 | 5 | - | yes | 2 | 0x120, 0x50E |
| 0x40020089 | 2 | - | yes | 2 | 0x120, 0x50E |

(0x40010034/35/3C/3D and 0x400200F9 occur only in the SDK samples.)

## 5. Corrections for librw-mfl (in priority order)

1. Decode normals with `/128.0f`.
2. Keep the 0x120 sector flag, and enable MatFX on the sector's render atomic when it is nonzero.
3. Rename `collSectorPresent` to an unused field. The header word is garbage.
4. Optional: check the version range, rotate triangles for sector STRUCT version < 0x36002, and use `value` for
   leftValue/rightValue when 0x40000000 is clear. Reject `colSectorSize >= 1` if you want to match the original.
5. Optional: compute the plane box (section 3.1) next to the tight box. Optional: draw sectors in camera order
   (back-to-front) for translucent world materials.
