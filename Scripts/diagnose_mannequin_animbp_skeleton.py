"""
Checks whether SK_Mannequin's own Skeleton actually matches ThirdPerson_AnimBP's target Skeleton.
A mismatch here wouldn't show up in a component-property inspection (mesh assigned, visible=True,
hidden_in_game=False all still read fine) but WOULD explain a mesh that renders correctly with no
anim instance ticking (e.g. a headless editor-world spawn, which never calls BeginPlay) yet goes
invisible the moment a real PIE session's AnimBP actually starts evaluating a pose against the
wrong skeleton each tick - exactly the gap between the clean diagnostics so far and the user's
real, repeated, zoomed-in-close "nothing there" PIE screenshot.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/diagnose_mannequin_animbp_skeleton.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

SKEL_MESH_PATH = "/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"
ANIM_BP_PATH = "/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP"

skel_mesh = unreal.load_asset(SKEL_MESH_PATH)
unreal.log(f"[SKELDIAG] SK_Mannequin loaded: {skel_mesh}")
mesh_skeleton = None
if skel_mesh:
    mesh_skeleton = skel_mesh.get_editor_property("skeleton")
    unreal.log(f"[SKELDIAG] SK_Mannequin.Skeleton: {mesh_skeleton}")

anim_bp = unreal.load_asset(ANIM_BP_PATH)
unreal.log(f"[SKELDIAG] ThirdPerson_AnimBP loaded: {anim_bp}")
bp_skeleton = None
if anim_bp:
    try:
        bp_skeleton = anim_bp.get_editor_property("target_skeleton")
    except Exception as e:
        unreal.log_warning(f"[SKELDIAG] target_skeleton property read failed: {e}")
    unreal.log(f"[SKELDIAG] ThirdPerson_AnimBP.TargetSkeleton: {bp_skeleton}")

if mesh_skeleton and bp_skeleton:
    match = mesh_skeleton.get_path_name() == bp_skeleton.get_path_name()
    unreal.log(f"[SKELDIAG] MATCH: {match}")
    if not match:
        unreal.log_error(f"[SKELDIAG] SKELETON MISMATCH: mesh uses {mesh_skeleton.get_path_name()}, AnimBP targets {bp_skeleton.get_path_name()}")

unreal.log("[SKELDIAG] Done.")
