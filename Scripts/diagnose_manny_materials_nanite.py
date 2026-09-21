"""
2026-09-12: Fresh fundamentals check on SKM_Manny_Simple, prompted by the user's continued
"pink stick" report even after: collision-channel fix, AnimBP disable, legacy->Manny asset swap,
AnimationMode=AnimationSingleNode. Two concrete, previously-unchecked hypotheses:

1. Material-slot binding: SK_Mannequin (skeleton) and PA_Mannequin (physics asset) were BOTH
   missing-dependency errors the first time Manny content was copied in, and had to be copied in
   separately. Did the SAME kind of silent-null failure happen to SKM_Manny_Simple's own MATERIAL
   SLOTS (pointing at MI_Manny_01_New/MI_Manny_02_New)? A skeletal mesh with a valid Skeleton/PhysicsAsset
   but a None material in every slot would still normally render (checkerboard fallback), so this is a
   secondary lead - but worth ruling in/out explicitly rather than assumed fine.

2. Nanite: this project is Nanite-heavy elsewhere (a real Nanite-usage-flag bug was already found and
   fixed this session for M_TerrainStampASLBand). UE5.6+ ships experimental Nanite support for
   SKELETAL meshes specifically. If SKM_Manny_Simple was authored/imported with Nanite enabled, and
   this project's runtime does not have skeletal-mesh Nanite fully supported/enabled, the component
   could fail to render via the normal path with no obvious error - never yet checked.
"""
import unreal

MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"

mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
if not mesh:
    print(f"[DIAG] FAILED to load {MESH_PATH}")
else:
    print(f"[DIAG] Loaded: {mesh.get_name()} ({mesh.get_class().get_name()})")

    # --- Material slots ---
    try:
        materials = mesh.get_editor_property("materials")
        print(f"[DIAG] Material slot count: {len(materials)}")
        for i, slot in enumerate(materials):
            mat_iface = slot.get_editor_property("material_interface")
            slot_name = slot.get_editor_property("material_slot_name")
            print(f"[DIAG]   Slot {i} ('{slot_name}'): material_interface = {mat_iface}")
    except Exception as e:
        print(f"[DIAG] materials property read failed: {e}")

    # --- Skeleton / PhysicsAsset sanity (re-confirm, cheap) ---
    try:
        skel = mesh.get_editor_property("skeleton")
        print(f"[DIAG] Skeleton: {skel}")
    except Exception as e:
        print(f"[DIAG] skeleton read failed: {e}")
    try:
        phys = mesh.get_editor_property("physics_asset")
        print(f"[DIAG] PhysicsAsset: {phys}")
    except Exception as e:
        print(f"[DIAG] physics_asset read failed: {e}")

    # --- Nanite settings ---
    try:
        nanite_settings = mesh.get_editor_property("nanite_settings")
        enabled = nanite_settings.get_editor_property("enabled")
        print(f"[DIAG] NaniteSettings.bEnabled: {enabled}")
    except Exception as e:
        print(f"[DIAG] nanite_settings read failed: {e}")

    # --- LOD info / MinLOD ---
    try:
        lod_count = unreal.SkeletalMeshEditorSubsystem.get_lod_count(mesh)
        print(f"[DIAG] LOD count: {lod_count}")
    except Exception as e:
        print(f"[DIAG] get_lod_count failed: {e}")

    try:
        min_lod = mesh.get_editor_property("min_lod")
        print(f"[DIAG] MinLod: {min_lod}")
    except Exception as e:
        print(f"[DIAG] min_lod read failed: {e}")

    # --- Bounds (re-confirm) ---
    try:
        bounds = mesh.get_bounds()
        print(f"[DIAG] Asset bounds: origin={bounds.origin}, box_extent={bounds.box_extent}, sphere_radius={bounds.sphere_radius}")
    except Exception as e:
        print(f"[DIAG] get_bounds failed: {e}")

# Also check the two MI materials themselves resolve and have a valid parent + base color texture.
for mi_path in [
    "/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New",
    "/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.MI_Manny_02_New",
]:
    mi = unreal.EditorAssetLibrary.load_asset(mi_path)
    if not mi:
        print(f"[DIAG] FAILED to load {mi_path}")
        continue
    print(f"[DIAG] Loaded MI: {mi.get_name()}")
    try:
        parent = mi.get_editor_property("parent")
        print(f"[DIAG]   Parent: {parent}")
    except Exception as e:
        print(f"[DIAG]   parent read failed: {e}")

print("[DIAG] Done.")
