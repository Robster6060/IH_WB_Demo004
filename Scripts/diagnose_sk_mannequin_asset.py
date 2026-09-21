"""
Deeper asset-level check on SK_Mannequin itself, now that both the AnimBP-enabled AND
AnimBP-disabled tests rendered nothing - ruling out animation evaluation as the cause and pointing
back at the mesh/component itself. Checks Nanite settings (this exact project already found one
real Nanite-usage-flag bug this session, for an unrelated static mesh) and basic render-data
sanity (LOD count, vertex/triangle count) directly on the asset.
"""
import unreal

SKEL_MESH_PATH = "/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"

mesh = unreal.load_asset(SKEL_MESH_PATH)
unreal.log(f"[SKASSET] SK_Mannequin: {mesh}")
if mesh:
    try:
        nanite = mesh.get_editor_property("nanite_settings")
        unreal.log(f"[SKASSET] NaniteSettings.enabled: {nanite.enabled}")
    except Exception as e:
        unreal.log_warning(f"[SKASSET] nanite_settings read failed: {e}")

    try:
        lod_count = mesh.get_lod_num()
        unreal.log(f"[SKASSET] LOD count: {lod_count}")
    except Exception as e:
        unreal.log_warning(f"[SKASSET] get_lod_num failed: {e}")

    try:
        import unreal as u
        lib = u.SkeletalMeshEditorSubsystem
    except Exception:
        lib = None

    skeleton = mesh.get_editor_property("skeleton")
    unreal.log(f"[SKASSET] Skeleton: {skeleton}")

    materials = mesh.get_editor_property("materials")
    unreal.log(f"[SKASSET] Material slots: {len(materials)}")
    for i, m in enumerate(materials):
        unreal.log(f"[SKASSET]   slot[{i}] = {m.material_interface if m else None}")

    bounds = mesh.get_editor_property("bounds") if mesh.has_editor_property("bounds") else None
    unreal.log(f"[SKASSET] bounds property present: {bounds is not None}")

unreal.log("[SKASSET] Done.")
