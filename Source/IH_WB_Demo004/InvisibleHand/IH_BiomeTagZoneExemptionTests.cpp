// Copyright Invisible Hand. All Rights Reserved.
// Automation test for DT_BiomeTagZoneExemption (IH-DEC-053) — loads the CSV scaffold directly,
// same technique as IH_ASLSlopeBiomeTests.cpp, decoupled from subsystem/PIE lifecycle.

#include "Misc/AutomationTest.h"
#include "IH_BiomeTagZoneExemptionRow.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIHBiomeTagZoneExemptionDataTableTest,
	"InvisibleHand.WorldBuilder.BiomeTagZoneExemption.DataTableIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIHBiomeTagZoneExemptionDataTableTest::RunTest(const FString& Parameters)
{
	const FString CsvPath = FPaths::ProjectContentDir() / TEXT("InvisibleHand/Data/DataTables/DT_BiomeTagZoneExemption.csv");
	FString FileContent;
	if (!TestTrue(TEXT("DT_BiomeTagZoneExemption.csv should be readable"), FFileHelper::LoadFileToString(FileContent, *CsvPath)))
	{
		return false;
	}
	if (!FileContent.IsEmpty() && FileContent[0] == 0xFEFF)
	{
		FileContent.RightChopInline(1);
	}

	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FIHBiomeTagZoneExemptionRow::StaticStruct();
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

	TArray<FIHBiomeTagZoneExemptionRow*> Rows;
	Table->GetAllRows(TEXT("BiomeTagZoneExemptionTest"), Rows);
	TestEqual(TEXT("Table should have exactly 14 seeded rows (IH-DEC-056 added Glacier/Reeds/Papyrus/Rice)"), Rows.Num(), 14);

	TSet<FName> SeenTags;
	int32 NordicExemptCount = 0;
	int32 TemperateExemptCount = 0;
	int32 TropicalExemptCount = 0;
	for (const FIHBiomeTagZoneExemptionRow* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}
		TestFalse(FString::Printf(TEXT("Tag '%s' should not be a duplicate row"), *Row->tag.ToString()),
			SeenTags.Contains(Row->tag));
		SeenTags.Add(Row->tag);

		TestTrue(FString::Printf(TEXT("%s: should be exempt from at least one zone"), *Row->tag.ToString()),
			Row->bExemptNordic || Row->bExemptTemperate || Row->bExemptTropical);
		TestFalse(FString::Printf(TEXT("%s: should not be exempt from all 3 zones (that tag should just be deleted from DT_ASLSlopeBiome instead)"), *Row->tag.ToString()),
			Row->bExemptNordic && Row->bExemptTemperate && Row->bExemptTropical);

		if (Row->bExemptNordic) { ++NordicExemptCount; }
		if (Row->bExemptTemperate) { ++TemperateExemptCount; }
		if (Row->bExemptTropical) { ++TropicalExemptCount; }
	}

	// Spot-check specific rows confirmed during this session's planning pass.
	auto FindRow = [&Rows](const TCHAR* TagName) -> const FIHBiomeTagZoneExemptionRow*
	{
		for (const FIHBiomeTagZoneExemptionRow* Row : Rows)
		{
			if (Row && Row->tag == FName(TagName))
			{
				return Row;
			}
		}
		return nullptr;
	};

	if (const FIHBiomeTagZoneExemptionRow* Coral = FindRow(TEXT("Coral")))
	{
		TestTrue(TEXT("Coral should be exempt from Nordic"), Coral->bExemptNordic);
		TestFalse(TEXT("Coral should NOT be exempt from Tropical"), Coral->bExemptTropical);
	}
	else
	{
		AddError(TEXT("Could not find 'Coral' row"));
	}

	if (const FIHBiomeTagZoneExemptionRow* Ice = FindRow(TEXT("Ice")))
	{
		TestTrue(TEXT("Ice should be exempt from Tropical"), Ice->bExemptTropical);
		TestFalse(TEXT("Ice should NOT be exempt from Nordic"), Ice->bExemptNordic);
	}
	else
	{
		AddError(TEXT("Could not find 'Ice' row"));
	}

	AddInfo(FString::Printf(
		TEXT("IHBiomeTagZoneExemption: rows=%d nordicExempt=%d temperateExempt=%d tropicalExempt=%d"),
		Rows.Num(), NordicExemptCount, TemperateExemptCount, TropicalExemptCount));

	return true;
}
