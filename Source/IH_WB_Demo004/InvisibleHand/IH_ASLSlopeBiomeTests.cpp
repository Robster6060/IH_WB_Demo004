// Copyright Invisible Hand. All Rights Reserved.
// Automation test for DT_ASLSlopeBiome (IH-DEC-052, trimmed per IH-DEC-054) — loads the CSV
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
	TestEqual(TEXT("Table should have exactly 20 rows (IH-DEC-054 trim — Hydro/Stamp/WWF/Ore/SEA LEVEL rows removed)"), Rows.Num(), 20);

	int32 ZoneAgnosticCount = 0;
	int32 SlopeAgnosticCount = 0;
	int32 NoZoneEligibilityCount = 0;
	double MinAslSeen = TNumericLimits<double>::Max();
	double MaxAslSeen = TNumericLimits<double>::Lowest();
	TSet<FString> SeenBiomeNames;

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

	TestEqual(TEXT("Every row must be eligible in at least one latitude zone"), NoZoneEligibilityCount, 0);
	// 8 zone-restricted rows survive the trim (Mid/High Tundra, Ice Floes = Nordic-only;
	// Low/High Desert, Estuary = Temperate+Tropical; Reef = Tropical-only; High Plains =
	// Temperate-only), so 12 of 20 remain zone-agnostic.
	TestEqual(TEXT("12 rows should be zone-agnostic (eligible in all 3 zones)"), ZoneAgnosticCount, 12);
	TestEqual(TEXT("No row should be slope-agnostic post-trim (the one prior case, Abysmal, was removed)"), SlopeAgnosticCount, 0);
	TestTrue(TEXT("ASL coverage should span down to the canonical -10m coastal floor (Reef/Estuary/Delta)"), MinAslSeen <= -9.0);
	TestTrue(TEXT("ASL coverage should span up to the canonical 2400m apex ceiling"), MaxAslSeen >= 2399.0);

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

	if (const FIHASLSlopeBiomeRow* Tundra = FindRow(TEXT("High Tundra")))
	{
		TestTrue(TEXT("High Tundra should be Nordic-eligible"), Tundra->bZoneNordic);
		TestFalse(TEXT("High Tundra should NOT be Temperate-eligible (narrowed to Nordic-only, IH-DEC-054)"), Tundra->bZoneTemperate);
		TestFalse(TEXT("High Tundra should NOT be Tropical-eligible"), Tundra->bZoneTropical);
	}
	else
	{
		AddError(TEXT("Could not find 'High Tundra' row"));
	}

	if (const FIHASLSlopeBiomeRow* Reef = FindRow(TEXT("Reef")))
	{
		TestFalse(TEXT("Reef should NOT be Nordic-eligible"), Reef->bZoneNordic);
		TestTrue(TEXT("Reef should be Tropical-eligible"), Reef->bZoneTropical);
	}
	else
	{
		AddError(TEXT("Could not find 'Reef' row"));
	}

	// Rows that should NOT exist post-trim — confirms the removal actually took, not just that
	// the survivors are correct.
	const TCHAR* RemovedNames[] = {
		TEXT("River"), TEXT("Glacier"), TEXT("Glacier Runoff"), TEXT("Swamp"),
		TEXT("Plateau"), TEXT("Harborage"), TEXT("Volcanic Rim"), TEXT("Coastal Cliffs"), TEXT("Canyon"),
		TEXT("Deep Sea"), TEXT("Verdant"), TEXT("Shallow Sea"), TEXT("Abysmal"),
		TEXT("Low Ore Escarpment"), TEXT("Mid Ore Escarpment"), TEXT("High Ore Escarpment"),
		TEXT("SEA LEVEL"),
	};
	for (const TCHAR* Removed : RemovedNames)
	{
		TestNull(FString::Printf(TEXT("'%s' should have been removed (IH-DEC-054)"), Removed), FindRow(Removed));
	}

	AddInfo(FString::Printf(
		TEXT("IHASLSlopeBiome: rows=%d zoneAgnostic=%d slopeAgnostic=%d aslRange=[%.0f,%.0f]"),
		Rows.Num(), ZoneAgnosticCount, SlopeAgnosticCount, MinAslSeen, MaxAslSeen));

	return true;
}
