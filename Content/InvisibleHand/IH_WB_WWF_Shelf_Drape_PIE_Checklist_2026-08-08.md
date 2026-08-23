# WWF Shelf / IslandMesh Drape — Clarification + PIE Checklist

**Status: PASS (2026-08-09)** — Magenta Contours + cyan ShelfMesh ↔ magenta rim **eye PASS** on ABBEY3 / POKED3 / ALERT4 / GIZMO7 (Ocean OFF, Contours ON). Gate 0: `normalPush=1`, `selfCross=0`, `shelfTris≫0`, `indexLock=1`, `shelfRings=1`. Contours/WWF presentation stack signed; **next gate = Features multi-seed**.

**Date:** 2026-08-08 (rev: Contours/WWF PASS 2026-08-09)  
**Related:** `IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md`, `IH_WB_Features_MultiSeed_PIE_Checklist_2026-08-09.md`

## Clarification

| Observation | Meaning |
| --- | --- |
| Red-arrow white curtains past gold | IslandMesh coast-straddlers (fixed: omit any vert Z &lt; 0) |
| Cyan between gold and magenta | **WWF presentation fill** — loft **decimated gold** → **gold-governed outer** |
| Magenta Contours | Stroke on **governed WWF rim** (same outer as cyan); **flat Z** at ShelfFloor+lift (no HF climb) |
| Governed extent | Decimate coast ~640 → blur Disp → **normal-push** once + firth shrink (**no** Chaikin / **no** variable miter) |
| Slope LUT | **40 m inland slope** → Beach 450 / Gentle 350 / Steep 250 / Sheer 100 m (blurred) |
| HF −25 isoline | Still extracted for **WWFFootprintAcres metrics only** — does **not** drive cyan/magenta XY |
| Green hatch deep channels | Procedural troughs/inlets — navigable water, not walkable WWF |
| Cyan ShelfMesh | Visual / zoning only (`NoCollision`); pier/hard-hat deferred |
| Dry walkable beach | Gate-2 lip on land — separate from WWF (beach-lip pass after Features) |
| IslandMesh ↔ gold dark gap | Expected (Z&lt;0 omit + 513 facets); close at **beach-lip / sand TOPO**, not cyan |

**Canon stack:** Gold Contours ASL 0 | **Cyan loft (index-locked decimated gold→LUT outer, +Z winding)** | Magenta flat on same outer | Sea Roots frustum deferred (never cyan).

**Azgaar / C·H·F / IslandMesh HF:** unchanged (presentation-only).

**Governed extent (current):** Decimate coast ~640 → blur Disp → **normal-push** `Coast+Outward*Disp` → opposite-shore + residual shrink → `selfCross=0`. No Chaikin after offset. Loft tris face +Z for fly-cam.

**Minimap SSOT:** Minimap gold + SeaRoots bands are **derivative consumers** of IslandActor `MainCoast` + `SeaRootsExtent` (same DeepOuter as PIE magenta) — not an independent HF/shelf bake. Residual Grab-5 interior chords = display prep (polish/Chaikin); fix under minimap fidelity **after** Features + beach-lip.

---

## Gate 0 — prove binary (regression)

1. `GovernedWWF … samples=~640 … normalPush=1 miterOffset=0 decimate=1 noChaikin=1` + `selfCross≈0`
2. `ShelfMesh shelfTris=<N> … governed=1 indexLock=1` with N ≫ 0
3. `ASL contour ribbons … shelfRings=1`
4. Eyes: cyan between gold and magenta; one magenta rim parallel to gold

| Log | Action |
| --- | --- |
| Tags + `shelfTris≫0` + clean cyan/magenta | Contours/WWF still PASS |
| `shelfTris=0` / scribble magenta / missing cyan | FAIL — cold rebuild or report regression |

### Setup

1. Cold Editor reopen `IH_WB_Heightmap.uproject`.
2. PIE: **ABBEY3** → **POKED3** → **ALERT4** → **GIZMO7**.
3. Ocean OFF, Clouds OFF, Contours ON, Features OFF.

### Do not expect yet (not a fail)

- Hard-hat diver / pier collision on cyan
- Sea Roots frustum below magenta
- Perfect beach-lip sand ramps / IslandMesh flush to gold
- Minimap free of all interior SeaRoots polish chords (deferred fidelity)
- WWFFootprintAcres matching cyan annulus area
