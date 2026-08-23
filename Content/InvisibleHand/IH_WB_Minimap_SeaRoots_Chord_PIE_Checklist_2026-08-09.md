# Minimap SeaRoots Chord Fix — PIE Recheck (2026-08-09)

**Status: PASS (2026-08-09)** — Multi-seed eye (ABBEY3 / POKED3 / GIZMO7): Cap-then-densify removed interior SeaRoots stroke chords. Main island outlines track coast without last→first ruler chords.

**Code:** `PrepareSeaRootsBandRingForMinimapDraw` = `CapClosedPolylineUniformCount` then densify **uncapped**. Prior densify-with-Max=384 aborted mid-ring (640→384) and `DrawClosedPolyline` closed last→first through land.

## Signed check

| Seed | Interior SeaRoots chords | Notes |
| --- | --- | --- |
| ABBEY3 | **PASS** | Main outlines smooth |
| POKED3 | **PASS** | — |
| GIZMO7 | **PASS** | — |

Open follow-ups (not this gate): ContourGold planform coastline smooth; walkable cove beachfront slopes (fly sawtooth / barrier islets **closed** — clamp + islet FeatureId PASS).
