"""
Checks the exact same class of bug already found once this session for a different mesh type
(M_TerrainStampASLBand missing bUsedWithNanite) - here checking whether SK_Mannequin's two
materials (M_UE4Man_Body, M_UE4Man_ChestLogo) have bUsedWithSkeletalMesh set. If not, the required
skeletal-mesh vertex-factory shader permutation may never have been compiled, which could mean the
mesh silently fails to render at all in a normal PIE/game context even though every other component
property checks out clean (matches: AnimBP on or off made no difference, ruling out animation).
"""
import unreal

BODY_MAT_PATH = "/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Character/Materials/M_UE4Man_Body"
LOGO_MI_PATH = "/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Character/Materials/M_UE4Man_ChestLogo"

body_mat = unreal.load_asset(BODY_MAT_PATH)
unreal.log(f"[SKELMATFLAG] M_UE4Man_Body: {body_mat}")
if body_mat:
    try:
        used_with_skel = body_mat.get_editor_property("used_with_skeletal_mesh")
        unreal.log(f"[SKELMATFLAG] M_UE4Man_Body.used_with_skeletal_mesh = {used_with_skel}")
    except Exception as e:
        unreal.log_warning(f"[SKELMATFLAG] read failed: {e}")

logo_mi = unreal.load_asset(LOGO_MI_PATH)
unreal.log(f"[SKELMATFLAG] M_UE4Man_ChestLogo (MI): {logo_mi}")
if logo_mi:
    parent = logo_mi.get_editor_property("parent")
    unreal.log(f"[SKELMATFLAG] M_UE4Man_ChestLogo parent: {parent}")
    if parent:
        try:
            used_with_skel = parent.get_editor_property("used_with_skeletal_mesh")
            unreal.log(f"[SKELMATFLAG] parent.used_with_skeletal_mesh = {used_with_skel}")
        except Exception as e:
            unreal.log_warning(f"[SKELMATFLAG] parent read failed: {e}")

unreal.log("[SKELMATFLAG] Done.")
