# Invisible Hand — World Builder Phase Order (Canon)

Accepted 2026-09-19; architecture corrected 2026-09-19 (same day, second pass — see "Architecture
correction history" at the bottom). This is the single authoritative source for World Builder
phase order — supersedes the informal/partial sequencing in
IH_WB_Demo002_Phase1_PlanArchive_2026-08-13.md and ACCEPTANCE_GATES.md (both flagged as
historical, see their own pointer notes).

## Phase order

1. **RealmSeed** — `AAAAA#` seed input; drives the master seed for all downstream generation.
2. **Island relocation + rotation → First Bake.** Player may reposition/rotate each generated
   island as a whole (not its terrain shape). First Bake then: (a) constructs an `FDynamicMesh3`
   from the base terrain's existing vertex/triangle data, (b) runs Geometry Script smoothing on it
   (this is also the fix for the low-poly triangular-tiling look), and (c) assigns the result to a
   `UDynamicMeshComponent` with collision enabled. **No Nanite** — see "Bake mechanism" below for
   why.
3. **Terrain Stamp Actors — drag/drop + transform.** Player-placed landform stamps (Mesa, Hill,
   Cliff, etc.) added on top of the First-Baked base, using the existing collision-query surface
   sampling (`UIH_P1C07_IslandCollisionSubsystem::TrySampleIslandSurfaceAtXY`), which queries
   collision generically via line trace and works identically whether the base is a
   `UProceduralMeshComponent` or a `UDynamicMeshComponent` — no change needed there.
4. **Latitude Selection → determines Biome Classification.** Player picks Nordic/Temperate/
   Tropical; this drives `DT_BiomeTagZoneExemption`'s zone-eligibility filtering. Player has no
   other way to alter Biome Classification.
5. **Hydrology Phase → Final Bake.** Adds springs/rivers/lakes; the carving algorithm itself
   (flow accumulation, channel width/depth, slope-driven rapids/ford/waterfall classification) is
   its own dedicated future round — deferred, not cut; it is an accepted phase of the final IH WB
   workproduct. Final Bake boolean-unions all currently-placed Stamp meshes into the base
   `FDynamicMesh3` (Geometry Script `ApplyMeshBoolean`), carves Hydrology's geometry, optionally
   applies a decimation pass for performance, and reassigns the result to the `UDynamicMeshComponent`.
   Still no Nanite. This is the point at which the island's mesh becomes **irrevocable** in the
   strong sense — see "On the word 'irrevocable'" below, since this term is used for three
   genuinely different things in this doc and they must not be conflated.
6. **Sector Fabric** — the ~3.4M one-acre polygon overlay (Contour-Guided Sector Fabric,
   IH-DEC-023) is applied as a derivative layer on the Final-Baked mesh. Its authoritative bake
   was already canon-specified (2026-08-13 archive doc) to happen only after Hydrology; this
   phase order doesn't change that, just makes First/Final Bake explicit around it.

## "Bake to Preview Progress" — player-facing preview mechanic

At any point during fabrication, the player may press **Bake to Preview Progress**: it runs the
same bake operation (smoothing / Stamp merge, whatever is applicable to the current phase) against
the fabricator's *current* state and lets the player play/explore that result live, in a real
gameplay session. The preview cannot be edited or added to while active. Ending the preview session
**permanently and irrevocably discards it** — see "On the word 'irrevocable'" below — and returns
the player to the active, fully-editable fabricator session exactly where they left off. The player
may repeat this as many times as they like; nothing accumulates on disk between attempts, because
baking never creates a persistent asset in the first place (no `UStaticMesh`, no saved package) —
discard is just clearing an in-memory mesh. An optional **Save As** lets the player export a
preview to an off-game file location if they want to keep one; this is the one opt-in exception to
"nothing persists," and see "Multiplayer / Host-Authoritative Game Map" below for what that export
should actually contain.

**Progress Bar UI** (new dedicated panel, not folded into the DEV View panel, since this is a real
player-facing feature): a `UProgressBar` (continuous fill, not six hard-snapped segments) under a
`UHorizontalBox` of six stage labels — RealmSeed / Isle Move+Rotate / Terrain Stamps / Latitude /
Hydrology / Sectors — with the "Bake to Preview Progress" button docked to the right. Built via
this project's existing C++ `WidgetTree->ConstructWidget<T>()` convention
(`IH_P1C08_DevViewWidget.cpp`), no separate Blueprint widget asset.

**Full IH gameplay requires all World Builder phases completed.** "Bake to Preview Progress" is a
preview/checkpoint mechanic *within* an in-progress fabricator session — it is not an alternate way
to skip a phase or begin real play early.

## Bake mechanism: no Nanite, no native World Partition — confirmed architectural mismatch, not a missing feature

This fabricator is a **live, in-game feature used by real players in the actually-shipped game**,
not a developer/Editor-only content-authoring tool — confirmed explicitly, since it changes which
UE5 systems are even eligible.

- **Nanite** requires a pre-processed, immutable cluster hierarchy built once, offline, by the
  Unreal Editor's toolchain. `UStaticMesh::Build()` (the function that produces this data, and the
  pipeline Nanite generation runs through) is wrapped in `#if WITH_EDITOR` in
  `Engine/Source/Runtime/Engine/Classes/Engine/StaticMesh.h` — it is not compiled into a
  packaged/shipped game at all. More fundamentally, **no live/editable mesh representation
  supports Nanite, in the Editor or out** — checked `UDynamicMeshComponent`
  (`Engine/Source/Runtime/GeometryFramework/Public/Components/DynamicMeshComponent.h`) directly,
  zero Nanite references, same as `UProceduralMeshComponent`. This is what Nanite is *for*
  (pre-shipped static content), not an engineering gap.
- **Native `UWorldPartition`** streams pre-cooked per-cell level data baked in at cook time, before
  the game ships. A realm a player generates from a seed *after* the game is already in their
  hands has no cooked cell files to stream — there is nothing for WP's mechanism to attach to.
- **Replacement, runtime-legal, already-partially-adopted**: `UDynamicMeshComponent` +
  `GeometryScriptingCore` for baking (`GeometryFramework`/`GeometryCore` are core Runtime engine
  modules, already linked in this project's `Build.cs`; `GeometryScriptingCore` is a Runtime-type
  plugin module, not yet enabled — requires adding `"GeometryScripting"` to the `.uproject`
  Plugins array and `"GeometryScriptingCore"` to `Build.cs`, both additive/low-risk). For
  streaming: a **custom distance-based island-loading system**, generalizing this project's own
  PGC groundcover proximity-streaming pattern (`IH_WB_IslandActor.cpp`'s
  `RefreshPGCGroundcoverProximity`/`BuildPGCEligibilityCache`) to island/region granularity —
  already proven to work for exactly this class of problem (content that doesn't exist until a
  player generates it).
- This design goal is not novel or unproven — established genre precedent (Age of Empires' live
  random-map generation, Minecraft/RimWorld/Valheim's live-generated and live-deformed worlds,
  Cities: Skylines' in-game Map Editor) has solved "a player shapes a persistent, live-generated
  world inside their own shipped game" for decades, using heightmap/voxel/dynamic-mesh techniques
  — not Nanite or native World Partition, neither of which predate or target this use case.

## Multiplayer (future phase) — Host-Authoritative Game Map

Not built this round; recorded here so the Bake architecture is designed compatibly with it from
the start, rather than needing a redesign later.

**Decision**: when Multiplayer ships, the game map must be **Host-Authoritative**, not
independently regenerated per client. One authority (a host player, or a dedicated server) runs
Bake once; the actual resulting mesh/collision *data* is distributed to all other clients, so
every player has the literally-identical bytes.

**Why not have every client regenerate independently from a shared seed+recipe instead** (the
lighter-weight option)? This project's entire generation pipeline is floating-point end to end —
the Voronoi/Delaunay cell graph, Perlin-based coastline shaping, and Geometry Script's
smoothing/boolean-merge operations. Different CPUs/compilers/instruction sets can produce tiny
floating-point rounding differences that compound across a multi-step pipeline into a visibly
different map between two players' machines. For a genre that includes **military combat** — where
whether a unit is on land or in water, or exactly where a collision boundary sits, can decide a
fight — that drift is a fairness/desync bug, not a cosmetic nit. Recipe-replay (seed + recorded
fabricator inputs) remains fine for purely cosmetic, non-collidable elements where per-client
variation wouldn't affect fairness; it is not relied on for terrain/collision.

**Why today's Bake design is already compatible**: Bake is a pure function over explicit,
well-defined inputs (retained fabricator state — island transform, placed Stamp list, Hydrology
parameters — in, a derived mesh out), with no hidden per-client randomness. This is exactly the
shape needed for host-authoritative distribution later: the host runs the same function once, and
its *output* is what gets sent, rather than trusting independent per-client recomputation.
Practically, distributing that output will need a custom bulk data-transfer step (Unreal's
standard Actor-property replication is built for small, frequent updates, not one-time bulk mesh
blobs) — sized and designed as its own dedicated plan when Multiplayer is scheduled, not now.

**"Save As" export, reconsidered for this**: an exported realm intended to be shared with another
player should capture the **fabricator recipe** (RealmSeed + recorded Stamp placements + Hydrology
parameters), not a raw dump of baked mesh geometry — smaller, durable across engine/version
changes, and consistent with the Host-Authoritative model above (the receiving side either
regenerates cosmetically-safe content from the recipe, or — for a real multiplayer match — receives
the host's authoritative baked data directly, never trusting its own independent regeneration for
anything gameplay-critical).

## On the word "irrevocable" — three distinct meanings, do not conflate

This doc (and earlier planning dialogue) uses "irrevocable" for three genuinely different things.
Future edits must keep these distinct rather than treating "irrevocable" as one uniform concept:

1. **A discarded "Bake to Preview Progress" preview is irrevocably gone** — once the player ends
   that preview session, that specific preview instance cannot be recovered (by design, to avoid
   file accumulation). This says nothing about the underlying fabricator state, which is
   untouched and remains fully editable — the player returns to it immediately.
2. **First Bake locks the base island terrain's shape** (position, rotation, the smoothed
   procedural geometry) irrevocably for that island — but Stamps can still be added on top
   afterward, so the island *as a whole* is not yet fully locked.
3. **Final Bake is the true, whole-island irrevocable lock** — after Final Bake, nothing about
   that island (base shape, Stamps, Hydrology carving) can be edited for the remainder of that
   playthrough. This is the only one of the three that means "this island's map is now completely
   fixed."

## Key architecture decisions (summary)

- **Unitary mesh scope: one per ISLAND**, not one for the whole realm — needed for the custom
  distance-based streaming system to have real per-island granularity to load/unload.
- **No Nanite, ever, in this pipeline** — architecturally incompatible with runtime
  player-generated content, not an editor-availability gap (see "Bake mechanism" above).
- **No native `UWorldPartition`** — requires pre-cooked, edit-time content this feature's whole
  premise doesn't have; replaced by a custom distance-based loader.
- **Host-Authoritative Game Map** for the future Multiplayer phase — see dedicated section above.
- **Player cannot edit**: the Azgaar-style cell graph → coastline/heightfield → base IslandMesh
  shape itself (only the island's position/rotation as a whole, at step 2), or Biome
  Classification directly (only indirectly, via Latitude at step 4).
- **Merging Stamps into the base mesh** at Final Bake likely needs a true CSG boolean union
  (Geometry Script `ApplyMeshBoolean` / `PlanarCut`), not a simple "Merge Actors" draw-call
  combine. Flagged as a real technical risk (self-intersection/non-manifold results on a coarse
  procedural mesh plus authored stamp meshes) needing its own small prototype/validation before
  implementation.

## Status

Phase-order and bake architecture (DynamicMeshComponent-based, no Nanite/native WP): accepted
2026-09-19. Not yet built — each is its own future round: Hydrology's carving algorithm, the
Latitude selector UI, the `GeometryScripting` plugin enablement + First Bake C++ implementation,
the World Builder Progress Bar / "Bake to Preview Progress" UI, the custom distance-based
streaming system, and the Multiplayer Host-Authoritative Game Map distribution mechanism. All are
accepted, roadmapped phases of the final IH WB workproduct — deferred, not cut.

## Architecture correction history

- 2026-09-19, first pass: phase order (RealmSeed → First Bake → Stamps → Latitude →
  Hydrology/Final Bake → Sector Fabric) accepted with Nanite + native World Partition as the
  intended bake/streaming tech.
- 2026-09-19, second pass (same day): Nanite and native World Partition dropped after confirming
  this fabricator must run live in the shipped game, not as an Editor-only tool — both are
  confirmed architecturally incompatible with that requirement (see "Bake mechanism" above), not
  merely harder to engineer. Replaced with `UDynamicMeshComponent`/`GeometryScriptingCore` and a
  custom distance-based streaming system. "Bake to Preview Progress" and the Host-Authoritative
  Game Map (future Multiplayer) decisions added in the same pass. The phase *order* itself,
  Latitude, and the "player cannot edit base shape/biome classification" boundary were not
  affected by this correction.
