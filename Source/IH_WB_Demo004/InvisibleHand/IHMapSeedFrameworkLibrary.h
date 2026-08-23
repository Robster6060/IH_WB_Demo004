// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IHMapSeedFrameworkTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IHMapSeedFrameworkLibrary.generated.h"

UCLASS()
class IH_WB_DEMO004_API UIHMapSeedFrameworkLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 SmallSampleDiversityMaxIslandCount = 4;

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Map Seed")
	static FIHIslandTemplateWeights GetDefaultTemplateWeights();

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Map Seed")
	static FString IslandTemplateTypeToString(EIHIslandTemplateType Type);

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Map Seed")
	static FString IslandTemplateTypeToNavAbbrev(EIHIslandTemplateType Type);

	static int32 DeriveStreamSeed(int32 MasterSeedInt32, uint32 StreamId, int32 Salt = 0);

	/**
	 * Phase 1 pipeline: normalize seed → count → shape (template urn) → Size* (Fibonacci area +
	 * template footprint layout extent, templates matched to Fibonacci ranks) → spawn plans.
	 */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Map Seed",
		meta = (CPP_Default_RealmHalfExtentNSKm = "13.0", CPP_Default_RealmHalfExtentEWKm = "0.0",
			CPP_Default_TargetEffectiveLandFraction = "0.30"))
	static bool BuildPhase1FromSeed(
		const FString& RawSeedWord,
		FIHMapSeedPhase1Result& OutResult,
		float RealmHalfExtentNSKm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm,
		float RealmHalfExtentEWKm = 0.f,
		float TargetEffectiveLandFraction = IHInvisibleHandSpec::DefaultTargetEffectiveLandFraction);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Map Seed")
	static void LogPhase1DebugReport(const FIHMapSeedPhase1Result& Result);

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Map Seed")
	static FString FormatPhase1DebugReport(const FIHMapSeedPhase1Result& Result);

	/** Weighted urn draw without replacement; refills from BaseUrn when empty. */
	static void AssignTemplatesDeterministic(
		int32 IslandCount,
		int32 MasterSeedInt32,
		const FIHIslandTemplateWeights& Weights,
		TArray<EIHIslandTemplateType>& OutTemplates);

	static void ApplySmallSampleDiversityGuard(
		TArray<EIHIslandTemplateType>& InOutTemplates,
		int32 MasterSeedInt32,
		const FIHIslandTemplateWeights& Weights);

	static void BuildSpawnOrderDeterministic(
		int32 IslandCount,
		int32 MasterSeedInt32,
		TArray<int32>& OutSpawnOrderByIslandIndex);

	/** Layout spacing multiplier for Low / High / Volcanic (matches coastline tuning profiles). */
	static float GetTemplateLayoutFootprintFactor(EIHIslandTemplateType TemplateType);

	/** CoastLayoutExtent = sqrt(A/π)×1.22 × footprint (Size* for layout). */
	static float ComputeLayoutCoastExtentKm(float AreaBudgetKm2, EIHIslandTemplateType TemplateType);

	/**
	 * Match drawn templates to Fibonacci ranks: smallest footprint → largest area slice (index 0).
	 * Preserves multiset of drawn templates; only permutes assignment to island indices.
	 */
	static void AssignTemplatesToFibonacciRanks(
		int32 IslandCount,
		TArray<EIHIslandTemplateType>& InOutTemplatesByIslandIndex);

	/** Fill OutLayoutCoastExtentKm from committed Phase1 spawn plans (island-index order). */
	static void GatherLayoutCoastExtentsKmFromPhase1(
		const FIHMapSeedPhase1Result& Phase1,
		TArray<float>& OutLayoutCoastExtentKm);
};
