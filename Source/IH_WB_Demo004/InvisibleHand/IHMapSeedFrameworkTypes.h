// Copyright Epic Games, Inc. All Rights Reserved.
// P1C09 Phase 1 — seed context, island templates (3:2:1), spawn/budget plans.
//
// Two orthogonal canonical axes (both deterministic per seed-word):
// 1) Land AREA split — Fibonacci island percentages (2–7 landforms, sums to 100%; see IHInvisibleHandDesignSpec::CanonicalIslandDrySectorsTable).
// 2) SHAPE template — Low:High:Volcanic = 3:2:1 (which island gets which generator profile in Phase 2+).

#pragma once

#include "CoreMinimal.h"
#include "IHMapSeedFrameworkTypes.generated.h"

/** Bump when assignment/budget rules change (reproducibility contract). */
constexpr int32 IHMapSeedFrameworkGeneratorVersion = 4;

UENUM(BlueprintType)
enum class EIHIslandTemplateType : uint8
{
	Low UMETA(DisplayName = "Low Island"),
	High UMETA(DisplayName = "High Island"),
	Volcanic UMETA(DisplayName = "Volcanic Island"),
};

/** Gate 2b: shoreline slope behavior (orthogonal to summit relief). */
UENUM(BlueprintType)
enum class EIHCoastShoreProfile : uint8
{
	Beach UMETA(DisplayName = "Beach"),
	Gentle UMETA(DisplayName = "Gentle Shore"),
	Cliff UMETA(DisplayName = "Cliff Shore"),
};

/** Gate 2b: interior summit shape (orthogonal to coast profile). */
UENUM(BlueprintType)
enum class EIHSummitReliefArchetype : uint8
{
	LowMound UMETA(DisplayName = "Low Mound"),
	HighRidge UMETA(DisplayName = "High Ridge"),
	VolcanicCone UMETA(DisplayName = "Volcanic Cone"),
};

USTRUCT(BlueprintType)
struct FIHIslandTemplateWeights
{
	GENERATED_BODY()

	// HIGH/VOLC retired (IH-DEC-064/069): weights collapsed to 100%-Low rather than removing
	// EIHIslandTemplateType's High/Volcanic values outright - the layout-footprint math, Nav UI,
	// and Template Gallery/Coastline Tuning dev-tool subsystem that also key off this enum stay
	// intact and compiling, they just never see anything but Low assigned now.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Map Seed")
	int32 LowWeight = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Map Seed")
	int32 HighWeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Map Seed")
	int32 VolcanicWeight = 0;

	/** Only Low is required - High/Volcanic are deliberately zero-weight (see comment above), not
	 * an invalid/degenerate configuration. */
	bool IsValidWeights() const
	{
		return LowWeight > 0;
	}

	int32 TotalWeight() const { return LowWeight + HighWeight + VolcanicWeight; }
};

USTRUCT(BlueprintType)
struct FIHMapSeedContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FString NormalizedSeedWord;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 MasterSeedInt32 = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 GeneratorVersion = IHMapSeedFrameworkGeneratorVersion;

	/** From IH eight-char convention (last digit 2–7). */
	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 IslandCount = 0;
};

USTRUCT(BlueprintType)
struct FIHIslandSpawnPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 IslandIndex = 0;

	/** Layout/process order after deterministic shuffle (0 = first). */
	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 SpawnOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	EIHIslandTemplateType TemplateType = EIHIslandTemplateType::Low;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 AreaBudgetAcres = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float AreaBudgetKm2 = 0.f;

	/** Size* coast envelope for tank layout (km); Fibonacci area × template footprint. */
	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float LayoutCoastExtentKm = 0.f;

	/** Fibonacci rank (0 = largest canonical slice). */
	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 FibonacciRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float TemplateFootprintFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 PlacementStreamSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 OrientationStreamSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 DetailStreamSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 CenterStreamSeed = 0;
};

USTRUCT(BlueprintType)
struct FIHIslandLayoutSolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float TargetEffectiveLandFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float BaseEffectiveLandFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float MaxEffectiveLandFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float AchievedEffectiveLandFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float UniformAreaScale = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	bool bUsedCompactPlacement = false;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	bool bUsedMaxSpreadFallback = false;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<float> IslandAreasKm2;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<float> LayoutExtentKm;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<FVector2D> CentersKm;
};

USTRUCT(BlueprintType)
struct FIHIslandBudgetPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	int32 TotalLandAcres = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<int32> PerIslandAcres;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<int32> PerIslandTargetSectors;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float RealmHalfExtentNSKm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float RealmHalfExtentEWKm = 0.f;

	/** Legacy budget fraction (informational; acres derive from layout solve). */
	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float DevLandAreaFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float TargetEffectiveLandFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	float AchievedEffectiveLandFraction = 0.f;
};

USTRUCT(BlueprintType)
struct FIHMapSeedPhase1DebugRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FString Key;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FString Value;
};

USTRUCT(BlueprintType)
struct FIHMapSeedPhase1Result
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FIHMapSeedContext SeedContext;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<FIHIslandSpawnPlan> SpawnPlans;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FIHIslandBudgetPlan BudgetPlan;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	FIHIslandLayoutSolveResult LayoutSolve;

	UPROPERTY(BlueprintReadOnly, Category = "Invisible Hand|Map Seed")
	TArray<FIHMapSeedPhase1DebugRow> DebugReport;
};
