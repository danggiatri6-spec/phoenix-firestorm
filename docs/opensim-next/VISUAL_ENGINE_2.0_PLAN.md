# OpenSim-next Visual Engine 2.0 — Engineering Plan

Status: architecture and source audit plan; implementation and runtime validation are not implied.

Target branch: `opensim-next/integration`  
Baseline: Firestorm-derived Viewer 7.2.5  
Compatibility: preserve Second Life and OpenSim login, legacy materials, GLTF/PBR assets, attachments, animation, scripting, and existing content behavior.

## 1. Goal

Move the OpenSim-next Viewer toward a more physically convincing, cinematic real-time presentation, using modern open-world games as a visual reference. This is a staged modernization of the existing Firestorm renderer, not a renderer replacement and not a claim of parity with GTA 6.

Primary visual goals:

- consistent physically based material response;
- stable, readable dynamic lighting and shadows;
- convincing reflections for water, glass, and metal;
- richer sky, cloud, fog, and weather presentation;
- more lifelike Avatar skin, eyes, hair, and clothing;
- restrained post-processing and cinematic color management;
- scalable performance from integrated GPUs to high-end GPUs.

## 2. Existing source foundations confirmed

The following are present in the inspected `opensim-next/integration` source. Presence in source does not by itself prove a feature is enabled, correct on every GPU, or performant at runtime.

| Area | Existing source evidence | Engineering interpretation |
|---|---|---|
| GLTF/PBR material data | `indra/llprimitive/llgltfmaterial.h/.cpp` | Base color, normal, metallic-roughness/ORM, emissive, alpha mode, double-sided state, and texture transforms already have data structures. |
| PBR shader variants | `indra/newview/llviewershadermgr.cpp`; `app_settings/shaders/class1/deferred/pbropaqueF.glsl` | Opaque, alpha, skinned, glow, terrain and GLTF shader paths exist. |
| Deferred renderer and render targets | `indra/newview/pipeline.cpp` | Existing deferred pipeline is the extension point; do not replace it as the first step. |
| Shadow controls | `pipeline.cpp`, shader manager, deferred shadow shaders | Shadow quality and filtering are already configurable; audit quality/performance before changing defaults. |
| Screen-space reflections | `pipeline.cpp`, `screenSpaceReflUtil.glsl`, shader manager | SSR-related controls and shader utility exist; validate coverage, artifacts, and water integration. |
| Reflection probes | shader manager and pipeline reflection-map manager | Probe infrastructure exists; audit update policy and visual contribution. |
| Post processing | deferred tonemap, exposure, gamma-correction shaders and shader-manager programs | Existing tone-map/exposure path should be measured and tuned rather than duplicated. |
| Avatar rendering | `app_settings/shaders/class1/avatar` and deferred avatar shaders | Existing shader paths provide a base for skin/eye/hair work; detailed capability audit remains open. |
| Performance instrumentation | Tracy hooks and `llperfstats` noted in the rendering audit | Reuse existing instrumentation; do not add a second profiler before checking coverage. |

### Known material limitation to preserve in future work

The current `LLGLTFMaterial` declaration maps occlusion to the metallic-roughness texture slot and documents that separate occlusion texture data is currently ignored in favor of ORM. The first PBR work package must explicitly test this behavior and avoid silently changing serialized assets or server expectations.

## 3. Architectural rules

1. Keep Firestorm's existing OpenGL renderer as the production path for this milestone.
2. Extend existing shader-manager, pipeline, render-target, material, and settings abstractions.
3. Keep legacy SL material rendering and GLTF/PBR rendering operational side by side.
4. Add quality tiers and feature detection; never force expensive effects on all users.
5. Do not change network protocol, asset serialization, LSL, permissions, or simulator-authoritative behavior as part of rendering work.
6. Do not introduce Vulkan or a wholesale renderer rewrite until the OpenGL path has a measured baseline and a demonstrated architectural limit.
7. Keep Avatar animation/face-control interfaces independent from shader implementation. Rendering must consume pose, morph, and expression data without changing their semantics.

## 4. Target module boundaries

These are logical boundaries first; they do not require creating seven new C++ subsystems immediately.

- **Render Core:** shader lifecycle, render targets, feature detection, GPU resource lifetime, debug views.
- **PBR Material:** GLTF material inputs, texture transforms, normal/tangent correctness, ORM handling, alpha modes, compatibility fallback.
- **Lighting:** sun/moon/environment lighting, shadow maps, cascades, bias, filtering, ambient contribution.
- **Reflection:** SSR, reflection probes, water-specific handling, roughness-aware fallback.
- **Atmosphere:** sky and cloud shading, fog, haze, time-of-day transitions, later weather effects.
- **Character Rendering:** skin response, eye/tearline/cornea, hair transparency and shading, cloth response; integrate with existing Avatar skeleton and face animation.
- **Post Processing:** exposure, tone mapping, color grading, anti-aliasing, optional bloom/DOF/vignette with restrained defaults.
- **Quality/Telemetry:** Low, Medium, High, Ultra profiles; GPU capability checks; frame-time and memory reporting.

## 5. Implementation phases and acceptance gates

### Phase 0 — Reproducible baseline and capability matrix

Tasks:
- record exact commit, Windows build configuration, GPU/driver, resolution, graphics preset, draw distance, Avatar count, and test scene;
- create a repeatable scene containing terrain, legacy prim materials, mesh, GLTF/PBR, alpha surfaces, water, glass/metal, vegetation, and several Avatars;
- capture CPU/GPU frame time, average FPS and 1% lows, RAM/VRAM, shader compile stutter, texture stalls, and scene-ready time;
- record which features actually render on the target GPU, not just which symbols exist in source.

Gate: baseline report and repeatable test scene exist. No visual optimization claim before this gate.

### Phase 1 — Quality profiles and diagnostics

Tasks:
- map current settings to Low/Medium/High/Ultra profiles without changing the existing default unexpectedly;
- implement safe capability checks and graceful fallback for unsupported shader paths;
- expose render-buffer/debug visualizations useful for diagnosing normals, depth, roughness/metal channels, shadows, and reflection inputs;
- ensure profile changes do not require relogging where the existing settings system supports live refresh.

Gate: each profile launches and renders the same test scene; Low remains usable on modest GPUs; no regression to legacy materials.

### Phase 2 — PBR correctness and consistency

Tasks:
- validate base color/sRGB, tangent-space normals, metallic-roughness/ORM channel interpretation, emissive, alpha mask/blend, double-sided materials, and texture transforms;
- test static mesh, rigged mesh, terrain, and object material overrides;
- document unsupported glTF extensions and define a compatibility fallback;
- evaluate separate occlusion texture support only after asset/protocol compatibility is understood.

Gate: reference materials match expected glTF behavior within the Viewer renderer; legacy materials remain unchanged.

### Phase 3 — Lighting and shadows

Tasks:
- profile shadow-map passes and filtering independently;
- improve cascade stability, bias, acne/peter-panning trade-offs, and temporal shimmer;
- validate sun/moon transitions, indoor/outdoor contrast, and Avatar self/receiver shadows;
- tune quality profiles against measured GPU frame time.

Gate: fewer visible shadow artifacts without unacceptable frame-time or memory cost.

### Phase 4 — Reflections and water

Tasks:
- test SSR on water, glossy floors, glass, and metallic objects;
- document SSR limitations (off-screen objects, disocclusion, transparency, thin geometry);
- tune reflection probes and update scheduling;
- provide probe/environment fallback where SSR cannot produce a valid result;
- keep expensive glossy sampling optional.

Gate: stable reflections without obvious ghosting or severe performance spikes in the reference scene.

### Phase 5 — Atmosphere and weather foundation

Tasks:
- audit existing sky, cloud, fog, and atmospheric shaders;
- improve visual continuity across time-of-day and horizon transitions;
- add weather as a later opt-in layer: rain particles, wind response, wetness parameters, and local fog;
- keep weather presentation client-side unless a future simulator protocol explicitly coordinates shared world state.

Gate: consistent day/night and weather transitions with predictable performance and no protocol changes.

### Phase 6 — Avatar material realism

Tasks:
- audit current avatar skin, eye, hair, alpha, and cloth shader paths;
- improve skin shading only where existing material/mesh data supports it; do not fake anatomical detail with a global tint;
- improve eye highlights/refraction-like appearance within the current renderer's feasible limits;
- reduce hair alpha sorting artifacts and preserve attachment compatibility;
- coordinate shader work with the existing eye-gaze, blink, head, lip-sync, and expression systems without changing animation authority.

Gate: representative system avatars and user-created avatars both render correctly; attachments, alpha clothing, and face animation remain functional.

### Phase 7 — Cinematic post-processing

Tasks:
- validate exposure adaptation and tone mapping under bright sun, shade, indoor light, and night scenes;
- add restrained optional color grading and bloom;
- keep depth of field and vignette optional and disabled or conservative by default;
- avoid effects that reduce readability in social/creator workflows.

Gate: cinematic preset improves presentation without obscuring UI, chat, object editing, or Avatar readability.

## 6. Performance tiers

| Tier | Intended behavior |
|---|---|
| Low | Conservative shadows, reduced reflection work, no expensive optional post effects, stable fallback shaders. |
| Medium | Standard PBR, moderate shadows, selected reflection features, modest post-processing. |
| High | Higher shadow resolution/filtering, richer probes/SSR, improved atmosphere, optional bloom. |
| Ultra | Highest supported settings and optional advanced effects; explicit GPU/VRAM checks and warnings. |

Do not use a single hard-coded GPU model list as the only compatibility test. Prefer capability checks plus measured safe presets.

## 7. First actionable engineering tickets

1. Produce a source-to-setting map for PBR, shadows, SSR, reflection probes, atmosphere, Avatar, and post-processing.
2. Build the Phase 0 reference scene and benchmark capture template.
3. Audit PBR channel handling and the current separate-occlusion limitation.
4. Audit shader fallbacks and graphics-level feature gates.
5. Only then select the first code change based on a reproducible defect or measured bottleneck.

## 8. Definition of done

A phase is complete only when its code is built in the OpenSim-enabled Windows x64 `ReleaseOS` configuration, tested on an OpenSim Grid, checked for relevant Second Life compatibility, and accompanied by measured results and known limitations.

A source audit or merged design document is not a successful build, runtime validation, or performance result.
