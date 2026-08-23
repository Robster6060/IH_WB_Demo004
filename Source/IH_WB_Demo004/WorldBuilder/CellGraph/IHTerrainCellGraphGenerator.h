// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "IHTerrainCellGraphTypes.h"

/**
 * Builds the Azgaar-style jittered-point Voronoi cell graph used as the Phase 1 terrain
 * generation substrate. Point placement and triangulation are deterministic given the same
 * MasterSeed/IslandIndex (IH DDU convention, IH-DEC-022).
 *
 * Uses UE5.8's built-in UE::Geometry::FDelaunay2 (Engine/Source/Runtime/GeometryCore) for
 * triangulation and Voronoi cell extraction — no third-party library required.
 */
class IH_WB_DEMO004_API FIHTerrainCellGraphGenerator
{
public:
	struct FBuildParams
	{
		/** Island-local cm center of the generation bounds. */
		FVector2D CenterLocalCm = FVector2D::ZeroVector;
		double HalfExtentXCm = 0.0;
		double HalfExtentYCm = 0.0;
		/** Average Voronoi cell width before jitter, cm. Recommended 5000-10000 cm (50-100 m). */
		double TargetCellWidthCm = 7500.0;
		int32 MasterSeed = 0;
		int32 IslandIndex = 0;
	};

	/**
	 * Generates the jittered point set, Delaunay triangulation, Voronoi cell polygons, and
	 * cell adjacency. Does not assign height or classify land/water — see
	 * FIHTerrainCellDiffusion (Phase 1c) for the height-diffusion ops that run on top of this.
	 * @return false if the params were invalid or triangulation failed.
	 */
	static bool BuildGraph(const FBuildParams& Params, FIHTerrainCellGraph& OutGraph);
};
