"""
Checks whether the manually-copied SKM_Manny_Simple (from UE5.8's own bundled TP_ThirdPersonBP
template content, no marketplace/download needed) resolves cleanly at its new project-local path -
correct Skeleton, correct Materials, no broken references - before wiring it into
AIH_P1C08_MannequinActor as a swap-in replacement for the legacy vendor SK_Mannequin.
"""
import unreal

MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"

mesh = unreal.load_asset(MESH_PATH)
unreal.log(f"[MANNYIMPORT] SKM_Manny_Simple: {mesh}")
if mesh:
    try:
        skeleton = mesh.get_editor_property("skeleton")
        unreal.log(f"[MANNYIMPORT] Skeleton: {skeleton}")
    except Exception as e:
        unreal.log_warning(f"[MANNYIMPORT] skeleton read failed: {e}")

    try:
        materials = mesh.get_editor_property("materials")
        unreal.log(f"[MANNYIMPORT] Material slots: {len(materials)}")
        for i, m in enumerate(materials):
            unreal.log(f"[MANNYIMPORT]   slot[{i}] = {m.material_interface if m else None}")
    except Exception as e:
        unreal.log_warning(f"[MANNYIMPORT] materials read failed: {e}")
else:
    unreal.log_error("[MANNYIMPORT] Failed to load - asset not found at this path")

unreal.log("[MANNYIMPORT] Done.")
