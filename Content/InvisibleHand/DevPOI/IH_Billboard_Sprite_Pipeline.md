# IH Billboard Sprite Pipeline — **LOCKED**

Quick reference for viewport-facing PNG billboard sprites (colorized woodcut / GIS-style art).

**Junction companion:** `Content/SignOff/P1C12_Gate2_Coast_Fallback_Junction.md` §2b dev POI markers.  
**Status:** PIE verified 2026-06-27 (random seed). Do not change proportions or placement math without PIE sign-off.

---

## Strategy (canonical)

Use **pre-colored PNG sprites** on **`UStaticMeshComponent` engine planes** with **Paper2D masked unlit material**, **yaw-only camera facing**, and **runtime texture-aspect sizing**. This is the preferred IH pattern for:

- Gate 2b dev POI arrows (pink landing cove, yellow summit)
- Future GIS-style world callouts, map pins, and low-cost icon overlays in fly-cam gameplay

**Why:** Small files, one-time PNG load, one MID per marker, per-frame cost is transform-only (no pixel work, no gameplay sim).

---

## Folder layout

| Path | Purpose |
|------|---------|
| `D:\InvisibleHandCharts\` | Source art (tight-cropped JPG exports) |
| `Content/InvisibleHand/DevPOI/` | Runtime PNG sprites + `convert_ih_billboard_sprite.py` + this doc |
| `IHInvisibleHandDesignSpec.h` | Locked constants, flags, PNG paths |
| `IH_P1C10_IslandActor.cpp` | `GetDevPOIBillboardTextureAspectWH`, placement, rotation, materials |
| `IH_P1C08_MinimapSubsystem.h` | `GetGate2bDevPOIMarkerDisplayColor()` |

---

## Locked PNG proportions (Gate 2b arrows — cropped woodcut export)

Measured from runtime PNG alpha bbox after `convert_ih_billboard_sprite.py` (2026-06-27):

| Sprite | File | Pixel size | Alpha bbox | **Aspect W/H** |
|--------|------|------------|------------|----------------|
| Landing Cove (pink) | `ArrowIndicatorSpritePink.png` | 165 × 1175 | 164 × 1173 | **0.1398** |
| Summit (yellow) | `ArrowIndicatorSpriteYellow.png` | 167 × 1175 | 165 × 1173 | **0.1407** |
| Code fallback | — | — | — | **0.14** |

Source JPGs (tight-cropped by design): `D:\InvisibleHandCharts\ArrowIndicatorSpritePink.jpg`, `ArrowIndicatorSpriteYellow.jpg`.

**Rule:** Billboard **width always follows height × runtime texture aspect** — never a fixed shaft-based width for billboards.

```cpp
// IH_P1C10_IslandActor.cpp — UpdateDevPOIArrowBillboard
const float ScaledHeightCm = (HoverCm + HeadLengthCm) * UniformScale;
const float WidthCm        = ScaledHeightCm * TextureAspectWH;  // from PNG GetSizeX/GetSizeY
```

---

## Locked world scale (cm)

| Constant | Value | World meaning |
|----------|-------|----------------|
| `CoastC2b_DevPOIMarkerHoverCm` | **26666** | Shaft component of billboard height |
| `CoastC2b_DevPOIMarkerArrowHeadLengthCm` | **1666** | Head component of billboard height |
| `CoastC2b_DevPOIMarkerBillboardUniformScale` | **2.0** | Height multiplier (width follows aspect) |
| `CoastC2b_DevPOIMarkerFloatAboveSurfaceCm` | **10000** | Tip anchor **+100 m ASL** above surface at POI XY |

### Derived billboard size (current locked values)

| Quantity | Formula | **Numeric result** |
|----------|---------|-------------------|
| Base height | `HoverCm + HeadLengthCm` | **28332 cm** (~283 m) |
| **Scaled height** | `Base × UniformScale` | **56664 cm** (~567 m) |
| **Scaled width (pink)** | `56664 × 0.1398` | **≈ 7921 cm** (~79 m) |
| **Scaled width (yellow)** | `56664 × 0.1407` | **≈ 7973 cm** (~80 m) |
| Plane scale (engine plane 100 cm) | `(Width/100, Height/100, 1)` | **(≈79, 567, 1)** |

### Locked placement (tip anchor — critical)

```
FloatAnchor  = TipLocalCm + (0, 0, FloatAboveSurfaceCm)     // tip at surface + 100 m
CenterLocal  = FloatAnchor + (0, 0, ScaledHeightCm / 2)    // MUST use ScaledHeightCm
```

**Do not** apply `UniformScale` to plane scale without the same factor on `CenterLocal` offset — that drops tips below sea level (bug fixed 2026-06-27).

### Locked rotation

- **Yaw-only** camera facing: `ToCamera` projected to horizontal XY (no pitch tilt).
- **+180° roll** on Z so arrow tip points down in world space.
- Function: `ComputeDevPOIVerticalBillboardWorldRotation`.

### Locked material

- Parent: `/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial`
- Set **`SpriteTexture` only** — **never** `SetVectorParameterValue(SpriteColor)` (crashes on static mesh).
- Widget3DPassThrough materials are fallback only (invisible on bare planes).

---

## Legacy width scale (3D fallback only)

`CoastC2b_DevPOIMarkerBillboardWidthScale = 7.9f` applies to **3D cylinder+cone fallback**, not billboards. Billboards use texture aspect (above).

Historical note: full-sheet PNG (832×1248, aspect ~0.67) needed ~7.9× shaft-based correction; **tight-cropped** art (~0.14 aspect) made shaft-based width obsolete.

---

## Runtime pipeline (junction)

```
bCoastC2b_DevPOIMarkersEnabled
  → bCoastC2b_DevPOIMarkersUseBillboard
      → bCoastC2b_DevPOIMarkersUseColorizedBillboardSprites
          → per-POI PNG + MaskedUnlitSpriteMaterial (SpriteTexture)
      → else legacy grayscale + CPU tint (+ optional secondary overlay)
  → else 3D cylinder + cone fallback
```

### Fallback order

1. Colorized billboard PNG per POI  
2. Legacy grayscale PNG + CPU tint  
3. 3D cylinder + cone meshes  

### Performance profile

| Step | Frequency |
|------|-----------|
| PNG load (`ImageWrapper`) | **Once per sprite kind** (static cache + `AddToRoot`) |
| Parent material load | **Once per session** |
| MID create | **Once per island actor** |
| `UpdateDevPOIArrowBillboard` | **Per frame per marker** — location, scale, yaw rotation only |

---

## Convert source art → runtime PNG

```powershell
python D:\Projects\IH_P1C12_Arbor\Content\InvisibleHand\DevPOI\convert_ih_billboard_sprite.py
```

### `colorized` mode (woodcut / GIS icons)

1. Key near-white (`r,g,b > 240`) and checkerboard neutrals (`chroma < 14`, `lum > 165`).
2. `trim_to_alpha_bbox(padding=1)`.
3. `remove_alpha_specks(max_area=12)` — drops JPEG fringe residue.

**Never ship JPG in-game.**

---

## Install a new PNG billboard (checklist)

1. Tight-crop art in `D:\InvisibleHandCharts\` (tip direction = down for POI arrows).
2. Run `convert_ih_billboard_sprite.py` → `Content/InvisibleHand/DevPOI/`.
3. Add paths to `IHInvisibleHandDesignSpec.h`.
4. Wire `GetDevPOIArrowBillboardTexturePaths()` / `ResolveDevPOIArrowBillboardTexture(Kind)`.
5. Pass aspect via `GetDevPOIBillboardTextureAspectWH(Kind)` into `UpdateDevPOIArrowBillboard`.
6. `CreateDevPOIBillboardMID(..., bPreColoredSprite=true)`.
7. Match minimap color in `GetGate2bDevPOIMarkerDisplayColor()`.
8. Build → **full editor restart** → PIE.
9. Optional: import as `T_*` uasset for packaged builds.

---

## Future phases — GIS / callout recommendations (beyond WB)

| Recommendation | Rationale |
|----------------|-----------|
| **Reuse this pipeline** for world-space GIS pins | Proven low cost; same material + aspect math |
| **Texture atlas** for many icons | One PNG + UV rects → fewer loads, one material |
| **Shared static MID pool** keyed by texture | Avoid per-actor MID explosion on dense maps |
| **Distance LOD** | Hide or halve `UniformScale` beyond N km — saves transform + overdraw |
| **Separate paths for pure 2D HUD map vs 3D world** | UMG/Slate for flat map; billboards only when icon must sit in world space |
| **Cook uassets for shipping** | Drop runtime `ImageWrapper` path in packaged builds |
| **Standardize art export** | Tight crop + transparent PNG; avoid checkerboard JPG faux-alpha |
| **Yaw-only billboards** | Never use full `ToCamera` pitch on vertical callouts — preserves Z anchor |
| **Centralize in a small library** | e.g. `IH_BillboardSpriteLibrary` when second consumer appears (not needed for Gate 2b alone) |

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Tip under sea level | `UniformScale` on plane but not center offset | Use `ScaledHeightCm` for both |
| Too wide vs art | Shaft-based `BillboardWidthScale` on billboards | Use `TextureAspectWH` from PNG |
| Invisible sprite | Widget3DPassThrough on static mesh | Paper2D `MaskedUnlitSpriteMaterial` |
| Crash on spawn | `SetVectorParameterValue(SpriteColor)` | Remove; use pre-colored PNG |
| Checkerboard plate | Uncropped JPG / weak key | Tight crop + re-run `colorized` |
| White specks | JPEG fringe | `remove_alpha_specks` in converter |

---

## Verify in PIE

- Pink down arrow at landing cove, yellow at summit — proportions match cropped source
- Tips at **+100 m ASL**, not clipped into terrain/water
- Log: `Phase C2b devMarkers billboard parentMaterial=/Paper2D/MaskedUnlitSpriteMaterial...`
- Minimap `+` colors match `GetGate2bDevPOIMarkerDisplayColor`
