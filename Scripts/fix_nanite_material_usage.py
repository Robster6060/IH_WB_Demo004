"""
Fixes the "missing usage flag Nanite! Default Material will be used in game." warning discovered
in the 2026-09-11 PIE log (Saved/Logs/IH_WB_Demo004.log:2779) for
MI_TerrainStamp_TableMesa001_ASLBand (parent: M_TerrainStampASLBand). Without bUsedWithNanite set
on the base UMaterial, UE substitutes the engine default material for Nanite-rendered draw calls
in game/PIE - meaning the real ASL-band coloring never actually renders on a packaged/PIE stamp,
only in the editor's non-Nanite preview path.

Also checks the two candidate base materials the selection/passive-tint MID
(AIH_TerrainStampActor::CreateStampTintMaterial) is built from
(/Engine/BasicShapes/BasicShapeMaterial, /Engine/EngineMaterials/FlattenMaterial) for the same
flag, since a tinted stamp is a Nanite static mesh too and would suffer the identical substitution
if either base material also lacks the flag.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/fix_nanite_material_usage.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MATERIALS_TO_CHECK = [
    "/Game/InvisibleHand/World/TerrainStamps/M_TerrainStampASLBand",
    "/Engine/BasicShapes/BasicShapeMaterial",
    "/Engine/EngineMaterials/FlattenMaterial",
]

MEL = unreal.MaterialEditingLibrary

for path in MATERIALS_TO_CHECK:
    mat = unreal.load_asset(path)
    if mat is None:
        unreal.log_warning(f"[NANITEFIX] Could not load {path} - skipping")
        continue

    if not isinstance(mat, unreal.Material):
        unreal.log(f"[NANITEFIX] {path} is a {type(mat).__name__}, not a base Material - skipping (flag lives on the base Material)")
        continue

    before = mat.get_editor_property("used_with_nanite")
    unreal.log(f"[NANITEFIX] {path}: used_with_nanite={before}")
    if not before:
        mat.set_editor_property("used_with_nanite", True)
        MEL.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(path)
        after = mat.get_editor_property("used_with_nanite")
        unreal.log(f"[NANITEFIX] {path}: set used_with_nanite=True, recompiled, saved. Verified={after}")

unreal.log("[NANITEFIX] Done.")
