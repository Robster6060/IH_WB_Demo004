"""
Removes M_IH_WwfSand's World-Position-Offset wave displacement (MF_OceanWavesWPO, 3 Sine + Time
nodes) - the real cause of Observation 2's "brown hash marks"/phantom wave ebb-flow (a genuine
vertex-displacement wave sim baked into the material, independent of Ocean/Waterline actors, and
a real threat to the IslandMesh/ShelfMesh seam-weld fix if ShelfMesh vertices wobble while
IslandMesh's don't). MF_OceanWavesWPO itself is shared by M_Ocean/M_Sand/other Waterline
materials - NOT touched. Only M_IH_WwfSand's own World Position Offset input is overridden with
a zero constant; the existing WPO call chain is left in place, just disconnected (freeze, not
delete - same pattern as the Panner fix).
"""
import unreal

MATERIAL_PATH = "/Game/InvisibleHand/Materials/M_IH_WwfSand.M_IH_WwfSand"
mat = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)

current_wpo_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
    mat, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
unreal.log(f"Current WPO input node: {current_wpo_node.get_name() if current_wpo_node else 'None'} "
           f"class={current_wpo_node.get_class().get_name() if current_wpo_node else 'None'}")

zero_const = unreal.MaterialEditingLibrary.create_material_expression(
    mat, unreal.MaterialExpressionConstant3Vector, -368, 1440)
zero_const.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))

ok = unreal.MaterialEditingLibrary.connect_material_property(
    zero_const, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
unreal.log(f"connect_material_property(zero_const -> MP_WORLD_POSITION_OFFSET) = {ok}")

unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)

new_wpo_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
    mat, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
unreal.log(f"New WPO input node: {new_wpo_node.get_name() if new_wpo_node else 'None'}")
unreal.log(f"Saved {MATERIAL_PATH} - World Position Offset now a static zero vector.")
