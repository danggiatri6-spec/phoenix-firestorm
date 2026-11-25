# OpenSim-next Avatar Texture Pipeline Audit

Status: source audit only. No runtime texture failure has been reproduced in this environment, and no Viewer build or visual test is claimed.
Branch: `opensim-next/integration`
Baseline comparison: `Firestorm_7.2.5`

## 1. Scope

This audit separates two workstreams that must not be conflated:

1. The existing native Avatar texture pipeline: legacy baked textures, local wearable textures, appearance compositing, alpha masks, texture fetch, and draw binding.
2. The new optional Avatar skin material pipeline: UUID metadata, optional normal/roughness/specular/detail maps, texture resolver, and shader integration.

The first workstream must remain correct before the second is enabled. This audit does not cover HD body geometry.

## 2. Findings

### 2.1 Existing baked texture fetch path

File: `indra/newview/llvoavatar.cpp`, `LLVOAvatar::getBakedTextureImage()`.

Observed behavior:
- Default Avatar/default/invisible UUIDs are looked up in the standard texture list.
- Other baked UUIDs call `getImageURL(te, uuid)`.
- When the URL is empty, the function requests the legacy bake from the object's simulator host using `FTT_HOST_BAKE`.
- Immediately after that conditional, the function unconditionally calls `getFetchedTextureFromUrl(url, FTT_SERVER_BAKE, ...)`, even when `url` is empty, replacing the earlier host-bake result.

This exact implementation is present in both `opensim-next/integration` and the untouched `Firestorm_7.2.5` baseline. It is therefore inherited behavior, not a regression introduced by OpenSim-next.

Risk to investigate: the empty-URL path appears to discard the host-bake fetch result. Before changing it, verify the empty-URL behavior of `getFetchedTextureFromUrl()`, the legacy bake protocol expectations, and actual host-bake use cases. A safe intended control flow would ordinarily select either host-bake fetch or server-URL fetch, not both, but this must be proven against the underlying texture-manager contract.

### 2.2 Existing baked/local texture update path

File: `indra/newview/llvoavatar.cpp`, `LLVOAvatar::updateTextures()`.

Observed behavior:
- Calls `releaseOldTextures()` first.
- Skips dummy avatars and avoids updating invisible/culled remote avatars.
- Iterates baked texture entries and binds unloaded baked images to trigger decode/load work.
- Iterates texture entries and wearable slots, obtains fetched textures, and updates local texture statistics/usage.
- This is the existing path that must remain the source of truth for classic wearables and baked appearance.

The audit has not yet exhaustively traced every branch in this function, texture callback lifecycle, and compositor upload behavior. No conclusion is made that the legacy texture path is fully correct or fully broken.

### 2.3 Existing appearance compositor

File: `indra/newview/llviewertexlayer.cpp`.

Observed behavior:
- Layer-set updates call `updateMeshTextures()` after the local composite is updated.
- The existing compositor remains the mechanism that combines traditional wearable layers into baked Avatar appearance.
- The new optional skin material maps do not replace this bake path.

The existing baked format and wearable composition must remain unchanged in the first skin-material integration.

### 2.4 New optional skin texture resolver is not wired into runtime

Files:
- `indra/newview/llavatarskintextureresolver.h/.cpp`
- `indra/newview/llavatarskinmaterial.h/.cpp`
- `indra/newview/llvoavatar.h/.cpp`

Observed:
- `LLVOAvatar` owns `mSkinTextureResolver` and `mSkinMaterial`.
- `setSkinMaterial()` sanitizes metadata and releases UUIDs no longer referenced.
- `TextureResolver::resolve()` can request a fetched texture by UUID and retain an `LLPointer`.
- `getState()` reports UNASSIGNED/LOADING/READY/MISSING/DELETED.
- Repository search and inspected call sites show no consumer of `getSkinTextureResolver()` or call to `resolve()` outside the resolver implementation.
- No current update/draw path requests the optional maps.
- No code binds these optional maps to the native Avatar skinned shader.

Conclusion: the new resolver and material metadata are infrastructure only. They do not currently load or render optional skin maps.

### 2.5 Native Avatar deferred shader only consumes diffuse

Files:
- `indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl`
- `indra/newview/lldrawpoolavatar.cpp`
- `indra/newview/llviewershadermgr.cpp`

Observed:
- `gDeferredAvatarProgram` is built from `deferred/avatarV.glsl` and `deferred/avatarF.glsl`.
- `LLDrawPoolAvatar::beginDeferredSkinned()` enables `DIFFUSE_MAP`.
- `endDeferredSkinned()` disables `DIFFUSE_MAP`.
- The inspected deferred Avatar fragment shader samples `diffuseMap`, writes diffuse to G-buffer target 0, writes a zero material target 1, and encodes the interpolated Avatar normal in target 2.
- The separate deferred impostor path binds normal/specular channels, but that does not mean the full-resolution skinned Avatar shader uses those channels.

Conclusion: normal/specular support in the impostor path must not be mistaken for normal/specular support in the full skinned Avatar path.

## 3. Required separation of issues

Do not report the following as one generic "texture bug":

- Legacy baked texture fetch or host/server bake routing.
- Local wearable texture fetching and compositing.
- Alpha-mask/transparent clothing behavior.
- GPU upload, texture residency and mip/LOD behavior.
- Optional skin-map fetching through the new resolver.
- Optional skin-map binding and sampling in Avatar shaders.

Each needs its own reproduction, logs and acceptance test.

## 4. Next implementation sequence

### Step A — finish legacy texture-path audit
1. Trace `getImageURL()` and `getFetchedTextureFromUrl()` behavior for empty and non-empty URLs.
2. Trace all callers of `setTETexture()`, including legacy host bake and server bake paths.
3. Trace texture callbacks, missing-asset handling, decode completion, GPU upload and texture eviction.
4. Trace local wearable texture loading through compositor input and final baked output.
5. Compare OpenSim legacy bake behavior with Second Life server-bake behavior.
6. Create a small reproducible test matrix before modifying inherited code.

### Step B — fix only reproduced legacy defects
- Preserve existing fallback semantics.
- Avoid changing the server-side baked texture format.
- Do not alter classic UVs, clothing layer composition or alpha semantics.
- Add diagnostics for selected fetch route (default/local/host-bake/server-bake), UUID, texture state and completion state, with no sensitive user data.

### Step C — connect optional skin maps
1. Request only enabled, supported material slots.
2. Define texture priority/boost and expected dimensions/format per map.
3. Track async completion and missing/deleted states.
4. Bind textures only in the intended native Avatar shader path.
5. Use shader permutations or safe defaults so disabled/missing maps preserve the legacy result.
6. Release per-Avatar references without invalidating globally shared texture-manager resources.

### Step D — visual and compatibility validation
Test:
- Self Avatar and remote Avatars.
- Server-baked and legacy host-baked Avatars.
- Default/missing texture UUIDs.
- Skin, tattoo, underwear, shirt, jacket, pants, skirt and alpha-mask wearables.
- Bake update while wearing items, outfit changes, teleport and relog.
- Texture download failure, missing asset and delayed completion.
- Impostor transition and return to full skinned Avatar.
- Low graphics profile and texture-memory pressure.

Record fetch route, UUID, load state, GPU residency, screenshots and logs. Do not claim fixed until the relevant reproduction passes.

## 5. Explicit status

- Legacy texture pipeline: inherited code inspected; full behavior audit and runtime reproduction remain incomplete.
- Empty-URL legacy bake fallback: suspicious inherited control flow identified; not changed pending confirmation of texture-manager behavior and reproduction.
- Optional skin material data: implemented.
- Optional skin texture resolver: implemented but not connected to runtime consumers.
- Native Avatar shader material integration: not implemented.
- Build/test/visual verification: not performed in this audit.
- HD geometry work: deferred until the texture pipeline work is closed.
