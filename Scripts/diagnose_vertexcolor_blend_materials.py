"""
Read-only diagnostic for the 2026-09-20 all-gray DEV View regression. Dumps whether BaseColor (and
key intermediate nodes) are actually connected in M_IH_IslandFlatVertexColor and
M_IH_IslandGroundNaturalistic, plus a live re-save+recompile pass to catch any latent compile error
these two never surfaced before (per this session's history of Python MaterialEditingLibrary calls
failing silently on bad output names).

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/diagnose_vertexcolor_blend_materials.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MEL = unreal.MaterialEditingLibrary

MATERIALS = [
    "/Game/InvisibleHand/Materials/M_IH_IslandFlatVertexColor.M_IH_IslandFlatVertexColor",
    "/Game/InvisibleHand/Materials/M_IH_IslandGroundNaturalistic.M_IH_IslandGroundNaturalistic",
]

PROPS = ["MP_BASE_COLOR", "MP_ROUGHNESS", "MP_SPECULAR"]


def describe(node, depth=0, seen=None):
    if seen is None:
        seen = set()
    indent = "  " * depth
    if node is None:
        unreal.log(f"{indent}(none)")
        return
    key = node.get_name()
    cls = node.get_class().get_name()
    unreal.log(f"{indent}[{key}] class={cls}")
    if key in seen or depth >= 4:
        return
    seen.add(key)
    try:
        inputs = MEL.get_input_expressions(node)
    except Exception:
        inputs = None
    if inputs:
        for inp in inputs:
            describe(inp, depth + 1, seen)


for mat_path in MATERIALS:
    unreal.log("=" * 80)
    unreal.log(f"MATERIAL: {mat_path}")
    mat = unreal.EditorAssetLibrary.load_asset(mat_path)
    if mat is None:
        unreal.log("  FAILED TO LOAD ASSET")
        continue

    for prop_name in PROPS:
        prop_enum = getattr(unreal.MaterialProperty, prop_name)
        node = MEL.get_material_property_input_node(mat, prop_enum)
        if node:
            unreal.log(f"{prop_name} <- [{node.get_name()}] class={node.get_class().get_name()}")
        else:
            unreal.log(f"{prop_name} <- (UNCONNECTED)")

    unreal.log("--- BaseColor input chain ---")
    base_color_node = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    describe(base_color_node)

    expressions = MEL.get_material_expressions(mat)
    unreal.log(f"Total expression count: {len(expressions)}")
    vcolor_nodes = [e for e in expressions if e.get_class().get_name() == "MaterialExpressionVertexColor"]
    unreal.log(f"MaterialExpressionVertexColor node count: {len(vcolor_nodes)}")

    # Force a recompile + report any compile errors that weren't previously surfaced.
    try:
        MEL.recompile_material(mat)
        unreal.log("Recompile: OK (no exception)")
    except Exception as ex:
        unreal.log(f"Recompile FAILED: {ex}")

unreal.log("=" * 80)
unreal.log("[DIAGNOSE] Done.")
