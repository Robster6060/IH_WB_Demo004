// Copyright Invisible Hand. All Rights Reserved.

#include "IHIslandIdentityService.h"
#include "IHCoastGenerationTypes.h"

EIHIslandProfile FIHIslandIdentityService::PickProfile(int32 MasterSeed, int32 IslandIndex)
{
	return IHPickIslandProfile321(MasterSeed, IslandIndex);
}
