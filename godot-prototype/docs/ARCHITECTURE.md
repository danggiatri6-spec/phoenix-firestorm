# Godot Viewer Target Architecture

## Product contract

The target is a desktop Viewer for both Second Life and OpenSimulator. The compatibility target includes legacy content and services: Prim/Linkset, Mesh, textures/materials, avatar appearance, clothing, animations, gestures, AO workflows, inventory and permissions, HUDs, LSL-driven object behavior, chat, groups, teleport, voice, and media.

"Full SL compatibility" is the target, not a claim about current implementation. Each area must have explicit tests and known limitations.

## Architectural boundaries

### 1. Godot runtime

Owns presentation and local real-time experience:

- Desktop UI and window/layout management.
- Scene graph and render submission.
- Camera and local input.
- Local animation evaluation and blending.
- Client-side physics queries, prediction, interpolation, and visual effects.
- Audio presentation and spatialization.
- Asset cache presentation and previews.

### 2. Compatibility core (engine-independent .NET)

Owns protocol and semantic translation:

- Login/session lifecycle.
- LLUDP message encoding/decoding and reliable packet behavior.
- HTTP Capabilities and LLSD.
- Simulator/region state and object update normalization.
- Asset and inventory protocol models.
- Permission and ownership semantics.
- Avatar appearance and animation data models.
- Service adapters for chat, groups, teleport, voice, media, and other capabilities.

The compatibility core must not depend on Godot scene nodes or rendering APIs. Godot-facing code consumes normalized models/events.

### 3. Grid services

Second Life and OpenSimulator remain authoritative for their existing server-side systems, including LSL execution, inventory permissions, object ownership, and world persistence. New OpenSim-next features may add server-authoritative physics and interaction services, but must not silently change legacy SL behavior.

## Protocol strategy

- Audit LibreMetaverse and related libOpenMetaverse lineage before selecting a dependency.
- Verify current maintenance, target frameworks, protocol coverage, license obligations, test coverage, and compatibility with both SL and OpenSimulator.
- Reuse only components whose behavior and license have been reviewed.
- Implement missing behavior behind small interfaces; avoid forking a large protocol stack before the gap analysis.
- Treat LLUDP and HTTP Capabilities as complementary, not interchangeable.
- Do not assume a successful login proves complete protocol compatibility.

## Rendering and asset strategy

- Build explicit decoders/converters for legacy SL asset and object representations.
- Preserve source semantics and identifiers; do not destructively rewrite user assets.
- Cache decoded assets with versioned formats and invalidation rules.
- Support progressive loading, LOD, occlusion/culling, texture memory budgets, and low-spec fallback.
- Validate against real content from controlled test accounts and grids.

## Physics and authority

- Godot physics may provide local collision queries, prediction, and presentation.
- The Grid/server remains authoritative for shared object state and permissions.
- New networked physics requires a defined server protocol, ownership model, reconciliation, sleeping/interest management, and compatibility behavior for legacy scripted objects.
- Never make client-side collision or grab decisions authoritative for other users.

## Compatibility rules

1. Existing SL content should continue to work without creator-side modification wherever the underlying protocol and asset access permit.
2. New features must be additive and capability-negotiated.
3. Legacy LSL remains server-executed; the Viewer implements the visible effects and protocol behavior.
4. Unknown or unsupported data must fail safely and be diagnosable, not silently corrupt assets.
5. Compare behavior with a known Firestorm build against the same Grid and test assets.

## Security and privacy

- Do not log passwords, session IDs, secure session IDs, or authentication headers.
- Keep credentials and test Grid configuration outside the repository.
- Validate all server-supplied lengths, identifiers, and asset payloads.
- Treat remote asset data and script-driven content as untrusted input.
