// Copyright Epic Games, Inc. All Rights Reserved.

#include "FIHTerrainStampTypes.h"

namespace
{
	static FIHTerrainStampDefinition MakeDef(
		EIHTerrainStampId Id,
		FName RowName,
		IHInvisibleHandSpec::ETerrainStampFamily Family,
		float RadiusKm,
		float Amp,
		float Exp,
		bool bInvert,
		bool bDoubleDuty,
		FName CoastTag = NAME_None,
		bool bSheerCliffWalls = false,
		bool bSuppressBeachfront = false)
	{
		FIHTerrainStampDefinition D;
		D.StampId = Id;
		D.RowName = RowName;
		D.Family = Family;
		D.DefaultRadiusKm = RadiusKm;
		D.DefaultAmplitudeAzgaar = Amp;
		D.ProfileExponent = Exp;
		D.bDefaultInvert = bInvert;
		D.bSupportsInvertDoubleDuty = bDoubleDuty;
		D.CoastTierOverrideTag = CoastTag;
		D.bForceSheerCliffCoastWalls = bSheerCliffWalls;
		D.bSuppressBeachfrontBand = bSuppressBeachfront;
		return D;
	}

	static const FIHTerrainStampDefinition GDefs[] = {
		MakeDef(EIHTerrainStampId::Hill, TEXT("Stamp_Hill"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.22f, 20.f, 1.5f, false, true),
		MakeDef(EIHTerrainStampId::Knoll, TEXT("Stamp_Knoll"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.14f, 12.f, 1.8f, false, true),
		MakeDef(EIHTerrainStampId::Ridge, TEXT("Stamp_Ridge"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.28f, 16.f, 0.9f, false, false),
		MakeDef(EIHTerrainStampId::Mesa, TEXT("Stamp_Mesa"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.26f, 14.f, 0.55f, false, true),
		MakeDef(EIHTerrainStampId::Butte, TEXT("Stamp_Butte"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.18f, 22.f, 2.2f, false, true),
		MakeDef(EIHTerrainStampId::VolcanoCone, TEXT("Stamp_VolcanoCone"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.24f, 28.f, 2.0f, false, true),
		MakeDef(EIHTerrainStampId::Escarpment, TEXT("Stamp_Escarpment"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.30f, 18.f, 0.75f, false, false, TEXT("Sheer"), true, true),
		MakeDef(EIHTerrainStampId::CliffStamp, TEXT("Stamp_Cliff"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.20f, 24.f, 2.5f, false, false, TEXT("Sheer"), true, true),
		MakeDef(EIHTerrainStampId::TerracedSlope, TEXT("Stamp_TerracedSlope"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.32f, 12.f, 0.65f, false, false),
		MakeDef(EIHTerrainStampId::Spur, TEXT("Stamp_Spur"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.16f, 14.f, 1.2f, false, false),
		MakeDef(EIHTerrainStampId::SummitCap, TEXT("Stamp_SummitCap"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.12f, 10.f, 2.8f, false, true),
		MakeDef(EIHTerrainStampId::Valley, TEXT("Stamp_Valley"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.24f, 16.f, 1.4f, true, true),
		MakeDef(EIHTerrainStampId::Basin, TEXT("Stamp_Basin"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.30f, 14.f, 0.8f, true, true),
		MakeDef(EIHTerrainStampId::Sink, TEXT("Stamp_Sink"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.12f, 18.f, 2.0f, true, true),
		MakeDef(EIHTerrainStampId::Canyon, TEXT("Stamp_Canyon"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.34f, 20.f, 0.7f, true, false),
		MakeDef(EIHTerrainStampId::Gorge, TEXT("Stamp_Gorge"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.22f, 22.f, 1.1f, true, false),
		MakeDef(EIHTerrainStampId::Crater, TEXT("Stamp_Crater"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.20f, 24.f, 2.2f, true, true),
		MakeDef(EIHTerrainStampId::LakeBed, TEXT("Stamp_LakeBed"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.26f, 10.f, 0.6f, true, false),
		MakeDef(EIHTerrainStampId::RiverChannel, TEXT("Stamp_RiverChannel"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.28f, 8.f, 0.45f, true, false),
		MakeDef(EIHTerrainStampId::Cove, TEXT("Stamp_Cove"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.18f, 6.f, 1.2f, true, false, TEXT("Bay")),
		MakeDef(EIHTerrainStampId::HarborScoop, TEXT("Stamp_HarborScoop"), IHInvisibleHandSpec::ETerrainStampFamily::Inverted, 0.22f, 8.f, 0.9f, true, false, TEXT("Bay")),
		MakeDef(EIHTerrainStampId::IslandShelf, TEXT("Stamp_IslandShelf"), IHInvisibleHandSpec::ETerrainStampFamily::Vertical, 0.40f, 0.f, 1.f, false, false, TEXT("ShelfWidth")),
	};
	static_assert(UE_ARRAY_COUNT(GDefs) == IHInvisibleHandSpec::TerrainStampCount, "TerrainStampCount mismatch");
}

const FIHTerrainStampDefinition& FIHTerrainStampCatalog::Get(EIHTerrainStampId Id)
{
	const int32 Idx = static_cast<int32>(Id);
	if (GDefs[Idx].StampId == Id)
	{
		return GDefs[Idx];
	}
	return GDefs[0];
}

const FIHTerrainStampDefinition* FIHTerrainStampCatalog::FindByRowName(FName RowName)
{
	for (const FIHTerrainStampDefinition& D : GDefs)
	{
		if (D.RowName == RowName)
		{
			return &D;
		}
	}
	return nullptr;
}

TArray<EIHTerrainStampId> FIHTerrainStampCatalog::GetAllStampIds()
{
	TArray<EIHTerrainStampId> Out;
	Out.Reserve(IHInvisibleHandSpec::TerrainStampCount);
	for (int32 i = 0; i < IHInvisibleHandSpec::TerrainStampCount; ++i)
	{
		Out.Add(static_cast<EIHTerrainStampId>(i));
	}
	return Out;
}
