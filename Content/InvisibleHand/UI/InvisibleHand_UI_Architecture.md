# Invisible Hand — UI Architecture (canonical)

**Project:** `IH_P1C10_Azgaar` (Grand Architect path)  
**Status:** Canon — accordion fly-out, phase gating, Fuzzy Finder, icon pipeline  
**Code anchors:** `IH_BuildPaletteHostWidget`, `UIH_BuildPaletteSubsystem`, `IHInvisibleHandDesignSpec.h`

> **Task start:** Read **`InvisibleHand_Placement_Z_Sync_Checklist.md`** before changing drag-drop, viewport terrain picks, or overlay hit-test vs world placement — keep **preview XYZ**, **drop XYZ**, and **Z band** synchronized.

---

## 1. Fly-out menu pattern (canon)

**Default:** single **220px** fly-out panel left of the **36px** tab strip (`IH_BuildPalettePanelStyle`).

| Rule | Canon |
|------|--------|
| Layout | One fly-out; **no cascade panels** unless a future tab exceeds practical accordion depth (Defense catalog — optional later) |
| Sections | Accordion headers: `▶` collapsed / `▼` expanded; show count `(N)` |
| Nested paths | `categoryPath` in `DT_BuildPaletteItem` (e.g. `Build/Civil/Public`) → **nested headers inside one panel** |
| Section size | Target **≤10 draggable tiles** per leaf section; split category if larger |
| Overflow | Vertical scroll inside fly-out; unlimited rows in data |
| Data | Every leaf = one `FIHBuildPaletteItemRow`; UI generated from `paletteTab` + parsed `categoryPath` |

---

## 2. Game phases vs HUD / tabs

### Phase map

| Phase | Purpose | Tab strip |
|-------|---------|-----------|
| **World Builder (WB)** | Sculpt & **bake 2–7 island landforms** → unitary mesh actors | **W** primary; **G/B/C/D** dev-scale review only (temporary) |
| **Grand Architect (GA)+** | Town grids, structures, zones, conveyance, defense on baked land | **G/B/C/D** primary; **W** dimmed or absent |

### Tab visibility matrix

| Tab | WB (production) | WB + dev scale review | GA+ |
|-----|-----------------|------------------------|-----|
| **W** | ✅ Active | ✅ Active | ❌ Dim / absent |
| **G** | ❌ Off | ✅ Temp (scale) | ✅ Active |
| **B** | ❌ Off | ✅ Temp (scale) | ✅ Active |
| **C** | ❌ Off | ✅ Temp (scale) | ✅ Active |
| **D** | ❌ Off | ✅ Temp (scale) | ✅ Active |

**WB dev-scale review kit (temporary):** Merchantman ship, town grid templates, D&D structures on grid — used to decide buildable terrain / sector acreage quota before canon lock.

**Removal trigger:** when total map **Sector = acreage quota** is finalized, remove **G/B/C/D from World Builder PIE** (`bWBDevScaleReviewPalette = false`). Not merely hidden — WB sessions must not expose GA tools after lock.

**Filtering:** tab strip gates on **game phase**; fly-out rows gate on `levelRequired`, `phaseMin`, unlock fields on `FIHBuildPaletteItemRow`.

### Structure placement (B Build) — canon summary

Players may D&D structures onto island terrain **without** a town grid. Drops record **build intent**; **Grand Architect** runs the full prerequisite stack (funds, workers, materials, technology, zone, sector acres) before procedural realization. **Insufficient funds** → planning ghost only (no spend). **No grid** → implicit single-parcel zone from `zoneRequired`. **Grid added later** → merge under existing structures (SPD wrap parcels as needed). **WB dev scale review** → structure D&D for acreage calibration with relaxed prerequisites.

Full spec: `BuildPalette/InvisibleHand_StructurePlacement_Canon.md`.

### HUD shells (shared palette engine)

```
UIH_BuildPaletteSubsystem + UIH_BuildPaletteHostWidget (shared)
        │
        ├── WB HUD shell     → W-primary; unique pre-bake stack (top-left)
        └── GA HUD shell     → G/B/C/D-primary; W absent
```

---

## 3. Fuzzy Finder (top-center, always available)

### Purpose

Universal search over all palette candidates. Player types a filter string → dropdown of matches → select entry → **opens the owning tab fly-out** scrolled/highlighted to the matching **icon tile** for drag-and-drop.

### Behavior (canon)

| Element | Spec |
|---------|------|
| Position | **Top-center** of game viewport; does not obscure Game Speed (top-right) or minimap (bottom-right) |
| Availability | **Always** in WB (W items) and GA (G/B/C/D items); filters by current phase + unlock state |
| Input | Text field + incremental dropdown (typeahead) |
| Index source | All rows in `DT_BuildPaletteItem` (+ optional alias table for synonyms: "town hall" → Rathaus) |
| On select | 1) Switch/open correct tab fly-out 2) Expand accordion path to leaf 3) Pulse/highlight tile 4) Optional: start drag immediately on second click |
| Hotkey | **`F`** — opens Fuzzy Finder and focuses text field (label: **▶ F Find** in HUD hint) |
| Hotkey (alt) | **`Ctrl+F`** — same behavior (focus cursor inside search field) |
| Empty state | Grey hint: "Search grids, structures, stamps…" |

### Implementation notes

- Subsystem: `UIH_FuzzyFinderSubsystem` or method on `UIH_BuildPaletteSubsystem::FindAndFocusItem(FName ItemID)`
- Search keys: `displayName`, `itemID`, `categoryPath`, `tooltip`, tags column (future)
- Disabled rows appear in list **dimmed** with reason tooltip ("Requires Phase 3", "CIV zone only")

---

## 4. Zone Brush + picker sub-panel (canon)

**One drag tile** on Grid tab — **`GridTool_ZoneBrush`** (`interactionType = PaintBrush`).  
**Not** eighteen separate draggable zone tiles.

### Player flow

```
1. Drag Zone Brush onto active town grid (or select brush from G fly-out)
2. Grid enters Zone Paint mode (cursor = tint circle)
3. Zone Picker sub-panel opens (inside G fly-out accordion section "Zones")
4. Player clicks a zone swatch (RES, CIV, OPE, …) → becomes active paint color
5. LMB on parcels / drag across cells → writes zone ID to parcel graph + material tint
6. Esc or tool switch → exit paint mode
```

### Zone Picker sub-panel layout

Accordion section **`▼ Zones (18)`** — fixed **2×9** or **3×6** grid of **clickable swatches** (not drag sources):

| Element | Spec |
|---------|------|
| Swatch size | 48×48 px inside fly-out (128×128 source art downscaled) |
| Active swatch | `#66BFFF` outline + checkmark |
| Hover | Tooltip: full zone name + gameplay notes |
| Recent | Optional row: last 3 zones used |
| Brush size | Default single parcel; Shift+drag = line; optional dropdown: 1×1, 3×3, 5×5 modules |

### Data

- Active zone stored on `UIH_BuildPaletteSubsystem` or `AIH_TownGridManager`: `ActivePaintZone`
- Paint commits to `UTownGridParcelGraphComponent` zone ID map
- Icons: **18 PNG swatches** in picker only (see catalog); **1** brush tile icon

### Why one brush vs 18 drag tiles

| One brush + picker | 18 drag tiles |
|--------------------|---------------|
| Faster zone switching mid-session | Cluttered fly-out |
| Matches paint-tool mental model | Implies 18 placement modes |
| Accordion stays ≤10 **drag** items per section | Exceeds section size canon |

---

## 5. Icon drag-tile art pipeline

See **`InvisibleHand_RightBuildPalette_Catalog.md`** for the exhaustive tile list.

### Can you use a 2D screen grab of the 3D FBX?

**Yes for prototyping and parsimonious production**, with rules:

| Do | Don't |
|----|--------|
| Orthographic or fixed **3/4 isometric** camera | Perspective hero shots ( unreadable at 128px ) |
| Consistent **scale** across a tab (ship ≈ grid ≈ structure) | Variable zoom per icon |
| **Transparent** background (PNG alpha) | Busy terrain in thumbnail |
| Top-down for **grids / zones / stamps** | — |
| 3/4 iso for **structures** | — |

**Long-term:** automated FBX batch icon render via Editor Utility Widget / `SceneCapture2D` from standardized `{Icon}` socket on each mesh (see `Scripts/blender_ih_structure_export.py` for Blender → FBX pipeline).

**Near-term (canon):** **hand grabs** (screen capture of mesh or stylized art) for all P0–P2 icons.

### Blender → FBX pipeline (structures)

Copy-paste script: **`Scripts/blender_ih_structure_export.py`**

- Creates or processes mesh in Blender via **bpy**
- Ground-centered pivot, UV unwrap, optional material slots
- Exports **FBX** with UE5-friendly scale (cm) and forward axis
- Optional `{Icon}` empty for future batch thumbnail render

When structure specs are finalized, extend script per `itemID` or pass parameters at top of file.

### Standard specifications

| Property | Canon value |
|----------|-------------|
| **Primary size** | **128×128 px** (fly-out tile) |
| **Compact size** | **64×64 px** (Fuzzy Finder dropdown, optional) |
| **Format (shipping)** | **PNG**, **sRGB**, **8-bit RGBA**, straight alpha |
| **Format (source)** | PNG or **TIFF** (working art only); import to UE as `Texture2D` |
| **DPI** | **Irrelevant** for screen UI (reference 72 dpi metadata OK); pixel dimensions matter |
| **Padding** | **8px** safe margin inside 128 canvas (content in 112×112) |
| **Naming** | `ICO_{Tab}_{Category}_{ItemID}.png` e.g. `ICO_Build_Civil_Rathaus.png` |
| **UE import** | `Texture Group: UI`, `Compression: UserInterface2D`, no mip gen for crisp HUD |

### Visual states (prefer material/widget tints over duplicate art)

| State | Treatment |
|-------|-------------|
| **Normal** | Full-color icon, white/neutral border |
| **Hovered** | `#66BFFF` focus outline (`TownGridFocusOutlineBlue` family) |
| **Selected** | Thicker focus outline + slight scale **1.05×** |
| **Dragging** | Icon follows cursor; source tile at **50% opacity** |
| **Disabled / locked** | **Desaturate 70%**, opacity **0.45**, optional padlock overlay (single shared glyph) |
| **Unavailable zone** | Amber tint on drop ghost (not on tile) |
| **Dev-only** | Small **DEV** corner badge (WB scale-review items) |

---

## 6. Related documents

| Document | Scope |
|----------|--------|
| `InvisibleHand_RightBuildPalette_Catalog.md` | Exhaustive drag-tile inventory |
| `InvisibleHand_TownGrid_Prototype_Path.md` | Blueline rendering, T1–T3 nucleus, T4 rings, bake |
| `UI/BuildPalette/README.md` | Runtime harness, hotkeys |
| `TownGrid/README.md` | Manager actor, generators |
| `World/P1C08_Phased_Plan.md` | Story tank phases |
| `IHInvisibleHandDesignSpec.h` | Numeric canon (module 8 m, road classes, templates) |
