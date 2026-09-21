"""
Builds M_IH_IslandGroundNaturalistic's sibling for BANDS/BIOME DEV View modes: a plain shared
material that reads the per-vertex color IH_WB_IslandActor.cpp bakes in (see
IH_WB_IslandActorPrivate::GetVertexColorValueForRow/AccumulateVertexColorValue) and displays it
directly as BaseColor. No texture layers - BANDS/BIOME are flat elevation-tier/per-biome swatch
colors, not textured ground, so there's nothing to blend between except the color itself.

Vertex color is stored already gamma-DECODED (linear-space) by the C++ side (it reuses
ParseBiomeHexColor's existing FLinearColor::FromSRGBColor decode before averaging/packing), and a
material's VertexColor node reads its raw stored bytes as linear with no further decode - so this
wires straight through with no gamma-correction node needed.

AlbedoScale/Roughness/Specular replace CreateIslandBiomeMaterial's old per-MID GrabContrast
scalars (IHDevViewRuntime::IsGrabContrastEnabled()) - still settable per-MID at runtime, just no
longer tied to a specific classified row (there's no per-row data left on this material at all;
all of that lives in vertex color now).

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/build_island_flat_vertexcolor_material.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MATERIAL_PACKAGE = "/Game/InvisibleHand/Materials/M_IH_IslandFlatVertexColor"

if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(MATERIAL_PACKAGE)

mat_factory = unreal.MaterialFactoryNew()
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_IH_IslandFlatVertexColor", "/Game/InvisibleHand/Materials", unreal.Material, mat_factory)
assert material is not None

MEL = unreal.MaterialEditingLibrary

vertex_color = MEL.create_material_expression(material, unreal.MaterialExpressionVertexColor, -600, 0)

albedo_scale = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 200)
albedo_scale.set_editor_property("parameter_name", "AlbedoScale")
albedo_scale.set_editor_property("default_value", 1.0)

scaled_color = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 0)
MEL.connect_material_expressions(vertex_color, "", scaled_color, "A")
MEL.connect_material_expressions(albedo_scale, "", scaled_color, "B")

roughness_param = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 300)
roughness_param.set_editor_property("parameter_name", "Roughness")
roughness_param.set_editor_property("default_value", 0.92)
specular_param = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 400)
specular_param.set_editor_property("parameter_name", "Specular")
specular_param.set_editor_property("default_value", 0.10)

MEL.connect_material_property(scaled_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(roughness_param, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(specular_param, "", unreal.MaterialProperty.MP_SPECULAR)

MEL.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(MATERIAL_PACKAGE)
unreal.log(f"[FLATVERTEXCOLOR] Built and saved {MATERIAL_PACKAGE}")
unreal.log("[FLATVERTEXCOLOR] Done.")
