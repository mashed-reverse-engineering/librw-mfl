// Rt2d subset for Mashed: .RWF texture fonts, brush, CTM, path, device mapping, text drawing through Im2D.
// Clean-room: stream layout from MashedFormats/FORMATS.md (RWF) and behaviour (string width, word wrap flow, glyph quads,
// gradient colours, CTM / device mapping) from the game's own statically linked routines as observed in the binary
// (Rt2dFontStreamRead 0x564E40, GetStringWidth 0x565F80, Flow 0x566060, texture-font show 0x5653F0, CTM 0x562D20.., brush 0x564B10)
// plus the game's call sites. No RenderWare SDK source was read. Deviations / guesses are marked "GUESS" / "DEVIATION"
// and collected in MFL_NOTES.md.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwrender.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwrt2d.h"

#include "ps2/rwps2.h"
#include "d3d/rwd3d.h"
#include "gl/rwgl3.h"

#define PLUGIN_ID 0

namespace rw {
namespace rt2d {

int32 Font::numAllocated = 0;
int32 Brush::numAllocated = 0;
int32 Path::numAllocated = 0;
int32 numAllocated(void) { return Font::numAllocated + Brush::numAllocated + Path::numAllocated; }

// ---------------------------------------------------------------- state
enum { CTMDEPTH = 32, MAXBATCH = 64 };	// 32 matrices (depth 0..31); batch of 64 quads (0x2400 byte vertex buffer in the game)
static Affine ctm[CTMDEPTH];
static int32 ctmTop;
static TexDictionary *fontDict;
static float32 viewW = 2.0f, viewH = 1.5f;	// GUESS: default 4:3 view until a camera is set
static int32 rasterW = 640, rasterH = 480;
static Camera *devCamera;
static QuadSink userSink;
static void *userSinkData;
static Vertex batch[MAXBATCH*4];
static int32 batchQuads;
static Texture *batchTex;

static Affine identity(void) { Affine a = { 1, 0, 0, 1, 0, 0 }; return a; }

// result = m * t (t applied first)
static Affine
concat(const Affine &m, const Affine &t)
{
	Affine r;
	r.a = m.a*t.a + m.c*t.b;
	r.b = m.b*t.a + m.d*t.b;
	r.c = m.a*t.c + m.c*t.d;
	r.d = m.b*t.c + m.d*t.d;
	r.tx = m.a*t.tx + m.c*t.ty + m.tx;
	r.ty = m.b*t.tx + m.d*t.ty + m.ty;
	return r;
}

static void
apply(const Affine &m, float32 x, float32 y, float32 *ox, float32 *oy)
{
	*ox = m.a*x + m.c*y + m.tx;
	*oy = m.b*x + m.d*y + m.ty;
}

bool32
open(void)
{
	ctmTop = 0;
	ctm[0] = identity();
	fontDict = nil;
	devCamera = nil;
	viewW = 2.0f; viewH = 1.5f; rasterW = 640; rasterH = 480;
	batchQuads = 0; batchTex = nil;
	return 1;
}

void
close(void)
{
	ctmTop = 0;
	ctm[0] = identity();
	fontDict = nil;
	devCamera = nil;
	batchQuads = 0; batchTex = nil;
}

// ---------------------------------------------------------------- CTM
void ctmSetIdentity(void) { ctm[ctmTop] = identity(); }

bool32
ctmPush(void)
{
	if(ctmTop >= CTMDEPTH-1) return 0;
	ctm[ctmTop+1] = ctm[ctmTop];
	ctmTop++;
	return 1;
}

void ctmPop(void) { if(ctmTop > 0) ctmTop--; }
int32 ctmDepth(void) { return ctmTop; }
Affine ctmGet(void) { return ctm[ctmTop]; }

void
ctmTranslate(float32 x, float32 y)
{
	Affine t = identity(); t.tx = x; t.ty = y;
	ctm[ctmTop] = concat(ctm[ctmTop], t);
}

void
ctmScale(float32 x, float32 y)
{
	Affine t = identity(); t.a = x; t.d = y;
	ctm[ctmTop] = concat(ctm[ctmTop], t);
}

void
ctmRotate(float32 degrees)
{
	// GUESS: counter-clockwise in the y-up Rt2d space (RwMatrixRotate about +z); the game only ever passes 0.
	float32 r = degrees*(float32)(M_PI/180.0), s = sinf(r), c = cosf(r);
	Affine t = { c, s, -s, c, 0, 0 };
	ctm[ctmTop] = concat(ctm[ctmTop], t);
}

// ---------------------------------------------------------------- Brush
Brush*
Brush::create(void)
{
	Brush *b = rwNewT(Brush, 1, MEMDUR_EVENT);
	if(b == nil) return nil;
	numAllocated++;
	b->refCount = 1;
	RGBA w = makeRGBA(255, 255, 255, 255);
	b->setRGBA(&w, &w, &w, &w);
	return b;
}

bool32
Brush::destroy(void)
{
	if(--this->refCount > 0) return 0;
	rwFree(this);
	numAllocated--;
	return 1;
}

void
Brush::setRGBA(const RGBA *bl, const RGBA *br, const RGBA *tr, const RGBA *tl)
{
	this->c[0] = *bl; this->c[1] = *br; this->c[2] = *tr; this->c[3] = *tl;
	// the game clears the gradient flag only when all four colours are identical
	this->gradient = !(equal(c[0], c[1]) && equal(c[0], c[2]) && equal(c[0], c[3]));
}

// ---------------------------------------------------------------- Path
Path*
Path::create(void)
{
	Path *p = rwNewT(Path, 1, MEMDUR_EVENT);
	if(p == nil) return nil;
	numAllocated++;
	memset(p, 0, sizeof(*p));
	return p;
}

void
Path::empty(void)
{
	for(int32 i = 0; i < numSubs; i++) rwFree(subs[i].xy);
	rwFree(subs);
	subs = nil; numSubs = capSubs = 0;
}

void
Path::destroy(void)
{
	empty();
	rwFree(this);
	numAllocated--;
}

static bool
subAdd(Path::Sub *s, float32 x, float32 y)
{
	if(s->n >= s->cap){
		int32 nc = s->cap ? s->cap*2 : 8;
		float32 *n = (float32*)rwRealloc(s->xy, nc*2*sizeof(float32), MEMDUR_EVENT);
		if(n == nil) return false;
		s->xy = n; s->cap = nc;
	}
	s->xy[2*s->n] = x; s->xy[2*s->n+1] = y; s->n++;
	return true;
}

void
Path::moveto(float32 x, float32 y)
{
	if(numSubs >= capSubs){
		int32 nc = capSubs ? capSubs*2 : 4;
		Sub *n = (Sub*)rwRealloc(subs, nc*sizeof(Sub), MEMDUR_EVENT);
		if(n == nil) return;
		subs = n; capSubs = nc;
	}
	Sub *s = &subs[numSubs++];
	memset(s, 0, sizeof(*s));
	subAdd(s, x, y);
}

void
Path::lineto(float32 x, float32 y)
{
	if(numSubs == 0){ moveto(x, y); return; }	// GUESS: lineto without moveto starts a subpath
	subAdd(&subs[numSubs-1], x, y);
}

void
Path::curveto(float32 x1, float32 y1, float32 x2, float32 y2, float32 x3, float32 y3)
{
	if(numSubs == 0) moveto(x1, y1);
	Sub *s = &subs[numSubs-1];
	float32 x0 = s->xy[2*s->n-2], y0 = s->xy[2*s->n-1];
	for(int i = 1; i <= 16; i++){	// GUESS: fixed 16 segments instead of the game's tolerance based flattening
		float32 t = i/16.0f, u = 1-t;
		subAdd(s, u*u*u*x0 + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3,
		          u*u*u*y0 + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3);
		s = &subs[numSubs-1];
	}
}

void
Path::close(void)
{
	if(numSubs) subs[numSubs-1].closed = 1;
}

void
Path::getBBox(float32 *r) const
{
	float32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool first = true;
	for(int32 i = 0; i < numSubs; i++)
		for(int32 j = 0; j < subs[i].n; j++){
			float32 x = subs[i].xy[2*j], y = subs[i].xy[2*j+1];
			if(first){ x0 = x1 = x; y0 = y1 = y; first = false; continue; }
			if(x < x0) x0 = x;
			if(x > x1) x1 = x;
			if(y < y0) y0 = y;
			if(y > y1) y1 = y;
		}
	r[0] = x0; r[1] = y0; r[2] = x1-x0; r[3] = y1-y0;
}

// ---------------------------------------------------------------- Device
void
deviceSetView(float32 w, float32 h, int32 rw_, int32 rh_)
{
	viewW = w; viewH = h; rasterW = rw_; rasterH = rh_;
}

bool32
deviceSetCamera(Camera *cam)
{
	devCamera = cam;
	if(cam == nil) return 1;
	if(cam->frameBuffer == nil) return 0;
	// the game stores 2*viewWindow as the extent and the frame buffer size as raster size
	deviceSetView(cam->viewWindow.x*2, cam->viewWindow.y*2, cam->frameBuffer->width, cam->frameBuffer->height);
	// DEVIATION: the game also replaces the camera's frame-sync callback (0x563120); not needed here
	return 1;
}

static float32 unit(void) { return viewH < viewW ? viewH : viewW; }

void
deviceToScreen(float32 x, float32 y, float32 *sx, float32 *sy)
{
	float32 m = unit();
	*sx = x*m/viewW*rasterW;
	*sy = (1.0f - y*m/viewH)*rasterH;
}

void
deviceGetClippath(Path *p)
{
	float32 m = unit();
	float32 w = viewW/m, h = viewH/m;
	// corner order of the game: bottom-right, top-right, top-left, bottom-left of the view
	float32 c[4][2] = { { w, 0 }, { w, h }, { 0, h }, { 0, 0 } };
	Affine t = ctm[ctmTop];
	float32 det = t.a*t.d - t.b*t.c;
	p->empty();
	if(fabsf(det) < 1e-20f) t = identity(), det = 1;	// GUESS: singular CTM -> identity
	for(int i = 0; i < 4; i++){
		float32 x = c[i][0]-t.tx, y = c[i][1]-t.ty;
		float32 ox = (t.d*x - t.c*y)/det, oy = (-t.b*x + t.a*y)/det;
		if(i == 0) p->moveto(ox, oy); else p->lineto(ox, oy);
	}
	p->close();
}

// ---------------------------------------------------------------- Font
void fontTexDictionarySet(TexDictionary *d) { fontDict = d; }

#define RD(p, n) (stream->read8((p), (n)) == (uint32)(n))

static void
fontFree(Font *f)
{
	for(int i = 0; i < MAXFONTTEXTURES; i++)
		if(f->tex[i]) f->tex[i]->destroy();
	rwFree(f->highMap);
	rwFree(f->glyphs);
	rwFree(f);
	Font::numAllocated--;
}

void Font::destroy(void) { fontFree(this); }

static bool
finite(float32 f) { return f == f && fabsf(f) <= 3.0e38f; }

Font*
Font::streamRead(Stream *stream, bool32 requireTextures)
{
	Font *f = rwNewT(Font, 1, MEMDUR_EVENT);
	uint32 version;
	int32 n, i;
	if(f == nil){ RWERROR((ERR_ALLOC, (uint32)sizeof(Font))); return nil; }
	memset(f, 0, sizeof(*f));
	numAllocated++;
#define FAIL(...) do{ RWERROR((ERR_GENERAL, __VA_ARGS__)); fontFree(f); return nil; }while(0)

	// header: version, type, f32, f32, f32, i32
	if(!RD(&version, 4)) FAIL("RWF: truncated");
	if((version >> 24) == 0) FAIL("RWF: bad version %X", version);	// the game only tests the top byte
	{
		int32 hdr[5];
		if(stream->read32(hdr, 20) != 20) FAIL("RWF: truncated header");
		f->type = hdr[0];
		memcpy(&f->nominalHeight, &hdr[1], 4);
		memcpy(&f->baseline, &hdr[2], 4);
		memcpy(&f->intergap, &hdr[3], 4);
		f->flags = hdr[4];
	}
	if(f->type != 0) FAIL("RWF: only texture fonts (type 0) supported, got %d", f->type);	// DEVIATION: vector fonts (path glyphs) not implemented
	if(!finite(f->nominalHeight) || !finite(f->baseline) || !finite(f->intergap)) FAIL("RWF: non-finite header float");
	{
		int32 cnt[3];	// highBase ("numLowMap"), numGlyphs, numHighMap, file order
		if(stream->read32(cnt, 12) != 12) FAIL("RWF: truncated counts");
		f->highBase = cnt[0];
		f->numGlyphs = cnt[1];
		f->numHigh = cnt[2];
	}
	// DEVIATION: the game does not bound these; indices are int16 so > 0x7FFF glyphs can never be addressed
	if(f->numGlyphs < 0 || f->numGlyphs > 0x7FFF) FAIL("RWF: bad glyph count %d", f->numGlyphs);
	if(f->numHigh < 0 || f->numHigh > 0x10000) FAIL("RWF: bad high map size %d", f->numHigh);
	if(f->numHigh > 0){
		f->highMap = rwNewT(int16, f->numHigh, MEMDUR_EVENT);
		if(f->highMap == nil) FAIL("RWF: out of memory");
		if(stream->read16(f->highMap, f->numHigh*2) != (uint32)f->numHigh*2) FAIL("RWF: truncated high map");
	}
	if(stream->read16(f->lowMap, 256) != 256) FAIL("RWF: truncated low map");
	for(i = 0; i < 128; i++)
		if(f->lowMap[i] >= f->numGlyphs) FAIL("RWF: low map entry %d out of range", f->lowMap[i]);	// DEVIATION: the game would read out of bounds
	for(i = 0; i < f->numHigh; i++)
		if(f->highMap[i] >= f->numGlyphs) FAIL("RWF: high map entry %d out of range", f->highMap[i]);
	if(f->numGlyphs > 0){
		f->glyphs = rwNewT(Glyph, f->numGlyphs, MEMDUR_EVENT);
		if(f->glyphs == nil) FAIL("RWF: out of memory");
		memset(f->glyphs, 0, sizeof(Glyph)*f->numGlyphs);
	}
	for(i = 0; i < f->numGlyphs; i++){
		Glyph *g = &f->glyphs[i];
		float32 uv[4];
		uint8 ti;
		if(stream->read32(uv, 16) != 16 || stream->read32(&g->advance, 4) != 4 || !RD(&ti, 1)) FAIL("RWF: truncated glyph %d", i);
		g->u0 = uv[0]; g->v0 = uv[1]; g->u1 = uv[2]; g->v1 = uv[3];
		g->texIndex = ti;
		if(!finite(g->u0) || !finite(g->v0) || !finite(g->u1) || !finite(g->v1) || !finite(g->advance)) FAIL("RWF: non-finite glyph %d", i);
	}
	if(stream->read32(&n, 4) != 4) FAIL("RWF: truncated texture count");
	if(n < 0 || n > MAXFONTTEXTURES) FAIL("RWF: bad texture count %d", n);	// the game has room for 4 and does not check
	f->numTextures = n;
	for(i = 0; i < n; i++){
		if(!RD(f->texNames[i], 32)) FAIL("RWF: truncated texture name");
		f->texNames[i][31] = '\0';
	}
	for(i = 0; i < f->numGlyphs; i++)
		if(f->glyphs[i].texIndex >= n) FAIL("RWF: glyph %d uses texture %d of %d", i, f->glyphs[i].texIndex, n);
	// textures are looked up in the font texture dictionary (Rt2dFontTexDictionarySet)
	for(i = 0; i < n; i++){
		Texture *t = fontDict ? fontDict->find(f->texNames[i]) : nil;
		if(t){ t->addRef(); f->tex[i] = t; }
		if(requireTextures && (t == nil || t->raster == nil)) FAIL("RWF: texture '%s' not found", f->texNames[i]);
	}
#undef FAIL
	return f;
}

Font*
Font::streamReadChunk(Stream *stream, bool32 requireTextures)
{
	uint32 len, ver;
	if(!findChunk(stream, ID_FONT2D, &len, &ver)){
		RWERROR((ERR_CHUNK, "FONT2D"));
		return nil;
	}
	return streamRead(stream, requireTextures);
}

int32
Font::glyphIndex(uint32 code) const
{
	int32 g;
	if(code < 0x80) g = lowMap[code];
	else{
		int64 d = (int64)code - highBase;
		if(d < 0 || d >= numHigh) return -1;
		g = highMap[d];
	}
	return g < 0 ? -1 : g;	// negative int16 = no glyph (the game tests the sign)
}

// Decodes a NUL terminated string into char codes.
static void
decode(const Font *f, const void *text, std::vector<uint32> &out)
{
	out.clear();
	if(f->flags & FONT_UNICODE){
		const uint8 *p = (const uint8*)text;	// byte access: no alignment assumption, little endian like the x86 game
		for(;; p += 2){
			uint32 c = p[0] | p[1] << 8;
			out.push_back(c);
			if(c == 0) break;
		}
	}else{
		const uint8 *p = (const uint8*)text;
		for(;; p++){
			out.push_back(*p);
			if(*p == 0) break;
		}
	}
}

static float32
spanWidth(const Font *f, const uint32 *c, size_t from, size_t to)
{
	double w = 0.0;
	for(size_t i = from; i < to && c[i]; i++){
		int32 g = f->glyphIndex(c[i]);
		if(g >= 0) w = (double)f->glyphs[g].advance + f->intergap + w;
	}
	return (float32)w;
}

float32
Font::getStringWidth(const void *text, float32 height) const
{
	std::vector<uint32> c;
	decode(this, text, c);
	return spanWidth(this, c.data(), 0, c.size()) * height;
}

// ---- output
#ifdef RWDEVICE
static void
imSink(void*, Texture *tex, const Vertex *q, int32 nq)
{
	static uint16 idx[MAXBATCH*6];
	static RWDEVICE::Im2DVertex verts[MAXBATCH*4];
	static bool init;
	Camera *cam = devCamera ? devCamera : (Camera*)engine->currentCamera;
	if(cam == nil || tex == nil || tex->raster == nil) return;
	if(!init){
		for(int i = 0; i < MAXBATCH; i++){	// triangles (0,2,3) (0,3,1): same as the game's index buffer
			uint16 b = i*4, *x = &idx[i*6];
			x[0] = b; x[1] = b+2; x[2] = b+3; x[3] = b; x[4] = b+3; x[5] = b+1;
		}
		init = true;
	}
	float32 recipZ = 1.0f/cam->nearPlane;
	for(int i = 0; i < nq*4; i++){
		RWDEVICE::Im2DVertex *v = &verts[i];
		v->setScreenX(q[i].x);
		v->setScreenY(q[i].y);
		v->setScreenZ(rw::im2d::GetNearZ());
		v->setCameraZ(cam->nearPlane);
		v->setRecipCameraZ(recipZ);
		v->setColor(q[i].r, q[i].g, q[i].b, q[i].a);
		v->setU(q[i].u, recipZ);
		v->setV(q[i].v, recipZ);
	}
	rw::SetRenderStatePtr(rw::TEXTURERASTER, tex->raster);
	rw::SetRenderState(rw::TEXTUREFILTER, tex->filterAddressing & 0xFF);
	uint32 cull = rw::GetRenderState(rw::CULLMODE);
	uint32 ztest = rw::GetRenderState(rw::ZTESTENABLE);
	rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);	// GUESS: the game relies on whatever state its caller set up
	rw::SetRenderState(rw::ZTESTENABLE, 0);
	im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, verts, nq*4, idx, nq*6);
	rw::SetRenderState(rw::CULLMODE, cull);
	rw::SetRenderState(rw::ZTESTENABLE, ztest);
}
#endif

void setQuadSink(QuadSink s, void *user) { userSink = s; userSinkData = user; }

void
flushBatch(void)
{
	if(batchQuads){
		if(userSink) userSink(userSinkData, batchTex, batch, batchQuads);
#ifdef RWDEVICE
		else imSink(nil, batchTex, batch, batchQuads);
#endif
	}
	batchQuads = 0;
}

static uint8
lerpc(uint8 a, uint8 b, float32 t)
{
	return (uint8)((int32)lrintf((float32)a + ((float32)b - (float32)a)*t) & 0xFF);
}

static void
showSpan(Font *f, const uint32 *c, size_t from, size_t to, float32 height, V2d *pos, Brush *brush)
{
	// Rt2dFontShow: CTM push, translate(pos), scale(height), translate(0,-baseline); quads are unit-height boxes
	if(!ctmPush()) return;	// DEVIATION: the game carries on and corrupts the stack
	ctmTranslate(pos->x, pos->y);
	ctmScale(height, height);
	ctmTranslate(0, -f->baseline);
	Affine m = ctmGet();
	float32 total = spanWidth(f, c, from, to);
	float32 invW = total > 0 ? 1.0f/total : 0.0f;
	float32 acc = 0;
	for(size_t i = from; i < to && c[i]; i++){
		int32 gi = f->glyphIndex(c[i]);
		if(gi < 0) continue;
		const Glyph *g = &f->glyphs[gi];
		Texture *tex = f->tex[g->texIndex];
		if(batchQuads && (tex != batchTex || batchQuads >= MAXBATCH)) flushBatch();
		batchTex = tex;
		Vertex *v = &batch[batchQuads*4];
		float32 xs[2] = { acc, acc + g->advance }, ts[2] = { acc*invW, (acc + g->advance)*invW };
		for(int k = 0; k < 4; k++){	// 0=BL 1=TL 2=BR 3=TR
			int right = k >> 1, top = k & 1;
			float32 px, py;
			apply(m, xs[right], top ? 1.0f : 0.0f, &px, &py);
			deviceToScreen(px, py, &v[k].x, &v[k].y);
			v[k].u = right ? g->u1 : g->u0;
			v[k].v = top ? g->v0 : g->v1;
			const RGBA *a = top ? &brush->c[3] : &brush->c[0];	// left end colour
			const RGBA *b = top ? &brush->c[2] : &brush->c[1];	// right end colour
			if(!brush->gradient){ v[k].r = brush->c[3].red; v[k].g = brush->c[3].green; v[k].b = brush->c[3].blue; v[k].a = brush->c[3].alpha; }
			else{
				float32 t = ts[right];
				v[k].r = lerpc(a->red, b->red, t); v[k].g = lerpc(a->green, b->green, t);
				v[k].b = lerpc(a->blue, b->blue, t); v[k].a = lerpc(a->alpha, b->alpha, t);
			}
		}
		batchQuads++;
		acc += f->intergap + g->advance;
	}
	flushBatch();
	pos->x += acc*height;
	ctmPop();
}

Font*
Font::show(const void *text, float32 height, V2d *pos, Brush *brush)
{
	std::vector<uint32> c;
	decode(this, text, c);
	showSpan(this, c.data(), 0, c.size()-1, height, pos, brush);
	return this;
}

// Rt2dFontFlow. body == nil: only count lines.
static Font*
flowCore(Font *f, const void *text, float32 height, const float32 *rect, int32 format, Brush *brush, int32 *lines)
{
	std::vector<uint32> c;
	decode(f, text, c);
	if(lines) *lines = 0;
	if(!(height > 0.0f)) return nil;
	float32 lineW = rect[2]/height;
	float32 align = format == FLOW_CENTER ? height*0.5f : format == FLOW_RIGHT ? height : 0.0f;
	float32 ly = (rect[3] + rect[1]) - height;
	size_t cur = 0, start = 0, brk = 0;
	float32 w = 0, lastW = 0;
	bool first = true;
	int32 guard = 0;
	V2d pos;
	for(;;){
		uint32 ch = c[cur];
		if(ch == 0){
			pos.x = (lineW - w)*align + rect[0]; pos.y = ly;
			if(lines) (*lines)++;
			if(brush) showSpan(f, c.data(), start, cur, height, &pos, brush);
			return f;
		}
		int32 g = f->glyphIndex(ch);
		if(first && ch == 0x20){ first = false; brk = cur; lastW = w; }
		else{
			first = true;
			if(ch == 10){ brk = cur+1; lastW = w; }
		}
		if(g >= 0) w = (float32)((double)f->glyphs[g].advance + f->intergap + w);
		if(lineW < w || ch == 10){
			pos.x = (lineW - lastW)*align + rect[0]; pos.y = ly;
			if(lines) (*lines)++;
			if(brush) showSpan(f, c.data(), start, brk, height, &pos, brush);
			cur = brk;
			while(c[cur] == 0x20) cur++;	// c is NUL terminated, so this stops
			ly -= height;
			first = true;
			brk = start = cur;
			w = 0;
			// DEVIATION: the game has no iteration limit (a word longer than the box repeats until it leaves the box)
			if(ly < rect[1] || ++guard > 65536) return nil;
		}else
			cur++;
	}
}

Font*
Font::flow(const void *text, float32 height, const float32 *rect, int32 format, Brush *brush)
{
	return flowCore(this, text, height, rect, format, brush, nil);
}

int32
Font::measureFlow(const void *text, float32 height, float32 boxWidth, float32 *totalHeight) const
{
	float32 rect[4] = { 0, 0, boxWidth, 1.0e6f };
	int32 n = 0;
	flowCore(const_cast<Font*>(this), text, height, rect, FLOW_LEFT, nil, &n);
	if(totalHeight) *totalHeight = n*height;
	return n;
}

}
}
