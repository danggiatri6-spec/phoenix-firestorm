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


## Paired-motion distance guard (2026-11-24)

The paired-motion start path now checks the current 3D separation of the requester and target Avatar objects before starting either role. The bundled handshake motion is allowed to start only when the participants are between 0.45 m and 2.0 m apart. If they are outside that range, the existing pending queue keeps the animation request alive and retries for the remainder of its 10-second window.

This prevents an accepted invitation from triggering a visibly disconnected handshake after either participant has moved away. The Viewer does not reposition either participant and does not weaken the server's consent, presence, or invitation-range checks. A user may still use the explicit local approach action to close the distance.

**Verification status:** the source edit was committed. This is a source-level change only; no C++ build, animation asset playback test, or two-viewer in-world test has been run. The distance thresholds still need visual tuning with different avatar shapes and viewer camera conditions.


## Face-to-face orientation gate for paired contact motions (2026-11-24)

The paired-motion start path now also checks both Avatar forward directions before starting a contact animation. Firestorm's existing avatar alignment implementation constructs the avatar's forward basis from its local X axis; the gate therefore rotates the local X axis by each Avatar's current rotation, projects both directions onto the horizontal plane, and requires each participant to face the other within 60 degrees (dot product >= 0.5).

If either participant is turned away, the existing pending request waits and retries within its 10-second lifetime. The Viewer does not rotate or reposition either participant. This is a conservative visual-quality gate, not a guarantee of hand-to-hand joint contact; the bundled upstream handshake remains a single-Avatar animation played on both participants.

**Verification status:** source-level checks only. No C++ build or two-viewer runtime test has been run. The forward-axis assumption is grounded in FSAvatarAlignBase::snapAvatarBody() and still needs in-world verification against avatar root rotations, especially for seated, custom-rigged, and nonstandard avatars.


## Affection interaction types — first protocol increment (2026-11-24)

The Viewer and simulator invitation allowlists now include four consent-gated interaction keys:

- `hug`
- `kiss`
- `holding_hands`
- `affection` (a general close-companionship pose category)

The existing capability invitation flow, server-side policy checks, request expiry, session creation, participant authorization, and end/timeout notifications are reused. The other-avatar context menu exposes the four additional invitation choices, and incoming/status notifications use human-readable labels.

**Important implementation boundary:** this increment adds protocol and UI support only. It does not claim these actions already have paired animation assets. The official OpenSim animation asset index currently exposes a solo `blowkiss` animation, not a synchronized two-avatar kiss; no verified paired hug, kiss, hand-holding, or affection-pose animation pair has been selected. The existing Viewer paired-motion handler only starts animations when the server supplies explicit role-specific animation UUIDs, so these new interaction sessions currently remain animation-neutral rather than playing an incorrect solo animation on both avatars.

Next implementation work is to source or create licensed role-specific animation pairs, then add per-action alignment metadata (relative position, facing, height offsets, and allowed seated/standing states) to the server event contract. No C# or C++ build, simulator test, or two-viewer runtime test has been run for this increment.


## Voice lip-sync stale-pose cleanup (2026-11-24)

Viewer branch: `opensim-next/avatar-interaction-actions`

Reviewed the existing voice pipeline before adding a parallel facial system. Firestorm already has an amplitude-driven Ooh/Aah lip-sync path through `LLVoiceVisualizer::lipSyncOohAah()` and `LLVOAvatar::idleUpdateLipSync()`, with fallback morphs for rigs that lack the dedicated lip-sync morphs.

Fixed a stale-mouth-pose edge case in `LLVOAvatar::idleUpdateLipSync()`:

- If voice becomes unavailable/disabled, or the LipSyncEnabled preference is switched off while the avatar is speaking, an already active lip-sync pose is reset to each morph's minimum weight.
- Visual parameters are updated and the avatar mesh dirtied after cleanup.
- Existing voice amplitude sampling, Ooh/Aah curves, fallback morphs, and legacy gesture/animation behavior are unchanged.

This is a correctness fix to the existing amplitude-based two-morph lip-sync path, not phoneme/viseme recognition. It does not infer speech content and does not add new network traffic. Static source review only; no C++ build or live voice test has been performed.



## Facial expression and emotion compatibility research (2026-11-24)

Inspected the existing Firestorm expression, gesture, animation-state, and Visual Parameter paths before introducing any new emotion controller.

### Existing compatible path

- `LLVOAvatar::initInstance()` registers the standard `ANIM_AGENT_EXPRESS_*` animation IDs with `LLEmote::create`. The registered set includes afraid, anger, bored, cry, disdain, embarrassed, frown, kiss, laugh, open-mouth, repulsed, sad, shrug, smile, surprise, tongue-out, tooth-smile, wink, and worry.
- `llanimationstates.cpp` maps those same standard IDs to the established Second Life animation state names. These are not new OpenSim-next animation IDs.
- `LLEmote` drives the face through the avatar's named Visual Parameters and cross-fades against `Express_Closed_Mouth`. This is the compatibility path that existing animation signals and gestures already use.
- `LLGestureMgr` sends start/stop animation requests for the animation IDs referenced by a gesture. It is therefore an existing user-authored trigger path, not a suitable place to synthesize AI emotion or silently replace gesture semantics.
- The voice lip-sync path is separate: it drives Ooh/Aah morphs from voice amplitude and now clears stale morph weights when voice/lip-sync is disabled.

### Design constraints for the next implementation

1. Do not write facial joints or face morph weights from a second competing per-frame controller. Let the registered `LLEmote` motions remain the owner of standard expression morphs.
2. Do not invent new animation UUIDs or change the standard expression UUID/name mappings. That would break existing LSL, gesture, AO, and inventory compatibility.
3. Emotion selection should be a semantic intent layer (for example, friendly greeting, attentive listening, amusement, concern, sadness), mapped onto existing standard expression IDs only after priority and ownership rules are explicit.
4. The intent layer must distinguish an expression it started from one already active due to LSL, a user gesture, or another animation source. It must never stop or overwrite an expression it does not own.
5. Expression transitions need bounded duration, fade/return behavior, and suppression rules for explicit user/LSL gestures. Lip-sync must remain independent so speaking does not cancel the emotional expression or leave the mouth morph stuck.
6. A future AI/chat-driven expression may be local-only as a presentation enhancement, but any expression intended to be visible to other users must use the normal animation signaling path and respect the simulator's existing animation permissions and propagation.

### Verification status

Source inspection covered `llemote.cpp/.h`, `llgesturemgr.cpp/.h`, `llanimationstates.cpp/.h`, `llvoavatar.cpp`, `llvoavatarself.cpp`, and `llmotioncontroller.cpp`. This is a research/design checkpoint; no expression controller or new runtime behavior was added in this step. No C++ build or in-world gesture/LSL conflict test has been run.


### Animation ownership finding

The local Avatar path exposes source-aware cleanup for attachment/object animation sources (`LLVOAvatarSelf::hasMotionFromSource()` and `stopMotionFromSource()`), while standard face-expression motions are registered and activated by the avatar's signaled animation state. Gesture stop sends an animation STOP for the gesture's animation UUID; it does not provide a distinct ownership token for a particular expression instance. Consequently, an emotion layer must not issue a STOP for a standard expression UUID merely because its own timer expired: the same UUID may have been activated by a user gesture, LSL, or another source. The next implementation should first introduce a source/lease-aware arbitration boundary, or limit itself to observing and recommending expression intents without taking over existing animation state.


## Expression lease ownership design (2026-11-24)

A further inspection of `process_avatar_animation()` in `llviewermessage.cpp` found a practical ownership signal for a future local emotion-expression controller:

- The simulator sends the complete current animation list for an avatar; the Viewer clears and rebuilds `mSignaledAnimations` for each update.
- Each animation entry includes an `AnimSequenceID`, stored as the value in `LLVOAvatar::mSignaledAnimations[animation_id]`.
- For the local avatar, the same update may include `AnimationSourceList` entries. The Viewer records attachment/object-originated animations in `mAnimationSources`.
- Gesture playback uses normal animation start/stop requests; it does not carry a unique gesture ownership token into the face-expression motion.

### Proposed lease protocol

1. The emotion controller may request a standard expression only when that expression is not already signaled and is not currently attributed to an attachment/object animation source.
2. It marks the expression request as pending, then waits for the next authoritative animation-state update rather than assuming the request succeeded.
3. Once the expression appears, it records the server-provided sequence ID as its lease generation.
4. Before expiring the expression, it checks that the same expression is still signaled with the same sequence ID and has no external animation source. If any condition differs, it relinquishes control without sending STOP.
5. If a competing gesture/LSL source reasserts the same expression and the sequence ID changes, the emotion controller must not stop it.
6. If the expression was already active before the controller requested it, the controller never claims ownership and never stops it.
7. Session end, logout, region change, and controller disable clear pending leases; cleanup sends STOP only for a still-valid lease.

This is safer than a timer-only approach because the sequence ID is supplied by the simulator's authoritative animation state. It is still not a perfect per-origin token: two same-ID requests can race before the next state update, and the protocol does not expose a unique owner for every agent-originated gesture. The controller must fail closed (drop ownership) whenever it cannot prove its lease is still current.

### Implementation boundary

The next code increment should implement the lease state machine and hook it into the existing animation-state update path. It should not alter `LLEmote`, standard expression UUIDs, Gesture Manager behavior, LSL semantics, or face Visual Parameters directly. Expression choices should map only to existing standard `ANIM_AGENT_EXPRESS_*` IDs. This remains a local Viewer enhancement using normal animation requests, not a new simulator protocol.

**Verification status:** source inspection only; no controller code, C++ build, gesture/LSL race test, or in-world runtime test has been added in this checkpoint.


## Sequence-aware social expression lease: first Viewer implementation (2026-11-24)

Implemented the first local Viewer-side lease primitive in `llagentcamera.h/.cpp`.

### API and behavior

- `LLAgentCamera::requestSocialExpression(animation_id, duration)` accepts only the existing standard `ANIM_AGENT_EXPRESS_*` IDs; duration is clamped to 0.5–12 seconds.
- It refuses to claim an expression that is already present in the local avatar's `mSignaledAnimations`, refuses object/attachment-sourced instances, and allows only one pending/owned social expression at a time.
- It sends the normal `AgentAnimation START` request and waits for the next authoritative local avatar animation-state update.
- When the requested expression appears, it records the server-provided Animation Sequence ID as the lease generation.
- The per-frame Viewer update releases the lease after its duration only if the same animation is still signaled with the same sequence ID and no object/attachment animation source is recorded.
- If the animation is not confirmed within 3 seconds, disappears, changes sequence, or gains an external object source, the controller fails closed and relinquishes ownership without sending STOP.
- `cancelSocialExpression()` performs the same sequence/source check before sending STOP for an owned lease. Pending requests are simply relinquished; no speculative STOP is sent.
- No direct face morph or joint writes were added. `LLEmote` remains responsible for rendering the standard expression and lip-sync remains independent.

### Important limitation

The simulator's sequence ID is an animation-generation signal, not a cryptographic per-origin ownership token. Gesture/LSL and Viewer requests can race before the next authoritative state update; a same-expression request that wins that race cannot always be distinguished from the Viewer request. The implementation therefore blocks pre-existing expressions, tracks sequence changes, checks object/attachment sources, and fails closed when ownership evidence is lost, but it does not claim perfect per-source arbitration. A future simulator-side source token would be needed for that guarantee.

### Verification status

Static source checks confirmed: all 19 standard expression IDs are whitelisted; non-expression IDs are rejected; pre-existing signals and object sources block acquisition; request confirmation captures sequence ID; timeout/mismatch/source conflict relinquish without STOP; expiry and explicit cancel re-check the lease; no direct Visual Parameter writes were introduced. No C++ build, simulator test, gesture/LSL race test, or in-world test has been run. This is an API primitive only; no automatic emotion-to-expression policy calls it yet.


## Semantic social-emotion request API (2026-11-24)

Added `LLAgentCamera::ESocialEmotion` and `requestSocialEmotion()` as a small semantic layer over the sequence-aware expression lease:

| Intent | Existing SL expression | Default duration |
|---|---|---:|
| `GREETING` | `EXPRESS_SMILE` | 2.8 s |
| `FRIENDLY` | `EXPRESS_TOOTHSMILE` | 2.5 s |
| `AMUSED` | `EXPRESS_LAUGH` | 2.0 s |
| `CONCERNED` | `EXPRESS_WORRY` | 3.2 s |
| `SAD` | `EXPRESS_SAD` | 3.5 s |
| `SURPRISED` | `EXPRESS_SURPRISE` | 1.8 s |

The semantic method delegates to `requestSocialExpression()`, so the same pre-existing-animation guard, source check, server confirmation, sequence-aware release, and fail-closed behavior apply. It does not parse chat, infer emotion from text/voice, or automatically trigger expressions. That separation is intentional: first expose a stable intent API; later connect it to interaction events or a configurable conversation policy without treating every message as an emotion cue.

Static verification only; no C++ build or live avatar test has been run.


## Interaction-scoped emotion reactions (2026-11-24)

Connected the semantic emotion API to two narrow Viewer events:

- When the server confirms an interaction session, the local avatar requests a brief greeting smile.
- For incoming nearby agent chat, the Viewer may react only when the speaker is the currently accepted interaction partner, the partner is still a live avatar in the same region and within 12m, the local user is not in DND, and the message has passed the existing mute, anti-spam, and RLVa receive-chat filtering path.
- Recognized cues are deliberately small and deterministic: laughter tokens/Chinese laughter -> amused; thanks -> friendly; apology/sadness -> concerned; "wow"/surprise -> surprised. Ordinary messages do not trigger an expression.
- An eight-second cooldown prevents rapid expression changes. The existing single-expression lease also prevents overlapping requests and relinquishes when another animation source takes over.
- Reactions are local Viewer behavior only; no chat text is sent to the server, no extra network messages are introduced, and no remote avatar is animated by this code.

This is a prototype cue mapper, not natural-language understanding. It does not infer emotion from arbitrary sentences, voice tone, sarcasm, or context. Users can still use existing gestures and LSL animations. No C++ build or in-world two-viewer test has been run.


### Social emotion reaction preference (2026-11-24)

Added the persistent Boolean setting `FSAvatarSocialEmotions` (default enabled). The preference is checked centrally by `requestSocialEmotion()`, so both the interaction-accepted greeting and incoming-chat emotion reactions are suppressed consistently when disabled. The low-level `requestSocialExpression()` lease API remains available for explicit internal callers and is not coupled to this social-reaction preference. No new network messages or server behavior are involved. Static source review only; no build or runtime test.


Follow-up hardening: the `FSAvatarSocialEmotions` preference is checked centrally in `requestSocialEmotion()`, covering both greeting and chat-triggered semantic reactions. English cue tokens now require ASCII word boundaries, reducing accidental matches inside unrelated words (for example, "lol" inside a larger word); Chinese cue matching remains explicit. Targeted static source checks passed 11/11. This is not a compile or runtime test.


### Facial expression and gaze coordination (2026-11-24)

Before mapping a semantic social emotion to its standard SL facial-expression animation, `requestSocialEmotion()` now refreshes the local interaction gaze target to the current accepted partner when that partner is a live avatar in the same region and within 12m. This deliberately calls the existing `setInteractionLookAt()` / `LLHUDEffectLookAt` overlay rather than writing eye or head joints. Existing attention priority arbitration, Mouselook suppression, the `PrivateLocalLookAtTarget` preference, and legacy AO/LSL/camera behavior remain in control. This creates a coordinated attention cue for greeting and recognized chat reactions without introducing new animation IDs or network traffic.

Targeted static source checks passed 10/10. No C++ build, simulator test, or in-world facial/gaze observation has been performed.


### Accepted-session greeting wave (2026-11-24)

The accepted-session event now requests the established Second Life `ANIM_AGENT_HELLO` animation state as a short local-avatar greeting wave plus a brief head nod, alongside the existing smile and partner gaze. The Viewer refuses to request it when the same animation is already signaled or attributed to an attachment/object source. It waits for the simulator's animation-state update, records the observed sequence ID, and releases only that same generation after approximately 2.8 seconds. If the animation is replaced, sourced externally, or never confirmed, the lease relinquishes without sending STOP. Ending/expiring the interaction cancels the local greeting lease safely.

These are single-avatar greeting cues, not paired animations. The nod reuses the official OpenSim/SL `YES` (`yes_head`) animation asset; both motions use the standard SL animation request path so normal simulator distribution and existing AO/gesture behavior remain in charge. Wave and nod have independent sequence-aware leases. The preference `FSAvatarSocialEmotions` also gates this greeting gesture. Static source review only; no C++ build, simulator test, or two-viewer animation observation has been performed.


### Conversational affirmation nod (2026-11-25)

The accepted interaction's short head nod is now also available as a restrained conversational response. When the accepted partner sends a nearby, filtered chat message containing an explicit affirmative cue (`yes`, `yeah`, `yep`, `agree`, `absolutely`) or thanks (`thank`, `谢谢`, `多谢`), and the existing semantic reaction is accepted, the local avatar requests the standard `ANIM_AGENT_YES` / `yes_head` nod. The existing cooldown, active-animation guard, source guard, and sequence-aware lease prevent repeated or competing requests. Ordinary messages do not trigger a nod. This is deterministic keyword matching, not conversational understanding; no chat text is transmitted beyond the normal chat path and no remote avatar is animated.

Static source checks only. No C++ build, live chat test, or in-world animation blend test has been run.


### Voice-activity-driven speaking gesture (2026-11-25)

Added a local Viewer controller for the established Second Life `ANIM_AGENT_TALK` animation (`talk`, UUID `5c682a95-6da4-a463-0bf6-0f5b7be129d1`). It observes the local voice client's `getIsSpeaking(gAgent.getID())` state and requests TALK only while the local user is actively speaking. Typed chat does not trigger it; the Viewer uses the normal standard animation start/stop path.

The controller is conservative: it does not acquire TALK if it is already signaled or attributed to an attachment/object source; it waits for simulator animation-state confirmation, records the observed sequence ID, and stops only the same generation when voice activity ends. If the animation disappears, changes generation, or an external source takes over, it relinquishes without STOP. Existing voice-amplitude Ooh/Aah lip-sync remains separate and unchanged.

This is an initial voice-activity gesture, not a full gesture vocabulary or prosody-aware system. It has not been compiled or tested with live voice, AO, or multiple viewers. The standard TALK asset's visual suitability and blending with custom Bento rigs still require in-world verification.


### Speaking gesture continuity refinement (2026-11-25)

The local TALK controller now keeps the animation alive for a short 0.65-second grace period after voice activity drops. This is intended to absorb brief voice-activity detection/network gaps between syllables and prevent repeated start/stop flicker. The grace period applies only to a TALK lease acquired by this controller; it does not extend an AO/manual animation. The lease is reset when the local avatar is unavailable or the controller releases its animation.


### Body-gesture arbitration research (2026-11-25)

Inspected `LLMotionController`, `LLMotion`, and `LLKeyframeMotion`. The Viewer exposes the active-motion list, and motions expose a base priority; keyframe assets additionally carry per-joint priorities and ease-in/ease-out durations. The pose blender resolves joint-level priority while applying motion output.

This means a single animation-level priority check is not sufficient to decide whether a social arm/torso gesture is safe: two motions can affect different joints, while two apparently compatible motions can compete on the same joint at different priorities. The active-motion list alone does not identify whether a motion is user/AO/attachment-owned in a way that permits safely stopping it. Gesture Manager also owns user inventory gestures and their multi-step execution; social automation must not take over that pathway.

Decision: do not stack `point_you`, `shrug`, or other full-body gestures over `TALK` based only on animation names. Before adding automatic upper-body gestures, implement a non-invasive arbitration layer that observes loaded motion joint coverage/priorities and relinquishes on conflicts, without stopping or rewriting AO, LSL, inventory gestures, or user-started animations. Preserve standard SL animation IDs and existing gesture semantics.


### Conservative upper-body motion conflict gate (2026-11-25)

The local voice-driven TALK controller now inspects the local avatar's active motion poses before acquiring TALK. It scans active motions' joint states and treats medium-or-higher-priority motion affecting shoulder, collar, elbow, wrist, hand, chest, torso, or spine joints as a conflict. Joint-state priority is used when specified; otherwise the motion's base priority is used. Existing TALK itself is excluded.

When a conflict is present, the social layer simply does not acquire TALK; it does not stop, reprioritize, rewrite, or fade any existing motion. This is deliberately conservative and read-only. It protects many AO, LSL, and user gesture overlaps, but does not yet prove source ownership for every local motion or provide per-joint blending guarantees. The check may suppress social TALK during legitimate upper-body idle motions; this requires live testing and tuning. No C++ build or runtime test has been run.


The conflict gate is also re-evaluated while the social TALK lease is active. If a conflicting upper-body motion begins mid-speech, the speech grace period is overridden and the controller releases its own TALK lease through the existing sequence-aware stop guard. Once the conflict clears, ongoing voice activity may acquire TALK again. Other motions are never stopped or modified.


### First body-region compatibility policy (2026-11-25)

Added a read-only body-region classifier over active motion pose joint states. It labels medium-or-higher-priority joints as face, head/neck, arms, hands, torso, legs, or unknown. Joint-level priority is preferred; motion base priority is the fallback. Unrecognized joint names are classified as unknown rather than silently assumed safe.

The first compatibility rule is intentionally narrow: automatic TALK conflicts with arms, hands, torso, or unknown regions; face-only, head/neck-only, and leg-only motion may coexist. Unknown future social motion kinds fail closed. This is an initial policy layer, not a complete coordinator: it does not yet arbitrate facial expression leases, nod/wave leases, paired motions, or motion transitions as one shared queue. The joint-name classifier needs validation against standard and custom avatar rigs. No C++ build or runtime tests have been run.


### Local single-avatar idle posture refinement (2026-11-25)

The current scope is restricted to the local quality of an individual avatar's expression and posture. No two-avatar interaction behavior, interaction capability, partner gaze, paired animation, or server protocol is changed in this step.

Extended the existing `LLHeadRotMotion` with a very low-amplitude procedural idle head drift when there is no explicit `LookAtPoint`. The motion changes its yaw/pitch target at randomized 2.5–5.5 second intervals, stays within approximately 1.5 degrees yaw and 0.7 degrees pitch, and eases toward each target with a 0.8-second half-life. Any explicit look target immediately drives the idle target back to neutral and suppresses the drift, preserving normal gaze behavior. The existing head/neck/torso pose blender remains responsible for applying the result; no joint is written outside the motion system and no network animation is sent.

This is intentionally a subtle first pass, not a breathing or full-body idle system. It must be visually checked with standard avatars, Bento/custom rigs, AO, LSL head animations, mouselook, and camera attention. Static source inspection only; no C++ build or runtime test has been performed.


### Natural blink cadence refinement (2026-11-25)

Refined only the local eye-motion blink scheduler. Ordinary blink intervals now vary between 2.2 and 6.8 seconds, with a 12% chance of a short 0.55–0.95 second follow-up blink to create occasional double blinks. The first blink still uses the existing 1.5–4.5 second settling delay, and the existing eyelid easing, left/right offset, eye look-away behavior, and deactivation cleanup are retained. This changes no avatar-to-avatar behavior, animation protocol, AO/LSL requests, or server code. Static source checks only; no C++ build or in-world visual test has been run.


## Legacy lip-sync / facial-expression arbitration

- `LLVOAvatar` now records whether either dedicated `Lipsync_Ooh` / `Lipsync_Aah` morph is missing and the legacy `Express_Kiss` / `Express_Open_Mouth` fallback is therefore in use.
- While any standard facial-expression animation is signaled, `idleUpdateLipSync()` does not write the shared fallback morphs. This prevents the per-frame voice update (including its release path) from overwriting an active `LLEmote` expression.
- The lip-sync timer is reset during this guard so resuming lip-sync does not inherit a stale elapsed interval.
- Avatars with both dedicated lip-sync morphs retain the existing amplitude-driven lip-sync path unchanged.
- This is a conservative compatibility behavior: legacy avatars may temporarily show the facial expression without moving lip-sync mouth shapes while that expression is active. It avoids morph corruption but does not provide additive mouth composition. Runtime appearance and compilation remain unverified.


## Facial expression teardown safety

- `LLEmote::onDeactivate()` now checks whether its cached `LLCharacter` is still valid before accessing facial visual parameters. Avatar teardown can precede motion-controller cleanup, so the previous unconditional dereference could access a destroyed character.
- When the character is already unavailable, the cached visual-parameter pointer is cleared and cleanup returns without touching avatar state.
- On the normal path, the expression morph and `Express_Closed_Mouth` are restored as before, visual parameters are updated, and the cached morph pointer is cleared.
- Standard expression ease-in/ease-out timing remains owned by `LLMotionController`; no extra timing curve or protocol change was introduced.
