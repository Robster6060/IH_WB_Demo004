// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "IHInvisibleHandDesignSpec.h"

#include "IHMapSeedFrameworkTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "IHSeedIslandLibrary.generated.h"



UENUM(BlueprintType)

enum class EIHNoiseLayer : uint8

{

	CoastlineJaggedness UMETA(DisplayName = "Coastline jaggedness"),

};



UCLASS()

class IH_WB_DEMO004_API UIHSeedIslandLibrary : public UBlueprintFunctionLibrary

{

	GENERATED_BODY()



public:

	static constexpr float DefaultTotalLandAreaKM2 = IHInvisibleHandSpec::CanonicalTotalLandAreaKm2;



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Island")

	static bool IsLandformCountInDesignRange(int32 IslandCount);



	/** E-W half-width (km) from N-S half-depth: full width = φ × full depth → halfWidth = φ × halfDepth. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static float ComputeRealmHalfExtentEWKmFromNS(float RealmHalfExtentNSKm);



	/** Tank water surface (km²) = (2×halfDepth) × (2×halfWidth). */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static float ComputeRealmAreaKm2(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm);



	/** Total dry land (km²) = tank area × DevLandAreaFraction (default 0.70). */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain",

		meta = (CPP_Default_DevLandAreaFraction = "0.70"))

	static float ComputeTotalLandKm2(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm,

		float DevLandAreaFraction = IHInvisibleHandSpec::DefaultDevLandAreaFraction);



	/** Rounded integer acres from land budget (1 sector = 1 acre canonical). */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain",

		meta = (CPP_Default_DevLandAreaFraction = "0.70"))

	static int32 ComputeTotalLandAcres(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm,

		float DevLandAreaFraction = IHInvisibleHandSpec::DefaultDevLandAreaFraction);



	/** Fibonacci weights + largest remainder; OutAcres sums to TotalLandAcres exactly. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Island")

	static void ComputeAcresBudgets(int32 IslandCount, int32 TotalLandAcres, TArray<int32>& OutAcres);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Island")

	static void GetFibonacciAreaWeights(int32 IslandCount, TArray<float>& OutWeightsNormalized);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Island")

	static void ComputeIslandAreasKM2(int32 IslandCount, float TotalLandAreaKM2, TArray<float>& OutAreasKM2);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static int32 DerivePerlinNoiseSeed(int32 MasterSeedFromEightChar, int32 IslandIndex, EIHNoiseLayer Layer);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static float PerlinNoise2DSeeded(float X, float Y, int32 NoiseSeed);



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static void DomainWarpOffset2DKm(float WorldXKm, float WorldYKm, int32 WarpNoiseSeed, float WarpFrequencyScale,

		float WarpStrengthKm, float& OutDeltaXKm, float& OutDeltaYKm);



	/**

	 * Default placement: sqrt(N) grid (three-island strip special case).

	 * Radii from canonical areas, scaled for pairwise MinSeparationKm.

	 */

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Terrain", meta = (CPP_Default_MinSeparationKm = "18.0"))

	static void ComputeDefaultIslandCentersAndRadiiKm(int32 IslandCount, float WorldWidthKm, float WorldHeightKm, float MarginKm,

		int32 MasterSeed, const TArray<float>& IslandAreasKm2, float RadiusFromAreaScale, TArray<FVector2D>& OutCentersKm,

		TArray<float>& OutFalloffRadiiKm, float MinSeparationKm = 18.f);



	/**

	 * Tank harness: φ landscape rect centered at world origin (depth N-S, width E-W).

	 * Land km² from DevLandAreaFraction; per-island acres via Fibonacci + largest remainder.

	 * Semi-major from golden-ratio waterline ellipse; seeded organic scatter (blue-noise).

	 * @param RealmHalfExtentNSKm Half-depth N-S in km (4 → 8 km depth edge).

	 * @param RealmHalfExtentEWKm Half-width E-W in km; 0 = auto (φ × halfDepth).

	 * @param PlacementScatter 0 = slightly jittered grid, 1 = full blue-noise.

	 * @param IslandSizeMultiplier Linear scale on semi-axes before placement (1.0–2.0).

	 */

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Terrain",

		meta = (CPP_Default_RealmHalfExtentNSKm = "13.0", CPP_Default_RealmHalfExtentEWKm = "0.0",

			CPP_Default_PlacementScatter = "1.0", CPP_Default_IslandSizeMultiplier = "1.0",

			CPP_Default_DevLandAreaFraction = "0.70"))

	static void ComputeTankIslandLayoutCm(int32 IslandCount, int32 MasterSeed, TArray<FVector2D>& OutCentersWorldCm,

		TArray<float>& OutSemiMajorCm, float RealmHalfExtentNSKm = 13.f, float RealmHalfExtentEWKm = 0.f,

		float PlacementScatter = 1.f, float IslandSizeMultiplier = 1.f,

		float DevLandAreaFraction = IHInvisibleHandSpec::DefaultDevLandAreaFraction);

	/** Size* layout: per-island coast extents from Phase1 (template × Fibonacci area). Skips post-hoc size multiplier. */
	static void ComputeTankIslandLayoutCmWithLayoutExtents(
		int32 IslandCount,
		int32 MasterSeed,
		TArray<FVector2D>& OutCentersWorldCm,
		TArray<float>& OutSemiMajorCm,
		float RealmHalfExtentNSKm,
		float RealmHalfExtentEWKm,
		float PlacementScatter,
		float DevLandAreaFraction,
		const TArray<float>& PerIslandLayoutExtentKm);



	/** Island waterline areas (km²) from integer acre split of the land budget. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain",

		meta = (CPP_Default_RealmHalfExtentEWKm = "0.0", CPP_Default_DevLandAreaFraction = "0.70"))

	static void ComputeIslandAreasKM2FromRealm(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm, int32 IslandCount,

		TArray<float>& OutAreasKM2, float DevLandAreaFraction = IHInvisibleHandSpec::DefaultDevLandAreaFraction);



	/** Per-island summit Z (cm ASL) from each island's own absolute footprint diameter (IH-DEC-052)
	 * — NOT realm-relative area rank (the pre-2026-08-28 formula: 30m smallest -> 180m largest,
	 * scaled by rank within the current realm only, meant the biggest island in a tiny 3-island dev
	 * realm and the biggest island in a future 512,000-acre realm both got pushed toward the same
	 * ceiling). ApexMeters = Diameter / phi^HeightExponent, capped at the canonical 2400m ceiling
	 * (MountainApexMeters) — recalibrated (IH-DEC-055) against the largest island's own Fibonacci
	 * area share in a representative 3-island realm at the 512,000-acre "River Prototype" gate
	 * (IH-DEC-026), not the whole realm's total land as a single island, which the original
	 * IH-DEC-052 calibration never reached in practice. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static void ComputeSummitTopZCmForAreas(const TArray<float>& AreasKm2, TArray<float>& OutSummitTopZCm);

	/** Height-grid coast radius (km) used for tank spacing — matches cell-map extent. */
	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")
	static float CoastLayoutExtentKmFromAreaKm2(float AreaKm2);

	/** Place 3 islands at tank insets; areas scaled by AreaScale² before extent check. */
	static bool TryPlaceThreeIslandsMaxSpreadKm(
		const TArray<float>& AreasKm2,
		float AreaScale,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		TArray<FVector2D>& OutCentersKm,
		float WallMarginKm = 0.875f);

	/** True when every coast pair fits in the tank at uniform area scale. */
	static bool CanFitCoastExtentsInTankAtAreaScale(
		const TArray<float>& AreasKm2,
		float AreaScale,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm = 0.875f);

	/** Coast-fit using precomputed Size* layout extents (linear scale on extent). */
	static bool CanFitLayoutExtentsInTankAtScale(
		const TArray<float>& LayoutExtentKm,
		float ExtentScale,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm = 0.875f);

	static bool TryPlaceThreeIslandsMaxSpreadFromLayoutExtentsKm(
		const TArray<float>& LayoutExtentKm,
		float ExtentScale,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		TArray<FVector2D>& OutCentersKm,
		float WallMarginKm = 0.875f);

	/** Max-spread placement from coast extents (km); supports N=2–7 for overlap resolution. */
	static bool TryPlaceIslandsMaxSpreadKm(
		int32 IslandCount,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		TArray<FVector2D>& OutCentersKm,
		float WallMarginKm = 0.875f);

	/** π×extent² sum / tank — spacing envelope diagnostic only (over-estimates vs area budget). */
	static float ComputeEffectiveLandFractionFromExtentsKm(
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentNSKm,
		float RealmHalfExtentEWKm);

	/** Sum of island area budgets / tank water surface — authoritative effective land metric. */
	static float ComputeEffectiveLandFractionFromAreasKm2(
		const TArray<float>& AreasKm2,
		float RealmHalfExtentNSKm,
		float RealmHalfExtentEWKm);

	/**
	 * Feasibility-first layout solve: compact placement by default, Fibonacci areas derived from
	 * achieved effective land (not DevLandAreaFraction budget).
	 */
	static bool SolveIslandLayoutForSeed(
		int32 IslandCount,
		int32 MasterSeed,
		float RealmHalfExtentNSKm,
		float RealmHalfExtentEWKm,
		float TargetEffectiveLandFraction,
		const TArray<float>& ShapeAreasKm2,
		const TArray<float>& BaseLayoutExtentKm,
		FIHIslandLayoutSolveResult& OutResult,
		float WallMarginKm = 0.875f);

	/** Copy solved centers (cm) and semi-majors from Phase 1 layout solve. */
	static void ApplyIslandLayoutSolveToTankCm(
		const FIHIslandLayoutSolveResult& Solve,
		TArray<FVector2D>& OutCentersWorldCm,
		TArray<float>& OutSemiMajorCm);

	/** Compact jittered-grid placement; max-spread only when compact fails. */
	static bool TryPlaceIslandsCompactKm(
		int32 IslandCount,
		int32 MasterSeed,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		TArray<FVector2D>& OutCentersKm,
		float WallMarginKm = 0.875f,
		bool* bOutUsedMaxSpreadFallback = nullptr);

	/** Semi-major axis (km) for a waterline ellipse with golden-ratio aspect and AreaKm2 footprint. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static float SemiMajorKmFromWaterlineAreaKm2(float AreaKm2);



	/** Minimum center distance (cm): 500 m + 1.1 × (R1 + R2) coast collision envelopes. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static float ComputeMinIslandCenterDistanceCm(float SemiMajorCmA, float SemiMajorCmB);



	/** Realm edge margin (cm) for layout AABB. */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain")

	static float GetRealmEdgeMarginCm() { return 87500.f; }



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain", meta = (CPP_Default_RealmHalfExtentNSKm = "13.0"))

	static float GetRealmHalfExtentNSCm(float RealmHalfExtentNSKm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm)

	{

		return RealmHalfExtentNSKm * 100000.f;

	}



	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain",
		meta = (CPP_Default_RealmHalfExtentEWKm = "21.034"))

	static float GetRealmHalfExtentEWCm(float RealmHalfExtentEWKm = 21.034f)

	{

		return RealmHalfExtentEWKm * 100000.f;

	}



	/**

	 * Validate proposed island center against tank walls and other islands.

	 * @param OutViolatingOtherIndex Paired island index when lane gap fails.

	 */

	UFUNCTION(BlueprintPure, Category = "Invisible Hand|Terrain",

		meta = (CPP_Default_RealmHalfExtentNSKm = "13.0", CPP_Default_RealmHalfExtentEWKm = "0.0",

			CPP_Default_WallCollisionRadiusFactor = "0.0"))

	static bool ValidateIslandCenterPlacementCm(

		int32 MovingIslandIndex,

		const FVector2D& ProposedCenterCm,

		float MovingSemiMajorCm,

		const TArray<FVector2D>& AllCentersCm,

		const TArray<float>& AllSemiMajorCm,

		int32& OutViolatingOtherIndex,

		const TArray<int32>& IslandIndices,

		float RealmHalfExtentNSKm = 13.f,

		float RealmHalfExtentEWKm = 0.f,

		float WallCollisionRadiusFactor = 0.f);

};


