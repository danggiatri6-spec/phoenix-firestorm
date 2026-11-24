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
- `target`: target avatar UUID (verified server field)
- `interaction`: `handshake`, `high_five`, or `shoulder_tap` (verified server values)

The simulator response is an LLSD map containing `ok` and `status`; a successful invite also returns the simulator-issued `request_id`. The Viewer must not invent a request ID or optimistically play an animation.

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

The simulator repository is `danggiatri6-spec/opensim`, branch `opensim-next/avatar-modernization`. The authoritative handler is `OpenSim/Region/ClientStack/Linden/Caps/AvatarInteractionModule.cs`; the exact request and event schema is documented below.

**Implementation gate:** do not add an outbound Viewer POST using guessed field names. First locate the server capability registration and handler in the active server branch. Then implement the Viewer action using that exact contract. This prevents a UI that appears to send invitations but can never be handled by the simulator.


## Server capability registration architecture confirmed

The active OpenSim server development branch `opensim-next-dev` exposes the region capability lifecycle through:

- `OpenSim/Region/Framework/Interfaces/ICapabilitiesModule.cs`
- `ICapabilitiesModule.GetCapsForUser(circuitCode)`
- `ICapabilitiesModule.GetCapsPath(agentId)`
- `ICapabilitiesModule.CreateCaps(agentId, circuitCode)` / `ActivateCaps(circuitCode)`

This confirms capability handling is per-agent/per-circuit and attached to the viewer's CAPS seed lifecycle. The concrete implementation is `OpenSim/Region/CoreModules/Framework/Caps/CapabilitiesModule.cs`; feature modules subscribe to `Scene.EventManager.OnRegisterCaps` and register per-agent handlers on the supplied `Caps` object.

The `AvatarInteraction` feature is present on `opensim-next/avatar-modernization`. It was not present on `opensim-next-dev`; keep the server and viewer integration branches aligned when building a test grid.

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


## Viewer invite UI implementation update (2026-11-24)

The first outbound Viewer path has now been added on `opensim-next/avatar-interaction-actions`:

- `indra/newview/llavataractions.h/.cpp`: `LLAvatarActions::requestInteractionInvite(target, interaction)` sends the verified LLSD fields to the `AvatarInteraction` capability and rejects self-targets, null targets, unsupported action keys, or unavailable capabilities.
- `indra/newview/llviewermenu.cpp`: adds a context-menu listener using the selected Avatar and a capability-aware enable check.
- `indra/newview/skins/default/xui/en/menu_avatar_other.xml`: adds an **Interact** submenu with Handshake, High-five, and Shoulder tap.

This is an outbound invitation UI only. It does not yet handle the HTTP response in the UI, offer a local pending state, play animations, or create a paired session. Incoming invitation acceptance/rejection remains wired through the existing generic-message callback.

**Verification status:** source edits are committed to the feature branch, but no C++ build, XUI load test, simulator runtime test, or end-to-end invitation test has been run. In particular, the menu listener and XML integration need compile/runtime verification before this is called complete.


## Viewer consent controls and event feedback (2026-11-24)

Additional Viewer changes committed after the first invite menu:

- `LLFloaterGesture` now cycles through all four server-supported policies: `off → friends → ask → everyone → off`.
- The policy button displays the current local selection (`Interactions: Off/Friends/Ask/Everyone`). The server still applies the authoritative policy; the Viewer does not locally grant permission.
- Incoming invitation text now renders readable interaction names (handshake, high-five, shoulder tap).
- Accepted/rejected/cancelled/expired event statuses now produce distinct human-readable messages, including the interaction type.
- The other-avatar pie menu now uses direct native menu items for the three interactions rather than nesting an unsupported `context_menu` element.

The consent policy is per-presence and resets with the server-side presence. The Viewer currently tracks the selection in the floater instance; no policy GET/persistence endpoint exists yet, so do not assume a value can be recovered after a viewer restart.

**Remaining gaps:** outgoing POST response/error feedback; invitation cancellation from the requester UI (requires tracking the server-returned `request_id`); timeout feedback when the server lazily prunes an invitation; and paired animation/session lifecycle.

**Verification status:** source changes and XML edits are committed. No C++ build, XUI runtime load, or simulator integration test has been performed.


## Server session lifecycle and Viewer session bridge (2026-11-24)

This section supersedes the earlier statements above that the server does not create sessions or that accepted events lack a session identifier.

The server branch `danggiatri6-spec/opensim` / `opensim-next/avatar-modernization` now implements:

- A new UUID `session_id` on acceptance, distinct from the invitation `request_id`.
- Session records containing requester, target, interaction kind, request ID, and a 120-second expiry.
- A maximum of 128 active sessions per region and one active session per participant.
- `AvatarInteractionAccepted` events to both participants, carrying `session_id` and the shared participant/action metadata.
- `action=end` with `session_id`; only either participant may end the session.
- `AvatarInteractionEnded` events to both participants on explicit end or when an expired session is pruned.
- Session-table cleanup when the region module is removed.

The Viewer feature branch now records the accepted `session_id`, clears only the matching ID when an end/expiry event arrives, and exposes `LLAvatarActions::endAvatarInteraction(session_id)` to POST the explicit end action. The Viewer stores one active session ID, matching the server's one-session-per-participant rule.

**Important remaining limitations:** expiry pruning is lazy and occurs on a later session operation; if no subsequent request reaches the region, the timeout event is delayed. The Viewer helper is not yet exposed as a visible End Interaction control. No paired animation or interaction-owned gaze target is started yet. There is no role-specific animation metadata in the server event.

**Verification status:** source-level checks confirm the new server session fields, participant authorization, explicit end event delivery, region cleanup, Viewer session capture/clear, and the end request payload. Neither C# nor C++ has been compiled, and no simulator or two-viewer runtime test has been performed.


## Viewer End Interaction context action (2026-11-24)

The `opensim-next/avatar-interaction-actions` Viewer branch now exposes the existing server `action=end` request in the right-click menu for the active interaction partner:

- `LLAvatarActions::getActiveAvatarInteractionSession()` exposes the locally recorded server-issued session UUID.
- `LLAvatarEndInteraction` posts the end request only when the selected avatar matches the recorded partner and the session UUID is non-null.
- `Avatar.CanEndInteraction` enables the menu item only for that same partner; unrelated avatars do not get an active End Interaction command.
- The Viewer does not clear local session/gaze state on click. It continues to wait for the server's `AvatarInteractionEnded` event, so UI state remains server-authoritative.

No new protocol fields or animation behavior were introduced. Source changes are committed below; no C++ build, XUI runtime load, or simulator test has been run.


## Viewer animation API and paired-asset audit (2026-11-24)

The Viewer already exposes the correct local animation entry points through `LLVOAvatar` / `LLCharacter`:

- `LLVOAvatar::startMotion(const LLUUID&, F32 time_offset = 0.f)` starts a registered/fetchable motion on that avatar instance.
- `LLVOAvatar::stopMotion(const LLUUID&, bool stop_immediate = false)` requests its ease-out/stop path.
- The underlying `LLCharacter::getMotionController()` exposes the motion controller, but interaction code should use the avatar's public start/stop methods rather than manipulating controller internals.
- Each Viewer has local avatar objects for both participants. A future paired controller can therefore apply the requester-role motion to one avatar and the target-role motion to the other in each Viewer, keyed by the same server `session_id`; this is local rendering, not a new simulator animation broadcast.

The standard animation registry in `indra/llcharacter/llanimationstates.cpp` was inspected. It contains general gestures and poses such as `hello`, `bow`, `clap`, `salute`, `blowkiss`, and `express_kiss`, but no standard paired `handshake`, `high_five`, or `shoulder_tap` animation constants or role-specific pairs. These general gestures are not valid substitutes for synchronized two-person contact animations.

**Implementation decision:** do not invent animation UUIDs or start arbitrary standard gestures as if they were paired interactions. The next animation milestone needs a verified two-role asset pair per interaction (asset UUIDs, duration, loop/ease behavior, role assignment, and optional root/hand alignment metadata). The controller should start both roles only after both assets are available, track the exact motions it owns per session, and stop only those motions when the matching server end event arrives. If an asset is unavailable, the session and gaze interaction must still work without falsely claiming an animation played.

**Verification:** inspected the Viewer headers/implementation and standard animation registry on this branch. This was source inspection only; no files outside this document were changed in this audit, and no build or runtime test was performed.

## Viewer paired-motion lifecycle hook (2026-11-24)

The generic AvatarInteraction event handler now recognizes optional `requester_animation` and `target_animation` UUID fields on `AvatarInteractionAccepted`. When both UUIDs are present and both participant avatar objects are available, it starts both role-specific motions as one operation. If either side cannot start, any motion started by the other side is immediately stopped, avoiding a one-sided contact pose.

Successful paired starts are tracked by server `session_id`, including both participant UUIDs and the exact motion UUIDs. A matching `AvatarInteractionEnded` or session-expiry event stops only those tracked motions. Unrelated AO, dance, gesture, and user-started motions are not stopped by this feature. Duplicate accepted events are ignored while that session is tracked.

The current OpenSim server event does not yet supply these optional animation UUIDs, and the standard Viewer animation registry has no verified paired handshake/high-five/shoulder-tap assets. Therefore this is a forward-compatible playback/lifecycle hook, not a claim that the three actions currently animate. The next server-side step is to provide configured, verified role-specific asset UUIDs in the accepted event. No C++ build or runtime test has been performed.


## Bundled handshake asset wired to the accepted event (2026-11-24)

The OpenSim server branch now adds both optional motion fields to the accepted-event body for `handshake` only:

- `requester_animation` = `ddc2400f-ecdb-b00e-aee7-442ff99d5fb7`
- `target_animation` = `ddc2400f-ecdb-b00e-aee7-442ff99d5fb7`

This UUID is not guessed: it is the bundled OpenSim `Handshake` asset in `bin/assets/AnimationsAssetSet/index.xml`, also present as an inventory-library item named `handshake`. Its inventory description identifies it as a simple right-handed, double-pump handshake. The asset animates one avatar's right arm/upper-body joints; the same asset is therefore started on both participant avatar objects by the existing all-or-nothing Viewer hook. Participants should face one another before accepting. The server does not yet auto-align their positions or rotations.

No animation fields are emitted for `high_five` or `shoulder_tap`; those actions continue to use the consent/session/gaze flow without a fabricated animation. The separate `give_and_handshake` asset is not used as a counterpart because its inventory description describes a different one-avatar hand sequence, not a verified partner role.

**Verification status:** source and inventory metadata were inspected and the server change was committed. No C# build, C++ build, simulator runtime test, asset-fetch test, or two-viewer visual test has been performed. In particular, successful simultaneous playback and visual hand contact still require runtime verification.


## Consent-scoped face-to-face heading action (2026-11-24)

The right-click menu for the currently active interaction partner now exposes `Face Interaction Partner`. It is enabled only when the selected avatar matches the active server-issued partner/session and is within Firestorm's existing 20 m facing range.

The action reuses `FSAvatarAlignBase::faceAvatar()` rather than introducing another rotation implementation. If the alignment floater is not already instantiated, the action opens the configured full/mini alignment floater and then invokes its existing face-avatar routine. That routine rotates the local user's agent frame toward the partner and sends the normal agent update. It does not translate either participant, does not issue a teleport/autopilot movement, and does not force the remote avatar to rotate. The partner can independently choose the same action from their own Viewer.

The existing alignment helper locally snaps the remote avatar render root back to its server-reported transform after changing the local heading; this is a visual correction, not a remote transform command. The action is limited to the active interaction partner and cannot be used from this menu against unrelated avatars.

**Verification status:** source checks confirm active-session/partner matching, 20 m range gating, existing alignment helper reuse, and menu registration. No C++ build or in-world two-viewer test has been run. The animation itself still requires runtime verification for hand contact and avatar orientation.


## Invitation menu range gating (2026-11-24)

The Viewer now disables the three invitation menu actions unless the selected avatar is within 3 m of the local avatar, in addition to the existing checks for a live non-self avatar, visible name under RLVa, and an available `AvatarInteraction` capability. This matches the server-side interaction range and avoids presenting an action that would predictably fail for a distant avatar. The server remains authoritative and rechecks presence, policy, and range when the invitation is submitted and accepted.

**Verification status:** the menu predicate was updated and committed; source-level inspection only. No C++ build or simulator test has been performed.


## Local-only approach movement research (2026-11-24)

Firestorm already exposes `LLAgent::startAutoPilotGlobal()`, `stopAutoPilot()`, and a completion callback. The API can optionally set a target rotation and has an `allow_flying` parameter (which defaults to true). This is sufficient infrastructure for a future explicit local approach action; a new movement system is not required.

The interaction feature must not call this automatically on `AvatarInteractionAccepted`. Acceptance authorizes the named interaction, not relocation. A future `Approach Interaction Partner` action must be explicitly invoked by the local user, move only the local avatar to a safe stand-off point, pass `allow_flying=false`, and never send a transform or movement command for the remote participant. It must also stop its own autopilot on user cancellation, session end/expiry, partner disappearance, or region change. The current 3 m invitation radius means this action should be optional convenience rather than a prerequisite for handshake.

The API was inspected in `llagent.h` and `llagent.cpp`; no movement behavior has been implemented or tested in this change.


## Explicit local approach action (2026-11-24)

The Viewer now exposes `Approach Interaction Partner` in the selected active partner's context menu. It is available only for the active server-issued session, a live selected partner, a local avatar that is not already using autopilot or flying, and a partner 1.6–20 m away.

When explicitly invoked, the local Viewer computes a horizontal stand-off point 1.25 m from the partner and starts Firestorm's existing `LLAgent::startAutoPilotGlobal()` for the local avatar only. It requests no flying, stops at a 0.35 m arrival tolerance, and supplies a facing rotation so the local avatar faces the partner on arrival. It does not send a transform or movement command for the remote avatar and does not change the server session. It is optional; the server's invite range remains 3 m.

The action refuses to take over an existing autopilot or start while the local avatar is flying. The feature records ownership by the dedicated autopilot behavior name `AvatarInteractionApproach`. Session end clears the active session through `clearActiveAvatarInteraction()`, which stops only this owned autopilot. The camera update also cancels this approach if the partner disappears or moves beyond 20 m. If the user replaces the autopilot with another movement action, cleanup checks the behavior name and leaves that unrelated action untouched.

**Verification status:** the latest Viewer source, declarations, menu registration, XUI wiring, session-end cleanup, and missing-partner cleanup were re-fetched and passed 15/15 targeted source checks. No C++ build or in-world two-viewer test has been run. Pathfinding, collision avoidance, parcel restrictions, and actual arrival/facing behavior remain runtime tests.


## Paired-motion late-load retry (2026-11-24)

The Viewer no longer drops a server-authorized paired animation merely because one participant avatar object is not yet present or an animation asset cannot start on the first frame. Accepted-event handling stores the two server-provided motion UUIDs in a pending queue and attempts an all-or-nothing start. The per-frame Viewer camera update retries while both avatar objects and their motion assets become available, for up to 10 seconds.

If one role starts and the other does not, the first role is immediately stopped before retrying; a one-sided contact pose is never intentionally left running. A successful start moves the exact session and motion IDs into the existing active-motion table. A matching end/expiry event removes pending work and stops only successfully tracked motions. A superseding interaction session also invalidates stale pending work.

This retry is local Viewer scheduling only. It does not alter the server session contract, move either participant, or fabricate animation UUIDs. If either animation cannot be started within the retry window, the interaction session, gaze, and consent state remain intact without claiming that the animation played.

**Verification status:** the three changed Viewer files were re-fetched from the branch and passed 10/10 targeted source checks covering the queue, both-avatar gate, partial-start rollback, 10-second timeout, session guard, end cleanup, public update declaration, and per-frame invocation. No C++ build or in-world two-viewer test has been run.
