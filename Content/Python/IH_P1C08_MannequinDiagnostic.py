"""
IH_P1C08_MannequinDiagnostic.py

Headless diagnostic for the Mannequin "pink stick" investigation (2026-09-12).
Run via UnrealEditor-Cmd.exe -ExecutePythonScript="Content/Python/IH_P1C08_MannequinDiagnostic.py"

Report-only: inspects the SKM_Manny_Simple / SK_Mannequin skeletal mesh assets and
scans every level under Content/ for placed AIH_P1C08_MannequinActor instances,
flagging any sitting near the Red Cube's canonical center-top point. Does NOT
delete or modify anything. A separate, explicit follow-up pass (same author,
different script/flag) performs any deletion once a human has reviewed this
report.
"""

import unreal

REPORT_LINES = []


def log(msg):
    REPORT_LINES.append(str(msg))
    unreal.log(msg)


def describe_skeletal_mesh(asset_path, label):
    log("")
    log("=== %s (%s) ===" % (label, asset_path))
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        log("  FAILED TO LOAD ASSET")
        return

    skeleton = None
    try:
        skeleton = asset.get_editor_property("skeleton")
    except Exception as e:
        log("  Could not read 'skeleton' property: %s" % e)

    if skeleton is None:
        log("  Skeleton: NONE (invalid/unassigned)")
    else:
        log("  Skeleton: %s" % skeleton.get_path_name())

    try:
        num_materials = asset.get_num_materials() if hasattr(asset, "get_num_materials") else None
    except Exception:
        num_materials = None

    try:
        materials = asset.get_editor_property("materials")
        log("  Material slots: %d" % len(materials))
        for i, slot in enumerate(materials):
            mat_iface = None
            try:
                mat_iface = slot.get_editor_property("material_interface")
            except Exception:
                pass
            log("    [%d] %s" % (i, mat_iface.get_path_name() if mat_iface else "NONE"))
    except Exception as e:
        log("  Could not read 'materials' property: %s" % e)


def scan_levels_for_mannequins():
    log("")
    log("=== Level scan for placed AIH_P1C08_MannequinActor instances ===")

    # Red Cube canonical center-top point, per IH_WB_Demo004GameMode.cpp:
    #   CubeCenterXCm = 4000, CubeCenterYCm = 0
    #   CubeBottomZCm = -2500, BuoyantCubeHalfExtentCm = 5000
    #   CubeCenterZCm = CubeBottomZCm + HalfExtent = 2500
    #   CubeTopZCm    = CubeCenterZCm + HalfExtent = 7500
    cube_top = unreal.Vector(4000.0, 0.0, 7500.0)
    flag_radius_cm = 1500.0

    mannequin_class = unreal.load_class(None, "/Script/IH_WB_Demo004.IH_P1C08_MannequinActor")
    if mannequin_class is None:
        log("  Could not load class /Script/IH_WB_Demo004.IH_P1C08_MannequinActor")
        return

    all_assets = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
    map_paths = sorted(set(p for p in all_assets if p.lower().endswith(".umap") or
                            unreal.EditorAssetLibrary.find_asset_data(p).asset_class_path.asset_name == "World"))
    if not map_paths:
        # Fallback: filter by asset data class name == "World"
        for p in all_assets:
            try:
                data = unreal.EditorAssetLibrary.find_asset_data(p)
                if str(data.asset_class_path.asset_name) == "World":
                    map_paths.append(p)
            except Exception:
                continue
        map_paths = sorted(set(map_paths))

    log("  Found %d level asset(s) under /Game" % len(map_paths))

    editor_level_lib = unreal.EditorLevelLibrary
    found_any = False

    for map_path in map_paths:
        try:
            editor_level_lib.load_level(map_path)
        except Exception as e:
            log("  Could not load level %s: %s" % (map_path, e))
            continue

        actors = editor_level_lib.get_all_level_actors()
        mannequins = [a for a in actors if a and a.get_class() == mannequin_class]
        if not mannequins:
            continue

        found_any = True
        log("  Level: %s -- %d Mannequin instance(s)" % (map_path, len(mannequins)))
        for m in mannequins:
            loc = m.get_actor_location()
            dist = ((loc.x - cube_top.x) ** 2 + (loc.y - cube_top.y) ** 2) ** 0.5
            z_diff = abs(loc.z - cube_top.z)
            flagged = dist <= flag_radius_cm and z_diff <= flag_radius_cm
            log("    %s @ (%.0f, %.0f, %.0f) cm  dist_to_cube_top_xy=%.0f cm  %s" % (
                m.get_name(), loc.x, loc.y, loc.z, dist,
                "<-- FLAGGED: near Red Cube center-top" if flagged else ""))

    if not found_any:
        log("  No placed AIH_P1C08_MannequinActor instances found in any saved level.")
        log("  (Consistent with the Red-Cube test placement having been PIE-only / already removed.)")


def main():
    log("IH_P1C08 Mannequin Diagnostic -- %s" % unreal.SystemLibrary.get_engine_version())

    describe_skeletal_mesh(
        "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple", "SKM_Manny_Simple (current)")
    describe_skeletal_mesh(
        "/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin", "SK_Mannequin (legacy)")

    scan_levels_for_mannequins()

    log("")
    log("=== END REPORT ===")

    report_path = unreal.Paths.project_saved_dir() + "IH_P1C08_MannequinDiagnostic_Report.txt"
    report_path = unreal.Paths.convert_relative_path_to_full(report_path)
    with open(report_path, "w") as f:
        f.write("\n".join(REPORT_LINES))
    unreal.log("Report written to: %s" % report_path)


main()
