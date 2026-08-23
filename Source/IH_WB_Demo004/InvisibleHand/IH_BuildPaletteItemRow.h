// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — Data Table row for DT_BuildPaletteItem.csv

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_BuildPaletteItemRow.generated.h"

/**
 * One Right Build Palette fly-out tile (Grid / World / Build / Convey / Defense).
 *
 * CSV / DataTable (DT_BuildPaletteItem):
 *   Header: ---,itemID,paletteTab,... (UE row-name column + struct fields; row name = itemID value)
 *   Editor import: Row Type = FIHBuildPaletteItemRow, Import Key Field = (empty)
 *   Do not use Name column + itemID key, or itemID-only header without --- — both cause import errors.
 *   Runtime fallback: UIHTownGridDataSubsystem loads the CSV via CreateTableFromCSVString.
 */
USTRUCT(BlueprintType)
struct FIHBuildPaletteItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FName itemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	EIHBuildPaletteTab paletteTab = EIHBuildPaletteTab::Grid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FString categoryPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FString displayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FString tooltip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	TSoftObjectPtr<UTexture2D> icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	EIHBuildPaletteInteraction interactionType = EIHBuildPaletteInteraction::DropActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	EIHBuildPaletteLevel levelRequired = EIHBuildPaletteLevel::MainGame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	int32 phaseMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	int32 jurisdictionMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	int32 householdTierMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	int32 researchTierMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	EIHParcelZoneCode zoneRequired = EIHParcelZoneCode::None;

	/** Optional structure category label (FName until EIHStructureCategory expands). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FName structureCategory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FName structureSubTypeRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FName stampRowID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	EIHTownGridTemplate townGridTemplate = EIHTownGridTemplate::Squared;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	FName militaryPackageRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	TSoftClassPtr<AActor> actorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	bool bBlueprintLayerFree = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	bool bTransferPipeTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	bool bHotkeyAssignable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	bool bLockedByDefault = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Build Palette")
	int32 sortOrder = 0;
};
