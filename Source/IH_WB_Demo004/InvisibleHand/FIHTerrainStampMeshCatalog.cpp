// Copyright Invisible Hand. All Rights Reserved.

#include "FIHTerrainStampMeshTypes.h"

UDataTable* FIHTerrainStampMeshCatalog::LoadCatalogTable()
{
	static TWeakObjectPtr<UDataTable> CachedTable;
	if (CachedTable.IsValid())
	{
		return CachedTable.Get();
	}

	UDataTable* Table = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/InvisibleHand/World/TerrainStamps/DT_TerrainStamp.DT_TerrainStamp"));
	CachedTable = Table;
	return Table;
}

namespace
{
	// Row-name lookup matching EIHTerrainStampId's declared order exactly (FIHTerrainStampTypes.h) -
	// a plain literal array rather than StaticEnum<EIHTerrainStampId>()->GetNameStringByValue, since
	// that enum isn't UHT-reflected (FIHTerrainStampTypes.h has no matching .generated.h include).
	static const TCHAR* const GRowNames[] = {
		TEXT("Hill"), TEXT("Knoll"), TEXT("Ridge"), TEXT("Mesa"), TEXT("Butte"), TEXT("VolcanoCone"),
		TEXT("Escarpment"), TEXT("CliffStamp"), TEXT("TerracedSlope"), TEXT("Spur"), TEXT("SummitCap"),
		TEXT("Valley"), TEXT("Basin"), TEXT("Sink"), TEXT("Canyon"), TEXT("Gorge"), TEXT("Crater"),
		TEXT("LakeBed"), TEXT("RiverChannel"), TEXT("Cove"), TEXT("HarborScoop"), TEXT("IslandShelf"),
	};
	static_assert(UE_ARRAY_COUNT(GRowNames) == IHInvisibleHandSpec::TerrainStampCount, "GRowNames/EIHTerrainStampId count mismatch");
}

const FIHTerrainStampMeshRow* FIHTerrainStampMeshCatalog::Get(EIHTerrainStampId StampId)
{
	UDataTable* Table = LoadCatalogTable();
	const int32 Index = static_cast<int32>(StampId);
	if (!Table || Index < 0 || Index >= UE_ARRAY_COUNT(GRowNames))
	{
		return nullptr;
	}

	return Table->FindRow<FIHTerrainStampMeshRow>(FName(GRowNames[Index]), TEXT("FIHTerrainStampMeshCatalog::Get"), false);
}

bool FIHTerrainStampMeshCatalog::IsAvailable(EIHTerrainStampId StampId)
{
	const FIHTerrainStampMeshRow* Row = Get(StampId);
	return Row && !Row->Mesh.IsNull();
}
