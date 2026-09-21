import unreal

PATH = "/Game/MS_ForestFloorV1/Surfaces/Bark_Soil_Mix_2x2_M_sesjcefb_surface/Albedo_8K_sesjcefb"
tex = unreal.load_asset(PATH)
assert tex is not None, f"[TEXCAP] Could not load {PATH}"

before = tex.get_editor_property("max_texture_size")
tex.set_editor_property("max_texture_size", 2048)
after = tex.get_editor_property("max_texture_size")
unreal.EditorAssetLibrary.save_asset(PATH)
unreal.log(f"[TEXCAP] {PATH}: MaxTextureSize {before} -> {after}, saved.")
unreal.log("[TEXCAP] Done.")
