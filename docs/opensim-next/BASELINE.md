# Firestorm Source Baseline

## Verified fork

- Fork: https://github.com/danggiatri6-spec/phoenix-firestorm
- Upstream parent: https://github.com/FirestormViewer/phoenix-firestorm
- GitHub reports `fork: true`; the parent and source repositories match the intended upstream.
- Fork visibility: public.
- Fork default branch: `master`.

## Pinned baseline

- Upstream branch: `Firestorm_7.2.5`
- Commit: `4ac201fdc56127de27de34567286885f4e75d194`
- Commit was verified against the upstream branch and is available through the fork's Git object database.
- Fork branch created: `Firestorm_7.2.5`
- Working branch created: `opensim-next/integration`
- Both branches were created at the pinned commit. The upstream `master` and fork `master` were not modified.

## Branch policy

- Treat `Firestorm_7.2.5` as the read-only baseline reference. Do not develop directly on it.
- Make OpenSim-next changes on `opensim-next/integration` or short-lived feature branches based on it.
- Keep upstream updates and OpenSim-next product changes in separate commits.
- Record the upstream commit SHA whenever rebasing or creating a new baseline.
- Never force-push the baseline branch.
- Before adopting a newer Firestorm release branch, review upstream changes, build requirements, licensing, and OpenSim compatibility.

## Important status boundary

The GitHub fork and branch references are verified. This does not mean the Viewer has been built, launched, tested against an OpenSim Grid, or packaged. Those remain separate engineering milestones.

## First build target

Use a supported Windows 10/11 x64 build environment. The user's older Windows 7 machine is not the target build environment for this Firestorm baseline.

Follow [BUILD_WINDOWS.md](BUILD_WINDOWS.md), and use the OpenSim-enabled build configuration (such as `ReleaseOS`), not a build configuration that omits OpenSim support.
