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
