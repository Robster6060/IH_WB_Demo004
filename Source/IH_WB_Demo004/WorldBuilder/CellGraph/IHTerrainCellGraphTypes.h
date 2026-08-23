// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * Azgaar-style terrain cell graph: a jittered-point Voronoi tessellation used as the
 * generation substrate for island shape/coastline (Phase 1). This is deliberately a
 * separate, coarser tessellation from the later Contour-Guided Sector Fabric (IH-DEC-023,
 * Phase 2) — the cell graph drives terrain, the Sector Fabric is a gameplay data layer
 * resampled from the finished terrain. Do not conflate the two.
 */

enum class EIHCellFeature : uint8
{
	Unclassified = 0,
	Ocean,
	Land,
	/** Reserved for the later Hydrology phase; Phase 1 never produces this (IH-DEC-019). */
	Lake,
};

struct FIHTerrainCell
{
	/** Jittered Voronoi site position, island-local cm. */
	FVector2D SitePos = FVector2D::ZeroVector;

	/** Voronoi cell polygon vertices, island-local cm, counter-clockwise, unclosed (no duplicate last==first). */
	TArray<FVector2D> Boundary;

	/** Adjacent cell indices — cells whose sites share a Delaunay edge with this one. */
	TArray<int32> Neighbors;

	/** Diffusion-accumulated height in arbitrary Azgaar-style units before sea-level calibration. */
	double Height = 0.0;

	EIHCellFeature Feature = EIHCellFeature::Unclassified;

	/** BFS hop distance from the coast: positive = inland, negative = out to open sea, 0 = coastal cell. */
	int32 CoastDistance = 0;

	/** True if this is a coastal land cell (has at least one Ocean/Lake neighbor). */
	bool bHaven = false;

	/** Count of adjacent Ocean/Lake cells, for haven/harbor-style dock-eligibility checks later. */
	int32 HarborCount = 0;
};

struct FIHTerrainCellGraph
{
	TArray<FIHTerrainCell> Cells;

	/** Island-local cm bounding box the graph was generated within (post Voronoi clip). */
	FVector2D BoundsMinLocalCm = FVector2D::ZeroVector;
	FVector2D BoundsMaxLocalCm = FVector2D::ZeroVector;

	int32 Num() const { return Cells.Num(); }
	bool IsValidIndex(const int32 Index) const { return Cells.IsValidIndex(Index); }
};
