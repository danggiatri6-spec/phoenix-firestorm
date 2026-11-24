# OpenSim-next Avatar Modernization

## Product target

The first release target is ordinary desktop PC use. Keyboard, mouse, and microphone are baseline inputs. A webcam is optional; VR controllers and body tracking are later adapters, not requirements.

## Existing Viewer capabilities to preserve and reuse

The Firestorm source already contains:
- `LLVOAvatar` default eye motion and facial visual-parameter support.
- Legacy expression motions registered through `LLEmote` (smile, frown, laugh, surprise, wink, etc.).
- Lip-sync morph parameters `Lipsync_Ooh` and `Lipsync_Aah`, with fallbacks to legacy expression parameters.
- Existing animation playback and network animation state handling.
- `LLPhysicsMotionController` as an existing avatar physics-motion integration point.
- Existing AO and inventory gesture systems.

Do not replace these systems. New controls should call the established motion/gesture APIs and must not bypass animation priority, AO, attachment, or permission behavior.

## Workstreams

### A. Desktop expression and gesture controls
1. Provide a compact expression/action palette with keyboard shortcuts.
2. Use existing expression animation IDs and the normal animation start/stop path.
3. Keep inventory gestures and user-defined gestures available; the palette is a shortcut layer, not a second animation inventory.
4. Add clear stop/reset behavior and prevent expression actions from permanently overriding locomotion or AO.
5. Add per-account enable/disable and shortcut customization.

### B. Face, gaze, and speech
1. Preserve the existing eye motion and lip-sync morph path.
2. Improve deterministic desktop defaults: natural blink timing, subtle gaze targets, and voice-energy-driven mouth movement.
3. Keep microphone analysis local. Never transmit raw microphone samples or webcam frames for avatar animation.
4. Add optional webcam tracking only after a local privacy/consent flow exists.
5. Synchronize only bounded expression/viseme state when a remote avatar needs it; use existing animation transport wherever possible.

### C. Secondary motion
1. Audit current physics motion and rigged attachment support before adding a solver.
2. Add optional spring-chain simulation for explicitly tagged bones only.
3. Run simulation locally with low/medium/high budgets and a hard per-avatar bone/collision cap.
4. Never make per-hair/per-cloth transforms authoritative on the simulator.
5. Provide deterministic fallback to the original rig when assets lack supported metadata.

### D. Avatar-to-avatar interaction
1. Add explicit interaction affordances (handshake, high-five, wave, shoulder tap) for desktop users.
2. Use server-validated interaction requests and consent policy; do not trust client-reported collision as authority.
3. The simulator validates same-region presence, distance, rate limits, and both users' interaction preferences.
4. Initially trigger paired legacy animations; do not implement physical pushing or ragdoll behavior.
5. Support Off, Friends, Ask, and Everyone policies, with Off as the safe default for unsolicited contact.

## Client/server responsibility boundary

Viewer:
- input capture, facial analysis, local animation blending, secondary-motion simulation, rendering;
- local prediction for responsiveness;
- privacy controls and quality budgets.

Simulator:
- presence identity and region membership;
- interaction consent, distance/rate validation, throttling and event relay;
- authoritative shared interaction state only.

The simulator must not run per-frame hair, cloth, gaze, or lip-sync simulation.

## Compatibility and rollout

- Keep legacy SL/OpenSim animation messages and LSL animation behavior unchanged.
- Do not require new asset formats for baseline expressions.
- New capabilities are negotiated and optional; unknown capability data must be ignored safely.
- If a region or viewer does not support the extension, fall back to existing animation/gesture behavior.
- Do not change the existing voice provider or require cloud speech processing.

## Initial implementation order

1. Inspect and reuse the current expression, eye, lip-sync, animation and physics paths.
2. Implement a desktop expression palette using existing animation IDs.
3. Add tests/static checks for animation start/stop, AO coexistence and keyboard focus.
4. Define and implement the opt-in interaction capability on the simulator and Viewer.
5. Add bounded local secondary motion.
6. Add optional webcam tracking behind explicit user consent.

## Acceptance gates

A feature is not complete until it has:
- source-level review and static checks;
- a successful Windows OpenSim-enabled Viewer build when build budget is available;
- server build/tests for simulator changes;
- runtime testing with two accounts on an OpenSim test region;
- regression checks for legacy SL login, AO, gestures, LSL animations and low-end performance.

A source commit or static check is not a build or runtime validation.


## Initial code increment

The Gestures floater now has a desktop quick-expression row for Smile, Laugh, Sad, Frown, Surprise, and Wink, plus Clear. It sends the existing legacy expression animation IDs through the normal Viewer animation request path. It does not add a new wire protocol or replace inventory gestures.

This is source-level work only. It has not yet been compiled or runtime-tested.


## Interaction capability increment

The Gestures floater now includes a desktop toggle that posts `set_policy` (`everyone` / `off`) to the optional `AvatarInteraction` capability when the current region advertises it. Regions without the capability are ignored safely. This is an initial control only: the toggle is local to the open floater session, and the Viewer does not yet expose Friends or Ask settings or a paired-interaction request UI.

Quick-expression controls and the interaction-policy toggle are source changes only; compile and runtime validation remain outstanding.
