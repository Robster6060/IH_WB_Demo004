import unreal

for name in dir(unreal.MaterialEditingLibrary):
    if name.startswith("_"):
        continue
    if "connect" in name.lower() or "property" in name.lower() or "delete" in name.lower():
        unreal.log(f"MaterialEditingLibrary.{name}")

unreal.log("--- MaterialExpressionMaterialFunctionCall properties ---")
mat = unreal.EditorAssetLibrary.load_asset("/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand")
expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
fc = next(e for e in expressions if e.get_name() == "MaterialExpressionMaterialFunctionCall_15")
try:
    props = fc.get_class().get_editor_property("class_default_object") if False else None
except Exception:
    pass
# Enumerate actual UPROPERTYs via the reflection system (not just Python-bound dir()).
for prop_name in ("FunctionInputs", "Inputs", "MaterialFunction"):
    try:
        val = fc.get_editor_property(prop_name)
        unreal.log(f"get_editor_property('{prop_name}') = {val}")
    except Exception as ex:
        unreal.log(f"get_editor_property('{prop_name}') FAILED: {ex}")
