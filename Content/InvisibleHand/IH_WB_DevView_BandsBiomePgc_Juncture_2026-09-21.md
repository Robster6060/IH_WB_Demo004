# Juncture — DEV View BANDS | BIOME | PGC all rendering correctly

**Tag:** `juncture/ih-wb-devview-bands-biome-pgc-2026-09-21`
**Date:** 2026-09-21
**Status:** **SAFE RETURN POINT** — PIE-confirmed by direct visual check: BANDS, BIOME, and PGC all
render correctly (real per-row color/texture variation, no gray/uniform fallback).

## What broke

A separate Claude session picked up the real, correctly-diagnosed problem of hard rock/snow color
edges at classified-row boundaries (see `IH_WB_Phase_Order_Canon.md`'s sibling docs and this
project's plan history for the "one MID per section means no cross-boundary blending" root cause)
and began building the right fix — a shared, vertex-color-driven material per DEV View mode. Its own
plan scoped a cheap, throwaway validation step ("Phase 0") before committing to the full build. That
step spiralled into 21+ hours and 6 git commits of escalating isolated-reproduction debugging on
branch `pgc-material-rebuild` (an isolated test level, four throwaway test materials, DDC-staleness
checks, a "ComponentMask hypothesis" test) without ever reaching Phase 1 — and along the way left the
real, live production BANDS material (`M_IH_IslandFlatVertexColor`) mid-experiment with an untested
`ComponentMask` node grafted in.

Independently of that, and not touched by any of those 6 commits, `ApplyDevColorMode` (the DEV View
checkbox toggle handler, added 2026-09-20 as part of the same shared-vertex-color-material work) had
its own real correctness bug: it accumulated vertex colors by iterating
`FProcMeshSection::ProcVertexBuffer` directly. In this codebase every classified-row section shares
ONE whole-island vertex array (`BuildMeshesFromCellGraph`: "Shared vert buffers; per-matched-row
index lists") — a section's `ProcVertexBuffer` is NOT scoped to that section's own vertices, it's the
entire island's buffer, with only the *index list* differing per section. So every row's classified
color got accumulated into *every* vertex on the island, and the resulting per-vertex color averaged
out to one flat, uniform blend of all ~20-48 rows present — BIOME rendered solid white, PGC rendered
solid uniform tan/gray. BANDS looked fine only because it's the default mode and was never
re-toggled through this buggy path in the sessions that hit this bug — it was still showing its
correct one-time generation-time bake from `ApplyDtBiomeColorBands`, which already iterates
correctly (see below).

## The fix

`ApplyDevColorMode`'s accumulation loop now iterates `ProcSection->ProcIndexBuffer` (that section's
own triangle indices into the shared vertex array) instead of the raw `ProcVertexBuffer`, mirroring
the already-correct pattern in `ApplyDtBiomeColorBands`'s generation-time bake. One file,
`IH_WB_IslandActor.cpp`, ~16 lines.

## Recurring lesson: `ProcVertexBuffer` is not section-scoped in this codebase

This is the **third** time this exact trap has caused a real bug in this project:

1. PGC groundcover eligibility cache — an earlier round appended a whole section's `ProcVertexBuffer`
   per triangle lookup, wasting work scanning far more vertices than a section's triangles actually
   touch.
2. First Bake's mesh-welding pass — appended each section's *entire* `ProcVertexBuffer` when
   constructing the combined `FDynamicMesh3`, producing a mesh with ~40x more vertices than triangles
   (5.3M vertices for 132K triangles) before being caught and fixed.
3. This bug — `ApplyDevColorMode`'s vertex-color accumulation.

**Any new code that touches `FProcMeshSection` data must go through `ProcIndexBuffer` to know which
of the shared buffer's vertices actually belong to that section** — never assume `ProcVertexBuffer`
is pre-filtered to just that section's own geometry.

## Bonus finding, not yet independently verified

`AccumulateVertexColorValue`/`SampleAccumulatedVertexColor` (`IH_WB_IslandActor.cpp`, ~line 1903) is
a true sum/count average keyed by quantized world position — so once accumulation is scoped
correctly (this fix), a vertex shared by position between two *differently-classified* adjacent
triangles now correctly blends their two values, with the GPU linearly interpolating that blend
across the triangle surface. This is exactly the texture-splatting technique needed to fix the
rock/snow hard-edge problem that motivated the other session's whole vertex-color rebuild attempt —
it may now already be working, or partially working, as a direct side effect of this fix, with no
further material-graph changes needed. Worth checking in PIE before scoping any further material
work.

## Diagnostic technique worth reusing

`git diff <checkpoint-commit> HEAD --stat -- <suspect-files>` — proving *zero* changes to a specific
file since a known-good checkpoint is a fast, decisive way to rule a committed code change in or out
as the cause of a regression, before spending time on runtime/PIE debugging.

## Git record

- Restored working tree: `git checkout main` (from `pgc-material-rebuild`, which is left intact,
  unmodified, for the record of the correct architectural diagnosis it contains).
- Bug fix commit: `877692a` — "Fix ApplyDevColorMode vertex-color accumulation: was reading
  whole-island shared buffer".
- This tag points at that commit.
