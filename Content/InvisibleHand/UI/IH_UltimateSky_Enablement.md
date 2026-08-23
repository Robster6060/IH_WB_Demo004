# Ultimate Sky — Enablement Checklist (IH_WB_Heightmap)

**Status:** Integration path ready. Plugin binary is **not** vendored in this repo. Enable after plan-view coast + FOV + ocean-skirt PIE sign-off (per Azgaar Coast Continuation plan).

## Prerequisites (sign-off)

1. Plan-view Azgaar coast accepted (tether halo, nested inlets, purple arcs)
2. FOV altitude lerp + 150 km ocean skirt fill horizon under normal fly framing
3. Prefer Gate-2 beach lip / SeaRoots LUT already baking (this pass)

## Install

1. Install **Ultimate Sky** (or the team’s chosen marketplace sky pack) into the UE 5.8 Engine `Plugins` folder **or** this project’s `Plugins/` directory.
2. Confirm a `.uplugin` is present (note the exact plugin `Name` field).
3. Add to [`IH_WB_Heightmap.uproject`](../../../IH_WB_Heightmap.uproject):

```json
{
  "Name": "<PluginNameFromUplugin>",
  "Enabled": true
}
```

4. Regenerate project files / restart editor. Resolve Template_Default skydome warning by replacing the level sky with the Ultimate Sky actor/BP.

## Do-not-harm

- Do **not** swap Gate 0 custom ocean (`AIH_P1C12_OceanPlane`) for Water Plugin
- Do **not** unlock `bWBUnlockProductionCanonicalAcres`
- Keep Sea Floor **−250 m ASL**, WWF **−25 m**, Gold Coastline **0**
- Aquarium water tank (walls / floor / WaterTankRig) remains **retired**

## PIE verify after enable

- [ ] No Template_Default skydome warning
- [ ] Daylight on sand / grass / rock / snow topo tiers
- [ ] Horizon skirt still fills under sky (no sky cliff)
- [ ] Ship-cam / fly FOV unchanged
