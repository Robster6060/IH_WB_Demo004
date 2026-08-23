// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/** B2 slope-tier shelf bake — shared by island mesh, minimap GIS overlay. */
enum class EIHSeaRootsTier : uint8
{
	Beach = 0,
	Gentle,
	Steep,
	Sheer,
};

struct FIHSeaRootsExtent
{
	TArray<FVector2D> CoastSamplesLocalCm;
	TArray<FVector2D> OutwardNormals;
	TArray<EIHSeaRootsTier> Tiers;
	TArray<float> OutwardDisplacementsMeters;
	TArray<float> CoastRadiiCm;
	TArray<float> TanOuterRadiiCm;
	TArray<float> CyanOuterRadiiCm;
	TArray<float> DeepOuterRadiiCm;
	TArray<FVector2D> TanOuterSamplesLocalCm;
	TArray<FVector2D> CyanOuterSamplesLocalCm;
	TArray<FVector2D> DeepOuterSamplesLocalCm;
	FVector2D AzimuthOriginLocalCm = FVector2D::ZeroVector;
	int32 SharedRingSampleCount = 0;
	int32 TierCounts[4] = {};
	float MinOutwardDispMeters = 0.f;
	float MaxOutwardDispMeters = 0.f;
};

inline bool HasValidBakedSeaRootsRingPolylines(const FIHSeaRootsExtent& Extent)
{
	const int32 SampleCount = Extent.CoastSamplesLocalCm.Num();
	return SampleCount >= 8
		&& Extent.TanOuterSamplesLocalCm.Num() == SampleCount
		&& Extent.CyanOuterSamplesLocalCm.Num() == SampleCount
		&& Extent.DeepOuterSamplesLocalCm.Num() == SampleCount;
}

inline bool HasValidSeaRootsRadiiRings(const FIHSeaRootsExtent& Extent)
{
	return Extent.CoastRadiiCm.Num() >= 8;
}

inline bool HasValidSeaRootsExtentForPresentation(const FIHSeaRootsExtent& Extent)
{
	return HasValidBakedSeaRootsRingPolylines(Extent) || HasValidSeaRootsRadiiRings(Extent);
}

/** One ~1-acre cell in island-local cm space. */
struct FIHP1C10Cell
{
	FVector2D CenterLocalCm = FVector2D::ZeroVector;
	float HeightMeters = 0.f;
};

struct FIHP1C10IslandGenParams
{
	int32 MasterSeed = 0;
	int32 IslandIndex = 0;
	float SemiMajorAxisCm = 60000.f;
	float AreaKm2 = 1.f;
	float SummitTopZCm = 18000.f;
};

struct FIHP1C10IslandGenResult
{
	TArray<FIHP1C10Cell> Cells;
	TArray<FVector2D> MainCoastPolylineLocalCm;
	FIHSeaRootsExtent SeaRootsExtent;
	bool bHasSeaRootsExtent = false;
	bool bCoastAuthorityValid = false;
	float CoastEnvelopeCm = 0.f;
};
