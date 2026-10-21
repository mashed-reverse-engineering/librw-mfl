// RpCollision plugin pieces used by the game (see MFL_NOTES.md "Collision"; query semantics follow MFL.exe).
// Independent of rendering: only needs rwbase.h/rwobjects.h types.
namespace rw {

enum { ID_COLLISION = MAKEPLUGINID(VEND_CRITERIONTK, 0x1D) };	// 0x11D

// One half of a split. type: bits 2..3 = axis (0 x, 4 y, 8 z), bit 0 set = "left" node (covers coordinate <= value,
// value is the maximum of its contents), clear = "right" node (coordinate >= value, minimum of its contents).
// contents == 0xFF: a split, index = index into CollisionTree::splits. Otherwise a leaf with `contents` entries
// starting at position `index` of the entry map.
struct CollNode
{
	uint8 type;
	uint8 contents;
	uint16 index;
	float32 value;
};

struct CollSplit
{
	CollNode left, right;
};

// kd-style tree (RpCollTree). splits[0] is the root pair; with numSplits == 0 the tree is one leaf of (u8)numEntries
// entries. Every triangle is referenced exactly once (the child volumes may overlap), entries are triangle indices of
// the owner (sector or geometry).
struct CollisionTree
{
	enum { HASMAP = 1, SPLIT = 0xFF, VERSION = 0x37002, MAXDEPTH = 512 };
	uint32 flags;
	BBox bbox;		// inf <= sup
	uint32 numEntries;
	uint32 numSplits;
	CollSplit *splits;
	uint16 *map;		// nil: identity (flags & HASMAP == 0)

	uint32 entry(uint32 k) const { return map ? map[k] : k; }

	// blob = content of the 0x11D plugin chunk (u32 version, chunk 0x2C { STRUCT }). Strict: sizes, indices,
	// split reachability (tree, no sharing, no cycles) and depth are checked. numTriangles must equal numEntries.
	static CollisionTree *fromBlob(const uint8 *blob, uint32 size, int32 numTriangles);
	uint32 blobSize(void) const;
	void toBlob(uint8 *blob) const;	// blobSize() bytes, stamp = stamp of the game's data (0x1C02000A)
	CollisionTree *clone(void) const;
	void destroy(void);
	// builds a tree over triangles (kd tree on centroids, median split, leaf <= params.maxEntriesPerLeaf; a single
	// leaf without splits when numTris <= maxEntriesPerLeaf). Not the game's builder: other tree, same hits.
	static CollisionTree *build(const V3d *vertices, const Triangle *tris, int32 numTris, int32 maxEntriesPerLeaf);
};

struct CollisionBuildParams
{
	int32 maxEntriesPerLeaf;	// default 2 (the game's data: 1..12, mostly 2)
};
void collisionBuildParamInitDefaults(CollisionBuildParams *p);

enum IntersectionType
{
	INTERSECTNONE = 0,	// values as in the game (rpINTERSECT*)
	INTERSECTLINE,		// 1
	INTERSECTPOINT,		// 2: no-op (as the original)
	INTERSECTSPHERE,	// 3
	INTERSECTBOX,		// 4
	INTERSECTATOMIC		// 5: sphere query with t.atomic's world bounding sphere
};

struct Intersection
{
	union {
		Line line;	// start, end
		Sphere sphere;
		BBox box;	// sup, inf
		Atomic *atomic;
	} t;
	int32 type;
};

struct CollisionTriangle
{
	V3d normal;		// unit normal (v1-v0)x(v2-v0), zero for degenerate triangles
	V3d point;		// = *vertices[0] for every query type (as the original; not a hit point)
	int32 index;		// triangle index in the sector / geometry
	V3d *vertices[3];
};

// callbacks return nil to stop the iteration; they get the caller's Intersection. distance:
//   line: fraction along start..end of the hit, in [-1e-5, 1+1e-5]
//   sphere: signed distance of the centre from the triangle plane in units of the radius, ((c-v0).n)/r, in [-1,1]
//   box: always 0
typedef CollisionTriangle *(*WorldIntersectionCB)(Intersection *isec, WorldSector *sector,
	CollisionTriangle *tri, float32 distance, void *data);
typedef CollisionTriangle *(*GeometryIntersectionCB)(Intersection *isec,
	CollisionTriangle *tri, float32 distance, void *data);

// plane tree, then the sector's kd tree. Lines: one sided (front faces only, det > 1e-8), near to far along the
// segment in both trees. Sphere/box: plane tree and kd tree left first; triangles of kd nodes inside the query are
// reported untested (degenerate ones too). Sectors without a collision tree are skipped. Unsupported types do
// nothing. Returns the world (nil only for nil arguments).
World *collisionWorldForAllIntersections(World *world, Intersection *isec, WorldIntersectionCB cb, void *data);
// sector->collTree or nil
CollisionTree *collisionWorldSectorQueryData(WorldSector *sector);
void collisionWorldSectorDestroyData(WorldSector *sector);

// geometry plugin (object space, uses morph target 0). Without a tree all triangles are tested.
void registerCollisionPlugin(void);
bool collisionGeometryBuildData(Geometry *geo, const CollisionBuildParams *params);	// params may be nil
CollisionTree *collisionGeometryQueryData(Geometry *geo);
void collisionGeometryDestroyData(Geometry *geo);
Geometry *collisionGeometryForAllIntersections(Geometry *geo, Intersection *isec, GeometryIntersectionCB cb, void *data);
// isec in world space, transformed into the atomic's local space: line, sphere (radius *= |inverse LTM at|),
// atomic (its world bounding sphere, then as sphere); box and others: nothing. Morphing geometries (more than one
// morph target) are brute forced on morph target 0 (librw has no interpolator). The callback gets the caller's isec
// and local space triangles.
Atomic *collisionAtomicForAllIntersections(Atomic *atomic, Intersection *isec, GeometryIntersectionCB cb, void *data);

// RtIntersection / RtBary
// lineDelta = end - start. One sided (det = (d x e2).e1 > 1e-8: d enters the front face), borders inclusive with
// 1e-5 relative slack. distance = fraction along the delta; on a miss after u/v passed it holds the raw t'.
bool intersectionLineTriangle(const V3d *lineStart, const V3d *lineDelta,
	const V3d *v0, const V3d *v1, const V3d *v2, float32 *distance);
// RAY from start (no end bound): true if the start is strictly inside or the ray passes within the radius ahead.
// distance (optional) = fraction (b/|d|) of the closest approach to the centre, not of the entry point.
bool lineSphereIntersectionTest(const Line *line, const Sphere *sphere, float32 *distance = nil);
// all 16 floats of the Matrix (incl. the flags/pad words) as a 4x4 for the row vector [x y z 1]: column i = 0,1,2 is
// the barycentric weight of v_i (w_i = m[i]x + m[4+i]y + m[8+i]z + m[12+i]), column 3 the signed plane distance.
// *scale = |n| (0 when degenerate, |n|^2 <= 2.44e-4; the matrix is still written, return false).
bool baryGetTransform(Matrix *transform, float32 *scale, const V3d *v0, const V3d *v1, const V3d *v2);

}
