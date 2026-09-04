// Copyright Invisible Hand. All Rights Reserved.
#include "IHTerrainCellGraphGenerator.h"

#include "CompGeom/Delaunay2.h"
#include "BoxTypes.h"
#include "Spatial/PointHashGrid2.h"

using namespace UE::Geometry;

namespace IHTerrainCellGraphGeneratorPrivate
{
	/** Distinct salt so this point stream never aliases another subsystem's seed derivation (IH DDU, IH-DEC-022). */
	constexpr int32 CellGraphSeedSalt = 0x43656C6C; // 'Cell'

	/**
	 * Azgaar-style single jitter pass over a regular grid — no relaxation, preserves organic
	 * irregularity. Enforces a minimum spacing between grid-adjacent sites: without it, two
	 * jittered sites can occasionally land pathologically close together, producing thin/sliver
	 * Delaunay triangles whose circumcenters (the resulting Voronoi cell boundary vertices) can
	 * land a fraction of a centimeter apart instead of exactly coinciding — confirmed root cause
	 * of a real coastline-tracing failure this session (a "High" profile island's coastline
	 * fragmented into ~30 disconnected pieces because of one such near-zero-length boundary edge).
	 * Since JitterFraction < 0.5, a site can never cross into a non-adjacent grid slot's
	 * territory, so only the 8 grid-adjacent slots can ever be close enough to matter — checking
	 * the 4 already-placed ones (left, upper-left, upper, upper-right) in this raster (Y outer,
	 * X inner) traversal order covers all 8 adjacency directions with no gaps, no spatial data
	 * structure needed.
	 */
	static void GenerateJitteredGridSites(
		const FVector2D& Min, const FVector2D& Max, const double Spacing,
		FRandomStream& Stream, TArray<FVector2D>& OutSites, int32& OutFallbackCount)
	{
		OutSites.Reset();
		OutFallbackCount = 0;
		constexpr double JitterFraction = 0.45;
		// Fraction of Spacing below which a jittered site is rejected and re-rolled - well below
		// the ~0.55*Spacing worst-case distance to an adjacent zero-jitter fallback neighbor, so
		// this only rejects the pathological tail (near-simultaneous max-jitter-toward-each-other
		// draws on both axes), not typical jitter.
		constexpr double MinSpacingFraction = 0.15;
		constexpr int32 MaxJitterAttempts = 8;
		const double MinSiteSpacingCmSq = FMath::Square(MinSpacingFraction * Spacing);

		int32 NumCols = -1; // fixed after row 0 - X-loop bounds/spacing never depend on Y
		int32 RowIndex = 0;
		for (double Y = Min.Y; Y <= Max.Y; Y += Spacing)
		{
			int32 ColIndex = 0;
			for (double X = Min.X; X <= Max.X; X += Spacing)
			{
				const int32 RowAboveStart = (RowIndex > 0 && NumCols > 0) ? (RowIndex - 1) * NumCols : INDEX_NONE;
				const int32 LeftIdx       = (ColIndex > 0) ? OutSites.Num() - 1 : INDEX_NONE;
				const int32 UpperLeftIdx  = (RowAboveStart != INDEX_NONE && ColIndex > 0) ? RowAboveStart + ColIndex - 1 : INDEX_NONE;
				const int32 UpperIdx      = (RowAboveStart != INDEX_NONE) ? RowAboveStart + ColIndex : INDEX_NONE;
				const int32 UpperRightIdx = (RowAboveStart != INDEX_NONE && ColIndex + 1 < NumCols) ? RowAboveStart + ColIndex + 1 : INDEX_NONE;
				const int32 NeighborIdx[4] = { LeftIdx, UpperLeftIdx, UpperIdx, UpperRightIdx };

				FVector2D Candidate;
				for (int32 Attempt = 0; Attempt < MaxJitterAttempts; ++Attempt)
				{
					const double JitterX = Stream.FRandRange(-JitterFraction, JitterFraction) * Spacing;
					const double JitterY = Stream.FRandRange(-JitterFraction, JitterFraction) * Spacing;
					Candidate = FVector2D(X + JitterX, Y + JitterY);

					bool bOk = true;
					for (const int32 NIdx : NeighborIdx)
					{
						if (NIdx != INDEX_NONE && FVector2D::DistSquared(Candidate, OutSites[NIdx]) < MinSiteSpacingCmSq)
						{
							bOk = false;
							break;
						}
					}
					if (bOk)
					{
						break;
					}
					if (Attempt == MaxJitterAttempts - 1)
					{
						Candidate = FVector2D(X, Y); // exhausted retries - safe fallback, see header comment
						++OutFallbackCount;
					}
				}
				OutSites.Add(Candidate);
				++ColIndex;
			}
			if (RowIndex == 0)
			{
				NumCols = ColIndex;
			}
			++RowIndex;
		}
	}

	/**
	 * Welds boundary vertices that are near-duplicates (within WeldEpsilonCm) into a single shared
	 * canonical position, globally and consistently across every cell that references it - fixes
	 * sliver-triangle circumcenters landing a fraction of a cm apart (see GenerateJitteredGridSites
	 * comment above for the full root-cause story). Union-find clustering keyed by lowest global
	 * vertex index (deterministic, no summation/iteration-order dependency) using TPointHashGrid2
	 * for proximity queries - its bucket sweep scans every grid cell overlapping the query radius,
	 * so a near-miss straddling a bucket boundary is still found (a naive single-bucket quantize-
	 * and-snap would reintroduce the exact rounding-flip failure mode that caused this bug).
	 */
	static void WeldNearDuplicateVertices(TArray<FIHTerrainCell>& Cells, const double WeldEpsilonCm)
	{
		struct FVertRef { int32 CellIdx; int32 VertIdx; };
		TArray<FVector2D> GlobalPos;
		TArray<FVertRef> GlobalOwner;
		for (int32 CellIdx = 0; CellIdx < Cells.Num(); ++CellIdx)
		{
			for (int32 VertIdx = 0; VertIdx < Cells[CellIdx].Boundary.Num(); ++VertIdx)
			{
				GlobalPos.Add(Cells[CellIdx].Boundary[VertIdx]);
				GlobalOwner.Add(FVertRef{ CellIdx, VertIdx });
			}
		}
		const int32 NumVerts = GlobalPos.Num();
		if (NumVerts == 0)
		{
			return;
		}

		TArray<int32> Parent;
		Parent.SetNumUninitialized(NumVerts);
		for (int32 i = 0; i < NumVerts; ++i)
		{
			Parent[i] = i;
		}
		TFunction<int32(int32)> Find = [&Parent, &Find](int32 X) -> int32
		{
			while (Parent[X] != X)
			{
				Parent[X] = Parent[Parent[X]]; // path halving
				X = Parent[X];
			}
			return X;
		};
		auto Union = [&Parent, &Find](int32 A, int32 B)
		{
			const int32 RootA = Find(A);
			const int32 RootB = Find(B);
			if (RootA != RootB)
			{
				Parent[FMath::Max(RootA, RootB)] = FMath::Min(RootA, RootB);
			}
		};

		// Query BEFORE insert each iteration so a vertex never matches itself.
		TPointHashGrid2d<int32> Grid(WeldEpsilonCm, INDEX_NONE);
		Grid.Reserve(NumVerts);
		for (int32 i = 0; i < NumVerts; ++i)
		{
			const TPair<int32, double> Nearest = Grid.FindNearestInRadius(
				GlobalPos[i], WeldEpsilonCm,
				[&GlobalPos, i](const int32& Other) { return FVector2D::DistSquared(GlobalPos[i], GlobalPos[Other]); });
			if (Nearest.Key != INDEX_NONE)
			{
				Union(i, Nearest.Key);
			}
			Grid.InsertPointUnsafe(i, GlobalPos[i]); // single-threaded here - no lock needed
		}

		for (int32 i = 0; i < NumVerts; ++i)
		{
			const int32 Root = Find(i);
			const FVertRef& Owner = GlobalOwner[i];
			Cells[Owner.CellIdx].Boundary[Owner.VertIdx] = GlobalPos[Root];
		}

		// Collapse now-EXACT-duplicate consecutive vertices (only ones that welded to the
		// identical canonical position as their neighbor - never near-duplicates that stayed
		// distinct, which are legitimate close-but-different corners).
		for (FIHTerrainCell& Cell : Cells)
		{
			TArray<FVector2D>& B = Cell.Boundary;
			for (int32 i = B.Num() - 1; i >= 0 && B.Num() >= 1; --i)
			{
				const int32 Prev = (i == 0) ? B.Num() - 1 : i - 1;
				if (B.Num() > 1 && B[i] == B[Prev])
				{
					B.RemoveAt(i);
				}
			}
		}
	}
}

bool FIHTerrainCellGraphGenerator::BuildGraph(const FBuildParams& Params, FIHTerrainCellGraph& OutGraph)
{
	using namespace IHTerrainCellGraphGeneratorPrivate;

	OutGraph = FIHTerrainCellGraph();

	if (Params.HalfExtentXCm <= 0.0 || Params.HalfExtentYCm <= 0.0 || Params.TargetCellWidthCm <= 0.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("IHTerrainCellGraphGenerator: invalid build params (extents/cell width must be positive)"));
		return false;
	}

	const FVector2D BoundsMin(
		Params.CenterLocalCm.X - Params.HalfExtentXCm,
		Params.CenterLocalCm.Y - Params.HalfExtentYCm);
	const FVector2D BoundsMax(
		Params.CenterLocalCm.X + Params.HalfExtentXCm,
		Params.CenterLocalCm.Y + Params.HalfExtentYCm);

	FRandomStream PointStream(
		static_cast<int32>(HashCombine(
			GetTypeHash(Params.MasterSeed),
			HashCombine(GetTypeHash(Params.IslandIndex), GetTypeHash(CellGraphSeedSalt)))));

	TArray<FVector2D> Sites;
	int32 JitterFallbackCount = 0;
	GenerateJitteredGridSites(BoundsMin, BoundsMax, Params.TargetCellWidthCm, PointStream, Sites, JitterFallbackCount);
	if (JitterFallbackCount > 0)
	{
		// Diagnostic (frame-artifact investigation): each fallback leaves a site at its exact
		// unjittered grid position. A lone fallback is harmless, but consecutive fallbacks along
		// the same row/column (more likely once one exact-grid point exists, since it's an easy
		// collision target for its raster-order neighbors) leave a short straight run of
		// grid-aligned, near-degenerate Voronoi cells - axis-aligned because the grid itself is
		// axis-aligned in local space, which would explain a straight "frame" line that persists
		// across completely different mesh-generation code and rotates/moves with the island
		// (Cell.Boundary, not the mesh-building logic, would be the true source).
		UE_LOG(LogTemp, Warning,
			TEXT("IHTerrainCellGraphGenerator: %d/%d sites hit the unjittered-grid-position fallback ")
			TEXT("(MaxJitterAttempts exhausted) - island=%d masterSeed=%d"),
			JitterFallbackCount, Sites.Num(), Params.IslandIndex, Params.MasterSeed);
	}

	if (Sites.Num() < 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("IHTerrainCellGraphGenerator: only %d sites generated, need >=4 for triangulation"), Sites.Num());
		return false;
	}

	FDelaunay2 Delaunay;
	Delaunay.RandomStream = FRandomStream(PointStream.GetCurrentSeed());
	const bool bTriangulated = Delaunay.Triangulate(TArrayView<const FVector2D>(Sites));
	if (!bTriangulated && !Delaunay.CanComputeVoronoiCells())
	{
		UE_LOG(LogTemp, Warning, TEXT("IHTerrainCellGraphGenerator: Delaunay triangulation failed, result=%d"),
			static_cast<int32>(Delaunay.GetResult()));
		return false;
	}

	const FAxisAlignedBox2d ClipBounds(BoundsMin, BoundsMax);
	TArray<TArray<FVector2D>> VoronoiCells = Delaunay.GetVoronoiCells(
		TArrayView<const FVector2D>(Sites), /*bIncludeBoundary=*/true, ClipBounds, /*ExpandBounds=*/0.0);

	if (VoronoiCells.Num() != Sites.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("IHTerrainCellGraphGenerator: Voronoi cell count %d != site count %d"),
			VoronoiCells.Num(), Sites.Num());
		return false;
	}

	OutGraph.Cells.SetNum(Sites.Num());
	for (int32 i = 0; i < Sites.Num(); ++i)
	{
		OutGraph.Cells[i].SitePos = Sites[i];
		OutGraph.Cells[i].Boundary = MoveTemp(VoronoiCells[i]);
	}

	constexpr double WeldEpsilonCm = 25.0; // ~32x the observed 0.78cm pathological gap; well under
											// TraceBoundaryLoops's existing 50cm "degenerate segment"
											// threshold, and far below real corner spacing at
											// typical (7500cm) cell width.
	WeldNearDuplicateVertices(OutGraph.Cells, WeldEpsilonCm);

	// Adjacency: two sites are Voronoi-cell neighbors iff their Delaunay triangulation has a
	// direct edge between them (Voronoi is the dual of Delaunay) — derived from the same
	// triangulation, no extra library or pass needed.
	TArray<FIndex3i> Triangles;
	TArray<FIndex3i> TriAdjacency;
	Delaunay.GetTrianglesAndAdjacency(Triangles, TriAdjacency);

	TArray<TSet<int32>> NeighborSets;
	NeighborSets.SetNum(Sites.Num());
	for (const FIndex3i& Tri : Triangles)
	{
		const int32 TriVerts[3] = { Tri.A, Tri.B, Tri.C };
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			const int32 A = TriVerts[Edge];
			const int32 B = TriVerts[(Edge + 1) % 3];
			if (Sites.IsValidIndex(A) && Sites.IsValidIndex(B) && A != B)
			{
				NeighborSets[A].Add(B);
				NeighborSets[B].Add(A);
			}
		}
	}

	// 2026-09-04 (IH-DEC pending "Fix 4"): drop degenerate long-range "neighbor" edges before they
	// ever reach TraceBoundaryLoops/FindSharedEdge or the water-component BFS. Root-caused this
	// session via temporary diagnostic instrumentation in TraceBoundaryLoops (real failing pairs'
	// site positions dumped, not guessed): real FindSharedEdge failures on a live ALERT4 self-test
	// showed inter-site distances 5-88x the graph's own average cell spacing, with pathologically
	// bunched pairs sharing a near-constant coordinate on one axis - classic Delaunay-triangulation
	// degeneracy on near-collinear site clusters, not a clip-boundary/tolerance issue (the two
	// widened-tolerance attempts already tried and reverted for IH-DEC-063 had zero effect for
	// exactly this reason - tolerance can't fix a topologically wrong edge). A genuine Voronoi
	// neighbor pair in this roughly-uniform jittered grid is never more than ~2x the average
	// spacing apart (adjacent, or diagonal-adjacent); only clearly-anomalous long edges are culled
	// here - the threshold is deliberately conservative (ratios up to ~4x left untouched) since the
	// real data showed a genuinely ambiguous zone in the 1.4-4x range this pass doesn't try to
	// resolve blind. This only prunes Cell.Neighbors itself, upstream of every consumer
	// (TraceBoundaryLoops, the water-component BFS, and trough/hill pathfinding) - it never touches
	// FindSharedEdge/FindConnectingSegment's own loop-chaining logic, the part of this pipeline
	// with the documented history of regressions (see IHTerrainCellDiffusion.cpp's "Do No Harm"
	// comment). Self-tested against both coastline-loop health and wwfAcres/shelf-ring health
	// before shipping, per that same lesson.
	const FVector2D FilterBoundsSize = BoundsMax - BoundsMin;
	const double ApproxCellSpacingCm = Sites.Num() > 0
		? FMath::Sqrt((FilterBoundsSize.X * FilterBoundsSize.Y) / static_cast<double>(Sites.Num()))
		: 0.0;
	constexpr double MaxNeighborDistRatio = 4.0;
	const double MaxNeighborDistCm = ApproxCellSpacingCm * MaxNeighborDistRatio;
	const double MaxNeighborDistSqCm = MaxNeighborDistCm * MaxNeighborDistCm;
	int32 PrunedLongEdgeCount = 0;
	for (int32 i = 0; i < Sites.Num(); ++i)
	{
		TArray<int32> Filtered;
		Filtered.Reserve(NeighborSets[i].Num());
		for (const int32 NeighborIdx : NeighborSets[i])
		{
			if (MaxNeighborDistCm <= 0.0 || FVector2D::DistSquared(Sites[i], Sites[NeighborIdx]) <= MaxNeighborDistSqCm)
			{
				Filtered.Add(NeighborIdx);
			}
			else
			{
				++PrunedLongEdgeCount;
			}
		}
		OutGraph.Cells[i].Neighbors = MoveTemp(Filtered);
	}
	if (PrunedLongEdgeCount > 0)
	{
		// Each bad edge is stored on both endpoints, so divide by 2 for the real edge count.
		UE_LOG(LogTemp, Log,
			TEXT("IHTerrainCellGraphGenerator: pruned %d degenerate long-range Neighbors edge(s) (>%.0fcm, %.1fx approxCellSpacing=%.1fcm) island=%d"),
			PrunedLongEdgeCount / 2, MaxNeighborDistCm, MaxNeighborDistRatio, ApproxCellSpacingCm, Params.IslandIndex);
	}

	OutGraph.BoundsMinLocalCm = BoundsMin;
	OutGraph.BoundsMaxLocalCm = BoundsMax;

	UE_LOG(LogTemp, Log, TEXT("IHTerrainCellGraphGenerator: built graph cells=%d island=%d seed=%d"),
		OutGraph.Num(), Params.IslandIndex, Params.MasterSeed);

	return true;
}
