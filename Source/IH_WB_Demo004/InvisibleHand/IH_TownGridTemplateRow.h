// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_TownGridTemplate.csv

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridTemplateRow.generated.h"

/**
 * Canonical town grid GIS Blueprint Layer template definition (T1–T5).
 *
 * CSV / DataTable (DT_TownGridTemplate):
 *   Header: ---,templateID,displayName,...  Editor import: Row Type = FIHTownGridTemplateRow, Import Key Field = (empty)
 *   Note: gridPlacementTimestamp is runtime-only (not authored in CSV).
 *   Runtime fallback: UIHTownGridDataSubsystem loads Content/InvisibleHand/Data/DataTables/DT_TownGridTemplate.csv
 */
USTRUCT(BlueprintType)
struct FIHTownGridTemplateRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	FName templateID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	FString displayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	FString charterTierHint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	EIHParcelZoneCode defaultCommonsZonePrimary = EIHParcelZoneCode::CIV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	EIHParcelZoneCode defaultCommonsZoneSecondary = EIHParcelZoneCode::SPD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bRequiresFlatTerrain = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bRequiresHillock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bRequiresValleyAxis = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	int32 radialSpokeCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	float citadelEllipseAspect = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	int32 citadelGateCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	FName harmonicBlockPresetRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bAutoSnapBoulevardsToRegionalRoads = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	float maxStretchSlopeDeg = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bOuterRingStretchOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bBlueprintLayerStretchFree = true;
};
