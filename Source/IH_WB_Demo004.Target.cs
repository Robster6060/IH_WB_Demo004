// Copyright Invisible Hand. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class IH_WB_Demo004Target : TargetRules
{
	public IH_WB_Demo004Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("IH_WB_Demo004");
	}
}
