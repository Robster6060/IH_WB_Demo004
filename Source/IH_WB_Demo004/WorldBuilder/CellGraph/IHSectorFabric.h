// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "IHTerrainCellGraphTypes.h"

/** IH-DEC-029 shared slope bands (InvisibleHand_RiverTributary_System.md §7: Steep >=26deg, Moderate 6-25deg, Flat 0-5deg). */
enum class EIHSectorSlopeClass : uint8
{
	Flat,
	Moderate,
	Steep
};

/**
 * Contour-Guided Sector Fabric (IH-DEC-023/027/029/031) - PROTOTYPE touchpoint struct. This is
 * deliberately NOT DT_Sector: no InletSubtype (no Cove/Harborage/Firth data exists anywhere yet
 * to classify against - explicitly deferred, not stubbed), no SectorID, no adjacency, no
 * UDataTable row. Just the WB-phase-relevant subset needed to validate that isoline+flowline+quad
 * generation produces plausible, slope-shaped ~1-acre polygons spanning both dry land and the Sea
 * Shelf WWF band (0 to -25m ASL) via one unified Height-domain sweep - see FIHSectorFabric's own
 * comments for why land and shelf share one algorithm instead of two.
 */
struct FIHSectorFabricCell
{
	/** Island-local cm, CCW (normalized via signed-area check, regardless of input winding), unclosed. */
	TArray<FVector2D> PolygonVerts;
	FVector2D Centroid = FVector2D::ZeroVector;
	EIHSectorSlopeClass SlopeClass = EIHSectorSlopeClass::Flat;
	/** Land or Ocean(shelf) - reuses EIHCellFeature rather than inventing a parallel enum, matching
	 * the target DT_Sector schema's FeatureType concept (a coarser physical classification than
	 * the eventual gameplay-facing ESectorType, which doesn't exist yet either). */
	EIHCellFeature FeatureType = EIHCellFeature::Land;
	double AreaCm2 = 0.0;
	/** Planar distance along the flowline between the two bounding isolines. */
	double RadialRunCm = 0.0;
	/** Arclength along the lower (inner) isoline between the two bounding flowlines. */
	double AlongContourWidthCm = 0.0;
	/** Index into PolygonVerts of the edge facing the lower-elevation isoline; INDEX_NONE if unset. */
	int32 SeawardEdgeIndex = INDEX_NONE;
};

struct FIHSectorFabricParams
{
	/** Vertical spacing between successive isolines, real-world cm (not raw Height units - the
	 * orchestrator converts via the same Height<->Zcm scale IH_WB_IslandActor.cpp's mesh building
	 * uses, so this parameter means the same "500cm" regardless of a given island's Height range). */
	double ContourIntervalCm = 500.0;
	/** 1 acre. Target average Sector area - matches AcresFromAreaKm2's own m^2-per-acre constant
	 * (4046.8564224), not independently re-derived. */
	double TargetSectorAreaCm2 = 40468564.224;
	int32 MaxFlowlineHops = 64;
	/** 0 = deterministic pure steepest-descent; >0 winds more, mirrors AddRange's PathRandomness. */
	double FlowlineRandomness = 0.0;
};

/**
 * Contour-Guided Sector Fabric (IH-DEC-023/027/029/031) prototype-touchpoint algorithm. Lives
 * alongside the cell-graph substrate rather than in InvisibleHand/ - matches
 * IHTerrainCellGraphTypes.h's own header comment framing Sector Fabric as "a gameplay data layer
 * resampled from the finished terrain... do not conflate the two" with the Phase 1 cell graph.
 * Bespoke native C++ over the already-proven FIHTerrainCellGraph substrate - deliberately does not
 * depend on the UE5.8 PCG or GeometryScripting plugins (neither is enabled in this project; PCG's
 * own isoline-extraction algorithm is sound, but its full graph-execution framework is
 * fundamentally async with no synchronous "generate and wait" API, validated by Epic only from
 * inside a live Editor process - incompatible with this project's headless self-test discipline).
 */
class IH_WB_DEMO004_API FIHSectorFabric
{
public:
	/**
	 * Per-cell slope in degrees, classified via the IH-DEC-029 shared bands. Slope = mean
	 * atan(|dZcm|/|dSitePos|) over all Neighbor edges (NOT Feature-filtered - the shelf band needs
	 * this too), where Zcm = (Height - LandThresholdRaw) * (SummitTopZCm / HeightSpan) - the SAME
	 * affine scale IH_WB_IslandActor.cpp's own mesh-Z derivation uses. Raw Height deltas alone are
	 * NOT valid degrees without this scale.
	 */
	static void ComputeSlopeDegrees(
		const FIHTerrainCellGraph& Graph, double LandThresholdRaw, double HeightSpan, double SummitTopZCm,
		TArray<double>& OutSlopeDegrees, TArray<EIHSectorSlopeClass>& OutSlopeClass);

	/**
	 * Steepest-descent walk from StartCellIdx until Height <= TargetHeightRaw is reached (may be
	 * below LandThresholdRaw, for a shelf-ward flowline), a local Height minimum with no downhill
	 * unvisited neighbor is hit (a pit - returns false), or MaxHops is reached (returns false).
	 * Same greedy-neighbor-scoring shape as IHTerrainCellDiffusionPrivate::FindPathBetweenCells,
	 * but cost = "how much lower is this neighbor" instead of "how aligned with a target
	 * direction". Not Feature-filtered - must be able to walk from Land cells into Ocean/shelf
	 * cells. Deterministic given the same Stream state when Randomness=0.
	 */
	static bool TraceFlowlineDownhill(
		const FIHTerrainCellGraph& Graph, int32 StartCellIdx, double TargetHeightRaw,
		int32 MaxHops, double Randomness, FRandomStream& Stream, TArray<int32>& OutPathCellIndices);

	/**
	 * Assembles one Sector polygon by concatenating four already-ordered polylines into one closed
	 * loop: OuterIsolineSeg (forward) -> Flowline2Pts (forward, downhill) -> InnerIsolineSeg
	 * (reversed) -> Flowline1Pts (reversed, back uphill) - i.e. OuterIsolineSeg's last point must
	 * already coincide with Flowline2Pts' first point, and so on around the loop. Bespoke stitch;
	 * no engine primitive fits this (confirmed via UE5.8 GeometryScript/PCG research this session).
	 * Normalizes winding to CCW via a signed-area check regardless of input order, and fills
	 * Centroid/AreaCm2/RadialRunCm/AlongContourWidthCm/SeawardEdgeIndex/FeatureType (FeatureType
	 * from the majority Feature of the graph cells nearest a handful of interior sample points).
	 * Returns false (does not add to output) for a degenerate result (fewer than 3 net vertices
	 * after collapsing near-duplicates, or non-finite area).
	 */
	static bool BuildSectorQuad(
		const TArray<FVector2D>& OuterIsolineSeg, const TArray<FVector2D>& InnerIsolineSeg,
		const TArray<FVector2D>& Flowline1Pts, const TArray<FVector2D>& Flowline2Pts,
		const FIHTerrainCellGraph& Graph, FIHSectorFabricCell& OutSector);

	/**
	 * Top-level orchestrator. Sweeps isolines (via FIHTerrainCellDiffusion::TraceHeightIsolineLoops)
	 * at ContourIntervalCm-equivalent steps from SweepFloorHeightRaw up to SweepCeilingHeightRaw -
	 * ONE continuous sweep; the caller decides the range (pass SweepFloorHeightRaw = the shelf
	 * floor's raw-Height equivalent to cover both dry land and the Sea Shelf WWF band in one pass,
	 * or SweepFloorHeightRaw = LandThresholdRaw for dry land only). For each pair of consecutive
	 * isoline levels and each closed loop found at the outer (higher) level, seeds points along
	 * that loop spaced to target Params.TargetSectorAreaCm2 given the LOCALLY SAMPLED slope at
	 * each point (steeper local slope -> tighter spacing, since RadialRun shrinks - this is what
	 * produces the broad+shallow / deep+narrow shape response, not a post-hoc classification),
	 * traces a flowline downhill from each seed, and stitches a quad between each consecutive pair
	 * via BuildSectorQuad (inner edge simplified to a straight segment between the two flowlines'
	 * endpoints for this first prototype pass, rather than extracting the true inner-isoline
	 * sub-arc - a deliberate scope cut, noted in the .cpp, that keeps this pass tractable while
	 * still validating the qualitative slope-shape hypothesis).
	 * BoundsMinLocalCm/MaxLocalCm restrict which seed points are kept (does not affect isoline
	 * tracing itself, which needs the whole graph for correct topology) - pass
	 * Graph.BoundsMinLocalCm/MaxLocalCm for a full-graph run, or a sub-window for a bounded test.
	 */
	static bool BuildSectorFabricForRegion(
		const FIHTerrainCellGraph& Graph, double SweepFloorHeightRaw, double SweepCeilingHeightRaw,
		double LandThresholdRaw, double HeightSpan, double SummitTopZCm,
		const FIHSectorFabricParams& Params, const FVector2D& BoundsMinLocalCm, const FVector2D& BoundsMaxLocalCm,
		FRandomStream& Stream, TArray<FIHSectorFabricCell>& OutSectors);
};
