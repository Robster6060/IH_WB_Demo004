# Invisible Hand — Structure Placement Canon (B Build)

**Status:** Canon — adopted 2026-06-01  
**Related:** `InvisibleHand_TownGrid_Prototype_Path.md` · `InvisibleHand_UI_Architecture.md` · `DT_BuildPaletteItem` · `AIH_StructurePlacementActor`

---

## 1. Canon statement

> Players in **Grand Architect** may drag structures from the **Build** palette onto island terrain **without first placing a town grid**. The drop records **build intent** at a world location subject to sector acreage, zone policy, and economy/technology **prerequisites**. When prerequisites are met, the game **procedurally realizes** the structure (construction, mesh, and registry). Town grids remain the tool for planned, parcel-zoned settlements and bake-to-roads workflow; they **enhance and constrain** structure placement but **do not gate** island-level placement.

---

## 2. Player workflow (intent vs. execution)

| Step | Player | Game |
|------|--------|------|
| 1 | Drag structure tile from **B Build** | Select build order from `DT_BuildPaletteItem` |
| 2 | Move ghost over island | Preview footprint, zone hint, terrain fit, prerequisite feedback |
| 3 | Release on island | **Commit build intent** (world XY, yaw, itemID) |
| 4 | Prerequisites satisfied | **Procedural realization** — construction time, mesh, roads stub, registry |
| 5 | Prerequisites not satisfied | **Planning ghost only** — intent saved; no spend, no final build (see §4) |

**Town grid (G)** and **Build (B)** are complementary tools:

| Path | When | Role of grid |
|------|------|----------------|
| **Grid-first** | Planned town, mosaic, charter bake | Parcels, zones, ROW, Bake Grid → roads/lots |
| **Island-direct** | Pioneer scatter, docks, farms, pre-grid settlement | **None required** at drop time |

---

## 3. Locked design choices

These decisions are **canon** unless explicitly revised in a later design pass.

### 3.1 Drop without funds

| Choice | **Yes — planning ghost only** |
|--------|-------------------------------|
| Behavior | Player may place intent on the map even when treasury cannot afford the build. |
| Visual | Planning ghost / blueline occupancy marker; not a finished structure. |
| Economy | No debit until prerequisites pass and construction starts (or completes — implementation detail TBD). |
| UI | Clear “insufficient funds” (or materials/workers/tech) on ghost and in build queue panel. |

### 3.2 No town grid present

| Choice | **Yes — auto-create implicit single-parcel zone** |
|--------|---------------------------------------------------|
| Behavior | On structure drop outside any `AIH_TownGridManager` footprint, the game creates an **implicit single-parcel** occupancy at the site. |
| Zoning | Parcel inherits **zone from structure** (`zoneRequired` on palette row, e.g. RES, WWF). |
| Data | Island-level structure registry until a formal grid absorbs the site (§3.3). |
| Acreage | Footprint consumes **sector acres** per island budget (1 sector = 1 acre canonical). |

### 3.3 Grid added later (merge under existing structures)

| Choice | **Yes — merge under existing structures** |
|--------|---------------------------------------------|
| Behavior | Player may drop a town grid template over or adjacent to structures already placed island-direct. |
| Reconciliation | Bake / graph merge **retains** existing structures; parcels adjust around them rather than deleting them. |
| Zone policy | Structures that do not match new parcel zoning may be wrapped as **`SPD`** (special district) parcels **around the existing footprint** where needed, rather than forced relocation. |
| Editability | Post-bake rezone / ROW override / raze remain supported per Town Grid prototype path. |

### 3.4 World Builder vs. Grand Architect

| Mode | Structure D&D | Prerequisites |
|------|----------------|---------------|
| **World Builder — production** | ❌ No B Build in shipping WB (`bWBDevScaleReviewPalette = false`) | N/A |
| **World Builder — dev scale review** | ✅ **Temporary** — structure + grid D&D for **acreage calibration** (Merchantman, sector quota) | **Relaxed / bypass** — instant or placeholder mesh for scale review only |
| **Grand Architect — production** | ✅ Full B catalog per unlocks | **Full prerequisite stack** — funds, workers, materials, technology, jurisdiction, zone, sector acres |

**Coastal / WWF / bluff placement (2026-08-10):**

| Mode | Rule |
|------|------|
| **WB DEV (now)** | Grand Theater (and other scale-review structures) may D&D onto **any ASL or slope** on dry or wet IslandMesh or WWF |
| **Grand Architect / later phases** | Only **pier**, **cofferdam**, **military walls**, and **sea walls** are permitted for D&D on coastal bluffs/cliffs and wet WWF locations |

**Removal trigger (unchanged):** when map **Sector = acreage quota** is finalized, remove **G/B/C/D from World Builder PIE** entirely — not merely hidden.

---

## 4. Prerequisite stack (Grand Architect)

Grand Architect applies the full gate before procedural realization. Typical dimensions:

| Prerequisite | At drop (preview) | At build (execution) |
|--------------|-------------------|----------------------|
| Funds / cash | Warn; allow planning ghost | Debit on start or completion |
| Workers / labor | Show crew / duration need | Job queue |
| Materials | Show shortages | Stockpile consumption |
| Technology / research | Palette + drop gate | `DT_BuildPaletteItem` unlock fields |
| Jurisdiction / charter | Phase + `phaseMin` | Charter milestones |
| Zone / land use | `zoneRequired` vs. parcel or implicit parcel | Enforce on bake |
| Sector acreage | Footprint vs. remaining island acres | Reserve on commit |
| Terrain | Slope / wet / cliff | Door-threshold align (`AIH_StructurePlacementActor`) |

**World Builder dev review** may skip or stub this stack; **Grand Architect** does not.

---

## 5. Procedural realization (target gameplay)

“Procedural generation” after prerequisites means the sim **realizes** the build order, not only spawning a dev placeholder:

1. **Validate** — sector, zone, prerequisites, collisions, charter  
2. **Reserve** — acreage / parcel occupancy (implicit or grid-backed)  
3. **Schedule** — construction duration, worker assignment  
4. **Generate** — mesh variant, foundation, access road stub, props (PCG / build script)  
5. **Register** — persist to island save (`DT_TownGridParcelGraph` when in grid; else island structure registry)

**P1C08 dev** currently spawns `AIH_StructurePlacementActor` (placeholder mesh or engine cube) immediately for tooling; GA prerequisite and queue layers are **implementation follow-on**.

---

## 6. Data & code anchors

| Asset / type | Role |
|--------------|------|
| `DT_BuildPaletteItem` | `paletteTab=Build`, `interactionType=DropActor`, `zoneRequired`, unlock fields |
| `AIH_StructurePlacementActor` | Door-origin terrain align; dev placeholder mesh paths |
| `UIH_BuildPaletteSubsystem` | Drag ghost, drop intent, preview actor, **sticky** placement cache |
| `AIH_TownGridManager` | Optional grid; bake commits parcel graph |
| Island sectors | `FIHIslandSectorRow` — acre budget per island |

### 6.1 Placement & Z sync (implementation — P1C08 affirmed 2026-06-02)

**Task start:** See **`InvisibleHand_Placement_Z_Sync_Checklist.md`**.

| Rule | Detail |
|------|--------|
| **Sticky ghost** | Last valid island `drawCenter` + `actorOrigin` kept while cursor over water/sky |
| **Drop** | Commit at `StickyBuildDragActorOrigin` — not a second screen resolve |
| **Z gate** | Reject island samples below water plane + clearance (~40 cm) |
| **Visual vs mesh** | Grid/debug at `DragGhostDrawCenterWorld`; actor at `actorOrigin` (often different Z) |
| **Logs** | Compare `drag ghost VALID` with `drop — placed` / `stickyOrigin` vs `actorAt` |

---

## 7. Cross-references

- **Placement + Z checklist (task start):** `InvisibleHand_Placement_Z_Sync_Checklist.md`  
- Grid-first workflow: `InvisibleHand_TownGrid_Prototype_Path.md` §7  
- Tab phase gating: `InvisibleHand_UI_Architecture.md` §2  
- Build palette runtime: `UI/BuildPalette/README.md`  
- Catalog tiles: `InvisibleHand_RightBuildPalette_Catalog.md`
