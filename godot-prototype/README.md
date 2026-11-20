# OpenSim-next Godot Viewer Prototype

This directory contains an isolated Godot-based Viewer feasibility prototype. It does not replace the Firestorm-derived Viewer and does not yet implement full Second Life compatibility.

## Objective

Validate whether Godot can become the rendering, interaction, animation, and UI runtime for a Viewer that targets full Second Life and OpenSimulator ecosystem compatibility.

Compatibility is a requirement to be demonstrated feature by feature, not an assumption.

## Current status

- [x] Isolated feature branch created from `opensim-next/integration`.
- [x] Godot .NET 4.7.2 / .NET 8 project scaffold.
- [x] LibreMetaverse 3.3.1 dependency pinned after initial license/framework review.
- [x] Engine-independent Grid connection boundary.
- [x] First LibreMetaverse login/logout adapter spike.
- [x] Windows x64 GitHub Actions build workflow.
- [ ] First successful CI build.
- [ ] OpenSimulator login proof of concept.
- [ ] Second Life login proof of concept.
- [ ] Region object and terrain rendering.

The current UI is only a startup shell. The adapter code is an initial integration spike and has not yet been compiled or tested against a live Grid.

## Architecture rule

Keep protocol and compatibility code independent from Godot scene/rendering code. Godot should consume normalized world/session models through interfaces. This keeps the compatibility layer testable and makes a future renderer change less disruptive.

## Project layout

- `src/Main.cs` — prototype startup UI.
- `src/Protocol/IGridConnection.cs` — engine-independent Grid connection contract.
- `src/Protocol/LibreMetaverseGridConnection.cs` — initial login/logout adapter spike.
- `docs/ARCHITECTURE.md` — target architecture and boundaries.
- `docs/COMPATIBILITY_MATRIX.md` — feature-by-feature acceptance tracking.
- `docs/DEPENDENCY_AUDIT.md` — initial Godot/LibreMetaverse review.
- `docs/MILESTONES.md` — implementation sequence and gates.

## Build prerequisites

Install Godot .NET 4.7.2 (x86_64) and the 64-bit .NET 8 SDK. Open `project.godot` in the Godot .NET editor, allow the C# solution to load, then build/run from the editor.

The Godot SDK and LibreMetaverse package versions are pinned in `OpenSimNextGodotViewer.csproj`; update them deliberately after compatibility testing.

## Security

Never commit SL passwords, session tokens, personal account data, private Grid credentials, or local authentication files. Use test accounts and local-only configuration.
