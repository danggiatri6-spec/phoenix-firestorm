# Firestorm Fork Setup

## Why a real fork is required

`opensim-next-viewer` is currently an independent documentation repository. It does not contain the Firestorm source tree and does not share Firestorm's Git ancestry.

The production viewer source must retain the official upstream history. Do not copy the contents of `indra/newview` or upload a source archive as a substitute for a fork.

## Repository roles

- `FirestormViewer/phoenix-firestorm`: official upstream; read-only source of truth.
- `danggiatri6-spec/phoenix-firestorm`: existing personal fork and canonical source-code workspace.
- `danggiatri6-spec/phoenix-firestorm/docs/opensim-next/`: OpenSim-next integration plan, build notes, decisions, and release policy.
- `danggiatri6-spec/opensim-next`: platform architecture and server/Grid work.

## Fork status and provenance

The GitHub fork already exists under `danggiatri6-spec/phoenix-firestorm`. The canonical Viewer source and its upstream Git history remain in this repository; the OpenSim-next engineering documents are consolidated under `docs/opensim-next/`.

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
