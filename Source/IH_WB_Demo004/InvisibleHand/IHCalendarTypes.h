// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IHCalendarTypes.generated.h"

/** InvisibleHand_CalendarSystem.md §7 — ten ordered gameplay time-of-day brackets. */
UENUM(BlueprintType)
enum class EIHTimeBracket : uint8
{
	Midnight,
	Daybreak,
	Sunrise,
	Midmorning,
	HighNoon,
	Afternoon,
	Eventide,
	Sunset,
	Twilight,
	Nightfall
};

/** IH's own Nordic/Temperate/Tropical latitude selection (Demo004-scoped native equivalent of
 * the canonical E_LatitudeType Blueprint enum — see IHInvisibleHandDesignSpec.h for why). */
UENUM(BlueprintType)
enum class EIHRealmLatitude : uint8
{
	Nordic,
	Temperate,
	Tropical
};
