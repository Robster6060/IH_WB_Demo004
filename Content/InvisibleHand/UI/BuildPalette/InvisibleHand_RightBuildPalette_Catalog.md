# Invisible Hand — Right Build Palette Catalog

**Status:** Canon inventory for drag-tile art fabrication  
**Data:** `DT_BuildPaletteItem.csv` · `FIHBuildPaletteItemRow`  
**UI spec:** `InvisibleHand_UI_Architecture.md`

Each row needs a **`TSoftObjectPtr<UTexture2D> icon`** at `Content/InvisibleHand/UI/Icons/{Tab}/{Category}/ICO_*.png` (128×128 PNG).

**Count summary (full catalog target):** ~**135** drag tiles + **18 zone picker swatches** (non-drag) + **8** shared utility glyphs.

---

## Grid tab (G) — 24 drag tiles + zone picker

### Town Templates (5) — `Grid/TownTemplates`

| itemID | Display | Icon depiction |
|--------|---------|----------------|
| `GridTemplate_Squared` | Squared (T1) | Top-down 16×16 orthogonal blueline grid, central CIV square |
| `GridTemplate_Harmonic` | Harmonic (T2) | Top-down φ rectangles + faint spiral hint |
| `GridTemplate_Radial` | Radial (T3) | Top-down 12-spoke hub + 3 rings |
| `GridTemplate_Citadel` | Citadel (T4) | Ellipse triple wall + inner orthogonal grid |
| `GridTemplate_Valley` | Valley (T5) | Curved spine + contour cross-lanes |

### Zone Brush + picker (1 drag + 18 swatches) — `Grid/Zones`

**Drag tile (1):**

| itemID | Display | Icon depiction |
|--------|---------|----------------|
| `GridTool_ZoneBrush` | Zone Brush | Paint brush + small zone swatch chip |

**Picker swatches (18)** — click to select active paint zone; **not** separate drag tiles:

`RES` · `MRE` · `LWK` · `RET` · `COM` · `CBD` · `CIV` · `CHA` · `SPD` · `OPE` · `RAN` · `AGR` · `MME` · `LID` · `HID` · `WWF` · `TRN` · `MIL`

Each swatch: 128×128 PNG (48×48 in UI), tint color + 2-letter glyph. See `InvisibleHand_UI_Architecture.md` § Zone Brush.

### Open spline ROW tools (8) — `Grid/OpenSplines`

| itemID | Display | Icon depiction |
|--------|---------|----------------|
| `SplineOpen_Boulevard` | Boulevard | Wide dashed blue path, diagonal break grid |
| `SplineOpen_Avenue` | Avenue | Grid-aligned wide path |
| `SplineOpen_Street` | Street | Medium collector path |
| `SplineOpen_Lane` | Lane | Narrow rear lane |
| `SplineOpen_Way` | Way | Winding rural path |
| `SplineOpen_Canal` | Canal | Blue water ribbon (ROW override) |
| `SplineOpen_Aqueduct` | Aqueduct | Elevated channel profile |
| `SplineOpen_Trace` | Trace | Dotted foot path |

### Closed spline tools (4) — `Grid/ClosedSplines`

| itemID | Display | Icon depiction |
|--------|---------|----------------|
| `SplineClosed_Park` | Park loop | Closed green loop |
| `SplineClosed_Pond` | Pond | Closed blue oval |
| `SplineClosed_Wall` | Enceinte | Closed wall ring segment |
| `SplineClosed_Market` | Market yard | Closed rectangle |

### Grid utilities (6) — `Grid/Tools`

| itemID | Display | Icon depiction |
|--------|---------|----------------|
| `GridTool_MergeLink` | Merge link | Two grids + yellow link flash |
| `GridTool_WayClip` | Way boundary | Red contour scissors |
| `GridTool_EraseParcel` | Raze parcels | Eraser on grid cells |
| `GridTool_ROWOverride` | ROW override | Diagonal slash through block |
| `GridTool_BakeGrid` | Bake Grid | Checkmark on blueline → solid road |
| `GridTool_Undo` | Undo | Curved arrow (shared glyph) |

---

## World tab (W) — 28 tiles (World Builder only)

### Terrain stamps — vertical family (11) — `World/Stamps/Vertical`

Hill, Knoll, Ridge, Mesa, Butte, Volcano cone, Escarpment, Cliff stamp, Terraced slope, Spur, Summit cap.

*(Match `IHInvisibleHandSpec::TerrainStampCount` family split — 21 total stamps; list all 21 below.)*

### Terrain stamps — inverted family (10) — `World/Stamps/Inverted`

Valley, Basin, Sink, Canyon, Gorge, Crater, Lake bed, River channel, Cove, Harbor scoop.

### Terrain stamps — special (1) — `World/Stamps/Special`

`Stamp_IslandShelf` — −25 m shelf width brush (seed-driven width preview)

**Full stamp set (21 icons):** one top-down or profile glyph per `DT_TerrainStamp` row — fabricate as **height silhouette** (white on transparent) for clarity at 128px.

### Water & climate (4) — `World/Water`

| itemID | Icon depiction |
|--------|----------------|
| `Water_Spring` | Spring droplet |
| `Water_StreamOpen` | Open spline stream |
| `Water_LakeClosed` | Closed lake oval |
| `Water_Hemisphere` | Globe hemisphere slice (Nordic / Temp / Tropical) |

### Island / bake (3) — `World/Island` *(UI actions — optional non-drag)*

| itemID | Icon depiction |
|--------|----------------|
| `Island_Slot` | Island outline + index |
| `Island_BakeLandform` | Mesh + lock flame |
| `Island_RegionalRoad` | Dashed road to coast |

---

## Build tab (B) — 58 tiles

Accordion sections with nested headers. Icons: **3/4 isometric** structure grab or stylized mesh render.

### Civil / Public (10) — `Build/Civil/Public`

Rathaus, Marktplatz, Church, Chapel, Monastery, Archive, School, University, Hospital, Well.

### Civil / Civic works (6) — `Build/Civil/Works`

Fountain, Bridge stone, Clock tower, Lighthouse, Pier, Warehouse civic.

### Agrarian (8) — `Build/Agrarian`

Farmstead, Barn, Granary, Mill, Orchard, Pasture fence, Silo, Vineyard.

### Commercial (8) — `Build/Commercial`

Shop, Tavern, Inn, Guild hall, Market stall, Warehouse, Bank, Theater.

### Residential (6) — `Build/Residential`

Cottage, Townhouse, Tenement, Manor, Apartment block, Hovel.

### Industrial (6) — `Build/Industrial`

Workshop, Smithy, Tannery, Potter, Lumber yard, Mine headframe.

### Infrastructure (6) — `Build/Infrastructure`

Gate house, Stable, Well house, Cistern, Granary public, Toll booth.

### Religious (4) — `Build/Religious`

Shrine, Cathedral (large), Cemetery gate, Pilgrim hostel.

### Decorative (4) — `Build/Decorative`

Statue, Obelisk, Garden pavilion, Bandstand.

---

## Convey tab (C) — 12 tiles

| Section | Items (icon = profile or plan) |
|---------|----------------------------------|
| **Road** (3) | Cobble upgrade, Bridge wooden, Bridge stone |
| **Water** (3) | Ferry, Dock, Canal lock |
| **Bulk** (3) | Cart depot, Crane, Harbor crane |
| **Pipe** (3) | Water pipe, Sewer, Aqueduct segment (`bTransferPipeTarget`) |

---

## Defense tab (D) — 13 tiles

| Section | Items |
|---------|-------|
| **Walls** (4) | Palisade, Masonry wall, Rampart, Bastion |
| **Towers** (3) | Watchtower, Gate tower, Keep |
| **Military** (4) | Barracks, Armory, Training yard, Siege workshop |
| **Packages** (2) | `CompositePackage` wall kit, `CompositePackage` gate kit |

---

## Shared / chrome glyphs (8) — not palette rows

| Glyph | Use |
|-------|-----|
| `GLO_Locked` | Padlock overlay on disabled tiles |
| `GLO_DEV` | WB dev-scale review badge |
| `GLO_FocusOutline` | 9-slice border (material, not PNG) |
| `GLO_DragGhost` | Semi-transparent tile clone |
| `GLO_Search` | Fuzzy Finder magnifier |
| `GLO_CategoryClosed` | ▶ |
| `GLO_CategoryOpen` | ▼ |
| `GLO_ZoneBrush` | Generic paint brush (optional parent for zone swatches) |

---

## Fuzzy Finder

Does **not** require separate art — reuses icons above. Dropdown row = **64×64** thumbnail + `displayName` + `categoryPath` breadcrumb.

---

## Fabrication priority (recommended order)

| Priority | Set | Count |
|----------|-----|------:|
| P0 | G Town Templates | 5 |
| P0 | Zone Brush + 18 swatch PNGs | 1 + 18 |
| P0 | Scale-review structures (Rathaus, Farmstead, Shop, Barracks, Church) | 5 |
| P1 | W Terrain stamps | 21 |
| P2 | B Civil + Agrarian core | 18 |
| P2 | G Open spline ROW | 8 |
| P3 | Remaining B / C / D | balance |

---

## CSV authoring

Add rows to `DT_BuildPaletteItem.csv` with:

```csv
---,itemID,paletteTab,categoryPath,displayName,tooltip,icon,interactionType,levelRequired,...
Rathaus_CIV,Rathaus_CIV,Build,Build/Civil/Public,Rathaus,CIV town hall,/Game/.../ICO_Build_Civil_Rathaus,DropActor,MainGame,...
```

Import rules: see `Data/DataTables/README.md`.
