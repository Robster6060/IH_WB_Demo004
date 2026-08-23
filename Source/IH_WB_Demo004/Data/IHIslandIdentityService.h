// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHCoastGenerationTypes.h"

/** Thin shim — full Demo identity service deferred; profile pick lives in IHCoastGenerationTypes. */
struct IH_WB_DEMO004_API FIHIslandIdentityService
{
	static EIHIslandProfile PickProfile(int32 MasterSeed, int32 IslandIndex);
};
