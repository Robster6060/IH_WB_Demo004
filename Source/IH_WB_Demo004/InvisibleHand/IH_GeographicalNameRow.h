// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for IH_Geographical_Names.csv

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_GeographicalNameRow.generated.h"

/**
 * One geographical name entry: Origin → Name → Transliteration.
 *
 * CSV / DataTable (IH_Geographical_Names):
 *   Header: Name,Origin,Name,Transliteration
 *   Row key (column 1): unique, e.g. Britain_Adlestrop
 *   Import: Row Type = FIHGeographicalNameRow, Import Key Field = (empty)
 */
USTRUCT(BlueprintType)
struct FIHGeographicalNameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Geography")
	FString Origin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Geography")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Geography")
	FString Transliteration;
};
