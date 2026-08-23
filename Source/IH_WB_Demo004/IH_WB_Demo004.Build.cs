// Copyright Invisible Hand. All Rights Reserved.

using UnrealBuildTool;

public class IH_WB_Demo004 : ModuleRules
{
	public IH_WB_Demo004(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"EngineSettings",
			"InputCore",
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Water",
			"AssetRegistry",
			"ImageWrapper",
			"Niagara",
			"Paper2D",
			"ProceduralMeshComponent",
			"GeometryCore",
			"GeometryFramework",
		});

		PublicIncludePaths.AddRange(new string[] {
			"IH_WB_Demo004",
			"IH_WB_Demo004/InvisibleHand",
			"IH_WB_Demo004/WorldBuilder",
			"IH_WB_Demo004/WorldBuilder/CoastGeneration",
			"IH_WB_Demo004/WorldBuilder/CellGraph",
			"IH_WB_Demo004/Data",
		});
	}
}
