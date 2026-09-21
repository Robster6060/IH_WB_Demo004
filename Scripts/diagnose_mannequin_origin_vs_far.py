"""
Phase 0 decisive test: does the Mannequin's SkeletalMeshComponent's own computed render bounds
degrade at a real gameplay world-space distance from the origin, vs. right at (0,0,0)? This checks
the bounds computation directly (UpdateBounds() runs on any transform change, even without
BeginPlay/animation ticking) rather than guessing - if bounds are sane near the origin but
degenerate (near-zero extent, huge/NaN sphere radius, or a wildly displaced Origin relative to the
actor) far out, that's the precision mechanism confirmed directly, matching the plan's Phase 0.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/diagnose_mannequin_origin_vs_far.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

FAR_LOCATION = unreal.Vector(-115666.0, -2413832.0, 410.0)  # a real logged placement this session
ORIGIN_LOCATION = unreal.Vector(0.0, 0.0, 410.0)


def inspect_at(label, location):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.load_class(None, "/Script/IH_WB_Demo004.IH_P1C08_MannequinActor"),
        location, unreal.Rotator(0, 0, 0))
    if actor is None:
        unreal.log_error(f"[ORIGINVSFAR] {label}: spawn failed")
        return
    mesh_comp = actor.get_editor_property("mesh")
    actor_loc = actor.get_actor_location()
    unreal.log(f"[ORIGINVSFAR] {label}: actor_location={actor_loc}")
    if mesh_comp:
        try:
            bounds = mesh_comp.get_editor_property("bounds")
            unreal.log(f"[ORIGINVSFAR] {label}: mesh bounds = {bounds}")
        except Exception as e:
            unreal.log_warning(f"[ORIGINVSFAR] {label}: bounds property read failed: {e}")
        world_bounds_origin = mesh_comp.bounds.origin if hasattr(mesh_comp, "bounds") else None
        unreal.log(f"[ORIGINVSFAR] {label}: mesh.is_visible={mesh_comp.is_visible()}")
        socket_loc = mesh_comp.get_socket_location("root") if mesh_comp.does_socket_exist("root") else None
        unreal.log(f"[ORIGINVSFAR] {label}: root socket loc = {socket_loc}")
    beacon = actor.get_editor_property("locator_beacon")
    if beacon:
        try:
            beacon_bounds = beacon.get_editor_property("bounds")
            unreal.log(f"[ORIGINVSFAR] {label}: beacon bounds = {beacon_bounds}")
        except Exception as e:
            unreal.log_warning(f"[ORIGINVSFAR] {label}: beacon bounds read failed: {e}")
    unreal.EditorLevelLibrary.destroy_actor(actor)


inspect_at("ORIGIN", ORIGIN_LOCATION)
inspect_at("FAR", FAR_LOCATION)
unreal.log("[ORIGINVSFAR] Done.")
