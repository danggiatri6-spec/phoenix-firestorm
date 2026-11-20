# Firestorm 7.2.5 Windows Build Audit

Audit baseline: `Firestorm_7.2.5` at `4ac201fdc56127de27de34567286885f4e75d194`.

This document records the build requirements found in that exact source revision's `doc/building_windows.md`, `requirements.txt`, and CMake configuration. It is a preparation checklist, not evidence that a build has succeeded.

## 1. Required host environment

| Component | Requirement | Purpose |
|---|---|---|
| OS | Windows 10/11, 64-bit | Supported build host; 32-bit viewer builds are no longer supported |
| Visual Studio | Visual Studio 2022 | C++ compiler and Windows SDK; install the **Desktop development with C++** workload |
| CMake | 4.1.2 or newer | Configure the viewer solution |
| Git | Git for Windows | Clone source and build-variable repositories |
| Python | Python 3 with pip | Autobuild and build scripts |
| Autobuild | 3.8 or newer | Fetch and install prebuilt third-party packages; install using the repository's requirements and the upstream documented command |
| Cygwin 64 | Include the `patch` package | Required by the documented test setup; Cygwin terminal is not used for the actual build commands |
| Firestorm build variables | Clone `FirestormViewer/fs-build-variables` | Supplies the `variables` file expected by Autobuild |

Use Windows Command Prompt (`cmd.exe`) for the documented build commands. The upstream guide specifically warns that PowerShell may not detect the Visual Studio build tools correctly.

Git for Windows line-ending setting: **Checkout as-is, commit as-is**.

## 2. Environment variables

Set the Visual Studio version and the path to the Firestorm build variables file:

```bat
set AUTOBUILD_VSVER=170
set AUTOBUILD_VARIABLES_FILE=C:\\firestorm\\fs-build-variables\\variables
```

The example path assumes the variables repository is cloned to `C:\\firestorm\\fs-build-variables`; adjust it if a different directory is used.

Install the Python requirements from the source checkout, then ensure the documented Autobuild installation is available:

```bat
python -m pip install -r requirements.txt
python -m pip install git+https://github.com/secondlife/autobuild.git#egg=autobuild
autobuild --version
```

The source requirements file at this baseline lists `llbase`, `autobuild`, and `llsd`. The Windows guide requires Autobuild 3.8 or newer.

## 3. OpenSim-specific build configuration

The Windows guide lists these relevant configurations:

- `ReleaseOS`: OpenSim enabled, no Kakadu, no FMOD.
- `RelWithDebInfoOS`: OpenSim enabled, no Kakadu, no FMOD, debug information.
- `ReleaseFS_open`: no Kakadu, no FMOD, **OpenSim disabled**.
- `ReleaseFS`: Kakadu and FMOD enabled, **OpenSim disabled**.

For the first OpenSim-next build, use `ReleaseOS`. Do not use `ReleaseFS_open`: it does not include OpenSim support.

Suggested first configuration (run from the root of the Firestorm source checkout):

```bat
autobuild configure -A 64 -c ReleaseOS -- --chan OpenSimNext -DLL_TESTS:BOOL=FALSE
```

Then build:

```bat
autobuild build -A 64 -c ReleaseOS --no-configure
```

The exact command should be checked against the generated configure output in the actual build environment before treating it as a reproducible build recipe.

## 4. Features intentionally excluded from the first build

- **Kakadu/KDU**: proprietary image codec; not required by the documented `ReleaseOS` target.
- **FMOD Studio**: requires a separately downloaded FMOD Studio API and package preparation; not included in `ReleaseOS`.
- **Havok**: do not enable `HAVOK_TPV` for this OpenSim build. The pinned source's `indra/cmake/Variables.cmake` explicitly turns `OPENSIM` off when `HAVOK_TPV` is enabled.
- **NSIS**: only needed later if creating a Windows installer; not required to compile and run the packaged build output.
- **Unit tests**: disabled for the initial build with `-DLL_TESTS:BOOL=FALSE`; add a separate test-enabled build later.

Important product trade-off: the upstream Windows guide says the open configuration without FMOD has no sound. Therefore the first OpenSim-enabled build should be treated as a connectivity and core-functionality milestone, not a final audio-complete client. Audio/voice dependencies need a separate compatibility and licensing decision.

## 5. Third-party dependency handling

Most third-party libraries are fetched and installed automatically by Autobuild into the build tree. A first configure may take substantial time and download a large dependency set. The `autobuild.xml` manifest in this pinned revision is the source of package definitions and platform-specific package hashes.

Do not manually replace package versions or edit the tracked `autobuild.xml` as an ad hoc workaround. If a local dependency override becomes necessary, follow the upstream guide's separate local Autobuild configuration approach and keep that override out of commits.

## 6. Suggested directory layout

```text
C:\\firestorm\\
  phoenix-firestorm\\       # this source checkout
  fs-build-variables\\     # upstream build-variable repository
```

Keep the baseline checkout clean. Use a separate working tree or clone for experimental patches, as recommended by the upstream Windows build guide.

## 7. Build stages and acceptance criteria

1. **Toolchain check**: `cmake --version`, `git --version`, `python --version`, `pip --version`, `autobuild --version`.
2. **Configure**: Autobuild completes the `ReleaseOS` configuration and installs required prebuilt packages.
3. **Compile**: `autobuild build` completes without compiler or linker errors.
4. **Launch**: packaged Viewer starts on Windows 10/11 x64.
5. **OpenSim login**: login panel can target an OpenSim Grid and authenticate.
6. **Smoke tests**: region arrival, avatar appearance, movement, inventory fetch, nearby chat, teleport, and basic object editing.

Do not claim a milestone until its acceptance criteria have actually been exercised and recorded.

## 8. Known constraints for this project

- The user's previously discussed Windows 7 PC is not a suitable target build host for this Firestorm baseline. Build on a supported Windows 10/11 x64 environment.
- The first goal is a stock Firestorm-derived OpenSim-enabled build, not a renderer rewrite.
- No successful build, runtime launch, or OpenSim Grid login has yet been established by this audit.
