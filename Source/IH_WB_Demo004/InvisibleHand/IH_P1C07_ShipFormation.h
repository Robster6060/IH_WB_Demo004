// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace IH_P1C07Formation
{
	/** Spacing between formation berths (cm). */
	static constexpr float BerthSpacingCm = 4000.f;

	/** Offset from fleet anchor for ship/unit Index of Total (line abreast + slight column
	 * stagger). SpacingCm defaults to the ship berth spacing; callers with a different unit scale
	 * (e.g. human-scale troops) should pass their own spacing rather than use the 40m ship default. */
	inline FVector ComputeBerthOffset(int32 Index, int32 Total, const FVector& ApproachDirWorld, float SpacingCm = BerthSpacingCm)
	{
		if (Total <= 1)
		{
			return FVector::ZeroVector;
		}

		const FVector Forward = ApproachDirWorld.GetSafeNormal2D();
		FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		if (Right.IsNearlyZero())
		{
			Right = FVector::RightVector;
		}
		Right.Normalize();

		const float Center = (Total - 1) * 0.5f;
		const float AlongRight = (static_cast<float>(Index) - Center) * SpacingCm;
		const int32 Row = Index / FMath::Max(1, (Total + 1) / 2);
		const float AlongBack = -static_cast<float>(Row) * SpacingCm * 0.35f;

		return Right * AlongRight + Forward * AlongBack;
	}
}
