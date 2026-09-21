"""
Confirms the new AnimationSingleNode fix took effect: GetMesh()'s AnimationMode is no longer the
default AnimationBlueprint-with-no-class combination (a state with no valid per-frame pose source
at all), which - if this was the real, uniform explanation for every prior invisibility symptom -
should let the mesh render its static reference pose regardless of AnimBP.
"""
import unreal

actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.load_class(None, "/Script/IH_WB_Demo004.IH_P1C08_MannequinActor"),
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

mesh_comp = actor.get_editor_property("mesh")
unreal.log(f"[ANIMMODE] SkeletalMesh: {mesh_comp.get_editor_property('skeletal_mesh_asset')}")
unreal.log(f"[ANIMMODE] AnimationMode: {mesh_comp.get_editor_property('animation_mode')}")
unreal.log(f"[ANIMMODE] AnimClass: {mesh_comp.get_editor_property('anim_class')}")
unreal.log(f"[ANIMMODE] Mesh visible: {mesh_comp.is_visible()}")

unreal.EditorLevelLibrary.destroy_actor(actor)
unreal.log("[ANIMMODE] Done.")
