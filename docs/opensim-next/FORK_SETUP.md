# Firestorm Fork Setup

## Why a real fork is required

`opensim-next-viewer` is currently an independent documentation repository. It does not contain the Firestorm source tree and does not share Firestorm's Git ancestry.

The production viewer source must retain the official upstream history. Do not copy the contents of `indra/newview` or upload a source archive as a substitute for a fork.

## Repository roles

- `FirestormViewer/phoenix-firestorm`: official upstream; read-only source of truth.
- `danggiatri6-spec/phoenix-firestorm`: intended personal fork and canonical source-code workspace after it is created.
- `danggiatri6-spec/opensim-next-viewer`: OpenSim-next integration plan, build notes, decisions, and links to the source fork.
- `danggiatri6-spec/opensim-next`: platform architecture and server/Grid work.

## Create the fork

1. Open https://github.com/FirestormViewer/phoenix-firestorm/fork
2. Choose owner `danggiatri6-spec`.
3. Keep the repository name `phoenix-firestorm`; do not rename the existing `opensim-next-viewer` repository.
4. Keep the fork public so upstream provenance and collaboration remain transparent.
5. Create the fork without selecting only one branch; preserve the upstream branches and tags.

The GitHub fork operation must be completed by the repository owner in the GitHub UI. The connected GitHub integration available to this workflow can inspect repositories and write files, but it does not expose a fork-creation operation.

## Verify after creation

Confirm that the fork page identifies `FirestormViewer/phoenix-firestorm` as its parent. Then verify:

- The fork contains branch `Firestorm_7.2.5`.
- That branch resolves to `4ac201fdc56127de27de34567286885f4e75d194`.
- The fork has the complete upstream source tree and tags.

Only after these checks should we create an OpenSim-next integration branch in the fork, for example `opensim-next/integration`. Keep the upstream baseline branch unchanged.

## Pinned evaluation baseline

- Upstream repository: https://github.com/FirestormViewer/phoenix-firestorm
- Branch: `Firestorm_7.2.5`
- Commit: `4ac201fdc56127de27de34567286885f4e75d194`
- Commit URL: https://github.com/FirestormViewer/phoenix-firestorm/commit/4ac201fdc56127de27de34567286885f4e75d194

This is a verified upstream branch/commit reference, not a verified OpenSim-next build. A successful Windows build and a successful login to an OpenSim test Grid are still required.
