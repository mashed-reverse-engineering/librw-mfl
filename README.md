# librw-mfl

This is a fork of [aap/librw](https://github.com/aap/librw) for the
[Mashed: Fully Loaded recompilation](https://github.com/mashed-reverse-engineering/MFL-recomp).
The game is built on RenderWare Graphics 3.7. The goal is to replace the closed-source
RenderWare libraries that the recompiled game links against with open code (MIT, no binary blobs).
Upstream behaviour is kept; our changes are additive and listed below.

## What we change

- **Adopt-first fixes** (patches 0001-0007): matfx effect index bound, cube/unsupported native
  textures in the D3D9 backend, `correctPathCase` overflow, long texture/mask names, FILLMODE render
  state, allocation guards, hardened compressed texture conversion.
- **BSP world loader** (`src/world.cpp`, spec in `SPEC_BSP.md`): the game's tracks are BSP worlds.
- **PI-TXD reader** (`src/pitexd.cpp`): the game's platform-independent texture dictionaries.
- **Rt2d** (`src/rt2d.cpp`): fonts, brushes, CTM and paths used by the menus.
- **PTank** (`src/ptank.cpp`): the particle tank atomic plugin (lock/render).
- **Collision** (`src/collision.cpp`): BSP collision tree and queries.

Each piece was written against the game's observed behaviour (decompilation-informed, no SDK source)
and checked by differential tests against the original code (`tests/diff*`, reviews in `REVIEW.md`
and `tests/diff_*/REVIEW.md`). `patches/` has the changes as separate patches; `MFL_NOTES.md` has
the working notes.

## Not done yet

- The D3D9 backend is shader-only; the original renders through fixed function. The plan is in
  `RX_D3D9_PLAN.md`.
- A review of the existing librw parts against RenderWare 3.7 (the game's version may differ from
  the one upstream targets).
- The closed-source RenderWare physics (Rwp) is not part of this fork.

Upstream README follows.

---

librw
=====

This library is supposed to be a re-implementation of RenderWare graphics,
or a good part of it anyway.

It is intended to be cross-platform in two senses:
support rendering on different platforms similar to RW;
supporting all file formats for all platforms at all times and provide
way to convert to all other platforms.

Supported file formats are DFF and TXD for PS2, D3D8, D3D9 and Xbox.
Not all pre-instanced PS2 DFFs are supported.
BSP is not supported at all.

For rendering we have D3D9 and OpenGL (>=2.1, ES >= 2.0) backends.
Rendering some things on the PS2 is working as a test only.

# Uses

librw can be used for rendering [GTA](https://github.com/gtamodding/re3).

# Building

Get premake5. Generate a config, e.g. with ``premake5 gmake``,
and look in the build directory.
