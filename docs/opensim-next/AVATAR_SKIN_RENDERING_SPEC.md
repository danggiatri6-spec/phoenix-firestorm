# OpenSim-next Avatar Skin Rendering Specification

Status: source audit and implementation specification only. No runtime feature is claimed by this document.

Branch reviewed: `opensim-next/integration`
Baseline: Firestorm 7.2.5-derived viewer
Scope: traditional SL/OpenSim system Avatar skin rendering. GLTF attachments and scene-object PBR are separate paths.

## 1. Source audit

The following source files were inspected on the integration branch:

- `indra/newview/app_settings/shaders/class1/avatar/avatarF.glsl`
- `indra/newview/app_settings/shaders/class1/avatar/avatarV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/avatarV.glsl`
- `indra/newview/lldrawpoolavatar.cpp`
- `indra/newview/llviewertexlayer.cpp`
- `indra/newview/llviewershadermgr.cpp`

### Findings

1. The basic Avatar fragment shader delegates to `default_lighting()`. Its paired vertex shader performs skeletal transform, transforms the vertex normal, forwards UVs, and computes the legacy lighting result.
2. The inspected deferred Avatar fragment shader samples only `diffuseMap`, writes diffuse color to the G-buffer, writes a zero-valued secondary material target, and encodes the interpolated mesh normal. This shader does not implement a skin-specific SSS model or sample independent skin roughness/specular/normal maps.
3. The main deferred skinned Avatar draw path in `LLDrawPoolAvatar::beginDeferredSkinned()` binds `gDeferredAvatarProgram` and enables `DIFFUSE_MAP`. The inspected path does not bind dedicated skin normal, roughness, or SSS inputs.
4. The deferred impostor path separately enables normal and specular texture channels. That is not evidence that the full-resolution skinned Avatar path has equivalent material inputs.
5. The self-Avatar appearance compositor creates baked color output. `LLViewerTexLayerSetBuffer::doUpload()` reads color and morph-mask data and writes a five-component baked image (RGBA plus a mask/bump-related channel). This confirms that appearance layers are composited into baked output; it is not a general-purpose real-time layered PBR skin stack.
6. The shader manager has separate Avatar and GLTF/PBR shader families. GLTF PBR support must not be treated as proof that traditional system Avatar skin already receives the same material model.

These findings are limited to the inspected paths and do not claim that every legacy material or optional rendering path has been exhaustively audited.

## 2. Target design

Introduce an optional Avatar Skin Material path while preserving the existing appearance and animation systems.

### Material channels

- Base Color: existing baked Avatar color remains the compatibility fallback.
- Normal: optional tangent-space micro-normal map; neutral normal when absent.
- Roughness: optional scalar or packed material channel with a safe default.
- Specular: optional specular intensity/color, with energy-conserving limits.
- Subsurface: optional low-cost screen-space/profile approximation, disabled by default on lower tiers.
- Detail overlays: optional freckles, pores, age lines, makeup and scars as authored material layers or masks. These must not be inferred from the account holder's real age or identity.

### Rendering principles

- Preserve the current Avatar skeleton, skinning, morphs, baked texture protocol, attachments, alpha behavior and appearance permissions.
- Do not change the network appearance message or baked asset format in the first implementation phase.
- Existing avatars without new material assets must render identically or within a documented tolerance.
- Material data must be optional and locally degradable; no missing map may produce a black, shiny or invalid surface.
- Keep face, body, eyelash, eye and hair rendering paths distinct where their alpha and shading requirements differ.
- Do not route GLTF attachment materials through the traditional Avatar skin shader.

## 3. Implementation phases

### Phase A — Instrumentation and baseline

- Capture fixed-camera screenshots of a neutral test Avatar under controlled key, fill and back lighting.
- Record GPU time, draw calls, texture memory and shader compile failures.
- Include self Avatar, remote Avatar, impostor transition, alpha-tested eyelashes and low graphics settings.
- Establish visual comparison tolerances before changing shaders.

### Phase B — Material data contract

- Define a viewer-local AvatarSkinMaterial structure and defaults.
- Add optional map references and scalar parameters without changing the legacy appearance protocol.
- Define cache lifetime, texture residency, validation and fallback behavior.
- Keep the first version local/test-only until an interoperable asset distribution and permission model is specified.

### Phase C — Full-resolution shader integration

- Add a dedicated skinned Avatar skin shader variant rather than overloading the generic object PBR shader.
- Bind optional normal, roughness and specular channels in the full-resolution Avatar draw path.
- Preserve the existing G-buffer layout initially; document any required extension before changing render targets.
- Ensure impostor rendering either receives a deliberate simplified material approximation or cleanly falls back.

### Phase D — Subsurface approximation

- Add a restrained, low-cost skin-lighting approximation only after normal and roughness channels are validated.
- Avoid expensive multi-pass diffusion as the default.
- Add quality-tier controls and explicit performance budgets.
- Verify that lighting remains stable across front/back lighting and does not wash out skin color.

### Phase E — Authoring and compatibility

- Add creator-facing controls only after the material runtime is stable.
- Define asset ownership, permissions, transfer/copy behavior and cache invalidation.
- Preserve legacy avatars and existing baked textures.
- Do not silently modify user-created skin assets.

## 4. Quality tiers

- Low: legacy baked color and current Avatar lighting; no additional material maps.
- Medium: optional normal and roughness, conservative sampling.
- High: normal, roughness and specular controls with improved skin response.
- Ultra: High plus optional subsurface approximation and authored micro-detail.

All tiers must retain a fallback path and must not force higher texture residency on remote avatars.

## 5. Acceptance criteria

A phase is not complete because source code compiles or a setting exists. Required checks include:

- Windows x64 ReleaseOS build.
- Login and appearance tests on an OpenSim grid.
- Second Life compatibility smoke test using the already validated viewer login path.
- Self and remote Avatar rendering, including baked texture updates and impostor transitions.
- Eye, eyelash, hair, alpha, morph, skeleton and animation regression checks.
- GPU timing and memory measurements on Low, Medium, High and Ultra profiles.
- Before/after screenshots under reproducible lighting.

## 6. Explicit non-goals for the first implementation

- No change to two-Avatar interaction behavior.
- No replacement of the existing animation, eye gaze, blink, facial expression or lip-sync systems.
- No change to the server-side baked texture format.
- No claim of GTA-level skin rendering until measured visual and performance results demonstrate it.


## 7. Phase B — Material data contract (design decision)

### 7.1 Keep three material systems separate

The source audit found an existing `LLMaterial` class in `indra/llprimitive/llmaterial.h/.cpp`. It represents the legacy object-material protocol, including normal/specular texture IDs, UV transforms, specular color/exponent, environment intensity and alpha behavior. GLTF/PBR has its own material family.

Do not extend either class to represent Avatar skin. Reusing them would conflate object-face material semantics, GLTF PBR semantics and the Avatar appearance/bake lifecycle. The Avatar skin contract is a separate viewer-side domain model.

### 7.2 Proposed viewer-side type

Initial implementation target (not yet created):

- Header: `indra/newview/llavatarskinmaterial.h`
- Source: `indra/newview/llavatarskinmaterial.cpp`
- Namespace: `LLAvatarSkin`
- Value type: `MaterialData`

The value type should contain only stable material description data, not owning GPU objects or raw `LLViewerTexture*` pointers.

Proposed fields:

| Field | Type / default | Purpose |
|---|---|---|
| schema_version | U32 / 1 | Version the optional local material description |
| base_color | existing baked texture / unchanged | Compatibility color source; no new required map |
| normal_map_id | LLUUID / null | Optional tangent-space detail normal |
| roughness_map_id | LLUUID / null | Optional roughness channel |
| specular_map_id | LLUUID / null | Optional specular intensity/color data |
| detail_mask_id | LLUUID / null | Optional mask for pores, freckles, lines or makeup |
| roughness_factor | F32 / 0.65 | Scalar fallback when no roughness map exists |
| specular_factor | F32 / 0.15 | Conservative scalar fallback |
| subsurface_strength | F32 / 0.0 | Disabled by default |
| subsurface_color | LLColor4 / neutral warm default | Tunable scattering tint, clamped by validation |
| detail_strength | F32 / 0.0 | Disabled by default |
| enabled | bool / false | Explicit opt-in; legacy rendering remains default |

The proposed defaults are starting engineering values, not calibrated skin-science constants. They must be tuned against reference captures.

### 7.3 Validation and ownership rules

- Null map UUID means the channel is absent; shader code must use a neutral constant or skip the sample.
- Clamp all scalar values to documented safe ranges at the data boundary.
- Reject unsupported schema versions and fall back to legacy Avatar shading.
- Store UUIDs and scalar metadata in the value object; resolve UUIDs to viewer-managed fetched textures through a separate resolver/cache layer.
- Do not retain raw GPU texture pointers in persistent Avatar appearance data.
- Material lifetime must follow the Avatar instance and texture cache rules; removal/replacement must release references safely.
- Do not serialize this structure into existing appearance messages, baked texture comments, wearable assets or object-material LLSD in the first iteration.

### 7.4 Ownership boundary

The initial owner should be a viewer-side Avatar rendering component associated with `LLVOAvatar`, not `LLAvatarAppearance` and not `LLTexLayerSet`.

Reason: `LLAvatarAppearance` is shared appearance/skeleton infrastructure, while `LLTexLayerSet` is the self-Avatar compositing/bake path. A viewer-only render material should not become a new appearance protocol dependency or be accidentally baked into the legacy color texture.

A small accessor on `LLVOAvatar` may expose immutable `MaterialData` to the draw pool once implementation begins. The draw pool should not own asset parsing or network serialization.

### 7.5 First implementation boundary

The first code change should add only the value type, defaults, validation and unit tests. It must not yet:

- bind new shader samplers;
- alter `LLDrawPoolAvatar` draw state;
- change shader source or G-buffer layout;
- change avatar appearance messages, wearable assets or bake output;
- add a creator UI or upload workflow.

This isolates data-contract correctness before touching the render path.


## 8. Phase B implementation status

The first viewer-local data-contract implementation has been added:

- `indra/newview/llavatarskinmaterial.h`
- `indra/newview/llavatarskinmaterial.cpp`
- Both files are registered in `indra/newview/CMakeLists.txt`.

Implemented behavior:

- Schema version 1.
- Optional UUIDs for normal, roughness, specular and detail-mask maps.
- Conservative scalar defaults; subsurface and detail are disabled by default.
- A `sanitize()` method clamps scalar and color channels to [0, 1].
- Unsupported schema versions disable the optional material path.
- The structure contains UUIDs and scalar data only; it does not own GPU texture pointers.

Not yet completed:

- No unit test has been added or run.
- No Windows x64 ReleaseOS build has been run.
- No runtime rendering path consumes this structure yet.
- No texture resolver/cache or asset distribution protocol exists yet.

This is source implementation only, not a build- or runtime-verified feature.


### Phase B unit-test coverage added

A Viewer TUT test source has been added at `indra/newview/tests/llavatarskinmaterial_test.cpp`, and `llavatarskinmaterial.cpp` is registered in the Viewer unit-test source list under `LL_TESTS`.

The test cases cover:
- Legacy-safe defaults and absent optional maps.
- Clamping of roughness, specular, subsurface, detail and color channels.
- Disabling an unsupported schema version.
- Null optional texture UUIDs remaining valid.


- Non-finite floating-point inputs (NaN and positive/negative infinity) are normalized to safe [0, 1] values; NaN and negative infinity become zero, positive infinity becomes one.
- A fifth TUT case covers these non-finite inputs.

These tests are authored and registered but have **not yet been compiled or executed**. A passing test result must not be inferred from source registration.

## Phase B2: Viewer texture resolver

The first Viewer-side resolver adapter is now present:

- `indra/newview/llavatarskintextureresolver.h`
- `indra/newview/llavatarskintextureresolver.cpp`

The resolver keeps `MaterialData` UUID-only and stores its own `LLPointer<LLViewerFetchedTexture>` references in a per-resolver cache. It obtains textures through `LLViewerTextureManager::getFetchedTexture()`, reusing the existing `gTextureList` lookup/fetch/cache path rather than creating a parallel texture system.

Resolver state values are:

- `UNASSIGNED`: null UUID or no cached entry yet.
- `LOADING`: a cached texture has not completed fetching.
- `READY`: the texture reports complete downloaded dimensions and no active fetcher. This does not prove GPU upload or successful rendering.
- `MISSING`: the Viewer texture reports a missing asset.
- `DELETED`: a cached texture has entered the deleted state.

A deleted entry is discarded and may be resolved again. `clear()` releases only the resolver's references; it does not directly destroy textures managed by the Viewer texture list. The `release(texture_id)` method also drops one no-longer-used UUID entry, so a future material owner can release superseded maps without clearing unrelated active maps.

Current scope and limitations:

- The resolver is registered in the Viewer CMake source/header lists.
- It is not yet owned by `LLVOAvatar`; Avatar construction, `markDead()`, and destruction wiring remain a separate integration step.
- No draw-pool, shader, sampler, material upload, wearable, bake, or network protocol changes have been made.
- No resolver unit test, Windows build, or runtime texture-fetch test has been executed yet.
- Before shader integration, define fallback behavior for missing/failed optional maps and verify the GL texture is usable at bind time.

### Phase B3: Avatar lifetime ownership

The resolver is now held as a value member by `LLVOAvatar` and exposed through `getSkinTextureResolver()` for later Viewer-side material integration. Both `markDead()` and the Avatar destructor call `clear()`; this is intentionally idempotent and releases only the Avatar's retained `LLPointer` references. Self and remote Avatars therefore have separate resolver caches while continuing to share the global Viewer texture manager.

A focused TUT test file now covers the resolver's null-UUID, empty-cache, and safe release-of-uncached-UUID behavior, and the resolver source is registered in the Viewer unit-test source list. These tests do not exercise a real texture fetch, missing-asset transition, GPU upload, or Avatar destruction. No build or test execution has been performed.


## Phase B4: Avatar material binding

`LLVOAvatar` now owns a viewer-local `MaterialData` value alongside its per-Avatar texture resolver. The public API exposes the material as a const reference and accepts replacements through `setSkinMaterial()`; callers cannot mutate the stored value while bypassing validation or cache reconciliation.

The setter copies and sanitizes incoming data, disables unsupported schema versions, and computes unique active texture UUID sets from the four optional map slots. It releases an old resolver entry only when that UUID is absent from every slot in the replacement material. Reusing one UUID in Normal and Roughness (or any other combination) therefore does not cause premature release. Disabled or unsupported materials have no active map references; their UUID metadata is retained in the value but cached resolver references are released.

Texture fetching remains lazy: setting a material does not initiate network fetches. The draw path has not yet been connected to these accessors, and no shader or rendering behavior changes in this phase. Unit coverage now includes unique UUID collection when slots share a texture. Tests are authored and registered but have not been compiled or executed.
