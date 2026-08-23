// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_IslandNavSubsystem.h"

#include "IH_P1C08_CoastlineTuningSubsystem.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IH_GeographicalNameRow.h"
#include "IH_IslandSectorRow.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IHSeedIslandLibrary.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	static int32 HashCombineInt(int32 A, int32 B)
	{
		return A ^ (B + 0x9E3779B9 + (A << 6) + (A >> 2));
	}
}

void UIH_P1C08_IslandNavSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString CsvRelativePath = DefaultGeographicalNamesCsvPath();
	const FString FullPath = FPaths::ProjectContentDir() / CsvRelativePath;
	if (!LoadGeographicalNamesFromCsv(CsvRelativePath))
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("P1C08 IslandNav: failed to load %s (full: %s)"), *CsvRelativePath, *FullPath);
	}
	else
	{
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("P1C08 IslandNav: loaded geographical names from %s — %d names, %d origins"),
			*FullPath, NameCatalog.Num(), OriginOrder.Num());
	}
}

bool UIH_P1C08_IslandNavSubsystem::LoadGeographicalNamesFromCsv(const FString& ContentRelativePath)
{
	NameCatalog.Reset();
	OriginOrder.Reset();
	OriginToCatalogIndices.Reset();

	const FString FullPath = FPaths::ProjectContentDir() / ContentRelativePath;
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FullPath))
	{
		return false;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, false);
	TSet<FString> SeenOrigins;

	const bool bHasRowKeyColumn = Lines.Num() > 0
		&& (Lines[0].StartsWith(TEXT("Name,")) || Lines[0].StartsWith(TEXT("---,")));

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		if (LineIndex == 0)
		{
			continue;
		}

		const FString& Line = Lines[LineIndex];
		if (Line.IsEmpty())
		{
			continue;
		}

		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(","), false);
		const int32 MinColumns = bHasRowKeyColumn ? 4 : 3;
		if (Parts.Num() < MinColumns)
		{
			continue;
		}

		const int32 OriginIndex = bHasRowKeyColumn ? 1 : 0;
		const int32 NameIndex = bHasRowKeyColumn ? 2 : 1;
		const int32 TransliterationIndex = bHasRowKeyColumn ? 3 : 2;

		FNameEntry Entry;
		Entry.Origin = Parts[OriginIndex].TrimStartAndEnd();
		Entry.Name = Parts[NameIndex].TrimStartAndEnd();
		Entry.Transliteration = Parts[TransliterationIndex].TrimStartAndEnd();
		Entry.Origin.ReplaceInline(TEXT("\""), TEXT(""));
		Entry.Name.ReplaceInline(TEXT("\""), TEXT(""));
		Entry.Transliteration.ReplaceInline(TEXT("\""), TEXT(""));

		if (Entry.Origin.IsEmpty() || Entry.Name.IsEmpty())
		{
			continue;
		}

		const int32 CatalogIndex = NameCatalog.Add(Entry);
		OriginToCatalogIndices.FindOrAdd(Entry.Origin).Add(CatalogIndex);
		if (!SeenOrigins.Contains(Entry.Origin))
		{
			SeenOrigins.Add(Entry.Origin);
			OriginOrder.Add(Entry.Origin);
		}
	}

	return NameCatalog.Num() > 0 && OriginOrder.Num() > 0;
}

void UIH_P1C08_IslandNavSubsystem::ClearAssignmentState()
{
	IslandRecords.Reset();
	UsedNames.Reset();
	IslandSectorsTable = nullptr;
	SelectedIslandIndex = INDEX_NONE;
}

void UIH_P1C08_IslandNavSubsystem::ComputeAcresBudgets(
	int32 IslandCount, float RealmHalfExtentNSKm, float RealmHalfExtentEWKm, float DevLandAreaFraction, TArray<int32>& OutAcres)
{
	const int32 TotalAcres = UIHSeedIslandLibrary::ComputeTotalLandAcres(
		RealmHalfExtentNSKm, RealmHalfExtentEWKm, DevLandAreaFraction);
	UIHSeedIslandLibrary::ComputeAcresBudgets(IslandCount, TotalAcres, OutAcres);
}

FString UIH_P1C08_IslandNavSubsystem::PickRandomOrigin(FRandomStream& Stream) const
{
	if (OriginOrder.Num() == 0)
	{
		return FString();
	}
	return OriginOrder[Stream.RandRange(0, OriginOrder.Num() - 1)];
}

int32 UIH_P1C08_IslandNavSubsystem::FindOriginIndex(const FString& Origin) const
{
	return OriginOrder.IndexOfByKey(Origin);
}

void UIH_P1C08_IslandNavSubsystem::ReleaseName(const FString& Name)
{
	if (!Name.IsEmpty())
	{
		UsedNames.Remove(Name);
	}
}

bool UIH_P1C08_IslandNavSubsystem::TryPickUnusedNameFromOrigin(
	const FString& Origin,
	FRandomStream& Stream,
	FNameEntry& OutEntry) const
{
	const TArray<int32>* Indices = OriginToCatalogIndices.Find(Origin);
	if (!Indices || Indices->Num() == 0)
	{
		return false;
	}

	TArray<int32> Candidates;
	Candidates.Reserve(Indices->Num());
	for (int32 Index : *Indices)
	{
		if (NameCatalog.IsValidIndex(Index) && !UsedNames.Contains(NameCatalog[Index].Name))
		{
			Candidates.Add(Index);
		}
	}

	if (Candidates.Num() == 0)
	{
		return false;
	}

	const int32 Pick = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	OutEntry = NameCatalog[Pick];
	return true;
}

bool UIH_P1C08_IslandNavSubsystem::TryPickUnusedNameWithOverflow(
	const FString& StartOrigin,
	FRandomStream& Stream,
	FNameEntry& OutEntry) const
{
	const int32 StartIndex = FindOriginIndex(StartOrigin);
	const int32 OriginCount = OriginOrder.Num();
	if (OriginCount == 0)
	{
		return false;
	}

	for (int32 Step = 0; Step < OriginCount; ++Step)
	{
		const int32 OriginIndex = (StartIndex + Step) % OriginCount;
		if (TryPickUnusedNameFromOrigin(OriginOrder[OriginIndex], Stream, OutEntry))
		{
			return true;
		}
	}

	return false;
}

bool UIH_P1C08_IslandNavSubsystem::AssignNameForIsland(
	int32 IslandIndex,
	const FString& PreferredOrigin,
	FRandomStream& Stream)
{
	if (!IslandRecords.IsValidIndex(IslandIndex))
	{
		return false;
	}

	FNameEntry Picked;
	if (!TryPickUnusedNameWithOverflow(PreferredOrigin, Stream, Picked))
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("P1C08 IslandNav: exhausted geographical names for island %d (origin=%s)."),
			IslandIndex, *PreferredOrigin);
		return false;
	}

	ReleaseName(IslandRecords[IslandIndex].Name);
	UsedNames.Add(Picked.Name);

	FIHIslandNavRecord& Record = IslandRecords[IslandIndex];
	Record.Origin = PreferredOrigin;
	Record.Name = Picked.Name;
	Record.Transliteration = Picked.Transliteration;
	return true;
}

void UIH_P1C08_IslandNavSubsystem::RebuildIslandSectorsTable()
{
	if (!IslandSectorsTable)
	{
		IslandSectorsTable = NewObject<UDataTable>(this, NAME_None);
		IslandSectorsTable->RowStruct = FIHIslandSectorRow::StaticStruct();
	}
	else
	{
		IslandSectorsTable->EmptyTable();
	}

	for (const FIHIslandNavRecord& Record : IslandRecords)
	{
		FIHIslandSectorRow Row;
		Row.IslandIndex = Record.IslandIndex;
		Row.SectorBudget = Record.DryAcres;
		Row.WwfSectorBudget = Record.WwfAcres;
		Row.Origin = Record.Origin;
		Row.Name = Record.Name;
		IslandSectorsTable->AddRow(FName(*FString::Printf(TEXT("Island_%d"), Record.IslandIndex)), Row);
	}
}

void UIH_P1C08_IslandNavSubsystem::SetIslandWwfAcres(int32 IslandIndex, int32 WwfAcres)
{
	for (FIHIslandNavRecord& Record : IslandRecords)
	{
		if (Record.IslandIndex == IslandIndex)
		{
			Record.WwfAcres = FMath::Max(0, WwfAcres);
			RebuildIslandSectorsTable();
			NotifyChanged();
			return;
		}
	}
}

void UIH_P1C08_IslandNavSubsystem::SetIslandDryAcres(int32 IslandIndex, int32 DryAcres)
{
	for (FIHIslandNavRecord& Record : IslandRecords)
	{
		if (Record.IslandIndex == IslandIndex)
		{
			Record.DryAcres = FMath::Max(0, DryAcres);
			RebuildIslandSectorsTable();
			NotifyChanged();
			return;
		}
	}
}

void UIH_P1C08_IslandNavSubsystem::AssignIslandsOnSpawn(
	int32 IslandCount, int32 MasterSeed, float RealmHalfExtentNSKm, bool bResetPerIslandTuning)
{
	if (bResetPerIslandTuning)
	{
		ClearPerIslandEditState();
	}
	ClearAssignmentState();
	ActiveMasterSeed = MasterSeed;

	if (IslandCount <= 0 || OriginOrder.Num() == 0)
	{
		NotifyChanged();
		return;
	}

	float RealmHalfExtentEWKm = UIHSeedIslandLibrary::ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	float DevLandFraction = IHInvisibleHandSpec::DefaultDevLandAreaFraction;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_WB_Demo004GameInstance* StoryGI = Cast<UIH_WB_Demo004GameInstance>(GI))
		{
			RealmHalfExtentEWKm = StoryGI->GetRealmHalfExtentEWKm();
			DevLandFraction = StoryGI->GetDevLandAreaFraction();
		}
	}

	TArray<int32> AcresBudgets;
	ComputeAcresBudgets(IslandCount, RealmHalfExtentNSKm, RealmHalfExtentEWKm, DevLandFraction, AcresBudgets);

	IslandRecords.Reserve(IslandCount);
	for (int32 IslandIndex = 0; IslandIndex < IslandCount; ++IslandIndex)
	{
		FIHIslandNavRecord Record;
		Record.IslandIndex = IslandIndex;
		Record.DryAcres = AcresBudgets.IsValidIndex(IslandIndex) ? AcresBudgets[IslandIndex] : 0;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UIH_WB_Demo004GameInstance* StoryGI = Cast<UIH_WB_Demo004GameInstance>(GI))
			{
				if (StoryGI->GetMapSeedPhase1().bSuccess)
				{
					for (const FIHIslandSpawnPlan& Plan : StoryGI->GetMapSeedPhase1().SpawnPlans)
					{
						if (Plan.IslandIndex == IslandIndex)
						{
							Record.TemplateType = Plan.TemplateType;
							break;
						}
					}
				}
			}
		}
		IslandRecords.Add(Record);
	}

	FRandomStream Stream(HashCombineInt(MasterSeed, IslandCount));
	for (int32 IslandIndex = 0; IslandIndex < IslandCount; ++IslandIndex)
	{
		const FString Origin = PickRandomOrigin(Stream);
		FRandomStream NameStream(HashCombineInt(MasterSeed, HashCombineInt(IslandIndex, 0x4E17)));
		AssignNameForIsland(IslandIndex, Origin, NameStream);
	}

	RebuildIslandSectorsTable();
	NotifyChanged();
}

void UIH_P1C08_IslandNavSubsystem::SetIslandOrigin(int32 IslandIndex, const FString& NewOrigin)
{
	if (!IslandRecords.IsValidIndex(IslandIndex) || NewOrigin.IsEmpty())
	{
		return;
	}

	FRandomStream Stream(HashCombineInt(ActiveMasterSeed, HashCombineInt(IslandIndex, GetTypeHash(NewOrigin))));
	if (!AssignNameForIsland(IslandIndex, NewOrigin, Stream))
	{
		return;
	}

	RebuildIslandSectorsTable();
	NotifyChanged();
}

void UIH_P1C08_IslandNavSubsystem::NotifyChanged()
{
	OnIslandNavChanged.Broadcast();
}

void UIH_P1C08_IslandNavSubsystem::SetSelectedIslandIndex(int32 IslandIndex)
{
	if (IslandIndex != INDEX_NONE && !IslandRecords.IsValidIndex(IslandIndex))
	{
		return;
	}

	if (SelectedIslandIndex == IslandIndex)
	{
		return;
	}

	SelectedIslandIndex = IslandIndex;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			Tuning->LoadActiveIslandFromSelection();
		}
	}

	OnSelectionChanged.Broadcast(SelectedIslandIndex);
}

FIHIslandCoastlineTuning UIH_P1C08_IslandNavSubsystem::GetCommittedCoastlineTuning(int32 IslandIndex) const
{
	if (const FIHIslandCoastlineTuning* Found = PerIslandCoastlineTuning.Find(IslandIndex))
	{
		return *Found;
	}
	return FIHIslandCoastlineTuning::SeedBaseline();
}

void UIH_P1C08_IslandNavSubsystem::SetCommittedCoastlineTuning(int32 IslandIndex, const FIHIslandCoastlineTuning& Tuning)
{
	if (!IslandRecords.IsValidIndex(IslandIndex))
	{
		return;
	}

	FIHIslandCoastlineTuning Stored = Tuning;
	Stored.bUserEdited = true;
	PerIslandCoastlineTuning.Add(IslandIndex, Stored);
}

void UIH_P1C08_IslandNavSubsystem::SetCommittedCoastlineTuningFromProfile(
	int32 IslandIndex,
	const FIHIslandCoastlineTuning& Tuning)
{
	if (IslandIndex == INDEX_NONE)
	{
		return;
	}

	FIHIslandCoastlineTuning Stored = Tuning;
	Stored.bUserEdited = false;
	PerIslandCoastlineTuning.Add(IslandIndex, Stored);
}

FIHIslandManualTransform UIH_P1C08_IslandNavSubsystem::GetCommittedManualTransform(int32 IslandIndex) const
{
	if (const FIHIslandManualTransform* Found = PerIslandManualTransform.Find(IslandIndex))
	{
		return *Found;
	}
	return FIHIslandManualTransform();
}

void UIH_P1C08_IslandNavSubsystem::SetCommittedManualTransform(int32 IslandIndex, const FIHIslandManualTransform& Transform)
{
	if (!IslandRecords.IsValidIndex(IslandIndex))
	{
		return;
	}

	PerIslandManualTransform.Add(IslandIndex, Transform);
}

void UIH_P1C08_IslandNavSubsystem::ClearPerIslandEditState()
{
	PerIslandCoastlineTuning.Reset();
	PerIslandManualTransform.Reset();
}

bool UIH_P1C08_IslandNavSubsystem::TryGetNavRecord(int32 IslandIndex, FIHIslandNavRecord& OutRecord) const
{
	if (!IslandRecords.IsValidIndex(IslandIndex))
	{
		return false;
	}
	OutRecord = IslandRecords[IslandIndex];
	return true;
}
