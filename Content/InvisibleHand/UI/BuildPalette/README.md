# Build Palette UI

**Catalog:** `InvisibleHand_RightBuildPalette_Catalog.md`  
**Structure placement canon:** `InvisibleHand_StructurePlacement_Canon.md`  
**Placement + Z sync (read at task start):** `../InvisibleHand_Placement_Z_Sync_Checklist.md`  
**Synopsis (Word-ready):** `../InvisibleHand_Blueline_TownGrid_Session_Synopsis.md`  
**Town grid path:** `InvisibleHand_TownGrid_Prototype_Path.md`

---

## Runtime (P1C08 — v17+)

| C++ class | Role |
|-----------|------|
| `UIHTownGridDataSubsystem` | Loads `DT_BuildPaletteItem`, `DT_TownGridTemplate`, harmonic blocks |
| `UIH_BuildPaletteSubsystem` | GameInstance — Prepare/Ensure, fly-out toggle, drag payload |
| `UIH_BuildPaletteHostWidget` | Viewport-anchored overlay: NativePaint tab strip + accordion fly-out |
| `AIH_TownGridManager` | Palette drop → blueline grid (replaces M1 stub) |

**Layout:** `[ 220px fly-out ] [ 3px gap ] [ 36px G/W/B/C/D strip ]` — `RightHUDInset=20`, `TabStripTopMargin=145`.

**Fly-out UI pattern:** accordion with nested headers (see UI Architecture doc).

---

## Phase gating (canon)

| Phase | Tabs |
|-------|------|
| **World Builder** | **W** only (production); **G/B/C/D** when `bWBDevScaleReviewPalette` (temporary scale review) |
| **Grand Architect+** | **G/B/C/D** active; **W** dimmed or absent |

Dev-scale review uses Merchantman + town grids + structures to calibrate sector acreage before canon lock.

---

## PIE hotkeys

| Key | Action |
|-----|--------|
| **G / W / B / C / D** | Toggle tab fly-out |
| **M** | Minimap |
| **P** | Pause / game speed (existing) |
| **Escape** | Cancel drag or close fly-out |
| **F** or **Ctrl+F** | Fuzzy Finder — focus search (**▶ F Find**) |

Click tab letters on strip — same as keys. Double-click island — selection (unchanged).

---

## Drag-drop

1. Drag tile from fly-out → focus-blue ghost (`#66BFFF`)
2. Ghost follows terrain line trace
3. LMB on terrain → spawns payload (`AIH_TownGridManager` for grid templates; `AIH_StructurePlacementActor` for Build)

**Structure canon:** Island-direct B Build does **not** require a prior town grid. GA records build intent, then procedurally realizes after prerequisites; insufficient funds → **planning ghost only**. See `InvisibleHand_StructurePlacement_Canon.md`.

`BindKey(G)` + tick poll fallback in `AIH_Cube2FlyPlayerController`.

---

## Fuzzy Finder (planned)

Top-center search → dropdown of all `DT_BuildPaletteItem` matches → open tab + accordion path + highlight tile. See UI Architecture doc.

---

## Data

Import `Content/InvisibleHand/Data/DataTables/DT_BuildPaletteItem.csv` → DataTable asset. Runtime CSV fallback via `UIHTownGridDataSubsystem`.

**Icon path:** `Content/InvisibleHand/UI/Icons/` — 128×128 PNG per catalog.

---

## Milestones

| Milestone | Status |
|-----------|--------|
| M0 Data + subsystem | ✅ |
| M1 C++ shell + drag | ✅ |
| v16–v17 Viewport overlay + tab strip paint | ✅ |
| Accordion tree from `categoryPath` | 📋 Next |
| Phase gating WB vs GA | 📋 |
| Zone Brush + picker | 📋 |
| Bake Grid (button + charter auto-bake) | 📋 |

---

## Style

`IH_BuildPalettePanelStyle` + `IH_P1C08_DevPanelStyle` · focus blue `#66BFFF`.

Optional later: `WBP_IH_RightBuildPalette` skin over C++ host.
