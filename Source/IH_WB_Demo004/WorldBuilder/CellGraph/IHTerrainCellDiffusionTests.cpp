// Copyright Invisible Hand. All Rights Reserved.
// Isolated automation test for Phase 1c diffusion/classification/coastline-trace — does not
// touch AIH_WB_IslandActor or any GameMode/PIE path (see IHTerrainCellGraphTests.cpp).

#include "Misc/AutomationTest.h"
#include "IHTerrainCellGraphGenerator.h"
#include "IHTerrainCellDiffusion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHTerrainCellDiffusionIslandTest,
	"InvisibleHand.WorldBuilder.TerrainCellGraph.BasicIslandShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHTerrainCellDiffusionIslandTest::RunTest(const FString& Parameters)
{
	const double StartSeconds = FPlatformTime::Seconds();

	FIHTerrainCellGraphGenerator::FBuildParams BuildParams;
	BuildParams.CenterLocalCm = FVector2D::ZeroVector;
	BuildParams.HalfExtentXCm = 150000.0; // 1500 m half-extent -> 3000 x 3000 m test area
	BuildParams.HalfExtentYCm = 150000.0;
	BuildParams.TargetCellWidthCm = 7500.0; // 75 m
	BuildParams.MasterSeed = 424242;
	BuildParams.IslandIndex = 1;

	FIHTerrainCellGraph Graph;
	const bool bBuilt = FIHTerrainCellGraphGenerator::BuildGraph(BuildParams, Graph);
	TestTrue(TEXT("BuildGraph should succeed"), bBuilt);
	if (!bBuilt)
	{
		return false;
	}

	FRandomStream Stream(BuildParams.MasterSeed);

	// One large central hill to form the base landmass, matching Azgaar's "one dominant seed
	// plus smaller accents" pattern for a single-island test shape. BlobPower must scale with
	// graph size: Azgaar's own 0.93-0.9973 range assumes map-scale cell counts (thousands to
	// tens of thousands); at this test's ~1,700 cells, 0.985 diffused across the ENTIRE test
	// area (landFraction=1.000, confirmed empirically) - dropped to 0.90 so the hill decays
	// within a fraction of the bounds, matching Azgaar's actual size-scaling intent rather than
	// literally reusing its large-map constant on a small test graph.
	FIHTerrainCellDiffusion::AddHill(
		Graph, /*Count=*/1, /*HeightMin=*/60.0, /*HeightMax=*/80.0,
		FVector2D(0.35, 0.65), FVector2D(0.35, 0.65), /*BlobPower=*/0.90, Stream);

	// A few smaller accent hills off-center so the coastline isn't a perfect circle.
	FIHTerrainCellDiffusion::AddHill(
		Graph, /*Count=*/4, /*HeightMin=*/15.0, /*HeightMax=*/35.0,
		FVector2D(0.15, 0.85), FVector2D(0.15, 0.85), /*BlobPower=*/0.88, Stream);

	FIHTerrainCellDiffusion::Smooth(Graph, 0.15);

	constexpr double LandThreshold = 20.0;
	FIHTerrainCellDiffusion::ClassifyLandWater(Graph, LandThreshold);
	FIHTerrainCellDiffusion::ComputeCoastalMetadata(Graph);

	int32 LandCount = 0;
	int32 HavenCount = 0;
	for (const FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Feature == EIHCellFeature::Land)
		{
			++LandCount;
			if (Cell.bHaven)
			{
				++HavenCount;
			}
		}
	}
	const double LandFraction = Graph.Num() > 0 ? static_cast<double>(LandCount) / Graph.Num() : 0.0;
	TestTrue(TEXT("Land fraction should be a plausible single-island shape (5%-60% of the test area)"),
		LandFraction > 0.05 && LandFraction < 0.60);
	TestTrue(TEXT("At least some land cells should be classified as haven (coastal)"), HavenCount > 0);

	TArray<TArray<FVector2D>> Loops;
	FIHTerrainCellDiffusion::TraceCoastlineLoops(Graph, Loops);
	TestTrue(TEXT("Coastline trace should produce at least one closed loop"), Loops.Num() > 0);

	int32 DegenerateLoops = 0;
	double TotalCoastlineLengthCm = 0.0;
	int32 TotalVerts = 0;
	for (const TArray<FVector2D>& Loop : Loops)
	{
		if (Loop.Num() < 6) // a real coastline loop on a ~75m-cell island should have well more than 6 verts
		{
			++DegenerateLoops;
		}
		TotalVerts += Loop.Num();
		const int32 N = Loop.Num();
		for (int32 i = 0; i < N; ++i)
		{
			TotalCoastlineLengthCm += FVector2D::Distance(Loop[i], Loop[(i + 1) % N]);
		}
	}
	TestTrue(TEXT("Main coastline loop should not be degenerate"), DegenerateLoops < Loops.Num());

	const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
	// Regression guard against ever reintroducing an unbounded-loop class of bug (see plan
	// Addendum 3) — this whole test should be near-instant on a few hundred cells.
	TestTrue(TEXT("Full diffusion+classification+trace pipeline should complete in well under 10s"),
		ElapsedSeconds < 10.0);

	AddInfo(FString::Printf(
		TEXT("IHTerrainCellDiffusion: cells=%d landFraction=%.3f havens=%d loops=%d totalLoopVerts=%d coastlineLenM=%.1f elapsedS=%.3f"),
		Graph.Num(), LandFraction, HavenCount, Loops.Num(), TotalVerts, TotalCoastlineLengthCm / 100.0, ElapsedSeconds));

	return true;
}

namespace IHTerrainCellDiffusionTroughTestPrivate
{
	struct FIslandStats
	{
		int32 LandCount = 0;
		int32 LoopCount = 0;
		int32 DegenerateLoops = 0;
	};

	static FIslandStats BuildAndMeasure(FRandomStream& Stream, const bool bCarveTrough)
	{
		FIslandStats Stats;

		FIHTerrainCellGraphGenerator::FBuildParams BuildParams;
		BuildParams.CenterLocalCm = FVector2D::ZeroVector;
		BuildParams.HalfExtentXCm = 150000.0;
		BuildParams.HalfExtentYCm = 150000.0;
		BuildParams.TargetCellWidthCm = 7500.0;
		BuildParams.MasterSeed = 909090;
		BuildParams.IslandIndex = 1;

		FIHTerrainCellGraph Graph;
		if (!FIHTerrainCellGraphGenerator::BuildGraph(BuildParams, Graph))
		{
			return Stats;
		}

		FIHTerrainCellDiffusion::AddHill(
			Graph, 1, 60.0, 80.0, FVector2D(0.35, 0.65), FVector2D(0.35, 0.65), 0.90, Stream);
		FIHTerrainCellDiffusion::AddHill(
			Graph, 4, 15.0, 35.0, FVector2D(0.15, 0.85), FVector2D(0.15, 0.85), 0.88, Stream);

		if (bCarveTrough)
		{
			FIHTerrainCellDiffusion::AddRange(
				Graph, 3, -70.0, -40.0, FVector2D(0.30, 0.70), FVector2D(0.30, 0.70), 0.80, 0.5, Stream);
		}

		FIHTerrainCellDiffusion::Smooth(Graph, 0.15);
		FIHTerrainCellDiffusion::ClassifyLandWater(Graph, 20.0);
		FIHTerrainCellDiffusion::ComputeCoastalMetadata(Graph);

		for (const FIHTerrainCell& Cell : Graph.Cells)
		{
			if (Cell.Feature == EIHCellFeature::Land)
			{
				++Stats.LandCount;
			}
		}

		TArray<TArray<FVector2D>> Loops;
		FIHTerrainCellDiffusion::TraceCoastlineLoops(Graph, Loops);
		Stats.LoopCount = Loops.Num();
		for (const TArray<FVector2D>& Loop : Loops)
		{
			if (Loop.Num() < 6)
			{
				++Stats.DegenerateLoops;
			}
		}
		return Stats;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHTerrainCellDiffusionTroughTest,
	"InvisibleHand.WorldBuilder.TerrainCellGraph.TroughCarving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHTerrainCellDiffusionTroughTest::RunTest(const FString& Parameters)
{
	using namespace IHTerrainCellDiffusionTroughTestPrivate;
	const double StartSeconds = FPlatformTime::Seconds();

	// Same MasterSeed/hill recipe with and without trough carving, same stream position at each
	// AddRange call site (separate FRandomStream per run, both starting fresh) so the only
	// difference is whether AddRange ran - isolates the trough's effect rather than just
	// re-running the whole recipe twice with a shared, diverging stream.
	FRandomStream StreamA(909090);
	const FIslandStats Baseline = BuildAndMeasure(StreamA, /*bCarveTrough=*/false);
	FRandomStream StreamB(909090);
	const FIslandStats Carved = BuildAndMeasure(StreamB, /*bCarveTrough=*/true);

	TestTrue(TEXT("Baseline (no trough) should produce at least one coastline loop"), Baseline.LoopCount > 0);
	TestTrue(TEXT("Carved (with trough) should produce at least one coastline loop"), Carved.LoopCount > 0);
	TestTrue(TEXT("Carved run should not increase degenerate loops beyond baseline's"),
		Carved.DegenerateLoops <= Baseline.DegenerateLoops + 1); // allow +1: a trough can legitimately split off a tiny islet

	// The actual proof the trough did something: it should remove land area relative to the
	// identical hill-only baseline (troughs use a negative height range).
	TestTrue(TEXT("Trough carving should reduce land cell count relative to the hill-only baseline"),
		Carved.LandCount < Baseline.LandCount);

	const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
	TestTrue(TEXT("Both runs combined should complete in well under 10s"), ElapsedSeconds < 10.0);

	AddInfo(FString::Printf(
		TEXT("IHTerrainCellDiffusion trough test: baselineLand=%d baselineLoops=%d carvedLand=%d carvedLoops=%d elapsedS=%.3f"),
		Baseline.LandCount, Baseline.LoopCount, Carved.LandCount, Carved.LoopCount, ElapsedSeconds));

	return true;
}
