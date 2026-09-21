"""
Checks SKM_Manny_Simple's actual per-LOD geometry (vertex/triangle counts) and MinLOD settings -
if LOD0 has zero real geometry (a stripped/corrupted "Simple" variant) or MinLOD forces an LOD that
doesn't exist, the mesh would render nothing despite every other property checking out clean.
"""
import unreal

MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
mesh = unreal.load_asset(MESH_PATH)
unreal.log(f"[LODDIAG] Mesh: {mesh}")
if mesh:
    lib = unreal.SkeletalMeshEditorSubsystem
    try:
        lod_count = lib.get_lod_count(mesh)
        unreal.log(f"[LODDIAG] LOD count: {lod_count}")
        for i in range(lod_count):
            try:
                verts = lib.get_lod_vertex_count(mesh, i) if hasattr(lib, "get_lod_vertex_count") else None
                unreal.log(f"[LODDIAG] LOD {i}: vertex_count={verts}")
            except Exception as e:
                unreal.log_warning(f"[LODDIAG] LOD {i} vertex query failed: {e}")
    except Exception as e:
        unreal.log_warning(f"[LODDIAG] get_lod_count failed: {e}")

    try:
        min_lod = mesh.get_editor_property("min_lod")
        unreal.log(f"[LODDIAG] MinLod: {min_lod}")
    except Exception as e:
        unreal.log_warning(f"[LODDIAG] min_lod read failed: {e}")

    try:
        ris = mesh.get_editor_property("ray_tracing_min_lod")
        unreal.log(f"[LODDIAG] RayTracingMinLOD: {ris}")
    except Exception:
        pass

    try:
        bounds_ext = mesh.get_bounds()
        unreal.log(f"[LODDIAG] mesh.get_bounds(): {bounds_ext}")
    except Exception as e:
        unreal.log_warning(f"[LODDIAG] get_bounds failed: {e}")

unreal.log("[LODDIAG] Done.")
