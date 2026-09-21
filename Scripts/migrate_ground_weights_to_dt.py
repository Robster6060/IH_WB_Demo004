"""
One-shot, standalone (no unreal import needed - pure CSV transform) migration for the shared-
material plan's Phase 1: adds 5 new columns (groundSandWeight, groundGrassWeight, groundDirtWeight,
groundSnowWeight, groundRockWeight) to DT_ASLSlopeBiome.csv, populated with EXACTLY the values
IH_WB_IslandActor.cpp's GetNaturalisticGroundWeights/GetNaturalisticRockBlendAlpha compute today -
a behavior-preserving migration into data, not new numbers. Run with plain python3, not
UnrealEditor-Cmd (this only rewrites a CSV file on disk).
"""
import csv

CSV_PATH = "D:/Projects/ClaudeProjects/IH_WB_Demo004/Content/InvisibleHand/Data/DataTables/DT_ASLSlopeBiome.csv"

NEW_COLUMNS = [
    "groundSandWeight", "groundGrassWeight", "groundDirtWeight", "groundSnowWeight", "groundRockWeight",
]


def get_naturalistic_ground_weights(terrain_tier):
    """Mirrors IH_WB_IslandActorPrivate::GetNaturalisticGroundWeights exactly."""
    sand = grass = dirt = snow = 0.0
    if terrain_tier in ("WWF", "Shorelands"):
        sand = 1.0
    elif terrain_tier in ("Lowlands", "Midlands"):
        grass = 1.0
    elif terrain_tier == "Highlands":
        grass = 0.4
        dirt = 0.6
    elif terrain_tier == "Montane":
        dirt = 0.5
        snow = 0.5
    elif terrain_tier == "Alpine":
        snow = 1.0
    else:
        grass = 1.0
    return sand, grass, dirt, snow


def get_naturalistic_rock_blend_alpha(min_slope_deg, max_slope_deg, b_slope_agnostic):
    """Mirrors IH_WB_IslandActorPrivate::GetNaturalisticRockBlendAlpha exactly."""
    if b_slope_agnostic:
        return 0.0
    mid_slope_deg = (min_slope_deg + max_slope_deg) * 0.5
    return max(0.0, min(1.0, (mid_slope_deg - 35.0) / (55.0 - 35.0)))


with open(CSV_PATH, "r", newline="", encoding="utf-8") as f:
    reader = csv.reader(f)
    rows = list(reader)

header = rows[0]
assert header[:1] == ["---"], f"Unexpected header start: {header[:1]}"
terrain_tier_idx = header.index("terrainTier")
min_slope_idx = header.index("minSlopeDeg")
max_slope_idx = header.index("maxSlopeDeg")
slope_agnostic_idx = header.index("bSlopeAgnostic")

new_header = header + NEW_COLUMNS
out_rows = [new_header]

for row in rows[1:]:
    terrain_tier = row[terrain_tier_idx]
    min_slope_deg = float(row[min_slope_idx])
    max_slope_deg = float(row[max_slope_idx])
    b_slope_agnostic = row[slope_agnostic_idx].strip().lower() == "true"

    sand, grass, dirt, snow = get_naturalistic_ground_weights(terrain_tier)
    rock = get_naturalistic_rock_blend_alpha(min_slope_deg, max_slope_deg, b_slope_agnostic)

    out_rows.append(row + [f"{sand:g}", f"{grass:g}", f"{dirt:g}", f"{snow:g}", f"{rock:g}"])

with open(CSV_PATH, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerows(out_rows)

print(f"[MIGRATE] Wrote {len(out_rows) - 1} data rows with {len(NEW_COLUMNS)} new columns to {CSV_PATH}")
