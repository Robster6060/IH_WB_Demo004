"""
Combined check: confirms LocatorBeacon (2026-09-11) did not disturb the actual SkeletalMeshComponent
setup on AIH_P1C08_MannequinActor - both components inspected together after the user reported
seeing only the new beacon ("pink stick") and asked whether the real mannequin mesh still renders.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/diagnose_mannequin_full.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.load_class(None, "/Script/IH_WB_Demo004.IH_P1C08_MannequinActor"),
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

if actor is None:
    unreal.log_error("[FULLDIAG] spawn failed")
else:
    mesh_comp = actor.get_editor_property("mesh")
    skel = mesh_comp.get_editor_property("skeletal_mesh_asset") if mesh_comp else None
    unreal.log(f"[FULLDIAG] SkeletalMeshComponent: {mesh_comp}")
    unreal.log(f"[FULLDIAG] SkeletalMesh assigned: {skel}")
    unreal.log(f"[FULLDIAG] Mesh visible: {mesh_comp.is_visible() if mesh_comp else None}")
    unreal.log(f"[FULLDIAG] Mesh hidden_in_game: {mesh_comp.get_editor_property('hidden_in_game') if mesh_comp else None}")
    unreal.log(f"[FULLDIAG] Mesh relative_location: {mesh_comp.get_editor_property('relative_location') if mesh_comp else None}")
    unreal.log(f"[FULLDIAG] Mesh relative_scale3d: {mesh_comp.get_editor_property('relative_scale3d') if mesh_comp else None}")

    beacon = actor.get_editor_property("locator_beacon")
    unreal.log(f"[FULLDIAG] LocatorBeacon: {beacon}")
    unreal.log(f"[FULLDIAG] Beacon visible: {beacon.is_visible() if beacon else None}")

    capsule = actor.get_editor_property("capsule_component")
    unreal.log(f"[FULLDIAG] Capsule half-height: {capsule.get_scaled_capsule_half_height() if capsule else None}")
    unreal.log(f"[FULLDIAG] Actor hidden: {actor.get_editor_property('hidden')}")

    unreal.EditorLevelLibrary.destroy_actor(actor)
    unreal.log("[FULLDIAG] Cleaned up.")

unreal.log("[FULLDIAG] Done.")
