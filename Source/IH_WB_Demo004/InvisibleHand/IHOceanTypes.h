// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IHOceanTypes.generated.h"

/** Selects which ocean actor Gate 0 spawns. See IH_WaterlinePro_Conversion.md (IH-DEC-043) for the
 * staged-migration plan this enum drives. */
UENUM(BlueprintType)
enum class EIHOceanProvider : uint8
{
	LegacyProcedural,
	WaterlineGen4
};
