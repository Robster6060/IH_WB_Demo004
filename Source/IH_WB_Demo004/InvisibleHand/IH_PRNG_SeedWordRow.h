// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand - Data Table row for IH_PRNG_2400_Seed_Words (CSV import)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_PRNG_SeedWordRow.generated.h"

/** One row in the seed word Data Table / CSV — five letters in the Word field. */
USTRUCT(BlueprintType)
struct FIHPRNGSeedWordRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Seed")
	FString Word;
};
