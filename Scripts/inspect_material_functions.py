import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand")
expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
func_calls = [e for e in expressions if isinstance(e, unreal.MaterialExpressionMaterialFunctionCall)]
unreal.log(f"Found {len(func_calls)} MaterialExpressionMaterialFunctionCall node(s)")

for fc in func_calls:
    func = fc.get_editor_property("material_function")
    if not func:
        unreal.log(f"  [{fc.get_name()}] material_function = None")
        continue
    func_path = func.get_path_name()
    unreal.log(f"  [{fc.get_name()}] -> {func_path}")
    inner_exprs = unreal.MaterialEditingLibrary.get_material_function_expressions(func)
    unreal.log(f"    Inner expression count: {len(inner_exprs)}")
    time_or_pan = [e for e in inner_exprs if e.get_class().get_name() in
                   ("MaterialExpressionPanner", "MaterialExpressionTime", "MaterialExpressionSine")]
    for e in time_or_pan:
        cls_name = e.get_class().get_name()
        line = f"    HIT: [{e.get_name()}] class={cls_name}"
        if cls_name == "MaterialExpressionPanner":
            line += f" speed_x={e.get_editor_property('speed_x')} speed_y={e.get_editor_property('speed_y')}"
        unreal.log(line)
    if not time_or_pan:
        unreal.log(f"    No Panner/Time/Sine nodes directly in this function's top level.")
        for e in inner_exprs:
            unreal.log(f"      inner class={e.get_class().get_name()}")

# Also check referencers of these functions, so we know the blast radius before changing anything.
for fc in func_calls:
    func = fc.get_editor_property("material_function")
    if func:
        refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(func.get_path_name(), True)
        unreal.log(f"Referencers of {func.get_path_name()}: {list(refs)}")
