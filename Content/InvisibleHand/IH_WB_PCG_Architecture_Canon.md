# Invisible Hand — Procedural Placement Architecture (Canon)

Accepted 2026-09-22. Records the decision to adopt UE 5.8's native PCG (Procedural Content
Generation) framework for procedural scatter/placement work, superseding this project's earlier
hand-rolled approach (`PGCEligibleTris`/camera-settle timer/manual `HISM->AddInstance` loop, built
across the 2026-09-18 → 2026-09-22 PGC groundcover rounds).

## Standing architecture decision

**UE 5.8's native PCG framework is the default for all procedural placement/scatter work in this
project going forward.** This supersedes hand-rolled per-triangle classification + manual
`UHierarchicalInstancedStaticMeshComponent` population as the default approach.

This is **not** an unconditional rule. Hand-rolled C++ remains the right call — and should be used —
when a specific, stated reason applies, for example:
- The placement logic needs tight custom C++ integration that doesn't express cleanly in PCG's
  node-graph model.
- An existing hand-rolled system is already stable and performance-proven, and migrating it would
  carry real risk for no clear benefit.

Any time hand-rolled is chosen over PCG going forward, the reason should be stated explicitly (in
code comments and/or the relevant plan), not silently defaulted to out of habit.

## Why: findings from direct engine source verification (not assumed)

Confirmed against this project's actual UE 5.8 install (`Engine/Plugins/PCG/`) before this decision
was made — matching this project's established practice of verifying architectural claims against
real source rather than trusting memory:

- **Runtime-legal, ships in packaged builds** — `PCG.uplugin` declares its module `"Type": "Runtime"`.
  `EPCGComponentGenerationTrigger::GenerateAtRuntime` (`PCGComponent.h`) is a real, first-class
  trigger backed by a genuine `PCGRuntimeGenScheduler` and player-position-based `PCGGenSourcePlayer`
  generation source (`RuntimeGen/` subdirectory) — Epic's own native version of the camera-
  proximity-gated generation pattern this project's PGC system hand-built across several rounds.
  This project has a hard "must work live in-game, not editor-only" requirement (see
  `IH_WB_Phase_Order_Canon.md`'s Nanite/World Partition rejection) — PCG's runtime generation was
  verified against exactly that bar before being adopted here.
- **Compatible with this project's non-Landscape terrain** — this project's terrain is not a UE
  Landscape actor, it's `BakedIslandMesh` (`UDynamicMeshComponent`, First Bake's own output).
  `Data/PCGDynamicMeshData.h` confirms native PCG support for sampling `UDynamicMesh` data directly.
  This compatibility fact is load-bearing for the whole decision and was validated directly (small
  real test against a baked island) before any production graph was built on top of the assumption.
- **Rendering parity plus a real improvement** — `PCGStaticMeshSpawner` uses
  `UPCGManagedISMComponent` under the hood, the same ISM/HISM rendering technology this project
  already used, but with PCG's own CRC-based instance reuse/diffing — a smarter incremental-update
  model than the hand-rolled system's "clear everything, rebuild everything" pattern.
- **Programmatically buildable, matching this project's asset-scripting convention** —
  `UPCGGraph::AddNodeOfType`/`AddEdge`/`GetInputNode`/`GetOutputNode` (all `BlueprintCallable`, so
  Python-accessible) let a PCG graph be constructed via a `Scripts/build_*.py` script, the same
  pattern already established for materials (`build_island_ground_naturalistic_material.py`) —
  no manual in-editor node-graph authoring required.

## Three confirmed native techniques

Node names below are UCLASS/settings-class names as they appear in
`Engine/Plugins/PCG/Source/PCG/Public/Elements/`, kept here as a durable reference so future rounds
don't have to re-research this from scratch.

### 1. Noise-threshold placement (no spacing computation needed)

`PCGSpatialNoise` (samples Perlin/other noise per candidate point) → `PCGDensityFilter`
(accept/reject by threshold). No pairwise distance math at all — the noise field's own peaks and
valleys ARE the spacing. Use for loose, natural-looking clumps (e.g. grass tufts) where exact
non-overlap doesn't matter.

### 2. Organic non-overlapping placement

`PCGSelfPruning` (`EPCGSelfPruningType::LargeToSmall` / `SmallToLarge` / `AllEqual`) — over-generate
candidate points cheaply (random or noise-driven), then prune points too close to a larger/denser
neighbor by extent. UE's native answer to "Poisson-disc-like" placement — not a literal Poisson-disc
generator, but the same practical result (guaranteed minimum spacing, organic look) via
generate-then-prune instead of a distance-aware generator. Use where instances shouldn't visibly
overlap/clip (denser foliage, larger props).

### 3. Mechanical rows (reserved for Grand Architect future phase — see below)

`PCGCreatePointsGrid` (real `CellSize` property, `FVector`, default 100cm) for exact regular rows.
Combine with `PCGCreateSurfaceFromSpline` to scope the grid to a player-drawn closed-spline area, and
`PCGDistance` to derive a distance-based value for a density/scale falloff.

## Grand Architect (future phase) — player-placed orchards/pines

**Status: accepted, roadmapped, not yet built** (matching this project's established
"deferred, not cut" convention — see Hydrology and the subdivision-based de-faceting round in
`IH_WB_Phase_Order_Canon.md`'s own Status section for the same pattern).

Concept, as specified by the user: a future Grand Architect gameplay phase lets the player place
"planted orchards" and "planted pines" (and similar) by drawing a **closed spline** — the enclosed
polygon becomes the planting area. Filled with crops/trees in **mechanical rows** (deliberately
regular, unlike the organic groundcover techniques above — this is cultivated, not wild), with
**canopy density intensity increasing toward the middle/crown** of the planted area (a rich, full
center falling off toward the edges, rather than uniform density throughout).

Confirmed-available native building blocks (not yet assembled into a real graph):
- `PCGCreateSurfaceFromSpline` — player-drawn closed spline → the surface/area to populate.
- `PCGCreatePointsGrid` — the mechanical-row placement itself.
- `PCGDistance` — a distance-based value (e.g. distance to the spline boundary, or to the
  polygon's centroid) usable to drive density or per-instance scale, producing the
  denser-toward-center falloff. Exact graph (which distance measure, how it maps to density vs.
  scale) is not yet designed — TBD when this phase is actually scoped.

Explicitly out of scope to build until the Grand Architect phase itself is scoped — recorded here so
the native building blocks don't need re-discovering later.

## Status

- PGC groundcover migration to PCG: in progress (2026-09-22 round) — Phase 0 (validate
  `PCGDynamicMeshData` against a real `BakedIslandMesh`) → Phase 1 (bridge this project's
  `DT_ASLSlopeBiome`/`DT_BiomeRecommendations`/`DT_PGCMeshCatalog` classification data into PCG
  per-point attributes) → Phase 2 (real PCG graph, noise-threshold + self-pruning) → Phase 3 (retire
  the hand-rolled system once parity is confirmed).
- Grand Architect orchard/pine placement: accepted spec, not yet scoped as its own build round.
- Everything else in this project (Terrain Stamps, future Sector Fabric props, etc.) stays on its
  current hand-rolled approach for now — broader PCG migration beyond PGC groundcover is its own
  future work, not assumed by this decision.
