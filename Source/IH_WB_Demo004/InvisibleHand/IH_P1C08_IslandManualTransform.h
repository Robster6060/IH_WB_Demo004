// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_P1C08_IslandManualTransform.generated.h"

/** Per-island XY offset (cm) and yaw (deg) relative to seed layout center. */
USTRUCT(BlueprintType)
struct FIHIslandManualTransform
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D OffsetXYCm = FVector2D::ZeroVector;

	UPROPERTY()
	float YawDeg = 0.f;

	UPROPERTY()
	bool bUserMoved = false;

	bool operator==(const FIHIslandManualTransform& Other) const
	{
		return OffsetXYCm.Equals(Other.OffsetXYCm, 0.1f)
			&& FMath::IsNearlyEqual(YawDeg, Other.YawDeg, 0.01f)
			&& bUserMoved == Other.bUserMoved;
	}

	bool operator!=(const FIHIslandManualTransform& Other) const { return !(*this == Other); }
};
