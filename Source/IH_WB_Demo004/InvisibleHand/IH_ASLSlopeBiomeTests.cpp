// Copyright Invisible Hand. All Rights Reserved.
// Automation test for DT_ASLSlopeBiome (IH-DEC-052) — loads the CSV scaffold directly (same
// technique as UIH_WorldBuilderDataSubsystem, but decoupled from subsystem/PIE lifecycle, matching
// this project's convention of pure data/logic automation tests, e.g. IHTerrainCellGraphTests.cpp).

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
	TestEqual(TEXT("Table should have exactly 45 rows (44 PCG-eligible + 1 SEA LEVEL marker)"), Rows.Num(), 45);

	int32 PcgEligibleCount = 0;
	int32 PcgMarkerCount = 0;
	int32 ZoneAgnosticCount = 0;
	int32 SlopeAgnosticCount = 0;
	int32 NoZoneEligibilityCount = 0;
	double MinAslSeen = TNumericLimits<double>::Max();
	double MaxAslSeen = TNumericLimits<double>::Lowest();

	for (const FIHASLSlopeBiomeRow* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}
		if (Row->bPcgEligible)
		{
			++PcgEligibleCount;
		}
		else
		{
			++PcgMarkerCount;
		}
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
			++SlopeAgnosticCount;
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

	TestEqual(TEXT("44 rows should be PCG-eligible"), PcgEligibleCount, 44);
	TestEqual(TEXT("Exactly 1 row (SEA LEVEL) should be the non-generating marker"), PcgMarkerCount, 1);
	TestEqual(TEXT("Every row must be eligible in at least one latitude zone"), NoZoneEligibilityCount, 0);
	// IH-DEC-051 confirmed 12 zone-restricted rows out of 45, so 33 should remain zone-agnostic.
	TestEqual(TEXT("33 rows should be zone-agnostic (eligible in all 3 zones) per IH-DEC-051"), ZoneAgnosticCount, 33);
	TestEqual(TEXT("Exactly 2 rows (SEA LEVEL, Abysmal) should be slope-agnostic"), SlopeAgnosticCount, 2);
	TestTrue(TEXT("ASL coverage should span down to the canonical -250m Abysmal floor"), MinAslSeen <= -249.0);
	TestTrue(TEXT("ASL coverage should span up to the canonical 2400m apex ceiling"), MaxAslSeen >= 2399.0);

	// Spot-check specific rows confirmed during this session's planning pass.
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

	if (const FIHASLSlopeBiomeRow* Glacier = FindRow(TEXT("Glacier")))
	{
		TestTrue(TEXT("Glacier should be Nordic-eligible"), Glacier->bZoneNordic);
		TestFalse(TEXT("Glacier should NOT be Tropical-eligible (IH-DEC-051 confirmed)"), Glacier->bZoneTropical);
	}
	else
	{
		AddError(TEXT("Could not find 'Glacier' row"));
	}

	if (const FIHASLSlopeBiomeRow* Reef = FindRow(TEXT("Reef")))
	{
		TestFalse(TEXT("Reef should NOT be Nordic-eligible (IH-DEC-051 confirmed)"), Reef->bZoneNordic);
		TestTrue(TEXT("Reef should be Tropical-eligible"), Reef->bZoneTropical);
	}
	else
	{
		AddError(TEXT("Could not find 'Reef' row"));
	}

	AddInfo(FString::Printf(
		TEXT("IHASLSlopeBiome: rows=%d pcgEligible=%d marker=%d zoneAgnostic=%d slopeAgnostic=%d aslRange=[%.0f,%.0f]"),
		Rows.Num(), PcgEligibleCount, PcgMarkerCount, ZoneAgnosticCount, SlopeAgnosticCount, MinAslSeen, MaxAslSeen));

	return true;
}
