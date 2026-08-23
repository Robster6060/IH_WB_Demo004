# IH_WB_Heightmap Handoff — 2026-08-10

## 1. Header

- **Date:** 2026-08-10 (superseded by `IH_WB_Heightmap_Handoff_2026-08-11.md`)
- **Project:** `D:\Projects\UE58Projects\IH_WB_Heightmap\IH_WB_Heightmap.uproject` (UE 5.8)
- **Repo:** `https://github.com/Robster6060/IH_WB_Heightmap.git`
- **Branch:** `main` (local; push only when asked)
- **Safe revert juncture:** `juncture/ih-wb-waterline-clamp-2026-08-10`
- **Juncture SHA (tag tip):** `389d4d25f2d53cdf17dbb6227cc39b34bcb92d82`
- **Do No Harm:** if C1 flood-fill chords return → set `bContourGoldCoastBeltEnabled=false` immediately (`bContourGoldRimStripEnabled` stays false)
- **Purpose:** Chord-free **SIGNED**. Rim v2 **method FAIL** (OFF). ContourGold **coastal-belt remesh** into gameplay IslandMesh (Problem A). HF planform densify for Azgaar nested density (Problem B). Stay **40k+513**.

## 2. Dual failure (Grab 1–2 vs Grab 3)

| Problem | Symptom | Fix |
| --- | --- | --- |
| **A** Minecraft MS stairs | IslandMesh lattice edges; ContourGold Chaikin is reporter-only | Coastal-belt remesh into IslandMesh (no Cheb omit) |
| **B** Bulbous planform | ~80% monotonous rounded arcs; nested inlets/islets localized | HF densifier/carve perimeter coverage + islet budget + less Chaikin |

Remesh alone cannot invent Grab 1–2 nesting. Planform densify cannot fix MS edge connectivity.

## 3. Signed off this arc

| Item | Status |
| --- | --- |
| Contours/WWF + Features + minimap chords/islets | **PASS** (prior) |
| IslandMesh fly sawtooth / open Z-omit rim | **PASS** — waterline clamp |
| DryGold apron cover strip | **Method FAIL** — do not revive |
| Gate A Grade + TheaterYellow | **PASS** G_reg / R3 hold |
| Inlet flood-fill chords | **PASS** R1/R2 signed |
| ContourGold rim v2 silhouette | **FAIL (method)** — flag OFF |

**Clamp checklist:** `IH_WB_Weld_WWF_Unitary_PIE_Checklist_2026-08-09.md` (**PASS**)  
**Inventory:** `IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md`  
**Beach next:** `IH_WB_BeachTerraform_NextPass_2026-08-07.md`  
**Acceptance:** `ACCEPTANCE_GATES.md`

## 4. Human gates

| Gate | Status |
| --- | --- |
| C1 Flood-fill chords under coast belt | Open — must PASS or disable belt |
| C2 Minecraft IslandMesh silhouette (A) | Open — coast belt eye |
| C3 Grade / TheaterYellow | **PASS** hold — confirm no regression |
| Planform nested density (B) | Open — Grab 3 purple arcs vs Grab 1–2 |
| Bluff long-span | After planform stable |
| Samples-1025 / Fibonacci | **Deferred** until A+B PASS + ask |

**PIE checklist:** `IH_WB_CoveBeach_ContourGold_PIE_Checklist_2026-08-10.md` (**C1/C2/C3**)

## 5. Next-chat priority

1. **Human re-PIE** — C1/C2/C3; paste `coastBelt enabled=1 coastBeltTris=… coastBeltRejected=… rimStrip enabled=0`
2. If C1 FAIL → **immediate** `bContourGoldCoastBeltEnabled=false`
3. Score A (Minecraft) vs B (planform) vs Grade separately
4. Samples-1025 / Fibonacci / full Azgaar rewrite — still gated

## 6. Deferred (do not casually unlock)

- Samples **1025** / 1-sector=1-acre densify / production Fibonacci
- Full Azgaar polyline/cell-frontier rewrite
- Sea Roots frustum / Ultimate Sky
- Re-enable rim v2 (`bContourGoldRimStripEnabled`)

## 7. Do not

- Revive SandApron / dryGold
- Patch chords forward if C1 FAIL — disable coast belt first
- Unlock samples-1025 / Fibonacci without ask
- Flatten Bluff / reopen Grade for Minecraft-only debt
- Re-enable rimStrip Cheb-omit half-state

## 8. Code pointers

- Coast belt: `IH_WB_IslandActor.cpp` — `AppendContourGoldCoastBelt`; log `coastBeltTris=`
- Spec: `bContourGoldRimStripEnabled=false`, `bContourGoldCoastBeltEnabled=true`
- Do No Harm kill switch: `bContourGoldCoastBeltEnabled=false` in `IHInvisibleHandDesignSpec.h`
- Planform densify: `IHHeightfieldCoastGenerator.cpp` — purple densifier / nest / islets / beach-skip
- ContourGold Chaikin: Main ×1 @0.22; secondary ×2 (less over-round)
- Clamp / hardSnap=2: unchanged; waterlineClamp owns mixed tris

## 9. Safe revert juncture

**Tag:** `juncture/ih-wb-waterline-clamp-2026-08-10`  
**Tag tip SHA:** `389d4d25f2d53cdf17dbb6227cc39b34bcb92d82`

```text
git fetch --tags
git switch --detach juncture/ih-wb-waterline-clamp-2026-08-10
```

## 10. Opening script (paste into new agent chat)

```text
You are continuing IH_WB_Heightmap (UE 5.8) at:
D:\Projects\UE58Projects\IH_WB_Heightmap\IH_WB_Heightmap.uproject

Read first (in order):
1. Content/InvisibleHand/IH_WB_Heightmap_Handoff_2026-08-10.md
2. Content/InvisibleHand/IH_WB_CoveBeach_ContourGold_PIE_Checklist_2026-08-10.md
3. Content/InvisibleHand/ACCEPTANCE_GATES.md
4. Content/InvisibleHand/IH_WB_Session_SignOff_Deferred_TODO_2026-08-08.md

Active: coast belt remesh (A) + planform densify (B). Stay 40k+513.
Do No Harm: C1 chords → bContourGoldCoastBeltEnabled=false.
Do not unlock 1025 / Fibonacci / SandApron / rim v2 without ask.
```
