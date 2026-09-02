// Copyright Invisible Hand. All Rights Reserved.
#include "IHTerrainCellDiffusion.h"

namespace IHTerrainCellDiffusionPrivate
{
	/**
	 * BFS diffusion from one or more seed cells sharing a single frontier (multi-source BFS),
	 * power-law decay + jitter (Azgaar heightmap-generator.ts mechanism). With multiple seeds,
	 * each cell's cross-section falls off from whichever seed reaches it first — this is what
	 * turns a seeded path into a valley/ridge shape rather than a chain of overlapping circular
	 * blobs. Guaranteed O(NumCells): each cell is enqueued at most once, guarded by Assigned[].
	 */
	static void DiffuseFromSeeds(
		FIHTerrainCellGraph& Graph, const TArray<int32>& SeedIndices, const double SeedHeight,
		const double PowerExp, FRandomStream& Stream)
	{
		if (SeedIndices.Num() == 0)
		{
			return;
		}

		const int32 NumCells = Graph.Num();
		TArray<double> Change;
		Change.Init(0.0, NumCells);
		TArray<bool> Assigned;
		Assigned.Init(false, NumCells);

		TArray<int32> Queue;
		Queue.Reserve(NumCells);
		for (const int32 SeedIdx : SeedIndices)
		{
			if (!Graph.IsValidIndex(SeedIdx) || Assigned[SeedIdx])
			{
				continue;
			}
			Change[SeedIdx] = SeedHeight;
			Assigned[SeedIdx] = true;
			Queue.Add(SeedIdx);
		}

		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const int32 Cur = Queue[Head++];
			const double CurChange = Change[Cur];
			if (FMath::Abs(CurChange) <= 1.0)
			{
				continue;
			}

			for (const int32 NeighborIdx : Graph.Cells[Cur].Neighbors)
			{
				if (!Graph.IsValidIndex(NeighborIdx) || Assigned[NeighborIdx])
				{
					continue;
				}
				const double Jitter = Stream.FRandRange(0.9, 1.1);
				const double Sign = CurChange >= 0.0 ? 1.0 : -1.0;
				const double NextChange = Sign * FMath::Pow(FMath::Abs(CurChange), PowerExp) * Jitter;

				Assigned[NeighborIdx] = true;
				Change[NeighborIdx] = NextChange;
				Graph.Cells[NeighborIdx].Height += NextChange;

				if (FMath::Abs(NextChange) > 1.0)
				{
					Queue.Add(NeighborIdx);
				}
			}
		}

		for (const int32 SeedIdx : SeedIndices)
		{
			if (Graph.IsValidIndex(SeedIdx))
			{
				Graph.Cells[SeedIdx].Height += SeedHeight;
			}
		}
	}

	/**
	 * Greedy cell-to-cell pathfind from StartIdx toward EndIdx: at each step, scores unvisited
	 * neighbors by alignment with the direction-to-target (plus Randomness noise for winding),
	 * picks the best. Hard-bounded to Graph.Num() steps — guaranteed termination regardless of
	 * graph topology, per this session's infinite-loop lesson (see IHCoastPolylineSmoothing.cpp).
	 */
	static bool FindPathBetweenCells(
		const FIHTerrainCellGraph& Graph, const int32 StartIdx, const int32 EndIdx,
		const double Randomness, FRandomStream& Stream, TArray<int32>& OutPath)
	{
		OutPath.Reset();
		if (!Graph.IsValidIndex(StartIdx) || !Graph.IsValidIndex(EndIdx))
		{
			return false;
		}

		TSet<int32> Visited;
		int32 Current = StartIdx;
		OutPath.Add(Current);
		Visited.Add(Current);

		const FVector2D TargetPos = Graph.Cells[EndIdx].SitePos;
		const int32 MaxSteps = Graph.Num(); // hard bound, see comment above

		for (int32 Step = 0; Step < MaxSteps; ++Step)
		{
			if (Current == EndIdx)
			{
				return true;
			}

			const FVector2D CurPos = Graph.Cells[Current].SitePos;
			const FVector2D ToTarget = (TargetPos - CurPos).GetSafeNormal();

			int32 Best = INDEX_NONE;
			double BestScore = -TNumericLimits<double>::Max();
			for (const int32 NeighborIdx : Graph.Cells[Current].Neighbors)
			{
				if (!Graph.IsValidIndex(NeighborIdx) || Visited.Contains(NeighborIdx))
				{
					continue;
				}
				const FVector2D Dir = (Graph.Cells[NeighborIdx].SitePos - CurPos).GetSafeNormal();
				double Score = FVector2D::DotProduct(Dir, ToTarget);
				Score += Stream.FRandRange(-1.0, 1.0) * Randomness;
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = NeighborIdx;
				}
			}

			if (Best == INDEX_NONE)
			{
				break; // dead end (all neighbors visited) — stop with whatever path we have
			}

			Current = Best;
			Visited.Add(Current);
			OutPath.Add(Current);
		}

		return OutPath.Num() > 1;
	}

	/** Linear-scan nearest-site lookup. Shared by PickCellInFractionalRange and PickCellNearPath. */
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

	static int32 PickCellInFractionalRange(
		const FIHTerrainCellGraph& Graph, const FVector2D& RangeXFrac, const FVector2D& RangeYFrac,
		FRandomStream& Stream)
	{
		const FVector2D BoundsSize = Graph.BoundsMaxLocalCm - Graph.BoundsMinLocalCm;
		const double TargetX = Graph.BoundsMinLocalCm.X + BoundsSize.X * Stream.FRandRange(
			static_cast<float>(RangeXFrac.X), static_cast<float>(RangeXFrac.Y));
		const double TargetY = Graph.BoundsMinLocalCm.Y + BoundsSize.Y * Stream.FRandRange(
			static_cast<float>(RangeYFrac.X), static_cast<float>(RangeYFrac.Y));
		return NearestCellToPoint(Graph, FVector2D(TargetX, TargetY));
	}

	/** Records the site positions of a diffusion path, if the caller asked for them. */
	static void RecordPathPositions(
		const FIHTerrainCellGraph& Graph, const TArray<int32>& Path, TArray<FVector2D>& OutPositions)
	{
		OutPositions.Reset();
		OutPositions.Reserve(Path.Num());
		for (const int32 Idx : Path)
		{
			if (Graph.IsValidIndex(Idx))
			{
				OutPositions.Add(Graph.Cells[Idx].SitePos);
			}
		}
	}
}

void FIHTerrainCellDiffusion::AddHill(
	FIHTerrainCellGraph& Graph, const int32 Count, const double HeightMin, const double HeightMax,
	const FVector2D& RangeXFrac, const FVector2D& RangeYFrac, const double BlobPower, FRandomStream& Stream)
{
	using namespace IHTerrainCellDiffusionPrivate;

	if (Graph.Num() == 0 || Count <= 0)
	{
		return;
	}

	for (int32 HillIdx = 0; HillIdx < Count; ++HillIdx)
	{
		const int32 SeedIdx = PickCellInFractionalRange(Graph, RangeXFrac, RangeYFrac, Stream);
		if (SeedIdx == INDEX_NONE)
		{
			continue;
		}
		const double SeedHeight = Stream.FRandRange(static_cast<float>(HeightMin), static_cast<float>(HeightMax));
		DiffuseFromSeeds(Graph, { SeedIdx }, SeedHeight, BlobPower, Stream);
	}
}

void FIHTerrainCellDiffusion::AddHillFromCellSet(
	FIHTerrainCellGraph& Graph, const TArray<int32>& SeedIndices, const double HeightMin,
	const double HeightMax, const double BlobPower, FRandomStream& Stream)
{
	using namespace IHTerrainCellDiffusionPrivate;

	if (Graph.Num() == 0 || SeedIndices.Num() == 0)
	{
		return;
	}

	const double SeedHeight = Stream.FRandRange(static_cast<float>(HeightMin), static_cast<float>(HeightMax));
	DiffuseFromSeeds(Graph, SeedIndices, SeedHeight, BlobPower, Stream);
}

void FIHTerrainCellDiffusion::PickMinSpacedCellsInFractionalRange(
	const FIHTerrainCellGraph& Graph, const int32 Count, const FVector2D& RangeXFrac,
	const FVector2D& RangeYFrac, const double MinSpacingCm, FRandomStream& Stream,
	TArray<int32>& OutCellIndices)
{
	using namespace IHTerrainCellDiffusionPrivate;

	OutCellIndices.Reset();
	if (Graph.Num() == 0 || Count <= 0)
	{
		return;
	}

	constexpr int32 MaxAttemptsPerSeed = 12;
	const double MinSpacingCmSq = MinSpacingCm * MinSpacingCm;

	for (int32 SeedSlot = 0; SeedSlot < Count; ++SeedSlot)
	{
		int32 Candidate = INDEX_NONE;
		for (int32 Attempt = 0; Attempt < MaxAttemptsPerSeed; ++Attempt)
		{
			const int32 Try = PickCellInFractionalRange(Graph, RangeXFrac, RangeYFrac, Stream);
			if (Try == INDEX_NONE)
			{
				continue;
			}
			Candidate = Try; // accept this draw unless a closer look finds it too close - see below

			bool bTooClose = false;
			for (const int32 Existing : OutCellIndices)
			{
				if (FVector2D::DistSquared(Graph.Cells[Try].SitePos, Graph.Cells[Existing].SitePos) < MinSpacingCmSq)
				{
					bTooClose = true;
					break;
				}
			}
			if (!bTooClose)
			{
				break; // good draw, keep Candidate
			}
		}
		if (Candidate != INDEX_NONE)
		{
			OutCellIndices.Add(Candidate); // last draw kept even if still too close - see header comment
		}
	}
}

void FIHTerrainCellDiffusion::AddRange(
	FIHTerrainCellGraph& Graph, const int32 Count, const double HeightMin, const double HeightMax,
	const FVector2D& RangeXFrac, const FVector2D& RangeYFrac, const double LinePower,
	const double PathRandomness, FRandomStream& Stream,
	TArray<TArray<FVector2D>>* OutPathsSitePositionsLocalCm)
{
	using namespace IHTerrainCellDiffusionPrivate;

	if (OutPathsSitePositionsLocalCm)
	{
		OutPathsSitePositionsLocalCm->Reset();
	}

	if (Graph.Num() == 0 || Count <= 0)
	{
		return;
	}

	for (int32 RangeIdx = 0; RangeIdx < Count; ++RangeIdx)
	{
		const int32 StartIdx = PickCellInFractionalRange(Graph, RangeXFrac, RangeYFrac, Stream);
		const int32 EndIdx = PickCellInFractionalRange(Graph, RangeXFrac, RangeYFrac, Stream);
		if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE || StartIdx == EndIdx)
		{
			continue;
		}

		TArray<int32> Path;
		if (!FindPathBetweenCells(Graph, StartIdx, EndIdx, PathRandomness, Stream, Path) || Path.Num() < 2)
		{
			continue;
		}

		const double Height = Stream.FRandRange(static_cast<float>(HeightMin), static_cast<float>(HeightMax));
		DiffuseFromSeeds(Graph, Path, Height, LinePower, Stream);

		if (OutPathsSitePositionsLocalCm)
		{
			TArray<FVector2D> Positions;
			RecordPathPositions(Graph, Path, Positions);
			OutPathsSitePositionsLocalCm->Add(MoveTemp(Positions));
		}
	}
}

void FIHTerrainCellDiffusion::AddRangeBetweenCells(
	FIHTerrainCellGraph& Graph, const int32 StartIdx, const int32 EndIdx, const double HeightMin,
	const double HeightMax, const double LinePower, const double PathRandomness, FRandomStream& Stream,
	TArray<FVector2D>* OutPathSitePositionsLocalCm)
{
	using namespace IHTerrainCellDiffusionPrivate;

	if (OutPathSitePositionsLocalCm)
	{
		OutPathSitePositionsLocalCm->Reset();
	}

	if (!Graph.IsValidIndex(StartIdx) || !Graph.IsValidIndex(EndIdx) || StartIdx == EndIdx)
	{
		return;
	}

	TArray<int32> Path;
	if (!FindPathBetweenCells(Graph, StartIdx, EndIdx, PathRandomness, Stream, Path) || Path.Num() < 2)
	{
		return;
	}

	const double Height = Stream.FRandRange(static_cast<float>(HeightMin), static_cast<float>(HeightMax));
	DiffuseFromSeeds(Graph, Path, Height, LinePower, Stream);

	if (OutPathSitePositionsLocalCm)
	{
		RecordPathPositions(Graph, Path, *OutPathSitePositionsLocalCm);
	}
}

bool FIHTerrainCellDiffusion::FindPathOnly(
	const FIHTerrainCellGraph& Graph, const int32 StartIdx, const int32 EndIdx,
	const double PathRandomness, FRandomStream& Stream, TArray<FVector2D>& OutPathSitePositionsLocalCm)
{
	using namespace IHTerrainCellDiffusionPrivate;

	OutPathSitePositionsLocalCm.Reset();
	if (!Graph.IsValidIndex(StartIdx) || !Graph.IsValidIndex(EndIdx) || StartIdx == EndIdx)
	{
		return false;
	}

	TArray<int32> Path;
	if (!FindPathBetweenCells(Graph, StartIdx, EndIdx, PathRandomness, Stream, Path) || Path.Num() < 2)
	{
		return false;
	}

	RecordPathPositions(Graph, Path, OutPathSitePositionsLocalCm);
	return true;
}

void FIHTerrainCellDiffusion::DiffuseAlongCells(
	FIHTerrainCellGraph& Graph, const TArray<int32>& SeedIndices, const double HeightMin,
	const double HeightMax, const double LinePower, FRandomStream& Stream,
	TArray<FVector2D>* OutPathSitePositionsLocalCm)
{
	using namespace IHTerrainCellDiffusionPrivate;

	if (OutPathSitePositionsLocalCm)
	{
		OutPathSitePositionsLocalCm->Reset();
	}

	if (SeedIndices.Num() == 0)
	{
		return;
	}

	const double Height = Stream.FRandRange(static_cast<float>(HeightMin), static_cast<float>(HeightMax));
	DiffuseFromSeeds(Graph, SeedIndices, Height, LinePower, Stream);

	if (OutPathSitePositionsLocalCm)
	{
		RecordPathPositions(Graph, SeedIndices, *OutPathSitePositionsLocalCm);
	}
}

int32 FIHTerrainCellDiffusion::PickCellNearPath(
	const FIHTerrainCellGraph& Graph, const TArray<FVector2D>& PathSitePositionsLocalCm,
	const double AlongFrac, const double LateralOffsetCm, FRandomStream& Stream)
{
	using namespace IHTerrainCellDiffusionPrivate;

	if (PathSitePositionsLocalCm.Num() == 0)
	{
		return INDEX_NONE;
	}
	if (PathSitePositionsLocalCm.Num() == 1)
	{
		return NearestCellToPoint(Graph, PathSitePositionsLocalCm[0]);
	}

	const double ClampedFrac = FMath::Clamp(AlongFrac, 0.0, 1.0);
	const double TargetParam = ClampedFrac * static_cast<double>(PathSitePositionsLocalCm.Num() - 1);
	const int32 SegIdx = FMath::Clamp(
		FMath::FloorToInt(static_cast<float>(TargetParam)), 0, PathSitePositionsLocalCm.Num() - 2);
	const double LocalT = TargetParam - SegIdx;

	const FVector2D A = PathSitePositionsLocalCm[SegIdx];
	const FVector2D B = PathSitePositionsLocalCm[SegIdx + 1];
	const FVector2D PointOnPath = FMath::Lerp(A, B, LocalT);

	FVector2D Tangent = (B - A).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector2D(1.0, 0.0);
	}
	const FVector2D Normal(-Tangent.Y, Tangent.X); // 2D perpendicular, no fixed left/right meaning
	const double JitteredOffset = LateralOffsetCm * Stream.FRandRange(0.9f, 1.1f);
	const FVector2D TargetPos = PointOnPath + Normal * JitteredOffset;

	return NearestCellToPoint(Graph, TargetPos);
}

void FIHTerrainCellDiffusion::Smooth(FIHTerrainCellGraph& Graph, const double Factor)
{
	if (Graph.Num() == 0 || Factor <= 0.0)
	{
		return;
	}

	TArray<double> Original;
	Original.SetNum(Graph.Num());
	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		Original[i] = Graph.Cells[i].Height;
	}

	const double ClampedFactor = FMath::Clamp(Factor, 0.0, 1.0);
	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		const TArray<int32>& Neighbors = Graph.Cells[i].Neighbors;
		if (Neighbors.Num() == 0)
		{
			continue;
		}
		double NeighborAvg = 0.0;
		for (const int32 NeighborIdx : Neighbors)
		{
			NeighborAvg += Original.IsValidIndex(NeighborIdx) ? Original[NeighborIdx] : Original[i];
		}
		NeighborAvg /= Neighbors.Num();
		Graph.Cells[i].Height = FMath::Lerp(Original[i], NeighborAvg, ClampedFactor);
	}
}

void FIHTerrainCellDiffusion::SmoothLandAware(
	FIHTerrainCellGraph& Graph, const double Factor, const double LandThreshold, const double OceanNeighborWeight)
{
	if (Graph.Num() == 0 || Factor <= 0.0)
	{
		return;
	}

	TArray<double> Original;
	Original.SetNum(Graph.Num());
	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		Original[i] = Graph.Cells[i].Height;
	}

	const double ClampedFactor = FMath::Clamp(Factor, 0.0, 1.0);
	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		const TArray<int32>& Neighbors = Graph.Cells[i].Neighbors;
		if (Neighbors.Num() == 0)
		{
			continue;
		}
		const bool bSelfIsLand = Original[i] >= LandThreshold;
		double NeighborAvg = 0.0;
		for (const int32 NeighborIdx : Neighbors)
		{
			const double NeighborHeight = Original.IsValidIndex(NeighborIdx) ? Original[NeighborIdx] : Original[i];
			// Land cell + sub-threshold (Ocean-side) neighbor: dampen that neighbor's pull toward
			// the cell's OWN height by OceanNeighborWeight, instead of fully substituting it (0.0
			// tried and reverted - froze raw coastal jitter into more fragments instead of letting
			// it consolidate). Every Land-Land pair still averages normally either way
			// (fragmentation-control fully preserved regardless of this weight).
			NeighborAvg += (bSelfIsLand && NeighborHeight < LandThreshold)
				? FMath::Lerp(Original[i], NeighborHeight, OceanNeighborWeight)
				: NeighborHeight;
		}
		NeighborAvg /= Neighbors.Num();
		Graph.Cells[i].Height = FMath::Lerp(Original[i], NeighborAvg, ClampedFactor);
	}
}

void FIHTerrainCellDiffusion::ClassifyLandWater(FIHTerrainCellGraph& Graph, const double LandThreshold)
{
	for (FIHTerrainCell& Cell : Graph.Cells)
	{
		Cell.Feature = (Cell.Height >= LandThreshold) ? EIHCellFeature::Land : EIHCellFeature::Ocean;
	}
}

void FIHTerrainCellDiffusion::ComputeCoastalMetadata(FIHTerrainCellGraph& Graph)
{
	const int32 NumCells = Graph.Num();
	if (NumCells == 0)
	{
		return;
	}

	// Reset.
	for (FIHTerrainCell& Cell : Graph.Cells)
	{
		Cell.CoastDistance = 0;
		Cell.bHaven = false;
		Cell.HarborCount = 0;
	}

	// Haven/harbor: any land cell adjacent to water is a haven; harbor count = adjacent water cells.
	// Also collect the reverse set (coastal water cells adjacent to land) as BFS seeds below —
	// bHaven is a Land-only concept (matches Azgaar's usage: it flags dock-eligible land, not
	// water), so it cannot double as the Ocean distance-BFS's seed predicate.
	TSet<int32> CoastalOceanSeedSet;
	for (FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Feature != EIHCellFeature::Land)
		{
			continue;
		}
		for (const int32 NeighborIdx : Cell.Neighbors)
		{
			if (Graph.IsValidIndex(NeighborIdx) && Graph.Cells[NeighborIdx].Feature != EIHCellFeature::Land)
			{
				Cell.bHaven = true;
				++Cell.HarborCount;
				CoastalOceanSeedSet.Add(NeighborIdx);
			}
		}
	}
	const TArray<int32> CoastalOceanSeeds = CoastalOceanSeedSet.Array();

	// Multi-source BFS distance bands, one pass per feature type, guaranteed O(NumCells) each
	// (standard visited-set BFS, same termination guarantee as DiffuseFromSeed above).
	auto RunDistanceBFS = [&Graph, NumCells](const TArray<int32>& SeedIndices,
		const EIHCellFeature PropagateFeature, const int32 Sign)
	{
		TArray<bool> Visited;
		Visited.Init(false, NumCells);
		TArray<int32> Queue;
		Queue.Reserve(NumCells);

		for (const int32 SeedIdx : SeedIndices)
		{
			if (Graph.IsValidIndex(SeedIdx) && !Visited[SeedIdx])
			{
				Visited[SeedIdx] = true;
				Queue.Add(SeedIdx);
			}
		}

		int32 Head = 0;
		int32 Hop = 1;
		int32 LevelEnd = Queue.Num();
		while (Head < Queue.Num())
		{
			if (Head == LevelEnd)
			{
				++Hop;
				LevelEnd = Queue.Num();
			}
			const int32 Cur = Queue[Head++];
			for (const int32 NeighborIdx : Graph.Cells[Cur].Neighbors)
			{
				if (!Graph.IsValidIndex(NeighborIdx) || Visited[NeighborIdx])
				{
					continue;
				}
				if (Graph.Cells[NeighborIdx].Feature != PropagateFeature)
				{
					continue;
				}
				Visited[NeighborIdx] = true;
				Graph.Cells[NeighborIdx].CoastDistance = Sign * Hop;
				Queue.Add(NeighborIdx);
			}
		}
	};

	TArray<int32> HavenLandSeeds;
	for (int32 i = 0; i < NumCells; ++i)
	{
		if (Graph.Cells[i].Feature == EIHCellFeature::Land && Graph.Cells[i].bHaven)
		{
			HavenLandSeeds.Add(i);
		}
	}

	RunDistanceBFS(HavenLandSeeds, EIHCellFeature::Land, +1);
	RunDistanceBFS(CoastalOceanSeeds, EIHCellFeature::Ocean, -1);
}

namespace IHTerrainCellDiffusionPrivate
{
	/**
	 * Traces the boundary of {Cell : InRegion(Cell) == true} into closed loops by walking shared
	 * Voronoi edges between in-region and out-of-region neighbor cells. Generalizes the original
	 * Land/Ocean-only coastline trace so the same robust edge-chaining machinery can also derive
	 * offset rings (e.g. a -25m shelf edge or +25m dry contour) from CoastDistance thresholds,
	 * without ever deforming/offsetting an existing polygon (see TraceCoastDistanceBoundaryLoops).
	 */
	static void TraceBoundaryLoops(
		const FIHTerrainCellGraph& Graph,
		TFunctionRef<bool(const FIHTerrainCell&)> InRegion,
		TArray<TArray<FVector2D>>& OutLoopsLocalCm)
	{
		OutLoopsLocalCm.Reset();

		// REVERTED (Do No Harm): a DCEL-style directed-edge rewrite (oriented via each cell's own
		// CCW winding, "next" edge chosen by smallest-CCW-sweep-from-reversed-incoming-direction)
		// was tried here. It passed all 4 automation tests AND left the already-healthy islands
		// (and Kazamaura's own main-ring numbers) unchanged - but self-testing against the real
		// PRAMS5 realm before handing it back caught a genuine NEW regression the test suite
		// didn't cover: a previously-healthy island's WWF shelf-ring trace
		// (TraceCoastDistanceBoundaryLoops, a different InRegion predicate through this same
		// function) went from 1 trivial discarded chain to 170, and that island's wwfAcres
		// collapsed from 18,694 to 1. Reverted immediately rather than ship it. The open question
		// (Kazamaura's coastline fragmenting into many small pieces instead of one dominant loop)
		// remains unsolved as of this revert - three attempts (undirected first-match, undirected
		// closest-turn, directed DCEL) have each been tested against real data and found either
		// ineffective or actively harmful; the next attempt needs to explain why a
		// mathematically-standard algorithm regressed a DIFFERENT predicate through the same
		// shared function before being tried live again.
		// IH-DEC-063 investigation (2026-09-01): widening this from 1.0 (~1cm) to 625.0 (~25cm, to
		// match WeldNearDuplicateVertices' own epsilon) was tried and self-tested - REVERTED. Zero
		// measurable effect (ABBEY3 island0's failure count was 104/34410 both before and after,
		// identical to the digit), disproving "near-duplicate vertex slightly over 1cm" outright.
		// A one-time diagnostic dump of the first 8 real failures (since removed) found TWO distinct
		// causes, not one: ~75% of the sample shared EXACTLY 1 boundary point at 0.0000cm (a genuine
		// single-point Voronoi pinch - 4+ cells meeting near one point instead of the usual 3 - which
		// structurally can never satisfy this function's >=2-point "shared edge" requirement, at ANY
		// tolerance, since there is no second point to find); the remainder shared ZERO points and
		// were tens of thousands of cm apart (60931.9cm, 8627.7cm) despite being listed as adjacent in
		// Cell.Neighbors - a separate, more concerning graph-topology question (why are non-adjacent
		// cells in each other's Neighbors list at all?) that needs its own investigation before this
		// function is touched again, given its documented history of regressions from confident-
		// seeming fixes (see the comment block above).
		//
		// Terrain Stamps pivot (2026-09-01), IH-DEC-063 continued: the single-point-pinch class above
		// is a MEASUREMENT bug, not a geometry bug - a genuine pinch has no real shared edge to trace
		// at any tolerance, so FindSharedEdge correctly contributes no segment for it either way; the
		// only thing wrong was counting/logging it alongside real failures, which made a ~26-real-case
		// anomaly (the non-adjacent-Neighbors class, still open, unfixed) look like a 104-case one.
		// Split the counters below so the still-open class stays visible and isolated, without
		// touching Segments/loop-chaining behavior at all - zero risk to the geometry this function's
		// own history warns against touching lightly. The non-adjacent-Neighbors class itself (why
		// Cell.Neighbors, built from Delaunay adjacency in IHTerrainCellGraphGenerator.cpp, can list a
		// site pair whose clipped Voronoi cells share no boundary point) is NOT fixed here - it needs
		// its own investigation with real self-test data, not a blind change to triangulation/clipping
		// code with no live verification.
		constexpr double SharedVertexToleranceCmSq = 1.0; // 1 cm^2 -> ~1 cm positional tolerance

		auto FindSharedEdge = [SharedVertexToleranceCmSq](
			const TArray<FVector2D>& BoundaryA, const TArray<FVector2D>& BoundaryB,
			FVector2D& OutP0, FVector2D& OutP1, int32& OutSharedPointCount) -> bool
		{
			TArray<FVector2D> Shared;
			for (const FVector2D& A : BoundaryA)
			{
				for (const FVector2D& B : BoundaryB)
				{
					if (FVector2D::DistSquared(A, B) <= SharedVertexToleranceCmSq)
					{
						Shared.Add(A);
						break;
					}
				}
				if (Shared.Num() >= 2)
				{
					break;
				}
			}
			OutSharedPointCount = Shared.Num();
			if (Shared.Num() < 2)
			{
				return false;
			}
			OutP0 = Shared[0];
			OutP1 = Shared[1];
			return true;
		};

		// Collect undirected boundary segments. Only the in-region side of a cross-boundary pair
		// ever becomes CellIdx (the Neighbor fails InRegion and is skipped by the outer continue
		// below), so each genuine coastline edge is visited exactly once already - no de-dup guard
		// needed (a prior `NeighborIdx <= CellIdx` guard here was a confirmed, since-fixed bug -
		// IH-DEC-035 - that silently dropped ~half of every coastline's boundary edges).
		TArray<TPair<FVector2D, FVector2D>> Segments;
		// Offline isolation diagnostic (no behavior change): parallel array recording which
		// (in-region cell, out-of-region neighbor) pair produced each segment, so a "dead end -
		// CONSUMED segment is 0.00cm away" collision can report whether the SAME cell appears on
		// both sides (a genuine single-cell self-touch) or two unrelated cells whose jittered
		// boundaries merely coincide at one point (a geometric artifact, not a topological pinch).
		TArray<TPair<int32, int32>> SegmentCellPairs;
		int32 FailedSharedEdgeCount = 0;
		int32 PinchPointSkipCount = 0; // genuine single-point Voronoi pinches - expected, not a failure
		int32 CrossBoundaryPairCount = 0;
		for (int32 CellIdx = 0; CellIdx < Graph.Num(); ++CellIdx)
		{
			const FIHTerrainCell& Cell = Graph.Cells[CellIdx];
			if (!InRegion(Cell))
			{
				continue;
			}
			for (const int32 NeighborIdx : Cell.Neighbors)
			{
				if (!Graph.IsValidIndex(NeighborIdx))
				{
					continue;
				}
				const FIHTerrainCell& Neighbor = Graph.Cells[NeighborIdx];
				if (InRegion(Neighbor))
				{
					continue;
				}
				++CrossBoundaryPairCount;
				FVector2D P0, P1;
				int32 SharedPointCount = 0;
				if (FindSharedEdge(Cell.Boundary, Neighbor.Boundary, P0, P1, SharedPointCount))
				{
					Segments.Add(TPair<FVector2D, FVector2D>(P0, P1));
					SegmentCellPairs.Add(TPair<int32, int32>(CellIdx, NeighborIdx));
				}
				else if (SharedPointCount == 1)
				{
					++PinchPointSkipCount;
				}
				else
				{
					++FailedSharedEdgeCount;
				}
			}
		}
		if (FailedSharedEdgeCount > 0)
		{
			// Real anomaly, still open (IH-DEC-063): zero shared boundary points despite being listed
			// in each other's Cell.Neighbors - a Delaunay-adjacency/Voronoi-clipping question in
			// IHTerrainCellGraphGenerator.cpp, not something this function can or should paper over.
			UE_LOG(LogTemp, Warning,
				TEXT("IHTerrainCellDiffusion::TraceBoundaryLoops: FindSharedEdge FAILED for %d/%d cross-boundary cell pairs (genuine coastline edges silently dropped; %d additional single-point Voronoi pinches skipped harmlessly, not counted here)"),
				FailedSharedEdgeCount, CrossBoundaryPairCount, PinchPointSkipCount);
		}


		TArray<bool> Consumed;
		Consumed.Init(false, Segments.Num());
		// Offline isolation diagnostic: records which StartSeg (loop attempt) consumed each
		// segment, so a dead end can report whether the segment it actually needed was already
		// claimed by a DIFFERENT, earlier-processed loop - confirming or refuting the hypothesis
		// that a small competing loop is "stealing" a shared segment from a much larger, more
		// legitimate trace before it gets there.
		TArray<int32> ConsumedByStartSeg;
		ConsumedByStartSeg.Init(INDEX_NONE, Segments.Num());

		// At a branch point where 3+ unconsumed segments share an endpoint, pick the one whose
		// outgoing direction most closely continues the incoming direction (largest dot product
		// of unit vectors - smallest absolute turn angle). Confirmed via live self-test to be a
		// no-op for the one island it was meant to fix (byte-identical main-ring output before
		// and after), but also confirmed to introduce no regression elsewhere - kept as the
		// current safe baseline while the real fix is still open.
		//
		// A segment whose own two endpoints are within SharedVertexToleranceCmSq of each other (a
		// "degenerate" near-zero-length segment - confirmed via live branch-point diagnostics on
		// PRAMS5 to occur at cell corners, e.g. Kazamaura's cell 34594) has a numerically unstable
		// outgoing direction: it's essentially noise, not a meaningful heading. Deleting such
		// segments was tried and broke loop closure on a test island (BasicIslandShape) - the
		// segment is still real, necessary connectivity, just too short to trust for tie-breaking.
		// Instead, deprioritize it: a non-degenerate candidate always wins over a degenerate one
		// regardless of raw dot product, and among same-tier candidates a near-exact dot tie is
		// broken by preferring the longer (more numerically stable) segment.
		constexpr double DegenerateSegmentLengthCmSq = 2500.0; // 50cm - real coastline segments run
																// to tens of meters at this cell
																// size; a degenerate one measures
																// centimeters.
		auto FindConnectingSegment = [&Segments, &Consumed, &SegmentCellPairs](
			const FVector2D& EndPoint, const FVector2D& IncomingDir, int32& OutSegIdx, bool& bOutReversed) -> bool
		{
			int32 BestSegIdx = INDEX_NONE;
			bool bBestReversed = false;
			bool bBestDegenerate = true; // anything found beats "no candidate", so start pessimistic
			double BestDot = -2.0; // unit-vector dot products lie in [-1,1]; anything found beats this
			double BestLengthSq = 0.0;
			// Offline isolation diagnostic (no behavior change): collect every LIVE candidate that
			// touches EndPoint, not just the winner, so a genuine branch point (2+ unconsumed
			// candidates competing for the same continuation) can be inspected directly - which one
			// the dot-product tie-break picked, and what the alternatives were. Trivial single-
			// candidate steps (the overwhelming majority) are not logged.
			struct FBranchCandidate { int32 SegIdx; double Dot; TPair<int32, int32> Cells; double LengthSq; };
			TArray<FBranchCandidate> BranchCandidates;
			for (int32 i = 0; i < Segments.Num(); ++i)
			{
				if (Consumed[i])
				{
					continue;
				}
				const double LengthSq = FVector2D::DistSquared(Segments[i].Key, Segments[i].Value);
				const bool bDegenerate = LengthSq < DegenerateSegmentLengthCmSq;
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const FVector2D& CandidateEnd = (Side == 0) ? Segments[i].Key : Segments[i].Value;
					if (FVector2D::DistSquared(CandidateEnd, EndPoint) > SharedVertexToleranceCmSq)
					{
						continue;
					}
					const FVector2D& OtherEnd = (Side == 0) ? Segments[i].Value : Segments[i].Key;
					const FVector2D OutgoingDir = (OtherEnd - CandidateEnd).GetSafeNormal();
					const double Dot = FVector2D::DotProduct(IncomingDir, OutgoingDir);
					BranchCandidates.Add(FBranchCandidate{i, Dot, SegmentCellPairs[i], LengthSq});

					bool bIsBetter;
					if (BestSegIdx == INDEX_NONE)
					{
						bIsBetter = true;
					}
					else if (bDegenerate != bBestDegenerate)
					{
						bIsBetter = !bDegenerate; // non-degenerate always beats degenerate
					}
					else if (!FMath::IsNearlyEqual(Dot, BestDot, 1e-3))
					{
						bIsBetter = Dot > BestDot;
					}
					else
					{
						bIsBetter = LengthSq > BestLengthSq; // near-exact dot tie -> longer (more stable) wins
					}

					if (bIsBetter)
					{
						BestDot = Dot;
						BestSegIdx = i;
						bBestReversed = (Side == 1); // Value matched -> walk Value->Key (existing convention)
						bBestDegenerate = bDegenerate;
						BestLengthSq = LengthSq;
					}
				}
			}
			if (BranchCandidates.Num() >= 2)
			{
				FString Dump;
				for (const FBranchCandidate& C : BranchCandidates)
				{
					Dump += FString::Printf(TEXT("[seg=%d cells=(%d,%d) dot=%.6f lenSq=%.4f%s] "),
						C.SegIdx, C.Cells.Key, C.Cells.Value, C.Dot, C.LengthSq,
						(C.SegIdx == BestSegIdx) ? TEXT("*CHOSEN*") : TEXT(""));
				}
				UE_LOG(LogTemp, Warning,
					TEXT("IHTerrainCellDiffusion::TraceBoundaryLoops: BRANCH at (%.1f,%.1f) - %d live candidate(s): %s"),
					EndPoint.X, EndPoint.Y, BranchCandidates.Num(), *Dump);
			}
			if (BestSegIdx == INDEX_NONE)
			{
				return false;
			}
			OutSegIdx = BestSegIdx;
			bOutReversed = bBestReversed;
			return true;
		};

		int32 ClosedLoopCount = 0;
		int32 DiscardedOpenChainCount = 0;
		int64 DiscardedOpenChainVerts = 0;
		// Offline isolation diagnostic (no behavior change): capture the first few discarded
		// chains' FULL vertex paths, so a genuinely pathological case (many discards, not the
		// usual 1-trivial-discard noise) can be reconstructed and reasoned about directly with
		// real coordinates instead of another live "theory -> implement -> test the whole system"
		// cycle. Only ever logged, never affects OutLoopsLocalCm.
		TArray<TArray<FVector2D>> SampleDiscardedChains;
		for (int32 StartSeg = 0; StartSeg < Segments.Num(); ++StartSeg)
		{
			if (Consumed[StartSeg])
			{
				continue;
			}
			TArray<FVector2D> Loop;
			Consumed[StartSeg] = true;
			ConsumedByStartSeg[StartSeg] = StartSeg;
			Loop.Add(Segments[StartSeg].Key);
			FVector2D Cursor = Segments[StartSeg].Value;
			Loop.Add(Cursor);
			FVector2D IncomingDir = (Segments[StartSeg].Value - Segments[StartSeg].Key).GetSafeNormal();
			int32 LastSegIdx = StartSeg; // which segment currently ends at Cursor - for the cell-pair diagnostic below

			const FVector2D LoopStart = Loop[0];
			bool bClosed = false;
			for (int32 Step = 0; Step < Segments.Num(); ++Step) // hard bound: at most one hop per remaining segment
			{
				if (FVector2D::DistSquared(Cursor, LoopStart) <= SharedVertexToleranceCmSq && Loop.Num() > 2)
				{
					bClosed = true;
					break; // loop closed
				}
				int32 NextSeg = INDEX_NONE;
				bool bReversed = false;
				if (!FindConnectingSegment(Cursor, IncomingDir, NextSeg, bReversed))
				{
					// Offline isolation diagnostic: if a genuinely CONSUMED segment sits right at
					// this dead end, log which earlier loop (StartSeg) claimed it - direct evidence
					// for/against the "small loop steals a segment the real trace needed" theory.
					double NearestConsumedGapCm = TNumericLimits<double>::Max();
					int32 NearestConsumedStartSeg = INDEX_NONE;
					int32 NearestConsumedSegIdx = INDEX_NONE;
					for (int32 i = 0; i < Segments.Num(); ++i)
					{
						if (!Consumed[i] || i == StartSeg)
						{
							continue;
						}
						const double DKey = FVector2D::Distance(Segments[i].Key, Cursor);
						const double DVal = FVector2D::Distance(Segments[i].Value, Cursor);
						const double D = FMath::Min(DKey, DVal);
						if (D < NearestConsumedGapCm)
						{
							NearestConsumedGapCm = D;
							NearestConsumedStartSeg = ConsumedByStartSeg[i];
							NearestConsumedSegIdx = i;
						}
					}
					if (NearestConsumedGapCm < 1000.0) // only log if plausibly the "missing" continuation (<10m)
					{
						// Cell-pair diagnostic: does the SAME cell appear on both sides of this
						// collision (a genuine single-cell self-touch, e.g. a saddle pinch), or are
						// the two segments produced by entirely unrelated cells (a geometric
						// coincidence - most likely the Perlin boundary-vertex jitter pushing two
						// unrelated cells' corners onto the same point) that a graph-topology fix
						// (like pinch-widening) could never address?
						const TPair<int32, int32>& HereCells = SegmentCellPairs[LastSegIdx];
						const TPair<int32, int32>& ThereCells = SegmentCellPairs[NearestConsumedSegIdx];
						const bool bSharesCell =
							HereCells.Key == ThereCells.Key || HereCells.Key == ThereCells.Value ||
							HereCells.Value == ThereCells.Key || HereCells.Value == ThereCells.Value;
						UE_LOG(LogTemp, Warning,
							TEXT("IHTerrainCellDiffusion::TraceBoundaryLoops: dead end at loop StartSeg=%d (len=%d) - a CONSUMED segment is %.2fcm away, claimed by loop StartSeg=%d - hereCells=(%d,%d) thereCells=(%d,%d) sharesCell=%s"),
							StartSeg, Loop.Num(), NearestConsumedGapCm, NearestConsumedStartSeg,
							HereCells.Key, HereCells.Value, ThereCells.Key, ThereCells.Value,
							bSharesCell ? TEXT("YES") : TEXT("NO"));
					}
					break; // open chain - discarded below, never emitted as a fake closed loop
				}
				Consumed[NextSeg] = true;
				ConsumedByStartSeg[NextSeg] = StartSeg;
				const FVector2D NextPoint = bReversed ? Segments[NextSeg].Key : Segments[NextSeg].Value;
				IncomingDir = (NextPoint - Cursor).GetSafeNormal();
				Cursor = NextPoint;
				Loop.Add(Cursor);
				LastSegIdx = NextSeg;
			}

			if (bClosed && Loop.Num() >= 3)
			{
				Loop.Pop(EAllowShrinking::No); // drop duplicate closing vertex (== LoopStart)
				OutLoopsLocalCm.Add(MoveTemp(Loop));
				++ClosedLoopCount;
			}
			else if (!bClosed)
			{
				++DiscardedOpenChainCount;
				DiscardedOpenChainVerts += Loop.Num();
				if (SampleDiscardedChains.Num() < 3)
				{
					SampleDiscardedChains.Add(Loop);
				}
			}
		}

		if (DiscardedOpenChainCount > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("IHTerrainCellDiffusion::TraceBoundaryLoops: segments=%d closedLoops=%d discardedOpenChains=%d discardedOpenChainVerts=%lld"),
				Segments.Num(), ClosedLoopCount, DiscardedOpenChainCount, DiscardedOpenChainVerts);
		}

		// Only dump full chain/cell detail for a genuinely pathological case (many discards, not
		// the usual 1-trivial-discard noise every island shows) - keeps this diagnostic silent in
		// the common case.
		if (DiscardedOpenChainCount > 5)
		{
			for (int32 ChainIdx = 0; ChainIdx < SampleDiscardedChains.Num(); ++ChainIdx)
			{
				const TArray<FVector2D>& Chain = SampleDiscardedChains[ChainIdx];
				FString PathStr;
				for (const FVector2D& P : Chain)
				{
					PathStr += FString::Printf(TEXT("(%.1f,%.1f) "), P.X, P.Y);
				}
				UE_LOG(LogTemp, Warning,
					TEXT("IHTerrainCellDiffusion::TraceBoundaryLoops: discardedChain[%d] verts=%d path=%s"),
					ChainIdx, Chain.Num(), *PathStr);

				// Reverse-lookup: for the chain's start and end points, find the nearest few cells
				// by SitePos (index, position, feature) - identifies the actual local geometry at
				// the point where this chain begins/dies, without needing a separate PIE session.
				auto LogNearbyCells = [&Graph](const FVector2D& Point, const TCHAR* Label)
				{
					TArray<TPair<double, int32>> Dists;
					Dists.Reserve(Graph.Num());
					for (int32 i = 0; i < Graph.Num(); ++i)
					{
						Dists.Add(TPair<double, int32>(FVector2D::Distance(Graph.Cells[i].SitePos, Point), i));
					}
					Dists.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
					const int32 TopN = FMath::Min(4, Dists.Num());
					for (int32 i = 0; i < TopN; ++i)
					{
						const int32 CellIdx = Dists[i].Value;
						const FIHTerrainCell& Cell = Graph.Cells[CellIdx];
						const TCHAR* FeatureStr = Cell.Feature == EIHCellFeature::Land ? TEXT("Land")
							: Cell.Feature == EIHCellFeature::Ocean ? TEXT("Ocean")
							: Cell.Feature == EIHCellFeature::Lake ? TEXT("Lake") : TEXT("Unclassified");
						UE_LOG(LogTemp, Warning,
							TEXT("IHTerrainCellDiffusion::TraceBoundaryLoops:   %s nearCell[%d] idx=%d sitePos=(%.1f,%.1f) distCm=%.1f feature=%s neighbors=%d boundaryVerts=%d"),
							Label, i, CellIdx, Cell.SitePos.X, Cell.SitePos.Y, Dists[i].Key, FeatureStr,
							Cell.Neighbors.Num(), Cell.Boundary.Num());
					}
				};
				if (Chain.Num() > 0)
				{
					LogNearbyCells(Chain[0], TEXT("ChainStart"));
					LogNearbyCells(Chain.Last(), TEXT("ChainEnd"));
				}
			}
		}
	}
}

void FIHTerrainCellDiffusion::TraceCoastlineLoops(
	const FIHTerrainCellGraph& Graph, TArray<TArray<FVector2D>>& OutLoopsLocalCm)
{
	IHTerrainCellDiffusionPrivate::TraceBoundaryLoops(
		Graph,
		[](const FIHTerrainCell& Cell) { return Cell.Feature == EIHCellFeature::Land; },
		OutLoopsLocalCm);
}

void FIHTerrainCellDiffusion::TraceCoastDistanceBoundaryLoops(
	const FIHTerrainCellGraph& Graph, const int32 MinCoastDistance, TArray<TArray<FVector2D>>& OutLoopsLocalCm)
{
	IHTerrainCellDiffusionPrivate::TraceBoundaryLoops(
		Graph,
		[MinCoastDistance](const FIHTerrainCell& Cell) { return Cell.CoastDistance >= MinCoastDistance; },
		OutLoopsLocalCm);
}

void FIHTerrainCellDiffusion::TraceHeightIsolineLoops(
	const FIHTerrainCellGraph& Graph, const double TargetHeightRaw, TArray<TArray<FVector2D>>& OutLoopsLocalCm)
{
	IHTerrainCellDiffusionPrivate::TraceBoundaryLoops(
		Graph,
		[TargetHeightRaw](const FIHTerrainCell& Cell) { return Cell.Height >= TargetHeightRaw; },
		OutLoopsLocalCm);
}
