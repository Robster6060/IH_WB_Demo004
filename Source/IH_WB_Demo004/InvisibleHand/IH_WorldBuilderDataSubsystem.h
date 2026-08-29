// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_WorldBuilderDataSubsystem.generated.h"

class UDataTable;

/**
 * Loads World Builder terrain/biome DataTables from cooked .uasset when present, otherwise from
 * CSV scaffolds under Content/InvisibleHand/Data/DataTables/ — same load-or-create pattern as
 * UIHTownGridDataSubsystem, kept as a separate subsystem since these are a WorldBuilder/terrain
 * concept, not a Town Grid one.
 */
UCLASS()
class IH_WB_DEMO004_API UIH_WorldBuilderDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** DT_ASLSlopeBiome (IH-DEC-052) — canonical elevation/slope/latitude biome lookup, row struct
	 * FIHASLSlopeBiomeRow. */
	UFUNCTION(BlueprintPure, Category = "Invisible Hand|World Builder|Data")
	UDataTable* GetASLSlopeBiomeTable() const { return ASLSlopeBiomeTable; }

private:
	UDataTable* LoadOrCreateDataTable(
		const TCHAR* AssetObjectPath,
		const TCHAR* ContentRelativeCsvPath,
		UScriptStruct* RowStruct,
		const TCHAR* TableLabel);

	UPROPERTY()
	TObjectPtr<UDataTable> ASLSlopeBiomeTable;
};
