# OpenSim-next Avatar State Synchronization Research

Date: 2026-11-24

## Purpose

Evaluate publicly available Vircadia (derived from High Fidelity) Avatar code against the existing Firestorm/OpenSim architecture. This is an architectural study, not a code port. Preserve Second Life protocol behavior and existing LSL/AO/furniture/HUD compatibility.

## Sources inspected

### Vircadia / High Fidelity lineage

Repository: https://github.com/live3d/vircadia

- `libraries/avatars/src/AvatarData.h` (blob observed: `efd1c0f694300d967194a5a5901db2c05e4b0fe9`)
- `libraries/avatars/src/AvatarData.cpp` (blob observed: `d90b1936c0bc2b7328b863207c05a9691359a71e`)
- `assignment-client/src/avatars/AvatarMixer.cpp`

The inspected AvatarData API/data model covers:
- global/local position, orientation and scale;
- look-at position and head orientation;
- joint rotations/translations, raw joint data and hand state;
- face-tracker/blendshape data through HeadData;
- avatar data detail tiers (`NoData`, `PALMinimum`, `MinimumData`, `CullSmallData`, `IncludeSmallData`, `SendAllData`);
- stateful serialization, last-sent joint data, bounded packet size and per-field rate counters.

The AvatarMixer uses a worker pool to process queued avatar packets and broadcast avatar data. Its source exposes throttling and listener/mixer statistics. This is a useful model for prioritization and observability, not a drop-in replacement for OpenSim.

Vircadia repository license: Apache-2.0 at the repository root. Any future reuse must preserve notices and verify licenses of each copied file and dependency. This note does not authorize copying third-party dependencies with incompatible terms.

### Existing Firestorm Viewer

Repository: https://github.com/danggiatri6-spec/phoenix-firestorm

Branch inspected: `opensim-next/avatar-modernization`

- `indra/newview/llvoavatar.h`
- `indra/newview/llvoavatar.cpp`
- `indra/newview/llviewerobject.cpp`
- `indra/newview/llviewerregion.cpp`

Existing relevant extension points:
- `LLVOAvatar::idleUpdate()` and `updateCharacter()` provide the avatar update/render scheduling path.
- `LLVOAvatar::processUpdateMessage()` receives the existing simulator object/avatar update path.
- `LLViewerObject::processUpdateMessage()` parses legacy object update messages.
- `LLViewerRegion::requestPostCapability()` supports asynchronous HTTP capability requests and success/failure callbacks.
- Firestorm already has an extensive avatar animation, skeleton, visual parameter, attachment, voice and render-complexity pipeline.

### Existing OpenSim server

Repository: https://github.com/danggiatri6-spec/opensim

Branch inspected: `opensim-next/avatar-modernization`

- `OpenSim/Region/Framework/Scenes/ScenePresence.cs`

OpenSim's existing `ScenePresence` owns the simulator-side presence, appearance, movement/physics actor, animator and movement animation overrides. The server already has mature legacy appearance, animation and movement replication paths. These must remain authoritative for legacy clients.

## Key architectural conclusion

Do **not** replace the existing Second Life avatar packet model with a Vircadia-style packet format. Do not send high-frequency pose/joint updates as repeated LLSD HTTP capability POSTs. HTTP capabilities are appropriate for negotiation, policy, configuration, event/control messages and infrequent metadata; they are a poor transport for 20-60 Hz pose streams.

Use a layered, opt-in extension:

1. **Legacy compatibility layer** — unchanged SL/OpenSim appearance, animation, movement, attachment and object update behavior. Legacy clients receive no new mandatory fields.
2. **Capability control plane** — negotiate protocol version, feature flags, skeleton/rig profile, blendshape profile, maximum rates, permissions and fallback mode. Use bounded LLSD maps and strict input validation.
3. **Realtime data plane** — only after negotiation, use a versioned, compact, bounded binary message path for incremental pose state. The transport must be explicitly supported by both Viewer and simulator. Do not assume an unregistered custom UDP message can pass through existing SL gateways.
4. **Viewer presentation layer** — apply received optional pose data to a separate overlay/pose layer, preserving the existing animation controller as the baseline. Do not overwrite AO, LSL-triggered animations, sit animations or legacy joint behavior.
5. **Server authority and safety** — server validates sender identity, region membership, sequence numbers, timestamp bounds, joint count, finite values, rate limits and payload size. Client-provided pose is presentation data, not authority for movement, collision, permissions or object ownership.

## Proposed data model (conceptual; not yet implemented)

A negotiated AvatarState v1 should use explicit feature groups rather than copying Vircadia's entire AvatarData structure:

- Root: protocol version, sequence, client timestamp, flags.
- Head: normalized head orientation and optional look-at target.
- Face: profile identifier plus bounded blendshape coefficients; do not assume all rigs share identical coefficient indices.
- Hands: compact hand pose/state and optional finger joint subset.
- Body pose: optional joint rotations, with rig/profile identifier and a strict maximum joint count.
- Quality: sender-selected detail tier; receiver can request/cull lower detail based on distance, visibility and bandwidth.

Use per-field dirty tracking and last-sent state. Send only changed fields, cap payload size, and prioritize root/head/hands before full-body detail. Apply interpolation and stale-packet rejection. A missing optional field must mean “no update”, not reset to a default pose.

## Compatibility and animation composition rules

- The default for existing grids and viewers remains disabled.
- SL-compatible animation state remains the base layer.
- Optional facial expression and gaze may compose as additive/overlay layers only where the rig supports it.
- AO, LSL animation commands, furniture/sit animations and attachment behavior must retain their current precedence.
- Never infer a universal joint map from one avatar rig. Negotiate skeleton profile and map by stable joint names or an agreed profile.
- If an extension is unsupported, continue normal legacy avatar behavior without warnings or broken appearance.

## Performance rules

- Use distance, visibility, avatar screen size and interaction relevance to select detail tiers.
- Near/interacting avatars may receive face, hand and selected joint data; distant avatars should receive root/head or legacy data only.
- Apply adaptive rate limits and per-agent bandwidth budgets.
- Avoid per-avatar allocations in hot paths; reuse buffers and maintain dirty masks.
- Record bytes/sec, update rate, dropped/stale packets, decode time and queue depth.
- A dedicated mixer process is not a first step. First profile the existing OpenSim region update pipeline and Viewer avatar update costs; only introduce worker queues if profiling proves a bottleneck.

## Implementation sequence

### Phase A — protocol and compatibility groundwork
- Confirm exact existing Viewer and OpenSim legacy animation/appearance message paths.
- Define capability negotiation and opt-in feature flags.
- Add server-side configuration defaulting to disabled.
- Add versioned schema documentation and input validation tests.

### Phase B — low-risk presentation features
- Add local-only eye blink/gaze and facial expression prototypes first, with no network protocol changes.
- Add optional remote face/gaze data after negotiation.
- Keep fallback to legacy animations.

### Phase C — compact pose synchronization
- Add incremental hand/head/joint data transport.
- Implement sequence, rate, size and finite-value validation.
- Add distance/visibility quality tiers and interpolation.

### Phase D — physical interaction
- Add collision/contacts only after stable skeleton and pose data exist.
- Separate visual contact events from authoritative world physics and consent policy.
- Reuse the existing AvatarInteraction capability for invitation/consent control; do not conflate it with high-frequency pose transport.

## Next concrete engineering task

Before adding a new packet or server module, map the exact existing animation and appearance replication path end-to-end in the current OpenSim branch and matching Firestorm branch. Identify the serializer, message name, send cadence, receive handler, animation precedence and any OpenSim-specific differences. Then prototype local-only face/gaze composition in the Viewer. This minimizes compatibility risk and avoids prematurely committing to a transport.

## Verification status

- Source files above were retrieved and inspected through GitHub.
- This is a source-level architecture assessment only.
- No code was copied from Vircadia.
- No new AvatarState protocol or implementation has been added.
- No build, simulator runtime test or network interoperability test has been performed.


## End-to-end legacy animation path mapped (2026-11-24)

The matching OpenSim server branch was inspected further:

- `OpenSim/Region/Framework/Scenes/Animation/ScenePresenceAnimator.cs`
  - `GetArrays()` obtains active animation UUIDs, sequence numbers and source object UUIDs from the animation state.
  - `SendAnimPackToClient(IClientAPI)` sends those arrays through `IClientAPI.SendAnimations(...)`.
  - `SendAnimPack()` broadcasts the same legacy animation state through `ScenePresence`.
- `OpenSim/Region/Framework/Scenes/ScenePresence.cs`
  - `SendAnimPackToAgentNF()`, `SendAnimPackToOthers()`, and initial-avatar setup send the legacy animation pack.
  - `SendAvatarDataToAgent()` sends the avatar as an entity full update.
  - `SendAppearanceToAgent...` is a separate appearance replication path.
  - `SendViewTo()` sends avatar entity update, appearance, animation pack and attachments as separate steps.
- `OpenSim/Region/ClientStack/Linden/UDP/LLClientView.cs`
  - `SendAnimations(...)` constructs the legacy AvatarAnimation high-frequency packet (message ID 20 in the inspected implementation), carrying source agent UUID, animation UUIDs, sequence IDs and associated object UUIDs.
  - The same class has a separate AvatarAppearance packet writer and entity full-update path.

Matching Firestorm Viewer branch:

- `indra/newview/llviewermessage.cpp`
  - `process_avatar_animation(...)` reads the sender UUID, resolves the corresponding `LLVOAvatar`, and processes the AnimationList blocks.
- `indra/newview/llvoavatar.cpp`
  - `LLVOAvatar::processUpdateMessage(...)` handles the existing avatar/object update path.
  - `LLVOAvatar::idleUpdate()` and `updateCharacter()` are the per-frame/LOD-aware avatar update and presentation paths.
- `indra/newview/llviewerobject.cpp`
  - `LLViewerObject::processUpdateMessage(...)` is the common legacy object update parser inherited by avatar objects.

### What this means

The existing AvatarAnimation packet communicates **which animation assets are active**, their sequence and source object. It does not carry a per-frame skeleton pose stream, arbitrary joint transforms, or a standardized face blendshape frame. The avatar entity update and appearance packet are distinct channels and should not be overloaded with high-rate facial or joint samples.

Therefore the first safe enhancement should be a Viewer-local expression/gaze composition layer, independent of incoming network state. The next network step should be a negotiated extension, separate from the legacy AvatarAnimation packet. It must not repurpose or change message ID 20, alter legacy packet layouts, or assume that third-party SL servers understand the extension.

The source mapping above is based on the OpenSim and Firestorm branches named in this document. Exact send cadence, all appearance serializers, and all animation precedence rules still require a focused follow-up trace before implementing the wire protocol.


### Animation packet semantics and precedence observations

A deeper source trace confirms:

- OpenSim `ScenePresenceAnimator` is the state owner for active legacy animation UUIDs, sequence numbers and source object UUIDs.
- `LLClientView.SendAnimations()` writes the legacy AvatarAnimation packet. For the avatar's own animations it separates animations associated with objects and writes the corresponding object UUID list; it currently writes a zero count for physical-avatar events.
- Firestorm `process_avatar_animation()` resolves the sender to an existing `LLVOAvatar`, clears/rebuilds the signaled-animation map, handles self-avatar animation sources and the animation asset blacklist, then calls `processAnimationStateChanges()`.
- The self-avatar path preserves animation-source object relationships. Any new pose layer must not discard these relationships or bypass the Viewer animation state machine.
- This path is state replication, not a high-frequency pose stream. The current OpenSim packet writer's physical-avatar-events count being zero is not an invitation to repurpose that field; compatibility with existing packet parsers must be preserved.

### Revised first implementation target

Do not start by adding a server-side joint-pose packet. The first implementation should be a **Viewer-local facial/gaze overlay prototype** behind a setting and with no network or server changes. It can establish:
1. where the rig exposes head/eye joints and morph/blendshape controls;
2. how to compose an overlay with the existing animation controller;
3. how to disable it for unsupported rigs and for legacy compatibility;
4. how to keep the cost bounded by visibility and avatar update period.

After that local prototype is stable, define the negotiated extension and only then implement server replication.


## Viewer-local face feature inventory (2026-11-24)

A source inspection of `indra/newview/llvoavatar.cpp` and `indra/newview/character/avatar_lad.xml` reveals that the legacy Viewer already has part of the required foundation:

- `LLVOAvatar::buildCharacter()` obtains the `Lipsync_Ooh` and `Lipsync_Aah` visual parameters, with fallbacks to `Express_Kiss` and `Express_Open_Mouth`.
- `LLVOAvatar::idleUpdateLipSync()` drives those morphs from the voice visualizer and speaking state, then updates visual parameters and dirties the mesh.
- `LLVOAvatar::initClass()` registers the built-in `ANIM_AGENT_EYE` motion through `LLEyeMotion::create`, and `startDefaultMotions()` starts it with other baseline motions.
- The standard avatar definition contains animatable `Blink_Left` and `Blink_Right` morph parameters (IDs 58 and 59 in the inspected definition).

This changes the implementation strategy: do not create a competing blink system until the built-in `LLEyeMotion` implementation and its behavior are traced. First determine whether it already provides timed blinking and whether the limitation is only lack of independent gaze targeting or poor compatibility with newer rigs. Likewise, preserve the existing lip-sync path and compose additional face controls around it rather than overwriting its morph weights.

A likely safe local prototype is an opt-in gaze controller that:
- operates only on fully built, visible avatars with valid eye/head joints;
- uses the existing avatar joint/motion framework rather than directly writing root transforms;
- has deterministic fallback to the built-in eye motion when the rig lacks required joints;
- updates at a bounded rate and smooths target changes;
- never changes legacy animation UUID state or server movement.

The actual `LLEyeMotion` class implementation and its registration/activation lifecycle still need to be located before changing code. No facial or gaze implementation has been modified in this research pass.


## First compatibility improvement implemented (2026-11-24)

File changed: `indra/llcharacter/llheadrotmotion.cpp`

The built-in `LLEyeMotion::onInitialize()` previously treated both alternate face-eye joints (`mFaceEyeAltLeft` and `mFaceEyeAltRight`) as mandatory. If a compatible/custom rig exposed the primary eye joints but omitted the alternate pair, initialization failed and the default eye motion could be disabled entirely.

The implementation now:
- continues to require the head joint and primary left/right eye joints;
- treats the alternate face-eye pair as optional;
- registers and updates the alternate pair only when both joints exist;
- retains the primary eye path unchanged;
- keeps deactivation null-safe.

This is a small compatibility hardening change, not yet a new gaze feature. It does not change animation UUIDs, network packets, server behavior, or the default Second Life rig path.

Verification performed: source-level checks confirmed the two primary eye joints remain required, alternate-joint registration and update are guarded, and deactivation checks null joints. No C++ compilation or runtime test has been performed. The next validation should include a standard SL avatar rig and a test rig with the alternate eye joints omitted.


## Existing gaze-target pipeline confirmed

Further inspection of `indra/newview/llhudeffectlookat.cpp` shows that the Viewer already has a target pipeline that should be reused:

- `LLHUDEffectLookAt::calcTargetPosition()` resolves a target Avatar to its head world position, or resolves a non-avatar object/world position.
- It converts the target into a vector relative to the source Avatar's head.
- Unless `DisableLookAtAnimation` is enabled, it stores that vector as the source Avatar's `LookAtPoint` animation data.
- Both `LLHeadRotMotion` and `LLEyeMotion` consume `LookAtPoint`, so head and eye motion already share one target.
- The standard `settings.xml` declares `DisableLookAtAnimation` with value 0 (enabled); the C++ fallback value of true is used only if the setting is absent.

This means the next gaze feature should be a **target-selection/arbitration layer**, not a second eye-rotation implementation. Candidate target sources can be ranked and blended into the existing LookAtPoint input: explicit user focus, interaction partner, selected object, then ambient idle behavior. Keep the existing privacy setting effective, retain the current HUD effect path, and do not alter legacy look-at packet semantics.

The existing eye motion already supplies random jitter, occasional look-away, return-to-target, vergence, and blinking. The gap is chiefly richer target selection, configurable naturalness, interaction-aware attention, and better rig fallbacks—not basic blink or eye rotation.
