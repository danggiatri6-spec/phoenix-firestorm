# OpenSim-next Native Avatar Skin Layer System

Status: architecture specification; implementation not yet enabled.
Branch: `opensim-next/integration`
Scope: native SL/OpenSim Avatar skin appearance. This is not a mesh-body replacement and does not change legacy Bake payloads.

## 1. Goals

- Add editable, ordered skin layers for color and surface appearance.
- Preserve existing SL/OpenSim wearable, tattoo, alpha-mask, UV, Bake and appearance behavior when the feature is disabled or unsupported.
- Keep the runtime rendering cost bounded on low-end PCs.
- Reuse the existing Avatar UVs and texture manager; do not add geometry or require HD meshes.
- Treat missing or delayed optional maps as non-fatal and fall back to the legacy diffuse result.

Non-goals for the first release:
- Unlimited independently sampled textures in the Avatar fragment shader.
- Replacing the legacy compositor or server/host Bake protocol.
- Changing the existing Avatar UV layout.
- Requiring new assets for existing users.

## 2. Layer model

The editor stores an ordered logical stack. Each layer has:
- stable layer UUID and layer type;
- optional color/albedo texture UUID;
- optional mask texture UUID;
- opacity in [0,1];
- blend mode (NORMAL, MULTIPLY, ADD, SOFT_LIGHT; unsupported modes are rejected);
- enabled flag and order;
- optional region mask (face, upper body, lower body, arms, legs) only where the source UV/layout can represent it safely.

Initial logical layer types:
1. BASE_REFERENCE (read-only view of the existing baked skin; not copied or modified);
2. COLOR_OVERLAY (tint, redness, discoloration);
3. MICRO_DETAIL (pores, fine lines, freckles, moles, scars);
4. MAKEUP (blush, lip/eye color);
5. TATTOO_OVERLAY (kept compatible with existing tattoo wearables; no duplicate application);
6. SURFACE_NORMAL;
7. SURFACE_ROUGHNESS;
8. SURFACE_SPECULAR;
9. SUBSURFACE_MASK.

A layer with a missing texture is skipped, not treated as black. Invalid UUIDs, unsupported schema versions, NaN values and out-of-range values are sanitized before use.

## 3. Bounded runtime representation

Do not bind every logical editor layer as a separate sampler. Flatten enabled logical layers into a bounded runtime set:

- Skin overlay composite: RGBA color + alpha.
- Skin normal composite: tangent-space normal, only when valid normal layers exist.
- Skin material composite: packed channels (R = roughness, G = specular strength, B = subsurface mask, A = reserved/detail mask).

The existing legacy baked diffuse remains sampler 0 and remains authoritative for base color, clothing and legacy appearance. Optional composites are additive enhancements.

Initial shader budget: at most three optional 2D samplers (overlay, normal, packed material). Low profile can disable all optional maps; Medium may enable overlay only; High/Ultra may enable all supported maps. Actual texture-unit availability must be checked before shader activation.

The logical layer count is an editor/content limit, not a shader sampler count. Initial editor limit: 16 enabled logical layers, with a smaller configurable memory budget per Avatar. The compositor merges them into the bounded runtime textures.

## 4. Rendering and compositing

- Use the existing Avatar UV0 for all skin maps. Do not infer or generate alternate UVs.
- Color overlays use premultiplied-alpha compositing and explicit blend-mode equations in the compositor. Alpha must never erase the legacy diffuse.
- Normal layers are decoded and combined in tangent space with a defined re-normalization step. If tangent basis support is unavailable for a particular Avatar draw path, disable normal mapping for that path rather than use an incorrect basis.
- Roughness/specular/subsurface channels are clamped and interpreted consistently in the deferred Avatar material output.
- The legacy diffuse alpha test and alpha-mask behavior remain unchanged.
- Impostor rendering must either receive a correctly generated representation or fall back to its current path; it must not sample uninitialized skin-layer textures.
- Transparent attachments and clothing are not to inherit skin-only material layers.

## 5. Compatibility and ownership

- Feature-off, missing-data, old-server and unsupported-shader cases must render exactly through the current legacy Avatar path.
- Do not change the legacy Bake UUIDs, Bake count, TE indices, upload protocol, or server appearance message in the first implementation.
- Existing Tattoo wearables must not be composited twice. The implementation must identify which content is already represented in the legacy baked diffuse before adding overlays.
- Texture UUID resolution continues through the Viewer texture manager. Resolver READY means fetch/decode readiness only; GPU residency and successful shader sampling are separate states.
- Per-Avatar layer metadata owns no GL objects. Runtime composite textures are owned by a dedicated Avatar skin-layer runtime object with explicit teardown on Avatar death and region/appearance invalidation.
- Shared fetched textures remain globally managed; Avatar release must drop references, not destroy shared texture-manager resources.

## 6. Implementation phases

### Phase 0 — legacy texture correctness (blocking)
Complete the existing texture-pipeline audit and reproduce/fix legacy texture failures first. Do not enable the new layer renderer while the original baked/local texture path is unstable.

### Phase 1 — data contract and editor-independent compositor
- Extend the current `LLAvatarSkin::MaterialData` contract with versioned logical layer descriptors.
- Add deterministic CPU-side validation, ordering and blend/composite tests.
- Build a standalone compositor input/output path without changing the existing LLTexLayerSet Bake output.
- Add memory limits and explicit missing-texture behavior.

### Phase 2 — texture lifecycle
- Resolve optional layer assets asynchronously.
- Track distinct states: UNASSIGNED, FETCHING, DECODED, GPU_READY, MISSING, FAILED, RELEASED.
- Invalidate composites on texture completion, layer edits, appearance change and region transition.
- Ensure stale async callbacks cannot publish into a destroyed or superseded Avatar layer runtime.

### Phase 3 — shader integration
- Add a separate optional native Avatar shader permutation.
- Bind only the bounded composite maps; preserve legacy shader as fallback.
- Implement overlay, normal and packed material sampling.
- Bind maps only for native skin/body surfaces, not arbitrary attachments.
- Check sampler limits and shader compile/link success before selecting the permutation.

### Phase 4 — persistence and creator workflow
- Define a versioned, permission-aware asset/metadata format.
- Keep local preview separate from publishing.
- Add creator controls for layer order, visibility, opacity, blend mode, map assignment and presets.
- Negotiate server/grid capability before sending or advertising the feature. Never send unknown appearance fields to legacy grids.

### Phase 5 — visual and compatibility validation
Test self and remote Avatars; legacy host Bake and server Bake; old grids; missing/delayed assets; teleport/relog; outfit changes; tattoo and alpha-mask wearables; impostor transitions; low graphics; texture-memory pressure; and shader fallback.
Capture logs and screenshots proving texture fetch, GPU residency, binding and final visible sampling.

## 7. Acceptance criteria

1. With no skin-layer metadata, the rendered Avatar follows the current legacy path.
2. With only an overlay map, the overlay appears at the correct UV locations and does not alter legacy alpha/clothing.
3. Missing optional maps do not make the Avatar gray, black, invisible or permanently pending.
4. Teleport, relog and appearance changes cannot reuse stale per-Avatar layer composites.
5. Low profile can disable optional maps without shader errors.
6. Existing tattoo, clothing, alpha-mask, host Bake and server Bake behavior remains unchanged.
7. A successful fetch is not reported as a successful render until GPU upload, binding and visual sampling have been verified.

### Phase 1 progress (source committed; tests not run)

The first editor-independent data/math slice is now present:
- `LayerDescriptor` and `LayerStack` define versioned logical layers, ordering, enabled state, texture/mask UUIDs, opacity, blend mode and an enforced maximum of 16 enabled layers.
- Supported logical types: color overlay, micro detail, makeup, tattoo overlay, surface normal, surface roughness, surface specular and subsurface mask.
- Supported blend modes: normal, multiply, add and soft light.
- `compositeLayer()` implements straight-alpha source-over compositing with premultiplied-alpha math internally and bounded inputs.
- TUT coverage has been authored for descriptors, ordering, duplicate IDs, layer cap, unknown schema, UUID collection and basic blend/alpha behavior (16 test cases total in the file).

This is not yet an image compositor: it does not iterate LLImageRaw pixels, create GPU composite textures, load assets for the layer stack, or bind anything to Avatar shaders. No build or test execution is claimed. The original texture pipeline audit remains a blocking prerequisite before runtime activation.

## 8. Current repository status

The existing `LLAvatarSkin::MaterialData` and texture resolver are infrastructure only. The resolver is not yet wired into the Avatar draw path, and the current deferred skinned Avatar fragment shader samples the legacy diffuse map only. This specification does not claim that skin layers are implemented or tested.
