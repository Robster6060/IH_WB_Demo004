"""
Gives this project its own persistent map (it currently boots on /Engine/Maps/Templates/
Template_Default - a shared Engine file, wrong place to save project-specific content like
APCGWorldActor into). Creates a new empty map owned by this project and places one PCGWorldActor
in it, so PCG's native runtime-generation scheduler has a legitimate, persisted place to find its
generation-radius settings in a packaged build (its own auto-create path is #if WITH_EDITOR only,
confirmed via engine source - no runtime fallback at all).

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/build_persistent_map.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MAP_PACKAGE = "/Game/InvisibleHand/Maps/L_IH_WB_Main"

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE)

created = level_subsystem.new_level(MAP_PACKAGE)
assert created, f"[MAPSETUP] Failed to create new level at {MAP_PACKAGE}"
unreal.log(f"[MAPSETUP] Created new level: {MAP_PACKAGE}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world_actor = actor_subsystem.spawn_actor_from_class(unreal.PCGWorldActor, unreal.Vector(0, 0, 0))
if world_actor is None:
    unreal.log_warning("[MAPSETUP] spawn_actor_from_class(PCGWorldActor) returned None - "
                        "NotPlaceable may be blocking direct Python spawn. See script comment for the "
                        "C++ fallback (trigger UPCGSubsystem::GetPCGWorldActor() from an editor-context "
                        "call, which auto-creates it - not itself Python-exposed, needs a small helper).")
else:
    unreal.log(f"[MAPSETUP] Spawned PCGWorldActor: {world_actor.get_name()}")

saved = unreal.EditorLevelLibrary.save_current_level()
unreal.log(f"[MAPSETUP] Save current level result: {saved}")
unreal.log("[MAPSETUP] Done.")
