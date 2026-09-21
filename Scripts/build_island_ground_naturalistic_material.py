"""
Builds M_IH_IslandGroundNaturalistic - PGC DEV View mode's real ground material. ONE shared
Material Instance now covers the WHOLE island (see IH_WB_IslandActor.cpp's
CreateSharedIslandModeMaterial) - the per-tier/per-rock weights below are read from per-VERTEX
color data (baked once at terrain-generation time by
IH_WB_IslandActorPrivate::GetVertexColorValueForRow/AccumulateVertexColorValue), not set as
per-MID scalar params anymore. This is the actual fix for the "harsh triangular rock/snow cutoff"
chased across several earlier rounds: the OLD design gave every classified DT_ASLSlopeBiome row its
own separate PMC section AND its own separate MID with its own scalar params - two adjacent
sections rendered through two totally separate Material Instances with no shared state, so nothing
that varied a scalar parameter could ever blend across that boundary. Reading smoothly-varying
per-vertex color instead lets the GPU's own linear interpolation blend every value continuously
across every triangle, matching the standard "vertex-weight texture splatting" pattern real terrain
shaders use (see Epic's own "Setting Up a Texture Blended Material for Vertex Weights Painting"
docs). Blends real ground textures already sitting unused in Content (MWLandscapeAutoMaterial's
matched Ground texture set, plus one extra variant per category from other already-owned packs -
see below) two ways:
  1. Per-biome-tier weighted blend of Sand/Grass/Dirt/Snow - R/G/B read directly from vertex color,
     Snow derived as saturate(1-R-G-B) (see the vertex_color/make_channel_mask block below for why
     VertexColor's default output needs ComponentMask, not a named "RGB" sub-output).
  2. A Rock blend: RockBlendAlpha, now vertex color's Alpha channel (was a scalar param set per-MID
     from C++ from that row's own real minSlopeDeg/maxSlopeDeg), acts as a FLOOR, guaranteeing a
     genuinely steep-classified area always shows at least that much rock.
     2026-09-19 round 2: a hard per-row-classified-SECTION constant alone produced a visible
     hard-edged patchwork (jumping abruptly between adjacent sections, most visible on the snow
     cap) - added a smoothed PER-PIXEL modulation on top, from VertexNormalWS.Z (the same
     smoothed-normal signal already used for lighting, so it is guaranteed continuous across
     triangle boundaries and cannot itself introduce a hard edge), scaled by a small
     PerPixelRockInfluence weight and added to the floor before a final Clamp. 2026-09-20: the
     floor itself is now also continuous (per-vertex, not per-section), so this per-pixel term is a
     complementary finer-grained modulation, not a fix for a discontinuity that no longer exists at
     the section level. This is NOT the sole slope signal (round 1 tried that and it barely
     triggered - see the RockBlendAlpha comment below) - it only smooths the transition around
     whatever floor the real
     classification already sets.

Texture variety (2026-09-19 round 2): Grass/Dirt/Rock/Sand each blend between TWO real, already-
owned texture variants via a shared world-space Noise value (NOT per-pixel dither - a large Scale
so it reads as a few different-looking patches across a hillside), so adjacent same-tier terrain
doesn't look copy-pasted. Snow has only one tileable ground texture anywhere in this project's
Content, so it stays single-texture. Variant B sources, confirmed unused elsewhere and requiring no
new asset purchase:
  - Grass B: OWD_Plants_Pack's own ground grass texture (different pack/style than MWAM's).
  - Dirt B: MS_ForestFloorV1's Bark_Soil_Mix surface.
  - Rock B: MWAM's own "Stones" texture - same pack as Rock A, already owned, previously unused.
  - Sand B: MWAM's own "SandC" texture - same pack as Sand A (SandA), already owned, previously unused.

UVs are a simple WorldPosition.XY / TileSizeCm planar projection (not full triplanar) - a known
first-pass simplification; steep rock faces may show some stretching until a triplanar upgrade.
No normal-map blending yet either - base color + slope-driven rock blend + variant diversity is the
deliverable for this round.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/build_island_ground_naturalistic_material.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

MATERIAL_PACKAGE = "/Game/InvisibleHand/Materials/M_IH_IslandGroundNaturalistic"

TEX_SAND_A = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_SandA_col.TEX_MWAM_SandA_col"
TEX_SAND_B = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_SandC_col.TEX_MWAM_SandC_col"
TEX_GRASS_A = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Grass_col.TEX_MWAM_Grass_col"
TEX_GRASS_B = "/Game/OWD_Plants_Pack/Add_Props/Landscape/Textures/T_Ground_Grass_Diffuse.T_Ground_Grass_Diffuse"
TEX_DIRT_A = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Dirt_col.TEX_MWAM_Dirt_col"
TEX_DIRT_B = "/Game/MS_ForestFloorV1/Surfaces/Bark_Soil_Mix_2x2_M_sesjcefb_surface/Albedo_8K_sesjcefb.Albedo_8K_sesjcefb"
TEX_SNOW = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Snow_col.TEX_MWAM_Snow_col"
TEX_ROCK_A = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Rock_col.TEX_MWAM_Rock_col"
TEX_ROCK_B = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Stones_col.TEX_MWAM_Stones_col"

TEXTURES = {
    "SandA": TEX_SAND_A, "SandB": TEX_SAND_B,
    "GrassA": TEX_GRASS_A, "GrassB": TEX_GRASS_B,
    "DirtA": TEX_DIRT_A, "DirtB": TEX_DIRT_B,
    "Snow": TEX_SNOW,
    "RockA": TEX_ROCK_A, "RockB": TEX_ROCK_B,
}
tex_assets = {}
for label, path in TEXTURES.items():
    asset = unreal.load_asset(path)
    assert asset is not None, f"[GROUNDMAT] Could not load {label} texture ({path})"
    tex_assets[label] = asset

if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(MATERIAL_PACKAGE)

mat_factory = unreal.MaterialFactoryNew()
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_IH_IslandGroundNaturalistic", "/Game/InvisibleHand/Materials", unreal.Material, mat_factory)
assert material is not None

MEL = unreal.MaterialEditingLibrary

# --- UVs: WorldPosition.XY / TileSizeCm ---
world_pos = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1200, -400)
uv_mask = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1000, -400)
uv_mask.set_editor_property("r", True)
uv_mask.set_editor_property("g", True)
uv_mask.set_editor_property("b", False)
uv_mask.set_editor_property("a", False)

tile_size = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1000, -250)
tile_size.set_editor_property("parameter_name", "TileSizeCm")
tile_size.set_editor_property("default_value", 800.0)

uv_scaled = MEL.create_material_expression(material, unreal.MaterialExpressionDivide, -800, -400)

MEL.connect_material_expressions(world_pos, "", uv_mask, "")
MEL.connect_material_expressions(uv_mask, "", uv_scaled, "A")
MEL.connect_material_expressions(tile_size, "", uv_scaled, "B")

# --- Shared world-space patch noise (0-1), reused to blend variant A/B for Sand/Grass/Dirt/Rock.
# Large Scale so this reads as a few differently-looking patches across a hillside, not per-pixel
# dither. Left at engine defaults for Quality/Levels/NoiseFunction (only Scale/OutputMin/OutputMax
# set explicitly) - minimizes the chance of a subtle property-name mismatch silently mis-wiring it.
patch_noise = MEL.create_material_expression(material, unreal.MaterialExpressionNoise, -1200, 900)
patch_noise.set_editor_property("scale", 3000.0)
patch_noise.set_editor_property("output_min", 0.0)
patch_noise.set_editor_property("output_max", 1.0)
MEL.connect_material_expressions(world_pos, "", patch_noise, "")

# --- Texture samples, all sharing uv_scaled: 2 variants each for Sand/Grass/Dirt/Rock, 1 for Snow ---
tex_nodes = {}
y = -700
for label in ("SandA", "SandB", "GrassA", "GrassB", "DirtA", "DirtB", "Snow", "RockA", "RockB"):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, y)
    node.set_editor_property("texture", tex_assets[label])
    MEL.connect_material_expressions(uv_scaled, "", node, "UVs")
    tex_nodes[label] = node
    y += 150

# --- Variant A/B Lerp per diversified category, blended by the shared patch noise ---
def make_variant_lerp(y_pos, label):
    lerp_node = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -200, y_pos)
    MEL.connect_material_expressions(tex_nodes[label + "A"], "RGB", lerp_node, "A")
    MEL.connect_material_expressions(tex_nodes[label + "B"], "RGB", lerp_node, "B")
    MEL.connect_material_expressions(patch_noise, "", lerp_node, "Alpha")
    return lerp_node

sand_blend = make_variant_lerp(-700, "Sand")
grass_blend = make_variant_lerp(-550, "Grass")
dirt_blend = make_variant_lerp(-400, "Dirt")
rock_blend = make_variant_lerp(700, "Rock")

# --- Per-tier weighted blend of Sand/Grass/Dirt/Snow ---
# 2026-09-20 (shared per-vertex-blended terrain material): weights promoted from per-MID scalar
# params (SandWeight/GrassWeight/DirtWeight/SnowWeight, one MID per classified PMC section - the
# actual root cause of every hard biome-color edge chased this session, since two adjacent
# sections rendered through two totally separate Material Instances with no shared state) to
# per-vertex color, read here via VertexColor + ComponentMask (same technique already used below
# for VertexNormalWS.Z - VertexColor has no named "R"/"G"/"B"/"A" sub-output in this UE version's
# Python API, confirmed via the Phase 0 proof-of-concept). Packing (see
# IH_WB_IslandActorPrivate::GetVertexColorValueForRow/PackVertexColorValue): R=Sand, G=Grass,
# B=Dirt, A=Rock (see rock_alpha_floor below) - Snow is DERIVED here as saturate(1-R-G-B) rather
# than stored directly, since it's always exactly what's left over once the other 3 tier weights
# are accounted for, and this frees the 4th (Alpha) channel for Rock instead.
vertex_color = MEL.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, -700)

def make_channel_mask(y_pos, r, g, b):
    # VertexColor's DEFAULT output ("") is RGB only (float3) - confirmed via a direct compile
    # error ("Not enough components ... for component mask 0001") when this was tried for Alpha
    # too. Alpha is a separate, already-scalar named "A" output (see vertex_color_alpha below,
    # which reads it directly with no ComponentMask needed at all).
    mask = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -200, y_pos)
    mask.set_editor_property("r", r)
    mask.set_editor_property("g", g)
    mask.set_editor_property("b", b)
    mask.set_editor_property("a", False)
    MEL.connect_material_expressions(vertex_color, "", mask, "")
    return mask

sand_w = make_channel_mask(-700, True, False, False)
grass_w = make_channel_mask(-550, False, True, False)
dirt_w = make_channel_mask(-400, False, False, True)
# Alpha is already a plain scalar output on VertexColor - no ComponentMask needed, just reference
# vertex_color's "A" output directly wherever this is used below.
vertex_color_alpha = vertex_color
VERTEX_COLOR_ALPHA_OUTPUT = "A"

snow_sum_rg = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 0, -650)
MEL.connect_material_expressions(sand_w, "", snow_sum_rg, "A")
MEL.connect_material_expressions(grass_w, "", snow_sum_rg, "B")
snow_sum_rgb = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 0, -550)
MEL.connect_material_expressions(snow_sum_rg, "", snow_sum_rgb, "A")
MEL.connect_material_expressions(dirt_w, "", snow_sum_rgb, "B")
snow_remainder = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, 0, -450)
MEL.connect_material_expressions(snow_sum_rgb, "", snow_remainder, "")
snow_w = MEL.create_material_expression(material, unreal.MaterialExpressionClamp, 0, -350)
snow_w.set_editor_property("min_default", 0.0)
snow_w.set_editor_property("max_default", 1.0)
MEL.connect_material_expressions(snow_remainder, "", snow_w, "")

sand_mul = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 200, -700)
grass_mul = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 200, -550)
dirt_mul = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 200, -400)
snow_mul = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 200, -250)

MEL.connect_material_expressions(sand_blend, "", sand_mul, "A")
MEL.connect_material_expressions(sand_w, "", sand_mul, "B")
MEL.connect_material_expressions(grass_blend, "", grass_mul, "A")
MEL.connect_material_expressions(grass_w, "", grass_mul, "B")
MEL.connect_material_expressions(dirt_blend, "", dirt_mul, "A")
MEL.connect_material_expressions(dirt_w, "", dirt_mul, "B")
MEL.connect_material_expressions(tex_nodes["Snow"], "RGB", snow_mul, "A")
MEL.connect_material_expressions(snow_w, "", snow_mul, "B")

add1 = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 400, -650)
add2 = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 600, -550)
add3 = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 800, -450)

MEL.connect_material_expressions(sand_mul, "", add1, "A")
MEL.connect_material_expressions(grass_mul, "", add1, "B")
MEL.connect_material_expressions(add1, "", add2, "A")
MEL.connect_material_expressions(dirt_mul, "", add2, "B")
MEL.connect_material_expressions(add2, "", add3, "A")
MEL.connect_material_expressions(snow_mul, "", add3, "B")

# --- Rock blend alpha: per-vertex floor (was a per-MID/per-SECTION scalar - see the comment above
# vertex_color) + smoothed per-pixel modulation on top ---
# 2026-09-19 round 1: first version computed slope ENTIRELY in-shader from VertexNormalWS.Z, the
# "real per-pixel slope" a landscape auto-material normally uses. PIE testing showed it almost
# never triggered even on visibly steep, classifier-confirmed-Steep/Craggy/Sheer terrain - root-
# caused to IslandMesh's own normals being deliberately SMOOTHED across adjacent triangles for
# clean lighting, which washes out true local slope at this terrain's coarse (~1,600 sq m/triangle)
# resolution. Round 2 replaced it with RockBlendAlpha, a hard per-row-classified-SECTION constant -
# correct minimum coverage, but produced a visible hard-edged patchwork wherever two differently-
# classified sections met, because each section was a separate Material Instance with no shared
# state (round 2 report). 2026-09-20: the floor itself is now the per-VERTEX-averaged value baked
# into vertex color's Alpha channel (continuous by construction, like every other channel here) -
# the smoothed per-pixel VertexNormalWS term below is kept as a complementary, EVEN finer-grained
# modulation on top of that already-continuous floor, not a fix for a discontinuity that no longer
# exists at the section level.
rock_alpha_floor = vertex_color_alpha

vtx_normal = MEL.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, 400, 400)
normal_z = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, 600, 400)
normal_z.set_editor_property("r", False)
normal_z.set_editor_property("g", False)
normal_z.set_editor_property("b", True)
normal_z.set_editor_property("a", False)
MEL.connect_material_expressions(vtx_normal, "", normal_z, "")

per_pixel_raw = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, 800, 400)
MEL.connect_material_expressions(normal_z, "", per_pixel_raw, "")

per_pixel_influence = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, 600, 550)
per_pixel_influence.set_editor_property("parameter_name", "PerPixelRockInfluence")
per_pixel_influence.set_editor_property("default_value", 0.35)

per_pixel_term = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 1000, 450)
MEL.connect_material_expressions(per_pixel_raw, "", per_pixel_term, "A")
MEL.connect_material_expressions(per_pixel_influence, "", per_pixel_term, "B")

rock_alpha_sum = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 1200, 300)
MEL.connect_material_expressions(rock_alpha_floor, VERTEX_COLOR_ALPHA_OUTPUT, rock_alpha_sum, "A")
MEL.connect_material_expressions(per_pixel_term, "", rock_alpha_sum, "B")

rock_alpha_final = MEL.create_material_expression(material, unreal.MaterialExpressionClamp, 1400, 300)
rock_alpha_final.set_editor_property("min_default", 0.0)
rock_alpha_final.set_editor_property("max_default", 1.0)
MEL.connect_material_expressions(rock_alpha_sum, "", rock_alpha_final, "")

# --- Final: Lerp(tier-blended base, Rock A/B blend, final rock alpha) ---
final_lerp = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 1600, 0)
MEL.connect_material_expressions(add3, "", final_lerp, "A")
MEL.connect_material_expressions(rock_blend, "", final_lerp, "B")
MEL.connect_material_expressions(rock_alpha_final, "", final_lerp, "Alpha")

roughness_param = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, 1600, 200)
roughness_param.set_editor_property("parameter_name", "Roughness")
roughness_param.set_editor_property("default_value", 0.92)
specular_param = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, 1600, 300)
specular_param.set_editor_property("parameter_name", "Specular")
specular_param.set_editor_property("default_value", 0.10)

MEL.connect_material_property(final_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(roughness_param, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(specular_param, "", unreal.MaterialProperty.MP_SPECULAR)

MEL.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(MATERIAL_PACKAGE)
unreal.log(f"[GROUNDMAT] Built and saved {MATERIAL_PACKAGE}")
unreal.log("[GROUNDMAT] Done.")
