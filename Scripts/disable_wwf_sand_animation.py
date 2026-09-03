"""
Removes M_IH_WwfSand's time-based Panner animation (Observation 2, 2026-09-02: "brown hash
marks"/phantom wave ebb-flow moving independent of Waterline Pro/Ocean state). User wants the
effect gone entirely, not just gated to Ocean-on. Zeroes Panner speed rather than deleting nodes
- keeps the material graph structurally intact/reversible, freezes the animation in place.
Run via: UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/disable_wwf_sand_animation.py"
"""
import unreal

MATERIAL_PATH = "/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand"

mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
if not mat:
    raise RuntimeError(f"Could not load {MATERIAL_PATH}")

expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
panners = [e for e in expressions if isinstance(e, unreal.MaterialExpressionPanner)]
unreal.log(f"Found {len(panners)} MaterialExpressionPanner node(s) in {MATERIAL_PATH}")

for i, panner in enumerate(panners):
    before = (panner.get_editor_property("speed_x"), panner.get_editor_property("speed_y"))
    panner.set_editor_property("speed_x", 0.0)
    panner.set_editor_property("speed_y", 0.0)
    after = (panner.get_editor_property("speed_x"), panner.get_editor_property("speed_y"))
    unreal.log(f"  Panner[{i}]: speed_x/y {before} -> {after}")

unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log(f"Recompiled and saved {MATERIAL_PATH} - animation frozen (Panner speeds zeroed, "
           "nodes left in place for easy reversal).")
