import unreal

TEXTURES = {
    "SandA": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_SandA_col.TEX_MWAM_SandA_col",
    "GrassA": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Grass_col.TEX_MWAM_Grass_col",
    "DirtA": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Dirt_col.TEX_MWAM_Dirt_col",
    "Snow": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Snow_col.TEX_MWAM_Snow_col",
    "RockA": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Rock_col.TEX_MWAM_Rock_col",
    "RockB": "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Stones_col.TEX_MWAM_Stones_col",
}

for label, path in TEXTURES.items():
    tex = unreal.load_asset(path)
    if tex is None:
        unreal.log(f"TEXDIAG: {label} FAILED TO LOAD ({path})")
        continue
    w = tex.blueprint_get_size_x() if hasattr(tex, "blueprint_get_size_x") else tex.get_editor_property("platform_data") if False else None
    try:
        width = tex.get_editor_property("import_width") if False else None
    except Exception:
        width = None
    # Robust generic path: Texture2D has size via GetSizeX/GetSizeY-equivalent editor properties.
    try:
        sx = tex.blueprint_get_size_x()
        sy = tex.blueprint_get_size_y()
    except Exception:
        sx = sy = None
    src_file = None
    try:
        src_data = tex.get_editor_property("asset_import_data")
        if src_data:
            filenames = src_data.extract_filenames()
            src_file = filenames[0] if filenames else None
    except Exception:
        pass
    unreal.log(f"TEXDIAG: {label} loaded OK, size=({sx},{sy}), class={tex.get_class().get_name()}, srcFile={src_file}")

unreal.log("[TEXDIAG] Done.")
