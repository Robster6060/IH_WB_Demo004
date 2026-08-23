// Copyright Invisible Hand. All Rights Reserved.
// Isolated automation test for the Phase 1 cell-graph generator — deliberately does not touch
// AIH_WB_IslandActor or any GameMode/PIE path, so a bug here can never reproduce the class of
// hang this project hit earlier this session (see plan Addendum 3).

#include "Misc/AutomationTest.h"
#include "IHTerrainCellGraphGenerator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHTerrainCellGraphBasicTest,
	"InvisibleHand.WorldBuilder.TerrainCellGraph.BasicBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHTerrainCellGraphBasicTest::RunTest(const FString& Parameters)
{
	FIHTerrainCellGraphGenerator::FBuildParams Params;
	Params.CenterLocalCm = FVector2D::ZeroVector;
	Params.HalfExtentXCm = 100000.0; // 1000 m half-extent -> 2000 x 2000 m test island
	Params.HalfExtentYCm = 100000.0;
	Params.TargetCellWidthCm = 7500.0; // 75 m, midpoint of the approved 50-100 m range
	Params.MasterSeed = 12345;
	Params.IslandIndex = 1;

	FIHTerrainCellGraph Graph;
	const bool bBuilt = FIHTerrainCellGraphGenerator::BuildGraph(Params, Graph);
	TestTrue(TEXT("BuildGraph should succeed"), bBuilt);
	if (!bBuilt)
	{
		return false;
	}

	// Expect roughly (2000/75)^2 ~= 711 cells; jittered grid so allow a wide sane band.
	TestTrue(TEXT("Cell count should be in a sane range for the given extent/cell width"),
		Graph.Num() > 300 && Graph.Num() < 1500);

	int32 DegenerateCount = 0;
	int32 TotalNeighborCount = 0;
	double TotalArea = 0.0;
	for (const FIHTerrainCell& Cell : Graph.Cells)
	{
		if (Cell.Boundary.Num() < 3)
		{
			++DegenerateCount;
			continue;
		}
		TotalNeighborCount += Cell.Neighbors.Num();

		double Area = 0.0;
		const int32 N = Cell.Boundary.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Cell.Boundary[i];
			const FVector2D& B = Cell.Boundary[(i + 1) % N];
			Area += A.X * B.Y - B.X * A.Y;
		}
		TotalArea += FMath::Abs(Area) * 0.5;
	}

	TestTrue(TEXT("No more than ~5% degenerate boundary cells (expected only at the clip edge)"),
		DegenerateCount < FMath::Max(1, Graph.Num() / 20));

	const double AvgNeighbors = Graph.Num() > 0 ? static_cast<double>(TotalNeighborCount) / Graph.Num() : 0.0;
	TestTrue(TEXT("Average neighbor count should be plausible for a Voronoi tessellation (4-8)"),
		AvgNeighbors > 4.0 && AvgNeighbors < 8.0);

	const double ExpectedTotalAreaCm2 = (Params.HalfExtentXCm * 2.0) * (Params.HalfExtentYCm * 2.0);
	const double AreaRatio = ExpectedTotalAreaCm2 > 0.0 ? TotalArea / ExpectedTotalAreaCm2 : 0.0;
	TestTrue(TEXT("Total cell area should approximately tile the full bounding box"),
		AreaRatio > 0.95 && AreaRatio < 1.05);

	// Determinism check (IH DDU convention, IH-DEC-022): identical params must reproduce identically.
	FIHTerrainCellGraph Graph2;
	FIHTerrainCellGraphGenerator::BuildGraph(Params, Graph2);
	TestEqual(TEXT("Rebuilding with identical params should produce the identical cell count"),
		Graph2.Num(), Graph.Num());
	if (Graph2.Num() == Graph.Num() && Graph.Num() > 0)
	{
		TestEqual(TEXT("Rebuilding with identical params should reproduce the first site position exactly"),
			Graph2.Cells[0].SitePos, Graph.Cells[0].SitePos);
	}

	AddInfo(FString::Printf(TEXT("IHTerrainCellGraph: cells=%d avgNeighbors=%.2f areaRatio=%.4f degenerate=%d"),
		Graph.Num(), AvgNeighbors, AreaRatio, DegenerateCount));

	return true;
}
