# Initial Dependency Audit

Audit date: 2026-11-20

## Godot

- Selected stable baseline for the prototype: Godot 4.7.2 .NET (x86_64).
- SDK package: `Godot.NET.Sdk/4.7.2`.
- Project target framework: `net8.0`.
- Renderer baseline: Compatibility renderer for the first low-spec feasibility pass.
- Official Windows download page lists the 4.7.2 .NET build and requires the .NET SDK.
- Godot's C# documentation requires the .NET-enabled editor and a separately installed .NET SDK. Godot 4.5 documentation specifies .NET 8 or later; the project currently targets .NET 8.

References:
- https://godotengine.org/download/windows/
- https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/c_sharp_basics.html
- https://www.nuget.org/packages/Godot.NET.Sdk/4.7.2

## LibreMetaverse

Initial assessment: suitable as the first protocol-library candidate, not yet accepted as a complete SL compatibility implementation.

- Source: https://github.com/cinderblocks/libremetaverse
- Repository description says it is a libOpenMetaverse fork seeking performance improvements and up-to-date compatibility with Second Life and OpenSimulator.
- Repository metadata at audit time: public, not archived, C# project, BSD-3-Clause license, pushed 2026-11-18.
- NuGet candidate: `LibreMetaverse 3.3.1`.
- NuGet lists .NET 8, .NET 9, .NET 10, .NET Standard 2.1 and .NET Framework 4.8.1 assets.
- Package documentation describes login/session, simulator circuit, avatars, object updates, inventory, asset transfer/cache, appearance/baking, parcel, grid, terrain, groups, friends, sound, environment, animesh, marketplace and interest-list managers.
- Separate packages are listed for model formats, OpenSim archives, utilities, Vivox/WebRTC voice, RLV, PrimMesher, mesh generation and imaging.
- License listed by NuGet and repository metadata: BSD-3-Clause.

References:
- https://www.nuget.org/packages/LibreMetaverse/3.3.1
- https://github.com/cinderblocks/libremetaverse

## Important gaps before adoption

1. Package feature descriptions are not proof of full Viewer behavior or current SL service compatibility.
2. We must inspect the current source, tests, message templates, Capabilities handling, login/MFA behavior, region crossing, asset transfer and error recovery.
3. We must test both a controlled OpenSimulator Grid and Second Life using authorized test accounts.
4. LibreMetaverse is a client protocol library, not a complete rendering/UI/Viewer implementation.
5. Its own rendering and imaging packages must be evaluated separately; Godot should own final scene rendering.
6. All transitive dependencies and licenses must be recorded before distribution.
7. Package integration and a clean Windows build have not yet been verified.

## Decision

Add LibreMetaverse 3.3.1 as a pinned prototype dependency for an integration spike. Keep it behind `IGridConnection` and engine-independent adapter boundaries. Do not couple Godot scene nodes directly to LibreMetaverse types.

This is a reversible prototype decision, not a commitment to use the package unchanged in the final Viewer.
