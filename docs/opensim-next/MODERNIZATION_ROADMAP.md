# OpenSim-next Firestorm Modernization Roadmap

## Decision

Firestorm remains the only production Viewer direction for OpenSim-next. Godot Viewer development is paused; the existing `godot-prototype` branch is retained as an isolated experiment and is not part of the release path.

The `opensim-next/integration` branch remains the development branch. Do not modify the upstream baseline branch or the ordinary Firestorm branch.

## Non-negotiable compatibility rule

Modernization must preserve existing Second Life and OpenSim content behavior. Changes to rendering, UI, inventory, animation, and interaction must not silently change LSL behavior, permissions, asset interpretation, attachment behavior, or network protocol semantics.

Prefer additive changes and existing Firestorm abstractions. Do not replace the renderer or rewrite protocol/asset systems as a first step.

## What the current source already gives us

The integration branch's display implementation already uses Firestorm's established render pipeline and includes integration points for:
- GLTF material handling (`llgltfmateriallist`)
- shader management (`llviewershadermgr`)
- scene monitoring and performance statistics (`llscenemonitor`, `llperfstats`)
- the existing pipeline and texture list (`pipeline`, `llviewertexturelist`)

This is evidence that the project has an existing modernization foundation; it is not evidence that any particular performance or visual target has been met.

## Workstream order

### 1. Establish a reproducible baseline before optimization

Record the exact source commit, build configuration, GPU/driver, display resolution, graphics preset, draw distance, avatar count, and test scene.

Measure at minimum:
- average FPS and 1% low FPS, where available
- CPU frame time and GPU frame time
- peak and steady-state RAM/VRAM
- login-to-world-ready time
- region-entry and teleport time
- texture/mesh loading stalls
- shader compilation and first-use stutter

Use the same scene, camera, avatar count, resolution, and graphics settings for every comparison. Do not claim a rendering improvement from screenshots alone.

### 2. Rendering and performance (first engineering priority)

Audit the existing pipeline before changing it. Prioritize:
1. Avoid unnecessary geometry rebuilds and duplicate GPU uploads.
2. Improve texture prioritization, decode scheduling, and cache reuse.
3. Reduce CPU submission overhead for repeated/static objects where safe.
4. Profile shadow, reflection, alpha, and water passes independently.
5. Provide clear low/medium/high presets with conservative low-end defaults.
6. Keep GLTF/PBR behavior correct while preserving legacy material appearance.

Do not globally increase visual quality defaults until low-end performance has been measured. Do not remove compatibility rendering paths merely to simplify the renderer.

### 3. Ordinary-user UI

Keep the 3D world as the primary workspace:
- Inventory opens on demand and closes to restore the full world viewport; it is not permanently docked.
- Put Create/build tools and advanced settings behind a compact More/Tools entry point.
- Keep common actions discoverable: chat, people, map, movement, appearance, inventory.
- Add inventory search and useful filtering before attempting expensive automatic thumbnail generation for every asset.
- Make animation/AO search, categories, preview, and selection easier without changing animation asset semantics.

UI changes should be incremental and individually testable. Preserve familiar shortcuts and provide a route to advanced controls.

### 4. Inventory usability

Inventory scale is a major constraint. Design for accounts with tens of thousands of items:
- Search and filtering must not require loading every asset.
- Generate previews lazily, cache them, and allow cancellation.
- Never bulk-download all inventory assets to create thumbnails.
- Show clear loading/error states and preserve existing folder/permission behavior.

### 5. Avatar, animation, and interaction

Only after baseline rendering and UI work:
- improve animation discovery and AO management;
- validate attachment and layered clothing behavior;
- improve interaction affordances for scripted objects;
- consider local prediction for responsive feedback while leaving authoritative object/script state on the simulator.

Do not introduce a new client-side physics authority that conflicts with the simulator.

## First acceptance milestone

The first modernization milestone is not a new renderer. It is a repeatable ReleaseOS build and a measured baseline on a real OpenSim test Grid, with:
- login, region arrival, avatar appearance and movement;
- terrain, Prim, Mesh, textures, alpha and GLTF/PBR materials;
- inventory fetch, nearby chat, teleport and basic object interaction;
- performance measurements from a documented representative scene.

## Change-control checklist

For every feature or optimization:
- state the user-visible problem;
- identify the exact Firestorm subsystem being changed;
- confirm whether SL and OpenSim behavior differ;
- preserve a fallback or setting when visual behavior may change;
- build the OpenSim-enabled configuration;
- test on an OpenSim Grid and, where relevant, a Second Life Grid;
- report measured results and known regressions.

## Status

This document records the agreed direction and engineering sequence. It does not claim that the baseline build, Grid tests, benchmark suite, or proposed improvements have already been completed.
