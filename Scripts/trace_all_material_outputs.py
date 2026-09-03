import unreal

mat = unreal.EditorAssetLibrary.load_asset("/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand")

props_to_check = [
    "MP_BASE_COLOR", "MP_METALLIC", "MP_SPECULAR", "MP_ROUGHNESS", "MP_EMISSIVE_COLOR",
    "MP_OPACITY", "MP_OPACITY_MASK", "MP_NORMAL", "MP_WORLD_POSITION_OFFSET",
    "MP_SUBSURFACE_COLOR", "MP_CUSTOM_DATA0", "MP_CUSTOM_DATA1", "MP_AMBIENT_OCCLUSION",
    "MP_REFRACTION", "MP_PIXEL_DEPTH_OFFSET", "MP_ANISOTROPY", "MP_TANGENT",
]

for prop_name in props_to_check:
    try:
        prop_enum = getattr(unreal.MaterialProperty, prop_name)
    except AttributeError:
        continue
    try:
        node = unreal.MaterialEditingLibrary.get_material_property_input_node(mat, prop_enum)
        if node:
            unreal.log(f"{prop_name} <- [{node.get_name()}] class={node.get_class().get_name()}")
        else:
            unreal.log(f"{prop_name} <- (unconnected)")
    except Exception as ex:
        unreal.log(f"{prop_name}: FAILED {ex}")

# Also: does the wave-function-call node (now orphaned from WPO, if it ever was connected there)
# still show up anywhere? And what feeds it (World Position input, typically Absolute World Position).
expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
fc = next((e for e in expressions if e.get_name() == "MaterialExpressionMaterialFunctionCall_15"), None)
unreal.log(f"MaterialExpressionMaterialFunctionCall_15 still present: {fc is not None}")
