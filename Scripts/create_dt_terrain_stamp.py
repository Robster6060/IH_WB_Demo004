"""
Creates the DT_TerrainStamp DataTable asset (2026-09-09) backing FIHTerrainStampMeshCatalog -
replaces the dead procedural-stamp height-grid path. One row per EIHTerrainStampId (22 rows,
matching IHInvisibleHandSpec::TerrainStampCount); only "Mesa" gets a real static mesh
(TableMesa001) to start. Adding a future stamp becomes: drop its mesh under the right
TS_Vertical/TS_Inverted subfolder, then edit that one row in-editor - no C++ change, no recompile.
Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/create_dt_terrain_stamp.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import json
import unreal

TABLE_PATH = "/Game/InvisibleHand/World/TerrainStamps"
TABLE_NAME = "DT_TerrainStamp"
TABLE_PACKAGE = f"{TABLE_PATH}/{TABLE_NAME}"

ROW_STRUCT = unreal.load_object(None, "/Script/IH_WB_Demo004.IHTerrainStampMeshRow")
assert ROW_STRUCT is not None, "Could not find row struct /Script/IH_WB_Demo004.IHTerrainStampMeshRow"

MESA_MESH_PATH = "/Game/InvisibleHand/World/TerrainStamps/TS_Vertical/Domes/TableMesa001/TableMesa001.TableMesa001"

# (StampId enum value string, Category, Family) - Category/Family are placeholder best-fit
# classifications for the 21 not-yet-available rows (Mesh stays empty); only Mesa matters precisely
# today, since it's the only row getting a real mesh.
ROWS = [
    ("Hill", "Vertical", "Dome"),
    ("Knoll", "Vertical", "Dome"),
    ("Ridge", "Vertical", "Ridge"),
    ("Mesa", "Vertical", "Dome"),
    ("Butte", "Vertical", "Dome"),
    ("VolcanoCone", "Vertical", "Dome"),
    ("Escarpment", "Vertical", "Cliff"),
    ("CliffStamp", "Vertical", "Cliff"),
    ("TerracedSlope", "Vertical", "Cliff"),
    ("Spur", "Vertical", "Ridge"),
    ("SummitCap", "Vertical", "Dome"),
    ("Valley", "Inverted", "Trench"),
    ("Basin", "Inverted", "Bowl"),
    ("Sink", "Inverted", "Bowl"),
    ("Canyon", "Inverted", "Trench"),
    ("Gorge", "Inverted", "Trench"),
    ("Crater", "Inverted", "Bowl"),
    ("LakeBed", "Inverted", "Bowl"),
    ("RiverChannel", "Inverted", "Trench"),
    ("Cove", "Inverted", "Scoop"),
    ("HarborScoop", "Inverted", "Scoop"),
    ("IslandShelf", "Vertical", "Dome"),
]

json_rows = []
for stamp_id, category, family in ROWS:
    row = {
        "Name": stamp_id,
        "StampId": stamp_id,
        "Category": category,
        "Family": family,
        "PlacementMode": "SingleDrop",
        "Mesh": MESA_MESH_PATH if stamp_id == "Mesa" else "",
        "DisplayLabel": stamp_id,
    }
    json_rows.append(row)

json_string = json.dumps(json_rows)

# --- Create (or reuse) the DataTable asset ---
if unreal.EditorAssetLibrary.does_asset_exist(TABLE_PACKAGE):
    unreal.log(f"[DT_TERRAINSTAMP] {TABLE_PACKAGE} already exists - reusing and repopulating.")
    data_table = unreal.load_asset(TABLE_PACKAGE)
else:
    factory = unreal.DataTableFactory()
    factory.struct = ROW_STRUCT
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    data_table = asset_tools.create_asset(TABLE_NAME, TABLE_PATH, unreal.DataTable, factory)
    unreal.log(f"[DT_TERRAINSTAMP] Created {TABLE_PACKAGE}")

assert data_table is not None, "Failed to create or load DT_TerrainStamp"

problems = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(data_table, json_string)
unreal.log(f"[DT_TERRAINSTAMP] fill_data_table_from_json_string problems: {problems}")

row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(data_table)
unreal.log(f"[DT_TERRAINSTAMP] Row names after import ({len(row_names)}): {row_names}")
assert len(row_names) == len(ROWS), f"Expected {len(ROWS)} rows, got {len(row_names)}"

unreal.EditorAssetLibrary.save_asset(TABLE_PACKAGE)
unreal.log(f"[DT_TERRAINSTAMP] Saved {TABLE_PACKAGE} with {len(ROWS)} rows.")
unreal.log("[DT_TERRAINSTAMP] Done.")
