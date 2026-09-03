# IH_WB_Demo002

Unreal Engine **5.8** C++ project for Invisible Hand World Builder heightmap islands.

Forked from `IH_WB_Heightmap` (`D:\Projects\UE58Projects\IH_WB_Heightmap`, commit `45a4c29`) for a from-scratch rewrite of the coastline/inlet/sector generation logic, preserving the existing Arbor harness UI, ship nav, and DevView tooling.

## Goals

- Preserve Arbor harness: fly camera, HUD, minimap, Island Nav, G/W/B/C/D Build Palette, Merchantman
- RealmSeed format: `AAAAA#` (5-letter word + digit 2–7 = island count)
- Azgaar multi-operator field composition (not Archipelago)
- Interior profile: **Low only** — HIGH/VOLC procedural generation retired (`IH-DEC-064`/`069`) in favor of player-placed Terrain Stamps
- Fill interior depressions &lt; 0 ASL (no inland seas)
- Detachable IslandMesh + contiguous Sea Shelf WWF per island

## Sources

- Harness: `D:\Projects\CursorProjects\IH_P1C12_Arbor`
- Heightfield ensemble: `D:\Projects\CodexProjects\IH_CX_Demo001` (`FIHHeightfieldCoastGenerator`)
- Azgaar: Fantasy Map Generator heightmap templates / generator

## Build

Open `IH_WB_Demo002.uproject` in UE 5.8, or:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" IH_WB_Demo002Editor Win64 Development -Project="D:\Projects\ClaudeProjects\IH_WB_Demo002\IH_WB_Demo002.uproject" -WaitMutex
```
