// Copyright Invisible Hand. All Rights Reserved.
#include "IHSectorFabric.h"
#include "IHTerrainCellDiffusion.h"
#include "Algo/Reverse.h"

namespace IHSectorFabricPrivate
{
	// IH-DEC-029 shared slope bands (InvisibleHand_RiverTributary_System.md §7). Kept local to this
	// module rather than IHInvisibleHandDesignSpec.h (an InvisibleHand/-folder concern) since these
	// are intrinsically a Sector Fabric measurement, not a design/dev-flag setting.
	constexpr double SlopeSteepMinDeg = 26.0;
	constexpr double SlopeModerateMinDeg = 6.0;

	/** Linear-scan nearest-site lookup - same idiom as IHTerrainCellDiffusionPrivate's own private helper. */
	static int32 NearestCellToPoint(const FIHTerrainCellGraph& Graph, const FVector2D& Point)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = TNumericLimits<double>::Max();
		for (int32 i = 0; i < Graph.Num(); ++i)
		{
			const double DistSq = FVector2D::DistSquared(Graph.Cells[i].SitePos, Point);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = i;
			}
		}
		return Best;
	}

	static double PolylineArcLength(const TArray<FVector2D>& Pts)
	{
		double Len = 0.0;
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			Len += FVector2D::Distance(Pts[i], Pts[i + 1]);
		}
		return Len;
	}

	/** Signed shoelace area (positive = CCW) and centroid of a closed, unclosed-storage polygon. */
	static bool ShoelaceSignedAreaAndCentroid(
		const TArray<FVector2D>& Poly, double& OutSignedAreaX2, FVector2D& OutCentroid)
	{
		OutSignedAreaX2 = 0.0;
		FVector2D Accum = FVector2D::ZeroVector;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[(i + 1) % Poly.Num()];
			const double Cross = A.X * B.Y - B.X * A.Y;
			OutSignedAreaX2 += Cross;
			Accum += (A + B) * Cross;
		}
		if (!FMath::IsFinite(OutSignedAreaX2) || FMath::IsNearlyZero(OutSignedAreaX2))
		{
			return false;
		}
		OutCentroid = Accum / (3.0 * OutSignedAreaX2);
		return true;
	}
}

void FIHSectorFabric::ComputeSlopeDegrees(
	const FIHTerrainCellGraph& Graph, const double LandThresholdRaw, const double HeightSpan,
	const double SummitTopZCm, TArray<double>& OutSlopeDegrees, TArray<EIHSectorSlopeClass>& OutSlopeClass)
{
	using namespace IHSectorFabricPrivate;

	const int32 NumCells = Graph.Num();
	OutSlopeDegrees.Init(0.0, NumCells);
	OutSlopeClass.Init(EIHSectorSlopeClass::Flat, NumCells);
	if (NumCells == 0)
	{
		return;
	}

	const double SafeHeightSpan = FMath::Max(HeightSpan, 1.0);
	const double K = SummitTopZCm / SafeHeightSpan; // Zcm per raw Height unit

	for (int32 i = 0; i < NumCells; ++i)
	{
		const FIHTerrainCell& Cell = Graph.Cells[i];
		const double ZcmSelf = (Cell.Height - LandThresholdRaw) * K;
		double SumDeg = 0.0;
		int32 Count = 0;
		for (const int32 NeighborIdx : Cell.Neighbors)
		{
			if (!Graph.IsValidIndex(NeighborIdx))
			{
				continue;
			}
			const FIHTerrainCell& Neighbor = Graph.Cells[NeighborIdx];
			const double ZcmNeighbor = (Neighbor.Height - LandThresholdRaw) * K;
			const double DZ = FMath::Abs(ZcmSelf - ZcmNeighbor);
			const double DPos = FVector2D::Distance(Cell.SitePos, Neighbor.SitePos);
			if (DPos > KINDA_SMALL_NUMBER)
			{
				SumDeg += FMath::RadiansToDegrees(FMath::Atan(DZ / DPos));
				++Count;
			}
		}
		const double MeanDeg = (Count > 0) ? (SumDeg / Count) : 0.0;
		OutSlopeDegrees[i] = MeanDeg;
		OutSlopeClass[i] = (MeanDeg >= SlopeSteepMinDeg) ? EIHSectorSlopeClass::Steep
			: (MeanDeg >= SlopeModerateMinDeg) ? EIHSectorSlopeClass::Moderate
			: EIHSectorSlopeClass::Flat;
	}
}

bool FIHSectorFabric::TraceFlowlineDownhill(
	const FIHTerrainCellGraph& Graph, const int32 StartCellIdx, const double TargetHeightRaw,
	const int32 MaxHops, const double Randomness, FRandomStream& Stream, TArray<int32>& OutPathCellIndices)
{
	OutPathCellIndices.Reset();
	if (!Graph.IsValidIndex(StartCellIdx))
	{
		return false;
	}

	TSet<int32> Visited;
	int32 Current = StartCellIdx;
	OutPathCellIndices.Add(Current);
	Visited.Add(Current);

	if (Graph.Cells[Current].Height <= TargetHeightRaw)
	{
		return true; // already there
	}

	const int32 Bound = FMath::Min(MaxHops, Graph.Num());
	for (int32 Step = 0; Step < Bound; ++Step)
	{
		const double CurHeight = Graph.Cells[Current].Height;
		int32 Best = INDEX_NONE;
		double BestScore = -TNumericLimits<double>::Max();
		for (const int32 NeighborIdx : Graph.Cells[Current].Neighbors)
		{
			if (!Graph.IsValidIndex(NeighborIdx) || Visited.Contains(NeighborIdx))
			{
				continue;
			}
			const double NeighborHeight = Graph.Cells[NeighborIdx].Height;
			double Score = CurHeight - NeighborHeight; // positive = downhill
			Score += Stream.FRandRange(-1.0, 1.0) * Randomness;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = NeighborIdx;
			}
		}

		if (Best == INDEX_NONE)
		{
			break; // pit / dead end - no unvisited neighbor to continue to
		}

		Current = Best;
		Visited.Add(Current);
		OutPathCellIndices.Add(Current);

		if (Graph.Cells[Current].Height <= TargetHeightRaw)
		{
			return true;
		}
	}

	return false; // never reached TargetHeightRaw within Bound hops
}

bool FIHSectorFabric::BuildSectorQuad(
	const TArray<FVector2D>& OuterIsolineSeg, const TArray<FVector2D>& InnerIsolineSeg,
	const TArray<FVector2D>& Flowline1Pts, const TArray<FVector2D>& Flowline2Pts,
	const FIHTerrainCellGraph& Graph, FIHSectorFabricCell& OutSector)
{
	using namespace IHSectorFabricPrivate;

	if (OuterIsolineSeg.Num() < 1 || InnerIsolineSeg.Num() < 1 || Flowline1Pts.Num() < 1 || Flowline2Pts.Num() < 1)
	{
		return false;
	}

	// Concatenate: Outer forward -> Flowline2 forward (downhill) -> Inner reversed -> Flowline1
	// reversed (back uphill), closing the loop.
	TArray<FVector2D> Poly;
	Poly.Reserve(OuterIsolineSeg.Num() + Flowline2Pts.Num() + InnerIsolineSeg.Num() + Flowline1Pts.Num());
	Poly.Append(OuterIsolineSeg);
	Poly.Append(Flowline2Pts);
	for (int32 i = InnerIsolineSeg.Num() - 1; i >= 0; --i)
	{
		Poly.Add(InnerIsolineSeg[i]);
	}
	for (int32 i = Flowline1Pts.Num() - 1; i >= 0; --i)
	{
		Poly.Add(Flowline1Pts[i]);
	}

	// Collapse consecutive near-duplicate points (the shared joins between the 4 segments).
	constexpr double MinPointSepCmSq = 1.0; // 1cm
	TArray<FVector2D> Cleaned;
	Cleaned.Reserve(Poly.Num());
	for (const FVector2D& P : Poly)
	{
		if (Cleaned.Num() == 0 || FVector2D::DistSquared(Cleaned.Last(), P) > MinPointSepCmSq)
		{
			Cleaned.Add(P);
		}
	}
	if (Cleaned.Num() >= 2 && FVector2D::DistSquared(Cleaned[0], Cleaned.Last()) <= MinPointSepCmSq)
	{
		Cleaned.Pop(EAllowShrinking::No); // drop duplicate closing vertex
	}
	if (Cleaned.Num() < 3)
	{
		return false;
	}

	double SignedAreaX2 = 0.0;
	FVector2D Centroid = FVector2D::ZeroVector;
	if (!ShoelaceSignedAreaAndCentroid(Cleaned, SignedAreaX2, Centroid))
	{
		return false;
	}
	const double AreaCm2 = FMath::Abs(SignedAreaX2) * 0.5;

	if (SignedAreaX2 < 0.0) // CW -> normalize to CCW
	{
		Algo::Reverse(Cleaned);
	}

	OutSector.PolygonVerts = MoveTemp(Cleaned);
	OutSector.Centroid = Centroid;
	OutSector.AreaCm2 = AreaCm2;
	OutSector.RadialRunCm = 0.5 * (PolylineArcLength(Flowline1Pts) + PolylineArcLength(Flowline2Pts));
	OutSector.AlongContourWidthCm = PolylineArcLength(InnerIsolineSeg);

	// SeawardEdgeIndex: approximate as the polygon vertex closest to the inner segment's midpoint
	// (the inner/lower-elevation edge is, by construction, the seaward-facing one).
	const FVector2D InnerMid = InnerIsolineSeg[InnerIsolineSeg.Num() / 2];
	int32 ClosestIdx = 0;
	double ClosestDistSq = TNumericLimits<double>::Max();
	for (int32 i = 0; i < OutSector.PolygonVerts.Num(); ++i)
	{
		const double DistSq = FVector2D::DistSquared(OutSector.PolygonVerts[i], InnerMid);
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			ClosestIdx = i;
		}
	}
	OutSector.SeawardEdgeIndex = ClosestIdx;

	// FeatureType: single nearest-cell sample at the centroid (simplified from a full majority
	// vote for this prototype - sufficient to distinguish land vs shelf sectors at a glance).
	const int32 CentroidCellIdx = NearestCellToPoint(Graph, Centroid);
	OutSector.FeatureType = Graph.IsValidIndex(CentroidCellIdx)
		? Graph.Cells[CentroidCellIdx].Feature : EIHCellFeature::Land;

	return true;
}

bool FIHSectorFabric::BuildSectorFabricForRegion(
	const FIHTerrainCellGraph& Graph, const double SweepFloorHeightRaw, const double SweepCeilingHeightRaw,
	const double LandThresholdRaw, const double HeightSpan, const double SummitTopZCm,
	const FIHSectorFabricParams& Params, const FVector2D& BoundsMinLocalCm, const FVector2D& BoundsMaxLocalCm,
	FRandomStream& Stream, TArray<FIHSectorFabricCell>& OutSectors)
{
	using namespace IHSectorFabricPrivate;

	OutSectors.Reset();
	if (Graph.Num() == 0 || SweepCeilingHeightRaw <= SweepFloorHeightRaw)
	{
		return false;
	}

	const double SafeHeightSpan = FMath::Max(HeightSpan, 1.0);
	const double K = SummitTopZCm / SafeHeightSpan; // Zcm per raw Height unit
	if (!FMath::IsFinite(K) || FMath::IsNearlyZero(K))
	{
		return false;
	}
	const double ContourIntervalRaw = FMath::Max(Params.ContourIntervalCm / K, KINDA_SMALL_NUMBER);

	TArray<double> SlopeDegrees;
	TArray<EIHSectorSlopeClass> SlopeClass;
	ComputeSlopeDegrees(Graph, LandThresholdRaw, HeightSpan, SummitTopZCm, SlopeDegrees, SlopeClass);

	// Isoline levels from ceiling down to floor, step ContourIntervalRaw, always including the
	// exact floor as the final level regardless of step alignment.
	TArray<double> Levels;
	for (double H = SweepCeilingHeightRaw; H > SweepFloorHeightRaw + KINDA_SMALL_NUMBER; H -= ContourIntervalRaw)
	{
		Levels.Add(H);
	}
	Levels.Add(SweepFloorHeightRaw);
	if (Levels.Num() < 2)
	{
		return false;
	}

	for (int32 LevelIdx = 0; LevelIdx < Levels.Num() - 1; ++LevelIdx)
	{
		const double OuterHeight = Levels[LevelIdx];
		const double InnerHeight = Levels[LevelIdx + 1];

		TArray<TArray<FVector2D>> OuterLoops;
		FIHTerrainCellDiffusion::TraceHeightIsolineLoops(Graph, OuterHeight, OuterLoops);

		for (const TArray<FVector2D>& Loop : OuterLoops)
		{
			if (Loop.Num() < 3)
			{
				continue;
			}

			// Adaptive seed placement: walk the loop, accumulate arclength, place a new seed once
			// accumulated length exceeds the LOCALLY sampled target spacing - derived from
			// Area ~= RadialRun x AlongContourWidth = TargetSectorAreaCm2 and
			// RadialRun ~= ContourIntervalCm / tan(LocalSlope), so
			// AlongContourWidth ~= TargetSectorAreaCm2 * tan(LocalSlope) / ContourIntervalCm -
			// this is what makes steep terrain produce tightly-spaced (narrow) seeds and flat
			// terrain produce widely-spaced (broad) seeds, matching the canon shape-derivation.
			TArray<int32> SeedLoopIndices;
			SeedLoopIndices.Add(0);
			double AccumLen = 0.0;
			for (int32 i = 0; i < Loop.Num(); ++i)
			{
				const FVector2D& A = Loop[i];
				const FVector2D& B = Loop[(i + 1) % Loop.Num()];
				AccumLen += FVector2D::Distance(A, B);

				const int32 NearestIdx = NearestCellToPoint(Graph, B);
				const double LocalSlopeDeg = Graph.IsValidIndex(NearestIdx) ? SlopeDegrees[NearestIdx] : 5.0;
				const double TanSlope = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(LocalSlopeDeg, 1.0, 89.0)));
				const double TargetSpacingCm = FMath::Clamp(
					Params.TargetSectorAreaCm2 * TanSlope / Params.ContourIntervalCm, 500.0, 100000.0);

				if (AccumLen >= TargetSpacingCm)
				{
					SeedLoopIndices.Add((i + 1) % Loop.Num());
					AccumLen = 0.0;
				}
			}
			if (SeedLoopIndices.Num() > 1 && SeedLoopIndices.Last() == SeedLoopIndices[0])
			{
				SeedLoopIndices.Pop(EAllowShrinking::No); // avoid a zero-length wrap sector
			}
			if (SeedLoopIndices.Num() < 2)
			{
				continue; // whole loop shorter than one target-width sector at this level
			}

			for (int32 s = 0; s < SeedLoopIndices.Num(); ++s)
			{
				const int32 FromIdx = SeedLoopIndices[s];
				const int32 ToIdx = SeedLoopIndices[(s + 1) % SeedLoopIndices.Num()];

				TArray<FVector2D> OuterSeg;
				OuterSeg.Add(Loop[FromIdx]);
				for (int32 i = FromIdx; i != ToIdx; i = (i + 1) % Loop.Num())
				{
					OuterSeg.Add(Loop[(i + 1) % Loop.Num()]);
				}

				const int32 SeedCellA = NearestCellToPoint(Graph, Loop[FromIdx]);
				const int32 SeedCellB = NearestCellToPoint(Graph, Loop[ToIdx]);
				if (!Graph.IsValidIndex(SeedCellA) || !Graph.IsValidIndex(SeedCellB))
				{
					continue;
				}

				TArray<int32> Flow1CellPath, Flow2CellPath;
				const bool bOk1 = TraceFlowlineDownhill(
					Graph, SeedCellA, InnerHeight, Params.MaxFlowlineHops, Params.FlowlineRandomness,
					Stream, Flow1CellPath);
				const bool bOk2 = TraceFlowlineDownhill(
					Graph, SeedCellB, InnerHeight, Params.MaxFlowlineHops, Params.FlowlineRandomness,
					Stream, Flow2CellPath);
				if (!bOk1 || !bOk2)
				{
					continue; // a flowline hit a pit/hop-limit before reaching InnerHeight - skip, don't fake it
				}

				TArray<FVector2D> Flow1Pts, Flow2Pts;
				Flow1Pts.Reserve(Flow1CellPath.Num());
				for (const int32 Idx : Flow1CellPath) { Flow1Pts.Add(Graph.Cells[Idx].SitePos); }
				Flow2Pts.Reserve(Flow2CellPath.Num());
				for (const int32 Idx : Flow2CellPath) { Flow2Pts.Add(Graph.Cells[Idx].SitePos); }

				// Inner edge simplified to a straight segment between the two flowlines' endpoints
				// for this first prototype pass, rather than extracting the true inner-isoline
				// sub-arc - a deliberate scope cut (see class header comment) that keeps this pass
				// tractable while still validating the qualitative slope-shape hypothesis.
				const TArray<FVector2D> InnerSeg = { Flow1Pts.Last(), Flow2Pts.Last() };

				FIHSectorFabricCell Sector;
				if (!BuildSectorQuad(OuterSeg, InnerSeg, Flow1Pts, Flow2Pts, Graph, Sector))
				{
					continue;
				}

				if (Sector.Centroid.X < BoundsMinLocalCm.X || Sector.Centroid.X > BoundsMaxLocalCm.X ||
					Sector.Centroid.Y < BoundsMinLocalCm.Y || Sector.Centroid.Y > BoundsMaxLocalCm.Y)
				{
					continue; // outside the caller's requested region
				}

				const int32 CentroidCellIdx = NearestCellToPoint(Graph, Sector.Centroid);
				Sector.SlopeClass = Graph.IsValidIndex(CentroidCellIdx)
					? SlopeClass[CentroidCellIdx] : EIHSectorSlopeClass::Flat;

				OutSectors.Add(MoveTemp(Sector));
			}
		}
	}

	return true;
}
