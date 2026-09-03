import unreal

MATERIAL_PATH = "/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand"
mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
by_name = {e.get_name(): e for e in expressions}

zero_scalar = unreal.MaterialEditingLibrary.create_material_expression(
    mat, unreal.MaterialExpressionConstant, -700, 1400)
zero_scalar.set_editor_property("r", 0.0)

for target_name in ("MaterialExpressionLinearInterpolate_34", "MaterialExpressionLinearInterpolate_2"):
    lerp = by_name[target_name]
    ok = unreal.MaterialEditingLibrary.connect_material_expressions(zero_scalar, "", lerp, "Alpha")
    unreal.log(f"connect zero_scalar -> {target_name}.Alpha = {ok}")

unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log(f"Saved {MATERIAL_PATH}.")
