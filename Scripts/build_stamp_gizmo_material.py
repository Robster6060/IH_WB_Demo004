"""
Builds M_TerrainStampGizmoTranslucent - a small unlit, translucent material (single TintColor
vector param) used for Stage 12b's whole-stamp bounding-box gizmo volume (AIH_TerrainStampActor's
new BoundingBoxMesh, an /Engine/BasicShapes/Cube). Not Nanite-related (plain engine Cube, not
Nanite-enabled), unlike the M_TerrainStampASLBand fix earlier this session.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/build_stamp_gizmo_material.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MATERIAL_PACKAGE = "/Game/InvisibleHand/World/TerrainStamps/M_TerrainStampGizmoTranslucent"

MEL = unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(MATERIAL_PACKAGE)

mat_factory = unreal.MaterialFactoryNew()
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_TerrainStampGizmoTranslucent", "/Game/InvisibleHand/World/TerrainStamps", unreal.Material, mat_factory)
assert material is not None

material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

tint_param = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 0)
tint_param.set_editor_property("parameter_name", "TintColor")
tint_param.set_editor_property("default_value", unreal.LinearColor(0.19, 0.83, 0.57, 1.0))  # #31D492

opacity_const = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 150)
opacity_const.set_editor_property("r", 0.25)

MEL.connect_material_property(tint_param, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(opacity_const, "", unreal.MaterialProperty.MP_OPACITY)

MEL.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(MATERIAL_PACKAGE)
unreal.log(f"[GIZMOMAT] Built and saved {MATERIAL_PACKAGE}")
