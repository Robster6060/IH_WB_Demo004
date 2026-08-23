# IH_WB_Heightmap Handoff — 2026-08-09

## 1. Header

- **Date:** 2026-08-09
- **Project:** `D:\Projects\UE58Projects\IH_WB_Heightmap\IH_WB_Heightmap.uproject` (UE 5.8)
- **Repo:** `https://github.com/Robster6060/IH_WB_Heightmap.git`
- **Branch:** `main` (local; push only when asked)
- **Tags:** `juncture/ih-wb-unit-select-move-2026-08-07`; coast juncture `juncture/ih-wb-heightmap-2026-08-07-azgaar-coast` @ `f88b331`
- **Prior handoff:** `IH_WB_Heightmap_Handoff_2026-08-08.md` (TW select/move + Contours/Features inventory)
- **Purpose:** Contours/WWF + Features **PASS**. Minimap Cap-then-densify + cell-scaled Beach lip **code shipped**. **Superseded by** `IH_WB_Heightmap_Handoff_2026-08-10.md` (waterline clamp PASS).

## 2. Signed off this arc

| Item | Status |
| --- | --- |
| Place Ship (DEV water spawn Merchantman) | Signed |
| **TW-style** LMB select/deselect/box; RMB replace move; Shift+RMB waypoint queue; multi-buoy | **Master sign-off** — `IH_WB_UnitSelectMove_Waypoint_SignOff_2026-08-08.md` |
| RMB look suspended while units selected | Signed with TW |
| DEV Clouds default-hide | Signed earlier |
| Island click vs leftover ship selection | Fixed (land wins / TW LMB) |
| Contours multi-ring (MainCoast=largest; bake all significant rings) | **Signed** — gold Contours eye PASS |
| Magenta/Cyan DeepOuter + ShelfMesh loft (+Z winding) | **Signed Contours/WWF PASS** |
| Features chord fix (per-class open runs / no inlet chords) | **Signed multi-seed PASS** — ABBEY3 / ALERT4 / GIZMO7 |
| IslandMesh Z&lt;0 tri omit (no white coast drapes) | **Code shipped** |
| Minimap SeaRoots polish chords | **Signed PASS** — Cap-then-densify |
| Minimap barrier islets | **Signed PASS** — ContourGold FeatureId rings |
| Beach-lip + sand skirt | Cell lip + **sloped sand skirt** (not iceberg −25 cap) |

## 3. Human gates open at handoff

| Gate | Expect | Status |
| --- | --- | --- |
| Contours/WWF stack | Magenta parallel gold; cyan between gold/magenta | **PASS** |
| Features multi-seed | Features ON; no inlet chords | **PASS** |
| Minimap SeaRoots chords | No interior cyan/tan chords | **PASS** |
| Minimap barrier islets | Gold rings on map match Contours/mesh | **PASS** |
| Beach-lip / sand skirt | Sloped sand skirt + minimap gold Chaikin | **Code ship — PIE confirm** |
| Class terraform | Beach/Gentle/Bluff look like class | **Next** |

**Inventory:** `Content/InvisibleHand/IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md`  
**WWF checklist:** `Content/InvisibleHand/IH_WB_WWF_Shelf_Drape_PIE_Checklist_2026-08-08.md` (**PASS**)  
**Features checklist:** `Content/InvisibleHand/IH_WB_Features_MultiSeed_PIE_Checklist_2026-08-09.md` (**PASS**)  
**Beach-lip:** `Content/InvisibleHand/IH_WB_BeachTerraform_NextPass_2026-08-07.md`

## 4. Next-chat priority

1. PIE confirm sloped sand skirt (fly sawtooth) + minimap gold Chaikin
2. **Class terraform** so Beach / Gentle / Bluff look like class
3. Shore-cam confirm Gate-2 Beach lip grade if needed

## 5. Deferred (do not casually unlock)

- Sea Roots frustum enable + warm-sand/indigo MID (**never cyan**)
- Ultimate Sky (`Content/InvisibleHand/UI/IH_UltimateSky_Enablement.md`) — after coast + FOV + skirt PIE
- Ocean ship-cam crest/heave; fly skirt horizon fill
- Production Fibonacci acres (`bWBUnlockProductionCanonicalAcres = false`)
- Samples **1025** (ROI-gated)
- Shift+RMB+drag smooth curve path
- RMB-on-unit attack
- Azimuth→arc-length Features classifier polish (post-lip only)
- Hard-hat diver / pier sockets on WWF (ShelfMesh stays NoCollision)

## 6. Do not

- Unlock production acres / 1025 without ROI + ask
- Touch Azgaar `Generate` / IslandMesh HF / C·H·F inlet generation (presentation-only Contours/Shelf)
- Restore HF −25 Max **XY** override for shelf extent (metrics-only isoline remains OK)
- Re-enable reverted mesh structural beachfront flags
- Paint Contours as band fills (cyan fill = ShelfMesh only)
- Cyan on Sea Roots frustum
- Reintroduce aquarium water tank; Sea Floor **−250 m ASL** remains vertical canon
- Invent a second minimap coast generator (must stay SSOT consumer)

## 7. Canon pointers

- GamePlay Manual §5.2 rev **1.15**
- `Content/InvisibleHand/IH_WB_UnitSelectMove_Waypoint_SignOff_2026-08-08.md`
- `Content/InvisibleHand/IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md`
- `Content/InvisibleHand/IH_WB_BeachTerraform_NextPass_2026-08-07.md`
- `Content/InvisibleHand/IH_WB_WWF_Shelf_Drape_PIE_Checklist_2026-08-08.md`
- `Content/InvisibleHand/ACCEPTANCE_GATES.md`
- Contours / SeaRoots / cyan loft: `Source/IH_WB_Heightmap/InvisibleHand/IH_WB_IslandActor.cpp`
- Minimap SeaRoots Cap-then-densify: `IH_P1C08_MinimapCoastline.cpp` → `PrepareSeaRootsBandRingForMinimapDraw`
- Gate-2 cell-scaled lip: `IHHeightfieldCoastGenerator.cpp` (`max(spec, ≥2.5×Spacing)`)
- Sand lip TOPO: `IHInvisibleHandDesignSpec.h` `TopographyBeachLipSandMaxMeters` + `ComputeVertexTopoTierIndex`

## 8. Next-agent prompt

```
You are continuing IH_WB_Heightmap (UE 5.8) at:
D:\Projects\UE58Projects\IH_WB_Heightmap\IH_WB_Heightmap.uproject

Read first (in order):
1. Content/InvisibleHand/IH_WB_Heightmap_Handoff_2026-08-09.md
2. Content/InvisibleHand/IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md
3. Content/InvisibleHand/IH_WB_BeachTerraform_NextPass_2026-08-07.md
4. Content/InvisibleHand/ACCEPTANCE_GATES.md

Context — Contours/WWF + Features PASS; minimap densify-only + beach-lip code shipped:
- Confirm PIE: GIZMO7/ABBEY3 minimap no interior SeaRoots chords
- Confirm PIE: Beach arcs softer ramp + sand ASL 0–4 m
- Next work: class terraform

Preserve (SIGNED / do not regress):
- TW-style unit select/move + waypoints
- Contours strokes only; cyan = ShelfMesh only; Sea Roots frustum never cyan
- Sea Floor −250 m ASL; production acres gated; samples 1025 gated
- Minimap stays SSOT consumer (no second rim bake)
- Do NOT re-enable structural beachfront flags
```
