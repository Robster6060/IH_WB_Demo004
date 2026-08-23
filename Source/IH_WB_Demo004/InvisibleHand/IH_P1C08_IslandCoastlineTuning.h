// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_P1C08_IslandCoastlineTuning.generated.h"



/** Per-island coastline dev tuning (seed baseline until bUserEdited). */

USTRUCT(BlueprintType)

struct FIHIslandCoastlineTuning

{

	GENERATED_BODY()



	UPROPERTY()

	float FbmAmplitudeScale = 1.f;



	UPROPERTY()

	float FbmFrequencyScale = 1.f;



	UPROPERTY()

	float DomainWarpStrengthScale = 1.f;



	UPROPERTY()

	float LobeStrengthScale = 1.f;



	UPROPERTY()

	float RippleStrengthScale = 1.f;



	/** 0 = slightly jittered grid, 1 = full blue-noise island placement. */

	UPROPERTY()

	float PlacementScatter = 1.f;



	/** Linear scale on island semi-axes (1.0–2.0). */

	UPROPERTY()

	float IslandSizeMultiplier = 1.f;



	/** Multiplier on procedural summit height for this island (0.25–2.5). */

	UPROPERTY()

	float SummitAltitudeScale = 1.f;



	UPROPERTY()

	bool bUserEdited = false;



	static FIHIslandCoastlineTuning SeedBaseline()

	{

		return FIHIslandCoastlineTuning();

	}

	bool operator==(const FIHIslandCoastlineTuning& Other) const
	{
		return FMath::IsNearlyEqual(FbmAmplitudeScale, Other.FbmAmplitudeScale)
			&& FMath::IsNearlyEqual(FbmFrequencyScale, Other.FbmFrequencyScale)
			&& FMath::IsNearlyEqual(DomainWarpStrengthScale, Other.DomainWarpStrengthScale)
			&& FMath::IsNearlyEqual(LobeStrengthScale, Other.LobeStrengthScale)
			&& FMath::IsNearlyEqual(RippleStrengthScale, Other.RippleStrengthScale)
			&& FMath::IsNearlyEqual(PlacementScatter, Other.PlacementScatter)
			&& FMath::IsNearlyEqual(IslandSizeMultiplier, Other.IslandSizeMultiplier)
			&& FMath::IsNearlyEqual(SummitAltitudeScale, Other.SummitAltitudeScale);
	}

	bool operator!=(const FIHIslandCoastlineTuning& Other) const { return !(*this == Other); }

};

