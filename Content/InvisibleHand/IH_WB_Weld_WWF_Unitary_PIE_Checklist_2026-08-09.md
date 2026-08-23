# Waterline Clamp + WWF Unitary — PIE Checklist (2026-08-09)

**Status: PASS (2026-08-09/10)** — Multi-seed eye (ABBEY3 / POKED3 / ALERT4 / GIZMO7): Ocean OFF / Contours OFF / Features ON — dark land-rim sawtooth teeth **gone**. Islands may be moved/rotated in grabs for framing only.

**Remedy:** IslandMesh coast-straddle tris kept with `Z=max(Z,0)` (omit all-wet only). SandApron demoted (`bSandApronEnabled=false`). Prior dryGold apron cover strip = method **FAIL** (do not revive).

**Log Gate:** `waterlineClamp=1`, `mixedClampTris≫0`, `SandApron demoted apronTris=0`, `ShelfMesh shelfTris≫0 collision=1`.

## PIE confirm (clamp) — signed

1. Cold reopen after DLL rebuild.
2. Seeds: **ABBEY3** → **POKED3** → **ALERT4** → **GIZMO7** (Ocean OFF, Contours OFF, Features ON).
3. Fly ASL ~0.5–7 km: dark triangular land-rim teeth gone (no open underside voids into navy).
4. Log: `IslandMesh waterlineClamp=1 mixedClampTris>0`.
5. Log: `SandApron demoted apronTris=0 (waterlineClamp owns rim)`.
6. Log: `ShelfMesh shelfTris≫0 … collision=1` unchanged.
7. Barrier islets still on minimap; Features class colors unchanged.

## Pass criteria

- [x] Clamp: teeth sealed Ocean OFF (`waterlineClamp=1`) — ABBEY3 / POKED3 / ALERT4 / GIZMO7
- [x] Apron demoted (`apronTris=0` demote log)
- [x] WWF: `collision=1` + cyan shelf still present Contours ON / Ocean OFF

## Open follow-ups (not this gate)

1. Walkable cove beachfront slopes (Beach/Gentle HF ≤8° / 15 m ASL) — shore-cam
2. ContourGold planform coastline smoothing (XY) — after slopes
3. Samples 1025 / frustum / seaward sector polygons — deferred
