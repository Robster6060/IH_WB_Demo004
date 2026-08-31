// Copyright Invisible Hand. All Rights Reserved.
// Automation test for DT_ASLSlopeBiome (IH-DEC-056: 48-row landform grid) — loads the CSV
// scaffold directly (same technique as UIH_WorldBuilderDataSubsystem, but decoupled from
// subsystem/PIE lifecycle, matching this project's convention of pure data/logic automation
// tests, e.g. IHTerrainCellGraphTests.cpp).

#include "Misc/AutomationTest.h"
#include "IH_ASLSlopeBiomeRow.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHASLSlopeBiomeDataTableTest,
	"InvisibleHand.WorldBuilder.ASLSlopeBiome.DataTableIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHASLSlopeBiomeDataTableTest::RunTest(const FString& Parameters)
{
	const FString CsvPath = FPaths::ProjectContentDir() / TEXT("InvisibleHand/Data/DataTables/DT_ASLSlopeBiome.csv");
	FString FileContent;
	if (!TestTrue(TEXT("DT_ASLSlopeBiome.csv should be readable"), FFileHelper::LoadFileToString(FileContent, *CsvPath)))
	{
		return false;
	}
	if (!FileContent.IsEmpty() && FileContent[0] == 0xFEFF)
	{
		FileContent.RightChopInline(1);
	}

	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FIHASLSlopeBiomeRow::StaticStruct();
	Table->ImportKeyField.Empty();
	const TArray<FString> CsvErrors = Table->CreateTableFromCSVString(FileContent);
	if (!TestEqual(TEXT("CSV should parse with zero errors"), CsvErrors.Num(), 0))
	{
		for (const FString& Err : CsvErrors)
		{
			AddError(Err);
		}
		return false;
	}

	TArray<FIHASLSlopeBiomeRow*> Rows;
	Table->GetAllRows(TEXT("ASLSlopeBiomeTest"), Rows);
	TestEqual(TEXT("Table should have exactly 48 rows (IH-DEC-056 — 7 tiers x 7 slope-types, WWF missing Basin)"), Rows.Num(), 48);

	int32 BasinCount = 0;
	int32 ZoneAgnosticCount = 0;
	int32 NoZoneEligibilityCount = 0;
	double MinAslSeen = TNumericLimits<double>::Max();
	double MaxAslSeen = TNumericLimits<double>::Lowest();
	TSet<FString> SeenBiomeNames;
	TMap<FName, int32> TierCounts;

	for (const FIHASLSlopeBiomeRow* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}
		const FString Name = Row->biomeName.ToString();
		TestFalse(FString::Printf(TEXT("biomeName '%s' should not be a duplicate row"), *Name),
			SeenBiomeNames.Contains(Name));
		SeenBiomeNames.Add(Name);

		TierCounts.FindOrAdd(Row->terrainTier)++;

		if (Row->bZoneNordic && Row->bZoneTemperate && Row->bZoneTropical)
		{
			++ZoneAgnosticCount;
		}
		if (!Row->bZoneNordic && !Row->bZoneTemperate && !Row->bZoneTropical)
		{
			++NoZoneEligibilityCount;
		}
		if (Row->bSlopeAgnostic)
		{
			++BasinCount;
			TestEqual(FString::Printf(TEXT("%s: Basin rows should use slopeRangeLabel='Basin'"), *Row->biomeID.ToString()),
				Row->slopeRangeLabel, FString(TEXT("Basin")));
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s: minSlopeDeg should be <= maxSlopeDeg"), *Row->biomeID.ToString()),
				Row->minSlopeDeg <= Row->maxSlopeDeg);
		}
		TestTrue(FString::Printf(TEXT("%s: aslLowerM should be <= aslUpperM"), *Row->biomeID.ToString()),
			Row->aslLowerM <= Row->aslUpperM);

		MinAslSeen = FMath::Min(MinAslSeen, static_cast<double>(Row->aslLowerM));
		MaxAslSeen = FMath::Max(MaxAslSeen, static_cast<double>(Row->aslUpperM));
	}

	TestEqual(TEXT("Every row must be eligible in at least one latitude zone"), NoZoneEligibilityCount, 0);
	TestEqual(TEXT("All 48 rows are zone-agnostic (IH-DEC-056 — zone restriction lives in DT_BiomeTagZoneExemption, not per-row)"), ZoneAgnosticCount, 48);
	TestEqual(TEXT("6 Basin rows (one per tier except WWF)"), BasinCount, 6);
	TestTrue(TEXT("ASL coverage should span down to the WWF floor (-25m)"), MinAslSeen <= -24.0);
	TestTrue(TEXT("ASL coverage should span up to the canonical 2400m apex ceiling"), MaxAslSeen >= 2399.0);

	// 7 tiers, each with 7 rows except WWF (6, no Basin).
	TestEqual(TEXT("Should have 7 distinct terrain tiers"), TierCounts.Num(), 7);
	for (const TPair<FName, int32>& Pair : TierCounts)
	{
		const int32 Expected = (Pair.Key == FName(TEXT("WWF"))) ? 6 : 7;
		TestEqual(FString::Printf(TEXT("Tier '%s' should have %d rows"), *Pair.Key.ToString(), Expected),
			Pair.Value, Expected);
	}

	// Spot-check specific rows confirmed during this session's redesign pass.
	auto FindRow = [&Rows](const TCHAR* BiomeName) -> const FIHASLSlopeBiomeRow*
	{
		for (const FIHASLSlopeBiomeRow* Row : Rows)
		{
			if (Row && Row->biomeName.ToString() == BiomeName)
			{
				return Row;
			}
		}
		return nullptr;
	};

	if (const FIHASLSlopeBiomeRow* CoastalBasin = FindRow(TEXT("Coastal Basin")))
	{
		TestTrue(TEXT("Coastal Basin should be slope-agnostic"), CoastalBasin->bSlopeAgnostic);
		TestTrue(TEXT("Coastal Basin should carry ice/glacier tags for the Nordic-wet-basin override"),
			CoastalBasin->features.Contains(FName(TEXT("Ice"))));
		TestTrue(TEXT("Coastal Basin should carry default wet-basin flora (Reeds)"),
			CoastalBasin->flora.Contains(FName(TEXT("Reeds"))));
	}
	else
	{
		AddError(TEXT("Could not find 'Coastal Basin' row"));
	}

	if (const FIHASLSlopeBiomeRow* SummitFaces = FindRow(TEXT("Summit Faces")))
	{
		TestEqual(TEXT("Summit Faces should be the Alpine Sheer row"), SummitFaces->terrainTier, FName(TEXT("Alpine")));
		TestEqual(TEXT("Summit Faces should be 81-90 deg"), SummitFaces->minSlopeDeg, 81.f);
	}
	else
	{
		AddError(TEXT("Could not find 'Summit Faces' row"));
	}

	if (const FIHASLSlopeBiomeRow* ShallowShelf = FindRow(TEXT("Shallow Shelf")))
	{
		TestEqual(TEXT("Shallow Shelf should be the WWF Flat row"), ShallowShelf->terrainTier, FName(TEXT("WWF")));
		TestEqual(TEXT("Shallow Shelf's ASL upper should be 0"), ShallowShelf->aslUpperM, 0.f);
	}
	else
	{
		AddError(TEXT("Could not find 'Shallow Shelf' row"));
	}

	// Confirm the old 20-row ecology names are actually gone, not just renamed duplicates.
	const TCHAR* OldNames[] = { TEXT("Marsh"), TEXT("Reef"), TEXT("High Timberland"), TEXT("Apex Caps"), TEXT("Barren Peaks") };
	for (const TCHAR* OldName : OldNames)
	{
		TestNull(FString::Printf(TEXT("Old ecology-named row '%s' should be gone (IH-DEC-056 replaced it)"), OldName), FindRow(OldName));
	}

	AddInfo(FString::Printf(
		TEXT("IHASLSlopeBiome: rows=%d tiers=%d basins=%d zoneAgnostic=%d aslRange=[%.0f,%.0f]"),
		Rows.Num(), TierCounts.Num(), BasinCount, ZoneAgnosticCount, MinAslSeen, MaxAslSeen));

	return true;
}
