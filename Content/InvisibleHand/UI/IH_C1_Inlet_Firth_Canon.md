# C1 Inlet Canon — Firth / Harborage / Cove + River Terminus Socket

Ocean-connected MainCoast concavities only (**C1**). Nesting is strict parent→daughter. Phase H rivers **outlet into Firths** via **River Terminus Sockets**; they do not create this hierarchy.

## Vocabulary (locked)

| Term | Definition | Nesting / traffic | Gameplay |
|------|------------|-------------------|----------|
| **Firth** | Large inlet; sea outlet of **large rivers**; sized for **significant two-way ship traffic** into multiple compound nested daughter inlets | Contains nested **Harborages** + **barrier islands** at/near the mouth | Riverine commerce gateway; later tolls, forts, bridges at river↔Firth junction |
| **Harborage** | Inlet adequate for **working waterfront** and coastal settlement | Contains nested daughter **Coves**; sits inside a Firth (or rarely as a primary on small islands) | Quays, WWF shelf, settlements |
| **Cove** | Small inlet adequate for maritime harvesting, fishing, light structures | Leaf (inside Harborage, or rarely direct on open coast) | Fishing, small craft, local structures |
| **Barrier island** | Positive land fragment at/near Firth mouth | Must **not** seal ocean connectivity | Mouth guardian / channeling |
| **River Terminus Socket** | Typed attachment at **Firth head** for Phase H river spline actors | One primary socket per Firth (v1) | Spline snap target; not a Harborage/Cove terminus for large rivers |

## Classification rules

1. **Firth** — ocean-connected; hosts ≥1 Harborage (or capacity); mouth supports two-way merchant traffic (well above the 10 m Cove floor).
2. **Harborage** — ocean-connected via parent Firth (or open coast on small islands); WWF-capable; may own Coves.
3. **Cove** — smallest named concavity; ≥10 m if “navigable/recognized,” else visual coastline detail only.
4. Barrier islands must leave Firth mouths open (`FloodExteriorAndFillEnclosed` does not seal them).
5. Do **not** call Phase H lakes, dry calderas, or enclosed ponds Firths/Harborages/Coves.
6. **Dual-end Firths on one primary** (Firth-scale inlets on opposite ends of the same island) are intentional superb competitive harborages — keep and prefer this pattern when seeds produce it.

## River Terminus Socket (C1 → Phase H)

| Concern | Owner |
|---------|--------|
| Firth / Harborage / Cove geometry | **C1** Azgaar coast |
| Socket pose + class (`FIHRiverTerminusSocket`) | **C1** (emitted with Firth) |
| River spline actor / pathfind | **Phase H** — downstream end binds to a socket |
| Estuary / delta soft banks | **C3** (optional; does not move socket) |

**Large rivers terminate at Firth sockets only** (v1). Harborages/Coves do not receive primary river termini.

Socket fields: `SocketId`, `IslandIndex`, `FirthIndex`, `LocationLocalCm`, `OutwardTangentXY`, `AcceptanceRadiusCm`, `MinChannelWidthCm`, `bOceanConnected`, optional `UpstreamValleyAxisXY`.

## Acre / sample gates (WB harness)

| Gate | Value | Status |
|------|-------|--------|
| Demo unit | 8,000 acres | Signed |
| WB working primary | **20,000 acres** + `SamplesPerSide` **513** (≥8k acres) | Active |
| Firth-capable primary | **40,000 acres** | **ON** (`bWBUnlockFirthCapable40kBudget`) — denser carve @ realm 13 km NS |
| Production Fibonacci | up to ~2.12M / island | **Gated** until bake + WP on play maps (`bWBUnlockProductionCanonicalAcres=false`) |

## Related

- Summit H/D φ lookup: Low `1/φ⁵≈0.090` (50–180 m) / High `1/φ⁴≈0.146` (160–420 m) / Volc `1/φ³≈0.236` (240–620 m) in `IHCoastGenerationTypes.h`
- Layout AABB: `RealmHalfExtentNSKm` / `RealmHalfExtentEWKm` (φ); aquarium water tank retired
- Acceptance: `Content/InvisibleHand/ACCEPTANCE_GATES.md`
