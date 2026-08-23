// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — stub row for DT_IslandSectors (pre-bake sector budget per island)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IH_IslandSectorRow.generated.h"

/** Stub sector row created when an island spawns (Phase 5a). */
USTRUCT(BlueprintType)
struct FIHIslandSectorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Sectors")
	int32 IslandIndex = 0;

	/** Integer dry-acre count (1 sector = 1 acre canonical). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Sectors")
	int32 SectorBudget = 0;

	/**
	 * Seaward WWF acre count (1 sector = 1 acre). Polygons past gold coastline into working
	 * waterfront (partly underwater to magenta −25). Zones: SPD/OPE/RAN/AGR/WWF/TRN/MIL.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Sectors")
	int32 WwfSectorBudget = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Sectors")
	FString Origin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Sectors")
	FString Name;
};
