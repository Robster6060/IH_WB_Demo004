"""
Phase 0 validation (IH_WB_PCG_Architecture_Canon.md): confirms PCG's native PCGDynamicMeshData can
actually sample this project's real terrain (BakedIslandMesh, a UDynamicMeshComponent - NOT a UE
Landscape actor) before any real PGC groundcover work is built on top of that assumption.

Deliberately minimal - no mesh spawning yet (that's Phase 2's job, where FPCGSoftISMComponentDescriptor
configuration deserves its own careful pass, not a throwaway script). This graph just surface-samples
real island actors (found via the SAME "IH_Island" actor tag UIH_P1C07_IslandCollisionSubsystem
already applies to every island/stamp actor at BeginPlay - see IslandActorTag in
IH_P1C07_IslandCollisionSubsystem.cpp - no new C++ needed for this) and enables Debug visualization
so PIE testing can directly SEE whether real points land on the actual terrain surface, or nothing/
garbage comes through.

Run via:
UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="Scripts/build_pcg_validate_dynamicmesh.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""
import unreal

GRAPH_PACKAGE = "/Game/InvisibleHand/PCG/PCG_ValidateDynamicMesh_Test"

if unreal.EditorAssetLibrary.does_asset_exist(GRAPH_PACKAGE):
    unreal.EditorAssetLibrary.delete_asset(GRAPH_PACKAGE)

factory = unreal.PCGGraphFactory()
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
graph = asset_tools.create_asset("PCG_ValidateDynamicMesh_Test", "/Game/InvisibleHand/PCG", unreal.PCGGraph, factory)
assert graph is not None, "[PCGVALIDATE] Could not create PCG graph asset"

# --- GetActorData: find real island/stamp actors via the "IH_Island" tag every one of them already
# carries (UIH_P1C07_IslandCollisionSubsystem::IslandActorTag, applied at BeginPlay) - ParseActorComponents
# mode should surface BakedIslandMesh's data as PCGDynamicMeshData once an island has been First Baked.
get_actor_node, get_actor_settings = graph.add_node_of_type(unreal.PCGDataFromActorSettings)
get_actor_settings.set_editor_property("mode", unreal.PCGGetDataFromActorMode.PARSE_ACTOR_COMPONENTS)
selector = get_actor_settings.get_editor_property("actor_selector")
selector.set_editor_property("actor_filter", unreal.PCGActorFilter.ALL_WORLD_ACTORS)
selector.set_editor_property("actor_selection", unreal.PCGActorSelection.BY_TAG)
selector.set_editor_property("actor_selection_tag", "IH_Island")
get_actor_settings.set_editor_property("actor_selector", selector)

# --- SurfaceSampler: sparse on purpose (this is a visual "is data real" check, not a density test) ---
# NOTE: bDebug (viewport debug-draw toggle) isn't reachable via set_editor_property under this exact
# property path from Python (Transient/EditCondition metadata likely affects reflection) - not
# essential for Phase 0's core check. Toggle "Debug" on the SurfaceSampler node manually in the graph
# editor (right-click -> Toggle Debug, or the checkbox in its Details panel) if visual confirmation in
# the viewport is wanted; the real validation signal is real point data flowing through at all.
sampler_node, sampler_settings = graph.add_node_of_type(unreal.PCGSurfaceSamplerSettings)
sampler_settings.set_editor_property("points_per_squared_meter", 0.02)

graph.add_edge(get_actor_node, "Out", sampler_node, "In")
graph.add_edge(sampler_node, "Out", graph.get_output_node(), "In")

unreal.EditorAssetLibrary.save_asset(GRAPH_PACKAGE)
unreal.log(f"[PCGVALIDATE] Built and saved {GRAPH_PACKAGE}")
unreal.log("[PCGVALIDATE] Done.")
