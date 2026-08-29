// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_WorldBuilderDataSubsystem.h"

#include "IH_ASLSlopeBiomeRow.h"
#include "IH_BiomeTagZoneExemptionRow.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace IH_WorldBuilderDataPaths
{
	static constexpr TCHAR ASLSlopeBiomeAsset[] =
		TEXT("/Game/InvisibleHand/Data/DataTables/DT_ASLSlopeBiome.DT_ASLSlopeBiome");
	static constexpr TCHAR ASLSlopeBiomeCsv[] =
		TEXT("InvisibleHand/Data/DataTables/DT_ASLSlopeBiome.csv");
	static constexpr TCHAR BiomeTagZoneExemptionAsset[] =
		TEXT("/Game/InvisibleHand/Data/DataTables/DT_BiomeTagZoneExemption.DT_BiomeTagZoneExemption");
	static constexpr TCHAR BiomeTagZoneExemptionCsv[] =
		TEXT("InvisibleHand/Data/DataTables/DT_BiomeTagZoneExemption.csv");
}

void UIH_WorldBuilderDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ASLSlopeBiomeTable = LoadOrCreateDataTable(
		IH_WorldBuilderDataPaths::ASLSlopeBiomeAsset,
		IH_WorldBuilderDataPaths::ASLSlopeBiomeCsv,
		FIHASLSlopeBiomeRow::StaticStruct(),
		TEXT("DT_ASLSlopeBiome"));

	BiomeTagZoneExemptionTable = LoadOrCreateDataTable(
		IH_WorldBuilderDataPaths::BiomeTagZoneExemptionAsset,
		IH_WorldBuilderDataPaths::BiomeTagZoneExemptionCsv,
		FIHBiomeTagZoneExemptionRow::StaticStruct(),
		TEXT("DT_BiomeTagZoneExemption"));
}

UDataTable* UIH_WorldBuilderDataSubsystem::LoadOrCreateDataTable(
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
				TEXT("WorldBuilderData: %s asset row struct mismatch (expected %s, got %s) — falling back to CSV"),
				TableLabel,
				RowStruct ? *RowStruct->GetName() : TEXT("(null)"),
				AssetTable->GetRowStruct() ? *AssetTable->GetRowStruct()->GetName() : TEXT("(null)"));
		}
		else
		{
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("WorldBuilderData: loaded %s from asset (%d rows)"),
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
			TEXT("WorldBuilderData: failed to read CSV for %s (%s)"),
			TableLabel, *FullCsvPath);
		return nullptr;
	}

	// UTF-8 BOM breaks header matching (e.g. '---' / 'biomeID' not found in editor or runtime import).
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
			TEXT("WorldBuilderData: failed to parse CSV for %s (%s): %s"),
			TableLabel, *FullCsvPath, *FString::Join(CsvErrors, TEXT("; ")));
		return nullptr;
	}

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("WorldBuilderData: loaded %s from CSV (%s) — %d rows"),
		TableLabel, *FullCsvPath, Table->GetRowNames().Num());
	return Table;
}
