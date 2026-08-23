# Minimap Barrier Islets — PIE Checklist (2026-08-09)

**Status: PASS (2026-08-09)** — Multi-seed eye (ABBEY3 / POKED3 / GIZMO7): barrier islets visible on minimap as ContourGold secondary rings. Same Contours gold bake (SSOT); no second contour generator.

**Code:** `RefreshMinimapCoastline` registers every `ContourGoldRingsLocalCm` ring with `(IslandIndex, FeatureId)` — FeatureId 0 = MainCoast, 1.. = barrier islets.

## Signed check

| Seed | Barrier islets on minimap | Notes |
| --- | --- | --- |
| ABBEY3 | **PASS** | Yellow/gold islet outlines match PIE mesh |
| POKED3 | **PASS** | — |
| GIZMO7 | **PASS** | — |

Open follow-up: walkable cove beachfront slopes / ContourGold planform smooth (fly sawtooth **closed** — waterline clamp PASS).
