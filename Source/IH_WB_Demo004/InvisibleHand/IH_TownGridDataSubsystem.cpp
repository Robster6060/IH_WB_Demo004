// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_TownGridDataSubsystem.h"

#include "IH_BuildPaletteItemRow.h"
#include "IH_TownGridHarmonicBlockRow.h"
#include "IH_TownGridTemplateRow.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace IH_TownGridDataPaths
{
	static constexpr TCHAR BuildPaletteItemAsset[] =
		TEXT("/Game/InvisibleHand/Data/DataTables/DT_BuildPaletteItem.DT_BuildPaletteItem");
	static constexpr TCHAR BuildPaletteItemCsv[] =
		TEXT("InvisibleHand/Data/DataTables/DT_BuildPaletteItem.csv");

	static constexpr TCHAR TownGridTemplateAsset[] =
		TEXT("/Game/InvisibleHand/Data/DataTables/DT_TownGridTemplate.DT_TownGridTemplate");
	static constexpr TCHAR TownGridTemplateCsv[] =
		TEXT("InvisibleHand/Data/DataTables/DT_TownGridTemplate.csv");

	static constexpr TCHAR TownGridHarmonicBlocksAsset[] =
		TEXT("/Game/InvisibleHand/Data/DataTables/DT_TownGridHarmonicBlocks.DT_TownGridHarmonicBlocks");
	static constexpr TCHAR TownGridHarmonicBlocksCsv[] =
		TEXT("InvisibleHand/Data/DataTables/DT_TownGridHarmonicBlocks.csv");
}

void UIHTownGridDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BuildPaletteItemTable = LoadOrCreateDataTable(
		IH_TownGridDataPaths::BuildPaletteItemAsset,
		IH_TownGridDataPaths::BuildPaletteItemCsv,
		FIHBuildPaletteItemRow::StaticStruct(),
		TEXT("DT_BuildPaletteItem"));

	TownGridTemplateTable = LoadOrCreateDataTable(
		IH_TownGridDataPaths::TownGridTemplateAsset,
		IH_TownGridDataPaths::TownGridTemplateCsv,
		FIHTownGridTemplateRow::StaticStruct(),
		TEXT("DT_TownGridTemplate"));

	TownGridHarmonicBlocksTable = LoadOrCreateDataTable(
		IH_TownGridDataPaths::TownGridHarmonicBlocksAsset,
		IH_TownGridDataPaths::TownGridHarmonicBlocksCsv,
		FIHTownGridHarmonicBlockRow::StaticStruct(),
		TEXT("DT_TownGridHarmonicBlocks"));
}

UDataTable* UIHTownGridDataSubsystem::LoadOrCreateDataTable(
	const TCHAR* AssetObjectPath,
	const TCHAR* ContentRelativeCsvPath,
	UScriptStruct* RowStruct,
	const TCHAR* TableLabel)
{
	if (UDataTable* AssetTable = LoadObject<UDataTable>(nullptr, AssetObjectPath))
	{
		if (AssetTable->GetRowStruct() != RowStruct)
		{
			UE_LOG(
				LogIH_WB_Demo004, Warning,
				TEXT("TownGridData: %s asset row struct mismatch (expected %s, got %s) — falling back to CSV"),
				TableLabel,
				RowStruct ? *RowStruct->GetName() : TEXT("(null)"),
				AssetTable->GetRowStruct() ? *AssetTable->GetRowStruct()->GetName() : TEXT("(null)"));
		}
		else
		{
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("TownGridData: loaded %s from asset (%d rows)"),
				TableLabel, AssetTable->GetRowNames().Num());
			return AssetTable;
		}
	}

	const FString FullCsvPath = FPaths::ProjectContentDir() / ContentRelativeCsvPath;
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FullCsvPath))
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("TownGridData: failed to read CSV for %s (%s)"),
			TableLabel, *FullCsvPath);
		return nullptr;
	}

	// UTF-8 BOM breaks header matching (e.g. '---' / 'itemID' not found in editor or runtime import).
	if (!FileContent.IsEmpty() && FileContent[0] == 0xFEFF)
	{
		FileContent.RightChopInline(1);
	}

	UDataTable* Table = NewObject<UDataTable>(this, NAME_None);
	Table->RowStruct = RowStruct;
	// CSV scaffolds use UE --- row-name format (column 1 = row ID). ImportKeyField must stay empty.
	Table->ImportKeyField.Empty();

	const TArray<FString> CsvErrors = Table->CreateTableFromCSVString(FileContent);
	if (CsvErrors.Num() > 0)
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("TownGridData: failed to parse CSV for %s (%s): %s"),
			TableLabel, *FullCsvPath, *FString::Join(CsvErrors, TEXT("; ")));
		return nullptr;
	}

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("TownGridData: loaded %s from CSV (%s) — %d rows"),
		TableLabel, *FullCsvPath, Table->GetRowNames().Num());
	return Table;
}
