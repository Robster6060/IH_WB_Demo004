# Juncture — IH Commandable Unit Select/Move + Place Ship

**Tag:** `juncture/ih-wb-unit-select-move-2026-08-07`  
**Date:** 2026-08-07 (rev: TW-style RMB orders 2026-08-07 evening)  
**Status:** **SIGNED OFF** — master: [`IH_WB_UnitSelectMove_Waypoint_SignOff_2026-08-08.md`](IH_WB_UnitSelectMove_Waypoint_SignOff_2026-08-08.md)

## Canon (GamePlay Manual §5.2 rev 1.15 + ACCEPTANCE_GATES)

| Input | Action |
| --- | --- |
| LMB unit / box | Select (Shift = additive) |
| LMB empty | Clear unit selection; sail continues |
| LMB island | Clear ships + island focus |
| RMB water (selection) | Replace move queue + buoy |
| Shift+RMB water | Append waypoint + buoy |
| RMB+drag | Camera look **only when no units selected** |
| Esc | Clear selection (PIE caveat) |
| Place Ship | DEV: LMB water spawn Merchantman → auto-select → exit mode |

**Stack:** `ShipRegistry` → `IssueMoveOrderToSelection(bAppend)` → `ReplaceSailOrder` / `EnqueueSailWaypoint` + multi-buoy.

## Beach terraforming (roadmap note)

Features yellow/cyan/magenta = **coast-character class labels**, not sand geometry. HF Gate-2 walkable lip (8–15 m, ≤8°) already runs on Beach-class arcs in `IHHeightfieldCoastGenerator`; mesh structural beachfront flags remain OFF. **Next after Features seed sign-off:** beach-lip visibility pass (strengthen HF soften + sand TOPO) so coves read as walkable beaches.
