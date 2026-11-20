# Windows End-User Distribution Policy

## Product requirement

OpenSim-next Viewer must run on supported Windows 10/11 x64 systems without requiring users to install development tools.

The end-user installation must not require:
- Python
- Autobuild
- CMake
- Git
- Visual Studio or Visual Studio Build Tools
- Cygwin

These are build-time tools only and belong exclusively on CI/build runners.

## Delivery formats

The first public test release should provide:

1. **Windows installer**: a signed or clearly marked unsigned NSIS installer, containing the complete Viewer runtime, plugins, resources, and required redistributable runtime files.
2. **Portable ZIP**: a complete unpacked Viewer distribution for users who prefer not to install. The CI workflow now creates this archive from the packaged runtime tree, excluding the installer subfolder.

The installer is the primary distribution format. The portable ZIP is secondary and must be validated independently.

## Build and packaging pipeline

- Build target: x64 `ReleaseOS` (OpenSim enabled, KDU disabled, FMOD disabled).
- CI runner: Windows Server 2022 / Visual Studio 2022 toolchain.
- CI-only dependencies: Visual Studio Build Tools, CMake, Python, Autobuild, Cygwin/patch as required by upstream, and NSIS for the legacy installer route.
- Use Firestorm's `viewer_manifest.py` to collect the application, DLLs, plugins, and resources.
- Configure with `--package` so the viewer manifest runs its packaging actions.
- Disable the CI Velopack signing-dependent path for the first build and use the existing NSIS package path, unless a future OpenSim-next signing/release service is deliberately configured.

## Runtime dependency policy

Do not assume that copying the main EXE is sufficient. The release package must include the full manifest output, including:
- Viewer executable and dependent DLLs
- media plugins and their runtime files
- skins, UI XML, shaders, character data, and static assets
- certificates and other runtime data expected by the viewer
- supported Microsoft runtime redistributables where redistribution is permitted

Do not package build tools (Python, Autobuild, CMake, Git, Visual Studio, Cygwin) as application dependencies.

## Release acceptance tests

A build is not considered distributable merely because compilation succeeds.

Before declaring a user-ready release:

1. Verify that the NSIS installer exists and has a non-zero size.
2. Verify that the unpacked distribution contains the Viewer executable, required plugins, DLLs, and resource directories.
3. Install on a clean Windows 10/11 x64 VM without developer tools.
4. Launch the Viewer and confirm it does not depend on paths or environment variables from the CI runner.
5. Confirm login to a test OpenSim Grid, avatar appearance, region synchronization, inventory, and chat.
6. Check Windows Event Viewer and application logs for missing DLLs or runtime failures.
7. Test uninstall and reinstall.
8. Validate the portable ZIP separately if it is offered.

## Current status

The initial CI configure attempts failed because Firestorm's documented `--package` / `--chan` example was forwarded verbatim to CMake, which rejected `--chan`. Inspection of the actual CMake project shows these are represented by CMake cache variables, so the workflow now uses `-DPACKAGE:BOOL=ON` and `-DVIEWER_CHANNEL:STRING=OpenSimNext`. The workflow also installs NSIS, disables Velopack for the unsigned first-build path, stages the complete manifest output, and creates a portable ZIP. The corrected run must still complete successfully, and both artifacts must pass clean-VM testing before this requirement can be marked complete.
