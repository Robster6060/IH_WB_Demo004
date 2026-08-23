// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_TownGridHarmonicBlocks.csv

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridHarmonicBlockRow.generated.h"

/**
 * Golden-rectangle block preset for Harmonic town grid (lookup only — no runtime φ math).
 *
 * CSV / DataTable (DT_TownGridHarmonicBlocks):
 *   Header: ---,presetRowID,...  Editor import: Row Type = FIHTownGridHarmonicBlockRow, Import Key Field = (empty)
 *   Runtime fallback: UIHTownGridDataSubsystem loads Content/InvisibleHand/Data/DataTables/DT_TownGridHarmonicBlocks.csv
 */
USTRUCT(BlueprintType)
struct FIHTownGridHarmonicBlockRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	FName presetRowID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	float blockWidthM = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	float blockDepthM = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	float widthDepthRatio = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	int32 ringIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	EIHHarmonicCardinalAxis cardinalAxis = EIHHarmonicCardinalAxis::Cardo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid|Harmonic")
	bool bCornerForumBlock = false;
};
