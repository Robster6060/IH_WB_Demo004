# IH_WB_Heightmap — Acceptance Gates

**Juncture:** `juncture/ih-wb-heightmap-2026-08-07-azgaar-coast` @ `f88b331` (prior: `juncture/ih-wb-heightmap-2026-08-06` @ `3288ca5`)  
**Active work:** Chord-free **SIGNED**; Grade PASS; rim v2 **OFF**; coast belt + planform densify **shipped** — human C1/C2/C3/C6 open. Handoff: `IH_WB_Heightmap_Handoff_2026-08-11.md` · checklist `IH_WB_CoveBeach_ContourGold_PIE_Checklist_2026-08-10.md`

## Project / build
- [x] `IH_WB_Heightmap.uproject` at `D:\Projects\UE58Projects\IH_WB_Heightmap`
- [x] EngineAssociation `5.8`
- [x] Git remote `https://github.com/Robster6060/IH_WB_Heightmap.git`
- [x] `IH_WB_HeightmapEditor` Win64 Development **build succeeded** (coast belt + planform densify 2026-08-11) — re-verify after next Continuation edits

## Harness (code present; confirm in PIE)
- [x] Fly PC/Pawn: `IH_Cube2FlyPlayerController` / `IH_Cube2FlyPawn`
- [x] Minimap: `IH_P1C08_Minimap*`
- [x] Island Nav: `IH_P1C08_IslandNav*`
- [x] Select/DnD: `SetSelectionHighlighted` + Cube2Fly Shift+drag/wheel handlers
- [x] **G / W / B / C / D**: Build Palette tab keys bound in Cube2Fly PC
- [x] Merchantman: `IH_P1C07_MerchantmanShipActor` + deferred spawn in GameMode
- [ ] PIE smoke: fly / M / nav / select-DnD / GWBCD / Merchantman

## RealmSeed `AAAAA#` (code verified)
- [x] Length exactly 6; digit 2–7; no 3-unique-digits rule
- [x] F2 panel `MaxRealmSeedLength = 6`; hint `ABBEY2`
- [x] Default seed `ABBEY3`
- [ ] PIE: sculpt `ABBEY2` / reject `ABBEY0` / `ABBEY123`

## HUD readout canon
- [x] Doc: `Content/InvisibleHand/UI/IH_HUD_Readout_Canon.md`
- [x] Realm Seed box = `AAAAA#`; status “N islands” = last digit; ASL = **camera** altitude (not summit)

## C1 inlet vocabulary + River Terminus Socket
- [x] Canon: `Content/InvisibleHand/UI/IH_C1_Inlet_Firth_Canon.md` — **Firth → Harborage → Cove** + barrier islands
- [x] `FIHRiverTerminusSocket` at Firth heads (C1 owns; Phase H snaps large-river splines)
- [x] Generator operators + logs: `firths=` / `harborages=` / `coves=` / `sockets=`
- [x] PIE debug arrows on sockets (Development)

## Acre / sample ladder (WB harness)
- [x] Active rung: **realm** layout `DefaultRealmHalfExtentNSKm = 13` (~26×42 km φ) for ~**40k** primary at 30% land
- [x] Aquarium **water tank retired** (WT-A): no glass walls / tank floor mesh / WaterTankRig spawn; orphan `P1C07_TankWall_*` / `P1C07_TankFloor` cleanup only
- [x] Layout AABB renamed (WT-B): `RealmHalfExtentNSKm` / `RealmHalfExtentEWKm` (φ width); DevSeed label `Realm: … (φ)`; ship clamp uses realm extents
- [x] `bWBUnlockFirthCapable40kBudget = true`; samples **513** @ ≥8k (**1025 reserved** until coastal ROI — do not unlock without ROI)
- [x] Nested-inlet carve: denser gold-nest + purple densifier + **Perlin coast-character** (beach / gentle / sheer) + half-moon beach scallops + Gate-2 **8–15 m walkable lip** on beach class + sheer outside beach + **scalene barrier islets** tethered `1.02–1.10R` + 40–80 m channel
- [x] Minimap map content 2× (`MapContentWidthPx` 552) with slightly thicker coast stroke
- [x] Fly FOV altitude lerp (~90 deg near sea → ~58 deg by ~5.5 km ASL)
- [x] Realm regen: ASCII RealmSeed status only
- [x] Gate-2 walkable beach lip — **implemented** heightfield-native on beach wedges / beach-class arcs (8–15 m, walk-slope capped)
- [x] SeaRoots B2 slope LUT wired: inland slope → Beach/Gentle/Steep/Sheer → 450/350/250/100 m outward (heightfield −25 m contour preferred for deep outer)
- [ ] Ultimate Sky plugin — enablement checklist at `Content/InvisibleHand/UI/IH_UltimateSky_Enablement.md` (plugin not vendored; enable after FOV + coast + skirt PIE)
- [x] Minimap SeaRoots fills from **baked MainCoast** polylines
- [x] Contours re-apply after heightfield rebuild when Contours ON
- [x] **Gated:** `bWBUnlockProductionCanonicalAcres = false` (Fibonacci millions / WP play maps — do not unlock without explicit ask)

## Heightmap / Azgaar (code verified)
- [x] Generator: `FIHHeightfieldCoastGenerator` (Demo multi-operator ensemble + C1 Firth hierarchy)
- [x] Depression fill: `FloodExteriorAndFillEnclosed` → `FilledInteriorHeightMeters = 0.25`
- [x] MapSeed Phase1 weights Low:High:Volc = **3:2:1**
- [x] Actor `AIH_WB_IslandActor` (not Arbor iceberg)
- [x] Sea Shelf extent via `FIHSeaRootsExtent` + slope-tier LUT
- [x] Summit H/D **φ lookup** (no runtime φ math): Low `1/φ⁵≈0.090` (50–180 m), High `1/φ⁴≈0.146` (160–420 m), Volc `1/φ³≈0.236` (240–620 m)
- [x] `TargetSummitMeters` + `ApplyInternalProfile` (coast/shelf ≤0 frozen)
- [x] Topography: lit Sand/Grass/Dirt/Rock/Snow PMC tiers
- [x] DEV Contours: gold ASL 0 + white +25 m — multi-ring (MainCoast=largest); gold/white heightfield-follow Z + opaque Color MID
- [x] DEV Contours magenta WWF rim: gold-governed DeepOuter via **normal-push** + firth cleanup + **flat Z**; Gate 0 + **multi-seed eye PASS**
- [x] DEV Features: Beach (`#FFD24A`) / Gentle (`#2EC4B6`) / Bluff (`#E91E63`) — per-run ribbons (no gap-marker chords across inlets); heightfield-follow Z + opaque Color MID
- [x] DEV Clouds: default hide Template/volumetric clouds; Clouds checkbox re-enables
- [x] Place Ship HUD left of ASL; Gate0 OceanPlane water resolve (ASL 0); click-to-sail + orange buoy
- [x] IH select/move canon (**Total War–style**): LMB select/deselect/box; **RMB** water = replace move + buoy; **Shift+RMB** = append waypoint queue; RMB look suspended while units selected; Esc clears selection (GamePlay Manual §5.2 rev 1.15); juncture `juncture/ih-wb-unit-select-move-2026-08-07`
- [x] Shift+RMB water waypoint queue (breadcrumb A→B→C) + multi-buoy persistence — **implemented**
- [x] Viewport island click: LMB land with ships selected clears ships then focuses island; LMB empty deselects ships (sail continues)
- [x] IslandMesh waterline clamp — keep mixed tris `Z=max(Z,0)`; omit all-wet; SandApron demoted; seals open Z-omit rim (supersedes absolute Z&lt;0 omit for coast-straddle)
- [x] WWF cyan ShelfMesh loft (index-locked gold↔DeepOuter; **+Z winding**) Gate 0 + **eye PASS** cyan between gold/magenta — checklist `IH_WB_WWF_Shelf_Drape_PIE_Checklist_2026-08-08.md` (**PASS**)
- [x] **Features multi-seed eye** — ABBEY3 / ALERT4 / GIZMO7 Features ON, no Feature inlet chords — `IH_WB_Features_MultiSeed_PIE_Checklist_2026-08-09.md` (**PASS**)
- [x] OceanPlane at **ASL 0**; `bDevDemo_LowerOceanToShelfFloor = false` (shelf = Contours magenta, not lowered sea)
- [x] Islet radial tether clamp ~1.02–1.06R; dual-end Firth harborages are gameplay canon
- [x] PIE visual: ALERT4 / GIZMO7 / ABBEY3 — Contours magenta + cyan WWF stack **PASS**; Features colors multi-seed **PASS**
- [x] Beach-lip / class terraform HF — first harden shipped; shore-cam **FAIL** (wedding-cake under yellow Beach)
- [x] Broad Highlands HF harden — width floor `15/tan(8°)`; still **FAIL** (ApplyInternalProfile undid band)
- [x] Post-profile finalize — `FinalizeWalkableBeachfrontSlopes` hard GradeCap Beach/Gentle; Contour extract after finalize
- [x] `ApplyInternalProfile` full Beach/Gentle walk-band preserve (+1.15× WidthFloor); summit scale skips walk band
- [x] ContourGold / MainCoast planform Chaikin **×1 @0.22**; secondary/islet ContourGold **×2** (less over-round for remesh authority); Features inherit MainCoast
- [x] DEV **GrabContrast** toggle — TankSun 5.5 + TOPO albedo 0.72 (vs pie sun 12 washout; UE outdoor ~3–8)
- [x] Bluff/Sheer buttress HF polish v1 + **long-wavelength de-rhythm** (sheer preserved)
- [x] Bluff/Sheer buttress **v2** — longer wavelength (0.20×R), arc-length phase, med noise 0.12, deterministic seed
- [x] IslandMesh waterline soft — MainCoast + all ContourGold rings
- [x] IslandMesh waterline **hard-snap v1** — dry+wet rim Chebyshev≤2, pull 0.92, radius 3.5×spacing (`hardSnap=1`) — **partial eye:** gold tracks, silhouette FAIL
- [x] IslandMesh waterline **hard-snap v3** (`hardSnap=2`) — eye FAIL M3/M4 topology
- [x] Gate A Grade **PASS**; R1/R2 flood-fill chords **PASS** after surgical revert
- [x] Inlet-safe ContourGold rim **v2** — **method FAIL** for silhouette (high reject + Cheb omit); flag **OFF**
- [x] ContourGold **coastal-belt remesh v1** into IslandMesh — ContourGold↔inland offset; no Cheb omit; `coastBeltTris=`; Do No Harm `bContourGoldCoastBeltEnabled`
- [x] Coast-belt authority polish — max-edge **4.0×** spacing, inland **1.5×**, ring verts **720** (cut reject / Minecraft stairs)
- [x] Features Beach/Gentle→Bluff multi-cell slope override (R3); Bluff densifier long-span spacing widen
- [x] Features yellow-on-bluff tighten — slope ~tan(6°) + side samples so yellow Beach ribbon leaves sheer faces
- [x] Planform densify (B) — densifier perimeter coverage + nest depth + islet punctuations + less Chaikin over-round
- [x] Planform densify iterate (B2) — narrower beach-skip; deeper knife stubs (no mouth-flare); more cove/micro/islets; Smooth 4.0; secondary Chaikin×1
- [x] Fjord polish — long densifier stubs → mid-spine daughters; short → half-moon coves; residual purple nick outside Gate-2 wedge
- [ ] **Human re-PIE:** Grab A–E — checklist `IH_WB_CoveBeach_ContourGold_PIE_Checklist_2026-08-10.md`; if chords FAIL set `bContourGoldCoastBeltEnabled=false`
- [ ] Samples **1025** / production Fibonacci — gated until A+B eye PASS + ask
- [ ] Full Azgaar polyline/cell-frontier rewrite — deferred

## Ocean (endless visual + hull-proportioned waves)
- [x] `AIH_P1C12_OceanPlane`: camera-follow Gerstner; **realm AABB is layout-only** (not aquarium)
- [x] Horizon skirt: flat 150 km half-extent (do **not** grow Gerstner with realm size)
- [x] Gerstner tile: **8×8 km** (`PlaneHalfExtentCm=400000`), `GridDivisions=256`, cell ≈ **31 m**
- [x] `hullProp` trains: λ 95/80/70/65 m; amps 28/18/12/8 cm (**ampSum=66**)
- [x] Translucent FlattenMaterial `OceanOpacity=0.40`; DEV Ocean toggles Gerstner+skirt
- [x] Sea Floor canonical **−250 m ASL** (vertical oceanography; not a tank floor mesh)
- [x] WWF shelf contour **−25 m**; Gold Coastline **0**
- [ ] PIE ship-cam: crest ~2× hull; heave under freeboard (human sign-off)
- [ ] PIE fly: skirt fills horizon (no sky cliff under normal framing)

## Manual PIE checklist
1. Open `IH_WB_Heightmap.uproject` in UE 5.8
2. PIE → `ALERT4` / `ABBEY3`; Contours ON — nested parent bay + daughter indentation; barrier islets hug parent halo after rotate
3. Confirm walkable beach lip on reserved wedges / beach-class arcs; sheer/bluff elsewhere
4. Ship cam @ Game Speed 1×: hullProp wave spacing/heave vs Merchantman
5. Log `firths≥1` `harborages=` `coves=` `sockets=` `samples=513` `islets=` (≥8k acres; 1025 gated on ROI)
6. DEV Contours ON → gold on **primary** coast (+ significant islets) + magenta −25 m closed rings + white +25 m; Features ON → Beach (yellow) / Gentle (cyan) / Bluff (magenta) follow perimeter with hard breaks at class changes (**no** straight chords across inlets); Clouds OFF by default
7. Confirm cyan River Terminus Socket arrows at Firth heads; ShelfMesh cyan WWF band (0→−25) with Ocean OFF — see `IH_WB_WWF_Shelf_Drape_PIE_Checklist_2026-08-08.md`
8. Confirm Volc/High taller than Low (φ H/D); sand/grass/rock/snow bands
9. F2 → sculpt seed list; Island Nav acres reflect ~40k-class primary; DevSeed shows **Realm** extents
10. Place Ship (left of ASL) → LMB water spawn Merchantman; **LMB** select ship; **RMB** water → buoy + sail; **Shift+RMB** append waypoints; LMB empty deselects (sail continues); Esc clears selection
11. With ships selected: RMB look suspended (use MMB); LMB **dry land** selects island (clears ships); water LMB deselects
12. After coast + FOV + skirt signed → follow `IH_UltimateSky_Enablement.md`
