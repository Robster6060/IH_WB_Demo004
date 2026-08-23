// Copyright Invisible Hand. All Rights Reserved.
// Prototypes and programmatically validates the compound nested-inlet diffusion recipe (layered
// AddHill + major-bay/sub-inlet AddRange passes + a guaranteed-nesting AddRangeBetweenCells bias
// pass, see the World Builder cell-graph pivot plan) entirely on the cell graph -- no mesh/actor
// involvement, matching the isolation discipline of the other TerrainCellGraph tests.

#include "Misc/AutomationTest.h"
#include "IHTerrainCellGraphGenerator.h"
#include "IHTerrainCellDiffusion.h"

namespace IHNestedInletTestPrivate
{
	/** Resamples a closed loop's centroid-relative radius onto N evenly arc-length-spaced samples. */
	static void ResampleRadiusByArcLength(const TArray<FVector2D>& Loop, const int32 NumSamples, TArray<double>& OutRadius)
	{
		OutRadius.Reset();
		const int32 N = Loop.Num();
		if (N < 3 || NumSamples < 3)
		{
			return;
		}

		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& P : Loop)
		{
			Centroid += P;
		}
		Centroid /= static_cast<double>(N);

		TArray<double> CumulativeLength;
		CumulativeLength.SetNum(N + 1);
		CumulativeLength[0] = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Loop[i];
			const FVector2D& B = Loop[(i + 1) % N];
			CumulativeLength[i + 1] = CumulativeLength[i] + FVector2D::Distance(A, B);
		}
		const double TotalLength = CumulativeLength[N];
		if (TotalLength <= 0.0)
		{
			return;
		}

		OutRadius.SetNum(NumSamples);
		int32 SegIdx = 0;
		for (int32 s = 0; s < NumSamples; ++s)
		{
			const double TargetLen = TotalLength * (static_cast<double>(s) / NumSamples);
			while (SegIdx < N - 1 && CumulativeLength[SegIdx + 1] < TargetLen)
			{
				++SegIdx;
			}
			const double SegStart = CumulativeLength[SegIdx];
			const double SegEnd = CumulativeLength[SegIdx + 1];
			const double SegLen = SegEnd - SegStart;
			const double T = SegLen > 0.0 ? (TargetLen - SegStart) / SegLen : 0.0;
			const FVector2D P = FMath::Lerp(Loop[SegIdx], Loop[(SegIdx + 1) % N], T);
			OutRadius[s] = FVector2D::Distance(P, Centroid);
		}
	}

	/** Centered moving-average low-pass filter over a circular (wrap-around) signal. */
	static void LowPassCircular(const TArray<double>& Signal, const int32 WindowRadius, TArray<double>& OutSmoothed)
	{
		const int32 N = Signal.Num();
		OutSmoothed.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			double Sum = 0.0;
			int32 Count = 0;
			for (int32 d = -WindowRadius; d <= WindowRadius; ++d)
			{
				const int32 Idx = ((i + d) % N + N) % N;
				Sum += Signal[Idx];
				++Count;
			}
			OutSmoothed[i] = Sum / FMath::Max(Count, 1);
		}
	}

	struct FNestingReport
	{
		int32 BroadBaySpans = 0;
		int32 BestSpanSubInletCount = 0;
	};

	/**
	 * Finds broad low-pass concavities (bay spans: contiguous runs where the smoothed radius sits
	 * meaningfully below the loop's mean radius) and, within each, counts sharper local dips in the
	 * *unsmoothed* radius signal relative to the smoothed baseline -- i.e. narrower/deeper sub-inlets
	 * cut into a broader bay. This is a direct programmatic stand-in for "does this coastline show
	 * compound nested inlets" so the recipe can be tuned and regression-guarded without a human
	 * eyeballing a screenshot every time.
	 */
	static FNestingReport AnalyzeNesting(const TArray<double>& Radius, const TArray<double>& Smoothed)
	{
		FNestingReport Report;
		const int32 N = Radius.Num();
		if (N == 0)
		{
			return Report;
		}

		double MeanRadius = 0.0;
		for (const double R : Smoothed)
		{
			MeanRadius += R;
		}
		MeanRadius /= N;

		// A "bay" sample: smoothed radius sits at least 15% below the loop's mean radius -- a
		// broad inward indentation, not just per-cell jitter.
		const double BayThreshold = MeanRadius * 0.85;

		TArray<bool> IsBay;
		IsBay.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			IsBay[i] = Smoothed[i] < BayThreshold;
		}

		TArray<bool> Visited;
		Visited.Init(false, N);
		for (int32 Start = 0; Start < N; ++Start)
		{
			if (!IsBay[Start] || Visited[Start])
			{
				continue;
			}

			// Extend backward to the true start of this circular run.
			int32 SpanStart = Start;
			for (int32 Steps = 0; Steps < N; ++Steps)
			{
				const int32 PrevIdx = ((SpanStart - 1) % N + N) % N;
				if (!IsBay[PrevIdx] || Visited[PrevIdx])
				{
					break;
				}
				SpanStart = PrevIdx;
			}

			int32 SpanEnd = SpanStart; // exclusive, may exceed N (circular)
			for (int32 Steps = 0; Steps < N; ++Steps)
			{
				const int32 Idx = ((SpanEnd % N) + N) % N;
				if (!IsBay[Idx] || Visited[Idx])
				{
					break;
				}
				Visited[Idx] = true;
				++SpanEnd;
			}

			const int32 SpanLen = SpanEnd - SpanStart;
			if (SpanLen < 3 || SpanLen >= N) // too short to matter, or the whole loop is "bay" (degenerate)
			{
				continue;
			}
			++Report.BroadBaySpans;

			// Within this span, count sub-inlet dips: local minima in Radius that sit meaningfully
			// below this span's own smoothed baseline (a narrower/deeper notch cut into the bay).
			int32 SubInlets = 0;
			for (int32 i = SpanStart + 1; i < SpanEnd - 1; ++i)
			{
				const int32 Idx = ((i % N) + N) % N;
				const int32 PrevIdx = ((Idx - 1) % N + N) % N;
				const int32 NextIdx = (Idx + 1) % N;
				const bool bLocalMin = Radius[Idx] < Radius[PrevIdx] && Radius[Idx] < Radius[NextIdx];
				const bool bMeaningfullyDeeper = Radius[Idx] < Smoothed[Idx] * 0.88;
				if (bLocalMin && bMeaningfullyDeeper)
				{
					++SubInlets;
				}
			}
			Report.BestSpanSubInletCount = FMath::Max(Report.BestSpanSubInletCount, SubInlets);
		}

		return Report;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHNestedInletShapeTest,
	"InvisibleHand.WorldBuilder.TerrainCellGraph.NestedInletShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHNestedInletShapeTest::RunTest(const FString& Parameters)
{
	using namespace IHNestedInletTestPrivate;
	const double StartSeconds = FPlatformTime::Seconds();

	FIHTerrainCellGraphGenerator::FBuildParams BuildParams;
	BuildParams.CenterLocalCm = FVector2D::ZeroVector;
	BuildParams.HalfExtentXCm = 150000.0; // 1500 m half-extent -> 3000 x 3000 m, matches the proven BasicIslandShape/TroughCarving box
	BuildParams.HalfExtentYCm = 150000.0;
	// Finer than BasicIslandShape/TroughCarving's 75 m -- even their pure hill-only recipe (no
	// troughs at all) fragments into ~17 small loops at 75 m resolution on this box size (see
	// BasicIslandShape's own AddInfo: cells=1681 loops=17 totalLoopVerts=57), which doesn't leave
	// enough per-loop vertex budget for this test's nested-inlet detection to tell real compound
	// bays apart from resampling noise on a near-degenerate polygon. More, smaller cells buy the
	// main coastline enough vertices to carry genuine detail.
	BuildParams.TargetCellWidthCm = 4500.0; // 45 m
	BuildParams.MasterSeed = 5150001;
	BuildParams.IslandIndex = 1;

	FIHTerrainCellGraph Graph;
	const bool bBuilt = FIHTerrainCellGraphGenerator::BuildGraph(BuildParams, Graph);
	TestTrue(TEXT("BuildGraph should succeed"), bBuilt);
	if (!bBuilt)
	{
		return false;
	}

	FRandomStream Stream(BuildParams.MasterSeed);

	// Mass: dominant hill matches BasicIslandShape's proven BlobPower (reach is a function of hop
	// count, roughly independent of physical cell size), but the accent window is pulled in tight
	// around the dominant hill's own core -- (0.15,0.85) was proven to fragment into many small
	// separate islands rather than merging into one landmass (BasicIslandShape's own recipe
	// already produces 17 loops for 57 total vertices at that window). Fewer accents in a tighter
	// window overlap the dominant hill's footprint instead of scattering independent bumps.
	FIHTerrainCellDiffusion::AddHill(
		Graph, 1, 60.0, 80.0, FVector2D(0.35, 0.65), FVector2D(0.35, 0.65), 0.90, Stream);
	FIHTerrainCellDiffusion::AddHill(
		Graph, 3, 20.0, 35.0, FVector2D(0.26, 0.74), FVector2D(0.26, 0.74), 0.90, Stream);

	// Major-bay pass: wide, shallow, single. Records the path so the guaranteed-nesting pass below
	// can bias a daughter trough toward it. Window matches the proven TroughCarving test's trough
	// window (0.30,0.70), narrower than the hills' (0.15,0.85) spread so troughs carve coastal
	// indentations rather than slicing clean across the whole landmass. Total trough-path count
	// across all three passes below is kept close to TroughCarving's proven single-pass Count=3
	// (here: 1 major + 2 sub + <=1 guaranteed = <=4) -- an earlier attempt stacked 7 trough paths
	// on this same hill mass and reduced land fraction to 0.2% (only 3 surviving cells).
	TArray<TArray<FVector2D>> MajorPaths;
	FIHTerrainCellDiffusion::AddRange(
		Graph, 1, -35.0, -20.0, FVector2D(0.30, 0.70), FVector2D(0.30, 0.70), 0.87, 0.3, Stream, &MajorPaths);

	// Sub-inlet pass: narrower/deeper, a couple of them, same seed window as the major-bay pass so
	// a meaningful fraction statistically land inside/adjacent to the major bay purely by overlap
	// -- Azgaar's own layering mechanism (heightmap-templates.ts), not an explicit hierarchy.
	FIHTerrainCellDiffusion::AddRange(
		Graph, 2, -50.0, -30.0, FVector2D(0.30, 0.70), FVector2D(0.30, 0.70), 0.78, 0.5, Stream);

	// Guaranteed-nesting pass: for the major-bay path, bias one daughter trough to start/end near
	// it -- a reliability boost on top of the statistical layering above, not a replacement for it
	// (a single global probability knob, not class-specific special-casing).
	constexpr float NestedInletBiasChance = 0.6f;
	int32 GuaranteedDaughters = 0;
	for (const TArray<FVector2D>& MajorPath : MajorPaths)
	{
		if (MajorPath.Num() < 2 || Stream.FRand() > NestedInletBiasChance)
		{
			continue;
		}
		const double AlongA = Stream.FRandRange(0.15f, 0.85f);
		const double AlongB = FMath::Clamp(AlongA + Stream.FRandRange(-0.2f, 0.2f), 0.0, 1.0);
		const double LateralSign = Stream.FRand() < 0.5f ? -1.0 : 1.0;
		const double LateralCm = Stream.FRandRange(3500.f, 7000.f) * LateralSign;
		const int32 StartIdx = FIHTerrainCellDiffusion::PickCellNearPath(Graph, MajorPath, AlongA, LateralCm, Stream);
		const int32 EndIdx = FIHTerrainCellDiffusion::PickCellNearPath(Graph, MajorPath, AlongB, LateralCm * 0.4, Stream);
		if (StartIdx == INDEX_NONE || EndIdx == INDEX_NONE || StartIdx == EndIdx)
		{
			continue;
		}
		FIHTerrainCellDiffusion::AddRangeBetweenCells(Graph, StartIdx, EndIdx, -55.0, -35.0, 0.78, 0.5, Stream);
		++GuaranteedDaughters;
	}

	// A single Smooth() call only reaches 1 hop, which turned out too narrow to consolidate this
	// recipe's multi-cell-scale height texture no matter how high a single-call Factor was pushed
	// (0.15-0.30 all left a threshold-dependent, non-monotonic tradeoff between land fraction and
	// fragment count -- raising Factor or lowering LandThreshold each independently helped one
	// axis and hurt the other). Three lighter passes reach further (~3 hops of blur) and
	// consolidate multi-cell texture that one wide-Factor pass over a 1-hop radius cannot.
	FIHTerrainCellDiffusion::Smooth(Graph, 0.30);
	FIHTerrainCellDiffusion::Smooth(Graph, 0.30);
	FIHTerrainCellDiffusion::Smooth(Graph, 0.30);

	constexpr double LandThreshold = 8.0;
	FIHTerrainCellDiffusion::ClassifyLandWater(Graph, LandThreshold);
	FIHTerrainCellDiffusion::ComputeCoastalMetadata(Graph);

	int32 LandCount = 0;
	for (const FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Feature == EIHCellFeature::Land)
		{
			++LandCount;
		}
	}
	const double LandFraction = Graph.Num() > 0 ? static_cast<double>(LandCount) / Graph.Num() : 0.0;
	AddInfo(FString::Printf(
		TEXT("IHNestedInletShape: cells=%d landCount=%d landFraction=%.3f majorPaths=%d guaranteedDaughters=%d"),
		Graph.Num(), LandCount, LandFraction, MajorPaths.Num(), GuaranteedDaughters));

	// Guard against a degenerate "technically has a loop" pass: a tiny surviving sliver of land
	// can pass the loop-count/nesting checks below on a technicality without actually looking
	// like a real island (hit once during tuning -- 0.4% land, 17-vertex main loop).
	TestTrue(TEXT("Land fraction should be a plausible island shape (5%-60% of the test area)"),
		LandFraction > 0.05 && LandFraction < 0.60);

	TArray<TArray<FVector2D>> Loops;
	FIHTerrainCellDiffusion::TraceCoastlineLoops(Graph, Loops);
	TestTrue(TEXT("Coastline trace should produce at least one closed loop"), Loops.Num() > 0);
	if (Loops.Num() == 0)
	{
		return false;
	}

	// Analyze the largest loop by enclosed area (the main island coastline).
	{
		TArray<int32> LoopSizes;
		for (const TArray<FVector2D>& Loop : Loops)
		{
			LoopSizes.Add(Loop.Num());
		}
		LoopSizes.Sort([](const int32 A, const int32 B) { return A > B; });
		AddInfo(FString::Printf(TEXT("IHNestedInletShape: loopCount=%d loopSizesDesc=%s"),
			Loops.Num(), *FString::JoinBy(LoopSizes, TEXT(","), [](const int32 N) { return FString::FromInt(N); })));
	}

	int32 BestLoopIdx = INDEX_NONE;
	double BestArea = -1.0;
	for (int32 i = 0; i < Loops.Num(); ++i)
	{
		const TArray<FVector2D>& Loop = Loops[i];
		const int32 N = Loop.Num();
		if (N < 25) // filters islet noise; a real main coastline loop at this cell density should clear this by a wide margin
		{
			continue;
		}
		double Area = 0.0;
		for (int32 v = 0; v < N; ++v)
		{
			const FVector2D& A = Loop[v];
			const FVector2D& B = Loop[(v + 1) % N];
			Area += A.X * B.Y - B.X * A.Y;
		}
		Area = FMath::Abs(Area) * 0.5;
		if (Area > BestArea)
		{
			BestArea = Area;
			BestLoopIdx = i;
		}
	}
	TestTrue(TEXT("Should find a non-degenerate main coastline loop"), BestLoopIdx != INDEX_NONE);
	if (BestLoopIdx == INDEX_NONE)
	{
		return false;
	}

	const TArray<FVector2D>& MainLoop = Loops[BestLoopIdx];
	constexpr int32 NumSamples = 256;
	TArray<double> Radius;
	ResampleRadiusByArcLength(MainLoop, NumSamples, Radius);
	TestEqual(TEXT("Resampled radius signal should have the requested sample count"), Radius.Num(), NumSamples);

	TArray<double> Smoothed;
	LowPassCircular(Radius, /*WindowRadius=*/NumSamples / 10, Smoothed);

	const FNestingReport Report = AnalyzeNesting(Radius, Smoothed);

	AddInfo(FString::Printf(
		TEXT("IHNestedInletShape: cells=%d majorPaths=%d guaranteedDaughters=%d loops=%d mainLoopVerts=%d broadBaySpans=%d bestSpanSubInlets=%d"),
		Graph.Num(), MajorPaths.Num(), GuaranteedDaughters, Loops.Num(), MainLoop.Num(),
		Report.BroadBaySpans, Report.BestSpanSubInletCount));

	TestTrue(TEXT("Main coastline should show at least one broad bay concavity"), Report.BroadBaySpans > 0);
	TestTrue(TEXT("At least one broad bay should contain 2+ nested sub-inlet dips (compound nesting)"),
		Report.BestSpanSubInletCount >= 2);

	const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
	TestTrue(TEXT("Full nested-inlet recipe + trace + analysis should complete in well under 10s"),
		ElapsedSeconds < 10.0);

	return true;
}
