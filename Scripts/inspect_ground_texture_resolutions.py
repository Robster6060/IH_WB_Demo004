import unreal

TEXTURES = {
    "SandA (MWAM SandA)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_SandA_col",
    "SandB (MWAM SandC)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_SandC_col",
    "GrassA (MWAM Grass)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Grass_col",
    "GrassB (OWD Ground_Grass)": "/Game/OWD_Plants_Pack/Add_Props/Landscape/Textures/T_Ground_Grass_Diffuse",
    "DirtA (MWAM Dirt)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Dirt_col",
    "DirtB (MS_ForestFloorV1 BarkSoil 8K)": "/Game/MS_ForestFloorV1/Surfaces/Bark_Soil_Mix_2x2_M_sesjcefb_surface/Albedo_8K_sesjcefb",
    "Snow (MWAM Snow)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Snow_col",
    "RockA (MWAM Rock)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Rock_col",
    "RockB (MWAM Stones)": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Stones_col",
}

registry = unreal.AssetRegistryHelpers.get_asset_registry()

for label, package_path in TEXTURES.items():
    asset_data = registry.get_asset_by_object_path(package_path + "." + package_path.rsplit("/", 1)[-1])
    if not asset_data or not asset_data.is_valid():
        # Fallback path form some registries expect.
        asset_data = unreal.EditorAssetLibrary.find_asset_data(package_path)
    dims = None
    fmt = None
    if asset_data and asset_data.is_valid():
        try:
            dims = asset_data.get_tag_value("Dimensions")
        except Exception as ex:
            dims = f"ERR:{ex}"
        try:
            fmt = asset_data.get_tag_value("Format")
        except Exception:
            fmt = None
    unreal.log(f"[TEXCHECK] {label}: Dimensions={dims} Format={fmt}")

unreal.log("[TEXCHECK] Done.")
