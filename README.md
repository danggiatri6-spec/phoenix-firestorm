<img align="left" width="100" height="100" src="doc/firestorm_256.png" alt="Logo of Firestorm viewer"/>

**[Firestorm](https://www.firestormviewer.org) is a free client for 3D virtual worlds such as Second Life and various OpenSim worlds where users can create, connect and chat with others from around the world.**

This repository contains the official source code for the Firestorm viewer.

## Open Source

Firestorm is a third party viewer derived from the official [Second Life](https://github.com/secondlife/viewer) client. The client codebase has been open source since 2007 and is available under the LGPL license.

## Download

Pre-built versions of the viewer releases for Windows, Mac and Linux can be downloaded from the [official website](https://www.firestormviewer.org/choose-your-platform/).

## Build Instructions

Build instructions for each operating system can be found using the links below and in the official [wiki](https://wiki.firestormviewer.org).

- [Windows](doc/building_windows.md)
- [Mac](doc/building_macos.md)
- [Linux](doc/building_linux.md)

> [!NOTE]
> We do not provide support for compiling the viewer or issues resulting from using a self-compiled viewer. However, there is a self-compilers group within Second Life that can be joined to ask questions related to compiling the viewer: [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers)

## Contribute

Help make Firestorm better! You can get involved with improvements by filing bugs and suggesting enhancements via [JIRA](https://jira.firestormviewer.org) or [creating pull requests](CONTRIBUTING.md).

## Community respect

This section is guided by the [TPV Policy](https://secondlife.com/corporate/third-party-viewers) and the [Second Life Code of Conduct](https://github.com/secondlife/viewer?tab=coc-ov-file).

Firestorm code is made available during ongoing development, with the **master** branch representing the current nightly build. Developers and self-compilers are encouraged to work on their own forks and contribute back via pull requests, as detailed in the [contributing guide](CONTRIBUTING.md).

If you intend to use our code for your own viewer beyond personal use, please only use code from official release branches (for example, `Firestorm_7.1.13`), rather than from pre-release/preview or nightly builds.


## OpenSim-next Viewer

This repository is the canonical Viewer source repository for OpenSim-next. It retains the complete Firestorm source tree, upstream Git history, build system, third-party dependency definitions, and licensing notices. OpenSim-next-specific Viewer work is developed on the `opensim-next/integration` branch and feature branches based on it.

### OpenSim-next engineering documents

- [Integration plan](docs/opensim-next/INTEGRATION_PLAN.md)
- [Pinned Firestorm baseline](docs/opensim-next/BASELINE.md)
- [Windows build audit](docs/opensim-next/BUILD_AUDIT.md)
- [Windows end-user distribution policy](docs/opensim-next/USER_DISTRIBUTION.md)
- [Firestorm modernization roadmap](docs/opensim-next/MODERNIZATION_ROADMAP.md)
- [Repository provenance and branch policy](docs/opensim-next/FORK_SETUP.md)

### Repository boundaries

- [opensim-next](https://github.com/danggiatri6-spec/opensim-next): platform-level architecture, roadmap, release coordination, and cross-repository decisions.
- [opensim](https://github.com/danggiatri6-spec/opensim): OpenSimulator-derived server, simulator runtime, scripting, protocol, and Grid services.
- This repository: Firestorm-derived PC Viewer source, client integration, UI/localization, rendering, performance, build, and packaging.

The former standalone [opensim-next-viewer documentation repository](https://github.com/danggiatri6-spec/opensim-next-viewer) is being consolidated here. The source repository is not replaced by a source snapshot: the Firestorm upstream history remains authoritative. No successful build or Grid-compatibility milestone is implied by this documentation migration.
