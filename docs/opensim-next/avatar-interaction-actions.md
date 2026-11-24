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
