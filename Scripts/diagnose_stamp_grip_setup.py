"""
Sanity check for Stage 12b's new bounding-box grip components on AIH_TerrainStampActor - confirms
the actor still spawns cleanly, the gizmo material now resolves (built just before this script runs
by build_stamp_gizmo_material.py), and the 9 grip markers + box mesh exist with real static meshes
assigned. Cleans up after itself. Cannot exercise the actual drag math (BeginGripDrag/UpdateGripDrag
are plain C++ methods, not UFUNCTION-exposed to Python) - that needs the user's own PIE pass.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/diagnose_stamp_grip_setup.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.load_class(None, "/Script/IH_WB_Demo004.IH_TerrainStampActor"),
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

if actor is None:
    unreal.log_error("[GRIPDIAG] spawn_actor_from_class returned None")
else:
    unreal.log(f"[GRIPDIAG] Spawned: {actor.get_name()}")

    bbox_mesh = actor.get_editor_property("bounding_box_mesh")
    unreal.log(f"[GRIPDIAG] BoundingBoxMesh: {bbox_mesh}")
    if bbox_mesh:
        unreal.log(f"[GRIPDIAG] BoundingBoxMesh static mesh: {bbox_mesh.get_editor_property('static_mesh')}")
        mat = bbox_mesh.get_material(0)
        unreal.log(f"[GRIPDIAG] BoundingBoxMesh material slot 0: {mat}")
        if mat:
            parent = mat.get_editor_property("parent") if hasattr(mat, "get_editor_property") else None
            unreal.log(f"[GRIPDIAG] BoundingBoxMesh material parent: {parent}")

    grips = actor.get_editor_property("grip_markers")
    unreal.log(f"[GRIPDIAG] GripMarkers count: {len(grips)}")
    for i, g in enumerate(grips):
        mesh = g.get_editor_property("static_mesh") if g else None
        mat = g.get_material(0) if g else None
        unreal.log(f"[GRIPDIAG]   grip[{i}] mesh={mesh} mat_set={mat is not None} visible={g.is_visible() if g else None}")

    unreal.EditorLevelLibrary.destroy_actor(actor)
    unreal.log("[GRIPDIAG] Cleaned up spawned actor.")

unreal.log("[GRIPDIAG] Done.")
