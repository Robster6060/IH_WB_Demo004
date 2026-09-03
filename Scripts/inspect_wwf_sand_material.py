"""
Headless inspection of M_IH_WwfSand for time-based/animated nodes (Observation 2,
2026-09-02: "brown hash marks" moving along the WWF shoreline independent of
Waterline Pro/Ocean state). Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/inspect_wwf_sand_material.py"
"""
import unreal

MATERIAL_PATH = "/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand"

mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
if not mat:
    raise RuntimeError(f"Could not load {MATERIAL_PATH}")

unreal.log(f"Loaded {MATERIAL_PATH}, class={mat.get_class().get_name()}")

expressions = unreal.MaterialEditingLibrary.get_material_expressions(mat)
unreal.log(f"Total expressions: {len(expressions)}")

TIME_RELATED_CLASSES = (
    "MaterialExpressionTime",
    "MaterialExpressionPanner",
    "MaterialExpressionSine",
    "MaterialExpressionRotator",
    "MaterialExpressionCurveAtlasRowParameter",
)

hits = []
for expr in expressions:
    cls_name = expr.get_class().get_name()
    desc = ""
    try:
        desc = expr.desc
    except Exception:
        pass
    param_name = ""
    for attr in ("parameter_name",):
        if hasattr(expr, attr):
            try:
                param_name = str(getattr(expr, attr))
            except Exception:
                pass
    is_time_related = any(t in cls_name for t in TIME_RELATED_CLASSES)
    keyword_hit = any(
        kw in (desc + param_name).lower()
        for kw in ("caustic", "wet", "dry", "tide", "ebb", "wave", "hash", "ripple")
    )
    if is_time_related or keyword_hit:
        hits.append((cls_name, param_name, desc))
        unreal.log(f"HIT: class={cls_name} param_name='{param_name}' desc='{desc}'")

if not hits:
    unreal.log("No time-based nodes or wet/dry/caustic-keyword nodes found by class name or "
               "description/parameter-name heuristic - dumping full expression class list instead.")
    for expr in expressions:
        unreal.log(f"  expr class={expr.get_class().get_name()}")

# Also list all scalar/vector parameter names+defaults, regardless of the above heuristic.
unreal.log("--- All scalar parameters ---")
for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionScalarParameter):
        unreal.log(f"  Scalar '{expr.parameter_name}' default={expr.default_value}")
unreal.log("--- All vector parameters ---")
for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionVectorParameter):
        unreal.log(f"  Vector '{expr.parameter_name}' default={expr.default_value}")
