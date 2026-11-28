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
- Skin material composite: packed channels (R = specular strength, G = roughness, B = subsurface mask, A = reserved/detail mask).

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


## 9. Revised first-release priority: skin surface material first

The first visible enhancement is narrowed to the native skin surface, not a general-purpose color-layer editor.

Priority order:
1. Preserve and validate the existing baked Base Skin path without modifying its pixels or alpha.
2. Add an optional tangent-space Normal Map for pores, fine lines and shallow skin relief. Do not create a separate pore color texture in the first release.
3. Add an optional packed Specular/Roughness map. Specular strength and roughness are distinct material properties and must both be represented.
4. Add subsurface response only after the first two material maps are stable and the target Avatar shader path supports it.
5. Add optional color overlays (freckles, discoloration, makeup and scars) after surface shading is working.

The first runtime material set is therefore:
- Legacy Base Skin: existing baked diffuse, unchanged.
- Skin Normal: one optional tangent-space normal composite.
- Skin Material: one optional packed RGBA map, with R = specular strength, G = roughness, B = reserved for subsurface mask, A = reserved for detail mask.

This supersedes the earlier proposal to prioritize a general color overlay composite. The logical layer schema may continue to describe future color layers, but they are not a prerequisite for the first surface-material milestone.

### Pixel-processing rules

- LLImageRaw already exposes dimensions, component count, data access, allocation, resize/scale, copy and legacy color-composite operations. The new path should use these existing image primitives rather than introducing a second raw-image container.
- Do not use the legacy LLImageRaw composite operation to combine tangent-space normal maps. Decode each normal sample to a signed vector, combine according to a documented normal-blending rule, then normalize the result before encoding it back to RGB.
- Specular and roughness are scalar channels. Blend each channel independently with clamped values and explicit defaults; never interpret missing data as black (which would incorrectly erase material response).
- Inputs must have compatible dimensions and expected component counts. A future resampling policy must be explicit; the first implementation should reject invalid/mismatched inputs safely rather than sample out of bounds.
- CPU image work must not mutate the legacy baked image. Output buffers are independently allocated and have bounded dimensions/byte budgets.
- Establish whether fetched source pixels are sRGB-encoded or linear before applying any color blend math. Normal vectors and scalar material channels are data textures and must not receive sRGB color transforms.
- Mask semantics must be explicit (first implementation: optional single-channel mask, with opacity multiplied into its coverage).
- Normal-map strength is a runtime parameter; distant-avatar mip behavior must avoid pore shimmer and aliasing.

### Existing renderer boundary found during source inspection

The legacy LLTexLayerSetBuffer::renderTexLayerSet() path renders the existing appearance stack into its render target through LLTexLayerSet::render(). That path remains authoritative for Base Skin and legacy wearable compatibility. The new material compositor must remain a separate optional path and must not replace or write into the legacy Bake render target.

The LLImageRaw API is suitable for a standalone CPU-side compositor, but the current inspection has not established a safe GPU upload owner, native-skin-only draw binding point, or tangent-basis correctness for every Avatar render path. Those remain later integration gates.


### Phase 1 progress: first LLImageRaw surface compositor slice

Implemented as CPU-only functions in the existing skin-material module:
- compositeNormalMap() accepts RGB/RGBA tangent-space maps, uses the detail alpha channel as coverage when present, blends vectors by clamped detail strength, normalizes the result and emits an independent RGB output.
- packSpecularRoughness() packs scalar R-channel inputs into RGBA (R = specular, G = roughness, B = reserved subsurface mask, A = neutral detail mask). Missing inputs use caller-supplied defaults.
- Both operations reject aliased output buffers, invalid component counts, mismatched dimensions and outputs exceeding a 32 MiB budget. No implicit resampling is performed.
- The legacy baked diffuse is not an input or output of these functions and remains untouched.
- Five additional TUT cases (tests 17–21) cover normal blending, transparent detail coverage, missing-map defaults, mismatched dimensions and invalid normal formats. The existing CMake test registration resolves llavatarskinmaterial.cpp to tests/llavatarskinmaterial_test.cpp.

These tests are authored but have not been compiled or executed. This CPU slice does not yet resolve texture UUIDs, generate GPU textures, select a shader permutation or render on an Avatar. Source pixel color-space metadata is also not established; this is safe for the normal and scalar data maps because these channels must be treated as linear data, not color.


## 10. Mesh Head material-slot model (SL / VRChat comparison)

A Mesh Head does not have a universal fixed number of skin texture layers. Keep these concepts separate:

- **Mesh faces / submeshes:** geometry partitions that can be assigned different materials. Their count is asset-author-defined and may separate face skin, ears, scalp, eyes, teeth, lashes or other surfaces.
- **Material slots:** material assignments used by those faces. A head can have multiple slots even when some slots share the same material or texture assets.
- **Texture maps per material:** maps sampled by the selected material shader. A legacy SL-style skin material commonly uses diffuse/base color, normal and specular inputs; a PBR material uses base color, normal and metallic-roughness (with optional additional maps). These are map roles, not editable stacked layers.
- **Logical editor layers:** authoring inputs that are composited into a bounded runtime set. Their count must not be confused with material slots or GPU texture samplers.

VRChat follows Unity's renderer/material model: the avatar author chooses mesh submeshes and materials, while the selected shader determines which maps and additional layers are available. Therefore, neither SL nor VRChat defines one fixed "number of head texture layers" for all creators' heads.

For OpenSim-next, attach the optional skin material set only to explicitly classified native skin material slots. Do not automatically apply it to eyes, teeth, hair, lashes, clothing or arbitrary attachments. A head may have several skin-related slots; the runtime may share a composite across compatible slots only when their UV layout, material parameters and skin classification are proven compatible. Otherwise keep separate per-slot composites, subject to a strict memory budget.

The canonical packed surface channel order is **R = specular strength, G = roughness, B = reserved subsurface mask, A = reserved detail mask**. This order matches `packSpecularRoughness()` and its current unit test. Do not silently swap channels in shader bindings or asset serialization.


## 11. Native avatar mesh classification: never infer skin from Bake ID

The current appearance dictionary explicitly associates mesh types with baked texture groups:

| Mesh type | Mesh ID | Bake association | Skin-material eligibility |
|---|---|---|---|
| `headMesh` | `MESH_ID_HEAD` | `BAKED_HEAD` | Yes |
| `eyelashMesh` | `MESH_ID_EYELASH` | `BAKED_HEAD` | No |
| `upperBodyMesh` | `MESH_ID_UPPER_BODY` | `BAKED_UPPER` | Yes |
| `lowerBodyMesh` | `MESH_ID_LOWER_BODY` | `BAKED_LOWER` | Yes |
| `eyeBallLeftMesh` | `MESH_ID_EYEBALL_LEFT` | `BAKED_EYES` | No |
| `eyeBallRightMesh` | `MESH_ID_EYEBALL_RIGHT` | `BAKED_EYES` | No |
| `hairMesh` | `MESH_ID_HAIR` | `BAKED_HAIR` | No |
| `skirtMesh` | `MESH_ID_SKIRT` | `BAKED_SKIRT` | No |

The mapping is defined by `LLAvatarAppearanceDictionary::MeshEntries` in `indra/llappearance/llavatarappearancedefines.cpp`. `LLAvatarAppearance::initInstance()` then populates each baked group's `mJointMeshes` from every mesh entry with a valid `mBakedID`. Consequently, `BAKED_HEAD` contains both the head mesh and eyelash mesh. A Bake ID is a texture/compositing association, **not** a semantic skin classification.

For the first native skin-material implementation, eligibility must be explicit and mesh-based: allow only `MESH_ID_HEAD`, `MESH_ID_UPPER_BODY`, and `MESH_ID_LOWER_BODY`. Exclude eyelash, eyeballs, hair, skirt, and all rigged attachments. Do not change the existing Bake-to-mesh association or the legacy `mJointMeshes` list to achieve this.

This is an initial classification for the built-in classic avatar meshes. Future custom/native mesh avatar support must carry explicit per-material semantic metadata (for example `SKIN`, `EYE`, `LASH`, `HAIR`, `CLOTHING`, `OTHER`) rather than relying on mesh names, Bake IDs, or face order. Missing or unknown classification must fall back to the legacy material path.


## 12. First implementation: per-LOD native mesh surface classification

The first code-level classification is stored on each `LLAvatarJointMesh` as `EAvatarSurfaceType`:

- `UNKNOWN` is the in-class default and must remain the safe fallback.
- `SKIN`: head, upper-body, and lower-body built-in mesh IDs.
- `EYE`: left and right eyeball mesh IDs.
- `EYELASH`: eyelash mesh ID.
- `HAIR`: hair mesh ID.
- `CLOTHING`: skirt mesh ID.
- `OTHER`: reserved for explicit future classification.

The classification is assigned when each LOD `LLAvatarJointMesh` is created in `LLAvatarAppearance::initInstance()`. It is independent of the baked-texture association: the legacy `mBakedTextureDatas[].mJointMeshes` population remains unchanged, including the existing head-bake/eyelash association.

This is metadata groundwork only. No renderer, shader, texture upload, material resolver, or network path consumes the classification yet. Until those paths are explicitly implemented and validated, all meshes continue using the existing rendering behavior. Future per-material-slot classification for custom mesh heads remains a separate extension; a mesh-level `SKIN` tag must not imply that every sub-material or face is skin.

## 13. Renderer and texture-lifecycle audit: draw-pool boundary identified

The source audit now covers `LLViewerJointMesh::drawShape()`, the Avatar draw-pool pass setup, and the Viewer texture-manager entry points.

### Draw submission and pass ownership

`LLViewerJointMesh::drawShape()` in `indra/newview/llviewerjointmesh.cpp`:
- Requires an already-bound `LLGLSLShader`; it does not select or bind the Avatar shader.
- Selects only `LLDrawPoolAvatar::sDiffuseChannel` and binds the current test image, legacy `LLViewerTexLayerSet` composite, mesh texture, or default texture.
- Submits the existing vertex buffer draw. It does not currently bind a skin Normal or packed Specular/Roughness map.
- Inherits `getSurfaceType()` from `LLAvatarJointMesh`, so the mesh classification can be checked here without introducing a parallel mesh registry. The classification is not consumed by the renderer yet.

The outer `LLDrawPoolAvatar` owns shader selection and pass-specific texture-unit setup:

| Pass | Existing path | Skin material policy |
|---|---|---|
| Impostor | `beginImpostor()` / deferred impostor | Do not bind per-mesh skin maps. Impostors use their own baked color/normal/specular representation. |
| Rigid | `beginRigid()` / deferred rigid | Exclude; this pass renders rigid meshes such as eyeballs. |
| Skinned, forward/preview | `beginSkinned()` | Legacy shader path; keep legacy behavior until a separately validated shader variant exists. |
| Skinned, deferred | `beginDeferredSkinned()` | The first candidate integration point for optional skin maps, gated by `getSurfaceType() == SKIN` and a successful shader permutation. |
| Post-deferred alpha | `beginPostDeferredPass()` | Preserve legacy alpha behavior; do not apply skin material maps to transparent surfaces. |
| Shadow opaque/alpha/mask | `beginShadowPass()` | No skin surface maps are needed for shadow depth/alpha decisions; keep existing diffuse/alpha binding only. |

The deferred skinned setup currently binds `DIFFUSE_MAP` only. By contrast, deferred impostor setup enables `SPECULAR_MAP` and `NORMAL_MAP` for the impostor's separate texture set. This confirms that the existence of normal/specular sampler names elsewhere does not mean the skinned Avatar shader currently consumes native skin maps.

### Texture creation and ownership findings

- `LLViewerTextureManager::getLocalTexture(const LLImageRaw*, ...)` creates a local `LLViewerTexture` backed by the raw image. The local texture is reference-counted; it is not an asset UUID fetch.
- `LLViewerTextureManager::getFetchedTexture(const LLImageRaw*, FTType, ...)` creates a `LLViewerFetchedTexture` and inserts it into the global `gTextureList`. This is a manager-owned fetched-texture path and is not automatically the correct owner for short-lived per-Avatar composites.
- `LLViewerFetchedTexture` has asynchronous fetch/decode state, loaded callbacks, discard levels, and texture-manager lifecycle. These asset-fetch semantics should not be overloaded to represent a CPU-generated Avatar composite unless a concrete lifecycle design requires it.
- `LLViewerTexture` derives from `LLGLTexture`; the existing local-texture manager path is the closest established mechanism for a generated image that needs GL residency. A per-Avatar runtime owner should hold reference-counted local texture handles and release those references on appearance replacement, region transition, and Avatar teardown. It must not destroy shared fetched assets.
- The CPU compositor output is currently only `LLImageRaw`. No upload, GL-ready state, cache key, invalidation generation, or Avatar-owned texture handle has been implemented.

### Integration decision

Do not add bindings in `drawShape()` yet. First add a dedicated, optional deferred-skinned Avatar shader permutation and have the draw pool enable its bounded samplers only when:
1. the mesh is explicitly classified `SKIN`;
2. the Avatar runtime reports the corresponding composite texture as GPU-ready;
3. the selected shader permutation compiled and linked successfully; and
4. the pass is the deferred skinned pass, not rigid, impostor, alpha-only, or shadow rendering.

`drawShape()` can then bind the already-selected optional channels for SKIN meshes only, while every other surface and every failure case continues through the existing diffuse path. The forward/preview path, impostor regeneration, tangent-basis correctness, texture invalidation generations, and final visual sampling still require separate validation. No shader or runtime behavior is changed by this audit.

No build or test execution is claimed.

### Additional shader-manager finding: do not reuse the generic material permutation yet

`LLViewerShaderMgr` also constructs a family of generic deferred material permutations with `HAS_SKIN`, `HAS_NORMAL_MAP`, and `HAS_SPECULAR_MAP`. The corresponding `deferred/materialV.glsl` contains tangent and skinned-transform plumbing, but the current `class1/deferred/materialF.glsl` is explicitly a debug stub: it writes fixed diagnostic values rather than sampling material textures and producing the normal material G-buffer output.

Therefore this generic material family is not a usable native Avatar skin shader today. Its vertex-side tangent plumbing is useful reference material only; it does not establish that the Avatar mesh vertex buffers provide valid tangent attributes or compatible UV1/UV2 data. The safe route remains a dedicated deferred Avatar skin permutation (or a separately completed and validated shared material shader), with explicit verification of tangent availability and UV semantics before normal-map sampling.

## 14. Native Avatar vertex tangent and UV audit

The legacy avatar mesh asset format contains more data than the live Avatar vertex buffer currently exposes. The source path was traced through `LLPolyMeshSharedData::loadMesh()`, `LLPolyMesh::initializeForMorph()`, `LLFace::getGeometryAvatar()`, and `LLViewerJointMesh::updateFaceData()`.

### What exists in the legacy mesh asset

- The binary mesh loader reads per-vertex base positions, normals, binormals, and UV0.
- If the mesh header sets `mHasDetailTexCoords`, it also reads a separate detail-UV array.
- `LLPolyMesh` exposes `getBinormals()`, `getBaseBinormals()`, and `getDetailTexCoords()`.
- This is not equivalent to a complete tangent-space vertex stream. In particular, the loader's binormal is not automatically the tangent attribute expected by the deferred material shader, and its handedness/sign convention has not been established.

### What reaches the live Avatar vertex buffer

- `LLFace::getGeometryAvatar()` obtains only position, normal, texcoord0, weight, and clothing-weight striders. It does not request tangent, texcoord1, or texcoord2.
- `LLViewerJointMesh::updateFaceData()` copies positions, normals, UV0, weights, clothing weights, and indices. It does not copy binormals, detail UV, or a tangent frame into the face VBO.
- The live path therefore does not currently populate the tangent and secondary UV attributes used by the generic deferred material vertex shader.
- The mesh's `initializeForMorph()` currently initializes `mBinormals` and `mScaledBinormals` from `mSharedData->mBaseNormals`, not from the separately loaded `mBaseBinormals`. This means the loaded base binormal data is not propagated into those runtime arrays by this initialization path.
- `LLVertexBuffer` supports `TYPE_TANGENT`, `TYPE_TEXCOORD1`, and `TYPE_TEXCOORD2`, but generic support in the buffer class does not imply that the Avatar face buffer allocates or fills those attributes.

### Consequence for skin Normal Maps

A normal-map shader must not be enabled for native Avatar meshes yet. Sampling a tangent-space normal map without a valid per-vertex tangent frame and a well-defined UV set would produce incorrect lighting or undefined attribute values.

The first implementation should explicitly generate or derive a tangent frame from the actual deformed mesh positions and UV0 (or deliberately adopt a documented alternate basis), then upload it to the Avatar face VBO. If detail UV is intended as the skin-map UV, it must be copied into a dedicated VBO attribute and its transform/scale semantics must be defined. The cheaper initial choice is to use UV0 for all three skin maps and generate tangents against UV0, unless asset compatibility testing proves otherwise.

### Required follow-up before shader work

1. Verify the Avatar face VBO allocation mask and its exact stride/attribute layout.
2. Verify how morphing and joint skinning affect tangent direction and handedness; do not upload static untransformed tangents if the shader's skinned basis expects deformed tangent vectors.
3. Decide whether tangents are generated on CPU during mesh update or generated/derived in the vertex shader from available data. CPU generation adds per-avatar work; shader derivation needs a mathematically valid and stable basis.
4. Add tests for degenerate UV triangles, mirrored UV islands, zero-area triangles, and LOD changes.
5. Keep Normal Map rendering disabled until the tangent stream is proven valid. Specular/Roughness scalar sampling does not need tangent attributes, but it still needs a verified UV stream and a valid material shader.

No runtime code, shader, or VBO layout was changed in this audit. No build or test execution is claimed.

### Confirmed Avatar VBO allocation mask

The allocation site is now confirmed in `LLVOAvatar::restoreMeshData()`: new Avatar face buffers are created with `LLDrawPoolAvatar::VERTEX_DATA_MASK`. In `lldrawpoolavatar.h`, that mask contains exactly:
- `MAP_VERTEX`
- `MAP_NORMAL`
- `MAP_TEXCOORD0`
- `MAP_WEIGHT`
- `MAP_CLOTHWEIGHT`

It does not allocate `MAP_TANGENT`, `MAP_TEXCOORD1`, or `MAP_TEXCOORD2`. Existing buffers that are resized preserve their original type mask, so the current Avatar path has no hidden secondary-attribute allocation path in this restore routine.

This closes the VBO-layout question: native Avatar face buffers must be extended deliberately before a tangent-space Normal Map shader can be used. Do not add attributes globally to every Avatar buffer without checking memory cost and rigid/alpha/shadow pass compatibility; prefer a capability-gated layout or a single shared Avatar buffer layout with deterministic defaults and measured memory impact.

## 15. Morph and skinning lifecycle: tangent update constraints

The follow-up source audit traced the classic morph data and Avatar face-update path through `LLPolyMorphData::loadBinary()`, `LLPolyMorphTarget::apply()`, `LLPolyMorphTarget::applyMask()`, `LLVOAvatar::updateMeshData()`, `LLViewerJointMesh::updateFaceData()`, and `LLVOAvatar::renderSkinned()`.

### Morph data already contains directional deltas

- Each morph target stores per-vertex coordinate, normal, binormal, and UV deltas. These are loaded independently from the legacy mesh asset.
- `LLPolyMorphTarget::apply()` accumulates coordinate deltas, softens and normalizes the normal accumulator, and separately accumulates binormal deltas.
- The morph code then reconstructs an orthogonalized binormal from the current normal and the accumulated binormal-derived tangent. Thus the intended CPU morph representation is not simply a normal-only stream.
- Morph-mask replacement also subtracts the prior normal and binormal contributions before applying the new mask.

### Confirmed base-binormal initialization defect

`LLPolyMesh::initializeForMorph()` currently copies `mSharedData->mBaseNormals` into both `mBinormals` and `mScaledBinormals`. The shared mesh loader has a separate `mBaseBinormals` array, and morph records also carry binormal deltas. The two runtime binormal arrays should therefore be initialized from `mSharedData->mBaseBinormals`, not from the base normals.

This is a concrete source defect in the existing morph-direction data path. It should be fixed with a focused regression test and asset-level validation before any new tangent implementation relies on these arrays. This audit records the defect; it does not change runtime code.

### What reaches the Avatar face VBO

- `LLVOAvatar::updateMeshData()` rebuilds/resizes the face buffers when geometry is dirty and invokes `updateFaceData()` for the participating mesh parts.
- `LLViewerJointMesh::updateFaceData()` copies the current morphed positions and normals into the face VBO. On a non-terse update it also copies UV0, weights, and clothing weights. It does not copy the CPU binormal array.
- A terse update is selected when the existing face buffer has the same vertex and index counts (except rigid eyeballs). In that case UV0 and weight data are retained rather than recopied; positions and normals are still copied.
- The Avatar VBO layout remains the previously audited layout: no tangent or secondary UV attribute is allocated.

### Update cadence and the correct tangent design boundary

- Morph deltas update the CPU-side `LLPolyMesh` arrays. `updateMeshData()` transfers the resulting positions/normals to the Avatar face VBO when the drawable/mesh geometry is rebuilt or marked dirty; it is not a general per-frame tangent refresh.
- In `renderSkinned()`, the Avatar shader path with a positive Avatar shader level clears `mNeedsSkin` and relies on the hardware-skinned rendering path. The software-skin branch is conditional on Avatar shader level being zero. This distinction means a CPU tangent generated only during `updateMeshData()` would still need to be transformed consistently with the per-frame joint skinning performed by the active shader.
- The current `LLViewerJointMesh::updateJointGeometry()` implementation is empty. Do not assume this method supplies a CPU tangent update or a software fallback.
- The current source audit has not yet established the exact vertex-shader joint-matrix transform used by every Avatar shader permutation, nor a safe place to upload a post-skin tangent. Those must be inspected before selecting an implementation.

### Design decision

Do not treat the legacy binormal as if it were already a tangent. The asset's base binormal and morph binormal deltas are useful directional data, but the tangent/bitangent handedness convention and UV orientation still need proof.

The next implementation gate should be:

1. Correct and test the base-binormal initialization, preserving existing morph behavior.
2. Inspect the actual Avatar vertex shader permutations and their normal/weight transforms, including non-uniform joint scale handling.
3. Define a tangent frame from UV0 with an explicit handedness sign. Verify mirrored UV seams and degenerate UV triangles using the built-in avatar meshes.
4. Ensure tangent and normal receive compatible morph and joint transforms. A static tangent copied once to the VBO is insufficient if the shader skins positions/normals dynamically.
5. Only then extend the Avatar VBO and add the optional deferred skin Normal Map permutation.

No runtime rendering behavior or shader was changed by this audit. No build or test was run.

### Vertex-shader skin transform: class1 Avatar path

The actual class1 shader sources add an important constraint:

- `avatarSkinV.glsl::getSkinnedTransform()` selects/interpolates adjacent entries from the 45-vec4 matrix palette using the packed `weight.x` value. This is the classic Avatar skinning path, not the four-influence rigged-object path in `objectSkinV.glsl`.
- `avatarV.glsl` transforms position with that matrix and transforms the normal using the upper 3x3 portion of the same matrix, then normalizes it.
- The shader currently has no tangent input and no binormal input. It therefore cannot preserve a CPU tangent frame unless the new shader explicitly adds and transforms the tangent consistently with this exact skin transform.
- The normal is transformed by the same linear 3x3 operation as a direction and normalized; this source path does not show an inverse-transpose normal matrix. Any new tangent implementation must match the existing Avatar convention or deliberately introduce and validate a corrected frame without silently changing legacy lighting.

This confirms that a tangent generated from morphed CPU positions cannot simply be uploaded and sampled as though it were already in final world/view space. The dedicated Avatar skin shader must apply the same selected/interpolated skin transform to tangent directions, then re-orthogonalize tangent against the transformed normal and carry a separately validated handedness sign. The exact matrix-space convention and shader variants still need end-to-end verification.

### Base-binormal initialization fix

The confirmed initialization defect has now been corrected in `indra/llappearance/llpolymesh.cpp`:

- `mBinormals` is initialized from `mSharedData->mBaseBinormals`.
- `mScaledBinormals` is initialized from `mSharedData->mBaseBinormals`.
- Base normals continue to initialize only the normal arrays.

The legacy binary mesh loader reads three independent float components per vertex for Base Binormals, and LOD shared data references the same base-binormal array as its reference mesh. The fix therefore uses the existing asset field and does not change the mesh file format, UVs, VBO layout, or LOD ownership model.

Commit: `e542d4be621b4a62d171c7639c4ef3601e2bca1e`.

Validation status: the edited source was fetched again and the two corrected assignments were confirmed. No llappearance-specific test directory or existing LLPolyMesh unit-test target is present in this branch; no build or automated test was run. A focused runtime/asset regression test remains required before enabling Tangent or Normal Map rendering.


## 16. Built-in mesh topology and legacy binormal basis audit

The repository contains the built-in classic Avatar mesh assets under `indra/newview/character/`, including `avatar_head.llm`, `avatar_upper_body.llm`, `avatar_lower_body.llm`, plus eye, eyelash, hair, and skirt meshes. The body/head assets are tracked in this repository; they are not merely external viewer cache files. The GitHub text-file endpoint cannot decode these binary `.llm` assets as UTF-8, so no per-mesh numerical topology/UV statistics are claimed in this audit.

### Confirmed binary layout and topology

- For a non-LOD/reference mesh, `LLPolyMeshSharedData::loadMesh()` reads positions, normals, base binormals, UV0, optional detail UVs, optional skin weights, then face indices.
- Each serialized face record read by this loader contains exactly three 16-bit vertex indices. The runtime `LLPolyFace` declaration also has three slots; its old comment mentioning quads is stale and must not be used to infer quad support in this file path.
- LOD mesh files reuse reference-mesh vertex arrays and provide their own face index lists. Tangent generation must therefore use each active LOD's own triangles, not assume that one LOD's triangle adjacency applies to every other LOD.
- The loader also reads a shared-vertex remap table after morph data. That table is used by morph-mask UV lookup; it is not evidence that separate UV seam vertices can safely be welded for tangent accumulation.

### New tangent-space lead: derive a candidate tangent from the legacy binormal

The morph implementation provides a concrete orientation relationship. After accumulating a binormal direction, it computes a tangent-like direction as `cross(binormal, normal)`, then reconstructs an orthogonal binormal as `cross(normal, tangent)`. This is consistent with a candidate frame:

- `N` = current morphed normal
- `B` = current, orthogonalized legacy binormal
- `T` = normalize(`cross(B, N)`)

This is a stronger starting point than rebuilding tangent from triangle derivatives on every morph update: the legacy asset already carries a binormal direction and morph targets carry binormal deltas. It may preserve the asset's authored orientation across morphs without per-frame triangle accumulation.

However, this is not yet proof that the candidate `T` matches the UV0 tangent convention expected by standard tangent-space normal maps. The asset producer's exact handedness convention, mirrored-UV behavior, and treatment of seam duplicates still need to be established. Do not assume the candidate frame can be passed to a conventional normal-map shader until those checks are complete.

### Implementation implications

1. Keep each mesh vertex as authored; do not weld vertices across UV seams. Any tangent accumulation path must preserve seam splits and per-vertex orientation.
2. Prefer evaluating the candidate `T = cross(B, N)` from the already-morphed CPU normal/binormal arrays, then validate its orientation against UV0 triangle derivatives on the built-in head/upper/lower meshes.
3. For validation, compute per-triangle UV determinant. A near-zero determinant is a degenerate UV triangle and must be excluded from derivative-based reference comparisons; mirrored islands have the opposite determinant sign and must not be averaged together across a seam.
4. Compare candidate `T` and reconstructed `B` against UV-derived tangent frames on representative triangles, recording angular error and sign agreement separately for positive- and negative-determinant UV triangles.
5. If the legacy binormal convention proves UV-consistent, the existing morph binormal path can supply the CPU-side frame. The class1 vertex shader must still transform `T` using the same Avatar skin matrix as the normal, re-orthogonalize it, and preserve the validated sign.
6. If the convention is not UV-consistent, generate tangents from morphed positions and UV0 with seam-aware per-vertex accumulation, and explicitly define how that tangent is refreshed or transformed during hardware skinning.

### Current conclusion

The built-in source assets and morph data make a legacy-binormal-derived tangent a promising lower-cost candidate. The repository inspection has confirmed the loader layout and morph cross-product relationship, but has **not** numerically inspected the binary mesh topology or UV handedness. No tangent code, VBO attribute, or shader was added. No build or test was run.


## 17. Morph tangent-frame derivation: confirmed source relationship

A closer read of `LLPolyMorphTarget::apply()` confirms that the legacy binormal is not merely accumulated independently. For every affected vertex, the current code:

1. Adds the weighted morph binormal delta to `scaled_binormals`.
2. Computes a temporary tangent as `cross(scaled_binormals, norm)`.
3. Reconstructs the output binormal as `cross(norm, tangent)`, then normalizes it.
4. Applies the morph's UV delta to the vertex's current `tex_coords`.

Here `norm` is the current normalized output normal after the normal morph accumulation. In the non-degenerate, orthogonal case, the reconstruction is algebraically consistent with:

- `T = normalize(cross(B, N))`
- `B = normalize(cross(N, T))`

Therefore the proposed `cross(B, N)` direction is not an arbitrary guess: it is the same cross-product orientation used internally by the existing morph code. The corrected base-binormal initialization also ensures that the accumulator starts from the mesh's authored base-binormal data rather than incorrectly copying base normals.

### Important additional constraint: UVs are morphed too

The same `apply()` loop changes `tex_coords` using each morph target's `mTexCoords` delta. `applyMask()` also removes the previous morph's UV contribution before applying the updated mask. Thus the UV0 field used by a tangent-space normal map is potentially shape-dependent, not guaranteed to remain at its base-mesh values.

This means the legacy binormal candidate must be compared against UV-derived tangent frames using the **current morphed positions and current morphed UV0**, not only the base mesh. It also means a static tangent generated once from the base mesh cannot be assumed correct for all facial/body morph combinations.

### Revised low-cost candidate and validation gates

- Candidate CPU frame: read the current output normal and the current orthogonalized output binormal after morph application; compute `T = normalize(cross(B, N))`. Reject non-finite or near-zero inputs and keep the legacy rendering path when the frame is invalid.
- Reference frame: derive tangent/bitangent from each active LOD triangle using current morphed positions and current morphed UV0. Skip near-zero geometric area and near-zero UV determinant triangles.
- Compare direction and sign separately for positive- and negative-determinant UV triangles. Preserve authored seam splits; never average across mirrored UV islands.
- Confirm whether the stored binormal convention carries a stable handedness sign for mirrored UV islands. The cross-product reconstruction alone does not prove that a single global sign is valid.
- Repeat the comparison for representative morphs that alter both geometry and UVs, including masked morph updates. Base-shape-only agreement is insufficient.
- Only after the CPU frame is validated should the shader path be designed to transform tangent with the same Avatar skin matrix as the normal, re-orthogonalize it after skinning, and carry the validated handedness.

### Decision

The legacy morph pipeline supplies a strong, potentially low-cost tangent direction candidate and already updates its normal/binormal basis during morph changes. It does **not** yet establish UV0 tangent equivalence, mirrored-island handedness, or correctness after GPU skinning. Do not add a Tangent VBO attribute, activate Normal Map sampling, or enable the new shader permutation until the numerical mesh/morph validation is complete.

Source inspected: `indra/llappearance/llpolymorph.cpp`, `LLPolyMorphTarget::apply()` and `LLPolyMorphTarget::applyMask()`. No runtime code was changed and no build or test was run.


### First numerical UV0 comparison: Upper Body and Lower Body base meshes

The tracked binary assets for `avatar_upper_body.llm` and `avatar_lower_body.llm` were decoded using the field order in `LLPolyMeshSharedData::loadMesh()`. For each serialized triangle, the UV determinant and position/UV derivatives were calculated. The reference tangent and bitangent were projected onto each corner's vertex-normal plane and normalized. The legacy candidate was `normalize(cross(base_binormal, base_normal))`. Each valid triangle corner contributes one sample; these are corner-sample means, not area-weighted or unique-vertex statistics.

| Asset | UV determinant | Triangles | Mean dot(candidate T, UV T) | Mean dot(base B, UV B) |
|---|---:|---:|---:|---:|
| Upper Body | positive | 2,527 | +0.928 | +0.952 |
| Upper Body | negative | 1,159 | -0.905 | +0.939 |
| Lower Body | positive | 1,418 | +0.942 | +0.961 |
| Lower Body | negative | 236 | -0.780 | +0.815 |

Additional topology checks:
- Upper Body: 2 UV-degenerate triangles; 0 geometric-degenerate triangles; 0 invalid indices.
- Lower Body: 0 UV-degenerate triangles; 0 geometric-degenerate triangles; 0 invalid indices.
- Both assets contain both positive- and negative-determinant UV triangles. Mirrored UV orientation is therefore present in these two body meshes and cannot be treated as a theoretical edge case.

### Interpretation and revised conclusion

The measured result is strong evidence that the authored base binormal tracks the UV-derived bitangent direction on these body meshes. The tangent candidate `cross(B, N)` also tracks the UV tangent on positive-determinant triangles. On negative-determinant triangles its direction is reversed. In other words, the observed relationship is approximately:

`T_uv ≈ sign(det(UV)) * normalize(cross(B, N))`

This is the important result: a single global tangent sign is not sufficient. The tangent frame needs a handedness/orientation sign that follows the UV island orientation. The sign must be derived or preserved per authored vertex/seam, not averaged across mirrored islands. The negative-determinant Lower Body samples have lower agreement (+0.780 after sign correction) than the other groups, so this area needs closer inspection rather than being declared perfect.

This is still a **base-mesh-only** comparison for Upper Body and Lower Body. It does not yet prove the same relationship after morph UV deltas, masked morphs, head morphs, or GPU joint skinning. The 1.66 MB Head mesh could not be decoded through the current text-only GitHub file/blob response (the binary payload is rejected or returned empty); no Head statistics are claimed. A local binary asset inspection or another binary-capable transfer path is required.

### Next implementation gate

1. Preserve authored vertex splits and store a per-vertex UV orientation/handedness sign; do not weld across mirrored UV seams.
2. Inspect the low-agreement negative-determinant Lower Body region and identify whether its samples cluster at seams, poles, or strongly varying tangent neighborhoods.
3. Obtain and analyze the Head mesh binary.
4. Validate the same sign relationship after representative geometry **and UV** morphs, including mask replacement.
5. Only then implement Tangent VBO upload and shader skinning.

The data supports continuing with the legacy-binormal-derived candidate, with UV orientation sign as a mandatory part of the frame. It does not authorize enabling Normal Map rendering yet. No runtime code, VBO, or shader was changed; no build or test was run.


### Vertex-aggregated tangent comparison

A second pass accumulated the UV-derived tangent contribution at each serialized vertex, applying the UV-determinant sign before accumulation so mirrored and non-mirrored triangle contributions are compared in the candidate frame convention. The accumulated tangent was projected onto the vertex-normal plane and normalized before comparison with `normalize(cross(base_binormal, base_normal))`.

| Asset | Unique vertex samples | Mean tangent direction dot |
|---|---:|---:|
| Upper Body | 2,211 | 0.889 |
| Lower Body | 977 | 0.904 |

These values are lower than a perfect match but support a useful approximation. This aggregation is deliberately a diagnostic only: it is not a production tangent generator. A vertex touched by both mirrored and non-mirrored triangles must be treated as a seam/topology concern, not silently averaged. The current pass records 732 Upper Body and 142 Lower Body vertices that touch at least one negative-determinant triangle; that count alone does not establish whether each is a mixed-orientation seam vertex.

The per-triangle-corner and per-vertex-aggregated measurements answer different questions. Corner samples expose local face-frame agreement; vertex aggregation approximates the smoothing behavior needed by a per-vertex attribute. Neither pass yet models the exact morph update, seam duplication policy, tangent handedness storage, or GPU skinning.

**Decision remains unchanged:** use the legacy binormal as a low-cost tangent-direction candidate, but preserve UV orientation explicitly and validate actual seam groups. Do not enable normal mapping. The Head mesh still requires binary-capable local inspection; the repository text connector rejects its large binary payload. No runtime source changed; no build or test was run.


### Morph and UV0 update semantics (source audit)

A source-level audit of `LLPolyMorphTarget::apply()` and `applyMask()` confirms that the morph system updates the complete candidate tangent frame inputs, not positions alone:

- `apply()` applies weighted position deltas, accumulates softened normal deltas, and normalizes the resulting normal.
- It accumulates the morph's binormal delta into the scaled-binormal accumulator. It then derives a tangent candidate as `cross(scaled_binormal, current_normal)`, reconstructs the binormal as `cross(current_normal, tangent)`, and normalizes it.
- The same morph application adds weighted `mTexCoords` deltas to the live UV0 array.
- Replacing an existing vertex mask first subtracts the previous mask's position, normal, binormal, and UV0 contributions, resets the last weight, generates the replacement mask, and reapplies the morph.

This means a future tangent/handedness implementation must use the post-morph UV0, normal, and binormal state. A tangent orientation sign computed once from base UVs is not sufficient if morph data changes UV0 or crosses a degenerate/orientation boundary.

**Important limitation:** the current morph code's reconstructed binormal is orthogonalized against the current normal, but that alone does not prove it remains aligned to the derivative of the post-morph UV mapping. The morph's UV0 deltas and binormal deltas must be validated together on actual morph targets, including mask replacement. We have not yet run a numerical morph-target replay, and no GPU skinning result is claimed.

Implementation implication: do not add a separate per-frame CPU tangent rebuild yet. First replay representative morph targets from the shipped mesh/morph assets and compare (a) the post-morph UV-derived tangent frame, (b) the live morph-updated normal/binormal frame, and (c) the result after the Avatar skinning transform. Preserve explicit per-vertex handedness/seam handling; do not infer it from one global sign.


### Mirrored-UV vertex incidence audit

A topology-level pass now distinguishes vertices that merely belong to mirrored UV triangles from vertices shared by triangles of both UV orientations. This resolves the earlier ambiguity in the mirrored-vertex counts.

| Asset | Positive-orientation faces | Negative-orientation faces | UV-degenerate faces | Positive-only vertices | Negative-only vertices | Mixed-orientation vertices |
|---|---:|---:|---:|---:|---:|---:|
| Upper Body | 2,527 | 1,159 | 2 | 1,479 | 720 | 12 |
| Lower Body | 1,418 | 236 | 0 | 835 | 142 | 0 |

Interpretation:

- Lower Body's positive and negative UV islands are fully separated at the indexed-vertex level in this asset. Its 142 negative-only vertices do not require an additional split merely because their UV orientation is mirrored.
- Upper Body has only 12 indexed vertices incident to both positive- and negative-determinant triangles. The issue is localized rather than affecting all 732 vertices that touch mirrored faces.
- For those 12 mixed vertices, face-derived tangent directions were projected onto each vertex's base-normal plane and corrected for UV orientation before averaging by orientation. The positive-vs-negative group direction dots ranged from -0.459 to 0.923. Two vertices (654 and 903) have opposing group directions (approximately -0.387 and -0.459), so a single smoothed tangent at those locations is not safe to assume.
- The remaining mixed vertices show varying agreement and still require seam-specific review; a dot below 1 is not by itself proof of a defective asset because adjacent face frames can vary over curved geometry.

**Implementation implication:** a single per-index tangent/handedness value is likely adequate for most of these two body assets, but Upper Body needs explicit handling of its 12 mixed-orientation vertices. Before VBO design, inspect whether those vertices are intentional topology junctions and whether splitting only the affected tangent vertices (without changing legacy position/UV indices or baked texture behavior) is viable. Do not split or rewrite the source mesh asset as part of this audit.

No runtime source or mesh asset changed. No build or test was run.


### Upper Body mixed-orientation vertex neighborhood inspection

The 12 mixed-orientation indices are not 12 unrelated locations. Their base positions form six left/right mirrored pairs (same X/Z and opposite Y):

| Positive/negative counterpart pair | Approximate base position (X, |Y|, Z) |
|---|---|
| 239 / 468 | (0.0230, 0.6995, 1.5233) |
| 260 / 432 | (-0.0110, 0.7365, 1.5349) |
| 265 / 441 | (-0.0097, 0.7287, 1.5400) |
| 582 / 633 | (-0.0045, 0.7340, 1.5213) |
| 654 / 903 | (0.0168, 0.7052, 1.5321) |
| 1392 / 1596 | (0.0004, 0.7312, 1.5222) |

This symmetry strongly suggests a localized mirrored-UV/topology boundary rather than widespread random mesh corruption. It does not, by itself, prove that the pairs should share a tangent frame.

The incident-face inspection also found very small but nonzero UV determinants around these neighborhoods. Examples include approximately 0.000026 and 0.000050 around vertices 654/903, and approximately 0.000001 around vertices 1392/1596. These triangles were not counted as exactly degenerate by the earlier 1e-10 determinant threshold, but their UV derivatives can still be numerically sensitive. The current degenerate-face count therefore must not be interpreted as a guarantee that all remaining UV tangent calculations are well-conditioned.

For the 12 mixed vertices, the positive- and negative-orientation face tangent groups were calculated separately, with each face tangent projected onto the vertex's base-normal plane and normalized before group averaging. The group-to-group direction comparison includes:
- Vertex 654 and counterpart 903: approximately -0.387 and -0.459, respectively. A single averaged tangent is not acceptable without further seam handling.
- Vertex pairs 239/468 and 582/633 show partial directional disagreement, while other pairs are more aligned. These differences need to be judged against local curvature and face quality, not treated automatically as asset defects.

**Candidate implementation direction, not yet implemented:** retain the legacy mesh topology and baked UVs as the source of truth, but build a tangent-ready vertex/index representation that can duplicate only vertices whose incident triangles require distinct orientation groups. The grouping must be derived from the *post-morph UV0* determinant signs, or be rebuilt when those signs change; a split computed once from base UVs is not sufficient because morphs can modify UV0. Near-degenerate UV triangles need a defined fallback (for example, use a stable neighboring tangent or disable normal-map contribution for those corners) rather than amplifying unstable derivatives.

No mesh asset, runtime source, VBO, or shader was changed. No build or test was run.


### Numerical replay of shipped body Morph targets

The previous source-only conclusion has now been tested against the actual binary body assets in `indra/newview/character`. The binary loader stores Morph records inside each base `.llm` file; each record contains a vertex index plus position, normal, binormal, and UV0 deltas. The replay used the serialized base mesh, triangle indices, and Morph records from `avatar_upper_body.llm` and `avatar_lower_body.llm`.

The diagnostic applies one Morph at a time to the base arrays at the requested weight:
- position and UV0 receive the Morph deltas multiplied by weight;
- the normal accumulator and binormal accumulator receive their Morph deltas multiplied by `NORMAL_SOFTEN_FACTOR = 0.65` and weight;
- the normal is normalized, and the candidate binormal is reconstructed by projecting the accumulated binormal onto the normal plane, matching the direction of the Viewer cross-product reconstruction.

The UV-orientation test compares each post-Morph triangle determinant against that triangle's base determinant. The near-degenerate warning threshold is `abs(det) < 1e-5`; the exact-degenerate diagnostic threshold is `abs(det) < 1e-10`.

| Asset / Morph | Weight | UV orientation flips from valid base triangles | Post-Morph near-degenerate triangles |
|---|---:|---:|---:|
| Upper Body / Fat_Torso | 0.25 | 0 | 24 |
| Upper Body / Fat_Torso | 0.50 | 0 | 24 |
| Upper Body / Fat_Torso | 0.75 | 3 | 24 |
| Upper Body / Fat_Torso | 1.00 | 6 | 27 |
| Upper Body / Male_Torso | 0.25 | 0 | 22 |
| Upper Body / Male_Torso | 0.50 | 0 | 22 |
| Upper Body / Male_Torso | 0.75 | 0 | 23 |
| Upper Body / Male_Torso | 1.00 | 7 | 27 |
| Lower Body / Fat_Legs | 1.00 | 4 | 2 |

For `Male_Torso`, two additional apparent sign changes at weights 0.25–1.0 originate from triangles that were already effectively degenerate in the base asset (base determinants approximately +/-6.7e-11). They are not counted as valid-base orientation flips. At weight 1.0, seven other triangles do cross orientation; their post-Morph determinants are small, so they also need a degeneracy-aware fallback rather than blind tangent splitting.

For `Fat_Torso`, three valid-base triangles flip by weight 0.75 and six by weight 1.0. For `Fat_Legs`, all four flipped triangles collapse to very small post-Morph determinants (approximately -0.0000017 to -0.0000119). These results show that UV0 orientation can change under real, XML-referenced shape Morphs; a tangent split computed once from base UVs is not robust. The practical issue is not simply a large number of cleanly mirrored faces: many flips occur as the local UV parameterization approaches degeneracy.

The lower-body asset also contains a record named `default_lowerbodyParallel` that produces eight sign changes when replayed at weight 1. However, that name is not referenced by `avatar_lad.xml`; it is therefore excluded from the active user-shape Morph results until its intended activation path is established. The same caution applies to any unreferenced binary Morph record. `Male_Legs` did not produce a valid-base orientation flip in this pass.

**Scope and limitations:** this is a one-Morph-at-a-time diagnostic from the base mesh, without avatar runtime mask textures, simultaneous Morph combinations, per-frame joint skinning, or GPU execution. The full-weight replay is a boundary test, not a claim that every avatar reaches that exact state. It establishes that live UV orientation can change and that near-degenerate UV triangles need explicit handling; it does not yet select a production tangent-splitting or dynamic-rebuild algorithm.

**Updated engineering decision:** tangent handedness and seam groups must be derived from the live post-Morph UV0 state, or maintained through an equivalent robust update mechanism. A static base-UV split alone is insufficient. Before implementation, the next validation should cover combinations of active Morphs, runtime mask replacement, the Head mesh, and the Avatar skinning transform. Do not enable normal mapping or change the Avatar VBO yet.

No runtime source or mesh asset changed. No build or test was run.


### Additive multi-Morph UV replay

A follow-up pass combined XML-referenced Upper Body Morph deltas additively, matching the mesh's accumulated position/UV0 and softened normal-delta model. This is still an unmasked diagnostic from the base mesh, but it tests whether individually modest UV changes compound into additional orientation changes.

| Combined Upper Body Morphs | Weights | Sign changes total | Changes from non-degenerate base faces | Post-Morph near-degenerate faces |
|---|---:|---:|---:|---:|
| Fat_Torso + Big_Chest | 0.75 + 0.75 | 3 | 3 | 24 |
| Fat_Torso + Big_Chest | 1.00 + 1.00 | 12 | 12 | 27 |
| Fat_Torso + Muscular_Torso | 0.75 + 0.75 | 3 | 3 | 24 |
| Fat_Torso + Muscular_Torso | 1.00 + 1.00 | 12 | 12 | 33 |
| Fat_Torso + Male_Torso | 0.75 + 0.75 | 13 | 11 | 29 |
| Fat_Torso + Male_Torso | 1.00 + 1.00 | 41 | 39 | 25 |

The two excluded changes in the `Fat_Torso + Male_Torso` rows again originate from base triangles whose determinants were already approximately +/-6.7e-11. The 39 valid-base changes at full weight show that combined shape changes can affect substantially more faces than either Morph tested in isolation. Several post-Morph determinants remain small, so the production response must distinguish a stable orientation reversal from a triangle whose UV parameterization is collapsing.

This combined pass is not a claim that the tested extreme values are a common avatar configuration. It confirms that the tangent frame cannot be treated as immutable after initial mesh load. A future implementation must either update orientation/seam data from the final accumulated UV0 state after the active Morph set is applied, or use a formulation that remains correct without explicit per-frame vertex splitting. Runtime masks and actual driven-parameter values still need replay before selecting the implementation.

No runtime source or mesh asset changed. No build or test was run.
