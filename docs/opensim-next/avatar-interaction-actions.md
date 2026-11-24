# Avatar Interaction Actions — implementation contract

Status: design contract for the next Viewer + simulator implementation step.

## Current verified Viewer integration

The `opensim-next/avatar-modernization` branch already contains:

- LLSD HTTP message registrations for `AvatarInteractionInvite`, `AvatarInteractionAccepted`, and `AvatarInteractionEnded` in `llviewergenericmessage.cpp`.
- An invitation callback that posts `accept` or `reject` plus `request_id` to the `AvatarInteraction` capability.
- A `LLFloaterGesture` quick-expression row and an interaction-policy toggle.
- LLSD message flavor declarations and an invitation notification in the English XUI.

This is invitation reception and policy UI, not yet a complete target-selection / outbound-action flow.

## Scope of this increment

Implement the outbound action path without changing legacy LSL animation behavior:

1. Select a target avatar from the existing avatar context/pie menu.
2. Offer only simulator-advertised actions.
3. Send an invitation request to the current region's `AvatarInteraction` capability.
4. Keep the invitation pending until the simulator returns a request identifier or an explicit error.
5. Start paired animation only after the simulator confirms acceptance.
6. Stop only animation instances owned by this interaction session when it ends.

## Capability request contract

The Viewer should send an LLSD map:

- `action`: `invite`
- `target_id`: target avatar UUID
- `interaction`: stable action key, e.g. `handshake`, `high_five`, or `shoulder_pat`

The simulator response must be treated as authoritative. It should include a stable `request_id` on success and a machine-readable error code on failure. The Viewer must not invent a request ID or optimistically play an animation.

The existing accept/reject callback remains:

- `action`: `accept` or `reject`
- `request_id`: simulator-issued request UUID

The simulator must validate target presence, same-region membership, range, policy, friendship/permission rules, cooldown, and request expiry on both invite and accept. The server must re-check range and presence at acceptance time.

## Paired animation contract

An accepted interaction is a short-lived server-owned session with:

- `session_id`
- `actor_id` and `target_id`
- `interaction` key
- two role-specific animation asset identifiers
- optional alignment transform / facing policy
- authoritative start and end notifications

Animation identifiers must come from a simulator/viewer shared action catalog or explicit negotiated metadata. Do not hard-code guessed UUIDs or assume that a Second Life asset UUID exists in every OpenSim grid.

The Viewer should:
- save the pre-interaction locomotion/animation state;
- avoid stopping unrelated AO, gesture, LSL, or HUD animations;
- play only the role animation assigned to the local avatar;
- stop only the animation started by this session;
- restore normal movement and animation state on end, timeout, region change, teleport, disconnect, or avatar disappearance.

## Security and compatibility rules

- No unsolicited animation start: both participants must explicitly accept unless a future action is explicitly documented as one-sided and non-contact.
- Simulator policy and permissions are authoritative; UI visibility is not authorization.
- Reject self-targets, stale UUIDs, malformed action keys, duplicate requests, and replayed request IDs.
- Rate-limit invites and use short expirations.
- Legacy `llStartAnimation`, `llStopAnimation`, AO, and gesture paths remain unchanged.
- Unsupported grids simply omit the capability; the Viewer hides or disables the new context-menu actions without affecting login or ordinary avatar interaction.

## Acceptance tests

1. Capability absent: no new menu action is offered; viewer remains usable.
2. Invite valid nearby avatar: recipient receives one invitation.
3. Recipient rejects or lets it expire: neither avatar starts a paired animation.
4. Recipient accepts: both clients receive the same session identity and correct role animation.
5. Either participant ends, teleports, leaves region, or disconnects: both sides stop the session-owned animation.
6. Concurrent invite, replay, out-of-range accept, blocked policy, and non-friend permission cases are rejected server-side.
7. Existing AO, LSL animation, gestures, and HUD animations continue unaffected.

## Next code files to change

- Viewer: avatar context/pie-menu implementation and a small interaction request helper.
- Viewer: generic-message LLSD handlers for accepted/ended session events.
- Simulator: the `AvatarInteraction` capability handler and session lifecycle.
- Shared: action catalog and role-animation metadata.

Do not add UI callbacks until the exact avatar context-menu class and simulator capability request/response schema have been inspected on the active integration branches.


## Repository inspection update (2026-11-24)

The Viewer repository paths have now been confirmed on `opensim-next/avatar-modernization`:

- `indra/newview/llviewermenu.cpp` — owns menu construction and already includes the avatar action and pie-menu infrastructure.
- `indra/newview/llavataractions.cpp` / `.h` — central avatar action layer; the appropriate place for a reusable `requestInteraction(target_id, interaction_key)` entry point once the request schema is verified.
- `indra/newview/lltoolpie.cpp` — should be inspected for the precise clicked-avatar context and menu dispatch before wiring the action.
- `indra/newview/llviewergenericmessage.cpp` — existing interaction invite/accepted/ended LLSD event receiver.

The simulator repository is `danggiatri6-spec/opensim`, branch `opensim-next/avatar-modernization`. Its `OpenSim/Region/Framework/Scenes/Scene.cs` and `ScenePresence.cs` are present, but the expected Linden Caps path `OpenSim/Region/ClientStack/Linden/Caps/` was not found at that path. No authoritative `AvatarInteraction` server handler or request schema has yet been located in the accessible branch.

**Implementation gate:** do not add an outbound Viewer POST using guessed field names. First locate the server capability registration and handler in the active server branch. Then implement the Viewer action using that exact contract. This prevents a UI that appears to send invitations but can never be handled by the simulator.


## Server capability registration architecture confirmed

The active OpenSim server development branch `opensim-next-dev` exposes the region capability lifecycle through:

- `OpenSim/Region/Framework/Interfaces/ICapabilitiesModule.cs`
- `ICapabilitiesModule.GetCapsForUser(circuitCode)`
- `ICapabilitiesModule.GetCapsPath(agentId)`
- `ICapabilitiesModule.CreateCaps(agentId, circuitCode)` / `ActivateCaps(circuitCode)`

This confirms capability handling is per-agent/per-circuit and attached to the viewer's CAPS seed lifecycle. The concrete `ICapabilitiesModule` implementation and its handler-registration API have not yet been located in the repository snapshot, so the new handler must be integrated there rather than added as an unrelated HTTP listener.

The branch check also confirms that the server's current public working branch is `opensim-next-dev`; the earlier guessed `OpenSim/Region/ClientStack/Linden/Caps/...` path is not present. Do not use that guessed path in implementation.

**Viewer integration point confirmed:** `menu_avatar_other.xml` is the right-click context menu for another avatar; `lltoolpie.cpp` selects and displays it. A menu item can therefore use the exact clicked avatar UUID, but it must remain capability-gated and must not be wired to a guessed LLSD payload.


## Verified server implementation — exact contract (2026-11-24)

The earlier proposed request fields in this document were speculative. They are superseded by the actual implementation found in the server repository:

- Repository: `danggiatri6-spec/opensim`
- Branch: `opensim-next/avatar-modernization`
- Handler: `OpenSim/Region/ClientStack/Linden/Caps/AvatarInteractionModule.cs`
- Handler blob SHA: `b5bc42d619fd6d707f7ffe7d529e86d21bfd84db`

The capability is registered during `Scene.EventManager.OnRegisterCaps` using `caps.RegisterSimpleHandler("AvatarInteraction", ...)`. This is the same established OpenSim per-agent CAPS pattern used by other Linden capability modules.

### Exact outbound LLSD request fields

- `action=set_policy`: `policy` is one of `off`, `friends`, `ask`, `everyone`.
- `action=check`: `target` is the target avatar UUID as a string.
- `action=invite`: `target` is the target avatar UUID as a string; `interaction` is one of `handshake`, `high_five`, `shoulder_tap`.
- `action=accept`, `reject`, or `cancel`: `request_id` is the simulator-issued invitation UUID as a string.

Important corrections:
- The field is `target`, not the previously proposed `target_id`.
- The implemented action key is `shoulder_tap`, not `shoulder_pat`.
- The server currently issues an invitation `request_id`; it does not yet create a paired `session_id` or role-specific animation metadata.
- The server response is an LLSD map with `ok` and `status`; successful invite also returns `request_id`. Policy success also returns `policy`.

### Exact event payloads

The invite event `AvatarInteractionInvite` contains `request_id`, `requester`, `requester_name`, `interaction`, and `expires_in`.

The accepted/ended event bodies contain `request_id`, `requester`, `target`, `interaction`, `status`, and `reason`. The current Viewer event handlers already recognise the invite's `requester_name` + `request_id` shape and the accepted/ended `status` shape, but accepted events currently only display a notification; they do not start paired animations.

### Verified server-side limits and gaps

- Invitation lifetime: 30 seconds.
- Maximum pending invitations: 128.
- One pending invitation per target.
- Same-scene root-presence/lifecycle and 3-metre distance checks.
- Policies are per-presence and default to `Off`; they are not persisted.
- `Friends` checks `IFriendsModule.IsFriend(target, requester)`.
- `Ask` allows delivery of the invitation; explicit target acceptance is still required.
- No per-agent rate limiter, automatic departure/teleport cleanup, session state, or paired animation start/end lifecycle is implemented yet. Expiry pruning is lazy on a subsequent request.

The server implementation is present on `opensim-next/avatar-modernization`; it was not present in the `opensim-next-dev` branch checked earlier. Do not describe `opensim-next-dev` as containing this feature.

### Viewer implementation sequence now unblocked

1. Add the outbound invite helper using `action=invite`, `target`, and `interaction` exactly as above.
2. Wire the helper to the clicked Avatar UUID from the existing avatar context menu.
3. Expand the policy UI from the current `everyone/off` toggle to all four server-supported values.
4. Keep accepted/ended handling notification-only until the simulator gains a real paired session contract. Do not invent `session_id` or animation UUID fields.
