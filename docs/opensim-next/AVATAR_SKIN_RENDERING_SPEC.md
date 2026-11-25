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
