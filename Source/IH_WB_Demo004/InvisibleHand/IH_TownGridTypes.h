// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Town Grid overlay data (M2/M3 prototype).

#pragma once

#include "CoreMinimal.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridTypes.generated.h"

/** Matches IHInvisibleHandSpec::ETownGridEditMode for Blueprint exposure. */
UENUM(BlueprintType)
enum class EIHTownGridEditMode : uint8
{
	Place = 0 UMETA(DisplayName = "Place"),
	Move UMETA(DisplayName = "Move"),
	GripEdit UMETA(DisplayName = "GripEdit"),
};

UENUM(BlueprintType)
enum class EIHTownGridGripHandle : uint8
{
	None = 0,
	CornerNE,
	CornerNW,
	CornerSE,
	CornerSW,
	EdgeNorth,
	EdgeSouth,
	EdgeEast,
	EdgeWest,
};

USTRUCT(BlueprintType)
struct FTownGridOverlaySegment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector StartWorld = FVector::ZeroVector;

	UPROPERTY()
	FVector EndWorld = FVector::ZeroVector;

	IHInvisibleHandSpec::ETownGridRoadOverlayClass RoadClass =
		IHInvisibleHandSpec::ETownGridRoadOverlayClass::LocalStreet;

	UPROPERTY()
	bool bCommonsParcelLine = false;
};

USTRUCT(BlueprintType)
struct FTownGridOverlayCommonsCell
{
	GENERATED_BODY()

	UPROPERTY()
	FVector CenterWorld = FVector::ZeroVector;

	UPROPERTY()
	FVector2D HalfExtentLocalCm = FVector2D::ZeroVector;

	UPROPERTY()
	EIHParcelZoneCode ZonePrimary = EIHParcelZoneCode::CIV;

	UPROPERTY()
	EIHParcelZoneCode ZoneSecondary = EIHParcelZoneCode::SPD;
};

USTRUCT(BlueprintType)
struct FTownGridOverlayData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTownGridOverlaySegment> Segments;

	UPROPERTY()
	TArray<FTownGridOverlayCommonsCell> CommonsCells;
};
