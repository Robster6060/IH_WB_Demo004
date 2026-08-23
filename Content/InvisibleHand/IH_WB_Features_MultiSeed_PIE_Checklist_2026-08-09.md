# Features Multi-Seed PIE Checklist — 2026-08-09

**Status: PASS (2026-08-09)** — Features ON eye on ABBEY3 / ALERT4 / GIZMO7 (POKED3 prior “working well”). No Feature inlet chords flagged; class strokes follow coast. Minimap SeaRoots interior chords and 3D sawtooth are **separate** gates (minimap polish / beach-lip), not Features fails.

**Project:** `D:\Projects\UE58Projects\IH_WB_Heightmap`  
**Related:** `IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md`, `IH_WB_BeachTerraform_NextPass_2026-08-07.md`

## Setup (regression)

1. Features ON; Contours OFF recommended; Clouds OFF.
2. Seeds: **POKED3** → **ALERT4** → **GIZMO7** (ABBEY3 optional).

## Expect (PASS)

| Look for | Pass | Fail |
| --- | --- | --- |
| Beach yellow `#FFD24A` | Follows Beach-class arcs on MainCoast | Missing / wrong class |
| Gentle teal `#2EC4B6` | Follows Gentle arcs | — |
| Bluff magenta `#E91E63` | Follows Bluff arcs | — |
| Hard breaks at class changes | Clean run ends | Soft blend smears |
| **No inlet chords** | Open runs stop at gaps; no straight stroke across firths/coves | Straight chord across water |
| Heightfield-follow Z | Strokes hug coast mesh / waterline | Float / dive through land |

## Not Features fails

- Minimap cyan/tan chords through island interiors → SeaRoots display prep
- IslandMesh sawtooth / mesh↔gold gap → beach-lip + sand TOPO

## After PASS (done / next)

1. ~~Minimap SeaRoots densify-only strokes~~ — **code shipped** (`PrepareSeaRootsBandRingForMinimapDraw`)
2. ~~Beach-lip visibility — HF strengthen + sand TOPO~~ — **code shipped**
3. Class terraform → PIE confirm minimap + lip if not yet eyed
