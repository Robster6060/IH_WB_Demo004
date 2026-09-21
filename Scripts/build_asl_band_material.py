"""
Builds T_ASLBandLUT (a 256x1 color-band lookup texture, hard-edged/no interpolation) and
M_TerrainStampASLBand (a small material sampling AbsoluteWorldPosition.z against that LUT), then
assigns the resulting material instance to TableMesa001 - replacing MI_TerrainStamp_TableMesa001's
textured MTL_MWAM_AutoMaterial_MASTER base with something that matches the REAL live island
terrain's own coloring convention (ApplyDtBiomeColorBands in IH_WB_IslandActor.cpp, sourced from
DT_ASLSlopeBiome).

Key simplification, verified directly against the real DT_ASLSlopeBiome data (2026-09-09) rather
than assumed: every row's biomeColor is IDENTICAL across all 7 slope subdivisions within a given
elevation tier - slope does not actually affect color in the current DataTable, only elevation
does. So a pure World-Z lookup (this script) reproduces the exact same visual result as the real
per-triangle classifier (which also checks slope) - not an approximation of it. If a future
DT_ASLSlopeBiome revision ever gives different rows within the same elevation tier different colors
by slope, this LUT would need a second (slope) axis added - flagged here so that's not a surprise.

The LUT is baked directly from DT_ASLSlopeBiome at bake time (not hand-transcribed thresholds), by
replaying the same ASL-range-first-match-by-sortOrder logic ClassifyBiomeRowIndex uses (slope
ignored, per the simplification above) - stays correct if the DataTable is ever edited, just re-run
this script. Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/build_asl_band_material.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import json
import os
import struct
import unreal

ASL_DT_PATH = "/Game/InvisibleHand/Data/DataTables/DT_ASLSlopeBiome"
LUT_TEXTURE_PACKAGE = "/Game/InvisibleHand/World/TerrainStamps/T_ASLBandLUT"
MATERIAL_PACKAGE = "/Game/InvisibleHand/World/TerrainStamps/M_TerrainStampASLBand"
MATERIAL_INSTANCE_PACKAGE = "/Game/InvisibleHand/World/TerrainStamps/TS_Vertical/Domes/TableMesa001/MI_TerrainStamp_TableMesa001_ASLBand"
STAMP_MESH_PATH = "/Game/InvisibleHand/World/TerrainStamps/TS_Vertical/Domes/TableMesa001/TableMesa001"

LUT_WIDTH = 256
Z_MIN_M = -25.0
Z_MAX_M = 2400.0

TEMP_BMP_PATH = "C:/Users/lynnd/AppData/Local/Temp/claude/asl_band_lut.bmp"


def write_bmp_1row(path, width, rgba_rows):
    """Writes a minimal uncompressed 32bpp BMP, width x 1, top row = rgba_rows[0] (B,G,R,A per px)."""
    height = 1
    row_bytes = width * 4
    pixel_data = bytearray()
    # BMP stores bottom-up by default; with only one row this doesn't matter.
    for (r, g, b, a) in rgba_rows:
        pixel_data += struct.pack("<BBBB", b, g, r, a)

    dib_header_size = 40
    file_header_size = 14
    pixel_data_offset = file_header_size + dib_header_size
    file_size = pixel_data_offset + len(pixel_data)

    with open(path, "wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<IHHI", file_size, 0, 0, pixel_data_offset))
        f.write(struct.pack("<IiiHHIIiiII",
            dib_header_size, width, height, 1, 32, 0, len(pixel_data), 2835, 2835, 0, 0))
        f.write(pixel_data)


def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip("#")
    return (int(hex_str[0:2], 16), int(hex_str[2:4], 16), int(hex_str[4:6], 16))


# --- Load DT_ASLSlopeBiome and replay the real classifier (ASL-range only - see docstring) ---
dt = unreal.load_asset(ASL_DT_PATH)
assert dt is not None, f"Could not load {ASL_DT_PATH}"
json_str = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt)
rows = json.loads(json_str)
rows.sort(key=lambda r: r["sortOrder"])
unreal.log(f"[ASLBAND] Loaded {len(rows)} DT_ASLSlopeBiome rows.")


def classify_z(z_m):
    for r in rows:
        if r["aslLowerM"] <= z_m <= r["aslUpperM"]:
            return hex_to_rgb(r["biomeColor"])
    return (128, 128, 128)


rgba_rows = []
for i in range(LUT_WIDTH):
    u = i / float(LUT_WIDTH - 1)
    z_m = Z_MIN_M + u * (Z_MAX_M - Z_MIN_M)
    r, g, b = classify_z(z_m)
    rgba_rows.append((r, g, b, 255))

os.makedirs(os.path.dirname(TEMP_BMP_PATH), exist_ok=True)
write_bmp_1row(TEMP_BMP_PATH, LUT_WIDTH, rgba_rows)
unreal.log(f"[ASLBAND] Wrote {TEMP_BMP_PATH} ({LUT_WIDTH}x1)")

# --- Import as a Texture2D asset ---
if unreal.EditorAssetLibrary.does_asset_exist(LUT_TEXTURE_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(LUT_TEXTURE_PACKAGE)

import_task = unreal.AssetImportTask()
import_task.filename = TEMP_BMP_PATH
import_task.destination_path = "/Game/InvisibleHand/World/TerrainStamps"
import_task.destination_name = "T_ASLBandLUT"
import_task.automated = True
import_task.save = True
import_task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([import_task])

lut_texture = unreal.load_asset(LUT_TEXTURE_PACKAGE)
assert lut_texture is not None, f"[ASLBAND] FAILED: {LUT_TEXTURE_PACKAGE} did not import"

# Hard edges, no mip blur, no compression artifacts on exact colors, clamp at both ends.
lut_texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
lut_texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
lut_texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
lut_texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
lut_texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
lut_texture.set_editor_property("srgb", True)
unreal.EditorAssetLibrary.save_asset(LUT_TEXTURE_PACKAGE)
unreal.log(f"[ASLBAND] Configured and saved {LUT_TEXTURE_PACKAGE}")

# --- Build the material ---
if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(MATERIAL_PACKAGE)

mat_factory = unreal.MaterialFactoryNew()
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_TerrainStampASLBand", "/Game/InvisibleHand/World/TerrainStamps", unreal.Material, mat_factory)
assert material is not None

MEL = unreal.MaterialEditingLibrary

world_pos = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -800, 0)
mask_z = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -600, 0)
mask_z.set_editor_property("r", False)
mask_z.set_editor_property("g", False)
mask_z.set_editor_property("b", True)
mask_z.set_editor_property("a", False)

to_meters = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, 0)
to_meters.set_editor_property("const_b", 0.01)

offset = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, -300, 0)
offset.set_editor_property("const_b", -Z_MIN_M)

normalize_u = MEL.create_material_expression(material, unreal.MaterialExpressionDivide, -150, 0)
normalize_u.set_editor_property("const_b", (Z_MAX_M - Z_MIN_M))

const_v = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -150, 150)
const_v.set_editor_property("r", 0.5)

append_uv = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, 0, 0)

tex_sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, 200, 0)
tex_sample.set_editor_property("texture", lut_texture)

roughness_const = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, 200, 250)
roughness_const.set_editor_property("r", 0.85)

MEL.connect_material_expressions(world_pos, "", mask_z, "")
MEL.connect_material_expressions(mask_z, "", to_meters, "")
MEL.connect_material_expressions(to_meters, "", offset, "")
MEL.connect_material_expressions(offset, "", normalize_u, "")
MEL.connect_material_expressions(normalize_u, "", append_uv, "A")
MEL.connect_material_expressions(const_v, "", append_uv, "B")
MEL.connect_material_expressions(append_uv, "", tex_sample, "UVs")

MEL.connect_material_property(tex_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(roughness_const, "", unreal.MaterialProperty.MP_ROUGHNESS)

MEL.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(MATERIAL_PACKAGE)
unreal.log(f"[ASLBAND] Built and saved {MATERIAL_PACKAGE}")

# --- Create a material instance in the stamp's own folder and assign it ---
if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_INSTANCE_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(MATERIAL_INSTANCE_PACKAGE)

mi_factory = unreal.MaterialInstanceConstantFactoryNew()
mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "MI_TerrainStamp_TableMesa001_ASLBand",
    "/Game/InvisibleHand/World/TerrainStamps/TS_Vertical/Domes/TableMesa001",
    unreal.MaterialInstanceConstant, mi_factory)
assert mi is not None
MEL.set_material_instance_parent(mi, material)
unreal.EditorAssetLibrary.save_asset(MATERIAL_INSTANCE_PACKAGE)
unreal.log(f"[ASLBAND] Created {MATERIAL_INSTANCE_PACKAGE}")

mesh = unreal.load_asset(STAMP_MESH_PATH)
assert mesh is not None
mesh.set_material(0, mi)
unreal.EditorAssetLibrary.save_asset(STAMP_MESH_PATH)
unreal.log(f"[ASLBAND] Assigned {MATERIAL_INSTANCE_PACKAGE} to {STAMP_MESH_PATH} slot 0")

unreal.log("[ASLBAND] Done.")
