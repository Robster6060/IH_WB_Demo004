"""
One-off verification (2026-09-09) that TableMesa001's Nanite and collision settings actually match
what the pipeline doc (_Pipeline/TerrainStamp_BlenderToUE5_Pipeline.md, Stage 6) claims. Also
completes Stage 6 step 4 (assign a physical material) - confirmed via this same script that it had
NOT actually been done despite the doc's own checklist implying it was (phys_material was None).
Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/verify_tablemesa001_nanite_and_collision.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MESH_PATH = "/Game/InvisibleHand/World/TerrainStamps/TS_Vertical/Domes/TableMesa001/TableMesa001"
PHYS_MATERIAL_PATH = "/Game/MWLandscapeAutoMaterial/Materials/MASTER/PhyMat/PM_MW_Rocks"

mesh = unreal.load_asset(MESH_PATH)
assert mesh is not None, f"[VERIFY] FAILED: could not load {MESH_PATH}"
unreal.log(f"[VERIFY] Loaded {MESH_PATH}")

# --- Nanite ---
nanite = mesh.get_editor_property("nanite_settings")
enabled = nanite.get_editor_property("enabled")
unreal.log(f"[VERIFY] nanite_settings.enabled = {enabled}")
assert enabled, "[VERIFY] FAILED: Nanite is not enabled on TableMesa001"

num_nanite_tris = mesh.get_num_nanite_triangles()
num_nanite_verts = mesh.get_num_nanite_vertices()
unreal.log(f"[VERIFY] Nanite triangle/vertex counts: {num_nanite_tris} tris, {num_nanite_verts} verts")
assert num_nanite_tris > 0 and num_nanite_verts > 0, "[VERIFY] FAILED: Nanite enabled but no Nanite data baked (0 tris/verts)"

fallback_target = nanite.get_editor_property("fallback_target")
keep_percent = nanite.get_editor_property("keep_percent_triangles")
fallback_error = nanite.get_editor_property("fallback_relative_error")
unreal.log(f"[VERIFY] Nanite fallback: target={fallback_target}, keep_percent_triangles={keep_percent}, fallback_relative_error={fallback_error}")

# --- Collision ---
body_setup = mesh.get_editor_property("body_setup")
assert body_setup is not None, "[VERIFY] FAILED: no BodySetup on TableMesa001"
trace_flag = body_setup.get_editor_property("collision_trace_flag")
unreal.log(f"[VERIFY] collision_trace_flag = {trace_flag}")
assert trace_flag == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE, \
    f"[VERIFY] FAILED: expected CTF_USE_COMPLEX_AS_SIMPLE, got {trace_flag}"

phys_material = body_setup.get_editor_property("phys_material")
if phys_material is None:
    unreal.log("[VERIFY] phys_material was None (Stage 6 step 4 was not actually completed despite the pipeline doc's checklist) - assigning PM_MW_Rocks now.")
    rock_phys_material = unreal.load_asset(PHYS_MATERIAL_PATH)
    assert rock_phys_material is not None, f"[VERIFY] FAILED: could not load {PHYS_MATERIAL_PATH}"
    body_setup.set_editor_property("phys_material", rock_phys_material)
    unreal.EditorAssetLibrary.save_asset(MESH_PATH)
    phys_material = body_setup.get_editor_property("phys_material")
unreal.log(f"[VERIFY] phys_material = {phys_material.get_name() if phys_material else 'None'}")
assert phys_material is not None, "[VERIFY] FAILED: phys_material still None after assignment attempt"

# --- Material slot ---
static_materials = mesh.get_editor_property("static_materials")
assert len(static_materials) > 0, "[VERIFY] FAILED: no material slots"
slot0_material = static_materials[0].get_editor_property("material_interface")
unreal.log(f"[VERIFY] Material slot 0 = {slot0_material.get_name() if slot0_material else 'None'}")

unreal.log("[VERIFY] All Nanite/collision assertions passed. Done.")
