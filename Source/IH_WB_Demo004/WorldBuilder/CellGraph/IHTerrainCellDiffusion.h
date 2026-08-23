// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "IHTerrainCellGraphTypes.h"

/**
 * Azgaar-style height diffusion ops operating directly on the cell adjacency graph
 * (heightmap-generator.ts, ported). Diffusion is breadth-first from seed cells with
 * power-law decay plus per-hop jitter — the mechanism that produces fractal, organic
 * coastlines when layered, as opposed to the old 4-neighbor-BFS raster approach that baked
 * a visible Manhattan-diamond comb pattern into every trough (see plan Addendum 4).
 *
 * All diffusion passes use an explicit visited-set per call and are therefore guaranteed to
 * terminate in O(NumCells) — no unbounded loops, learned the hard way earlier this session.
 */
class IH_WB_DEMO004_API FIHTerrainCellDiffusion
{
public:
	/**
	 * Radial hills/pits (Azgaar addHill/addPit). Seeds Count independent blobs at random cells
	 * within the given fractional sub-rectangle of the graph bounds, each diffusing outward via
	 * BFS with power-law decay. Pass negative Height range for pits/depressions.
	 * @param RangeXFrac, RangeYFrac  Fractional [0,1] window within the graph's bounding box to seed within.
	 * @param BlobPower  Decay exponent per hop, Azgaar range ~0.93-0.9973 (higher = wider spread).
	 */
	static void AddHill(
		FIHTerrainCellGraph& Graph,
		int32 Count,
		double HeightMin,
		double HeightMax,
		const FVector2D& RangeXFrac,
		const FVector2D& RangeYFrac,
		double BlobPower,
		FRandomStream& Stream);

	/**
	 * Same underlying multi-source BFS diffusion as AddHill, but seeded from a caller-supplied set
	 * of cell indices instead of one random point within a fractional window — e.g. every cell
	 * whose SitePos falls inside an arbitrary polygon (a golden-ratio seed triangle for organic,
	 * non-circular island base shapes). One random height in [HeightMin, HeightMax] is chosen and
	 * applied uniformly to all seeds, matching AddHill's single-height-per-call convention.
	 */
	static void AddHillFromCellSet(
		FIHTerrainCellGraph& Graph,
		const TArray<int32>& SeedIndices,
		double HeightMin,
		double HeightMax,
		double BlobPower,
		FRandomStream& Stream);

	/**
	 * Picks Count cell indices within the given fractional sub-rectangle of the graph bounds, each
	 * rejected and re-rolled if within MinSpacingCm of any already-picked seed this call (bounded
	 * retries, accepts the last draw regardless if retries exhaust - never fails to return Count
	 * seeds, just occasionally with two closer than the target). Intended as a pre-pass before a
	 * per-seed AddHillFromCellSet loop, so independent accent-hill blobs are less likely to
	 * geometrically overlap and merge into one another or into the main landmass than AddHill's own
	 * fully-independent per-seed picks - confirmed via live self-test this session to be a real,
	 * seed-dependent failure mode (2 of 5 islands on one realm lost every existing islet when the
	 * accent-hill Count was simply increased, because more independent random picks meant more
	 * chances for two to land close enough to merge). Deterministic given the same Stream.
	 */
	static void PickMinSpacedCellsInFractionalRange(
		const FIHTerrainCellGraph& Graph,
		int32 Count,
		const FVector2D& RangeXFrac,
		const FVector2D& RangeYFrac,
		double MinSpacingCm,
		FRandomStream& Stream,
		TArray<int32>& OutCellIndices);

	/**
	 * Linear ridges/troughs (Azgaar addRange/addTrough — same primitive, sign of Height picks
	 * which). Pathfinds between two random cells within the given fractional windows (bounded
	 * step count — terminates in at most Graph.Num() hops regardless of topology, see the
	 * infinite-loop lesson in IHCoastPolylineSmoothing.cpp this session), then diffuses
	 * perpendicular from the whole path as one multi-source BFS (so cross-section falls off from
	 * the nearest path cell, producing a valley/ridge shape rather than a chain of circular blobs).
	 * Pass negative Height range for troughs (inlet carving) — this is the op that actually
	 * produces nested Cove/Harborage/Firth-style coastline character; AddHill alone only ever
	 * produces smooth blobs.
	 * @param LinePower  Decay exponent per hop, steeper than BlobPower (Azgaar range ~0.75-0.93) — narrower cross-section than a hill.
	 * @param PathRandomness  0 = always step toward the target cell; higher values wind more.
	 */
	static void AddRange(
		FIHTerrainCellGraph& Graph,
		int32 Count,
		double HeightMin,
		double HeightMax,
		const FVector2D& RangeXFrac,
		const FVector2D& RangeYFrac,
		double LinePower,
		double PathRandomness,
		FRandomStream& Stream,
		TArray<TArray<FVector2D>>* OutPathsSitePositionsLocalCm = nullptr);

	/**
	 * Same primitive as AddRange's single-trough body, but the start/end cells are caller-supplied
	 * rather than randomly picked within a fractional window. Used to bias a daughter trough's path
	 * toward an already-carved parent trough (e.g. via PickCellNearPath below) so compound nested
	 * inlets emerge reliably rather than relying purely on independent-pass overlap.
	 */
	static void AddRangeBetweenCells(
		FIHTerrainCellGraph& Graph,
		int32 StartIdx,
		int32 EndIdx,
		double HeightMin,
		double HeightMax,
		double LinePower,
		double PathRandomness,
		FRandomStream& Stream,
		TArray<FVector2D>* OutPathSitePositionsLocalCm = nullptr);

	/**
	 * Finds the cell nearest a point sampled along a previously-recorded path (see AddRange's/
	 * AddRangeBetweenCells' OutPath... params), offset laterally. AlongFrac in [0,1] parameterizes
	 * position along the path (0 = start, 1 = end); LateralOffsetCm shifts perpendicular to the
	 * path's local tangent (either sign — no fixed "left"/"right" meaning). Intended for picking
	 * daughter-trough start/end cells near a parent trough's path.
	 */
	static int32 PickCellNearPath(
		const FIHTerrainCellGraph& Graph,
		const TArray<FVector2D>& PathSitePositionsLocalCm,
		double AlongFrac,
		double LateralOffsetCm,
		FRandomStream& Stream);

	/** Averages each cell's height toward its neighbors', blended by Factor (0 = no change, 1 = full average). */
	static void Smooth(FIHTerrainCellGraph& Graph, double Factor);

	/** Classifies every cell Land (Height >= LandThreshold) or Ocean. Does not produce Lake (IH-DEC-019). */
	static void ClassifyLandWater(FIHTerrainCellGraph& Graph, double LandThreshold);

	/**
	 * BFS coastal-distance bands and haven/harbor metadata, ported from Azgaar's features.ts.
	 * Must run after ClassifyLandWater.
	 */
	static void ComputeCoastalMetadata(FIHTerrainCellGraph& Graph);

	/**
	 * Direct cell-boundary coastline trace ("Option A" — the approved approach): walks the
	 * Voronoi edges between Land and Ocean cells into closed loops. No raster, no contour
	 * extraction — the coastline is exactly the classified cell tessellation's boundary, so it
	 * is structurally immune to the sawtooth/quantization class of defect. Must run after
	 * ClassifyLandWater.
	 * @param OutLoopsLocalCm  Each entry is one closed loop (island-local cm, unclosed — no duplicate last==first vertex).
	 */
	static void TraceCoastlineLoops(const FIHTerrainCellGraph& Graph, TArray<TArray<FVector2D>>& OutLoopsLocalCm);

	/**
	 * Generalized version of TraceCoastlineLoops: traces the boundary of the region
	 * {Cell.CoastDistance >= MinCoastDistance} instead of hard-coding {Feature == Land}. Reuses
	 * the same robust shared-Voronoi-edge chaining, so offshore/inland offset rings (e.g. the
	 * -25m WWF shelf edge or a +25m dry contour ribbon) are re-derived directly from classified
	 * cells rather than geometrically offsetting an existing polygon — immune to the
	 * self-crossing failure mode that affects naive polyline-offset approaches on concave,
	 * nested-inlet coastlines. Pass a negative MinCoastDistance to trace an offshore ring K hops
	 * out to sea (includes all Land plus Ocean cells within K hops); pass a positive
	 * MinCoastDistance to trace an inland ring K hops from the coast (Land cells only, since Ocean
	 * cells never have positive CoastDistance). Must run after ComputeCoastalMetadata.
	 * @param OutLoopsLocalCm  Each entry is one closed loop (island-local cm, unclosed).
	 */
	static void TraceCoastDistanceBoundaryLoops(
		const FIHTerrainCellGraph& Graph, int32 MinCoastDistance, TArray<TArray<FVector2D>>& OutLoopsLocalCm);

	/**
	 * Generalizes TraceCoastlineLoops/TraceCoastDistanceBoundaryLoops one step further: traces the
	 * boundary of {Cell.Height >= TargetHeightRaw} - an arbitrary-elevation isoline over the raw
	 * diffusion-height field (Contour-Guided Sector Fabric prototype input, IH-DEC-023/027). Reuses
	 * the same shared-Voronoi-edge chaining as its two siblings. Operates on raw Height only - does
	 * not require ClassifyLandWater/ComputeCoastalMetadata to have run, and is NOT restricted to
	 * Land cells (a threshold below LandThreshold traces an underwater isoline over Ocean cells,
	 * whose Height is a genuine, non-arbitrary artifact of the same diffusion process as their Land
	 * neighbors - diffusion has zero classification-awareness, and nothing downstream resets
	 * Height for Ocean cells).
	 * KNOWN RISK: TraceBoundaryLoops has an existing, documented, currently-unresolved fragmentation
	 * failure mode at some predicates (see its own internal comments re: an unresolved shelf-ring
	 * regression from an earlier session). Reusing it for isolines at NEW arbitrary thresholds
	 * inherits this risk - verify against real generated terrain before trusting broadly.
	 * @param OutLoopsLocalCm  Each entry is one closed loop (island-local cm, unclosed).
	 */
	static void TraceHeightIsolineLoops(
		const FIHTerrainCellGraph& Graph, double TargetHeightRaw, TArray<TArray<FVector2D>>& OutLoopsLocalCm);
};
