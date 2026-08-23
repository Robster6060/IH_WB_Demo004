// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — T1 Squared town grid generator (M3).

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridTypes.h"

struct FIHTownGridGeneratorParams
{
	FVector CenterWorldCm = FVector::ZeroVector;
	FVector2D BboxHalfExtentCm = FVector2D(6400.f, 6400.f);
	float YawDeg = 0.f;
	float ModuleSizeCm = 800.f;
	int32 CollectorIntervalModules = 4;
	int32 CommonsModules = 4;
	EIHParcelZoneCode CommonsZonePrimary = EIHParcelZoneCode::CIV;
	EIHParcelZoneCode CommonsZoneSecondary = EIHParcelZoneCode::SPD;
};

namespace IH_TownGridSquaredGenerator
{
	IH_WB_DEMO004_API void GenerateSquared(
		const FIHTownGridGeneratorParams& Params,
		FTownGridOverlayData& OutOverlay);

	IH_WB_DEMO004_API FVector2D SnapHalfExtentToModules(
		FVector2D HalfExtentCm,
		float ModuleSizeCm,
		int32 MinModulesPerAxis = 4);
}
