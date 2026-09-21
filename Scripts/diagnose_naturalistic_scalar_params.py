import unreal
mat = unreal.EditorAssetLibrary.load_asset("/Game/InvisibleHand/Materials/M_IH_IslandGroundNaturalistic.M_IH_IslandGroundNaturalistic")
MEL = unreal.MaterialEditingLibrary
exprs = MEL.get_material_expressions(mat)
for e in exprs:
    if e.get_class().get_name() == "MaterialExpressionScalarParameter":
        unreal.log(f"SCALARPARAM: {e.get_editor_property('parameter_name')}")
unreal.log("[SCALARDIAG] Done.")
