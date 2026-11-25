# OpenSim-next Native Avatar HD Geometry Specification

Status: source-audited architecture specification. No new geometry runtime, asset format, or build success is claimed.
Branch reviewed: `opensim-next/integration`
Baseline: Firestorm 7.2.5-derived Viewer

## 1. Goal

Upgrade the traditional system Avatar so it can gain important Mesh Avatar advantages—higher geometric detail, smoother deformation, richer facial/body shape control, and a path to modern skin materials—without requiring users to replace their system body or abandon legacy wearables.

The project must preserve:
- Existing Avatar identity and appearance flow.
- Existing skeleton hierarchy and animation behavior.
- Existing Shape sliders and wearable visual parameters.
- Existing baked texture and classic clothing workflows.
- Existing attachment points, alpha behavior, eyes, eyelashes, hair and face animation.
- Existing Second Life/OpenSim asset and protocol compatibility unless a separately versioned extension is explicitly designed.

This is not a plan to silently replace the classic body with a third-party mesh body.

## 2. Source audit

Files inspected on `opensim-next/integration`:

- `indra/llappearance/llpolymesh.h`
- `indra/llappearance/llpolymesh.cpp`
- `indra/llappearance/llpolymorph.h`
- `indra/llappearance/llpolymorph.cpp`
- `indra/llappearance/llavatarappearance.cpp`
- `indra/newview/character/avatar_lad.xml`
- `indra/newview/character/avatar_skeleton.xml`
- `indra/newview/llvoavatar.h`
- `indra/newview/llvoavatar.cpp`
- `indra/newview/lldrawpoolavatar.cpp`
- `indra/newview/llviewertexlayer.cpp`

### 2.1 Native geometry representation

`LLPolyMeshSharedData` contains the shared mesh data, including:
- Base coordinates, normals and binormals.
- Primary and optional detail UV coordinates.
- Per-vertex weights and a flag indicating whether weights exist.
- Face/index data.
- Skin-joint names.
- Morph target data.
- An optional reference mesh used for LOD sharing.

`LLPolyMesh` contains per-instance/deformed data, including morphed vertex data, normals/binormals, clothing weights and output texture coordinates.

The mesh loader reads a 16-bit vertex count for the base mesh. This is a constraint of the current legacy mesh asset representation and loader path; do not assume that merely increasing polygon density is a safe or unlimited operation.

### 2.2 Morph topology dependency

`LLPolyMorphData` stores vertex indices and per-vertex position, normal, binormal and UV deltas. `LLPolyMorphTarget` applies these data to a particular `LLPolyMesh`.

The morph implementation is therefore topology-dependent. A replacement mesh with a different vertex order, vertex count, UV layout or surface correspondence cannot safely reuse the existing morph data without an explicit remapping or conversion process.

Shape controls in `avatar_lad.xml` are not independent cosmetic values: they reference morph data and may also drive skeleton/collision-volume changes. Any geometry upgrade must audit both vertex morphs and skeleton/volume effects.

### 2.3 Current LOD relationship

`LLPolyMeshSharedData::setupLOD()` assigns the LOD mesh a reference to the higher/base mesh's coordinates, normals, binormals, UVs, detail UVs and weights. LOD face indices refer to that shared vertex data.

Consequences:
- Current LOD assets are not simply independent higher-resolution meshes.
- Replacing the base mesh changes the shared data contract for dependent LODs.
- A new high-detail representation needs a deliberate LOD family and validated index correspondence.
- LOD transitions must not change UV seams, morph behavior or skinning unexpectedly.

### 2.4 Skeleton and animation

The current avatar configuration loads a skeleton file and an avatar appearance definition. The inspected skeleton includes base and extended-support bones, including face and hand-related bones.

The existence of extended bones does not prove that every legacy body mesh has useful weights for all of them. New geometry must explicitly define and validate weights for every supported deformation bone. Existing animation assets must continue to drive the same expected joints.

### 2.5 Appearance and rendering

The traditional Avatar geometry, appearance/bake system and Avatar draw path are distinct from rigged mesh attachments and GLTF/PBR object rendering.

The already implemented viewer-local `LLAvatarSkin::MaterialData` and texture resolver remain a separate material-data foundation. They do not upgrade geometry by themselves and must not be treated as evidence that the current body mesh has become high resolution.

## 3. Core architecture decision

Adopt a **Legacy-Compatible Native Geometry Upgrade** with two selectable geometry profiles:

1. **Legacy Geometry Profile** — current body meshes, current morph data, current LOD behavior and existing appearance pipeline. This remains the guaranteed fallback.
2. **HD Geometry Profile** — a new, versioned native geometry family with explicit correspondence data for Shape morphs, skinning, UVs, material regions and LODs.

Both profiles use the existing Avatar identity, skeleton/animation system and appearance controls wherever compatibility is proven. The HD profile is opt-in until validated. Never silently reinterpret old mesh or morph assets as HD assets.

The HD profile should not be implemented by routing the native body through the rigged-attachment renderer. It should remain a first-class native Avatar geometry path, with a deliberate bridge to the shared Avatar skin material contract.

## 4. HD geometry asset contract

A future HD geometry package must define, at minimum:

- Schema/version identifier and stable asset identity.
- Body region and submesh/material-region identifiers.
- Base positions, normals and tangent basis.
- Primary UVs and any explicitly supported secondary UV set.
- Skinning joint names and normalized weights.
- LOD levels with explicit topology and transition metadata.
- Shape parameter mapping from legacy visual parameter names to HD morph targets.
- Morph target deltas or a validated transfer map from legacy topology.
- Clothing/body coverage masks and compatibility metadata.
- Bounds, validation metadata and safe fallback behavior.

Do not introduce an undocumented binary format. First produce a format proposal and validation tooling. Reuse existing asset containers only if their constraints are proven sufficient; otherwise version the new format and define its distribution, permissions, cache and server compatibility separately.

## 5. Morph compatibility strategy

Do not assume one-to-one vertex indices between legacy and HD meshes.

Evaluate these strategies in order:

1. **Authoritative HD morph set**: create HD-native morph targets corresponding to the existing visual parameter names. This is the preferred quality path.
2. **Offline morph transfer**: transfer legacy morph displacement to the HD surface using a validated surface correspondence/barycentric mapping, then review and correct the result artistically.
3. **Runtime mapping**: only consider if offline conversion is insufficient and profiling proves the runtime cost acceptable.

The mapping must account for more than positions: normals/tangents, clothing masks, region boundaries, and any coupled skeleton/collision-volume changes must remain coherent.

Existing wearable sliders should keep their names and semantic ranges where possible. If an HD morph cannot reproduce a legacy parameter, it must report a compatibility gap and use a documented fallback rather than silently ignoring the control.

## 6. UV, baked texture and clothing compatibility

The first HD release must preserve the legacy body UV contract wherever the existing baked skin/clothing textures are expected to map.

If the HD geometry needs improved UV layout:
- Treat it as a separately versioned UV set.
- Provide a tested legacy-UV compatibility channel or explicit texture remapping.
- Do not change the server-side baked texture format in the first phase.
- Do not assume creator-made clothing appliers will automatically support a new UV layout.
- Maintain a compatibility matrix for classic system clothing, tattoo/underwear layers, alpha masks, universal wearables and mesh clothing.

The safest first HD body keeps the established UV layout and improves topology/normal quality around it. UV redesign is a later, separately gated project.

## 7. Skinning, deformation and joint behavior

- Preserve legacy joint names and aliases.
- Validate normalized weights and maximum influence assumptions against the existing skinning implementation.
- Validate shoulders, elbows, wrists, hips, knees, ankles, neck and face under extreme poses.
- Validate extended hand and facial bones only where the HD mesh has authored weights.
- Do not change animation assets or joint transforms as a shortcut for correcting bad geometry.
- Keep collision-volume and skeleton-driven Shape behavior consistent.
- Detect missing joints, invalid weights, NaNs and out-of-range indices during asset validation.

## 8. LOD and performance design

The HD family must provide independent, authored LOD topology with explicit morph/skin compatibility. Avoid assuming the current shared-vertex LOD scheme can represent all HD requirements.

Proposed quality policy:
- Low: legacy geometry, legacy Avatar material path.
- Medium: optimized HD body LOD with conservative material features.
- High: HD body LOD with full supported skin material channels.
- Ultra: highest authored body LOD and optional advanced skin shading.

The viewer must choose LOD based on projected size, avatar importance and performance budget. Remote avatars must not force high-resolution geometry or additional texture residency. Include hysteresis to prevent rapid LOD oscillation.

No fixed polygon target is approved yet. Set vertex/triangle budgets only after profiling the current baseline and representative low-end hardware.

## 9. Relationship to Avatar Skin Material

The existing `LLAvatarSkin::MaterialData` remains a separate, optional material contract.

Geometry and material responsibilities:
- Geometry supplies stable UVs, normals/tangents, material regions and deformation.
- Material data supplies optional normal/roughness/specular/detail/SSS references and factors.
- The legacy baked diffuse appearance remains the fallback base color.
- Missing or invalid optional maps must fall back safely.
- The HD geometry path must not force all users to download high-resolution material maps.

Native Avatar skin and rigged Mesh Avatar attachments may share material semantics, but they do not automatically share the same draw path or shader bindings.

## 10. Implementation phases and gates

### Phase 0 — Baseline inventory
- Record current mesh asset names, LOD relationships, vertex/index counts, UV sets, morph counts and joint weights.
- Capture fixed-camera baseline images and animation poses.
- Record CPU/GPU time, memory and Avatar draw calls.
- Build a compatibility inventory for legacy clothing and baked textures.

Exit gate: a reproducible baseline dataset and a complete list of geometry assets and dependencies.

### Phase 1 — HD asset specification and validator
- Finalize versioned HD geometry schema.
- Build offline validation for topology, UVs, morph correspondence, weights, joints, LODs and bounds.
- Define fallback rules and compatibility reports.

Exit gate: validator rejects malformed assets and reports all required compatibility fields. No viewer runtime change yet.

### Phase 2 — One-region prototype
- Prototype one isolated region (preferably torso or head, selected after baseline review).
- Keep legacy UVs and existing skeleton semantics.
- Author HD morphs or a reviewed offline transfer.
- Compare legacy and HD geometry at matching Shape values and poses.

Exit gate: no critical morph/UV/skinning regressions in the prototype region.

### Phase 3 — Native HD geometry runtime
- Add opt-in profile selection and safe fallback.
- Load HD geometry without changing legacy asset interpretation.
- Integrate with existing native Avatar joint, morph and draw lifecycle.
- Add memory accounting and LOD selection.

Exit gate: self/remote Avatar, teleport, appearance update, impostor and cleanup tests pass.

### Phase 4 — Full body and face
- Expand to head, upper body, lower body, hands and feet.
- Validate face bones, eye/eyelash/hair boundaries and alpha behavior.
- Validate all supported legacy Shape controls and coupled skeleton parameters.

Exit gate: documented coverage of all legacy controls and supported animation bones.

### Phase 5 — Clothing compatibility
- Test classic clothing and baked layers across body shapes.
- Add explicit compatibility reporting for assets that require remapping or author updates.
- Do not claim universal clothing compatibility without a representative asset test corpus.

Exit gate: published compatibility matrix and no silent visual corruption.

### Phase 6 — Material and creator tools
- Connect the HD geometry's material regions to the existing Avatar Skin Material contract.
- Add creator preview, validation and packaging tools.
- Define asset permissions, transfer/copy behavior, delivery and cache invalidation before public distribution.

Exit gate: creators can validate an HD asset package and users can fall back to legacy appearance.

## 11. Required regression matrix

Test all of the following before enabling HD geometry by default:

- Self and remote Avatars.
- Default and extreme Shape slider combinations.
- Legacy skins, tattoos, underwear and clothing layers.
- Alpha masks, eyelashes, hair and eye surfaces.
- Walk, run, sit, crouch, jump, dance and extreme joint poses.
- Hand/finger and facial animations where supported.
- Attachments and attachment point alignment.
- Appearance changes, bake updates, relog and teleport.
- Impostor transitions and all LOD levels.
- Low graphics profile and low-memory conditions.
- OpenSim grid login/appearance and Second Life compatibility smoke tests.

Capture before/after images under identical lighting and camera settings. Record GPU time, CPU time, memory, vertex/triangle counts and draw calls.

## 12. Explicit non-goals

- Do not replace the legacy Avatar by default.
- Do not require users to buy or wear a Mesh body.
- Do not break legacy Shape sliders or silently drop unsupported parameters.
- Do not change the network appearance protocol or baked texture format in the first phase.
- Do not modify existing animation behavior to conceal geometry defects.
- Do not promise that every third-party clothing item will fit a different topology/UV layout.
- Do not claim GTA-level realism without reproducible visual and performance evidence.

## 13. Immediate next engineering task

Complete Phase 0 inventory before writing runtime code:
1. Enumerate all native body mesh and LOD assets referenced by the avatar definition.
2. Extract vertex/index counts, UV presence, skin-weight availability, joint names and morph target counts.
3. Map every visual parameter to its morph target and any skeleton/volume side effects.
4. Identify the exact legacy UV/bake regions and clothing-mask dependencies.
5. Produce a machine-readable audit report and a human-readable compatibility table.

Only after this inventory should the team choose the first HD prototype region and approve geometry budgets.
