# OpenSim-next Godot Viewer Prototype

This directory contains an isolated Godot-based Viewer feasibility prototype. It does not replace the Firestorm-derived Viewer and does not yet implement Second Life connectivity.

## Objective

Validate whether Godot can become the rendering, interaction, animation, and UI runtime for a Viewer that targets full Second Life and OpenSimulator ecosystem compatibility.

Compatibility is a requirement to be demonstrated feature by feature, not an assumption.

## Current status

- [x] Isolated feature branch created from `opensim-next/integration`.
- [x] Godot .NET project scaffold added.
- [ ] Godot editor/import/build verification.
- [ ] .NET protocol-library dependency audit.
- [ ] OpenSimulator login proof of concept.
- [ ] Second Life login proof of concept.
- [ ] Region object and terrain rendering.

The current application is only a startup shell. It has no Grid connection, no LLUDP implementation, no Capabilities implementation, and no SL asset decoder.

## Architecture rule

Keep protocol and compatibility code independent from Godot scene/rendering code. Godot should consume normalized world/session models through interfaces. This keeps the compatibility layer testable and makes a future renderer change less disruptive.

## Project layout

- `src/Main.cs` — prototype startup UI.
- `src/Protocol/IGridConnection.cs` — transport boundary only; no protocol implementation yet.
- `docs/ARCHITECTURE.md` — target architecture and boundaries.
- `docs/COMPATIBILITY_MATRIX.md` — feature-by-feature acceptance tracking.
- `docs/MILESTONES.md` — implementation sequence and gates.

## Build prerequisites

Install a matching Godot .NET (C#) editor/runtime and the .NET SDK required by that Godot release. Open `project.godot` in the Godot .NET editor, allow the C# solution to load, then build/run from the editor.

The Godot SDK version is pinned in `OpenSimNextGodotViewer.csproj`; update it deliberately when upgrading the Godot editor.

## Security

Never commit SL passwords, session tokens, personal account data, private Grid credentials, or local authentication files. Use test accounts and local-only configuration.
