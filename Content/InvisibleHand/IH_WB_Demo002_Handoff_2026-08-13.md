# IH_WB_Demo002 Handoff — 2026-08-13

## 1. Header

- **Date:** 2026-08-13
- **Project:** `D:\Projects\ClaudeProjects\IH_WB_Demo002\IH_WB_Demo002.uproject` (UE 5.8)
- **Forked from:** `D:\Projects\UE58Projects\IH_WB_Heightmap` (git history preserved), rename commit `e731795`
- **Branch:** `main` (local; no remote configured yet for this fork)
- **Safe revert commit:** `7381dcf` ("Phase 1c: AddRange/AddTrough diffusion op + top-down dev camera command") — last commit before this session's dev-preview-actor debugging began
- **This session's preview-actor work:** committed (see git log — message tags it explicitly as a dev tool, findability/rendering caveats included) — see §7
- **Do No Harm:** none of this session's changes touch the live `AIH_WB_IslandActor` generation path at all; everything is isolated to the new, currently-unused `WorldBuilder/CellGraph/` module. Nothing needs disabling.
- **Purpose:** Close out a 7-round dev-preview-actor rendering mystery (documented below, non-blocking), and hand off the actual next step per the user's explicit direction: **review, analyze, and learn this handoff and the underlying code first — then determine your own solid, simple course to road-test UE5.8's native GeometryCore module (§2) and jump-start forward progress.** Re-fixing or re-applying the visualization tool (§4) is explicitly **not** required before doing this.

## 2. The eureka finding: UE5.8 needs no third-party library for the Azgaar cell graph

This is the pivot that made the Phase 1 rewrite viable at all, and it must not get lost under the preview-actor detour in §4 — it's the actual forward momentum from this project's fork.

**Original finding, verbatim from the session that made it:**

> Good news to start Phase 1 on: no third-party library needed at all.
>
> Phase 1a resolved: UE5.8 ships a built-in `UE::Geometry::FDelaunay2` in the core `GeometryCore` module (`Engine/Source/Runtime/GeometryCore/Public/CompGeom/Delaunay2.h`) — part of the base engine, not even requiring the optional GeometryProcessing plugin. It does exactly what the Azgaar-style cell graph needs in one call: `GetVoronoiCells()` returns "cells as dual of the Delaunay triangulation," with built-in boundary clipping (`ClipBounds`/`ExpandBounds` params) — precisely the jittered-point Voronoi cell graph the plan calls for, first-party and free. This eliminates the third-party-library question entirely and de-risks Phase 1's foundation.

This resolved what had been an open risk in the original plan (Task 4, Plugin/Tooling Recommendations): "evaluate a small permissively-licensed C++ header library vs. hand-rolled." Neither was needed. `FDelaunay2` is already linked into this project (`GeometryCore` + `GeometryFramework` in `IH_WB_Demo002.Build.cs`).

**Design decision made from this finding (already implemented, not just theorized):**

- **Option A chosen — direct cell-boundary coastline trace**, over raster-heightfield + Geometry Script contouring. The coastline is defined as the set of Voronoi edges between classified Land and Ocean cells, chained into closed loops (`FIHTerrainCellDiffusion::TraceCoastlineLoops`). This is structurally immune to the raster/marching-squares sawtooth quantization that root-caused the legacy generator's defect (§5) — there's no grid to quantize against, the coastline *is* the cell-graph topology.
- **Cell density chosen: ~75 m** (`TargetCellWidthCm = 7500.0` in the generator's build params), within the ~50–100 m range discussed when this was decided — fine enough to resolve Harborage/Firth-scale inlet features (canon minimum channel widths 40–200 m, per the river/inlet dimension table established this session), coarse enough to keep cell counts tractable (~1,700 cells for a 3 km test patch).
- Jittered-grid site generation with a **single jitter pass, no Lloyd relaxation** (`GenerateJitteredGridSites`, `IHTerrainCellGraphGenerator.cpp`) — deliberately preserves Azgaar's organic irregularity; relaxation would smooth the jitter back toward a regular grid and defeat the point.
- Adjacency comes free from the same triangulation (`Delaunay.GetTrianglesAndAdjacency`) — Voronoi neighbors are exactly the cells whose sites share a Delaunay edge, no second pass needed.

**Core data types** (`IHTerrainCellGraphTypes.h`) — `FIHTerrainCellGraph` holds a flat `TArray<FIHTerrainCell>`; each cell carries its Voronoi boundary polygon, neighbor indices, diffusion-accumulated `Height`, `EIHCellFeature` (Ocean/Land/Lake-reserved-for-Hydrology), coastal distance, and haven/harbor metadata — a direct C++ port of Azgaar's `heightmap-generator.ts`/`features.ts` field set onto this engine-native substrate.

**This part of Phase 1 is finished and needs no further design work** — only the wiring described in §5.

## 3. What's done and verified (independent of any rendering question)

Phase 1's Azgaar-style terrain substrate is **complete and passing 3 automation tests**, all headless, all asserting on data, none dependent on PIE or rendering:

| File | Contents |
| --- | --- |
| `WorldBuilder/CellGraph/IHTerrainCellGraphTypes.h` | `EIHCellFeature`, `FIHTerrainCell`, `FIHTerrainCellGraph` |
| `WorldBuilder/CellGraph/IHTerrainCellGraphGenerator.h/.cpp` | `FIHTerrainCellGraphGenerator::BuildGraph()` — jittered-grid Voronoi cell graph via UE5.8-native `FDelaunay2`, adjacency derived from Delaunay dual |
| `WorldBuilder/CellGraph/IHTerrainCellGraphTests.cpp` | `InvisibleHand.WorldBuilder.TerrainCellGraph.BasicBuild` — **PASS** |
| `WorldBuilder/CellGraph/IHTerrainCellDiffusion.h/.cpp` | `AddHill`, `AddRange` (Azgaar-style BFS power-law diffusion), `Smooth`, `ClassifyLandWater`, `ComputeCoastalMetadata` (haven/harbor), `TraceCoastlineLoops` (direct cell-boundary coastline trace — structurally immune to raster sawtooth) |
| `WorldBuilder/CellGraph/IHTerrainCellDiffusionTests.cpp` | `InvisibleHand.WorldBuilder.TerrainCellGraph.BasicIslandShape` — **PASS**; `.TroughCarving` — **PASS** |

Run them yourself:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\Projects\ClaudeProjects\IH_WB_Demo002\IH_WB_Demo002.uproject" -game -nullrhi -nosound -unattended -nosplash -log `
  -ExecCmds="Automation RunTests InvisibleHand.WorldBuilder.TerrainCellGraph;Quit"
```

This is real, working, tested code — the actual deliverable of Phase 1's rewrite. Do not re-derive it; extend/consume it.

## 4. What's broken and de-prioritized: the dev preview actor

`AIHTerrainCellGraphPreviewActor` + `ih.PreviewTerrainCellGraph [seed]` console command was built purely as a convenience so a human could eyeball the cell-graph output in PIE before it's wired into the real pipeline. **It has never once rendered visibly**, across 7 rounds this session, each of which fixed a real, independently-confirmed bug:

| # | Round | Bug found | Fix |
| --- | --- | --- | --- |
| 1 | Findability | Marker/camera missing entirely | Auto-snap camera + billboard marker + coordinate logging |
| 2 | Marker invisible | `UBillboardComponent` defaults `bHiddenInGame=true` (confirmed in engine source) | `SetHiddenInGame(false)`; scale 60→85 (~1,000 m, ~20x Dreadnought hull) |
| 3 | Camera drift | Oblique offset camera + `ih.CameraTopDown` chaining stranded the view off-patch | Zero-XY-offset camera snap, pitch -80°→-55° |
| 4 | Origin collision | Patch spawned at the exact same (0,0) as the project's own "Story Stick" dev landmark | Patch offset to (+2000, +2000) m |
| 5 | Marker placement | Billboard can't rotate to "point at" anything (always camera-facing, by definition) | Marker repositioned north of patch + permanent gold connector line to patch center |
| 6 | **Mesh itself still invisible** | Logged mesh bounds/triangle count (`tris=6550 verts=9912`, bounds centered exactly on `200000,200000`) proved the data was correct — yet nothing rendered | Diagnostic bright-pink `#F6339A` `UMaterialInstanceDynamic` added; hypothesized backface culling from undocumented `FDelaunay2::GetVoronoiCells` winding; fixed to choose winding per-triangle for a guaranteed +Z normal |
| 7 | **Still invisible** | Same tri/vert/bounds numbers, still FAIL | Full rendering-API swap: `UDynamicMeshComponent`/`SetMesh` → `UProceduralMeshComponent`/`CreateMeshSection` — the exact API `AIH_WB_IslandActor` uses successfully for every real island (`IslandMesh`, `ShelfMesh`, etc.) |

**Result after all 7 rounds: still FAIL.** Empty ocean, no pink patch, connector line and marker both visible and correctly placed, mesh data proven correct twice over (two different rendering components), yet nothing renders.

**One real, unexplored lead:** `MakeDiagnosticPinkMID()`'s `UMaterialInstanceDynamic*` return value was never logged/confirmed non-null. If `LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))` silently fails in this runtime context, `SetMaterial` is never called and the mesh falls back to whatever UE's default material does — untested. Worth a one-line log if anyone picks this back up, but **not recommended as the next session's priority** — see §5.

## 5. A candidate theory of the fix — a starting hypothesis, not a mandate

**The user's explicit direction for next session: review, analyze, and learn this handoff and the underlying code first, then determine your own solid, simple course to road-test the native GeometryCore module (§2) and jump-start forward progress. Re-applying/fixing the visualization tool (§4) is not required.** What follows is one well-reasoned candidate path from this session's analysis, offered as a starting point for that review — not a fixed instruction to execute blindly.

The preview actor was always a side tool, not the deliverable. The actual fix for the original sawtooth-coastline defect (root-caused in the prior session: `ApplyTroughCellFrontier`'s 4-neighbor-only BFS ring flood-fill in `IHHeightfieldCoastGenerator.cpp:299-498` bakes Manhattan-diamond boundaries into the heightfield before any smoothing or extraction ever runs — see the plan file's Addendum 4) is to replace that legacy generation call with the now-complete cell-graph pipeline from §2.

One concrete path worth evaluating: **`AIH_WB_IslandActor` already has a fully proven `UProceduralMeshComponent`/`CreateMeshSection` rendering pipeline** — it's what renders `IslandMesh`, `ShelfMesh`, `SandApronMesh`, `ContourRibbonMesh`, and `FeatureRibbonMesh` in every screenshot from this entire project. A representative call: `IH_WB_IslandActor.cpp:1733`, `Mesh->CreateMeshSection(SectionIdx, MeshVerts, TierTris, Normals, UV0, DummyColors, Tangents, true)`. `AIH_WB_IslandActor` is spawned at runtime via `World->SpawnActor<AIH_WB_IslandActor>()` (`IH_WB_Demo002GameMode.cpp:1009`) — the same spawn mechanism the broken preview actor also used, which is why spawn timing was never the differentiator; the component/API choice was.

Feeding `FIHTerrainCellDiffusion::TraceCoastlineLoops`'s output through `AIH_WB_IslandActor`'s existing `CreateMeshSection` calls, replacing the legacy heightfield/`ApplyTroughCellFrontier` path, would inherit proven rendering, materials, and DEV View toggle integration automatically, and would make the entire §4 mystery moot rather than requiring it be solved. But the next session should form its own judgment after reviewing the code fresh, rather than treat this as the only option.

## 6. Human gates (carried over from the original Phase 0/1 plan, unaffected by §4)

| Gate | Status |
| --- | --- |
| Phase 0 fork/rename, UI preserved | **PASS** (confirmed in PIE, 2026-08-12) |
| Sawtooth coastline root cause identified | **PASS** — `ApplyTroughCellFrontier` 4-neighbor BFS, confirmed via code read, not guessed |
| Phase 1 cell-graph algorithm (generation/diffusion/classification/trace) | **PASS** — 3 automation tests |
| Cell-graph visually confirmed in PIE | **FAIL / deferred** — see §4; not required to proceed to §5's integration |
| Cell-graph wired into `AIH_WB_IslandActor`'s live pipeline | **Not started** — next session's primary task |
| Sawtooth defect confirmed fixed on BRICK2/ABBEY3-Toledo | **Not started** — depends on the above |

## 7. This session's preview-actor work — committed as a tagged dev tool

```
M Source/IH_WB_Demo002/WorldBuilder/CellGraph/IHTerrainCellGraphPreviewActor.cpp
M Source/IH_WB_Demo002/WorldBuilder/CellGraph/IHTerrainCellGraphPreviewActor.h
```

All 7 rounds from §4 — correct, working, headless-self-tested diffusion/marker/camera logic, with the one open rendering question left as-is. Committed at the user's explicit request, tagged in the commit message as a dev tool kept for possible future use, not as a resolved feature. It is **not** on the critical path for next session (§5) — kept for reference/possible reuse only, not something that needs re-fixing before other work proceeds.

## 8. Next-session to-do list

1. **First:** review, analyze, and learn this handoff and the current code state — then determine your own solid, simple course to road-test the native GeometryCore module (§2) and jump-start forward progress. This is the user's explicit preference; do not default to executing §5's candidate path without first forming your own view.
2. Re-applying or fixing the visualization tool (§4) is explicitly **not** required as a prerequisite.
3. Whatever course is chosen, verify against `BRICK2` (positive control) and `ABBEY3` Toledo (original sawtooth-arm example) — compare coastline character before/after.
4. Optional, not blocking: revisit §4's one open lead (log whether `MakeDiagnosticPinkMID`'s MID is null) only if there's spare appetite and it seems relevant to the chosen course.
5. Keep the discipline that held up all session: compile → headless timeout-guarded self-test → only then ask for PIE review. Caught two real infinite-loop hangs earlier this project before they ever reached the user.

## 9. Do not

- Do not keep debugging `AIHTerrainCellGraphPreviewActor`'s rendering in isolation — see §4/§5.
- Do not touch `IH_Cube2FlyPlayerController`'s canonical fly-camera pitch clamp/egress behavior (`IH-DEC-014`).
- Do not reintroduce Cove:Harborage:Firth ratio enforcement (retired, `IH-DEC-025`) — the unrelated LOW:HIGH:VOLC 3:2:1 topology-type weighting stays active (`IH-DEC-018`).
- Do not generate lakes/inland water before Hydrology (`IH-DEC-019`).
- Do not skip the headless self-test step before requesting a PIE grab.

## 10. Code pointers

- Cell graph: `Source/IH_WB_Demo002/WorldBuilder/CellGraph/IHTerrainCellGraphGenerator.cpp`
- Diffusion/classification/trace: `Source/IH_WB_Demo002/WorldBuilder/CellGraph/IHTerrainCellDiffusion.cpp`
- Automation tests: `IHTerrainCellGraphTests.cpp`, `IHTerrainCellDiffusionTests.cpp` (same folder)
- Legacy sawtooth root cause: `Source/IH_WB_Demo002/WorldBuilder/CoastGeneration/IHHeightfieldCoastGenerator.cpp:299-498` (`ApplyTroughCellFrontier`)
- Proven rendering pattern to reuse: `Source/IH_WB_Demo002/InvisibleHand/IH_WB_IslandActor.cpp:1733` (`CreateMeshSection`), `:1790-1815` (component construction)
- Canonical decision: `D:\Codex Folder\IH - Protocols\IH_Canonical_Decisions.md` — `IH-DEC-028`
- Full session narrative (all 12 addenda, includes original Azgaar pipeline research + Task 2/3/4 understanding): `C:\Users\lynnd\.claude\plans\sparkling-crafting-yeti.md` (Claude Code plan file, prior session)

## 11. Opening script (paste into new agent chat)

```text
You are continuing Invisible Hand World Builder Phase 1 at:
D:\Projects\ClaudeProjects\IH_WB_Demo002\IH_WB_Demo002.uproject (UE 5.8)

Read first (in order):
1. Content/InvisibleHand/IH_WB_Demo002_Handoff_2026-08-13.md (this handoff)
2. D:\Codex Folder\IH - Protocols\IH_Canonical_Decisions.md - IH-DEC-028
3. Source/IH_WB_Demo002/WorldBuilder/CellGraph/ (IHTerrainCellGraphGenerator,
   IHTerrainCellDiffusion, IHTerrainCellGraphPreviewActor + their .cpp files)

Status: Phase 1's Azgaar-style cell-graph terrain substrate (jittered-Voronoi
graph, Hill/Range/Trough diffusion, land/water classification, direct
cell-boundary coastline trace) is COMPLETE and verified via 3 passing
automation tests (InvisibleHand.WorldBuilder.TerrainCellGraph.BasicBuild /
BasicIslandShape / TroughCarving) - this is real, working, tested code. It
runs entirely on UE5.8's built-in UE::Geometry::FDelaunay2 (core
GeometryCore module, no third-party library) - see handoff section 2 for the
full finding and the Option A / cell-density design decisions made from it.
Read that section first; it is the actual forward momentum of this fork.

The dev preview actor (ih.PreviewTerrainCellGraph console command) that was
supposed to visualize it in PIE has an unresolved rendering bug: the mesh
data is provably correct (bounds/triangle-count logged and verified twice,
across two different rendering components: UDynamicMeshComponent and
UProceduralMeshComponent) but has never once rendered visibly, across 7
rounds of independently-fixed, verified bugs (marker visibility, marker
scale, camera framing, world-origin collision, marker placement, triangle
winding, full rendering-API swap). It's committed and tagged as a dev tool
for possible future reference, but it is NOT on your critical path.

USER'S EXPLICIT DIRECTION FOR THIS SESSION: review, analyze, and learn this
handoff and the current code first - then determine your OWN solid, simple
course to road-test the native GeometryCore module (handoff section 2) and
jump-start forward progress. Re-applying or fixing the visualization tool
above is explicitly NOT required before doing this. Do not just execute a
prior session's plan on autopilot - form your own judgment after review.

One candidate path from the prior session's analysis (handoff section 5,
offered as a starting point, not a mandate): wire the cell-graph's output
directly into AIH_WB_IslandActor's EXISTING, PROVEN UProceduralMeshComponent/
CreateMeshSection rendering pipeline (IH_WB_IslandActor.cpp:1733 is a
reference CreateMeshSection call; IslandMesh/ShelfMesh are the proven
components) - replacing ApplyTroughCellFrontier's 4-neighbor BFS trough
carving (IHHeightfieldCoastGenerator.cpp:299-498, confirmed root cause of
the original sawtooth-coastline defect) with the cell-graph's
TraceCoastlineLoops output. This sidesteps the preview-actor mystery
entirely rather than solving it. Evaluate this against your own read of
the code before committing to it.

Whatever course you choose, verify against BRICK2 (positive control) and
ABBEY3/Toledo (original sawtooth example).

Discipline that held up well all last session: compile -> headless
timeout-guarded self-test (UnrealEditor-Cmd -game -nullrhi -ExecCmds=...
wrapped in a PowerShell script with Start-Process + WaitForExit +
Stop-Process fallback) -> THEN ask for PIE review. Two infinite-loop
hangs earlier in the project were caught this way before reaching PIE.
```
