"""
Sanity check for the new LocatorBeacon component on AIH_P1C08_MannequinActor (2026-09-11) - a
bright, tall, thin marker added so a placed Mannequin can actually be spotted from this world's
typical whole-island review camera distance, where the true-scale 1.76m capsule itself is only
a couple of screen pixels. Confirms the component exists, has a mesh/material, and sits where
expected (rising up from the capsule's feet). Cleans up after itself.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/diagnose_mannequin_beacon.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.load_class(None, "/Script/IH_WB_Demo004.IH_P1C08_MannequinActor"),
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

if actor is None:
    unreal.log_error("[BEACONDIAG] spawn_actor_from_class returned None")
else:
    unreal.log(f"[BEACONDIAG] Spawned: {actor.get_name()}")
    beacon = actor.get_editor_property("locator_beacon")
    if beacon is None:
        unreal.log_error("[BEACONDIAG] LocatorBeacon component is None!")
    else:
        mesh = beacon.get_editor_property("static_mesh")
        mat = beacon.get_material(0)
        loc = beacon.get_editor_property("relative_location")
        scale = beacon.get_editor_property("relative_scale3d")
        unreal.log(f"[BEACONDIAG] LocatorBeacon mesh: {mesh}")
        unreal.log(f"[BEACONDIAG] LocatorBeacon material: {mat}")
        unreal.log(f"[BEACONDIAG] LocatorBeacon relative_location: {loc}")
        unreal.log(f"[BEACONDIAG] LocatorBeacon relative_scale3d: {scale}")
        unreal.log(f"[BEACONDIAG] LocatorBeacon visible: {beacon.is_visible()}")
    unreal.EditorLevelLibrary.destroy_actor(actor)
    unreal.log("[BEACONDIAG] Cleaned up spawned actor.")

unreal.log("[BEACONDIAG] Done.")
