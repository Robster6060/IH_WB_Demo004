"""
Deeper investigation, round 2: the Panner-speed-zero fix didn't stop the visible "brown hash
marks" ebb/flow. Either (a) those Panners don't actually feed the visible output, (b) there's a
second animation source not caught by the first pass's narrow class-name filter, or (c) the
actually-rendered material isn't M_IH_WwfSand at all (LoadShelfBandMaterial's fallback chain).
"""
import unreal

for path in [
    "/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand",
    "/Game/InvisibleHand/Materials/Waterline/M_IH_ShelfSand.M_IH_ShelfSand",
]:
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if not mat:
        unreal.log(f"COULD NOT LOAD: {path}")
        continue
    unreal.log(f"=== {path} ===")
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
    unreal.log(f"Total expressions: {len(expressions)}")
    for expr in expressions:
        cls_name = expr.get_class().get_name()
        line = f"  [{expr.get_name()}] class={cls_name}"
        if cls_name == "MaterialExpressionPanner":
            sx = expr.get_editor_property("speed_x")
            sy = expr.get_editor_property("speed_y")
            line += f" speed_x={sx} speed_y={sy}"
        unreal.log(line)

    # What actually feeds BaseColor / Opacity / WorldPositionOffset / Normal / Emissive?
    for prop_enum_name in ("MP_BASE_COLOR", "MP_OPACITY", "MP_WORLD_POSITION_OFFSET", "MP_NORMAL", "MP_EMISSIVE_COLOR"):
        try:
            prop_enum = getattr(unreal.MaterialProperty, prop_enum_name)
            connected_expr = unreal.MaterialEditingLibrary.get_material_property_input_expression(mat, prop_enum)
            name = connected_expr.get_name() if connected_expr else "None"
            cls = connected_expr.get_class().get_name() if connected_expr else "None"
            unreal.log(f"  {prop_enum_name} <- [{name}] class={cls}")
        except Exception as ex:
            unreal.log(f"  {prop_enum_name}: FAILED {ex}")
