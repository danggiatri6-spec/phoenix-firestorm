# OpenSim-next Rendering Audit

Audit target: `opensim-next/integration` (Firestorm-derived source).

## Finding 1 — Spatial-group vertex-buffer capacity check

**Status: patched; build and runtime validation pending.**

File: `indra/newview/llspatialpartition.cpp`  
Function: `LLSpatialPartition::rebuildGeom(LLSpatialGroup* group)`

The capacity guard compared both requested capacities against `getNumVerts()`:

```cpp
group->mVertexBuffer->getNumVerts() != vertex_count ||
group->mVertexBuffer->getNumVerts() != index_count
```

The second comparison is dimensionally incorrect. `LLVertexBuffer` exposes a distinct `getNumIndices()` accessor, and the buffer is allocated with `allocateBuffer(vertex_count, index_count)`. The guard now compares index capacity with index count:

```cpp
group->mVertexBuffer->getNumVerts() != vertex_count ||
group->mVertexBuffer->getNumIndices() != index_count
```

Why it matters: when vertex and index counts differ (a normal condition for indexed geometry), the old second comparison remains true even if the buffer already has the required index capacity. That can force unnecessary buffer-object reallocation whenever this geometry-rebuild path runs. This is a plausible CPU/driver overhead and allocation-churn reduction; no FPS improvement is claimed until measured.

### Upstream cross-check

The current upstream Firestorm `master` version of this same function already uses `getNumIndices() != index_count`. This OpenSim-next patch brings the integration branch into line with that upstream correction.

### Change

- Commit: `73d17bb275fbbbe043335a3b2753bc616051f303`
- Scope: one-line correctness/performance guard correction.
- No protocol, asset format, LSL, rendering output, or user setting changes intended.

### Validation still required

1. Build the OpenSim-enabled Windows x64 `ReleaseOS` configuration.
2. Exercise a scene with repeated static geometry and dynamic geometry rebuilds.
3. Capture CPU frame time, GPU frame time, FPS/1% lows, and allocation/rebuild counts before and after.
4. Confirm geometry, alpha, PBR materials, attachments, and region crossing remain visually and functionally correct.

## Existing profiling and optimization infrastructure

The inspected source already contains Tracy/profile instrumentation in the rendering pipeline, spatial partitioning, texture processing, and draw pools. `llperfstats.cpp` also contains avatar timing and autotune metrics. Therefore the next optimization step should use these existing instruments rather than introduce a new profiler or blindly alter global quality defaults.

## Next audit targets

1. Texture fetch/decode scheduling and virtual-size/discard-level decisions in `llviewertexture.cpp`.
2. Spatial culling and geometry rebuild frequency in `llspatialpartition.cpp` / `pipeline.cpp`.
3. Alpha and transparent draw passes in `lldrawpoolalpha.cpp`.
4. Avatar impostor and attachment rendering cost.
5. Establish a repeatable OpenSim test scene and benchmark capture protocol.

## Status boundary

This audit is source inspection plus a one-line patch. It is **not** a successful build, benchmark, or live Grid test. Do not infer measured performance gains from the source change alone.
