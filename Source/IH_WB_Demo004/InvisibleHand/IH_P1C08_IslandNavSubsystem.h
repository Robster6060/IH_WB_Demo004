// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_IslandCoastlineTuning.h"
#include "IH_P1C08_IslandManualTransform.h"
#include "IHMapSeedFrameworkTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C08_IslandNavSubsystem.generated.h"

class UDataTable;

USTRUCT(BlueprintType)
struct FIHIslandNavRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	int32 IslandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	FString Origin;

	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	FString Transliteration;

	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	int32 DryAcres = 0;

	/** Seaward WWF footprint acres (1 sector = 1 acre) past gold into shelf. */
	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	int32 WwfAcres = 0;

	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	EIHIslandTemplateType TemplateType = EIHIslandTemplateType::Low;
};

DECLARE_MULTICAST_DELEGATE(FOnIslandNavChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnIslandSelectionChanged, int32);

/** Pre-bake island naming, origin assignment, and dry-acre budgets from φ land budget. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_IslandNavSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stub: dev panels stay visible until bake (Phase 5e will hide). */
	UPROPERTY(BlueprintReadOnly, Category = "P1C08|IslandNav")
	bool bPreBakeMode = true;

	FOnIslandNavChanged OnIslandNavChanged;
	FOnIslandSelectionChanged OnSelectionChanged;

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	bool IsPreBakeMode() const { return bPreBakeMode; }

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	int32 GetSelectedIslandIndex() const { return SelectedIslandIndex; }

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	bool HasSelectedIsland() const { return SelectedIslandIndex != INDEX_NONE; }

	/** Row select in Island Nav — saves prior island tuning, loads new island into Coastline panel. */
	void SetSelectedIslandIndex(int32 IslandIndex);

	FIHIslandCoastlineTuning GetCommittedCoastlineTuning(int32 IslandIndex) const;
	void SetCommittedCoastlineTuning(int32 IslandIndex, const FIHIslandCoastlineTuning& Tuning);
	void SetCommittedCoastlineTuningFromProfile(int32 IslandIndex, const FIHIslandCoastlineTuning& Tuning);

	FIHIslandManualTransform GetCommittedManualTransform(int32 IslandIndex) const;
	void SetCommittedManualTransform(int32 IslandIndex, const FIHIslandManualTransform& Transform);

	/** Clears committed coastline tuning and manual transforms (realm seed regen). */
	void ClearPerIslandEditState();

	/** @deprecated Use GetCommittedCoastlineTuning. */
	FIHIslandCoastlineTuning GetCoastlineTuning(int32 IslandIndex) const { return GetCommittedCoastlineTuning(IslandIndex); }
	/** @deprecated Use SetCommittedCoastlineTuning. */
	void SetCoastlineTuning(int32 IslandIndex, const FIHIslandCoastlineTuning& Tuning) { SetCommittedCoastlineTuning(IslandIndex, Tuning); }
	/** @deprecated Use ClearPerIslandEditState. */
	void ClearPerIslandCoastlineTuning() { ClearPerIslandEditState(); }

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	bool TryGetNavRecord(int32 IslandIndex, FIHIslandNavRecord& OutRecord) const;

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	const TArray<FIHIslandNavRecord>& GetIslandRecords() const { return IslandRecords; }

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	TArray<FString> GetAvailableOrigins() const { return OriginOrder; }

	UFUNCTION(BlueprintPure, Category = "P1C08|IslandNav")
	UDataTable* GetIslandSectorsTable() const { return IslandSectorsTable; }

	/** Assign/reassign all islands after spawn or realm seed regen. */
	void AssignIslandsOnSpawn(int32 IslandCount, int32 MasterSeed, float RealmHalfExtentNSKm, bool bResetPerIslandTuning);

	/** After HF bake: seaward WWF acre budget (1 sector = 1 acre past gold). */
	void SetIslandWwfAcres(int32 IslandIndex, int32 WwfAcres);

	/** Plan Addendum 9: after cell-graph generation, replace the pre-bake phi-budget target with
	 * the realized dry acreage - DryAcres otherwise stays frozen at the pre-generation estimate,
	 * which can diverge substantially from what actually renders (profile/seed-dependent). */
	void SetIslandDryAcres(int32 IslandIndex, int32 DryAcres);

	/** Player changed origin in Nav UI — re-roll name from that origin. */
	void SetIslandOrigin(int32 IslandIndex, const FString& NewOrigin);

	/** Split total land acres across N islands (delegates to UIHSeedIslandLibrary::ComputeAcresBudgets). */
	static void ComputeAcresBudgets(int32 IslandCount, float RealmHalfExtentNSKm, float RealmHalfExtentEWKm,
		float DevLandAreaFraction, TArray<int32>& OutAcres);

	static FString DefaultGeographicalNamesCsvPath()
	{
		return TEXT("InvisibleHand/Data/IH_Geographical_Names.csv");
	}

private:
	struct FNameEntry
	{
		FString Origin;
		FString Name;
		FString Transliteration;
	};

	bool LoadGeographicalNamesFromCsv(const FString& ContentRelativePath);
	void ClearAssignmentState();
	void RebuildIslandSectorsTable();
	FString PickRandomOrigin(FRandomStream& Stream) const;
	bool AssignNameForIsland(int32 IslandIndex, const FString& PreferredOrigin, FRandomStream& Stream);
	bool TryPickUnusedNameFromOrigin(const FString& Origin, FRandomStream& Stream, FNameEntry& OutEntry) const;
	bool TryPickUnusedNameWithOverflow(const FString& StartOrigin, FRandomStream& Stream, FNameEntry& OutEntry) const;
	int32 FindOriginIndex(const FString& Origin) const;
	void ReleaseName(const FString& Name);
	void NotifyChanged();

	UPROPERTY(Transient)
	TArray<FIHIslandNavRecord> IslandRecords;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> IslandSectorsTable = nullptr;

	TArray<FNameEntry> NameCatalog;
	TArray<FString> OriginOrder;
	TMap<FString, TArray<int32>> OriginToCatalogIndices;
	TSet<FString> UsedNames;
	int32 ActiveMasterSeed = 0;

	UPROPERTY(Transient)
	int32 SelectedIslandIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TMap<int32, FIHIslandCoastlineTuning> PerIslandCoastlineTuning;

	UPROPERTY(Transient)
	TMap<int32, FIHIslandManualTransform> PerIslandManualTransform;
};
