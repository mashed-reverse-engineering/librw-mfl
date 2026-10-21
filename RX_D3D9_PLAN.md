# Rx pipeline / D3D9 hooks: study and plan (2026-10-19)

Source: read-only Ghidra study of MFL.exe (decompilation-informed, no SDK source). Raw material was in the study's scratch dir (xrefs, decompiles of 0x4a5b00-0x4aab00 and the RW D3D9/Rx/matfx functions).

## Summary
1. `rxD3D9VertexShaderDefaultEndCallBack` (0x50b5e0, 150 refs) is a lone RET: the linker's ICF merged every empty `void f(void){}` into it. No game code depends on the RW vertex-shader pipeline. Nothing to implement.
2. 0x4fb460 (named "skin lighting callback") is `RxD3D9AllInOneSetRenderCallBack` (same bytes, ICF-folded). Also misleading: 0x4dbc40 "DeleteVertexShader" also deletes pixel shaders; 0x550190 "WorldSectorEnableEffects" is also the atomic enable. Do not implement against those delink names.
3. The data has no skinned geometry (no 0x116 chunk in 1389 DFFs/BSPs). HAnim only for frame hierarchies.
4. The game's Rx usage is tiny: one-node pipelines `Create -> Lock -> AddFragment(locked,NULL,AllInOneNodeDef,0) -> Unlock`, then swap the node render callback (FindNodeByName + RxD3D9AllInOneSet/GetRenderCallBack), destroy at shutdown. A general Rx implementation is NOT needed: game render callbacks on `d3d9::ObjPipeline::renderCB` cover it (optionally ~50-line Rx shim: pipeline = ObjPipeline, FindNodeByName returns it, Set/Get = renderCB).
5. The real gap is FIXED-FUNCTION D3D9 parity. The original (RW 3.7) renders through fixed function: D3D lights, D3DMATERIAL9, TEXTUREFACTOR material colour, D3DTS_WORLD/VIEW/PROJECTION, D3DRS_FOG*. The game's own callbacks assume that state. librw-mfl's D3D9 backend is shader-only (defaultRenderCB_Shader; defaultRenderCB_Fix commented out); its d3d9 matfx renders only env; dual, uv-transform and bump fall back to default.

## Game pipelines (all live)
- A `g_app_771030`, render callback 0x4a9ff0: course props, one atomic drawn at up to 48 frames (reverse frame order, WORLD per instance, CLIPPING inherited).
- B `g_object_set_6c3eb0`, callback 0x4a6860: JaCFX atomic pipeline (Dem/DemR buildings, rouabout SHINEBILD).
- C `g_object_set_6c3eb4`, callback 0x4a6720: JaCFX world-sector pipeline, set on every sector of a world with JaCFX materials (13 track BSPs).
- JaCFX types 1-4 on shader-capable HW (VS and PS above 1.0): own vs_1_1 + three ps_1_1 shaders (bytecode in the exe at 0x5e1838, 0x5e1920, 0x5e1980, 0x5e19f0) draw a second texture projected in screen space (fake reflection/glow). Without shaders: tex0 swapped in per frame (RpMaterialSetTexture churn), callback_b.
- Dead code: pipelines g_app_771054/771048, material pipeline 6c3eb8, fixed-function projected-texture fallback (g_app_771058 == 2).
- Game material plugin 0x26990003 (0x30 bytes, cfxType/tex0/tex1/...), atomic plugin 0x26990004 (distance culling, fading, deferred list).
- MatFX in data: dual 160 materials (all SRCALPHA/INVSRCALPHA), uv-transform 57, dual+uv 2, env 2, bump-env 1.

## What librw already has / must add
Has: ObjPipeline{instanceCB,uninstanceCB,renderCB}, RW-layout InstanceData, d3d9 state cache with deferred states (same model as RW), setTexture with stage-0 alpha logic (but binds whiteTex for NULL), setIndices/setStreamSource/VertexDeclaration/shaders, setD3dMaterial, simplified lightingCB_Fix, env-only matfx.
Must add: (1) fixed-function compatibility layer (setTransform with cache, VIEW/PROJECTION at camera begin, fog by caps, RW light enable lists, setSurfaceProperties as in RW, device NULL texture, RW default state block from rwD3D9RenderStateOpen 0x4e4320, anisotropy, ZWRITE/ZENABLE coupling); (2) RW 3.7 default fixed-function callbacks for atomics and sectors (incl. black/unlit path, texture mode table, "fully inside" frustum test); (3) sectors as pipeline objects (WorldSector::pipeline, per-sector instData without temp Atomic, sector render callback get/set, sector lighting); (4) game pipelines A/B/C + dispatch, callback_a/b, shader draw, embedded shader bytecode, plugins; (5) matfx fixed-function (default+uv, dual one/two pass, env, black, uvanim); (6) line path (FVF cache, DrawPrimitiveUP with setStreamSource(0,nil)).

## Plan (about 7-9 working days)
| # | Piece | Effort | Differential test |
|---|---|---|---|
| 0 | Trace oracle: apitrace d3d9 wrapper or ~300-line d3d9.dll proxy under Wine (WINEDLLOVERRIDES=d3d9=n,b) in the wine-run container; deterministic scripted runs (menu, cup scene, Forest, City/HighwayN/NT1 JaCFX, rain course); normaliser writes one record per draw (render states, TSS, samplers, texture content hashes, transforms, VS/PS bytecode hashes, VS constants, material, lights, decl, prim type/count); log caps | 0.5-1 d | two recordings must be identical |
| 1 | Fixed-function layer | 1.5 d | unit tests vs transcribed RW rules; menu trace |
| 2 | Default atomic fixed-function callback | 1 d | per-draw records vs oracle (cup scene, vehicle select, characters) |
| 3 | Sector pipelines and lighting | 1 d | Forest race draw counts, records, CLIPPING |
| 4 | Game pipelines A/B/C, plugins, JaCFX, shaders | 1-1.5 d | City/HighwayN/NT1/sands/Dem with no_multitexture 0 and 1; VS constants (ulp tolerance) |
| 5 | MatFX fixed-function + uvanim | 1.5-2 d | vehicles (env), Dem/Forest (dual), uvanim courses, two caps profiles |
| 6 | Line path | 0.25 d | rain course DrawPrimitiveUP records |
| 7 | Recomp wiring: replace Rx calls and the +0x6c/+0x7c/+0x08 stores, drop dead pipelines | 0.5 d | full regress + imgdiff |
| 8 | Shader/GL3 port | large | image diff vs pieces 2-5 |

## Risks
State-cache order differs on first touch (compare per-draw snapshots); callbacks inherit state so draw order is part of correctness; NULL texture semantics (original binds NULL, librw whiteTex); caps-dependent branches (matfx dual 1/2 pass, fog mode, anisotropy, max lights, shader flag: wined3d reports SM3); shaders are vs_1_1/ps_1_1 (GL3 needs translation); strip-to-list conversion differences in instancing; the per-frame RpMaterialSetTexture in mode 0 must be reproduced; the rw~ functions were not diffed against the reference libs: the exe's behaviour is the spec.
