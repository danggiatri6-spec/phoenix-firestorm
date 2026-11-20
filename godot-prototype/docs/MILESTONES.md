# Godot Viewer Milestones

## M0 — Project and toolchain validation

- [x] Isolated prototype branch and project scaffold.
- [ ] Open project in the selected Godot .NET release.
- [ ] Confirm C# solution generation and clean build.
- [ ] Record Godot, .NET SDK, OS, GPU, and renderer versions.
- [ ] Add a repeatable CI build.

Exit gate: clean project import/build/run on Windows x64.

## M1 — Protocol dependency and login proof

- [ ] Audit LibreMetaverse and libOpenMetaverse lineage.
- [ ] Confirm license, maintenance status, supported target frameworks, and protocol gaps.
- [ ] Add dependency only after audit.
- [ ] Implement a Grid connection adapter behind IGridConnection.
- [ ] Login to a controlled OpenSimulator test Grid.
- [ ] Prove Second Life login separately using an authorized test account and current service requirements.

Exit gate: repeatable login/session lifecycle, redacted diagnostics, reconnect and logout tests. Login alone is not full compatibility.

## M2 — Minimal world replication

- [ ] Receive region handshake and region metadata.
- [ ] Receive terrain and basic object updates.
- [ ] Normalize object state into engine-independent models.
- [ ] Render a small controlled scene in Godot.
- [ ] Validate movement, object updates, and region teardown.

Exit gate: same controlled region can be inspected in reference Viewer and prototype with documented gaps.

## M3 — Legacy content foundation

- [ ] Prim/Linkset geometry and parameters.
- [ ] Texture and face/material mapping.
- [ ] Mesh assets and LOD.
- [ ] Asset cache and progressive loading.
- [ ] Avatar skeleton and baseline appearance.

Exit gate: representative legacy content gallery passes.

## M4 — User ecosystem

- [ ] Inventory and permissions.
- [ ] Wearables, attachments, and appearance baking.
- [ ] Animation, AO, gestures.
- [ ] HUD and scripted object-visible behavior.
- [ ] Chat, friends, groups, teleport.
- [ ] Voice and media integrations.

Exit gate: compatibility matrix has repeatable tests for each supported feature.

## M5 — Modern interaction

- [ ] Local interaction and grab UX.
- [ ] Collision and rigid-body presentation.
- [ ] Server-authoritative shared object state.
- [ ] Reconciliation, interest management, and low-spec performance.
- [ ] Legacy LSL behavior regression suite.

Exit gate: multi-client test with old content remains correct.

## M6 — Release readiness

- [ ] Windows x64 clean-machine package.
- [ ] Low-spec GPU and memory tests.
- [ ] Crash/reconnect/cache recovery tests.
- [ ] Security and dependency/license audit.
- [ ] Explicit compatibility statement with known limitations.

No milestone is considered complete solely because it compiles.
