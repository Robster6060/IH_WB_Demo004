// Copyright Invisible Hand. All Rights Reserved.
// Isolated automation test for the Contour-Guided Sector Fabric prototype touchpoint
// (IH-DEC-023/027/029/031) - does not touch AIH_WB_IslandActor or any GameMode/PIE path, matching
// IHTerrainCellDiffusionTests.cpp's own isolation convention.

#include "Misc/AutomationTest.h"
#include "IHTerrainCellGraphGenerator.h"
#include "IHTerrainCellDiffusion.h"
#include "IHSectorFabric.h"

namespace IHSectorFabricTestPrivate
{
	struct FShapeStats
	{
		int32 SectorCount = 0;
		double AvgAreaCm2 = 0.0;
		double AvgRadialRunCm = 0.0;
		double AvgAlongContourWidthCm = 0.0;
		FVector2D FirstCentroid = FVector2D::ZeroVector;
	};

	/** BlobPower is the "steep vs flat" lever: lower = faster decay = steeper falloff per hop;
	 * higher (plus extra Smooth passes) = slower decay = a broader, flatter profile. */
	static bool BuildTestGraph(FRandomStream& Stream, const double BlobPower, const int32 ExtraSmoothPasses,
		FIHTerrainCellGraph& OutGraph)
	{
		FIHTerrainCellGraphGenerator::FBuildParams BuildParams;
		BuildParams.CenterLocalCm = FVector2D::ZeroVector;
		BuildParams.HalfExtentXCm = 150000.0;
		BuildParams.HalfExtentYCm = 150000.0;
		BuildParams.TargetCellWidthCm = 7500.0;
		BuildParams.MasterSeed = 314159;
		BuildParams.IslandIndex = 1;

		if (!FIHTerrainCellGraphGenerator::BuildGraph(BuildParams, OutGraph))
		{
			return false;
		}

		FIHTerrainCellDiffusion::AddHill(
			OutGraph, /*Count=*/1, /*HeightMin=*/60.0, /*HeightMax=*/80.0,
			FVector2D(0.35, 0.65), FVector2D(0.35, 0.65), BlobPower, Stream);
		for (int32 i = 0; i < ExtraSmoothPasses; ++i)
		{
			FIHTerrainCellDiffusion::Smooth(OutGraph, 0.3);
		}

		constexpr double LandThreshold = 20.0;
		FIHTerrainCellDiffusion::ClassifyLandWater(OutGraph, LandThreshold);
		return true;
	}

	static bool RunSectorFabric(const FIHTerrainCellGraph& Graph, FRandomStream& Stream, FShapeStats& OutStats)
	{
		constexpr double LandThreshold = 20.0;
		double MaxLandHeight = LandThreshold;
		for (const FIHTerrainCell& Cell : Graph.Cells)
		{
			if (Cell.Feature == EIHCellFeature::Land)
			{
				MaxLandHeight = FMath::Max(MaxLandHeight, Cell.Height);
			}
		}
		const double HeightSpan = FMath::Max(MaxLandHeight - LandThreshold, 1.0);
		constexpr double SummitTopZCm = 8000.0; // 80m - a plausible test-scale summit height

		FIHSectorFabricParams Params;
		// Coarser than the real default so a ~1700-cell test graph still yields several sectors
		// (the real ContourIntervalCm=500 default assumes map-scale terrain, same size-scaling
		// reasoning the existing diffusion tests already apply to BlobPower).
		Params.ContourIntervalCm = 1000.0;

		TArray<FIHSectorFabricCell> Sectors;
		const bool bOk = FIHSectorFabric::BuildSectorFabricForRegion(
			Graph, /*SweepFloorHeightRaw=*/LandThreshold, /*SweepCeilingHeightRaw=*/MaxLandHeight,
			LandThreshold, HeightSpan, SummitTopZCm, Params,
			Graph.BoundsMinLocalCm, Graph.BoundsMaxLocalCm, Stream, Sectors);
		if (!bOk)
		{
			return false;
		}

		OutStats.SectorCount = Sectors.Num();
		if (Sectors.Num() == 0)
		{
			return true; // caller checks SectorCount > 0 itself
		}
		double SumArea = 0.0, SumRun = 0.0, SumWidth = 0.0;
		for (const FIHSectorFabricCell& Sector : Sectors)
		{
			SumArea += Sector.AreaCm2;
			SumRun += Sector.RadialRunCm;
			SumWidth += Sector.AlongContourWidthCm;
		}
		OutStats.AvgAreaCm2 = SumArea / Sectors.Num();
		OutStats.AvgRadialRunCm = SumRun / Sectors.Num();
		OutStats.AvgAlongContourWidthCm = SumWidth / Sectors.Num();
		OutStats.FirstCentroid = Sectors[0].Centroid;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHSectorFabricSlopeShapeTest,
	"InvisibleHand.WorldBuilder.SectorFabric.SlopeShapeResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHSectorFabricSlopeShapeTest::RunTest(const FString& Parameters)
{
	using namespace IHSectorFabricTestPrivate;
	const double StartSeconds = FPlatformTime::Seconds();
	constexpr double OneAcreCm2 = 40468564.224;

	// Steep: fast-decaying hill, no extra smoothing -> narrow, tightly-packed contours.
	FIHTerrainCellGraph SteepGraph;
	FRandomStream SteepStream(11111);
	TestTrue(TEXT("Steep test graph should build"), BuildTestGraph(SteepStream, /*BlobPower=*/0.75, 0, SteepGraph));
	FShapeStats SteepStats;
	TestTrue(TEXT("Sector Fabric should run on the steep graph"), RunSectorFabric(SteepGraph, SteepStream, SteepStats));
	TestTrue(TEXT("Steep graph should produce at least one sector (isoline fragmentation did not kill the run)"),
		SteepStats.SectorCount > 0);

	// Flat: slow-decaying hill plus extra smoothing -> broad, widely-spaced contours.
	FIHTerrainCellGraph FlatGraph;
	FRandomStream FlatStream(22222);
	TestTrue(TEXT("Flat test graph should build"), BuildTestGraph(FlatStream, /*BlobPower=*/0.96, 4, FlatGraph));
	FShapeStats FlatStats;
	TestTrue(TEXT("Sector Fabric should run on the flat graph"), RunSectorFabric(FlatGraph, FlatStream, FlatStats));
	TestTrue(TEXT("Flat graph should produce at least one sector (isoline fragmentation did not kill the run)"),
		FlatStats.SectorCount > 0);

	if (SteepStats.SectorCount > 0)
	{
		TestTrue(TEXT("Steep terrain should produce broad+shallow sectors (AlongContourWidth > RadialRun)"),
			SteepStats.AvgAlongContourWidthCm > SteepStats.AvgRadialRunCm);
		TestTrue(TEXT("Steep sectors' mean area should be within a generous factor of 1 acre (prototype tolerance)"),
			SteepStats.AvgAreaCm2 > OneAcreCm2 * 0.2 && SteepStats.AvgAreaCm2 < OneAcreCm2 * 5.0);
	}
	if (FlatStats.SectorCount > 0)
	{
		TestTrue(TEXT("Flat terrain should produce deep+narrow sectors (RadialRun > AlongContourWidth)"),
			FlatStats.AvgRadialRunCm > FlatStats.AvgAlongContourWidthCm);
		TestTrue(TEXT("Flat sectors' mean area should be within a generous factor of 1 acre (prototype tolerance)"),
			FlatStats.AvgAreaCm2 > OneAcreCm2 * 0.2 && FlatStats.AvgAreaCm2 < OneAcreCm2 * 5.0);
	}

	// Determinism (IH-DEC-022 convention): rebuild + rerun the steep graph with an identical seed,
	// expect the same sector count and first centroid.
	FIHTerrainCellGraph SteepGraphRepeat;
	FRandomStream SteepStreamRepeat(11111);
	TestTrue(TEXT("Steep test graph should rebuild identically"),
		BuildTestGraph(SteepStreamRepeat, /*BlobPower=*/0.75, 0, SteepGraphRepeat));
	FShapeStats SteepStatsRepeat;
	TestTrue(TEXT("Sector Fabric should rerun on the rebuilt steep graph"),
		RunSectorFabric(SteepGraphRepeat, SteepStreamRepeat, SteepStatsRepeat));
	TestEqual(TEXT("Same seed should produce the same sector count"),
		SteepStatsRepeat.SectorCount, SteepStats.SectorCount);
	if (SteepStats.SectorCount > 0)
	{
		TestTrue(TEXT("Same seed should produce the same first sector centroid"),
			FVector2D::DistSquared(SteepStatsRepeat.FirstCentroid, SteepStats.FirstCentroid) < 1.0);
	}

	const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
	TestTrue(TEXT("All three runs combined should complete in well under 10s"), ElapsedSeconds < 10.0);

	AddInfo(FString::Printf(
		TEXT("IHSectorFabric: steepSectors=%d steepAvgAreaAcres=%.2f steepRun=%.0f steepWidth=%.0f | ")
		TEXT("flatSectors=%d flatAvgAreaAcres=%.2f flatRun=%.0f flatWidth=%.0f | elapsedS=%.3f"),
		SteepStats.SectorCount, SteepStats.AvgAreaCm2 / OneAcreCm2, SteepStats.AvgRadialRunCm, SteepStats.AvgAlongContourWidthCm,
		FlatStats.SectorCount, FlatStats.AvgAreaCm2 / OneAcreCm2, FlatStats.AvgRadialRunCm, FlatStats.AvgAlongContourWidthCm,
		ElapsedSeconds));

	return true;
}
