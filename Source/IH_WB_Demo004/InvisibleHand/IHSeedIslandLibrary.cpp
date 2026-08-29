// Copyright Epic Games, Inc. All Rights Reserved.



#include "IHSeedIslandLibrary.h"

#include "IHInvisibleHandDesignSpec.h"

#include "Math/UnrealMathUtility.h"

#include "Math/RandomStream.h"



namespace IHSeedIslandLibPrivate

{

	static constexpr float GoldenRatio = 1.6180339887f;

	/** Drag-time lane validation — conservative jagged-coast envelope. */
	static constexpr float CoastCollisionRadiusFactor = 1.1f;
	/** Seed layout spacing — max coast extent over semi-major so waterline meshes do not merge in PIE. */
	static constexpr float LayoutCollisionRadiusFactor = IHInvisibleHandSpec::LayoutCollisionRadiusFactor;
	static constexpr float BaseLaneKm = 0.5f;
	/** Never crush below this (legacy post-pack hit ~0.04); 0.50 keeps islands visible if solver hits floor. */
	static constexpr float AbsoluteMinLaneFitScale = 0.50f;
	/** Lower bound for 3-island lane-fit binary search; typical result ~0.55 at 50% budget (not a post-scale clamp). */
	static constexpr float ThreeIslandMinLaneFitScaleFloor = 0.50f;

	static float CoastLayoutExtentKmFromArea(float AreaKm2)
	{
		return FMath::Sqrt(FMath::Max(AreaKm2, 0.05f) / PI) * 1.22f;
	}

	static float ComputeMinCenterDistanceKm(float SemiMajorKmA, float SemiMajorKmB)
	{
		return BaseLaneKm + LayoutCollisionRadiusFactor * (SemiMajorKmA + SemiMajorKmB);
	}

	static float ComputeMinCenterDistanceFromExtentsKm(float ExtentKmA, float ExtentKmB)
	{
		return BaseLaneKm + ExtentKmA + ExtentKmB;
	}

	static bool PairsFitAtExtentsKm(
		const TArray<FVector2D>& CentersKm,
		const TArray<float>& LayoutExtentKm,
		float MinLaneKm = 0.5f)
	{
		if (CentersKm.Num() != LayoutExtentKm.Num())
		{
			return false;
		}
		const int32 N = CentersKm.Num();
		for (int32 i = 0; i < N; ++i)
		{
			for (int32 j = i + 1; j < N; ++j)
			{
				const float Dist = (CentersKm[i] - CentersKm[j]).Size();
				const float Required = ComputeMinCenterDistanceFromExtentsKm(LayoutExtentKm[i], LayoutExtentKm[j]);
				if (Dist + KINDA_SMALL_NUMBER < Required)
				{
					return false;
				}
			}
		}
		return true;
	}

	static bool TryPlaceTwoIslandsMaxSpreadKm(
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		if (LayoutExtentKm.Num() != 2)
		{
			return false;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);
		const float E0 = LayoutExtentKm[0];
		const float E1 = LayoutExtentKm[1];
		if (E0 > UsableHalfWidthKm || E1 > UsableHalfWidthKm || E0 > UsableHalfDepthKm || E1 > UsableHalfDepthKm)
		{
			return false;
		}

		const float Sep = ComputeMinCenterDistanceFromExtentsKm(E0, E1);
		if (Sep > 2.f * UsableHalfWidthKm + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutCentersKm.SetNum(2);
		OutCentersKm[0] = FVector2D(-Sep * 0.5f, 0.f);
		OutCentersKm[1] = FVector2D(Sep * 0.5f, 0.f);
		return PairsFitAtExtentsKm(OutCentersKm, LayoutExtentKm);
	}

	static bool TryPlaceFourIslandsMaxSpreadKm(
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		if (LayoutExtentKm.Num() != 4)
		{
			return false;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);
		for (const float Ext : LayoutExtentKm)
		{
			if (Ext > UsableHalfWidthKm + KINDA_SMALL_NUMBER || Ext > UsableHalfDepthKm + KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}

		OutCentersKm.SetNum(4);
		OutCentersKm[0] = FVector2D(-UsableHalfWidthKm + LayoutExtentKm[0], UsableHalfDepthKm - LayoutExtentKm[0]);
		OutCentersKm[1] = FVector2D(UsableHalfWidthKm - LayoutExtentKm[1], UsableHalfDepthKm - LayoutExtentKm[1]);
		OutCentersKm[2] = FVector2D(-UsableHalfWidthKm + LayoutExtentKm[2], -UsableHalfDepthKm + LayoutExtentKm[2]);
		OutCentersKm[3] = FVector2D(UsableHalfWidthKm - LayoutExtentKm[3], -UsableHalfDepthKm + LayoutExtentKm[3]);
		return PairsFitAtExtentsKm(OutCentersKm, LayoutExtentKm);
	}

	static bool TryPlaceRingIslandsMaxSpreadKm(
		int32 IslandCount,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		if (LayoutExtentKm.Num() != IslandCount || IslandCount < 5)
		{
			return false;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);
		float MaxExtent = 0.f;
		for (const float Ext : LayoutExtentKm)
		{
			MaxExtent = FMath::Max(MaxExtent, Ext);
			if (Ext > UsableHalfWidthKm + KINDA_SMALL_NUMBER || Ext > UsableHalfDepthKm + KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}

		const float RingRadius = FMath::Max(
			FMath::Min(UsableHalfWidthKm, UsableHalfDepthKm) - MaxExtent - 0.05f,
			MaxExtent + 0.5f);
		OutCentersKm.SetNum(IslandCount);
		for (int32 i = 0; i < IslandCount; ++i)
		{
			const float Angle = (2.f * PI * static_cast<float>(i)) / static_cast<float>(IslandCount);
			OutCentersKm[i] = FVector2D(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius);
		}
		return PairsFitAtExtentsKm(OutCentersKm, LayoutExtentKm);
	}

	static bool ComputeMaxSpreadThreeIslandCentersKm(
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		if (LayoutExtentKm.Num() != 3)
		{
			return false;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

		OutCentersKm.SetNum(3);
		OutCentersKm[0] = FVector2D(-UsableHalfWidthKm + LayoutExtentKm[0], UsableHalfDepthKm - LayoutExtentKm[0]);
		OutCentersKm[1] = FVector2D(UsableHalfWidthKm - LayoutExtentKm[1], UsableHalfDepthKm - LayoutExtentKm[1]);
		OutCentersKm[2] = FVector2D(0.f, -UsableHalfDepthKm + LayoutExtentKm[2]);

		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = i + 1; j < 3; ++j)
			{
				const float Dist = (OutCentersKm[i] - OutCentersKm[j]).Size();
				const float Required = ComputeMinCenterDistanceFromExtentsKm(LayoutExtentKm[i], LayoutExtentKm[j]);
				if (Dist + KINDA_SMALL_NUMBER < Required)
				{
					return false;
				}
			}
		}

		return true;
	}

	static void BuildLayoutExtentRadiiKm(const TArray<float>& AreasKm2, TArray<float>& OutLayoutExtentKm)
	{
		OutLayoutExtentKm.Reset();
		OutLayoutExtentKm.Reserve(AreasKm2.Num());
		for (const float AreaKm2 : AreasKm2)
		{
			OutLayoutExtentKm.Add(CoastLayoutExtentKmFromArea(AreaKm2));
		}
	}

	/** Spacing radii for compact placement — omits square-grid coast envelope (√2) used for mesh bounds. */
	static void BuildPlacementSpacingExtentsKm(
		const TArray<float>& LayoutExtentKm,
		TArray<float>& OutSpacingExtentKm)
	{
		OutSpacingExtentKm.Reset(LayoutExtentKm.Num());
		const float InvEnvelope = 1.f / IHInvisibleHandSpec::SquareGridCoastEnvelopeFactor;
		for (const float ExtentKm : LayoutExtentKm)
		{
			OutSpacingExtentKm.Add(FMath::Max(ExtentKm * InvEnvelope, 0.05f));
		}
	}

	static float ClampPatternRadiusKm(
		float DesiredRadiusKm,
		float MaxPairwiseRadiusKm,
		float WallLimitedRadiusKm)
	{
		const float MaxR = FMath::Max(WallLimitedRadiusKm, 0.f);
		const float FeasibleMax = FMath::Min(FMath::Max(MaxPairwiseRadiusKm, 0.f), MaxR);
		return FMath::Clamp(DesiredRadiusKm, 0.f, FeasibleMax);
	}

	/** Max half-span R for symmetric 2×2 grid: (±R,±R) corners, all six pair distances. */
	static float ComputeMaxSymmetricGridHalfSpanKm(
		const TArray<float>& LayoutExtentKm,
		float MinGapKm)
	{
		if (LayoutExtentKm.Num() != 4)
		{
			return 0.f;
		}

		static constexpr float Sqrt2 = 1.414213562f;
		static const int32 PairIndices[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
		static const float PairDistFactors[6] = {2.f, 2.f, 2.f * Sqrt2, 2.f * Sqrt2, 2.f, 2.f};

		float MaxR = MAX_FLT;
		for (int32 PairIdx = 0; PairIdx < 6; ++PairIdx)
		{
			const int32 I = PairIndices[PairIdx][0];
			const int32 J = PairIndices[PairIdx][1];
			const float Required = LayoutExtentKm[I] + LayoutExtentKm[J] + MinGapKm;
			const float Factor = PairDistFactors[PairIdx];
			if (Factor > KINDA_SMALL_NUMBER)
			{
				MaxR = FMath::Min(MaxR, Required / Factor);
			}
		}
		return FMath::Max(MaxR, 0.f);
	}

	static float ComputeUniformLaneFitScale(
		const TArray<FVector2D>& CentersKm,
		const TArray<float>& SemiMajorRadiiKm,
		float MinSeparationKm)
	{
		const int32 N = CentersKm.Num();
		if (N < 2 || SemiMajorRadiiKm.Num() != N)
		{
			return 1.f;
		}

		const float Gap = FMath::Max(MinSeparationKm, 0.f);
		float Scale = 1.f;
		for (int32 i = 0; i < N; ++i)
		{
			for (int32 j = i + 1; j < N; ++j)
			{
				const float D = (CentersKm[i] - CentersKm[j]).Size();
				const float Required = ComputeMinCenterDistanceKm(SemiMajorRadiiKm[i], SemiMajorRadiiKm[j]);
				if (Required <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				if (D <= Gap + KINDA_SMALL_NUMBER)
				{
					Scale = 0.f;
				}
				else
				{
					Scale = FMath::Min(Scale, (D - Gap) / Required);
				}
			}
		}

		return FMath::Clamp(Scale, 0.f, 1.f);
	}

	static void ScaleFalloffRadiiForPairwiseSeparation(const TArray<FVector2D>& CentersKm, TArray<float>& FalloffRadiiKm,

		float MinSeparationKm)

	{

		const int32 N = CentersKm.Num();

		if (N < 2 || FalloffRadiiKm.Num() != N)

		{

			return;

		}

		const float Scale = FMath::Clamp(
			ComputeUniformLaneFitScale(CentersKm, FalloffRadiiKm, MinSeparationKm),
			AbsoluteMinLaneFitScale,
			1.f);

		for (float& R : FalloffRadiiKm)

		{

			R = FMath::Max(R * Scale, 0.5f);

		}

	}



	static FVector2D Rotate2DKm(const FVector2D& V, float AngleRad)

	{

		const float C = FMath::Cos(AngleRad);

		const float S = FMath::Sin(AngleRad);

		return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);

	}



	static void ClampCenterToUsableTankRectKm(FVector2D& CenterKm, float UsableHalfWidthKm, float UsableHalfDepthKm)
	{
		CenterKm.X = FMath::Clamp(CenterKm.X, -UsableHalfWidthKm, UsableHalfWidthKm);
		CenterKm.Y = FMath::Clamp(CenterKm.Y, -UsableHalfDepthKm, UsableHalfDepthKm);
	}



	/** Nudge centers inward when coast extent exceeds wall margin — radii stay at land budget. */
	static void ClampCentersInsideTankWallsKm(
		TArray<FVector2D>& CentersKm,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm)
	{
		const int32 N = CentersKm.Num();
		if (N < 1 || LayoutExtentKm.Num() != N)
		{
			return;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

		for (int32 i = 0; i < N; ++i)
		{
			const float ExtentKm = LayoutExtentKm[i];
			const float InnerHalfWidth = FMath::Max(UsableHalfWidthKm - ExtentKm, 0.05f);
			const float InnerHalfDepth = FMath::Max(UsableHalfDepthKm - ExtentKm, 0.05f);
			ClampCenterToUsableTankRectKm(CentersKm[i], InnerHalfWidth, InnerHalfDepth);
		}
	}

	/** Seeded jittered grid: structured patterns for N=2..7, small jitter, tank-centered. */
	static void ComputeJitteredGridIslandCentersKm(
		int32 IslandCount,
		int32 MasterSeed,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		OutCentersKm.Reset();
		if (IslandCount < 1)
		{
			return;
		}

		OutCentersKm.SetNum(IslandCount);

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);
		static constexpr float PatternFillFactor = 0.85f;
		const float SpanHalfWidthKm = UsableHalfWidthKm * PatternFillFactor;
		const float SpanHalfDepthKm = UsableHalfDepthKm * PatternFillFactor;

		FRandomStream Stream(MasterSeed != 0 ? MasterSeed : 1);
		const float JitterMax = FMath::Min(SpanHalfWidthKm, SpanHalfDepthKm) * 0.025f;
		const float RotationRad = (IslandCount >= 5) ? Stream.FRandRange(-0.12f, 0.12f) : 0.f;

		if (IslandCount == 2)
		{
			OutCentersKm[0] = FVector2D(-SpanHalfWidthKm, 0.f);
			OutCentersKm[1] = FVector2D(SpanHalfWidthKm, 0.f);
		}
		else if (IslandCount == 3)
		{
			// E-W pair + south apex; lane-fit scale applied in ComputeTankIslandLayoutCm.
			OutCentersKm[0] = FVector2D(-SpanHalfWidthKm * 0.95f, SpanHalfDepthKm * 0.12f);
			OutCentersKm[1] = FVector2D(SpanHalfWidthKm * 0.95f, SpanHalfDepthKm * 0.12f);
			OutCentersKm[2] = FVector2D(0.f, -SpanHalfDepthKm * 0.55f);
		}
		else if (IslandCount == 4)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const int32 Col = i % 2;
				const int32 Row = i / 2;
				OutCentersKm[i] = FVector2D(
					(static_cast<float>(Col) - 0.5f) * 2.f * SpanHalfWidthKm,
					(static_cast<float>(Row) - 0.5f) * 2.f * SpanHalfDepthKm);
			}
		}
		else
		{
			const int32 Cols = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(IslandCount))));
			const int32 Rows = FMath::Max(1, FMath::CeilToInt(static_cast<float>(IslandCount) / static_cast<float>(Cols)));
			const float StepX = (Cols > 1) ? (2.f * SpanHalfWidthKm) / static_cast<float>(Cols - 1) : 0.f;
			const float StepY = (Rows > 1) ? (2.f * SpanHalfDepthKm) / static_cast<float>(Rows - 1) : 0.f;
			const float OriginX = -SpanHalfWidthKm;
			const float OriginY = -SpanHalfDepthKm;
			for (int32 i = 0; i < IslandCount; ++i)
			{
				const int32 Row = i / Cols;
				const int32 Col = i % Cols;
				OutCentersKm[i] = FVector2D(OriginX + StepX * static_cast<float>(Col), OriginY + StepY * static_cast<float>(Row));
			}
		}

		for (int32 i = 0; i < IslandCount; ++i)
		{
			FVector2D Jitter(Stream.FRandRange(-JitterMax, JitterMax), Stream.FRandRange(-JitterMax, JitterMax));
			OutCentersKm[i] = Rotate2DKm(OutCentersKm[i] + Jitter, RotationRad);
			ClampCenterToUsableTankRectKm(OutCentersKm[i], UsableHalfWidthKm, UsableHalfDepthKm);
		}
	}

	/** Tighter cluster for layout solve — diamond/ring patterns sized to coast extents. */
	static void ComputeCompactIslandCentersKm(
		int32 IslandCount,
		int32 MasterSeed,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		OutCentersKm.Reset();
		if (IslandCount < 1)
		{
			return;
		}

		OutCentersKm.SetNum(IslandCount);
		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);
		const float Fill = IHInvisibleHandSpec::CompactLayoutPatternFillFactor;
		const float SpanHalfWidthKm = UsableHalfWidthKm * Fill;
		const float SpanHalfDepthKm = UsableHalfDepthKm * Fill;
		const float CompactRadiusKm = FMath::Min(SpanHalfWidthKm, SpanHalfDepthKm);
		const float MinGapKm = 0.5f;

		float MaxExtentKm = 0.05f;
		if (LayoutExtentKm.Num() == IslandCount)
		{
			for (const float Ext : LayoutExtentKm)
			{
				MaxExtentKm = FMath::Max(MaxExtentKm, Ext);
			}
		}

		const float WallLimitedRadius = FMath::Max(
			FMath::Min(UsableHalfWidthKm, UsableHalfDepthKm) - MaxExtentKm - 0.05f,
			0.f);

		FRandomStream Stream(MasterSeed != 0 ? MasterSeed : 1);
		const float RotationRad = (IslandCount >= 4) ? Stream.FRandRange(-0.15f, 0.15f) : 0.f;

		if (IslandCount == 2)
		{
			const float HalfSep = LayoutExtentKm.Num() == 2
				? FMath::Min(
					(SpanHalfWidthKm * 0.72f),
					FMath::Max(
						LayoutExtentKm[0] + LayoutExtentKm[1] + MinGapKm,
						0.5f * WallLimitedRadius))
				: SpanHalfWidthKm * 0.72f;
			OutCentersKm[0] = FVector2D(-HalfSep, 0.f);
			OutCentersKm[1] = FVector2D(HalfSep, 0.f);
		}
		else if (IslandCount == 3)
		{
			OutCentersKm[0] = FVector2D(-SpanHalfWidthKm * 0.78f, SpanHalfDepthKm * 0.08f);
			OutCentersKm[1] = FVector2D(SpanHalfWidthKm * 0.78f, SpanHalfDepthKm * 0.08f);
			OutCentersKm[2] = FVector2D(0.f, -SpanHalfDepthKm * 0.62f);
		}
		else if (IslandCount == 4)
		{
			float GridHalfSpanKm = CompactRadiusKm * 0.55f;
			if (LayoutExtentKm.Num() == 4)
			{
				const float MaxPairSpan = ComputeMaxSymmetricGridHalfSpanKm(LayoutExtentKm, MinGapKm);
				GridHalfSpanKm = FMath::Min(GridHalfSpanKm, MaxPairSpan);
			}
			GridHalfSpanKm = ClampPatternRadiusKm(GridHalfSpanKm, GridHalfSpanKm, WallLimitedRadius);
			OutCentersKm[0] = FVector2D(-GridHalfSpanKm, GridHalfSpanKm);
			OutCentersKm[1] = FVector2D(GridHalfSpanKm, GridHalfSpanKm);
			OutCentersKm[2] = FVector2D(-GridHalfSpanKm, -GridHalfSpanKm);
			OutCentersKm[3] = FVector2D(GridHalfSpanKm, -GridHalfSpanKm);
		}
		else
		{
			const int32 Cols = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(IslandCount))));
			const int32 Rows = FMath::Max(1, FMath::CeilToInt(static_cast<float>(IslandCount) / static_cast<float>(Cols)));
			float GridHalfWidthKm = SpanHalfWidthKm * 0.72f;
			float GridHalfDepthKm = SpanHalfDepthKm * 0.72f;
			GridHalfWidthKm = FMath::Min(GridHalfWidthKm, WallLimitedRadius);
			GridHalfDepthKm = FMath::Min(GridHalfDepthKm, WallLimitedRadius);
			const float StepX = (Cols > 1) ? (2.f * GridHalfWidthKm) / static_cast<float>(Cols - 1) : 0.f;
			const float StepY = (Rows > 1) ? (2.f * GridHalfDepthKm) / static_cast<float>(Rows - 1) : 0.f;
			const float OriginX = -GridHalfWidthKm;
			const float OriginY = -GridHalfDepthKm;
			for (int32 i = 0; i < IslandCount; ++i)
			{
				const int32 Row = i / Cols;
				const int32 Col = i % Cols;
				OutCentersKm[i] = FVector2D(OriginX + StepX * static_cast<float>(Col), OriginY + StepY * static_cast<float>(Row));
			}
		}

		const float JitterMax = CompactRadiusKm * 0.02f;
		for (int32 i = 0; i < IslandCount; ++i)
		{
			FVector2D Jitter(Stream.FRandRange(-JitterMax, JitterMax), Stream.FRandRange(-JitterMax, JitterMax));
			OutCentersKm[i] = Rotate2DKm(OutCentersKm[i] + Jitter, RotationRad);
		}
	}

	/** Largest-first Poisson-disk scatter with hard no-overlap and wall margins. */
	static void ComputeBlueNoiseIslandCentersKm(
		int32 IslandCount,
		int32 MasterSeed,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		float MinLaneKm,
		const TArray<float>& SemiMajorRadiiKm,
		const TArray<FVector2D>& FallbackCentersKm,
		TArray<FVector2D>& OutCentersKm)
	{
		OutCentersKm.SetNumZeroed(IslandCount);
		if (IslandCount < 1)
		{
			return;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

		TArray<int32> Order;
		Order.Reserve(IslandCount);
		for (int32 i = 0; i < IslandCount; ++i)
		{
			Order.Add(i);
		}
		Order.Sort([&SemiMajorRadiiKm](int32 A, int32 B) {
			const float Ra = SemiMajorRadiiKm.IsValidIndex(A) ? SemiMajorRadiiKm[A] : 0.f;
			const float Rb = SemiMajorRadiiKm.IsValidIndex(B) ? SemiMajorRadiiKm[B] : 0.f;
			return Ra > Rb;
		});

		TArray<FVector2D> Placed;
		TArray<float> PlacedRadii;
		Placed.Reserve(IslandCount);
		PlacedRadii.Reserve(IslandCount);

		static constexpr int32 MaxAttemptsPerIsland = 512;

		for (int32 OrderIdx = 0; OrderIdx < IslandCount; ++OrderIdx)
		{
			const int32 IslandIdx = Order[OrderIdx];
			const float RadiusKm = SemiMajorRadiiKm.IsValidIndex(IslandIdx) ? SemiMajorRadiiKm[IslandIdx] : 0.5f;
			const float CollisionR = RadiusKm * LayoutCollisionRadiusFactor;

			FRandomStream Stream((MasterSeed != 0 ? MasterSeed : 1) + IslandIdx * 7919 + 31337);
			FVector2D BestPos = FVector2D::ZeroVector;
			float BestScore = -1.f;
			bool bFound = false;

			const float InnerHalfWidth = FMath::Max(UsableHalfWidthKm - CollisionR, 0.05f);
			const float InnerHalfDepth = FMath::Max(UsableHalfDepthKm - CollisionR, 0.05f);
			for (int32 Attempt = 0; Attempt < MaxAttemptsPerIsland; ++Attempt)
			{
				const FVector2D Candidate(
					Stream.FRandRange(-InnerHalfWidth, InnerHalfWidth),
					Stream.FRandRange(-InnerHalfDepth, InnerHalfDepth));

				bool bOverlap = false;
				float MinNeighborGap = MAX_FLT;
				for (int32 P = 0; P < Placed.Num(); ++P)
				{
					const float Dist = (Candidate - Placed[P]).Size();
					const float Required = ComputeMinCenterDistanceKm(RadiusKm, PlacedRadii[P]);
					if (Dist < Required - KINDA_SMALL_NUMBER)
					{
						bOverlap = true;
						break;
					}
					MinNeighborGap = FMath::Min(MinNeighborGap, Dist - Required);
				}
				if (bOverlap)
				{
					continue;
				}

				const float WallClear = FMath::Min(
					UsableHalfWidthKm - FMath::Abs(Candidate.X) - CollisionR,
					UsableHalfDepthKm - FMath::Abs(Candidate.Y) - CollisionR);
				const float Score = (Placed.Num() > 0) ? FMath::Min(MinNeighborGap, WallClear) : WallClear;
				if (Score > BestScore)
				{
					BestScore = Score;
					BestPos = Candidate;
					bFound = true;
				}
			}

			if (!bFound)
			{
				// Keep spread when Poisson fails — random center cluster caused post-layout scale → 0.04.
				BestPos = FallbackCentersKm.IsValidIndex(IslandIdx)
					? FallbackCentersKm[IslandIdx]
					: FVector2D::ZeroVector;
				ClampCenterToUsableTankRectKm(BestPos, InnerHalfWidth, InnerHalfDepth);
			}

			OutCentersKm[IslandIdx] = BestPos;
			Placed.Add(BestPos);
			PlacedRadii.Add(RadiusKm);
		}
	}

	/** Blend jittered grid (0) with blue-noise scatter (1). */
	static void ComputeOrganicIslandCentersKm(
		int32 IslandCount,
		int32 MasterSeed,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		float MinLaneKm,
		float PlacementScatter,
		const TArray<float>& SemiMajorRadiiKm,
		TArray<FVector2D>& OutCentersKm)
	{
		OutCentersKm.Reset();
		if (IslandCount < 1)
		{
			return;
		}

		const float Scatter = FMath::Clamp(PlacementScatter, 0.f, 1.f);
		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

		TArray<FVector2D> GridCenters;
		TArray<FVector2D> BlueNoiseCenters;
		ComputeJitteredGridIslandCentersKm(
			IslandCount, MasterSeed, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, GridCenters);
		ComputeBlueNoiseIslandCentersKm(
			IslandCount, MasterSeed, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, MinLaneKm, SemiMajorRadiiKm,
			GridCenters, BlueNoiseCenters);

		OutCentersKm.SetNum(IslandCount);
		for (int32 i = 0; i < IslandCount; ++i)
		{
			OutCentersKm[i] = FMath::Lerp(GridCenters[i], BlueNoiseCenters[i], Scatter);
			ClampCenterToUsableTankRectKm(OutCentersKm[i], UsableHalfWidthKm, UsableHalfDepthKm);
		}
	}

	/** Push overlapping centers apart using coast layout extents (matches built height-grid footprint). */
	static void EnforceCenterLaneSeparationKm(
		TArray<FVector2D>& CentersKm,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm)
	{
		const int32 N = CentersKm.Num();
		if (N < 2 || LayoutExtentKm.Num() != N)
		{
			return;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

		auto ClampCenterForIsland = [&](int32 Index)
		{
			const float ExtentKm = LayoutExtentKm[Index];
			const float InnerHalfWidth = FMath::Max(UsableHalfWidthKm - ExtentKm, 0.05f);
			const float InnerHalfDepth = FMath::Max(UsableHalfDepthKm - ExtentKm, 0.05f);
			ClampCenterToUsableTankRectKm(CentersKm[Index], InnerHalfWidth, InnerHalfDepth);
		};

		static constexpr int32 MaxIterations = 48;
		for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
		{
			bool bMoved = false;
			for (int32 i = 0; i < N; ++i)
			{
				for (int32 j = i + 1; j < N; ++j)
				{
					const float Required = ComputeMinCenterDistanceFromExtentsKm(LayoutExtentKm[i], LayoutExtentKm[j]);
					const FVector2D Delta = CentersKm[j] - CentersKm[i];
					const float Dist = Delta.Size();
					if (Dist + KINDA_SMALL_NUMBER >= Required)
					{
						continue;
					}

					FVector2D PushDir = Delta;
					if (Dist <= KINDA_SMALL_NUMBER)
					{
						PushDir = (j > i) ? FVector2D(1.f, 0.f) : FVector2D(-1.f, 0.f);
					}
					else
					{
						PushDir /= Dist;
					}

					const float PushDist = (Required - Dist) * 0.5f + 0.001f;
					CentersKm[i] -= PushDir * PushDist;
					CentersKm[j] += PushDir * PushDist;
					ClampCenterForIsland(i);
					ClampCenterForIsland(j);
					bMoved = true;
				}
			}

			if (!bMoved)
			{
				break;
			}
		}
	}

	static bool FinalizeCompactPlacementKm(
		TArray<FVector2D>& CentersKm,
		const TArray<float>& SpacingExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm)
	{
		ClampCentersInsideTankWallsKm(
			CentersKm, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		EnforceCenterLaneSeparationKm(
			CentersKm, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		ClampCentersInsideTankWallsKm(
			CentersKm, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		return PairsFitAtExtentsKm(CentersKm, SpacingExtentKm);
	}

	static bool TryCompactPlacementAtClusterScale(
		const TArray<FVector2D>& PatternCentersKm,
		float ClusterScale,
		const TArray<float>& SpacingExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		if (PatternCentersKm.Num() != SpacingExtentKm.Num() || PatternCentersKm.Num() < 1)
		{
			return false;
		}

		const float Scale = FMath::Clamp(ClusterScale, 0.f, 1.f);
		OutCentersKm.SetNum(PatternCentersKm.Num());
		for (int32 i = 0; i < PatternCentersKm.Num(); ++i)
		{
			OutCentersKm[i] = PatternCentersKm[i] * Scale;
		}
		return FinalizeCompactPlacementKm(
			OutCentersKm, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
	}

	static float BinarySearchMaxClusterScaleForCompactKm(
		const TArray<FVector2D>& PatternCentersKm,
		const TArray<float>& SpacingExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutBestCentersKm)
	{
		OutBestCentersKm.Reset();
		if (PatternCentersKm.Num() != SpacingExtentKm.Num() || PatternCentersKm.Num() < 1)
		{
			return 0.f;
		}

		float Lo = 0.25f;
		float Hi = 1.f;
		float BestScale = 0.f;
		TArray<FVector2D> BestCentersKm;

		for (int32 Iter = 0; Iter < 16; ++Iter)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			TArray<FVector2D> TrialCentersKm;
			if (TryCompactPlacementAtClusterScale(
				PatternCentersKm, Mid, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
				TrialCentersKm))
			{
				BestScale = Mid;
				BestCentersKm = TrialCentersKm;
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}

		OutBestCentersKm = BestCentersKm;
		return BestScale;
	}

	static void EnforceCenterLaneSeparationFromSemiMajorKm(
		TArray<FVector2D>& CentersKm,
		const TArray<float>& SemiMajorRadiiKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm)
	{
		TArray<float> LayoutExtentKm;
		LayoutExtentKm.Reserve(SemiMajorRadiiKm.Num());
		for (const float SemiMajorKm : SemiMajorRadiiKm)
		{
			LayoutExtentKm.Add(SemiMajorKm * LayoutCollisionRadiusFactor);
		}
		EnforceCenterLaneSeparationKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
	}

	/** Analytical E-W pair + south apex; satisfies 500 m + 1.0× semi-major lane rule when feasible. */
	static bool SolveLaneAwareThreeIslandCentersKm(
		const TArray<float>& SemiMajorRadiiKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		if (SemiMajorRadiiKm.Num() != 3)
		{
			return false;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

		const float R0 = SemiMajorRadiiKm[0];
		const float R1 = SemiMajorRadiiKm[1];
		const float R2 = SemiMajorRadiiKm[2];

		const float ReqEW = ComputeMinCenterDistanceKm(R0, R1);
		const float HalfEWMax = FMath::Min(
			UsableHalfWidthKm - R0 * LayoutCollisionRadiusFactor,
			UsableHalfWidthKm - R1 * LayoutCollisionRadiusFactor);
		if (HalfEWMax <= 0.05f)
		{
			return false;
		}

		const float HalfEWNeeded = ReqEW * 0.5f;
		if (HalfEWMax + KINDA_SMALL_NUMBER < HalfEWNeeded)
		{
			return false;
		}
		const float HalfEW = FMath::Min(HalfEWMax, HalfEWNeeded);
		const float ReqDiag = FMath::Max(
			ComputeMinCenterDistanceKm(R0, R2),
			ComputeMinCenterDistanceKm(R1, R2));
		const float SouthMax = FMath::Max(UsableHalfDepthKm - R2 * LayoutCollisionRadiusFactor, 0.05f);
		const float MinSouth = FMath::Sqrt(FMath::Max(ReqDiag * ReqDiag - HalfEW * HalfEW, 0.f));
		if (MinSouth > SouthMax + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float SouthOffset = FMath::Clamp(FMath::Max(MinSouth * 1.005f, SouthMax * 0.85f), 0.05f, SouthMax);
		const float NorthBias = FMath::Min(UsableHalfDepthKm * 0.06f, 0.25f);

		OutCentersKm.SetNum(3);
		OutCentersKm[0] = FVector2D(-HalfEW, NorthBias);
		OutCentersKm[1] = FVector2D(HalfEW, NorthBias);
		OutCentersKm[2] = FVector2D(0.f, -SouthOffset);

		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = i + 1; j < 3; ++j)
			{
				const float Dist = (OutCentersKm[i] - OutCentersKm[j]).Size();
				const float Required = ComputeMinCenterDistanceKm(SemiMajorRadiiKm[i], SemiMajorRadiiKm[j]);
				if (Dist + KINDA_SMALL_NUMBER < Required)
				{
					return false;
				}
			}
		}

		return true;
	}

	static bool DispatchMaxSpreadPlacementKm(
		int32 IslandCount,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		switch (IslandCount)
		{
		case 2:
			return TryPlaceTwoIslandsMaxSpreadKm(LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
		case 3:
			return ComputeMaxSpreadThreeIslandCentersKm(
				LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
		case 4:
			return TryPlaceFourIslandsMaxSpreadKm(LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
		default:
			if (IslandCount >= 5 && IslandCount <= 7)
			{
				return TryPlaceRingIslandsMaxSpreadKm(
					IslandCount, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
			}
			return false;
		}
	}

	static bool TryPlaceIslandsCompactFromSpacingKm(
		int32 IslandCount,
		int32 MasterSeed,
		const TArray<float>& SpacingExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm,
		bool& bOutUsedMaxSpreadFallback)
	{
		bOutUsedMaxSpreadFallback = false;
		if (SpacingExtentKm.Num() != IslandCount || IslandCount < 1)
		{
			return false;
		}

		TArray<FVector2D> PatternCentersKm;
		ComputeCompactIslandCentersKm(
			IslandCount, MasterSeed, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
			PatternCentersKm);

		if (FinalizeCompactPlacementKm(
			PatternCentersKm, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm))
		{
			OutCentersKm = PatternCentersKm;
			return true;
		}

		TArray<FVector2D> ScaledCentersKm;
		if (BinarySearchMaxClusterScaleForCompactKm(
			PatternCentersKm, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
			ScaledCentersKm) > KINDA_SMALL_NUMBER
			&& ScaledCentersKm.Num() == IslandCount)
		{
			OutCentersKm = ScaledCentersKm;
			return true;
		}

		if (IslandCount == 3)
		{
			TArray<float> SemiMajorKm;
			SemiMajorKm.SetNum(3);
			for (int32 i = 0; i < 3; ++i)
			{
				SemiMajorKm[i] = FMath::Max(
					SpacingExtentKm[i] / LayoutCollisionRadiusFactor,
					0.05f);
			}
			TArray<FVector2D> SolverCentersKm;
			if (SolveLaneAwareThreeIslandCentersKm(
				SemiMajorKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, SolverCentersKm)
				&& PairsFitAtExtentsKm(SolverCentersKm, SpacingExtentKm))
			{
				OutCentersKm = SolverCentersKm;
				return true;
			}
		}

		bOutUsedMaxSpreadFallback = DispatchMaxSpreadPlacementKm(
			IslandCount, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
		return bOutUsedMaxSpreadFallback;
	}

	static bool TryPlaceIslandsCompactKm(
		int32 IslandCount,
		int32 MasterSeed,
		const TArray<float>& LayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm,
		bool& bOutUsedMaxSpreadFallback)
	{
		TArray<float> SpacingExtentKm;
		BuildPlacementSpacingExtentsKm(LayoutExtentKm, SpacingExtentKm);
		return TryPlaceIslandsCompactFromSpacingKm(
			IslandCount, MasterSeed, SpacingExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
			OutCentersKm, bOutUsedMaxSpreadFallback);
	}

	/** Max uniform scale on budget layout extents so max-spread triangle fits the tank. */
	static float BinarySearchMaxLaneFitScaleForThreeIslandsFromExtents(
		const TArray<float>& BudgetLayoutExtentKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		OutCentersKm.Reset();
		if (BudgetLayoutExtentKm.Num() != 3)
		{
			return 1.f;
		}

		float Lo = ThreeIslandMinLaneFitScaleFloor;
		float Hi = 1.f;
		float BestScale = ThreeIslandMinLaneFitScaleFloor;
		TArray<FVector2D> BestCentersKm;

		for (int32 Iter = 0; Iter < 24; ++Iter)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			TArray<float> ScaledExtentKm;
			ScaledExtentKm.SetNum(3);
			for (int32 i = 0; i < 3; ++i)
			{
				ScaledExtentKm[i] = FMath::Max(BudgetLayoutExtentKm[i] * Mid, 0.05f);
			}

			TArray<FVector2D> CentersKm;
			if (ComputeMaxSpreadThreeIslandCentersKm(
				ScaledExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm))
			{
				BestScale = Mid;
				BestCentersKm = CentersKm;
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}

		OutCentersKm = BestCentersKm;
		return BestScale;
	}

	/** Max uniform scale on budget radii so lane-aware triangle placement fits the tank. */
	static float BinarySearchMaxLaneFitScaleForThreeIslands(
		const TArray<float>& BudgetSemiMajorRadiiKm,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm,
		TArray<FVector2D>& OutCentersKm)
	{
		OutCentersKm.Reset();
		if (BudgetSemiMajorRadiiKm.Num() != 3)
		{
			return 1.f;
		}

		float Lo = ThreeIslandMinLaneFitScaleFloor;
		float Hi = 1.f;
		float BestScale = ThreeIslandMinLaneFitScaleFloor;
		TArray<FVector2D> BestCentersKm;

		for (int32 Iter = 0; Iter < 24; ++Iter)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			TArray<float> ScaledRadiiKm;
			ScaledRadiiKm.SetNum(3);
			for (int32 i = 0; i < 3; ++i)
			{
				ScaledRadiiKm[i] = BudgetSemiMajorRadiiKm[i] * Mid;
			}

			TArray<FVector2D> CentersKm;
			if (SolveLaneAwareThreeIslandCentersKm(
				ScaledRadiiKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm))
			{
				BestScale = Mid;
				BestCentersKm = CentersKm;
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}

		OutCentersKm = BestCentersKm;
		return BestScale;
	}

	static void ApplyBoundedCenterJitterKm(
		TArray<FVector2D>& CentersKm,
		const TArray<float>& SemiMajorRadiiKm,
		int32 MasterSeed,
		float JitterStrength,
		float RealmHalfExtentEWKm,
		float RealmHalfExtentNSKm,
		float WallMarginKm)
	{
		const int32 N = CentersKm.Num();
		if (N < 1 || SemiMajorRadiiKm.Num() != N || JitterStrength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
		const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);
		FRandomStream Stream(MasterSeed != 0 ? MasterSeed : 1);
		const float JitterMax = FMath::Min(UsableHalfWidthKm, UsableHalfDepthKm) * 0.02f * JitterStrength;

		for (int32 i = 0; i < N; ++i)
		{
			const float CollisionR = SemiMajorRadiiKm[i] * LayoutCollisionRadiusFactor;
			const float InnerHalfWidth = FMath::Max(UsableHalfWidthKm - CollisionR, 0.05f);
			const float InnerHalfDepth = FMath::Max(UsableHalfDepthKm - CollisionR, 0.05f);
			CentersKm[i] += FVector2D(Stream.FRandRange(-JitterMax, JitterMax), Stream.FRandRange(-JitterMax, JitterMax));
			ClampCenterToUsableTankRectKm(CentersKm[i], InnerHalfWidth, InnerHalfDepth);
		}
	}

}



bool UIHSeedIslandLibrary::IsLandformCountInDesignRange(int32 IslandCount)

{

	return IslandCount >= IHInvisibleHandSpec::LandformCountMin && IslandCount <= IHInvisibleHandSpec::LandformCountMax;

}



void UIHSeedIslandLibrary::GetFibonacciAreaWeights(int32 IslandCount, TArray<float>& OutWeightsNormalized)

{

	OutWeightsNormalized.Reset();

	if (!IsLandformCountInDesignRange(IslandCount))

	{

		return;

	}

	const float TotalS = static_cast<float>(IHInvisibleHandSpec::CanonicalTotalDrySectors);

	if (TotalS <= KINDA_SMALL_NUMBER)

	{

		return;

	}

	for (int32 i = 0; i < IslandCount; ++i)

	{

		const float S = static_cast<float>(IHInvisibleHandSpec::GetCanonicalIslandDrySectors(IslandCount, i));

		OutWeightsNormalized.Add(S / TotalS);

	}

}



void UIHSeedIslandLibrary::ComputeIslandAreasKM2(int32 IslandCount, float TotalLandAreaKM2, TArray<float>& OutAreasKM2)

{

	OutAreasKM2.Reset();

	if (!IsLandformCountInDesignRange(IslandCount) || TotalLandAreaKM2 <= KINDA_SMALL_NUMBER)

	{

		return;

	}



	TArray<float> Weights;

	GetFibonacciAreaWeights(IslandCount, Weights);

	for (const float Weight : Weights)

	{

		OutAreasKM2.Add(Weight * TotalLandAreaKM2);

	}

}



float UIHSeedIslandLibrary::ComputeRealmHalfExtentEWKmFromNS(float RealmHalfExtentNSKm)
{
	return RealmHalfExtentNSKm * static_cast<float>(IHInvisibleHandSpec::GoldenRatioPhi);
}

float UIHSeedIslandLibrary::ComputeRealmAreaKm2(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm)
{
	return (2.f * RealmHalfExtentNSKm) * (2.f * RealmHalfExtentEWKm);
}

float UIHSeedIslandLibrary::ComputeTotalLandKm2(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm, float DevLandAreaFraction)
{
	return ComputeRealmAreaKm2(RealmHalfExtentNSKm, RealmHalfExtentEWKm)
		* FMath::Clamp(DevLandAreaFraction, 0.01f, 1.f);
}

int32 UIHSeedIslandLibrary::ComputeTotalLandAcres(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm, float DevLandAreaFraction)
{
	const float TotalLandKm2 = ComputeTotalLandKm2(RealmHalfExtentNSKm, RealmHalfExtentEWKm, DevLandAreaFraction);
	const double Acres = static_cast<double>(TotalLandKm2) * 1000000.0 / IHInvisibleHandSpec::InternationalAcreSquareMeters;
	return FMath::RoundToInt(static_cast<float>(Acres));
}

void UIHSeedIslandLibrary::ComputeAcresBudgets(int32 IslandCount, int32 TotalLandAcres, TArray<int32>& OutAcres)
{
	OutAcres.Reset();
	if (IslandCount <= 0 || TotalLandAcres <= 0)
	{
		return;
	}

	TArray<float> Weights;
	GetFibonacciAreaWeights(IslandCount, Weights);
	if (Weights.Num() != IslandCount)
	{
		OutAcres.Init(TotalLandAcres / IslandCount, IslandCount);
		const int32 Remainder = TotalLandAcres - OutAcres[0] * IslandCount;
		for (int32 i = 0; i < Remainder; ++i)
		{
			OutAcres[i]++;
		}
		return;
	}

	TArray<int32> Floors;
	TArray<float> Fractions;
	int32 FloorSum = 0;
	for (int32 i = 0; i < IslandCount; ++i)
	{
		const float Raw = Weights[i] * static_cast<float>(TotalLandAcres);
		const int32 Floor = FMath::FloorToInt(Raw);
		Floors.Add(Floor);
		Fractions.Add(Raw - static_cast<float>(Floor));
		FloorSum += Floor;
	}

	OutAcres = Floors;
	int32 Remaining = TotalLandAcres - FloorSum;
	TArray<int32> Order;
	Order.Reserve(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		Order.Add(i);
	}
	Order.Sort([&Fractions](int32 A, int32 B) { return Fractions[A] > Fractions[B]; });

	for (int32 i = 0; i < Remaining && Order.IsValidIndex(i); ++i)
	{
		OutAcres[Order[i]]++;
	}
}

void UIHSeedIslandLibrary::ComputeIslandAreasKM2FromRealm(
	float RealmHalfExtentNSKm, float RealmHalfExtentEWKm, int32 IslandCount, TArray<float>& OutAreasKM2, float DevLandAreaFraction)
{
	OutAreasKM2.Reset();
	if (!IsLandformCountInDesignRange(IslandCount))
	{
		return;
	}

	if (RealmHalfExtentEWKm <= KINDA_SMALL_NUMBER)
	{
		RealmHalfExtentEWKm = ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	}

	const int32 TotalAcres = ComputeTotalLandAcres(RealmHalfExtentNSKm, RealmHalfExtentEWKm, DevLandAreaFraction);
	TArray<int32> AcresBudgets;
	ComputeAcresBudgets(IslandCount, TotalAcres, AcresBudgets);

	const float Km2PerAcre = static_cast<float>(
		IHInvisibleHandSpec::InternationalAcreSquareMeters / 1000000.0);
	for (const int32 Acres : AcresBudgets)
	{
		OutAreasKM2.Add(static_cast<float>(Acres) * Km2PerAcre);
	}
}



float UIHSeedIslandLibrary::SemiMajorKmFromWaterlineAreaKm2(float AreaKm2)

{

	if (AreaKm2 <= KINDA_SMALL_NUMBER)

	{

		return 0.f;

	}

	return FMath::Sqrt(AreaKm2 * IHSeedIslandLibPrivate::GoldenRatio / PI);

}

float UIHSeedIslandLibrary::ComputeMinIslandCenterDistanceCm(float SemiMajorCmA, float SemiMajorCmB)
{
	static constexpr float BaseLaneCm = 50000.f;
	// Drag-time spacing: waterline radii + 500 m lane (layout seed uses 1.48× envelope separately).
	static constexpr float DragCollisionRadiusFactor = 1.0f;
	return BaseLaneCm + DragCollisionRadiusFactor * (SemiMajorCmA + SemiMajorCmB);
}

bool UIHSeedIslandLibrary::ValidateIslandCenterPlacementCm(
	int32 MovingIslandIndex,
	const FVector2D& ProposedCenterCm,
	float MovingSemiMajorCm,
	const TArray<FVector2D>& AllCentersCm,
	const TArray<float>& AllSemiMajorCm,
	int32& OutViolatingOtherIndex,
	const TArray<int32>& IslandIndices,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm,
	float WallCollisionRadiusFactor)
{
	OutViolatingOtherIndex = INDEX_NONE;

	if (RealmHalfExtentEWKm <= KINDA_SMALL_NUMBER)
	{
		RealmHalfExtentEWKm = ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	}

	const float RealmHalfExtentEWCm = GetRealmHalfExtentEWCm(RealmHalfExtentEWKm);
	const float RealmHalfExtentNSCm = GetRealmHalfExtentNSCm(RealmHalfExtentNSKm);
	const float WallMarginCm = GetRealmEdgeMarginCm();
	const float UsableHalfWidthCm = FMath::Max(RealmHalfExtentEWCm - WallMarginCm, 1.f);
	const float UsableHalfDepthCm = FMath::Max(RealmHalfExtentNSCm - WallMarginCm, 1.f);
	const float WallFactor = WallCollisionRadiusFactor > KINDA_SMALL_NUMBER
		? WallCollisionRadiusFactor
		: IHSeedIslandLibPrivate::LayoutCollisionRadiusFactor;
	const float CollisionR = MovingSemiMajorCm * WallFactor;

	if (FMath::Abs(ProposedCenterCm.X) + CollisionR > UsableHalfWidthCm + KINDA_SMALL_NUMBER
		|| FMath::Abs(ProposedCenterCm.Y) + CollisionR > UsableHalfDepthCm + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	for (int32 OtherIdx = 0; OtherIdx < AllCentersCm.Num(); ++OtherIdx)
	{
		const int32 OtherIslandIndex = IslandIndices.IsValidIndex(OtherIdx)
			? IslandIndices[OtherIdx]
			: OtherIdx;
		if (OtherIslandIndex == MovingIslandIndex)
		{
			continue;
		}

		const float OtherSemiMajor = AllSemiMajorCm.IsValidIndex(OtherIdx) ? AllSemiMajorCm[OtherIdx] : 0.f;
		const float RequiredDist = ComputeMinIslandCenterDistanceCm(MovingSemiMajorCm, OtherSemiMajor);
		const float ActualDist = FVector2D::Distance(ProposedCenterCm, AllCentersCm[OtherIdx]);
		if (ActualDist + KINDA_SMALL_NUMBER < RequiredDist)
		{
			OutViolatingOtherIndex = OtherIslandIndex;
			return false;
		}
	}

	return true;
}



int32 UIHSeedIslandLibrary::DerivePerlinNoiseSeed(int32 MasterSeedFromEightChar, int32 IslandIndex, EIHNoiseLayer Layer)

{

	switch (Layer)

	{

	case EIHNoiseLayer::CoastlineJaggedness:

		return MasterSeedFromEightChar + IslandIndex * 9176;

	default:

		return MasterSeedFromEightChar;

	}

}



float UIHSeedIslandLibrary::PerlinNoise2DSeeded(float X, float Y, int32 NoiseSeed)

{

	const float Ox = static_cast<float>(NoiseSeed & 0xFFFF) * 0.01f;

	const float Oy = static_cast<float>((NoiseSeed >> 16) & 0xFFFF) * 0.01f;

	return FMath::PerlinNoise2D(FVector2D(X + Ox, Y + Oy));

}



void UIHSeedIslandLibrary::DomainWarpOffset2DKm(float WorldXKm, float WorldYKm, int32 WarpNoiseSeed,

	float WarpFrequencyScale, float WarpStrengthKm, float& OutDeltaXKm, float& OutDeltaYKm)

{

	const float Wx = WorldXKm * WarpFrequencyScale;

	const float Wy = WorldYKm * WarpFrequencyScale;

	const float Nx = PerlinNoise2DSeeded(Wx, Wy, WarpNoiseSeed);

	const float Ny = PerlinNoise2DSeeded(Wx + 37.17f, Wy + 91.41f, WarpNoiseSeed + 911);

	OutDeltaXKm = Nx * WarpStrengthKm;

	OutDeltaYKm = Ny * WarpStrengthKm;

}



void UIHSeedIslandLibrary::ComputeDefaultIslandCentersAndRadiiKm(int32 IslandCount, float WorldWidthKm, float WorldHeightKm,

	float MarginKm, int32 MasterSeed, const TArray<float>& IslandAreasKm2, float RadiusFromAreaScale,

	TArray<FVector2D>& OutCentersKm, TArray<float>& OutFalloffRadiiKm, float MinSeparationKm)

{

	(void)MasterSeed;

	OutCentersKm.Reset();

	OutFalloffRadiiKm.Reset();

	if (IslandCount < 1 || WorldWidthKm <= 2.f * MarginKm || WorldHeightKm <= 2.f * MarginKm)

	{

		return;

	}

	const float UsableW = WorldWidthKm - 2.f * MarginKm;

	const float UsableH = WorldHeightKm - 2.f * MarginKm;



	OutCentersKm.Reserve(IslandCount);

	OutFalloffRadiiKm.Reserve(IslandCount);



	const bool bThreeColumnStrip = (IslandCount == 3);



	int32 Cols = 1;

	int32 Rows = 1;

	float StepX = UsableW;

	float StepY = UsableH;

	if (!bThreeColumnStrip)

	{

		Cols = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(IslandCount)));

		Rows = FMath::Max(1, FMath::CeilToInt(static_cast<float>(IslandCount) / static_cast<float>(Cols)));

		StepX = UsableW / static_cast<float>(Cols + 1);

		StepY = UsableH / static_cast<float>(Rows + 1);

	}



	for (int32 SlotIdx = 0; SlotIdx < IslandCount; ++SlotIdx)

	{

		float Cx = 0.f;

		float Cy = 0.f;

		if (bThreeColumnStrip)

		{

			Cx = MarginKm + UsableW * (static_cast<float>(SlotIdx) + 0.5f) / 3.f;

			Cy = MarginKm + UsableH * 0.5f;

		}

		else

		{

			const int32 Row = SlotIdx / Cols;

			const int32 Col = SlotIdx % Cols;

			Cx = MarginKm + StepX * static_cast<float>(Col + 1);

			Cy = MarginKm + StepY * static_cast<float>(Row + 1);

		}

		OutCentersKm.Add(FVector2D(Cx, Cy));



		float Area = IslandAreasKm2.IsValidIndex(SlotIdx) ? IslandAreasKm2[SlotIdx] : 0.f;

		if (Area < KINDA_SMALL_NUMBER)

		{

			Area = 1.f;

		}

		const float SemiMajorKm = SemiMajorKmFromWaterlineAreaKm2(Area);

		OutFalloffRadiiKm.Add(FMath::Max(SemiMajorKm, 0.05f));

	}



	IHSeedIslandLibPrivate::ScaleFalloffRadiiForPairwiseSeparation(OutCentersKm, OutFalloffRadiiKm, MinSeparationKm);

}



void UIHSeedIslandLibrary::ComputeSummitTopZCmForAreas(
	const TArray<float>& AreasKm2, TArray<float>& OutSummitTopZCm)
{
	OutSummitTopZCm.Reset();
	if (AreasKm2.Num() < 1)
	{
		return;
	}

	// IH-DEC-052: replaced the old realm-relative-rank formula (30m smallest -> 180m largest,
	// scaled by AreaT = this island's area rank *within the current realm only*) with one keyed
	// to each island's own absolute footprint diameter — the old formula meant the biggest island
	// in a tiny 3-island dev realm and the biggest island in a future 512,000-acre realm both got
	// pushed toward the same 180m ceiling, which is exactly backwards for a DT meant to eventually
	// fit much larger islands. This computation is now fully independent per island — no
	// realm-wide min/max pass needed.
	//
	// ApexMeters = DiameterMeters / phi^HeightExponent, capped at the canonical 2400m ceiling
	// (MountainApexMeters, IHInvisibleHandDesignSpec.h — validated in
	// Topography_Elevation_Chart_Comprehensive_Recommendations.md, never previously wired to any
	// live formula). HeightExponent=6.367 is calibrated so the 512,000-acre "River Prototype" gate
	// (IH-DEC-026, ~2072 km^2, ~51.4km diameter) lands almost exactly at 2400m — real large
	// volcanic ocean islands (La Palma ~47km diameter -> 2426m; Maui ~77km -> 3055m) sit in a
	// comparable apex-to-diameter band, so this is a plausible real-world ratio, not an arbitrary
	// number fit to one target alone. At this project's current ~10-20km dev-scale island
	// diameters this lands in the ~370-950m range (vs. the old formula's fixed 30-180m band) —
	// directly addressing the "islands look flat/uniformly tan" observation.
	static constexpr double HeightExponent = 6.367;
	const double SummitDiameterDivisor = FMath::Pow(IHInvisibleHandSpec::GoldenRatioPhi, HeightExponent);

	OutSummitTopZCm.Reserve(AreasKm2.Num());
	for (const float AreaKm2 : AreasKm2)
	{
		const double DiameterMeters = 2.0 * FMath::Sqrt(FMath::Max(0.0, static_cast<double>(AreaKm2)) / PI) * 1000.0;
		const double ApexMeters = FMath::Min(
			DiameterMeters / SummitDiameterDivisor,
			static_cast<double>(IHInvisibleHandSpec::MountainApexMeters));
		OutSummitTopZCm.Add(static_cast<float>(ApexMeters * 100.0));
	}
}

namespace IHSeedIslandLayoutPrivate
{
static void ComputeTankIslandLayoutCmImpl(
	int32 IslandCount,
	int32 MasterSeed,
	TArray<FVector2D>& OutCentersWorldCm,
	TArray<float>& OutSemiMajorCm,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm,
	float PlacementScatter,
	float IslandSizeMultiplier,
	float DevLandAreaFraction,
	const TArray<float>* PerIslandLayoutExtentKm)
{
	OutCentersWorldCm.Reset();
	OutSemiMajorCm.Reset();

	IslandCount = FMath::Clamp(IslandCount, IHInvisibleHandSpec::LandformCountMin, IHInvisibleHandSpec::LandformCountMax);
	RealmHalfExtentNSKm = FMath::Max(RealmHalfExtentNSKm, 0.5f);
	if (RealmHalfExtentEWKm <= KINDA_SMALL_NUMBER)
	{
		RealmHalfExtentEWKm = UIHSeedIslandLibrary::ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	}

	static constexpr float WallMarginKm = 0.875f;
	static constexpr float MinLaneKm = 0.5f;

	TArray<float> AreasKm2;
	UIHSeedIslandLibrary::ComputeIslandAreasKM2FromRealm(
		RealmHalfExtentNSKm, RealmHalfExtentEWKm, IslandCount, AreasKm2, DevLandAreaFraction);
	if (AreasKm2.Num() != IslandCount)
	{
		const float TotalLandKm2 = UIHSeedIslandLibrary::ComputeTotalLandKm2(RealmHalfExtentNSKm, RealmHalfExtentEWKm, DevLandAreaFraction);
		const float EqualAreaKm2 = TotalLandKm2 / FMath::Max(IslandCount, 1);
		AreasKm2.SetNum(IslandCount);
		for (int32 i = 0; i < IslandCount; ++i)
		{
			AreasKm2[i] = EqualAreaKm2;
		}
	}

	TArray<float> SemiMajorRadiiKm;
	SemiMajorRadiiKm.Reserve(AreasKm2.Num());
	for (const float AreaKm2 : AreasKm2)
	{
		SemiMajorRadiiKm.Add(UIHSeedIslandLibrary::SemiMajorKmFromWaterlineAreaKm2(AreaKm2));
	}

	const float SizeMult = FMath::Clamp(IslandSizeMultiplier, 1.f, 2.f);
	if (!FMath::IsNearlyEqual(SizeMult, 1.f))
	{
		for (float& RadiusKm : SemiMajorRadiiKm)
		{
			RadiusKm = FMath::Max(RadiusKm * SizeMult, 0.05f);
		}
	}

	TArray<float> LayoutExtentKm;
	const bool bUseSizeStarExtents =
		PerIslandLayoutExtentKm && PerIslandLayoutExtentKm->Num() == IslandCount;
	if (bUseSizeStarExtents)
	{
		LayoutExtentKm = *PerIslandLayoutExtentKm;
	}
	else
	{
		IHSeedIslandLibPrivate::BuildLayoutExtentRadiiKm(AreasKm2, LayoutExtentKm);
		if (!FMath::IsNearlyEqual(SizeMult, 1.f))
		{
			for (float& ExtentKm : LayoutExtentKm)
			{
				ExtentKm = FMath::Max(ExtentKm * SizeMult, 0.05f);
			}
		}
	}

	if (bUseSizeStarExtents)
	{
		for (int32 i = 0; i < IslandCount; ++i)
		{
			if (SemiMajorRadiiKm.IsValidIndex(i) && LayoutExtentKm.IsValidIndex(i))
			{
				SemiMajorRadiiKm[i] = FMath::Max(
					LayoutExtentKm[i] / IHSeedIslandLibPrivate::LayoutCollisionRadiusFactor, 0.05f);
			}
		}
	}

	TArray<float> BudgetSemiMajorRadiiKm = SemiMajorRadiiKm;
	TArray<FVector2D> CentersKm;
	float LaneFitScale = 1.f;

	if (IslandCount == 3)
	{
		if (bUseSizeStarExtents)
		{
			TArray<FVector2D> BinarySearchCentersKm;
			LaneFitScale = IHSeedIslandLibPrivate::BinarySearchMaxLaneFitScaleForThreeIslandsFromExtents(
				LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, BinarySearchCentersKm);

			for (float& ExtentKm : LayoutExtentKm)
			{
				ExtentKm = FMath::Max(ExtentKm * LaneFitScale, 0.05f);
			}
			for (float& RadiusKm : SemiMajorRadiiKm)
			{
				RadiusKm = FMath::Max(RadiusKm * LaneFitScale, 0.05f);
			}

			if (IHSeedIslandLibPrivate::ComputeMaxSpreadThreeIslandCentersKm(
				LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm))
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(
					LogIH_WB_Demo004, Log,
					TEXT("TankLayout: 3-island Size* max-spread (DevLandAreaFraction %.2f, lane-fit %.3f)."),
					DevLandAreaFraction, LaneFitScale);
#endif
			}
			else if (BinarySearchCentersKm.Num() == 3)
			{
				CentersKm = BinarySearchCentersKm;
#if !UE_BUILD_SHIPPING
				UE_LOG(
					LogIH_WB_Demo004, Warning,
					TEXT("TankLayout: 3-island Size* max-spread failed — using extent binary-search centers."));
#endif
			}
			else
			{
				IHSeedIslandLibPrivate::ComputeJitteredGridIslandCentersKm(
					3, MasterSeed, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm);
#if !UE_BUILD_SHIPPING
				UE_LOG(
					LogIH_WB_Demo004, Warning,
					TEXT("TankLayout: 3-island Size* spread failed — using jittered grid fallback."));
#endif
			}
		}
		else
		{
		TArray<FVector2D> BinarySearchCentersKm;
		LaneFitScale = IHSeedIslandLibPrivate::BinarySearchMaxLaneFitScaleForThreeIslands(
			BudgetSemiMajorRadiiKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, BinarySearchCentersKm);

		for (float& RadiusKm : SemiMajorRadiiKm)
		{
			RadiusKm = FMath::Max(RadiusKm * LaneFitScale, 0.05f);
		}

		TArray<FVector2D> SolverCentersKm;
		const bool bSolverOk = IHSeedIslandLibPrivate::SolveLaneAwareThreeIslandCentersKm(
			SemiMajorRadiiKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, SolverCentersKm);

		if (bSolverOk && SolverCentersKm.Num() == 3)
		{
			CentersKm = SolverCentersKm;
		}
		else if (BinarySearchCentersKm.Num() == 3)
		{
			CentersKm = BinarySearchCentersKm;
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogIH_WB_Demo004, Warning,
				TEXT("TankLayout: 3-island lane solver retry failed — using binary-search centers (DevLandAreaFraction %.2f)."),
				DevLandAreaFraction);
#endif
		}
		else if (IHSeedIslandLibPrivate::ComputeMaxSpreadThreeIslandCentersKm(
			LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm))
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogIH_WB_Demo004, Warning,
				TEXT("TankLayout: 3-island lane solver failed — using max-spread triangle (DevLandAreaFraction %.2f)."),
				DevLandAreaFraction);
#endif
		}
		else
		{
			IHSeedIslandLibPrivate::ComputeJitteredGridIslandCentersKm(
				3, MasterSeed, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm);
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogIH_WB_Demo004, Warning,
				TEXT("TankLayout: 3-island spread failed — using jittered grid fallback (DevLandAreaFraction %.2f)."),
				DevLandAreaFraction);
#endif
		}

		IHSeedIslandLibPrivate::EnforceCenterLaneSeparationKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		IHSeedIslandLibPrivate::ClampCentersInsideTankWallsKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		}

		IHSeedIslandLibPrivate::EnforceCenterLaneSeparationKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		IHSeedIslandLibPrivate::ClampCentersInsideTankWallsKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);

		if (PlacementScatter > KINDA_SMALL_NUMBER)
		{
			IHSeedIslandLibPrivate::ApplyBoundedCenterJitterKm(
				CentersKm, SemiMajorRadiiKm, MasterSeed, PlacementScatter * 0.12f,
				RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
			IHSeedIslandLibPrivate::EnforceCenterLaneSeparationKm(
				CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
			IHSeedIslandLibPrivate::ClampCentersInsideTankWallsKm(
				CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		}
	}
	else
	{
		IHSeedIslandLibPrivate::ComputeOrganicIslandCentersKm(
			IslandCount, MasterSeed, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, MinLaneKm, PlacementScatter,
			SemiMajorRadiiKm, CentersKm);
		IHSeedIslandLibPrivate::EnforceCenterLaneSeparationKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		IHSeedIslandLibPrivate::ClampCentersInsideTankWallsKm(
			CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);

		LaneFitScale = FMath::Clamp(
			IHSeedIslandLibPrivate::ComputeUniformLaneFitScale(CentersKm, SemiMajorRadiiKm, MinLaneKm),
			IHSeedIslandLibPrivate::AbsoluteMinLaneFitScale,
			1.f);

		if (LaneFitScale < 1.f - KINDA_SMALL_NUMBER)
		{
			for (float& RadiusKm : SemiMajorRadiiKm)
			{
				RadiusKm = FMath::Max(RadiusKm * LaneFitScale, 0.05f);
			}
			if (bUseSizeStarExtents)
			{
				for (float& ExtentKm : LayoutExtentKm)
				{
					ExtentKm = FMath::Max(ExtentKm * LaneFitScale, 0.05f);
				}
				IHSeedIslandLibPrivate::EnforceCenterLaneSeparationKm(
					CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
			}
			else
			{
				IHSeedIslandLibPrivate::EnforceCenterLaneSeparationFromSemiMajorKm(
					CentersKm, SemiMajorRadiiKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
			}
			IHSeedIslandLibPrivate::ClampCentersInsideTankWallsKm(
				CentersKm, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm);
		}
	}

#if !UE_BUILD_SHIPPING
	const float EffectiveLandFraction = DevLandAreaFraction * LaneFitScale * LaneFitScale;
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("TankLayout: DevLandAreaFraction %.2f | lane-fit scale %.3f | coast envelope %.2f× | effective land ~%.0f%% of tank | %d islands"),
		DevLandAreaFraction, LaneFitScale, IHSeedIslandLibPrivate::LayoutCollisionRadiusFactor,
		EffectiveLandFraction * 100.f, IslandCount);
#endif

	if (CentersKm.Num() != IslandCount || SemiMajorRadiiKm.Num() != IslandCount)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("TankLayout: layout size mismatch (centers=%d radii=%d expected=%d) — filling jittered grid."),
			CentersKm.Num(), SemiMajorRadiiKm.Num(), IslandCount);
#endif
		IHSeedIslandLibPrivate::ComputeJitteredGridIslandCentersKm(
			IslandCount, MasterSeed, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, CentersKm);
		if (SemiMajorRadiiKm.Num() != IslandCount)
		{
			SemiMajorRadiiKm.SetNum(IslandCount);
			for (int32 i = 0; i < IslandCount; ++i)
			{
				const float AreaKm2 = AreasKm2.IsValidIndex(i) ? AreasKm2[i] : 1.f;
				SemiMajorRadiiKm[i] = UIHSeedIslandLibrary::SemiMajorKmFromWaterlineAreaKm2(AreaKm2);
			}
		}
	}

	OutCentersWorldCm.Reserve(CentersKm.Num());
	OutSemiMajorCm.Reserve(SemiMajorRadiiKm.Num());
	for (int32 i = 0; i < CentersKm.Num(); ++i)
	{
		OutCentersWorldCm.Add(FVector2D(CentersKm[i].X * 100000.f, CentersKm[i].Y * 100000.f));
		const float SemiMajorCm = SemiMajorRadiiKm.IsValidIndex(i) ? SemiMajorRadiiKm[i] * 100000.f : 0.f;
		OutSemiMajorCm.Add(SemiMajorCm);
	}



#if !UE_BUILD_SHIPPING
	for (int32 i = 0; i < OutCentersWorldCm.Num(); ++i)
	{
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("TankLayout: Island %d center (%.0f, %.0f) cm | semi-major %.0f cm | area budget %.3f km² | size x%.2f"),
			i, OutCentersWorldCm[i].X, OutCentersWorldCm[i].Y, OutSemiMajorCm[i],
			AreasKm2.IsValidIndex(i) ? AreasKm2[i] : 0.f, SizeMult);
	}
#endif

}

} // namespace IHSeedIslandLayoutPrivate

void UIHSeedIslandLibrary::ComputeTankIslandLayoutCm(
	int32 IslandCount,
	int32 MasterSeed,
	TArray<FVector2D>& OutCentersWorldCm,
	TArray<float>& OutSemiMajorCm,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm,
	float PlacementScatter,
	float IslandSizeMultiplier,
	float DevLandAreaFraction)
{
	IHSeedIslandLayoutPrivate::ComputeTankIslandLayoutCmImpl(
		IslandCount, MasterSeed, OutCentersWorldCm, OutSemiMajorCm, RealmHalfExtentNSKm, RealmHalfExtentEWKm,
		PlacementScatter, IslandSizeMultiplier, DevLandAreaFraction, nullptr);
}

void UIHSeedIslandLibrary::ComputeTankIslandLayoutCmWithLayoutExtents(
	int32 IslandCount,
	int32 MasterSeed,
	TArray<FVector2D>& OutCentersWorldCm,
	TArray<float>& OutSemiMajorCm,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm,
	float PlacementScatter,
	float DevLandAreaFraction,
	const TArray<float>& PerIslandLayoutExtentKm)
{
	IHSeedIslandLayoutPrivate::ComputeTankIslandLayoutCmImpl(
		IslandCount, MasterSeed, OutCentersWorldCm, OutSemiMajorCm, RealmHalfExtentNSKm, RealmHalfExtentEWKm,
		PlacementScatter, 1.f, DevLandAreaFraction, &PerIslandLayoutExtentKm);
}

float UIHSeedIslandLibrary::CoastLayoutExtentKmFromAreaKm2(float AreaKm2)
{
	return IHSeedIslandLibPrivate::CoastLayoutExtentKmFromArea(AreaKm2);
}

bool UIHSeedIslandLibrary::TryPlaceThreeIslandsMaxSpreadKm(
	const TArray<float>& AreasKm2,
	float AreaScale,
	float RealmHalfExtentEWKm,
	float RealmHalfExtentNSKm,
	TArray<FVector2D>& OutCentersKm,
	float WallMarginKm)
{
	TArray<float> LayoutExtentKm;
	const float ClampedScale = FMath::Clamp(AreaScale, 0.25f, 1.f);
	const float AreaMult = ClampedScale * ClampedScale;
	LayoutExtentKm.Reserve(AreasKm2.Num());
	for (const float AreaKm2 : AreasKm2)
	{
		LayoutExtentKm.Add(CoastLayoutExtentKmFromAreaKm2(FMath::Max(AreaKm2, 0.05f) * AreaMult));
	}

	if (LayoutExtentKm.Num() != 3)
	{
		return false;
	}

	return IHSeedIslandLibPrivate::ComputeMaxSpreadThreeIslandCentersKm(
		LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
}

bool UIHSeedIslandLibrary::CanFitCoastExtentsInTankAtAreaScale(
	const TArray<float>& AreasKm2,
	float AreaScale,
	float RealmHalfExtentEWKm,
	float RealmHalfExtentNSKm,
	float WallMarginKm)
{
	const int32 N = AreasKm2.Num();
	if (N < 1)
	{
		return true;
	}

	const float ClampedScale = FMath::Clamp(AreaScale, 0.25f, 1.f);
	const float AreaMult = ClampedScale * ClampedScale;
	const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
	const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

	TArray<float> ExtentKm;
	ExtentKm.Reserve(N);
	for (const float AreaKm2 : AreasKm2)
	{
		const float Ext = CoastLayoutExtentKmFromAreaKm2(FMath::Max(AreaKm2, 0.05f) * AreaMult);
		if (Ext > UsableHalfWidthKm + KINDA_SMALL_NUMBER || Ext > UsableHalfDepthKm + KINDA_SMALL_NUMBER)
		{
			return false;
		}
		ExtentKm.Add(Ext);
	}

	for (int32 i = 0; i < N; ++i)
	{
		for (int32 j = i + 1; j < N; ++j)
		{
			const float RequiredKm = IHSeedIslandLibPrivate::ComputeMinCenterDistanceFromExtentsKm(ExtentKm[i], ExtentKm[j]);
			if (RequiredKm > 2.f * FMath::Min(UsableHalfWidthKm, UsableHalfDepthKm) + KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}
	}

	TArray<FVector2D> CentersKm;
	return TryPlaceIslandsMaxSpreadKm(N, ExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, CentersKm, WallMarginKm);
}

bool UIHSeedIslandLibrary::TryPlaceThreeIslandsMaxSpreadFromLayoutExtentsKm(
	const TArray<float>& LayoutExtentKm,
	float ExtentScale,
	float RealmHalfExtentEWKm,
	float RealmHalfExtentNSKm,
	TArray<FVector2D>& OutCentersKm,
	float WallMarginKm)
{
	if (LayoutExtentKm.Num() != 3)
	{
		return false;
	}

	const float ClampedScale = FMath::Clamp(ExtentScale, 0.25f, 1.f);
	TArray<float> ScaledExtentKm;
	ScaledExtentKm.SetNum(3);
	for (int32 i = 0; i < 3; ++i)
	{
		ScaledExtentKm[i] = FMath::Max(LayoutExtentKm[i] * ClampedScale, 0.05f);
	}

	return IHSeedIslandLibPrivate::ComputeMaxSpreadThreeIslandCentersKm(
		ScaledExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
}

bool UIHSeedIslandLibrary::CanFitLayoutExtentsInTankAtScale(
	const TArray<float>& LayoutExtentKm,
	float ExtentScale,
	float RealmHalfExtentEWKm,
	float RealmHalfExtentNSKm,
	float WallMarginKm)
{
	const int32 N = LayoutExtentKm.Num();
	if (N < 1)
	{
		return true;
	}

	const float ClampedScale = FMath::Clamp(ExtentScale, 0.25f, 1.f);
	const float UsableHalfWidthKm = FMath::Max(RealmHalfExtentEWKm - WallMarginKm, 0.05f);
	const float UsableHalfDepthKm = FMath::Max(RealmHalfExtentNSKm - WallMarginKm, 0.05f);

	TArray<float> ExtentKm;
	ExtentKm.Reserve(N);
	for (const float BaseExtentKm : LayoutExtentKm)
	{
		const float Ext = FMath::Max(BaseExtentKm * ClampedScale, 0.05f);
		if (Ext > UsableHalfWidthKm + KINDA_SMALL_NUMBER || Ext > UsableHalfDepthKm + KINDA_SMALL_NUMBER)
		{
			return false;
		}
		ExtentKm.Add(Ext);
	}

	for (int32 i = 0; i < N; ++i)
	{
		for (int32 j = i + 1; j < N; ++j)
		{
			const float RequiredKm = IHSeedIslandLibPrivate::ComputeMinCenterDistanceFromExtentsKm(ExtentKm[i], ExtentKm[j]);
			if (RequiredKm > 2.f * FMath::Min(UsableHalfWidthKm, UsableHalfDepthKm) + KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}
	}

	TArray<FVector2D> CentersKm;
	return TryPlaceIslandsMaxSpreadKm(N, ExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, CentersKm, WallMarginKm);
}

float UIHSeedIslandLibrary::ComputeEffectiveLandFractionFromExtentsKm(
	const TArray<float>& LayoutExtentKm,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm)
{
	const float TankAreaKm2 = ComputeRealmAreaKm2(RealmHalfExtentNSKm, RealmHalfExtentEWKm);
	if (TankAreaKm2 <= KINDA_SMALL_NUMBER || LayoutExtentKm.Num() < 1)
	{
		return 0.f;
	}

	float LandAreaKm2 = 0.f;
	for (const float ExtentKm : LayoutExtentKm)
	{
		const float Ext = FMath::Max(ExtentKm, 0.f);
		LandAreaKm2 += PI * Ext * Ext;
	}
	return LandAreaKm2 / TankAreaKm2;
}

float UIHSeedIslandLibrary::ComputeEffectiveLandFractionFromAreasKm2(
	const TArray<float>& AreasKm2,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm)
{
	const float TankAreaKm2 = ComputeRealmAreaKm2(RealmHalfExtentNSKm, RealmHalfExtentEWKm);
	if (TankAreaKm2 <= KINDA_SMALL_NUMBER || AreasKm2.Num() < 1)
	{
		return 0.f;
	}

	float TotalLandKm2 = 0.f;
	for (const float AreaKm2 : AreasKm2)
	{
		TotalLandKm2 += FMath::Max(AreaKm2, 0.f);
	}
	return TotalLandKm2 / TankAreaKm2;
}

bool UIHSeedIslandLibrary::SolveIslandLayoutForSeed(
	int32 IslandCount,
	int32 MasterSeed,
	float RealmHalfExtentNSKm,
	float RealmHalfExtentEWKm,
	float TargetEffectiveLandFraction,
	const TArray<float>& ShapeAreasKm2,
	const TArray<float>& BaseLayoutExtentKm,
	FIHIslandLayoutSolveResult& OutResult,
	float WallMarginKm)
{
	OutResult = FIHIslandLayoutSolveResult();
	OutResult.TargetEffectiveLandFraction = FMath::Clamp(TargetEffectiveLandFraction, 0.05f, 0.95f);

	if (!IsLandformCountInDesignRange(IslandCount)
		|| ShapeAreasKm2.Num() != IslandCount
		|| BaseLayoutExtentKm.Num() != IslandCount)
	{
		return false;
	}

	if (RealmHalfExtentEWKm <= KINDA_SMALL_NUMBER)
	{
		RealmHalfExtentEWKm = ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	}

	OutResult.BaseEffectiveLandFraction = ComputeEffectiveLandFractionFromAreasKm2(
		ShapeAreasKm2, RealmHalfExtentNSKm, RealmHalfExtentEWKm);
	if (OutResult.BaseEffectiveLandFraction <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float EnvelopeEffectiveAtShape = ComputeEffectiveLandFractionFromExtentsKm(
		BaseLayoutExtentKm, RealmHalfExtentNSKm, RealmHalfExtentEWKm);

	TArray<float> BaseSpacingExtentKm;
	IHSeedIslandLibPrivate::BuildPlacementSpacingExtentsKm(BaseLayoutExtentKm, BaseSpacingExtentKm);

	const float MinScale = IHInvisibleHandSpec::MinLayoutSolveAreaScale;
	float Lo = MinScale;
	float Hi = 1.f;
	float MaxFeasibleScale = MinScale;
	TArray<FVector2D> CentersAtMaxFeasible;

	for (int32 Iter = 0; Iter < 24; ++Iter)
	{
		const float Mid = (Lo + Hi) * 0.5f;
		const float ExtentLinear = FMath::Sqrt(Mid);
		TArray<float> ScaledSpacingExtentsKm;
		ScaledSpacingExtentsKm.SetNum(IslandCount);
		for (int32 i = 0; i < IslandCount; ++i)
		{
			ScaledSpacingExtentsKm[i] = FMath::Max(BaseSpacingExtentKm[i] * ExtentLinear, 0.05f);
		}

		TArray<FVector2D> TrialCentersKm;
		bool bUsedMaxSpread = false;
		if (IHSeedIslandLibPrivate::TryPlaceIslandsCompactFromSpacingKm(
			IslandCount, MasterSeed, ScaledSpacingExtentsKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
			TrialCentersKm, bUsedMaxSpread)
			&& !bUsedMaxSpread)
		{
			MaxFeasibleScale = Mid;
			CentersAtMaxFeasible = TrialCentersKm;
			Lo = Mid;
		}
		else
		{
			Hi = Mid;
		}
	}

	OutResult.MaxEffectiveLandFraction = OutResult.BaseEffectiveLandFraction * MaxFeasibleScale;

	// Shape areas already encode the target hint — only shrink when compact placement cannot fit.
	OutResult.UniformAreaScale = FMath::Clamp(MaxFeasibleScale, MinScale, 1.f);
	OutResult.AchievedEffectiveLandFraction = OutResult.BaseEffectiveLandFraction * OutResult.UniformAreaScale;

	const float FinalExtentLinear = FMath::Sqrt(OutResult.UniformAreaScale);
	OutResult.IslandAreasKm2.SetNum(IslandCount);
	OutResult.LayoutExtentKm.SetNum(IslandCount);
	for (int32 i = 0; i < IslandCount; ++i)
	{
		OutResult.IslandAreasKm2[i] = FMath::Max(ShapeAreasKm2[i] * OutResult.UniformAreaScale, 0.05f);
		OutResult.LayoutExtentKm[i] = FMath::Max(BaseLayoutExtentKm[i] * FinalExtentLinear, 0.05f);
	}

	bool bUsedMaxSpreadFinal = false;
	TArray<float> FinalSpacingExtentsKm;
	IHSeedIslandLibPrivate::BuildPlacementSpacingExtentsKm(OutResult.LayoutExtentKm, FinalSpacingExtentsKm);
	if (!IHSeedIslandLibPrivate::TryPlaceIslandsCompactFromSpacingKm(
		IslandCount, MasterSeed, FinalSpacingExtentsKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
		OutResult.CentersKm, bUsedMaxSpreadFinal)
		&& CentersAtMaxFeasible.Num() == IslandCount
		&& FMath::IsNearlyEqual(OutResult.UniformAreaScale, MaxFeasibleScale, 0.02f))
	{
		OutResult.CentersKm = CentersAtMaxFeasible;
		bUsedMaxSpreadFinal = false;
	}

	OutResult.bUsedCompactPlacement = !bUsedMaxSpreadFinal;
	OutResult.bUsedMaxSpreadFallback = bUsedMaxSpreadFinal;
	OutResult.bSuccess = OutResult.CentersKm.Num() == IslandCount;

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("LayoutSolve: target %.0f%% | area base %.0f%% | envelope base %.0f%% | max %.0f%% | achieved %.0f%% | area scale %.3f | compact=%d | N=%d"),
		OutResult.TargetEffectiveLandFraction * 100.f,
		OutResult.BaseEffectiveLandFraction * 100.f,
		EnvelopeEffectiveAtShape * 100.f,
		OutResult.MaxEffectiveLandFraction * 100.f,
		OutResult.AchievedEffectiveLandFraction * 100.f,
		OutResult.UniformAreaScale,
		OutResult.bUsedCompactPlacement ? 1 : 0,
		IslandCount);
#endif

	return OutResult.bSuccess;
}

void UIHSeedIslandLibrary::ApplyIslandLayoutSolveToTankCm(
	const FIHIslandLayoutSolveResult& Solve,
	TArray<FVector2D>& OutCentersWorldCm,
	TArray<float>& OutSemiMajorCm)
{
	OutCentersWorldCm.Reset();
	OutSemiMajorCm.Reset();
	if (!Solve.bSuccess || Solve.CentersKm.Num() < 1)
	{
		return;
	}

	OutCentersWorldCm.Reserve(Solve.CentersKm.Num());
	OutSemiMajorCm.Reserve(Solve.CentersKm.Num());
	for (int32 i = 0; i < Solve.CentersKm.Num(); ++i)
	{
		OutCentersWorldCm.Add(Solve.CentersKm[i] * 100000.f);
		const float AreaKm2 = Solve.IslandAreasKm2.IsValidIndex(i) ? Solve.IslandAreasKm2[i] : 0.05f;
		OutSemiMajorCm.Add(SemiMajorKmFromWaterlineAreaKm2(AreaKm2) * 100000.f);
	}
}

bool UIHSeedIslandLibrary::TryPlaceIslandsCompactKm(
	int32 IslandCount,
	int32 MasterSeed,
	const TArray<float>& LayoutExtentKm,
	float RealmHalfExtentEWKm,
	float RealmHalfExtentNSKm,
	TArray<FVector2D>& OutCentersKm,
	float WallMarginKm,
	bool* bOutUsedMaxSpreadFallback)
{
	bool bUsedMaxSpread = false;
	const bool bOk = IHSeedIslandLibPrivate::TryPlaceIslandsCompactKm(
		IslandCount, MasterSeed, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm,
		OutCentersKm, bUsedMaxSpread);
	if (bOutUsedMaxSpreadFallback)
	{
		*bOutUsedMaxSpreadFallback = bUsedMaxSpread;
	}
	return bOk;
}

bool UIHSeedIslandLibrary::TryPlaceIslandsMaxSpreadKm(
	int32 IslandCount,
	const TArray<float>& LayoutExtentKm,
	float RealmHalfExtentEWKm,
	float RealmHalfExtentNSKm,
	TArray<FVector2D>& OutCentersKm,
	float WallMarginKm)
{
	if (LayoutExtentKm.Num() != IslandCount)
	{
		return false;
	}

	switch (IslandCount)
	{
	case 2:
	case 3:
	case 4:
	default:
		return IHSeedIslandLibPrivate::DispatchMaxSpreadPlacementKm(
			IslandCount, LayoutExtentKm, RealmHalfExtentEWKm, RealmHalfExtentNSKm, WallMarginKm, OutCentersKm);
	}
}

