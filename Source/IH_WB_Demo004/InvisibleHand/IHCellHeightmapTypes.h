// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IHMapSeedFrameworkTypes.h"
#include "IHP1C10_AzgaarTypes.h"

/** Regular cell grid in island-local km space (Azgaar-style heightmap substrate). */
struct FIHCellHeightmapGrid
{
	int32 Width = 0;
	int32 Height = 0;
	/** Cell edge length in km. Origin at grid center. */
	float CellSizeKm = 0.f;
	/** Half-extent of grid in km (centered at 0,0). */
	float ExtentKm = 1.f;
	TArray<uint8> Heights;
	/** Per-cell feature id (−1 = water/unassigned). */
	TArray<int32> FeatureIds;
	/** Nearest Voronoi site index per cell (spatial authority for Phase A). */
	TArray<int32> SiteIndices;

	int32 GetIndex(int32 X, int32 Y) const { return Y * Width + X; }
	bool IsInside(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
	uint8 GetHeight(int32 X, int32 Y) const
	{
		return IsInside(X, Y) ? Heights[GetIndex(X, Y)] : 0;
	}
	void SetHeight(int32 X, int32 Y, uint8 Value)
	{
		if (!IsInside(X, Y))
		{
			return;
		}
		const int32 Expected = Width * Height;
		if (Heights.Num() != Expected)
		{
			Heights.SetNum(Expected);
		}
		Heights[GetIndex(X, Y)] = Value;
	}
	int32 GetFeature(int32 X, int32 Y) const
	{
		return IsInside(X, Y) ? FeatureIds[GetIndex(X, Y)] : INDEX_NONE;
	}
	void SetFeature(int32 X, int32 Y, int32 Id)
	{
		if (!IsInside(X, Y))
		{
			return;
		}
		const int32 Expected = Width * Height;
		if (FeatureIds.Num() != Expected)
		{
			FeatureIds.SetNum(Expected);
			for (int32 i = 0; i < Expected; ++i)
			{
				FeatureIds[i] = INDEX_NONE;
			}
		}
		FeatureIds[GetIndex(X, Y)] = Id;
	}
	FVector2D CellCenterKm(int32 X, int32 Y) const
	{
		const float Ox = -ExtentKm + (static_cast<float>(X) + 0.5f) * CellSizeKm;
		const float Oy = -ExtentKm + (static_cast<float>(Y) + 0.5f) * CellSizeKm;
		return FVector2D(Ox, Oy);
	}
};

/** One disconnected land feature (main island or islet). */
struct FIHLandFeatureCoast
{
	int32 FeatureId = 0;
	bool bIsMainFeature = false;
	/** Labeled land cells in the heightfield grid (main island or detached islet). */
	int32 LandCellCount = 0;
	TArray<FVector2D> CoastPolylineLocalCm;
};

/**
 * Alt 2 (2026-07-15): copyable SSOT for IslandMesh WWF −25 m shelf **bottom-cap outer polygon**.
 * Baked once from Phase A soup (leftmost-turn outer cycle). SeaRoots frustum top ring = this ring.
 * Not Path F azimuth radii; not full triangle soup.
 */
/** Phase A WWF shelf bottom-cap triangle soup (island-local cm). */
struct FIHPhaseABottomCapSoup
{
	TArray<FVector> VerticesLocalCm;
	TArray<int32> Triangles;

	void Reset()
	{
		VerticesLocalCm.Reset();
		Triangles.Reset();
	}

	bool IsValid() const
	{
		return VerticesLocalCm.Num() >= 3 && Triangles.Num() >= 3;
	}
};

struct FIHShelfBottomCapContour
{
	bool bValid = false;
	/** INDEX_NONE = main feature; else FeaturePresentationExtent.FeatureId. */
	int32 FeatureId = INDEX_NONE;
	/** Ordered closed outer ring (island-local cm), CCW, arc-length resampled (largest loop). */
	TArray<FVector2D> OuterRingLocalCm;
	/**
	 * All soft-ok exterior loops from Phase A soup (raw boundary walk, not resampled).
	 * Magenta rim draws every ring so islets / detached shelf components are included.
	 */
	TArray<TArray<FVector2D>> AllExteriorRingsLocalCm;
	int32 RawLoopVertCount = 0;
	int32 BoundaryEdgeCount = 0;
	float AreaCm2 = 0.f;
	float MinRCm = 0.f;
	float MaxRCm = 0.f;
	FVector2D AzimuthOriginLocalCm = FVector2D::ZeroVector;

	void Reset()
	{
		bValid = false;
		FeatureId = INDEX_NONE;
		OuterRingLocalCm.Reset();
		AllExteriorRingsLocalCm.Reset();
		RawLoopVertCount = 0;
		BoundaryEdgeCount = 0;
		AreaCm2 = 0.f;
		MinRCm = 0.f;
		MaxRCm = 0.f;
		AzimuthOriginLocalCm = FVector2D::ZeroVector;
	}
};

/**
 * IB-1: per-feature presentation SSOT baked once at cell-map build.
 * Waterline @ z=0 and shelf floor @ z=-25 m (DeepOuter) for iceberg hull + downstream consumers.
 */
struct FIHFeaturePresentationExtent
{
	int32 FeatureId = INDEX_NONE;
	bool bIsMainFeature = false;
	/** z = 0 waterline authority (MainCoast or islet authority coast). */
	TArray<FVector2D> WaterlinePolylineLocalCm;
	/** z = -25 m shelf-floor authority (= segment-normal DeepOuter ring). */
	TArray<FVector2D> ShelfFloorPolylineLocalCm;
	/** Full slope-tier ring bake (coast, tan, cyan, deep). */
	FIHSeaRootsExtent SeaRootsExtent;
};

struct FIHIslandCellMapBuildParams
{
	int32 MasterSeed = 0;
	int32 IslandIndex = 0;
	EIHIslandTemplateType TemplateType = EIHIslandTemplateType::Low;
	float ExtentRadiusKm = 0.6f;
	float AreaKm2 = 1.f;
	float BaseSummitTopZCm = 18000.f;
};

/** Smoothed shoreline of enclosed interior water (lake) on the main island. */
struct FIHEnclosedLakeCoast
{
	TArray<FVector2D> CoastPolylineLocalCm;
};

/** Phase C3: coarse grid bitset aligned with FIHCellHeightmapGrid (1 = coastal lowland). */
struct FIHCoastalLowlandMask
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Cells;

	void Reset()
	{
		Width = 0;
		Height = 0;
		Cells.Reset();
	}

	bool IsValid() const
	{
		return Width > 0 && Height > 0 && Cells.Num() == Width * Height;
	}

	bool IsLowlandCell(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < Width && Y < Height && Cells[Y * Width + X] != 0;
	}
};

/** Gate 2b: seed-driven landing cove POI — guaranteed low-slope beach egress sector. */
struct FIHLandingCovePOI
{
	bool bEnabled = false;
	/** Land centroid at bake time (km, island-local). */
	FVector2D CentroidKm = FVector2D::ZeroVector;
	/** Outward azimuth from land centroid (radians). */
	float CenterAzimuthRad = 0.f;
	float HalfArcRad = 0.38f;
	EIHCoastShoreProfile CoveShoreProfile = EIHCoastShoreProfile::Beach;
};

struct FIHIslandCellMapResult
{
	FIHCellHeightmapGrid Grid;
	TArray<FIHLandFeatureCoast> FeatureCoasts;
	/** IB-1: parallel to meshed features — main + C5 islets when bake succeeds. */
	TArray<FIHFeaturePresentationExtent> FeaturePresentationExtents;
	TArray<FIHEnclosedLakeCoast> EnclosedLakeCoasts;
	FIHCoastalLowlandMask CoastalLowlandMask;
	int32 CoastalLowlandCellCount = 0;
	int32 MasterSeed = 0;
	int32 IslandIndex = 0;
	int32 MainFeatureIndex = 0;
	/** Combined main feature coast for nav / legacy radii resample. */
	TArray<FVector2D> MainCoastPolylineLocalCm;
	/** Gate 2b: default shoreline slope for arcs outside the landing cove wedge. */
	EIHCoastShoreProfile DefaultCoastShoreProfile = EIHCoastShoreProfile::Cliff;
	/** Gate 2b: interior summit bump archetype (independent of coast profile). */
	EIHSummitReliefArchetype SummitArchetype = EIHSummitReliefArchetype::LowMound;
	/** Gate 2b: seed landing cove POI baked at cell-map build. */
	FIHLandingCovePOI LandingCovePOI;
	/** §2c-H1: per-cell chamfer distance to nearest sea cell (m); −1 = unset. */
	TArray<float> HeightGridDistToSeaMeters;
	/** §2c-H1: land cells carved toward beach target height inside effective band. */
	int32 HeightGridBeachCarvedCellCount = 0;
};
