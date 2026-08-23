// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_TownGridDataSubsystem.generated.h"

class UDataTable;

/**
 * Loads Town Grid / Build Palette DataTables from cooked .uasset when present,
 * otherwise from CSV scaffolds under Content/InvisibleHand/Data/DataTables/.
 */
UCLASS()
class IH_WB_DEMO004_API UIHTownGridDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Town Grid|Data")
	UDataTable* GetBuildPaletteItemTable() const { return BuildPaletteItemTable; }

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Town Grid|Data")
	UDataTable* GetTownGridTemplateTable() const { return TownGridTemplateTable; }

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Town Grid|Data")
	UDataTable* GetTownGridHarmonicBlocksTable() const { return TownGridHarmonicBlocksTable; }

private:
	UDataTable* LoadOrCreateDataTable(
		const TCHAR* AssetObjectPath,
		const TCHAR* ContentRelativeCsvPath,
		UScriptStruct* RowStruct,
		const TCHAR* TableLabel);

	UPROPERTY()
	TObjectPtr<UDataTable> BuildPaletteItemTable;

	UPROPERTY()
	TObjectPtr<UDataTable> TownGridTemplateTable;

	UPROPERTY()
	TObjectPtr<UDataTable> TownGridHarmonicBlocksTable;
};
