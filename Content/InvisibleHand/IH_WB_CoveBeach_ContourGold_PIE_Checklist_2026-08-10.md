# Broad Highlands Cove Beachfront — Re-PIE Shot Checklist — 2026-08-10

**Status: C1 chords PASS (Grab1–4). C6 planform ~15% purple (B2 PASS closer). C2 Minecraft stairs PARTIAL FAIL. Coast-belt authority + fjord daughter/half-moon polish shipped — Human Grab A–E eye OPEN.**  
**Handoff (current):** `IH_WB_Heightmap_Handoff_2026-08-11.md`

**Dual failure (do not conflate):**
- **A — Minecraft MS stairs** on IslandMesh (mesh topology). Fixed path: ContourGold coastal-belt remesh into IslandMesh (no Cheb omit). Belt authority polish: max-edge **4.0×**, inland **1.5×**, verts **720**.
- **B — Bulbous planform** — densify B2 PASS closer (~15% purple). Fjord polish: long stubs → mid-spine daughters; short → half-moon coves.

**Do not reopen** Grade for Minecraft-only debt. No samples-1025 / Fibonacci / SandApron / full Azgaar rewrite without ask.  
**Do No Harm:** If C1 chords return → set `bContourGoldCoastBeltEnabled=false` immediately (rim v2 stays OFF).

**Project:** `D:\Projects\UE58Projects\IH_WB_Heightmap`

**Juncture:** `juncture/ih-wb-waterline-clamp-2026-08-10`

**Canon:** WWF inland **40 m**; dry walk ≤**8°** ASL **0→15 m**; Theater **20×12×7 m**.

**TheaterYellow:** walkable Beach theater pad (ASL 0→15 m, ≤8°, ~20×12×7 m) — **not** “any yellow Features stroke.” Yellow on sheer face = class mis-paint (Bluff should be magenta).

---

## Agent scorecard (locked)

| Gate | Verdict |
| --- | --- |
| Flood-fill chords R1/R2 / C1 | **PASS** under coast belt (Grab1–4) — must hold after belt authority polish |
| Rim v2 silhouette N2 | **FAIL (method)** — flag **OFF** |
| Minecraft IslandMesh (A) | Open — belt authority polish eye (Grab A/B) |
| Planform nested density (B) | **Closer** (~15% purple); fjord daughter/half-moon eye (Grab D/E) |
| Grade / TheaterYellow | Grade **PASS** hold; TheaterYellow pad eye (Grab C) |
| Samples-1025 / Fibonacci | **Deferred** |

**Features strokes:** reporters only. Contours gold=ASL0; Contours magenta=WWF−25. IslandMesh = gameplay walk/collision/spawn.

---

## Session setup

Ocean OFF · Contours ON · Features ON · Clouds OFF · **GrabContrast ON** · Samples **513** · Speed **1×**

---

## Report-back after polish build (Grab A–E)

Paste Output Log every shot:

```text
coastBelt enabled=? coastBeltTris=? coastBeltRejected=? | rimStrip enabled=0 | hardSnap=2 | waterlineClamp=1 | mixedClampTris=
```

| Grab | Gate | Seed / framing | Toggles | Report |
| --- | --- | --- | --- | --- |
| **Grab A** | C2 / belt | `ABBEY3` firth ASL **500–700** | Ocean ON; Contours OFF; Features OFF | MinecraftStairs Y/N; stairPct≈; closerToGold Y/N; chords Y/N |
| **Grab B** | C2 / belt | `ALERT4` ASL **400–600** | Ocean ON; Contours OFF; Features OFF | Same; compare `coastBeltRejected` vs prior ~3051 |
| **Grab C** | C3 TheaterYellow | `ALERT4` **true beach lip** ASL **40–100** | Contours OFF; Features ON | Grade; TheaterPad20×12×7 Y/N; yellowOnBluff Y/N |
| **Grab D** | Daughter / cove | `POKED4` ASL **~1500–2500** | Ocean ON; Contours OFF | purpleArcPct≈; daughtersOnFjords Y/N; shortHalfMoon Y/N; canalLook |
| **Grab E** | Nest close-up | `POKED4`/`ABBEY3` one fjord ASL **400–800** | Contours ON optional | daughtersVisible Y/N; note |

**Minimum paste:**

```text
=== SETUP === Samples 513 | GrabContrast ON
=== LOG === coastBelt enabled=? coastBeltTris=? coastBeltRejected=? | rimStrip enabled=0 | hardSnap=2 | waterlineClamp=1 | mixedClampTris=

GrabA ABBEY3 ASL=? MinecraftStairs=Y/N stairPct≈ closerToGold=Y/N chords=Y/N
GrabB ALERT4 ASL=? MinecraftStairs=Y/N stairPct≈ coastBeltRejected=?
GrabC ALERT4 ASL=? Grade= TheaterPad20x12x7=Y/N yellowOnBluff=Y/N
GrabD POKED4 purpleArcPct≈ daughtersOnFjords=Y/N shortHalfMoon=Y/N canalLook=
GrabE closeupSeed= daughtersVisible=Y/N note=
```

**Stop rule:** Grab A chords = Y → STOP — set `bContourGoldCoastBeltEnabled=false`.

---

## COPY/PASTE — legacy C1/C2/C3 (reference)

```text
=== SETUP ===
Ocean OFF|ON | Contours ON/OFF | Features ON/OFF | GrabContrast ON | Samples 513
Paste: coastBelt enabled=? coastBeltTris=? coastBeltRejected=? | rimStrip enabled=0 | hardSnap=2 | waterlineClamp=1 | mixedClampTris=

=== DO NOT ===
Unlock samples-1025 / Fibonacci / SandApron / Chaikin SeaRoots
Re-enable bContourGoldRimStripEnabled (method FAIL)
Flatten Bluff / reopen Grade for Minecraft-only debt
Patch chords forward if C1 FAIL — disable coast belt first
```
