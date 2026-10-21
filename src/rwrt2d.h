// Rt2d subset used by Mashed (Fully Loaded / Drive to Survive): texture fonts (.RWF, chunk 0x199),
// brush, CTM stack, path (bbox / clip rectangle), device (camera mapping) and text drawing through Im2D.
// Clean-room, see MFL_NOTES.md "Rt2d" for sources and the list of guesses.
#ifndef RW_RT2D_H
#define RW_RT2D_H

namespace rw {
namespace rt2d {

enum { ID_FONT2D = 0x199, MAXFONTTEXTURES = 4 };
enum { FONT_UNICODE = 2 };	// Font::flags bit: strings are 16 bit chars (all shipped fonts)
enum { FLOW_LEFT = 0, FLOW_CENTER = 1, FLOW_RIGHT = 2 };	// Rt2dFontFlow format argument

// Rt2d module state: Open/Close mirror Rt2dOpen/Rt2dClose (CTM stack, font texture dictionary, device).
bool32 open(void);
void close(void);
// Live Font+Brush+Path objects; 0 after everything was destroyed (leak check).
int32 numAllocated(void);

// ---- 2D affine matrix used for the CTM (column convention)
struct Affine { float32 a, b, c, d, tx, ty; };	// x' = a*x + c*y + tx, y' = b*x + d*y + ty

// ---- CTM (Rt2dCTM*). Operations pre-concatenate like RwMatrix*(rwCOMBINEPRECONCAT).
void ctmSetIdentity(void);
bool32 ctmPush(void);	// false at depth 31
void ctmPop(void);	// no-op at depth 0
void ctmTranslate(float32 x, float32 y);
void ctmScale(float32 x, float32 y);
void ctmRotate(float32 degrees);	// about the z axis, degrees (RwMatrixRotate convention)
Affine ctmGet(void);	// top of the stack
int32 ctmDepth(void);

// ---- Brush (Rt2dBrush*): four corner colours bottom-left, bottom-right, top-right, top-left of every glyph quad;
// colours are interpolated along the text width. All four equal -> flat colour.
struct Brush
{
	RGBA c[4];	// [0]=bottom-left, [1]=bottom-right, [2]=top-right, [3]=top-left (argument order of Rt2dBrushSetRGBA)
	bool32 gradient;
	int32 refCount;
	static int32 numAllocated;
	static Brush *create(void);	// white
	bool32 destroy(void);	// drops one reference; true when freed (Rt2dBrushDestroy returns 1 only then)
	void setRGBA(const RGBA *bl, const RGBA *br, const RGBA *tr, const RGBA *tl);
};

// ---- Path (Rt2dPath*): only what the game calls (create/destroy/bbox) plus the builders Rt2dDeviceGetClippath needs
struct Path
{
	struct Sub { float32 *xy; int32 n, cap; bool32 closed; };
	Sub *subs;
	int32 numSubs, capSubs;
	static int32 numAllocated;
	static Path *create(void);
	void destroy(void);
	void empty(void);
	void moveto(float32 x, float32 y);
	void lineto(float32 x, float32 y);
	void curveto(float32 x1, float32 y1, float32 x2, float32 y2, float32 x3, float32 y3);	// cubic, flattened into 16 segments
	void close(void);
	// bbox as {x, y, w, h}, i.e. {minX, minY, maxX-minX, maxY-minY}; all zero for an empty path
	void getBBox(float32 *xywh) const;
};

// ---- Device (Rt2dDevice*)
// Rt2d space: origin bottom-left of the view window, y up, one unit = min(viewW, viewH) where viewW/H = 2*camera->viewWindow.
// (so a 4:3 view is 1.333 x 1.0 units; ui_2d_begin reads exactly that rectangle back with Rt2dDeviceGetClippath).
void deviceSetView(float32 viewW, float32 viewH, int32 rasterW, int32 rasterH);	// camera-less variant (tests)
bool32 deviceSetCamera(Camera *cam);	// Rt2dDeviceSetCamera; nil clears. false if cam has no frame buffer
// Screen pixel position of a point in device space, i.e. a Rt2d point that already went through the CTM
void deviceToScreen(float32 x, float32 y, float32 *sx, float32 *sy);
// Clip path = the visible rectangle in current CTM space (Rt2dDeviceGetClippath): 4 corners, closed
void deviceGetClippath(Path *p);

// ---- Font (Rt2dFont*), texture fonts only (type 0)
struct Glyph
{
	float32 advance;	// glyph width in units of the text height (file: 5th float of the record)
	float32 u0, v0, u1, v1;	// texture rectangle; v0 = top edge, v1 = bottom edge
	uint8 texIndex;
};

struct Font
{
	int32 type;	// 0 = texture font (only supported value)
	float32 nominalHeight;	// header float 1 (34.0 / 33.0); not used by measuring or drawing
	float32 baseline;	// header float 2 (5/34); drawing shifts the text down by baseline*height
	float32 intergap;	// header int 3, a float: extra advance after every glyph (0 in the files; Rt2dFontSetIntergapSpacing)
	int32 flags;	// header int 4 (2 in all files) = FONT_UNICODE
	int32 highBase;	// first char code covered by highMap (header "numLowMap", 128)
	int32 numHigh;
	int16 *highMap;
	int16 lowMap[128];	// glyph index per char 0..127, -1 = none
	int32 numGlyphs;
	Glyph *glyphs;
	int32 numTextures;
	char texNames[MAXFONTTEXTURES][32];
	Texture *tex[MAXFONTTEXTURES];	// resolved from the texture dictionary, nil if absent
	static int32 numAllocated;

	// Rt2dFontStreamRead: stream positioned after the 0x199 chunk header. requireTextures: fail if a texture is not in the
	// font texture dictionary (what the game does); otherwise missing textures stay nil (no GPU / unit tests).
	static Font *streamRead(Stream *stream, bool32 requireTextures = 0);
	// findChunk(0x199) + streamRead (rt2d_font_load in the game)
	static Font *streamReadChunk(Stream *stream, bool32 requireTextures = 0);
	void destroy(void);

	int32 glyphIndex(uint32 code) const;	// -1 if the font has no glyph for the char
	void setIntergapSpacing(float32 s) { intergap = s; }	// Rt2dFontSetIntergapSpacing
	// Rt2dFontGetStringWidth: sum of (advance+intergap) over mapped chars * height. text: NUL terminated, 16 bit chars
	// if flags&FONT_UNICODE else 8 bit.
	float32 getStringWidth(const void *text, float32 height) const;
	// Number of lines Rt2dFontFlow would produce for a box of the given width, and their total height (lines*height).
	int32 measureFlow(const void *text, float32 height, float32 boxWidth, float32 *totalHeight) const;
	// Rt2dFontShow: draws at pos (bottom-left of the first glyph, baseline-shifted), advances pos->x by the width.
	Font *show(const void *text, float32 height, V2d *pos, Brush *brush);
	// Rt2dFontFlow: word wrap into rect {x, y, w, h} (y = bottom). Returns nil if the text does not fit (rest not drawn).
	Font *flow(const void *text, float32 height, const float32 *rectXYWH, int32 format, Brush *brush);
};

// Rt2dFontTexDictionarySet: dictionary the font loader resolves textures in (not owned).
void fontTexDictionarySet(TexDictionary *dict);

// ---- Output. Every glyph is turned into 4 screen vertices (TL, BL, TR, BR ordering is {bottom-left, top-left, bottom-right,
// top-right} of the glyph) and batched (<= 63 quads, one texture per batch). The default sink draws through Im2D
// (only when built with a render platform); tests install their own.
struct Vertex { float32 x, y, u, v; uint8 r, g, b, a; };
typedef void (*QuadSink)(void *user, Texture *tex, const Vertex *quads /* 4 per quad */, int32 numQuads);
void setQuadSink(QuadSink sink, void *user);	// nil = default Im2D sink
void flushBatch(void);	// called by show/flow at the end; exposed for sinks that need an explicit flush

}
}

#endif
