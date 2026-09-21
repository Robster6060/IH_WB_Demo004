"""
Confirms the AnimBP-disable diagnostic (2026-09-11) actually took effect: mesh still assigned,
anim_class now None. Run after rebuilding, before handing back to the user's PIE test.
"""
import unreal

actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.load_class(None, "/Script/IH_WB_Demo004.IH_P1C08_MannequinActor"),
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

mesh_comp = actor.get_editor_property("mesh")
unreal.log(f"[ANIMDISABLE] SkeletalMesh: {mesh_comp.get_editor_property('skeletal_mesh_asset')}")
unreal.log(f"[ANIMDISABLE] AnimClass: {mesh_comp.get_editor_property('anim_class')}")
unreal.log(f"[ANIMDISABLE] Mesh visible: {mesh_comp.is_visible()}")

unreal.EditorLevelLibrary.destroy_actor(actor)
unreal.log("[ANIMDISABLE] Done.")
