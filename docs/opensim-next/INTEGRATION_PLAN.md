# Firestorm-first Integration Plan

## Goal

Build the OpenSim-next PC Viewer by extending a mature Firestorm-derived viewer rather than reimplementing the Second Life/OpenSim client stack.

## Baseline

- Upstream repository: https://github.com/FirestormViewer/phoenix-firestorm
- Candidate branch: `Firestorm_7.2.5`
- Candidate commit: `4ac201fdc56127de27de34567286885f4e75d194`
- This is a candidate pin only. It becomes the verified baseline only after a reproducible Windows build and successful connection to a test OpenSim Grid.

## Workstreams

### W0 — Repository and provenance
- Keep `danggiatri6-spec/phoenix-firestorm` as the single canonical Viewer source repository; keep OpenSim-next Viewer plans and build notes under `docs/opensim-next/` in this repository.
- Preserve upstream Git history; do not paste a source snapshot into a new unrelated history.
- Record upstream URL, branch, commit SHA, import date, local patches, and third-party notices.
- Keep OpenSim-next-specific changes in clearly named commits and avoid broad unrelated formatting changes.

### W1 — Reproducible Windows build
- Target Windows 10/11 x64 first.
- Use Visual Studio 2022 with Desktop development with C++.
- Install the branch-compatible CMake, Python 3, Git for Windows, Autobuild, and required Windows build tools.
- Use the upstream Windows build guide for the pinned branch as the authority for exact dependency versions.
- Build the OpenSim-enabled configuration (candidate: `ReleaseOS`), not a configuration that excludes OpenSim support.
- Initially avoid optional licensed components such as Kakadu/KDU, FMOD, or Havok unless their licensing and build requirements are separately satisfied.
- Capture complete build logs and dependency versions.

### W2 — OpenSim compatibility validation
Validate in this order:
1. Grid login and logout.
2. Region handshake and scene synchronization.
3. Avatar appearance, movement, and animation.
4. Inventory and asset retrieval.
5. Local chat, IM, friends, and group-related flows as supported by the target Grid.
6. Object rez, edit, link, and basic building tools.
7. Teleport and region crossing.

Do not claim compatibility based only on a successful compile.

### W3 — Product identity and Grid entry
- Add OpenSim-next branding without removing upstream attribution.
- Add a configurable Grid/login entry flow.
- Keep authentication and Grid service endpoints configurable rather than hard-coded.
- Preserve support for ordinary OpenSim grids where technically possible.

### W4 — Performance and rendering evolution
- First measure the inherited viewer on representative low-, mid-, and high-range Windows PCs.
- Establish frame-time, memory, startup, login, and region-entry baselines.
- Introduce quality presets and conservative defaults.
- Replace renderer subsystems only behind explicit interfaces and only after visual and functional parity tests.
- Keep the SDL3/OpenGL prototype as a separate research track until a concrete subsystem is ready to migrate.

### W5 — UX and AI assistance
- Prioritize navigation, inventory, chat, building, accessibility, and clear settings.
- AI assistance should help real users communicate and create: translation, speech transcription, building assistance, and script help.
- Do not introduce AI residents that impersonate human users.

## Platform delivery order

1. Windows 10/11 x64.
2. macOS after Windows core workflows stabilize.
3. Linux as a later supported desktop platform; early CI portability checks do not imply equal release priority.
4. Android as a separate lightweight client after the PC viewer is mature.

## Definition of done for the first integration milestone

- [ ] Upstream source history is preserved.
- [ ] Exact upstream commit is pinned and documented.
- [ ] Windows 10/11 x64 build is reproducible from a clean checkout.
- [ ] Viewer launches without optional licensed components.
- [ ] Viewer logs into a test OpenSim Grid.
- [ ] Login, scene sync, avatar movement, chat, inventory, and basic building pass manual tests.
- [ ] Upstream notices and third-party licenses are inventoried.

## Current limitations

The former standalone `opensim-next-viewer` repository contained project documentation only; those documents have been migrated into this repository under `docs/opensim-next/`. The full Firestorm source remains in its existing upstream-derived history. No build, runtime, or Grid-compatibility test has yet been completed.