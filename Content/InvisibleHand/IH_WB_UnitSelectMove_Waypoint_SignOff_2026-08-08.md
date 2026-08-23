# SIGNED OFF — Unit Select / Move + Waypoint Queue (Total War–style)

**Status:** SIGNED OFF (master canon)  
**Date:** 2026-08-08  
**Juncture tag:** `juncture/ih-wb-unit-select-move-2026-08-07`  
**Manual:** GamePlay Manual §5.2 rev **1.15**  
**Gates:** `Content/InvisibleHand/ACCEPTANCE_GATES.md`  
**Juncture note:** `Content/InvisibleHand/IH_WB_UnitSelectMove_Juncture_2026-08-07.md`

## Canon input table

| Input | Action |
| --- | --- |
| LMB unit / box | Select (Shift = additive) |
| LMB empty | Clear unit selection; sail continues |
| LMB island (dry land) | Clear ships + island focus |
| RMB water (selection) | Replace move queue + orange buoy |
| Shift+RMB water | Append waypoint + buoy per waypoint |
| RMB+drag | Camera look **only when no units selected** |
| Esc | Clear selection (PIE may still Stop Play) |
| Place Ship | DEV: LMB water spawn Merchantman → auto-select → exit mode |

## Stack (no behavior change expected)

`ShipRegistry` → `IssueMoveOrderToSelection(bAppend)` → `ReplaceSailOrder` / `EnqueueSailWaypoint` + multi-buoy (`AIH_P1C07_MoveDestinationBuoy` completes by proximity to anchor).

Key types: `AIH_Cube2FlyPlayerController`, `UIH_P1C07_ShipRegistrySubsystem`, `AIH_P1C07_CommandableShipActor`.

## PIE checklist (confirm once if desired)

1. Place Ship → LMB water → Merchantman appears, selected.
2. LMB select / Shift multi-select / box select.
3. RMB water → replace sail + buoy; ship sails.
4. Shift+RMB → second/third waypoint + buoys; queue advances.
5. LMB empty → deselect; sail continues; RMB look restored.
6. LMB dry land with ships selected → ships clear, island focuses.
7. While selected: RMB look suspended (use MMB for look).

## Explicitly deferred (not part of this sign-off)

- Shift+RMB+drag **smooth curve** path (v1 = discrete clicks only)
- RMB-on-unit **attack** (combat stub later)
- Esc vs PIE Stop Play conflict (engine caveat)

## Related sign-offs this arc

- Place Ship (DEV water spawn) — signed earlier
- DEV Clouds default-hide — signed earlier
- Island click vs leftover ship selection — fixed under TW LMB rules
