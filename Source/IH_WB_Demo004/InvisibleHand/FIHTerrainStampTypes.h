// Copyright Epic Games, Inc. All Rights Reserved.
// Phase B2b — terrain stamp catalog (21 canonical rows; DT_TerrainStamp later).

#pragma once

#include "CoreMinimal.h"
#include "IHInvisibleHandDesignSpec.h"

/** Stable row id for DT_TerrainStamp / build palette. */
UENUM(BlueprintType)
enum class EIHTerrainStampId : uint8
{
	Hill = 0,
	Knoll,
	Ridge,
	Mesa,
	Butte,
	VolcanoCone,
	Escarpment,
	CliffStamp,
	TerracedSlope,
	Spur,
	SummitCap,
	Valley,
	Basin,
	Sink,
	Canyon,
	Gorge,
	Crater,
	LakeBed,
	RiverChannel,
	Cove,
	HarborScoop,
	IslandShelf,
	MAX UMETA(Hidden)
};

struct FIHTerrainStampDefinition
{
	EIHTerrainStampId StampId = EIHTerrainStampId::Hill;
	FName RowName;
	IHInvisibleHandSpec::ETerrainStampFamily Family = IHInvisibleHandSpec::ETerrainStampFamily::Vertical;
	/** Default footprint radius (km). */
	float DefaultRadiusKm = 0.18f;
	/** Peak height delta in Azgaar units (0–100 scale). */
	float DefaultAmplitudeAzgaar = 18.f;
	/** 1 = smooth dome, 2+ = sharper peak, <1 = broad plateau. */
	float ProfileExponent = 1.4f;
	/** When true, stamp subtracts height (inverted family default). */
	bool bDefaultInvert = false;
	/** Coast shelf tier override tag (B2b+); none for relief-only stamps. */
	FName CoastTierOverrideTag;
	/**
	 * Gate 6: stamp opts into sheer vertical ICE-01m exterior walls in its radius
	 * (overrides §2b-F procedural steep suppression when stamp mesh hook lands).
	 */
	bool bForceSheerCliffCoastWalls = false;
	/** Gate 6: stamp suppresses §2c universal beachfront band locally (planned). */
	bool bSuppressBeachfrontBand = false;
	/** Recommended for vertical→inverted double-duty via actor invert toggle. */
	bool bSupportsInvertDoubleDuty = true;
};

struct FIHTerrainStampCatalog
{
	static const FIHTerrainStampDefinition& Get(EIHTerrainStampId Id);
	static const FIHTerrainStampDefinition* FindByRowName(FName RowName);
	static TArray<EIHTerrainStampId> GetAllStampIds();
	static int32 NumStamps() { return IHInvisibleHandSpec::TerrainStampCount; }
};

/** B2b-3 replay header stub — one placed stamp instance (pre-bake). */
struct FIHPlacedTerrainStampReplayEntry
{
	int32 IslandIndex = INDEX_NONE;
	EIHTerrainStampId StampId = EIHTerrainStampId::Hill;
	FVector2D CenterLocalCm = FVector2D::ZeroVector;
	float RadiusKm = 0.f;
	float AmplitudeAzgaar = 0.f;
	float RotationDeg = 0.f;
	bool bInvertHeight = false;
};
