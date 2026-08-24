// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — authoritative numeric targets (P1C08 shared Phase 1B slice).

#pragma once

#include "CoreMinimal.h"
#include "IHCalendarTypes.h"

namespace IHInvisibleHandSpec
{
	static constexpr int32 LandformCountMin = 2;
	static constexpr int32 LandformCountMax = 7;

	/** Golden ratio φ — landscape tank width = φ × depth (E-W × N-S). */
	static constexpr double GoldenRatioPhi = 1.6180339887498948482;

	/**
	 * Default realm half-extent N-S (16.3 km → 32.7 km depth; φ width → ~52.9 km E-W).
	 * Sized so total realm land ≈ 128,000 acres at 30% land fraction (IH-DEC-026 "Firth
	 * Review" gate, current active target for the cell-graph pivot) — was 13 km (~81k acres)
	 * under the legacy heightfield pipeline; raised alongside the Demo003 fork.
	 * Production millions remain GATED (bWBUnlockProductionCanonicalAcres).
	 */
	static constexpr float DefaultRealmHalfExtentNSKm = 16.3f;
	/** @deprecated Prefer DefaultRealmHalfExtentNSKm (WT-B layout AABB rename). */
	static constexpr float DefaultTankHalfDepthKm = DefaultRealmHalfExtentNSKm;

	/** Demo Task-1 representative island (acres). */
	static constexpr int32 DemoRepresentativeIslandAcres = 8000;
	/** WB working primary-island target (Demo scale-test). */
	static constexpr int32 WBWorkingPrimaryIslandTargetAcres = 20000;
	/** Firth + pre-hydrology primary cap — unlocked for nested-inlet experiments. */
	static constexpr int32 WBFirthCapablePrimaryIslandAcresCap = 40000;
	/** Do not drive WB harness from CanonicalIslandDrySectorsTable millions until bake+WP. */
	static constexpr bool bWBUnlockProductionCanonicalAcres = false;
	/** 40k primary rung ON — denser coast carve + ≥513/1025 samples. */
	static constexpr bool bWBUnlockFirthCapable40kBudget = true;
	/** WB viewport: island select = dry mesh or MainCoast×fraction; water/outer ring → ships. */
	static constexpr bool bWBIslandSelectCoreOnly = true;
	static constexpr float WBIslandSelectCoreFraction = 2.f / 3.f;
	/** Heightfield samples: 513 @ ≥8k (incl. 40k rung until coastal ROI); else 257. */
	static constexpr int32 HeightfieldSamplesPerSideDefault = 257;
	static constexpr int32 HeightfieldSamplesPerSideFine = 513;
	/** Reserved for coastal-ROI / denser Firth mouths once ROI sampling ships. */
	static constexpr int32 HeightfieldSamplesPerSideFirth = 1025;

	inline int32 GetHeightfieldSamplesPerSideForAcres(const int64 TargetAcres)
	{
		// Full-grid 1025 is reserved until coastal ROI exists — 40k still meets ≥513 gate.
		(void)HeightfieldSamplesPerSideFirth;
		(void)WBFirthCapablePrimaryIslandAcresCap;
		return TargetAcres >= DemoRepresentativeIslandAcres
			? HeightfieldSamplesPerSideFine
			: HeightfieldSamplesPerSideDefault;
	}

	/** Cove navigable floor (m). Firth/Harborage mouths are much wider. */
	static constexpr float CoveMinNavigableWidthMeters = 10.f;
	/** Firth two-way merchant mouth floor (m) — soft diagnostic gate. */
	static constexpr float FirthMinTwoWayWidthMeters = 80.f;
	/** Harborage working channel floor (m). */
	static constexpr float HarborageMinChannelWidthMeters = 40.f;

	/** Legacy budget hint (deprecated as driver — use DefaultTargetEffectiveLandFraction). */
	static constexpr float DefaultDevLandAreaFraction = 0.70f;

	/** Soft target for achieved dry-land / tank water surface (30:70 starting preference). */
	static constexpr float DefaultTargetEffectiveLandFraction = 0.30f;

	/** Floor for feasibility-first uniform area scale in layout solve + overlap reconcile. */
	static constexpr float MinLayoutSolveAreaScale = 0.25f;

	/** Compact cluster placement span (fraction of usable tank); lower = tighter group, not corners. */
	static constexpr float CompactLayoutPatternFillFactor = 0.56f;

	static constexpr uint32 CanonicalTotalDrySectors = 3428100u;
	static constexpr double InternationalAcreSquareMeters = 4046.8564224;

	static constexpr float CanonicalTotalLandAreaKm2 = static_cast<float>(
		static_cast<double>(CanonicalTotalDrySectors) * InternationalAcreSquareMeters / 1000000.0);

	static constexpr uint32 CanonicalIslandDrySectorsTable[6][7] = {
		{2118600u, 1309500u, 0u, 0u, 0u, 0u, 0u},
		{1714500u, 1059300u, 654300u, 0u, 0u, 0u, 0u},
		{1533600u, 947700u, 585900u, 360900u, 0u, 0u, 0u},
		{1439100u, 889200u, 550800u, 339300u, 209700u, 0u, 0u},
		{1386900u, 856800u, 529200u, 327600u, 202500u, 125100u, 0u},
		{1356300u, 838800u, 518400u, 320400u, 197100u, 122400u, 74700u},
	};

	inline uint32 GetCanonicalIslandDrySectors(int32 LandformCount, int32 IslandIndex)
	{
		if (IslandIndex < 0 || IslandIndex >= LandformCount)
		{
			return 0u;
		}
		if (LandformCount < LandformCountMin || LandformCount > LandformCountMax)
		{
			return 0u;
		}
		return CanonicalIslandDrySectorsTable[LandformCount - 2][IslandIndex];
	}

	/** Canonical abysmal sea floor (m ASL). */
	static constexpr float DeepSeaFloorMeters = -250.f;
	/** Nautical / HUD / parchment waterline. */
	static constexpr float SeaLevelMeters = 0.f;
	/** Continental shelf floor (m ASL). */
	static constexpr float ShelfFloorMeters = -25.f;
	/**
	 * B+ three shelf vertical spans (m ASL):
	 * tan shore +5→−5 (straddles SeaLevelMeters), cyan −5→−10, deep blue −10→−25.
	 */
	static constexpr float ShelfShoreTopMeters = 5.f;
	static constexpr float ShelfShoreBottomMeters = -5.f;
	static constexpr float ShelfMidTopMeters = -5.f;
	static constexpr float ShelfMidBottomMeters = -10.f;
	static constexpr float ShelfDeepTopMeters = -10.f;
	/** Legacy break labels (shore bottom / mid band bottom). */
	static constexpr float ShelfShallowDepthMeters = ShelfShoreBottomMeters;
	static constexpr float ShelfMidDepthMeters = ShelfMidBottomMeters;
	static constexpr int32 ShelfRingCount = 3;
	/** Submerged shelf outer ring vs waterline footprint (1.08 = 8% wider at −25 m). */
	static constexpr float ShelfOutwardRadiusScale = 1.08f;

	// --- Phase B ocean material (shelf depth tint + shore-band transparency) ---

	/** Tan shore band geometry spans +5→−5 m ASL; 50/50 above/below waves is visual (transparent ocean MID). */
	static const FLinearColor OceanShallowTanColor = FLinearColor(0.82f, 0.72f, 0.48f, 0.45f);
	/** Cyan mid-shelf tint anchor (−5→−10 m) — RGB locked to minimap cyan fill (B6). */
	static const FLinearColor OceanShallowCyanColor = FLinearColor(0.22f, 0.68f, 0.72f, 0.55f);
	/** Deep blue outer shelf tint anchor (−10→−25 m) — RGB locked to minimap deep fill (B6). */
	static const FLinearColor OceanDeepBlueColor = FLinearColor(0.06f, 0.32f, 0.78f, 0.85f);
	/** Water MID depth breakpoints (cm) aligned to canonical shelf floors. */
	static constexpr float OceanShelfDepth5Cm = 500.f;
	static constexpr float OceanShelfDepth10Cm = 1000.f;
	static constexpr float OceanShelfDepth25Cm = 2500.f;
	/** Shallow tan-band opacity — lower alpha yields ~50/50 land/water read through surface. */
	static constexpr float OceanShoreBandOpacity = 0.34f;
	/** Minimum outward shelf width at −25 m when island radius estimate is tiny (km). */
	static constexpr float CoastShelfMinOutwardWidthKm = 0.045f;
	/** Main coast cliff skirt segment spacing along smoothed poly (km). */
	static constexpr float CoastCliffSkirtDensifySpacingKm = 0.06f;
	/** Lake shore cliff skirt spacing (tighter than main coast). */
	static constexpr float CoastLakeCliffSkirtDensifySpacingKm = 0.04f;
	/** Shoreline ribbon inset from coast poly toward land (× fine cell km). */
	static constexpr float CoastShorelineRibbonInsetFactor = 1.2f;
	/** Subdivide each shelf ring edge (1 = no split; 2 = two quads per edge). */
	static constexpr int32 ShelfRingEdgeSubdivisions = 2;
	/** Shelf constant-width offset: max miter length as multiple of offset distance. */
	static constexpr float ShelfOffsetMiterLimit = 2.5f;
	/** Shelf ring resample target (uniform arc-length verts per contour). */
	static constexpr int32 ShelfSampleCountMin = 384;
	static constexpr int32 ShelfSampleCountMax = 2048;

	/** Coast mesh: fine cells per height-grid cell (A++: 6 ≈ 6× shore tessellation vs base grid). */
	static constexpr int32 CoastMeshFineFactor = 6;
	/** Near coast/lake mask boundary: subdivide each fine cell into N×N sub-quads. */
	static constexpr int32 CoastBoundaryFineSubdivisions = 4;
	/** Inward poly-following band depth (× fine cell km) for crisp coast silhouette. */
	static constexpr float CoastConformingBandDepthFactor = 2.5f;
	/** Uniform samples around coast ring for conforming band + shelf index lock. */
	static constexpr int32 CoastConformingBandSampleCount = 2048;
	/** Max radial width multiplier before a shelf quad is rejected as a flare. */
	static constexpr float ShelfMaxRadialWidthFlareScale = 10.f;
	/** Light Chaikin on coast poly before shelf/conforming mesh (keeps inlets, softens stairsteps). */
	static constexpr int32 CoastMeshDisplaySmoothIterations = 2;
	static constexpr float CoastMeshDisplaySmoothCutRatio = 0.25f;
	/** Cap fine mesh resolution per axis (160×6=960 → capped via effective factor). */
	static constexpr int32 CoastMeshMaxFineCellsPerAxis = 768;
	/** Max enclosed lake holes before poly-interior ear-clip (bridge merge fails with many holes). */
	static constexpr int32 CoastPolyInteriorMaxLakeHoles = 48;
	/** Phase A: crisp coast at waterline only — shelf bands disabled until signoff. */
	static constexpr bool bCoastPhaseA_CrispCoastOnly = false;
	/** Phase B2: slope-tier shelf horizontal extent from coast segment normals (see SeaRoots slope lookup doc). */
	static constexpr bool bCoastPhaseB2_SlopeTierShelf = true;
	/** PIE proc-mesh tan/cyan/deep bands + minimap Sea Roots annuli (P2).
	 * Requires valid FIHSeaRootsExtent baked ring polylines from IHP1C10_SeaRootsBake.
	 */
	static constexpr bool bCoastPieBandPresentationEnabled = true;
	/** Minimap tan/cyan/deep annulus fills — independent of PIE 3D shelf presentation.
	 * ON ICE (2026-06-17): POKED125 signoff = full-ring MakeBox scanline + smoothed azimuth rings.
	 * Further harness edits blocked until PIE coast shelf is stable (Engineering Canon §3). */
	static constexpr bool bMinimapSeaDepthBandFillsEnabled = true;
	/** Flat azimuth annuli at +50/−50/−150 cm (overhead rings) — off; use sloped shelf instead. */
	static constexpr bool bPieShoreFlatAnnuliOverlayEnabled = false;
	/** Sloped shelf (+5→−25 m ASL) on ShelfPresentationMesh via BuildShelfBandProcMesh. */
	static constexpr bool bPieShorePresentationSlopedShelfEnabled = true;
	/** Step 1: skip 8 m vertical band walls — they read as dark rectangular chunks in PIE. */
	static constexpr bool bPieShoreOverlayVerticalWallsEnabled = false;
	/** Presentation duplicate (off — IslandMesh World DPG is the PIE-visible path). */
	static constexpr bool bPieShoreMirrorBandsOnIslandMesh = false;
	/** Primary draw: flat annulus sections on IslandMesh — off; sloped shelf uses ShelfPresentationMesh. */
	static constexpr bool bPieShoreBandsOnIslandMeshPrimary = false;
	/** Coast fine-grid quads: skip partial quads when any corner is at/below sea (reduces water spikes). */
	static constexpr bool bCoastStrictLandQuadSeaClipEnabled = true;
	/** Step 1: hide SeaRoots shelf shell (−25..−50 m) — reads as dark underwater panels, not pie bands. */
	static constexpr bool bPieShoreHideSeaRootsMeshInPie = true;
	/**
	 * Vertical color stack (DEV Contours = strokes only; fills are separate meshes):
	 *   Gold stroke ASL 0 | WWF shelf FILL cyan (ShelfMesh / PieBandCyan, 0→−25) |
	 *   Magenta stroke −25 | Sea Roots frustum FILL warm sand / deep indigo (−25→−250; never cyan) |
	 *   Abyss −250. Frustum mesh enablement deferred; when enabled lock MID to DevDemo_FrustumContrastTint.
	 */
	/** Inlet-only cull on azimuth rings (off until full rings sign off in PIE). */
	static constexpr bool bPieShoreOverlayInletCullEnabled = false;
	/** Step 2: 3D proc-mesh / ISM gold stroke — OFF (dark blue artifacts in PIE). */
	static constexpr bool bCoastStrokePieDisplayEnabled = false;
	/** Screen-projected gold stroke — Phase 4 on hold (perf / alignment). */
	static constexpr bool bCoastStrokeWorldScreenOverlayEnabled = false;
	/** Phase 4c: independent Island Base Dev Prop actors per island (Buoyant Cube class). */
	static constexpr bool bIslandBaseDevPropEnabled = true;
	/** MainCoast authority plane ASL (cm) — flat slab bottom. */
	static constexpr float IslandBaseDevPropBottomZOffsetCm = 0.f;
	/** Slab height (cm) — 1.0 m dev reference. */
	static constexpr float IslandBaseDevPropHeightCm = 100.f;
	/** Dev fill (#7CCF35 — same as minimap camera glyph). Dry upland Island Base Dev Prop slab. */
	static const FLinearColor IslandBaseDevPropFillColor = FLinearColor(
		124.f / 255.f, 207.f / 255.f, 53.f / 255.f, 1.f);
	/** Coastal lowland dev fill (#BBF451) — marsh/estuary/delta/tidal flat inside MainCoast (Phase C3/C4). */
	static const FLinearColor CoastalLowlandDevFillColor = FLinearColor(
		187.f / 255.f, 244.f / 255.f, 81.f / 255.f, 1.f);
	/** Phase C2: fill template depression water at cell-map build; inland lakes deferred to Hydrology H. */
	static constexpr bool bAdminFillTemplateDepressionWater = true;
	/** Phase C3: build CoastalLowlandMask on island from heightfield inside MainCoast. */
	static constexpr bool bCoastalLowlandMaskBuildEnabled = true;
	/** Phase C3: flat chartreuse overlay on island mesh for coastal lowland cells. */
	static constexpr bool bCoastalLowlandDevOverlayEnabled = true;
	/** GA production: hide chartreuse wetland dev overlay (keep mask bake for gameplay). */
	static constexpr bool bCoastGAProductionPresentation = false;
	/** ICE-01f: retire orange debug shoreline ring — selection glow on IslandMesh instead. */
	static constexpr bool bIslandSelectionDebugRingEnabled = false;
	/** ICE-02g: coast-band emissive on IslandMesh when selected — ON HOLD (Gate 5 deferred). */
	static constexpr bool bIslandMeshSelectionGlowEnabled = false;
	static constexpr float IslandSelectionGlowIntensity = 40.f;
	static constexpr float IslandSelectionCoastBandHalfWidthCm = 600.f;
	/** Bright unlit overlay on IslandMesh-extracted undercarriage / coast-cliff faces. */
	static constexpr float IslandSelectionOverlayEmissiveBoost = 72.f;
	/** MainCoast waterline purple ring tube — mesh-relative z=0 (sea level / gameplay coast). */
	static constexpr float IslandSelectionWaterlineHaloZCm = 0.f;
	/** P1C10 coast-following undercarriage ring half-thickness (cm). */
	static constexpr float IslandSelectionUndercarriageRingHalfThicknessCm = 480.f;
	/** ICE-02g: gizmo cross + gold yaw ring hub ASL (cm). Undercarriage glow is mesh-relative height_z only — not this plane. */
	static constexpr float IslandSelectionAxisHubZCm = 250.f;
	static const FLinearColor IslandSelectionEmissiveTint = FLinearColor(0.78f, 0.28f, 1.0f, 1.f);

	inline bool IsIslandMeshSelectionGlowEnabled()
	{
		return bIslandMeshSelectionGlowEnabled;
	}
	/** Phase C4: coastal lowland chartreuse section on Island Base Dev Prop. */
	static constexpr bool bIslandBaseDevPropLowlandSectionEnabled = true;
	/** Z offset (cm ASL) for coastal lowland dev overlay quads — above waterline to reduce z-fight. */
	static constexpr float CoastalLowlandDevOverlayZOffsetCm = 50.f;
	/** Instanced static-mesh planes along MainCoast — failed v3 (edge-on dark polylines). */
	static constexpr bool bCoastStrokeInstancedMeshEnabled = false;
	/** Vertical stroke walls read as dark rectangular chunks; thin horizontal ribbon only. */
	static constexpr bool bCoastStrokeVerticalWallsEnabled = false;
	/** Validation pass: fixed ribbon width (ignore radius-scaled width that widens on large islands). */
	static constexpr bool bCoastStrokePieDisplayUseCappedWidth = true;
	/** Proc-mesh stroke paths — dark blue line in PIE despite correct logs; off when ISM on. */
	static constexpr bool bCoastStrokeOnIslandMeshPrimary = false;
	static constexpr bool bCoastStrokeOnPresentationMeshPrimary = false;
	/** Optional duplicate stroke on IslandMesh (off). */
	static constexpr bool bCoastStrokeMirrorOnPresentationMesh = false;

	// --- Path 1: bake-once coast authority (height-field contour truth) ---

	/** When true, legacy full A+++ densify + multi-octave noise (hairpin risk — off for Path 1). */
	static constexpr bool bCoastAuthorityRefineNoiseEnabled = false;
	/** Break grid-aligned straight runs before authority bake (no full densify). */
	static constexpr bool bCoastAuthorityStraightBreakerEnabled = true;
	/** C1d: break merged collinear cardinal stairstep runs on raw extract (pre-768 resample). */
	static constexpr bool bCoastC1d_CardinalStairstepBreakerEnabled = true;
	/** C1d: chamfer grid 90° corners on extract polyline before authority bake. */
	static constexpr bool bCoastC1d_CardinalCornerChamferEnabled = true;
	/** C1d: max merged horizontal/vertical run (km) before outward kicks are inserted. */
	static constexpr float CoastC1d_MaxCollinearCardinalRunKm = 0.04f;
	/** C1d: chamfer leg length clamp (km) at cardinal 90° corners. */
	static constexpr float CoastC1d_ChamferMinKm = 0.008f;
	static constexpr float CoastC1d_ChamferMaxKm = 0.022f;
	/** C1d: fraction of shorter leg used for corner chamfer. */
	static constexpr float CoastC1d_ChamferLegFraction = 0.32f;
	/** C1d harness gate — post-remedy MainCoast max cardinal run (km). */
	static constexpr float CoastC1d_GateMaxCardinalRunKm = 0.15f;
	/** C1d harness gate — post-remedy MainCoast prominent 90° corners (both legs >= 0.025 km). */
	static constexpr int32 CoastC1d_GateMaxProminent90Corners = 6;
	/** C1d: pre-authority bail — force extra remedy passes when extract still has long grid runs (km). */
	static constexpr float CoastC1d_PreAuthorityBailMaxCardinalRunKm = 0.20f;
	static constexpr int32 CoastC1d_PreAuthorityBailRemedyPasses = 4;
	/** ICE-01o: post-authority bail — C1d remedy after bake when breaker/macro reintroduces long runs (km). */
	static constexpr float CoastC1d_PostAuthorityBailMaxCardinalRunKm = 0.08f;
	static constexpr int32 CoastC1d_PostAuthorityBailRemedyPasses = 4;
	/** ICE-01o: cap longest authority edge after post-bail (km) — remedy can leave 80–140m slits. */
	static constexpr float CoastC1d_PostAuthorityBailMaxEdgeKm = 0.04f;
	/** C1d: min adjacent leg (km) to count a corner as a prominent grid 90°. */
	static constexpr float CoastC1d_ProminentCornerMinLegKm = 0.025f;

	// --- Gate 2a: grid-bbox straight-edge coast (height field source fix) ---

	/** Strip land in a margin along the height-grid bbox before MS extract — prevents straight E/S coast runs. */
	static constexpr bool bCoastC2a_StripLandAtGridBoundaryEnabled = true;
	/** Land cells within this many cells of the grid edge are forced to sea before coast extract. */
	static constexpr int32 CoastC2a_GridBoundaryLandMarginCells = 2;
	/** Also break long straight polyline segments during C1d remedy (any bearing, not only cardinal runs). */
	static constexpr bool bCoastC2a_BreakLongStraightSegmentsInC1dRemedy = true;

	inline bool IsCoastC2aStripLandAtGridBoundaryEnabled()
	{
		return bCoastC2a_StripLandAtGridBoundaryEnabled;
	}

	// --- Gate 2b: landing cove POI + coast/summit profile separation ---

	/** Seed-driven landing cove with guaranteed low-slope beach egress (Option C). */
	static constexpr bool bCoastC2b_LandingCovePOIEnabled = false;
	/** Decouple coast shoreline taper from summit taper in mesh world-Z mapping. */
	static constexpr bool bCoastC2b_SeparateCoastSummitProfilesEnabled = true;
	/** Post-template radial summit bump (cone / ridge) on height grid. */
	static constexpr bool bCoastC2b_SummitArchetypeHeightBumpEnabled = true;
	/** Half-width of landing cove wedge (degrees each side of center azimuth). Phase F2: 32→36. */
	static constexpr float CoastC2b_LandingCoveHalfArcDeg = 36.f;
	/** Outer radial band (fraction of centroid→coast distance) for beach flatten + carve. Phase F2: 0.32→0.38. */
	static constexpr float CoastC2b_LandingCoveOuterBandFraction = 0.38f;
	/** Azgaar height subtracted at cove outer edge to bite a shallow inlet. */
	static constexpr float CoastC2b_LandingCoveCarveAzgaar = 26.f;
	/** Target beach Azgaar height in landing cove wedge. */
	static constexpr uint8 CoastC2b_LandingCoveTargetBeachAzgaar = 24;
	/** Land-t fraction below which coast taper blends toward beach/gentle (Beach profile). */
	static constexpr float CoastC2b_BeachLandTFraction = 0.45f;
	/** Land-t fraction for Gentle coast profile mesh Z blend. */
	static constexpr float CoastC2b_GentleLandTFraction = 0.30f;
	/** Land-t fraction for landing-cove wedge mesh Z — wide gentle ramp inland. */
	static constexpr float CoastC2b_LandingCoveLandTFraction = 0.72f;
	/** Mesh Z taper exponents for coast profiles (lower = gentler beach). */
	static constexpr float CoastC2b_BeachTaperExponent = 0.55f;
	static constexpr float CoastC2b_LandingCoveBeachTaperExponent = 0.42f;
	static constexpr float CoastC2b_GentleTaperExponent = 1.05f;
	/**
	 * Cliff mesh Z soften (all cliff-profile islands): land-t band below which coast taper blends
	 * toward a lower exponent — steep shoreline between sheer Cliff (0) and Gentle/Beach.
	 * Phase F1: wider land-t band + lower taper = softer waterline, mitigates ICE-01m fangs.
	 */
	static constexpr float CoastC2b_HighCliffLandTFraction = 0.28f;
	static constexpr float CoastC2b_VolcanicCliffLandTFraction = 0.26f;
	/** Option A: Low islands — cliff coast mesh soften (converged toward High). */
	static constexpr float CoastC2b_LowCliffLandTFraction = 0.28f;
	static constexpr float CoastC2b_HighCliffTaperExponent = 0.62f;
	static constexpr float CoastC2b_VolcanicCliffTaperExponent = 0.58f;
	static constexpr float CoastC2b_LowCliffTaperExponent = 0.62f;
	/** Summit height-grid boosts (Azgaar units) by archetype — cone uses max() to override caldera. */
	static constexpr float CoastC2b_VolcanicConePeakBoostAzgaar = 48.f;
	static constexpr float CoastC2b_HighRidgePeakBoostAzgaar = 22.f;
	static constexpr float CoastC2b_LowMoundPeakBoostAzgaar = 20.f;
	static constexpr float CoastC2b_VolcanicConeRadiusFraction = 0.41f;
	/** Radial taper exponent for volcanic cone height-grid boost (lower = gentler slopes). */
	static constexpr float CoastC2b_VolcanicConeSteepnessPower = 1.18f;
	static constexpr float CoastC2b_HighRidgeRadiusFraction = 0.30f;
	static constexpr float CoastC2b_LowMoundRadiusFraction = 0.30f;
	/** Mesh summit Z multipliers by archetype (on top of template relief scale). */
	static constexpr float CoastC2b_VolcanicSummitZScale = 1.85f;
	static constexpr float CoastC2b_HighRidgeSummitZScale = 1.18f;
	static constexpr float CoastC2b_LowMoundSummitZScale = 1.15f;
	/** In wedge: cap inland heights to this radial fraction for egress ramp. Phase F2: longer gentle ramp. */
	static constexpr float CoastC2b_LandingCoveInlandRampRadialFraction = 0.82f;
	/**
	 * §2b-C: smoothstep Beach↔Cliff coast taper + LandTFraction across wedge azimuth sides.
	 * Eliminates hard Beach/Cliff binary sawtooth at landing cove rim (mesh Z in AzgaarToWorldZCm).
	 */
	static constexpr bool bCoastC2b_LandingCoveWedgeAngularFeatherEnabled = true;
	/** Degrees beyond wedge half-arc (each side) for Beach→Cliff taper/LandT angular feather. */
	static constexpr float CoastC2b_LandingCoveWedgeFeatherHalfAngleDeg = 10.f;
	/**
	 * §2b-D: smoothstep Cliff→Gentle CoastTaper + LandTFraction by distance to MainCoast authority polyline.
	 * Reduces rect-grid per-corner Z stair-step on full perimeter (angular-independent; complements §2b-C wedge).
	 * tune 2026-07-04 v2: wider band + softer shore gentle + double smoothstep alpha + corner Z coherence.
	 */
	static constexpr bool bCoastC2b_PerimeterCoastSmoothingEnabled = true;
	/** Meters inland from authority waterline for perimeter taper soften (0=Gentle at shore, band=full Cliff). */
	static constexpr float CoastC2b_PerimeterCoastSmoothBandMeters = 750.f;
	/** §2b-D shore-end CoastTaper (higher = gentler waterline Z; separate from global Gentle profile). */
	static constexpr float CoastC2b_PerimeterCoastGentleTaperExponent = 1.20f;
	/** §2b-D shore-end LandTFraction (wider coast taper band at waterline). */
	static constexpr float CoastC2b_PerimeterCoastGentleLandTFraction = 0.38f;
	/** §2b-D: floor LandTFraction at waterline (alpha→0) to reduce per-corner taper variance. */
	static constexpr float CoastC2b_PerimeterCoastShoreLandTMinFraction = 0.36f;
	/** §2b-D v2: blend coast-touching quad corner Z toward quad min to kill rect stair-step.
	 * §2b-D v3 revert (2026-07-04 PIE): default off — v3 cove-interior + CoastCornerCount≥1
	 * caused downslope terracing (horizontal contour bands on slopes). Quad-min taper kept. */
	static constexpr bool bCoastC2b_PerimeterCoastCornerZCoherenceEnabled = false;
	/** Blend weight toward quad min Z for coast-touching corners (0=off, 1=full flatten). */
	static constexpr float CoastC2b_PerimeterCoastCornerZCoherenceBlend = 0.65f;
	/** Max corner Z span (cm) before coherence kicks in on coast-touching quads. */
	static constexpr float CoastC2b_PerimeterCoastCornerZMaxSpanCm = 180.f;
	/**
	 * §2b-E: extend §2b-D gentle taper inside landing cove wedge (Beach profile rim).
	 * Mesh Z only — reduces rect-grid stair-step on cove interior coast-touching quads.
	 */
	static constexpr bool bCoastC2b_LandingCovePerimeterSmoothEnabled = true;
	/**
	 * §2b-E: on coast-straddling quads, flatten sea-level corners (H≤sea+ε) to unified
	 * waterline Z — no inland slope flatten (safer than v3 corner Z coherence).
	 */
	static constexpr bool bCoastC2b_CoastStraddleQuadWaterlineFlattenEnabled = true;
	/** Azgaar height tolerance for coast-straddle waterline corner flatten (matches coherence). */
	static constexpr float CoastC2b_CoastStraddleQuadWaterlineEpsilonAzgaar = 0.15f;
	/**
	 * §2b-F: when true, steep exterior walls (>22 m) become gentle-only (cap 18 m).
	 * REVERTED 2026-07-05 PIE: suppressing all closure walls caused see-through coast holes.
	 * Keep false until sloped skirt mesh or §2c beachfront replaces vertical closure.
	 */
	static constexpr bool bCoastC2b_SuppressProceduralSteepCoastWallsEnabled = false;
	/** Gentle wall top cap (m ASL) when §2b-F steep suppression is on (was 12 m). */
	static constexpr float CoastC2b_GentleWallMaxTopWhenSteepSuppressedMeters = 18.f;

	// --- Gate 2 v2 step 2: default Gentle coast for all templates (summit archetype unchanged) ---
	/** When true, ResolveDefaultCoastShoreProfile returns Gentle for LOW/HIGH/VOLC. Gate 2 v2 2026-07-05. */
	static constexpr bool bCoastG2v2_DefaultGentleCoastAllTemplatesEnabled = true;

	inline bool IsCoastG2v2DefaultGentleCoastAllTemplatesEnabled()
	{
		return bCoastG2v2_DefaultGentleCoastAllTemplatesEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	// --- Gate 2 v2 step 3: structural beach pass — REVERTED 2026-07-08 (curtain + no visible rim) ---
	/** Step 3 corner-only lip + exterior hull sync — REVERTED; superseded by 3c. */
	static constexpr bool bCoastG2v2_StructuralBeachPassEnabled = false;

	inline bool IsCoastG2v2StructuralBeachPassEnabled()
	{
		return bCoastG2v2_StructuralBeachPassEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	// --- Gate 2 v2 step 3b: coast-distance field + fine subdiv + quad-straddle lip + hull sync ---
	/** REVERTED 2026-07-09 PIE — subdiv=8 ICE closure comb (honeycomb) + lip/wall mismatch (curtain). */
	static constexpr bool bCoastG2v2_StructuralBeachPass3bEnabled = false;
	/** Finer coast subdiv inside structural beach band (was ICE-01m subdiv=4 → ~5 m micro-quads). */
	static constexpr int32 CoastG2v2_3b_BoundaryFineSubdivisions = 8;
	/** Extend lip cap to inland corners on coast-straddle quads (m beyond band max). */
	static constexpr float CoastG2v2_3b_QuadStraddleFeatherMeters = 12.f;

	inline bool IsCoastG2v2StructuralBeachPass3bEnabled()
	{
		return bCoastG2v2_StructuralBeachPass3bEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	/**
	 * Gate 2 v2 step 3c (2026-07-10): structural beach — shared lip + hull sync.
	 * Keeps ICE subdiv=4 (avoids 3b honeycomb). Uses straddle-feather lip (from 3b) so
	 * coast-straddle inland corners get walk-slope cap. Hull sync clamps ALL wall-edge tops
	 * on band quads (no Freimanis waterline-probe skip — that left curtain in step 3).
	 * Revert: set false.
	 * PERF PRUNE 2026-07-10: OFF — ~130 s PIE cliff (per-quad polyline distance); sea-wall look remains.
	 */
	static constexpr bool bCoastG2v2_StructuralBeachPass3cEnabled = false;
	/** Straddle feather for 3c lip (m beyond band max) — same role as 3b feather. */
	static constexpr float CoastG2v2_3c_QuadStraddleFeatherMeters = 12.f;

	inline bool IsCoastG2v2StructuralBeachPass3cEnabled()
	{
		return bCoastG2v2_StructuralBeachPass3cEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastG2v2StructuralBeachMeshEnabled()
	{
		return (IsCoastG2v2StructuralBeachPassEnabled()
				|| IsCoastG2v2StructuralBeachPass3bEnabled()
				|| IsCoastG2v2StructuralBeachPass3cEnabled())
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline float GetCoastG2v2QuadStraddleFeatherMeters()
	{
		if (IsCoastG2v2StructuralBeachPass3cEnabled())
		{
			return CoastG2v2_3c_QuadStraddleFeatherMeters;
		}
		if (IsCoastG2v2StructuralBeachPass3bEnabled())
		{
			return CoastG2v2_3b_QuadStraddleFeatherMeters;
		}
		return 0.f;
	}

	// --- Gate 2c-R: mesh taper universal beach band — REVERTED 2026-07-05 (pending Gate 2 v2) ---
	/** Mesh-only Gentle lip — REVERTED (curtain + inland shimmer). See junction §2c-R REVERTED + Gate 2 v2. */
	static constexpr bool bCoastC2c_UniversalBeachfrontBandEnabled = false;
	static constexpr float CoastC2c_BeachfrontBaseWidthMeters = 10.f;
	static constexpr float CoastC2c_BeachfrontMinWidthMeters = 8.f;
	static constexpr float CoastC2c_BeachfrontMaxWidthMeters = 15.f;
	static constexpr bool bCoastC2c_BeachfrontSlopeAdaptiveWidthEnabled = true;
	/** Aligns with SeaRootsTierBeachMaxSlopeDeg — foot + mounted walk target. Step 3c A+B: 8° (was 5°) so band can rise ~1.1–2.1 m over 8–15 m. */
	static constexpr float CoastC2c_BeachfrontMaxWalkSlopeDeg = 8.f;
	/** Inland sample span for slope-adaptive width (m). */
	static constexpr float CoastC2c_SlopeSampleInlandMeters = 2.f;
	/** Suppress ICE-01m steep/gentle closure walls inside the beachfront band (§2c universal only — OFF for S1). */
	static constexpr bool bCoastC2c_SuppressIcebergClosureWallsInBand = false;
	/** Skip ICE-01m prism volume entirely inside beachfront band — OFF (caused phantom coastal curtain; do not re-enable without replacement hull). */
	static constexpr bool bCoastC2c_SkipIcebergVolumeInBand = false;
	/** Use coarse coast subdiv (1) inside beachfront band — avoids ICE-01m micro-comb. */
	static constexpr bool bCoastC2c_UseCoarseCoastSubdivInBand = true;

	// --- Gate 2c-H: height-grid universal beach band — PARKED 2026-07-05 (Option A dormant) ---
	// C++ retained; all flags false. Do not enable in PIE — pending Gate 2 v2 structural beach.
	/** §2c-H PARKED — carve Azgaar heights before coast extract (inactive when master flag false). */
	static constexpr bool bCoastC2cH_HeightGridUniversalBeachBandEnabled = false;
	/** Mesh Z: Gentle coast profile inside height-grid beach band (after landing cove wedge). */
	static constexpr bool bCoastC2cH_MeshGentleProfileInBandEnabled = false;
	/** Suppress ICE-01m steep/gentle closure walls inside §2c-H height-grid beach band. */
	static constexpr bool bCoastC2cH_SuppressIcebergClosureWallsInBand = false;
	/** Sloped coast skirt: closure wall bottoms at z=0 (waterline) not −25 m shelf floor in band. */
	static constexpr bool bCoastC2cH_SlopedCoastSkirtToWaterlineEnabled = false;

	inline bool IsCoastC2cHHeightGridBeachBandEnabled()
	{
		return bCoastC2cH_HeightGridUniversalBeachBandEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2cHMeshGentleProfileInBandEnabled()
	{
		return bCoastC2cH_MeshGentleProfileInBandEnabled
			&& IsCoastC2cHHeightGridBeachBandEnabled();
	}

	inline bool IsCoastC2cHSuppressIcebergClosureWallsInBand()
	{
		return bCoastC2cH_SuppressIcebergClosureWallsInBand
			&& IsCoastC2cHHeightGridBeachBandEnabled();
	}

	inline bool IsCoastC2cHSlopedCoastSkirtToWaterlineEnabled()
	{
		return bCoastC2cH_SlopedCoastSkirtToWaterlineEnabled
			&& IsCoastC2cHHeightGridBeachBandEnabled();
	}

	// --- Gate 2c-S1: coast comb beach strip — REVERTED (superseded by §2c-S3) ---
	static constexpr bool bCoastC2cS1_CoastCombBeachStripEnabled = false;
	/** Inside strip: never emit steep closure walls — gentle-cap only (lip Z keeps EdgeMax low). */
	static constexpr bool bCoastC2cS1_GentleWallsOnlyInStrip = true;

	// --- Gate 2c-S3: integrated ICE-01m comb beach band (structural “little beachâ€ pass) ---
	/** Master switch — §2c-S3 per P1C12_Gate2_Coast_Fallback_Junction.md */
	static constexpr bool bCoastC2cS3_IntegratedCombBeachBandEnabled = false;
	/** Pass 1b: apply comb beach band to islet C5 fine grid as well as main island. */
	static constexpr bool bCoastC2cS3_ApplyToIsletFineGrid = true;
	/** Inside band: suppress all ICE closure walls (steep + gentle). Supersedes gentle-only. */
	static constexpr bool bCoastC2cS3_SuppressAllClosureWallsInBand = true;
	/** Inside band: gentle closure walls only (no steep walls) — OFF when suppress-all is on. */
	static constexpr bool bCoastC2cS3_GentleWallsOnlyInBand = false;
	/** Feather relief Z beyond band edge (m) — reduces strip-boundary curtain. */
	static constexpr float CoastC2cS3_BandEdgeFeatherMeters = 2.f;
	/** Pass 1c: coarse subdiv=1 through feather ring (align with lip Z feather). */
	static constexpr bool bCoastC2cS3_CoarseSubdivInFeatherZone = true;

	// --- Gate 2c-S2: authority beach ribbon deck — REVERTED (no visual gain) ---
	/** Master switch — §2c-S2 per P1C12_Gate2_Coast_Fallback_Junction.md */
	static constexpr bool bCoastC2cS2_BeachRibbonDeckEnabled = false;
	/** Segment densify along authority waterline (m). */
	static constexpr float CoastC2cS2_BeachRibbonDensifySpacingMeters = 4.f;
	/** Mesh ribbon on islet / feature coasts as well as MainCoast. */
	static constexpr bool bCoastC2cS2_BeachRibbonIncludeFeatureCoasts = true;
	/** Skip ribbon quads inside §2b landing cove wedge (Beach POI wins). */
	static constexpr bool bCoastC2cS2_BeachRibbonSkipLandingCoveWedge = true;
	/** §2c-S2a: append islet ribbon after C5 — REVERTED (authority poly; no visual gain). */
	static constexpr bool bCoastC2cS2a_IsletRibbonAfterC5Enabled = false;
	/** §2c-S2b: defer islet ribbon until after C5 — REVERTED with S2. */
	static constexpr bool bCoastC2cS2b_IsletRibbonAfterC5Enabled = false;

	inline bool IsCoastC2cS2BeachRibbonDeckEnabled()
	{
		return bCoastC2cS2_BeachRibbonDeckEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2cS2aIsletRibbonAfterC5Enabled()
	{
		return bCoastC2cS2a_IsletRibbonAfterC5Enabled
			&& IsCoastC2cS2BeachRibbonDeckEnabled();
	}

	inline bool IsCoastC2cS2bIsletRibbonAfterC5Enabled()
	{
		return bCoastC2cS2b_IsletRibbonAfterC5Enabled
			&& IsCoastC2cS2BeachRibbonDeckEnabled();
	}

	/** True when islet ribbon is deferred out of the main S2 pass (S2b; legacy S2a). */
	inline bool IsCoastC2cS2IsletRibbonDeferredAfterC5Enabled()
	{
		return IsCoastC2cS2bIsletRibbonAfterC5Enabled()
			|| IsCoastC2cS2aIsletRibbonAfterC5Enabled();
	}

	inline bool IsCoastC2cS3IntegratedCombBeachBandEnabled()
	{
		return bCoastC2cS3_IntegratedCombBeachBandEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2cS1CoastCombBeachStripEnabled()
	{
		return bCoastC2cS1_CoastCombBeachStripEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2cUniversalBeachfrontBandEnabled()
	{
		return bCoastC2c_UniversalBeachfrontBandEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	/** §2c-R alias — mesh Z taper only (no H carve, no ICE hull mutation). */
	inline bool IsCoastC2cRMeshTaperBeachBandEnabled()
	{
		return IsCoastC2cUniversalBeachfrontBandEnabled();
	}

	/** True when any beach-strip mesh pass is active (§2c universal OR §2c-S1 comb strip). */
	inline bool IsCoastC2cBeachStripMeshEnabled()
	{
		return IsCoastC2cUniversalBeachfrontBandEnabled()
			|| IsCoastC2cS1CoastCombBeachStripEnabled()
			|| IsCoastC2cS3IntegratedCombBeachBandEnabled();
	}

	/** Authority-distance queries (scope + lip Z) — comb band, S2 ribbon, or Gate 2 v2 structural beach. */
	inline bool IsCoastC2cDistanceBandQueryEnabled()
	{
		return IsCoastC2cBeachStripMeshEnabled()
			|| IsCoastC2cS2BeachRibbonDeckEnabled()
			|| IsCoastG2v2StructuralBeachMeshEnabled();
	}

	inline float GetCoastC2cBeachfrontBaseWidthMeters()
	{
		return CoastC2c_BeachfrontBaseWidthMeters;
	}

	inline float GetCoastC2cBeachfrontMinWidthMeters()
	{
		return CoastC2c_BeachfrontMinWidthMeters;
	}

	inline float GetCoastC2cBeachfrontMaxWidthMeters()
	{
		return CoastC2c_BeachfrontMaxWidthMeters;
	}

	inline bool IsCoastC2bLandingCovePOIEnabled()
	{
		return bCoastC2b_LandingCovePOIEnabled;
	}

	inline bool IsCoastC2bSeparateCoastSummitProfilesEnabled()
	{
		return bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2bSummitArchetypeHeightBumpEnabled()
	{
		return bCoastC2b_SummitArchetypeHeightBumpEnabled;
	}

	inline bool IsCoastC2bLandingCoveWedgeAngularFeatherEnabled()
	{
		return bCoastC2b_LandingCoveWedgeAngularFeatherEnabled
			&& bCoastC2b_LandingCovePOIEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2bPerimeterCoastSmoothingEnabled()
	{
		return bCoastC2b_PerimeterCoastSmoothingEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	inline bool IsCoastC2bLandingCovePerimeterSmoothEnabled()
	{
		return bCoastC2b_LandingCovePerimeterSmoothEnabled
			&& bCoastC2b_LandingCovePOIEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled
			&& bCoastC2b_PerimeterCoastSmoothingEnabled;
	}

	inline bool IsCoastC2bCoastStraddleQuadWaterlineFlattenEnabled()
	{
		return bCoastC2b_CoastStraddleQuadWaterlineFlattenEnabled
			&& bCoastC2b_SeparateCoastSummitProfilesEnabled;
	}

	/** TEMP dev: pink / orange / yellow down arrows for landing cove, caldera, summit. */
	static constexpr bool bCoastC2b_DevPOIMarkersEnabled = true;
	/** Prefer camera-facing sprite billboards; falls back to 3D cylinder+cone when false or texture missing. */
	static constexpr bool bCoastC2b_DevPOIMarkersUseBillboard = true;
	/** Use pre-colored pink/yellow sprite PNGs (no CPU tint bake). */
	static constexpr bool bCoastC2b_DevPOIMarkersUseColorizedBillboardSprites = true;
	/** LOCKED 3D-fallback only. Billboards use runtime PNG aspect (see IH_Billboard_Sprite_Pipeline.md). */
	static constexpr float CoastC2b_DevPOIMarkerBillboardWidthScale = 7.9f;
	/** LOCKED billboard height multiplier; width = ScaledHeight * TextureAspectWH (~0.14 cropped arrows). */
	static constexpr float CoastC2b_DevPOIMarkerBillboardUniformScale = 2.f;
	/** Pre-colored landing cove (pink) sprite. */
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardPinkTextureObjectPath =
		TEXT("/Game/InvisibleHand/DevPOI/T_ArrowIndicatorSpritePink.T_ArrowIndicatorSpritePink");
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardPinkTextureFileRelative =
		TEXT("InvisibleHand/DevPOI/ArrowIndicatorSpritePink.png");
	/** Pre-colored summit (yellow) sprite. */
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardYellowTextureObjectPath =
		TEXT("/Game/InvisibleHand/DevPOI/T_ArrowIndicatorSpriteYellow.T_ArrowIndicatorSpriteYellow");
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardYellowTextureFileRelative =
		TEXT("InvisibleHand/DevPOI/ArrowIndicatorSpriteYellow.png");
	/** Legacy grayscale sprite + CPU tint (fallback when colorized PNGs missing). */
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardTextureObjectPath =
		TEXT("/Game/InvisibleHand/DevPOI/T_ArrowIndicatorSprite.T_ArrowIndicatorSprite");
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardTextureFileRelative =
		TEXT("InvisibleHand/DevPOI/ArrowIndicatorSprite.png");
	/** Secondary line-art overlay (legacy grayscale pipeline only). */
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardSecondaryTextureObjectPath =
		TEXT("/Game/InvisibleHand/DevPOI/T_ArrowIndicatorSpriteSecondary.T_ArrowIndicatorSpriteSecondary");
	static constexpr const TCHAR* CoastC2b_DevPOIMarkerBillboardSecondaryTextureFileRelative =
		TEXT("InvisibleHand/DevPOI/ArrowIndicatorSpriteSecondary.png");
	/** DrawDebug down-arrow shaft length (cm) — island scale; dev markers. */
	static constexpr float CoastC2b_DevPOIMarkerArrowLengthCm = 1666.f;
	static constexpr float CoastC2b_DevPOIMarkerHoverCm = 26666.f;
	static constexpr float CoastC2b_DevPOIMarkerLineThicknessCm = 116.f;
	static constexpr float CoastC2b_DevPOIMarkerShaftRadiusCm = 666.f;
	static constexpr float CoastC2b_DevPOIMarkerArrowHeadLengthCm = 1666.f;
	/** Arrow float anchor ASL above IslandMesh surface at POI XY (100 m). */
	static constexpr float CoastC2b_DevPOIMarkerFloatAboveSurfaceCm = 10000.f;
	/** TEMP dev: orange caldera particle plume at caldera XY. */
	static constexpr bool bCoastC2b_DevCalderaPlumeEnabled = true;
	/** Primary path: Niagara Examples NS_Smoke_Plume wispy puff (not mesh column). */
	static constexpr bool bCoastC2b_DevCalderaPlumePreferNiagaraExamples = true;
	/** Static mesh column debug only. */
	static constexpr bool bCoastC2b_DevCalderaPlumePreferRibbonColumn = false;
	/** Uniform Niagara puff scale multiplier (10× the original small gray plume). */
	static constexpr float CoastC2b_DevCalderaPlumeVisualScaleMultiplier = 10.f;
	/** XY component scale baseline for NS_Smoke_Plume puff profile (pre-10×). */
	static constexpr float CoastC2b_DevCalderaPlumeNiagaraBaseXYScale = 1.25f;
	/** Z-only ribbon stretch (10× taller, same width) on top of visual scale. */
	static constexpr float CoastC2b_DevCalderaPlumeNiagaraVerticalRibbonStretch = 10.f;
	/** Additional Z-only length multiplier on locked ribbon (4× taller). */
	static constexpr float CoastC2b_DevCalderaPlumeNiagaraVerticalLengthMultiplier = 4.f;
	/** Spawn-rate boost for plume dispersion (conical dissipation feel). */
	static constexpr float CoastC2b_DevCalderaPlumeNiagaraDispersionSpawnRateMultiplier = 2.25f;
	/** Upper-altitude Niagara tiers widen/disperse plume (same orange emissive). */
	static constexpr bool bCoastC2b_DevCalderaPlumeNiagaraConicalDispersionTiers = true;
	static constexpr int32 CoastC2b_DevCalderaPlumeNiagaraDispersionTierCount = 2;
	/** Plume top ALS Z = caldera surface ALS Z × this multiplier (2 = double caldera altitude). */
	static constexpr float CoastC2b_DevCalderaPlumeTargetTopZMultiplier = 2.f;
	/** Minimum visible rise when caldera is near sea level. */
	static constexpr float CoastC2b_DevCalderaPlumeMinRiseHeightCm = 500.f;
	/** Niagara Z-scale reference rise (cm) for NS_Smoke_Plume tuning. */
	static constexpr float CoastC2b_DevCalderaPlumeNiagaraReferenceRiseCm = 15000.f;
	static constexpr float CoastC2b_DevCalderaPlumeNiagaraMaxScale = 64.f;
	/** Primary Niagara Examples vertical smoke plume. */
	static constexpr const TCHAR* CoastC2b_DevCalderaPlumeNiagaraSystemPath =
		TEXT("/Game/NiagaraExamples/FX_Smoke/NS_Smoke_Plume.NS_Smoke_Plume");
	static constexpr int32 CoastC2b_DevCalderaPlumeRibbonSegmentCount = 24;
	static constexpr int32 CoastC2b_DevCalderaPlumeSpriteCount = 36;
	static constexpr float CoastC2b_DevCalderaPlumeFallbackRiseHeightCm = 15000.f;
	static constexpr float CoastC2b_DevCalderaPlumeRiseSpeedCmPerSec = 320.f;
	static constexpr float CoastC2b_DevCalderaPlumeRibbonRadiusCm = 850.f;
	static constexpr float CoastC2b_DevCalderaPlumeSpriteRadiusCm = 380.f;

	/** Rise height so plume top ALS = caldera surface ALS × TargetTopZMultiplier. */
	inline float ComputeGate2bCalderaPlumeRiseHeightCm(const float CalderaSurfaceZCm)
	{
		const float RiseCm = CalderaSurfaceZCm
			* (CoastC2b_DevCalderaPlumeTargetTopZMultiplier - 1.f);
		return FMath::Max(CoastC2b_DevCalderaPlumeMinRiseHeightCm, RiseCm);
	}

	inline float ComputeGate2bCalderaPlumeTopZCm(const float CalderaSurfaceZCm)
	{
		return CalderaSurfaceZCm + ComputeGate2bCalderaPlumeRiseHeightCm(CalderaSurfaceZCm);
	}

	inline bool IsCoastC2bDevCalderaPlumePreferRibbonColumn()
	{
		return bCoastC2b_DevCalderaPlumePreferRibbonColumn;
	}

	inline bool IsCoastC2bDevCalderaPlumeEnabled()
	{
		return bCoastC2b_DevPOIMarkersEnabled && bCoastC2b_DevCalderaPlumeEnabled;
	}

	inline bool IsCoastC2bDevPOIMarkersEnabled()
	{
		return bCoastC2b_DevPOIMarkersEnabled;
	}

	inline bool IsCoastC2bDevPOIMarkersUseBillboard()
	{
		return bCoastC2b_DevPOIMarkersEnabled && bCoastC2b_DevPOIMarkersUseBillboard;
	}

	inline bool IsCoastC2bDevPOIMarkersUseColorizedBillboardSprites()
	{
		return IsCoastC2bDevPOIMarkersUseBillboard() && bCoastC2b_DevPOIMarkersUseColorizedBillboardSprites;
	}

	// --- C1b: marching-squares coast extract (ICE-01m+ — poly/extract only; no mesh deformation) ---

	/** C1b: replace corner-grid boundary walk with marching-squares edge crossings. */
	static constexpr bool bCoastC1b_MarchingSquaresExtractEnabled = true;
	/** C1b: interpolate edge crossing along height (vs midpoint) on land/water transitions. */
	static constexpr bool bCoastC1b_BilinearEdgeCrossingsEnabled = true;
	/** C1b: saddle cases 5/10 connect through center-inside diagonal when true. */
	static constexpr bool bCoastC1b_SaddleConnectCenterInside = true;
	/** C1b: merge MS graph nodes at coincident XY before loop walk (fixes deg1 dangling ends). */
	static constexpr bool bCoastC1b_Deg1CoincidentNodeMergeEnabled = true;
	/** C1b: bridge remaining deg1 endpoint pairs within cell-scale distance. */
	static constexpr bool bCoastC1b_Deg1ChainBridgeEnabled = true;
	/** C1b: reject MS loops smaller than this vert count — fall back to corner extract. */
	static constexpr int32 CoastC1b_MinLoopRejectExtractVerts = 100;

	inline bool IsCoastC1bMarchingSquaresExtractEnabled()
	{
		return bCoastC1b_MarchingSquaresExtractEnabled;
	}

	inline bool IsCoastC1bDeg1CoincidentNodeMergeEnabled()
	{
		return IsCoastC1bMarchingSquaresExtractEnabled()
			&& bCoastC1b_Deg1CoincidentNodeMergeEnabled;
	}

	inline bool IsCoastC1bDeg1ChainBridgeEnabled()
	{
		return IsCoastC1bMarchingSquaresExtractEnabled()
			&& bCoastC1b_Deg1ChainBridgeEnabled;
	}
	/** Macro-wavelength edge noise only (no meso/micro spurs). */
	static constexpr bool bCoastAuthorityMacroNoiseEnabled = true;
	/** Max straight segment (km) before authority breaker inserts a kick vertex. */
	static constexpr float CoastAuthorityMaxStraightSegmentKm = 0.03f;
	/** Scale on CoastNoiseMacroAmpKm for authority silhouette (applied after resample @768). */
	static constexpr float CoastAuthorityMacroNoiseAmpScale = 0.55f;
	/** Phase F3: Chaikin on authority coast after breaker/macro noise — rounds grid sawteeth before mesh bake. */
	static constexpr int32 CoastAuthoritySmoothIterations = 2;
	/** C1a Exp2: second hairpin pass after subdivide/resample removed 169–294 more verts — off for authority. */
	static constexpr bool bCoastAuthoritySecondHairpinSanitizeEnabled = false;
	/** C1a Exp3: relaxed hairpin dot for authority bake (display stroke keeps CoastHairpinBacktrackDot). */
	static constexpr float CoastAuthorityHairpinBacktrackDot = -0.92f;
	/** C1a Exp3: keep macro/breaker spurs — outward bulge strip was eating inlet detail. */
	static constexpr bool bCoastAuthoritySkipOutwardBulgeStrip = true;
	/** C1a Exp3: resample up to MaxVerts after sanitize when count drops below cap. */
	static constexpr bool bCoastAuthorityResampleFillToCap = true;
	/** Dot threshold for hairpin vertex removal (more negative = stricter). */
	static constexpr float CoastHairpinBacktrackDot = -0.78f;
	/** Max verts in a self-intersection loop clip (small = breaker spurs only). */
	static constexpr int32 CoastSanitizeMaxLoopVerts = 28;
	/** Outward breaker/macro spur: min perpendicular bulge height (km). */
	static constexpr float CoastSpurMinBulgeHeightKm = 0.012f;
	/** Outward spur: max chord length across spur base (km). */
	static constexpr float CoastSpurMaxChordKm = 0.18f;
	/** Resample + sanitize MainCoast to CoastRenderPolyVerts once at cell-map build. */
	static constexpr bool bCoastAuthorityBakeAtExtract = true;
	/** PIE coast stroke: symmetric centerline ribbon (not outward-only miter extrusion). */
	static constexpr bool bCoastStrokeSymmetricCenterline = true;
	/**
	 * B2 land interior: heightfield fine-grid masked by baked MainCoast (matches minimap).
	 * Ear-clip with lake holes corrupts bays and produces triangular voids.
	 */
	static constexpr bool bCoastB2_FineGridInteriorFromAuthority = true;
	/** Option 2: waterline skirt bridging fine-grid land inset ↔ authority coast / gold stroke. */
	/** Off: authority skirt bridges narrow inlet mouths (closed bay vs minimap). */
	static constexpr bool bCoastB2_FineGridCoastSkirtEnabled = false;
	/** Skirt inset depth in fine-grid cell units (~1 cell closes land/stroke margin). */
	static constexpr float CoastFineGridSkirtDepthCells = 1.0f;

	inline bool IsCoastAuthorityBakeAtExtractActive()
	{
		return bCoastAuthorityBakeAtExtract;
	}

	inline bool IsCoastC1dCardinalStairstepBreakerEnabled()
	{
		return bCoastC1d_CardinalStairstepBreakerEnabled;
	}

	inline bool IsCoastC1dGridArtifactRemedyEnabled()
	{
		return bCoastC1d_CardinalStairstepBreakerEnabled || bCoastC1d_CardinalCornerChamferEnabled;
	}

	// --- C1e: PIE topography silhouette follows MainCoast authority (not fine-grid land AABB) ---

	/**
	 * C1e-a ear-clip triangulation (sparse ~766 triangles). Causes conspicuous splayed/radial
	 * facet ridges on High/Volcanic relief — keep false. Rollback only for A/B comparison.
	 */
	static constexpr bool bCoastC1e_AuthoritySilhouetteEarClipPreferred = false;
	/** C1e-b: fine-grid interior masked by MainCoast @768 (dense relief + authority XY clip). */
	static constexpr bool bCoastC1e_FineGridInteriorFromAuthorityBoundary = true;
	/** When ear-clip interior fails, fall back to fine-grid masked interior (legacy B2). */
	static constexpr bool bCoastC1e_FallbackFineGridInterior = true;

	inline bool IsCoastC1eAuthoritySilhouetteEarClipPreferred()
	{
		return bCoastC1e_AuthoritySilhouetteEarClipPreferred;
	}

	inline bool IsCoastC1eFineGridInteriorFromAuthorityBoundaryEnabled()
	{
		return bCoastC1e_FineGridInteriorFromAuthorityBoundary;
	}

	inline bool IsCoastC1eAuthoritySilhouetteActive()
	{
		return bCoastC1e_AuthoritySilhouetteEarClipPreferred
			|| bCoastC1e_FineGridInteriorFromAuthorityBoundary;
	}

	inline bool IsCoastC1eFallbackFineGridInteriorEnabled()
	{
		return bCoastC1e_FallbackFineGridInterior;
	}

	inline bool IsCoastAuthoritySecondHairpinSanitizeEnabled()
	{
		return bCoastAuthoritySecondHairpinSanitizeEnabled;
	}

	inline bool IsCoastPhaseB2SlopeTierShelfActive()
	{
		return bCoastPhaseB2_SlopeTierShelf && !bCoastPhaseA_CrispCoastOnly;
	}

	// --- C5: detached islet topography (FeatureCoasts mesh reactivation) ---

	/** Mesh non-main FeatureCoasts with C1e-b fine-grid + per-islet authority coast. */
	static constexpr bool bCoastC5_IsletTerrainMeshEnabled = true;
	/** C5-01 / MIN-islet: per-islet Sea Roots bands + gold coast on minimap. */
	static constexpr bool bCoastC5_IsletMinimapSeaRootsEnabled = true;
	/** Skip labeled features smaller than this (noise fragments). */
	static constexpr int32 CoastC5_MinIsletLandCells = 4;
	static constexpr int32 CoastC5_IsletRenderPolyVertsMin = 48;
	static constexpr int32 CoastC5_IsletRenderPolyVertsMax = 256;

	inline bool IsCoastC5IsletTerrainMeshEnabled()
	{
		return bCoastC5_IsletTerrainMeshEnabled && IsCoastPhaseB2SlopeTierShelfActive();
	}

	inline bool IsCoastC5IsletMinimapSeaRootsEnabled()
	{
		return IsCoastC5IsletTerrainMeshEnabled() && bCoastC5_IsletMinimapSeaRootsEnabled;
	}

	// --- IB-1: floating-iceberg SSOT (per-feature waterline + shelf-floor bake at cell-map build) ---

	/** Bake FeaturePresentationExtents on BuildIslandCellMap (generation only; no mesh change). */
	static constexpr bool bCoastIB1_FeaturePresentationExtentBakeEnabled = true;

	inline bool IsCoastIB1FeaturePresentationExtentBakeEnabled()
	{
		return bCoastIB1_FeaturePresentationExtentBakeEnabled && IsCoastPhaseB2SlopeTierShelfActive();
	}

	/** @deprecated Ring annulus hull — retired; use IsCoastFloatingIcebergEnabled. */
	static constexpr bool bCoastIB2_IcebergHullEnabled = false;

	inline bool IsCoastIB2IcebergHullEnabled()
	{
		return bCoastIB2_IcebergHullEnabled && IsCoastIB1FeaturePresentationExtentBakeEnabled();
	}

	/** ICE-01: unified IslandMesh iceberg 0→−25 m via heightmap submerged taper (see P1C10_FloatingIceberg_Canon.md). */
	static constexpr bool bCoastFloatingIcebergEnabled = true;
	/** Retired on IslandMesh when floating iceberg is active — shelf is IslandMesh only. */
	/** ICE-01c: coarser fine factor for seaward shelf tessellation (1 = same as SSTO). */
	static constexpr int32 IcebergShelfFineFactorDivisor = 2;
	/** ICE-01d: coast stitch + channel-only edge walls (see P1C10_FloatingIceberg_Canon.md §7.1). */
	static constexpr bool bCoastFloatingIcebergHullClosureEnabled = true;
	/** ICE-01e: coast stitch band @ z=0 — off for ghost-ring A/B; reconcile to C1e before re-enable. */
	static constexpr bool bCoastFloatingIcebergCoastStitchEnabled = false;
	/** ICE-01d: vertical waterline perimeter hull — off; PIE shore curtain (~35k–74k hull tris/island). */
	static constexpr bool bCoastFloatingIcebergWaterlinePerimeterHullEnabled = false;
	/** ICE-01g: Freimanis-style coast skin — continuous top to z=0; −25 m cap + vertical walls inland/channels only. */
	static constexpr bool bCoastFloatingIcebergCoastSkinEnabled = true;
	/** ICE-01h: ghost-ring fix — relax C1e sea clip + skip shelf flat z≈0 coast ring (see P1C10_FloatingIceberg_Canon.md §7.1). */
	/** ICE-01i: unified C1e grid + submerged seaward shelf; retires separate shelf pass when coast skin on. */
	/** ICE-01k: Freimanis coast skin — waterline authority, no Z snap, no seaward shelf tops (ICE-02 restores shelf). */
	/** ICE-01l: sheer-cliff exterior walls (> min top ASL) + straddling coast cells — closes coastal pillar gaps. */
	/** ICE-01p: cliff seam walls on steep edges straddling authority waterline — closes subdiv vertical slits. */
	/**
	 * Phase F4: ICE-01l/01p exterior cliff-wall trigger (m ASL). Edge max Z must exceed this before a
	 * vertical seam wall is extruded. Decoupled from ShelfShoreTopMeters (+5 m shelf band) so shelf
	 * parity and ocean tint bands are unchanged. 22 m — Phase F4 tune above Gate 4 beach (+15 m).
	 */
	static constexpr float CoastIcebergCliffExteriorWallMinTopMeters = 22.f;
	/**
	 * Phase F4b v2: gentle closure — one wall segment from min(edgeZ, cap) → shelf floor only.
	 * No z=0 skirt ring (v1 caused shallow-water honeycomb). Closes F4 open pockets without fangs.
	 */
	static constexpr bool bCoastIcebergGentleClosureWallEnabled = true;
	/**
	 * Legacy gate (m ASL) for gentle walls on non-exterior paths. Exterior WWF coast closure now
	 * uses shelf-floor relative test (see ComputeIcebergChannelEdgeWallMask) so z≈0 Freimanis
	 * tips still get walls to −25 m (2026-07-09 solid shelf fix).
	 */
	static constexpr float CoastIcebergGentleClosureWallMinTopMeters = 0.01f;
	/** Cap gentle wall top height (m ASL) — limits vertical extent vs full cliff. */
	static constexpr float CoastIcebergGentleClosureWallMaxTopMeters = 12.f;
	/**
	 * TEMP DEV dry-view (2026-07-15): omit IslandMesh iceberg **exterior closure walls**
	 * (gentle + steep ICE faces 0→−25 m, incl. outward skirt) AND Freimanis underwater land
	 * slope skirt (wet corners → waterline; submerged quads skipped). SeaRoots top rises to
	 * z=0 with matte unlit sand so tan frustum replaces the dark hull read.
	 * Keeps: dry land tops, Phase A soup (from remaining volume caps), WWF bake data.
	 * Active only with HideOcean. Restore: false.
	 */
	/**
	 * Dry-shoulder lab: hide IslandMesh ICE/Freimanis walls; SeaRoots top → z=0.
	 * Independent of HideOcean (runtime: IHDevViewRuntime). Default OFF = Gold→Magenta walls visible.
	 */
	static constexpr bool bDevDemo_HideIslandMeshIcebergExteriorWalls = false;

	/**
	 * TEMP DEV: slight outward slope on gentle WWF shelf faces (bottoms seaward at −25 m).
	 * v2 (2026-07-09): waterline-shared offset per corner XY (not per-edge sum).
	 * When coast-polyline shared bottom ring is ON, per-quad gentle walls + skirt are suppressed
	 * (ring owns the outer face). Steep walls stay vertical.
	 */
	static constexpr bool bDevDemo_OutwardGentleShelfSkirtEnabled = true;
	/** Horizontal seaward offset at shelf floor (m). 10 m ≈ ~22° from vertical — visually obvious vs 3 m. */
	static constexpr float CoastDevDemo_OutwardGentleShelfSkirtMeters = 10.f;
	/**
	 * PIE shimmer fix (2026-07-10): pull closure wall tops slightly below lip / land top Z (cm).
	 * Negative = below. Avoids coplanar z-fight with 3c lip / Freimanis tops.
	 */
	static constexpr float CoastICE01_ClosureWallTopZBiasCm = -2.f;
	/**
	 * PIE shimmer fix: lift wall bottoms + bottom caps slightly above shelf floor (cm).
	 * Positive = above −25 m. Avoids coplanar fight with SeaRoots frustum top / shelf caps.
	 */
	static constexpr float CoastICE01_ClosureWallBottomZBiasCm = 1.f;
	/**
	 * Coast-polyline walls + shared bottom ring — REVERTED 2026-07-10 PIE.
	 * Emitted many segments (polyRingSeg≈8k–12k) but tops sat on authority WaterlineKm
	 * inland of Freimanis tip overhang; per-quad gentle walls were suppressed → no visible
	 * shelf faces (see-through under top cap to SeaRoots). Do not re-enable without attaching
	 * the ring to the meshed coast silhouette (not inland waterline authority alone).
	 */
	static constexpr bool bCoastICE01_CoastPolylineSharedBottomRingEnabled = false;

	inline bool IsDevDemoOutwardGentleShelfSkirtEnabled()
	{
		return bDevDemo_OutwardGentleShelfSkirtEnabled
			&& bCoastIcebergGentleClosureWallEnabled;
	}

	inline float GetDevDemoOutwardGentleShelfSkirtCm()
	{
		return CoastDevDemo_OutwardGentleShelfSkirtMeters * 100.f;
	}

	/** Skip −25 m bottom cap when quad is shallow coast with gentle walls only — OFF for WWF solid hull review (2026-07-09); was causing hollow underside at −18 m ASL. */
	static constexpr bool bCoastIcebergSuppressShallowBottomCap = false;
	static constexpr float CoastIcebergShallowBottomCapMaxTopMeters = 14.f;
	/** ICE-01m: LOCKED FALLBACK — signed off 2026-06-19. Full flag table + lessons: P1C10_FloatingIceberg_Canon.md §7.2 */
	/** C1f: IslandMesh coast silhouette — exterior corner clip (ICE-01k/l safe). */
	/** C1f conforming band — OFF: duplicate waterline ring + cliff walls = shore curtain regression. */
	static constexpr bool bCoastFloatingIcebergCoastConformingBandEnabled = false;
	static constexpr bool bCoastC1f_ExteriorCornerClipEnabled = false;
	/** C1f-2: coast-facing corner clip — RETIRED with exterior clip (T-junction top holes + cliff shaft comb). */
	static constexpr bool bCoastC1f_CoastFacingCornerClipEnabled = false;
	/** C1f-3: snap all coast-band corners when cell touches heightfield grid envelope — RETIRED (T-junction top holes + cliff shafts). */
	static constexpr bool bCoastC1f_GridEnvelopeCornerClipEnabled = false;
	/** C1f: coast-band half-width multiplier (× fine half-cell) for corner clip. */
	static constexpr float CoastC1f_CornerClipBandHalfCellFactor = 2.5f;
	/** C1f: finer coast-boundary subdivisions — 4 matches default (6 caused cliff comb + top holes). ICE-01m locked. */
	static constexpr int32 CoastFloatingIcebergBoundaryFineSubdivisions = 4;
	/** C1f: re-run C1d on MainCoast @768 — OFF (ICE-01m: triple-C1d stack caused waterline follies). */
	static constexpr bool bCoastC1f_AuthorityPostResampleC1dEnabled = false;
	/** C1f: final C1d after authority bake — OFF (ICE-01m). */
	static constexpr bool bCoastC1f_AuthorityFinalC1dEnabled = false;
	/** C1f: second extract C1d — OFF (ICE-01m: outward kicks → mesh follies). */
	static constexpr bool bCoastC1f_ExtractSecondC1dEnabled = false;
	/** ICE-01m: straddling coast cells need ≥2 corners inside (not 1) — blocks C1d-kick follies. */
	static constexpr int32 CoastIcebergStraddleMinCornersInside = 2;
	/**
	 * Tip-extension clip (2026-07-09): cull coast fine quads that sit mostly seaward of the
	 * authority waterline but still pass height sea-clip (h>0.01 on a tip). Removes thin z≈0
	 * tabs beyond the gold coastline. Does NOT deform C1e corners (C1f mesh clip stays OFF).
	 * Revert: set false.
	 * PERF PRUNE 2026-07-10: OFF — tipClippedQuads=0 in PIE (predicate-only cost).
	 */
	static constexpr bool bCoastICE01_TipExtensionClipEnabled = false;
	/** Min corners inside WaterlineKm to keep a center-outside coast quad (matches straddle). */
	static constexpr int32 CoastICE01_TipExtensionMinCornersInside = 2;
	/** ICE-02: restore submerged seaward shelf tops on unified C1e grid (retires ICE-01k suppression). */
	/**
	 * OFF — 2026-07-11 PIE: enabling reintroduced ICE-01k “ghost annulusâ€
	 * (`unifiedSeawardShelf cells≈61k–79k`, flat z≈−25 m triangular checkerboard through water;
	 * worse with DEV-WWF ocean also at −25 m). Keep false until shelf tops are sloped/opaque
	 * and signed without coplanar ocean z-fight. Frustum stays DeepOuter (ICE-02e–m).
	 */
	static constexpr bool bCoastICE02_SeawardShelfTopsEnabled = false;
	/** ICE-02: per-island SeaRoots frustum mesh −25→−250 m from IB-1 shelf contour bake. */
	static constexpr bool bCoastICE02_IslandFrustumEnabled = true;
	/** ICE-02-PIE junction (LOCKED 2026-07-03): frustum stays visible in PIE — only working lit material path. */
	static constexpr bool bCoastICE02_KeepFrustumVisibleInPie = true;
#if !UE_BUILD_SHIPPING
	/** ICE-02b: PIE-only frustum mesh stats + 30 s unlit sandy-orange pulse (LOCKED junction 2026-07-03). */
	static constexpr bool bCoastICE02b_DebugFrustumVisibilityEnabled = true;
	static constexpr float CoastICE02b_DebugFrustumTintSeconds = 30.f;
	/** ICE-02c: persistent unlit fuchsia — OFF (junction: do not regress from SandyGrey lit). */
	static constexpr bool bCoastICE02c_DevFrustumFuchsiaUnlit = false;
	/** ICE-02d: lime exterior wireframe overlay on SeaRoots frustum (dev). */
	/** ICE-02d dev shell wireframe — off in GA; vertical struts read as coastal curtains underwater. */
	static constexpr bool bCoastICE02d_DevFrustumExteriorWireframe = false;
	static constexpr float CoastICE02d_FrustumWireframeThicknessCm = 650.f;
	static const FLinearColor CoastICE02d_FrustumWireframeColor = FLinearColor(0.45f, 1.f, 0.55f, 1.f);
#endif
	/** ICE-02e: frustum top @ DeepOuter butt joint with IslandMesh −25 m shelf floor (retires in-frustum shelf bands). */
	static constexpr bool bCoastICE02e_FrustumTopAligned = true;
	/** ICE-02e: nudge frustum top below shelf floor to reduce z-fight with IslandMesh end cap (cm). */
	static constexpr float CoastICE02e_FrustumTopOffsetBelowShelfFloorCm = 0.f;
	/** ICE-02f: place azimuth radii at IB-1 AzimuthOriginLocalCm (minimap/GIS parity). */
	static constexpr bool bCoastICE02f_FrustumAzimuthOrigin = true;
	/** ICE-02g: shelf-floor top ring uses baked DeepOuter radii without summit WaterlineScale. */
	static constexpr bool bCoastICE02g_FrustumDeepOuterUnscaled = true;
	/** ICE-02j: outward abyss flare TopRadiiCm → BottomRadiiCm over full −25→−250 m span. */
	static constexpr bool bCoastICE02j_FrustumOutwardAbyssSlope = true;
	/** ICE-02h: top ring radii from DeepOuter polyline raycast @ AzimuthOrigin (not index resample). */
	static constexpr bool bCoastICE02h_FrustumPolylineTopRadii = true;
	/** ICE-02k: skip rebuild-time TopRadiiCm re-smooth (bake already smoothed). */
	static constexpr bool bCoastICE02k_SkipFrustumTopResmooth = true;
	/** ICE-02l: segment-normal DeepOuter polyline shell (exact −25 m butt joint). */
	static constexpr bool bCoastICE02l_FrustumPolylineShell = true;
	/**
	 * ICE-02t / A3 (2026-07-11): continuous wall envelope when IslandMesh lip is unavailable
	 * (islets / extract fail). Step B: main frustum top prefers ICE-02n lip; A3 does not override lip top.
	 */
	static constexpr bool bCoastICE02t_FrustumAzimuthOuterEnvelope = true;
	static constexpr bool bCoastICE02t_KeepOutwardWallTrianglesOnly = true;
	static constexpr bool bCoastICE02t_SkipBottomCap = false;
	static constexpr int32 CoastICE02t_EnvelopeSmoothPasses = 2;
	/**
	 * ICE-02n: frustum top ring SSOT = IslandMesh −25 m WWF shelf outer lip (canon floating iceberg).
	 * Step B (2026-07-12): ON — continuous lip via shelf-Z azimuth max-radius envelope (not longest
	 * local −25 m blob). Top XY from lip; walls flare via ICE-02j. A3 DeepOuter top only if lip missing.
	 */
	static constexpr bool bCoastICE02n_FrustumTopFromIslandMesh = true;
	/**
	 * ICE-02n Step B: min fraction of azimuth bins that must hold a shelf-Z sample before the
	 * continuous envelope is accepted (else longest-loop / fallback).
	 */
	static constexpr float CoastICE02n_LipAzimuthCoverageMinFraction = 0.70f;
	/**
	 * ICE-02n Path B / XY flush (2026-07-12): frustum top = azimuth max of
	 * (1) ICE wall-bottom XY (incl. gentle skirt) + (2) IslandMesh verts in near-shelf Z band.
	 * Superseded for rim match by Path F when enabled.
	 */
	static constexpr bool bCoastICE02n_XyFlushWallBottomAndNearShelf = true;
	/** Catch stepped WWF shelf faces above −25 m (m). Matches gentle cliff band (~gentleMaxTopM). */
	static constexpr float CoastICE02n_XyFlushNearShelfCatchMeters = 12.f;
	/**
	 * ICE-02n Path F (2026-07-13): frustum TopRadii + Path E solid top disk = IslandMesh
	 * WWF shelf-floor bottom outer rim only (|Z+25m|≤tol). Excludes Path B near-shelf catch
	 * and skirted wall bottoms (those caused excess lid beyond dark shelf bottom).
	 * F-simple: walls + disk share this SSOT.
	 */
	static constexpr bool bCoastICE02n_PathF_BottomCapRimOnly = true;
	/**
	 * Path G (2026-07-13): outer shelf-cap boundary loop as wall+disk SSOT.
	 * REVERTED 2026-07-13: meanR picked tiny far tip loops (rawLoopVerts=14) → wallTopR≈1000cm
	 * → missing frustum sectors (yellow box). Safe juncture = Path E polar disk + Path F walls.
	 */
	static constexpr bool bCoastICE02n_PathG_OuterBoundaryLoopWallAndDisk = false;
	/** Reject candidate loops whose maxR is below this fraction of Path F envelope maxR. */
	static constexpr float CoastICE02n_PathG_OuterLoopMinMaxRFractionOfPathF = 0.70f;
	/**
	 * Phase 1 (2026-07-13): diagnose IslandMesh WWF −25 m shelf-floor **outer rim** candidates.
	 * Logs only — does NOT change Path E disk or Path F walls (safe juncture preserved).
	 */
	/** Phase 1 rim diagnostic — OFF 2026-07-15 PERF (accepted=0 always; ~30s waste). */
	static constexpr bool bCoastICE02n_Phase1_ShelfFloorRimDiagnostic = false;
	/** Magenta wire: best-coverage Phase1 loop preview (local fragments). */
	static constexpr bool bCoastICE02n_Phase1_ShelfFloorRimDiagnosticWire = false;
	static constexpr int32 CoastICE02n_Phase1_RimMinVertCount = 64;
	static constexpr float CoastICE02n_Phase1_RimMinAzimuthCoverage = 0.85f;
	static constexpr float CoastICE02n_Phase1_RimMinMaxRFractionOfPathF = 0.85f;
	static constexpr float CoastICE02n_Phase1_RimMinAreaFractionOfPathF = 0.50f;
	static constexpr int32 CoastICE02n_Phase1_RimAzimuthBins = 192;
	/**
	 * Phase A (2026-07-14): during IslandMesh bake, collect WWF shelf **bottom-cap** tris
	 * (downward faces @ ShelfFloor) into a dedicated buffer → IslandMesh section 1.
	 * Exact underside geometry SSOT for later frustum top copy. Does not change SeaRoots.
	 */
	static constexpr bool bCoastICE02n_PhaseA_ShelfBottomCapSection = true;
	/**
	 * Phase B (2026-07-14): cyan wire of Phase A bottom-cap **geometric** boundary edges
	 * (XY-quantized; shared interior edges cancel — Phase A quads are unwelded by index)
	 * + gate log vs Path F. Magenta Phase1 preview remains. No frustum geometry change.
	 * PERF 2026-07-15: never index-key boundary edges (drew full quad grid every Tick).
	 */
	static constexpr bool bCoastICE02n_PhaseB_ShelfBottomCapContourWire = false;
	static constexpr float CoastICE02n_PhaseB_RimMinAzimuthCoverage = 0.85f;
	static constexpr float CoastICE02n_PhaseB_RimMinMaxRFractionOfPathF = 0.85f;
	static constexpr int32 CoastICE02n_PhaseB_RimAzimuthBins = 192;
	/** Quantize XY (cm) when merging coincident Phase A verts for contour extract. */
	static constexpr float CoastICE02n_PhaseB_EdgeQuantizeCm = 2.f;
	/** Hard cap on cyan DrawDebug segments per island per Tick (contour should be << this). */
	static constexpr int32 CoastICE02n_PhaseB_MaxWireSegments = 4096;
	static constexpr float CoastICE02n_PhaseB_WireThicknessCm = 48.f;
	/**
	 * Phase C soup→SeaRoots lid — OFF (Alt 2 2026-07-15). Too heavy; IslandMesh keeps underside.
	 * Frustum top polygon = FIHShelfBottomCapContour (Phase D), not triangle soup clone.
	 */
	static constexpr bool bCoastICE02n_PhaseC_FrustumTopFromPhaseASoup = false;
	/**
	 * Alt 2 / Phase D: bake FIHShelfBottomCapContour for DEV magenta rim (IslandMesh −25 m join).
	 * Wall handoff to SeaRoots is gated separately — see bCoastICE02n_Alt2_ApplyContourToSeaRootsWalls.
	 */
	static constexpr bool bCoastICE02n_PhaseD_FrustumWallsFromPhaseAOuterContour = true;
	static constexpr int32 CoastICE02n_PhaseD_OuterContourSamples = 1024;
	static constexpr int32 CoastICE02n_PhaseD_OuterContourMinVerts = 64;
	static constexpr float CoastICE02n_PhaseD_OuterContourMinAzimuthCoverage = 0.85f;
	static constexpr float CoastICE02n_PhaseD_OuterContourMinMaxRFractionOfPathF = 0.85f;
	static constexpr float CoastICE02n_PhaseD_OuterContourMinAreaFractionOfPathF = 0.50f;
	/** Keep candidates within this fraction of max |area| when ranking by perimeter (chord fix). */
	static constexpr float CoastICE02n_PhaseD_OuterContourAreaKeepFractionOfMax = 0.92f;
	/** Reject loops whose longest edge exceeds this × median edge (bay chord / missing soup). */
	static constexpr float CoastICE02n_PhaseD_OuterContourMaxEdgeMultipleOfMedian = 12.f;
	/** Absolute floor for long-edge reject (cm) — ignores micro-edges on small islets. */
	static constexpr float CoastICE02n_PhaseD_OuterContourMaxEdgeAbsoluteMinCm = 25000.f;
	/** Dry-view: ear-clip top lid from contour — OFF (Z-fights IslandMesh −25 m underside). */
	static constexpr bool bCoastICE02n_PhaseD_EarClipTopLidFromContour = false;
	/**
	 * Apply Alt 2 Contour as SeaRoots wall top XY SSOT (Gold→Magenta walls signed 2026-07-17;
	 * Contour→SeaRoots Red Letter 2026-07-17).
	 */
	static constexpr bool bCoastICE02n_Alt2_ApplyContourToSeaRootsWalls = true;
	/**
	 * Contour → A3 star-domain envelope. OFF 2026-07-17: max-R per azimuth chords across
	 * inlets (straight-edge SeaRoots while IslandMesh/magenta follow the bay). Prefer Contour
	 * polylineShell loft (UseA3=false) so SeaRoots top XY matches Contour into concavities.
	 * 2026-07-15 polylineShell inset was SoftOk/Path-F Contour — signed OuterRing Contour is OK.
	 */
	static constexpr bool bCoastICE02n_Alt2_UseA3EnvelopeFromContour = false;
	/**
	 * Prefer IslandMesh Path F shelf-lip as Alt 2 SSOT. OFF — Path F envelope overshoots.
	 */
	static constexpr bool bCoastICE02n_Alt2_PreferMeshShelfLipOverPhaseA = false;
	/**
	 * DEV: thick magenta rim at ShelfFloor (−25 m ASL) = Phase A WWF bottom-cap
	 * continuous outer rim(s). Interior weld grid fixed via aggressive XY weld + optional
	 * primary+islets-only draw (see MagentaRimPrimaryPlusIsletsOnly).
	 */
	static constexpr bool bCoastICE02n_Alt2_ShelfContourMagentaRimWire = true;
	static constexpr float CoastICE02n_Alt2_ShelfContourMagentaRimThicknessCm = 280.f;
	/**
	 * TEMP 2026-07-16: magenta draw = primary OuterRing + per-islet Contours only
	 * (no SoftOk multi-ring pool). Clears axis-aligned interior weld grid. Set false to
	 * use AllExteriorRings after MagentaRimEdgeQuantizeCm weld (step 1 multi-component).
	 */
	static constexpr bool bCoastICE02n_Alt2_MagentaRimPrimaryPlusIsletsOnly = true;
	/**
	 * Contour OuterRing + Magenta SoftOk multi-ring weld (cm). SIGNED Red Letter 2026-07-16/17.
	 * Contour SSOT = phaseASoup SoftOk OuterRing @ this weld only. Never Contour Contour SSOT
	 * rescues (FineWeld/Mesh Contour swap, global weld=2, long-chord OuterRing, Path F Contour
	 * while Contour bValid). Tip OuterRing → invalidate Contour → logged Path F SeaRoots fallback.
	 */
	static constexpr float CoastICE02n_Alt2_MagentaRimEdgeQuantizeCm = 80.f;
	/** SoftOk OuterRing: prefer candidates with azimuth coverage ≥ this (own Contour metric). */
	static constexpr float CoastICE02n_Alt2_ContourOuterRingMinAzimuthCoverage = 0.55f;
	/** Magenta: keep soft-ok loops with |area| ≥ this fraction of the largest (tiny islets). */
	static constexpr float CoastICE02n_Alt2_MagentaRimMinAreaFractionOfMax = 0.0005f;
	/** Magenta: min loop verts (Phase D primary still uses OuterContourMinVerts). */
	static constexpr int32 CoastICE02n_Alt2_MagentaRimMinLoopVerts = 12;
	/**
	 * Magenta: reject loops whose longest edge exceeds this × median edge (interior “weldâ€
	 * chords that cut across a shelf component).
	 */
	static constexpr float CoastICE02n_Alt2_MagentaRimMaxEdgeMultipleOfMedian = 6.f;
	static constexpr float CoastICE02n_Alt2_MagentaRimMaxEdgeAbsoluteMinCm = 12000.f;
	/**
	 * SoftOk OuterRing eject: SoftOk with colinear run ≥ this is SoftChord (not Contour SSOT).
	 * Diagnostic / tip-invalidate gate only — never Contour Contour SSOT mutation.
	 */
	static constexpr float CoastICE02n_Alt2_ContourStraightRunRepairCm = 15000.f; // 150 m
	static constexpr float CoastICE02n_Alt2_ContourStraightRunMaxTurnDeg = 10.f;
	/** Legacy mesh Contour Contour SSOT swap — OFF (Do No Harm Red Letter restore 2026-07-18). */
	static constexpr bool bCoastICE02n_Alt2_RepairContourStraightRunsFromIslandMesh = false;
	/** Tip OuterRing: area < PathF×this → Contour bValid=false (Path F SeaRoots fallback). */
	static constexpr float CoastICE02n_Alt2_ContourRepairMinAreaFractionOfSoup = 0.50f;
	/**
	 * ICE-02n: skip a second full-section extract in RefreshSeaRootsFrustumMesh when build-time
	 * extract already filled CachedMeshShelfFloorPolylineLocalCm from the same verts/tris.
	 */
	static constexpr bool bCoastICE02n_SkipFullMeshExtract = true;
	/**
	 * ICE-02r: log IslandMesh −25 m shelf lip vs baked DeepOuter polyline (max inset/outset).
	 * PERF PRUNE 2026-07-10: OFF — full-vertex diagnostic every bake.
	 */
	static constexpr bool bCoastICE02r_ShelfMeshVsDeepOuterDiagnostic = false;
	/** ICE-02s: allow seaward shelf fine cells that straddle ShelfFloor edge (mesh lip → DeepOuter). */
	static constexpr bool bCoastICE02s_ShelfBoundaryCellsEnabled = true;
	/** B5: SeaRoots 0→−25 m water-info collar for wave attenuation over shelf. */
	static constexpr bool bCoastB5_WaterTerrainCollarEnabled = true;
	/** B6: ocean MID shallow tints use minimap band RGB + canonical depth breakpoints. */
	static constexpr bool bCoastB6_OceanMidMinimapParityEnabled = true;
	/** B6b: water-info collar uses baked coast→tan/cyan/deep horizontal rings (+ stepped shelf caps). */
	static constexpr bool bCoastB6b_OceanMidHorizontalShoreParityEnabled = true;
	/** B6c: flat minimap-parity shore tint annuli on ShelfPresentationMesh (floating iceberg). */
	static constexpr bool bCoastB6c_VisibleShoreTintEnabled = true;
	static constexpr float OceanB6cShoreTintOpacity = 0.58f;
	static constexpr float OceanB6cShoreColorAlpha = 0.88f;
	/**
	 * WB-phase waterfront depth preview only — enables B6/B6b/B6c in PIE for pre-bake reference.
	 * GA+ gameplay: keep false; tan/cyan/deep depth bands are 2D consumers only (minimap, GIS).
	 */
	static constexpr bool bCoastWB_WaterfrontDepthPreviewEnabled = false;

	inline bool IsCoastFloatingIcebergEnabled()
	{
		return bCoastFloatingIcebergEnabled && IsCoastPhaseB2SlopeTierShelfActive();
	}

	inline bool IsCoastICE01CoastPolylineSharedBottomRingEnabled()
	{
		return bCoastICE01_CoastPolylineSharedBottomRingEnabled
			&& IsCoastFloatingIcebergEnabled()
			&& bCoastFloatingIcebergCoastSkinEnabled
			&& bCoastFloatingIcebergHullClosureEnabled;
	}

	inline bool IsCoastICE01TipExtensionClipEnabled()
	{
		return bCoastICE01_TipExtensionClipEnabled
			&& IsCoastFloatingIcebergEnabled()
			&& bCoastFloatingIcebergCoastSkinEnabled;
	}

	inline float GetCoastIcebergCliffExteriorWallMinTopZCm()
	{
		return CoastIcebergCliffExteriorWallMinTopMeters * 100.f;
	}

	inline bool IsCoastIcebergGentleClosureWallEnabled()
	{
		return bCoastIcebergGentleClosureWallEnabled;
	}

	inline float GetCoastIcebergGentleClosureWallMinTopZCm()
	{
		return CoastIcebergGentleClosureWallMinTopMeters * 100.f;
	}

	inline bool IsCoastC2bSuppressProceduralSteepCoastWallsEnabled()
	{
		return bCoastC2b_SuppressProceduralSteepCoastWallsEnabled;
	}

	inline float GetCoastIcebergGentleClosureWallMaxTopZCm()
	{
		const float MaxMeters = IsCoastC2bSuppressProceduralSteepCoastWallsEnabled()
			? CoastC2b_GentleWallMaxTopWhenSteepSuppressedMeters
			: CoastIcebergGentleClosureWallMaxTopMeters;
		return MaxMeters * 100.f;
	}

	inline float GetCoastIcebergShallowBottomCapMaxTopZCm()
	{
		return CoastIcebergShallowBottomCapMaxTopMeters * 100.f;
	}

	inline bool IsCoastICE02SeawardShelfTopsEnabled()
	{
		return IsCoastFloatingIcebergEnabled()
			&& bCoastFloatingIcebergCoastSkinEnabled
			&& bCoastICE02_SeawardShelfTopsEnabled;
	}

	inline bool IsCoastICE02IslandFrustumEnabled()
	{
		return IsCoastFloatingIcebergEnabled() && bCoastICE02_IslandFrustumEnabled;
	}

	inline bool IsCoastICE02bDebugFrustumVisibilityEnabled()
	{
#if !UE_BUILD_SHIPPING
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02b_DebugFrustumVisibilityEnabled;
#else
		return false;
#endif
	}

	inline bool IsCoastICE02cDevFrustumFuchsiaUnlit()
	{
#if !UE_BUILD_SHIPPING
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02c_DevFrustumFuchsiaUnlit;
#else
		return false;
#endif
	}

	inline bool IsCoastICE02dDevFrustumExteriorWireframeEnabled()
	{
#if !UE_BUILD_SHIPPING
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02d_DevFrustumExteriorWireframe;
#else
		return false;
#endif
	}

	inline bool IsCoastICE02eFrustumTopAlignedEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02e_FrustumTopAligned;
	}

	inline bool IsCoastICE02fFrustumAzimuthOriginEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02f_FrustumAzimuthOrigin;
	}

	inline bool IsCoastICE02gFrustumDeepOuterUnscaledEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02g_FrustumDeepOuterUnscaled;
	}

	inline bool IsCoastICE02jFrustumOutwardAbyssSlopeEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02j_FrustumOutwardAbyssSlope;
	}

	inline bool IsCoastICE02hFrustumPolylineTopRadiiEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled()
			&& IsCoastICE02eFrustumTopAlignedEnabled()
			&& bCoastICE02h_FrustumPolylineTopRadii;
	}

	inline bool IsCoastICE02kSkipFrustumTopResmoothEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled() && bCoastICE02k_SkipFrustumTopResmooth;
	}

	inline bool IsCoastICE02tFrustumAzimuthOuterEnvelopeEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled()
			&& IsCoastICE02eFrustumTopAlignedEnabled()
			&& bCoastICE02t_FrustumAzimuthOuterEnvelope;
	}

	inline bool IsCoastICE02tKeepOutwardWallTrianglesOnlyEnabled()
	{
		return IsCoastICE02tFrustumAzimuthOuterEnvelopeEnabled()
			&& bCoastICE02t_KeepOutwardWallTrianglesOnly;
	}

	inline bool IsCoastICE02tSkipBottomCapEnabled()
	{
		return IsCoastICE02tFrustumAzimuthOuterEnvelopeEnabled()
			&& bCoastICE02t_SkipBottomCap;
	}

	inline bool IsCoastICE02lFrustumPolylineShellEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled()
			&& IsCoastICE02eFrustumTopAlignedEnabled()
			&& bCoastICE02l_FrustumPolylineShell
			&& !IsCoastICE02tFrustumAzimuthOuterEnvelopeEnabled();
	}

	inline bool IsCoastICE02nFrustumTopFromIslandMeshEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled()
			&& IsCoastICE02eFrustumTopAlignedEnabled()
			&& bCoastICE02n_FrustumTopFromIslandMesh;
	}

	inline bool IsCoastICE02nPathFBottomCapRimOnlyEnabled()
	{
		return IsCoastICE02nFrustumTopFromIslandMeshEnabled()
			&& bCoastICE02n_PathF_BottomCapRimOnly;
	}

	inline bool IsCoastICE02nXyFlushEnabled()
	{
		// Path F wins for rim SSOT — do not expand with skirt / near-shelf catch.
		return IsCoastICE02nFrustumTopFromIslandMeshEnabled()
			&& bCoastICE02n_XyFlushWallBottomAndNearShelf
			&& !bCoastICE02n_PathF_BottomCapRimOnly;
	}

	inline bool IsCoastICE02nSkipFullMeshExtractEnabled()
	{
		return IsCoastICE02nFrustumTopFromIslandMeshEnabled()
			&& bCoastICE02n_SkipFullMeshExtract;
	}

	inline bool IsCoastICE02rShelfMeshVsDeepOuterDiagnosticEnabled()
	{
		return IsCoastFloatingIcebergEnabled()
			&& IsCoastICE02IslandFrustumEnabled()
			&& bCoastICE02r_ShelfMeshVsDeepOuterDiagnostic;
	}

	inline bool IsCoastICE02sShelfBoundaryCellsEnabled()
	{
		return IsCoastICE02SeawardShelfTopsEnabled()
			&& bCoastICE02s_ShelfBoundaryCellsEnabled;
	}

	inline bool IsCoastB5WaterTerrainCollarEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled() && bCoastB5_WaterTerrainCollarEnabled;
	}

	inline bool IsCoastWBWaterfrontDepthPreviewEnabled()
	{
		return bCoastWB_WaterfrontDepthPreviewEnabled;
	}

	inline bool IsCoastB6OceanMidMinimapParityEnabled()
	{
		return bCoastWB_WaterfrontDepthPreviewEnabled
			&& bCoastB6_OceanMidMinimapParityEnabled;
	}

	inline bool IsCoastB6bOceanMidHorizontalShoreParityEnabled()
	{
		return IsCoastB5WaterTerrainCollarEnabled()
			&& IsCoastB6OceanMidMinimapParityEnabled()
			&& bCoastB6b_OceanMidHorizontalShoreParityEnabled;
	}

	inline bool IsCoastB6cVisibleShoreTintEnabled()
	{
		return IsCoastB6bOceanMidHorizontalShoreParityEnabled()
			&& bCoastB6c_VisibleShoreTintEnabled;
	}

	// --- B2b: W flyout terrain stamp palette ---

	static constexpr bool bCoastB2b_WorldStampPaletteEnabled = true;
	static constexpr int32 TerrainStampPaletteColumns = 7;
	static constexpr int32 TerrainStampPaletteActiveRows = 3;
	static constexpr int32 TerrainStampPaletteMainSlotCount = 21;
	static constexpr int32 TerrainStampPaletteReservedSlotCount = 7;
	static constexpr int32 TerrainStampPaletteSlotCapacity = 29;

	inline bool IsCoastB2bWorldStampPaletteEnabled()
	{
		return bCoastB2b_WorldStampPaletteEnabled;
	}

	// --- Gate 0 (P1C12 Arbor): ocean surface selection ---
	// LOCKED: see Content/SignOff/P1C12_Ocean_Fallback_Junction.md

	/**
	 * TRUE (heightmap / WT-A): AIH_P1C12_OceanPlane is the primary ocean surface
	 * (endless camera-follow Gerstner tile). WaterTankRig aquarium path is retired.
	 */
	static constexpr bool bGate0UseCustomOceanPlane = true;

	inline bool IsGate0CustomOceanPlaneEnabled()
	{
		return bGate0UseCustomOceanPlane;
	}

	/**
	 * Compile-time default for ocean suppress. PIE HUD "Ocean" overrides via IHDevViewRuntime
	 * (WB-only; does not carry to later game phases). false = ocean ON at startup.
	 */
	static constexpr bool bDevDemo_HideOceanWaterActors = false;

	// --- Gate 0 (IH-DEC-040): Ultra Dynamic Sky replaces hardcoded ADirectionalLight ---

	/**
	 * TRUE: StartPlay spawns Ultra_Dynamic_Sky_C + Ultra_Dynamic_Weather_C (Content-only
	 * Blueprint pack, no native C++ type) instead of the retired ADirectionalLight +
	 * TankSunIntensityPie/GrabContrast system. Do No Harm: does not touch
	 * AIH_P1C12_OceanPlane, bWBUnlockProductionCanonicalAcres, or Sea Floor/WWF/Gold
	 * Coastline constants (Content/InvisibleHand/UI/IH_UltimateSky_Enablement.md).
	 */
	static constexpr bool bGate0UseUltraDynamicSky = true;

	inline bool IsGate0UltraDynamicSkyEnabled()
	{
		return bGate0UseUltraDynamicSky;
	}

	inline const TCHAR* GetUdsSkyBlueprintClassPath()
	{
		return TEXT("/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky.Ultra_Dynamic_Sky_C");
	}
	inline const TCHAR* GetUdsWeatherBlueprintClassPath()
	{
		return TEXT("/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather.Ultra_Dynamic_Weather_C");
	}

	/**
	 * CORRECTION (was wrongly assumed minutes-past-midnight 0-1440 all session): UDS's real "Time
	 * of Day" scale is HHMM, 0-2400 (confirmed via headless probe: calling UDS's own "Set Time of
	 * Day With String" with "18:00" produces 1800.0, "06:00" produces 600.0 — official UDS docs
	 * independently confirm "Get Time of Day" also "outputs ... from 0-2400"). Rather than compute
	 * that scale by hand and risk another silent mis-scale, this now returns a plain HH:MM string
	 * per EIHTimeBracket (InvisibleHand_CalendarSystem.md §7) for GameMode to pass directly into
	 * UDS's own "Set Time of Day With String" function, letting UDS do the parsing/scaling itself.
	 * Honors the doc's note that Nightfall-to-Midnight is intentionally compressed.
	 */
	inline FString GetTimeBracketTimeString(EIHTimeBracket Bracket)
	{
		static const TCHAR* Times[10] = {
			TEXT("00:00"), TEXT("05:00"), TEXT("06:00"), TEXT("09:00"), TEXT("12:00"),
			TEXT("15:00"), TEXT("17:30"), TEXT("18:00"), TEXT("19:30"), TEXT("22:00")
		};
		const int32 Index = static_cast<int32>(Bracket);
		return Times[FMath::Clamp(Index, 0, 9)];
	}

	/**
	 * IH's own deliberate 30-day lunar cycle (InvisibleHand_CalendarSystem.md §5) — "not the
	 * natural approximately 29.5-day astronomical cycle." A simple lookup, same every month
	 * (user-confirmed), NOT UDS's Simulate Real Moon (which would compute the real ~29.5-day cycle
	 * and directly contradict this canonical rule). Day-of-month maps linearly onto UDS's real
	 * "Moon Phase" property (confirmed 0-1 cycle position: 0=New, 0.5=Full, 1=New again, with the
	 * renderer deriving the crescent/gibbous shape itself — not a pre-baked illumination percent).
	 * Day 1 -> 0.0 (New), Day 15 -> ~0.5 (Full, the canonical doc's 100% illumination day), Day 30
	 * -> 1.0 (New again, the canonical doc's "Second New Moon"). Day 15 lands at 14/29≈0.483 rather
	 * than exactly 0.5 — the 30-day table can't hit both New-Moon endpoints AND the exact midpoint
	 * with pure linear spacing; the ~1.7% offset is visually negligible.
	 */
	inline float GetMoonPhaseValue(int32 Day)
	{
		const int32 Clamped = FMath::Clamp(Day, 1, 30);
		return static_cast<float>(Clamped - 1) / 29.f;
	}

	/**
	 * UDS ships 13 stock Weather Presets (UDS_Weather_Settings_C PrimaryDataAssets) under this
	 * folder — confirmed via headless probe. "Change Weather" on the Weather actor takes a loaded
	 * instance of one of these directly (ObjectProperty, not a class reference).
	 */
	inline const TCHAR* GetUdsWeatherPresetsFolder()
	{
		return TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/");
	}
	inline const TArray<FString>& GetUdsWeatherPresetNames()
	{
		static const TArray<FString> Names = {
			TEXT("Clear_Skies"), TEXT("Partly_Cloudy"), TEXT("Cloudy"), TEXT("Overcast"), TEXT("Foggy"),
			TEXT("Rain_Light"), TEXT("Rain"), TEXT("Rain_Thunderstorm"),
			TEXT("Snow_Light"), TEXT("Snow"), TEXT("Snow_Blizzard"),
			TEXT("Sand_Dust_Calm"), TEXT("Sand_Dust_Storm")
		};
		return Names;
	}

	/**
	 * Real-degree Latitude input for UDS's Simulate Real Sun/Stars, per EIHRealmLatitude. Reuses
	 * this project's own already-established canonical reference values (found in the Biome
	 * DataTable header "Nordic70N,Temperate45N,Tropical25N" — DataTable_Column_Header_Validation
	 * _Report.md), rather than inventing new numbers for the same three latitude bands.
	 */
	inline float GetRealmLatitudeDegrees(EIHRealmLatitude Latitude)
	{
		switch (Latitude)
		{
		case EIHRealmLatitude::Nordic: return 70.f;
		case EIHRealmLatitude::Tropical: return 25.f;
		default: return 45.f; // Temperate
		}
	}

#if !UE_BUILD_SHIPPING
	/** Implemented in IHDevViewRuntime.cpp — PIE HUD may override compile defaults. */
	bool DevView_IsHideOceanEnabled();
	bool DevView_IsDryShoulderHullHideEnabled();
	bool DevView_AreContoursEnabled();
#endif

	inline bool IsDevDemoHideOceanWaterActorsEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return DevView_IsHideOceanEnabled();
#endif
	}

	/** Dry-shoulder: hide IslandMesh ICE walls + SeaRoots top@0. Independent of ocean. */
	inline bool IsDevDemoHideIslandMeshIcebergExteriorWallsEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return DevView_IsDryShoulderHullHideEnabled();
#endif
	}

	/**
	 * TEMP DEV (2026-07-11): skip/disable ExponentialHeightFog (+ hide volumetric clouds)
	 * so IslandMesh + SeaRoots frustum are visible in dry PIE. Restore with HideOcean.
	 */
	static constexpr bool bDevDemo_ClearAtmosphericFog = true;

	inline bool IsDevDemoClearAtmosphericFogEnabled()
	{
		return bDevDemo_ClearAtmosphericFog;
	}

	/**
	 * TEMP DEV (2026-07-11): spawn unlit yellow plane at DeepSeaFloorMeters (−250 m)
	 * color #FFD230 for abyss contrast under frustum. Restore: set false.
	 */
	static constexpr bool bDevDemo_YellowAbyssFloor = false;
	static const FLinearColor DevDemo_YellowAbyssFloorColor = FLinearColor(1.f, 0.823529f, 0.188235f, 1.f);

	inline bool IsDevDemoYellowAbyssFloorEnabled()
	{
		return bDevDemo_YellowAbyssFloor;
	}

	/**
	 * TEMP DEV dry-view (2026-07-11): ICE-02e skips frustum top annulus (IslandMesh owns −25 m).
	 * With A3 envelope far seaward of the lip, looking up/down shows an open tube.
	 * When HideOcean is on, emit coast→TopRadii annulus (both faces) at −25 m for sign-off.
	 * Restore: set false with other DEV-WWF flags.
	 */
	static constexpr bool bDevDemo_FrustumTopAnnulusDryView = false;

	inline bool IsDevDemoFrustumTopAnnulusDryViewEnabled()
	{
		return bDevDemo_FrustumTopAnnulusDryView && bDevDemo_HideOceanWaterActors;
	}

	/**
	 * Path E (2026-07-13): solid frustum top disk — both faces — so SeaRoots closes hollow tube
	 * under IslandMesh WWF −25 m bottom. Not coast→lip “little beachâ€.
	 * Active with HideOcean dry-view. Restore: false with other DEV-WWF flags.
	 */
	static constexpr bool bDevDemo_FrustumSolidTopDisk = true;
	/** Nudge top disk slightly below IslandMesh shelf bottom to reduce z-fight (cm). */
	static constexpr float DevDemo_FrustumSolidTopDiskZNudgeBelowCm = 1.f;
	/**
	 * Path E + boundary loop: disk from shelf-cap boundary loop.
	 * REVERTED 2026-07-13 with Path G — wrong loops caused missing/oversize lid.
	 * Safe: Path E polar disk at Path F TopRadii (`bDevDemo_FrustumSolidTopDisk=true`).
	 */
	static constexpr bool bDevDemo_FrustumSolidTopDiskFromBoundaryLoop = false;

	inline bool IsDevDemoFrustumSolidTopDiskEnabled()
	{
		return bDevDemo_FrustumSolidTopDisk && bDevDemo_HideOceanWaterActors;
	}

	inline bool IsDevDemoFrustumSolidTopDiskFromBoundaryLoopEnabled()
	{
		return IsDevDemoFrustumSolidTopDiskEnabled()
			&& bDevDemo_FrustumSolidTopDiskFromBoundaryLoop;
	}

	inline bool IsCoastICE02nPathGOuterBoundaryLoopEnabled()
	{
		return IsCoastICE02nFrustumTopFromIslandMeshEnabled()
			&& bCoastICE02n_PathG_OuterBoundaryLoopWallAndDisk;
	}

	inline bool IsCoastICE02nPhase1ShelfFloorRimDiagnosticEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return IsCoastICE02nFrustumTopFromIslandMeshEnabled()
			&& bCoastICE02n_Phase1_ShelfFloorRimDiagnostic;
#endif
	}

	inline bool IsCoastICE02nPhase1ShelfFloorRimDiagnosticWireEnabled()
	{
		return IsCoastICE02nPhase1ShelfFloorRimDiagnosticEnabled()
			&& bCoastICE02n_Phase1_ShelfFloorRimDiagnosticWire;
	}

	inline bool IsCoastICE02nPhaseAShelfBottomCapSectionEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return IsCoastFloatingIcebergEnabled()
			&& bCoastICE02n_PhaseA_ShelfBottomCapSection;
#endif
	}

	inline bool IsCoastICE02nPhaseBShelfBottomCapContourWireEnabled()
	{
		return IsCoastICE02nPhaseAShelfBottomCapSectionEnabled()
			&& bCoastICE02n_PhaseB_ShelfBottomCapContourWire;
	}

	inline bool IsCoastICE02nPhaseCFrustumTopFromPhaseASoupEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return IsCoastICE02nPhaseAShelfBottomCapSectionEnabled()
			&& IsDevDemoFrustumSolidTopDiskEnabled()
			&& bCoastICE02n_PhaseC_FrustumTopFromPhaseASoup;
#endif
	}

	inline bool IsCoastICE02nPhaseDFrustumWallsFromPhaseAOuterContourEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return IsCoastICE02nPhaseAShelfBottomCapSectionEnabled()
			&& IsCoastICE02nFrustumTopFromIslandMeshEnabled()
			&& bCoastICE02n_PhaseD_FrustumWallsFromPhaseAOuterContour;
#endif
	}

	inline bool IsCoastICE02nPhaseDEarClipTopLidFromContourEnabled()
	{
		return IsCoastICE02nPhaseDFrustumWallsFromPhaseAOuterContourEnabled()
			&& IsDevDemoFrustumSolidTopDiskEnabled()
			&& bCoastICE02n_PhaseD_EarClipTopLidFromContour;
	}

	/** When false: SeaRoots walls are unitary ICE-02t A3 (tan Red Letter). Contour is magenta-only. */
	inline bool IsCoastICE02nAlt2ApplyContourToSeaRootsWallsEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return IsCoastICE02nPhaseDFrustumWallsFromPhaseAOuterContourEnabled()
			&& bCoastICE02n_Alt2_ApplyContourToSeaRootsWalls;
#endif
	}

	inline bool IsCoastICE02nAlt2UseA3EnvelopeFromContourEnabled()
	{
		return IsCoastICE02nAlt2ApplyContourToSeaRootsWallsEnabled()
			&& bCoastICE02n_Alt2_UseA3EnvelopeFromContour;
	}

	inline bool IsCoastICE02nAlt2PreferMeshShelfLipOverPhaseAEnabled()
	{
		return IsCoastICE02nAlt2ApplyContourToSeaRootsWallsEnabled()
			&& bCoastICE02n_Alt2_PreferMeshShelfLipOverPhaseA;
	}

	inline bool IsCoastICE02nAlt2ShelfContourMagentaRimWireEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return IsCoastICE02nPhaseAShelfBottomCapSectionEnabled()
			&& bCoastICE02n_Alt2_ShelfContourMagentaRimWire;
#endif
	}

	inline bool IsCoastICE02nAlt2MagentaRimPrimaryPlusIsletsOnlyEnabled()
	{
		return IsCoastICE02nAlt2ShelfContourMagentaRimWireEnabled()
			&& bCoastICE02n_Alt2_MagentaRimPrimaryPlusIsletsOnly;
	}

	/**
	 * DEV / WB: ASL Contour Lines. Gold @ z=0 (waterline), shelf magenta #C6185C
	 * @ z=−25 (Phase A Contour SSOT), white #ECFDF5 every 25 m from −225 to IslandMesh summit
	 * (skip 0 / −25). Underwater white TEMP (`IncludeUnderwater`); dry bands intended for later GIS.
	 * When ON, Alt2 Contour magenta wire is skipped (−25 owned here as #C6185C).
	 * Default draw = baked unlit mesh ribbons (stable at aerial zoom). DrawDebug is fallback only.
	 */
	static constexpr bool bDevDemo_AslContourLines = true;
	/**
	 * Underwater whites follow DEV View Contours checkbox (see IsDevDemoAslContourIncludeUnderwaterEnabled).
	 * Contours default OFF → underwater off at seed regen; checking Contours bakes/shows them.
	 */
	static constexpr bool bDevDemo_AslContourLinesIncludeUnderwater = false;
	/** Preferred: ICE-02d-style ribbon quads (not DrawDebug). */
	static constexpr bool bDevDemo_AslContourMeshRibbons = true;
	/**
	 * During SpawnIslands / realm regen: bake Contour cache only, then flush ribbons after all
	 * islands exist (avoids N× heavy CreateMeshSection on the critical spawn path).
	 */
	static constexpr bool bDevDemo_AslContourDeferRibbonUntilSpawnBatch = true;
	static constexpr float DevDemo_AslContourIntervalMeters = 25.f;
	static constexpr float DevDemo_AslContourWhiteMinMeters = -225.f;
	/** DrawDebug fallback thickness (cm) when mesh ribbons off. */
	static constexpr float DevDemo_AslContourThicknessCm = 180.f;
	/** Full ribbon width (cm) — world-space; holds aerial readability better than DrawDebug. */
	static constexpr float DevDemo_AslContourRibbonThicknessCm = 420.f;
	/** Magenta Contour rim ribbon width (cm); slightly heavier than white isolines. */
	static constexpr float DevDemo_AslContourRibbonMagentaThicknessCm = 560.f;
	/** Gold waterline ribbon width (cm) — match magenta for Gold→Magenta wall sign-off. */
	static constexpr float DevDemo_AslContourRibbonGoldThicknessCm = 560.f;
	/** Emissive multiply for lit parents only; LevelColorationUnlit uses base Color unscaled. */
	static constexpr float DevDemo_AslContourRibbonEmissiveBoost = 4.f;
	/** Cap white iso segment quads per island (decimate if plane-clip yields more). */
	static constexpr int32 DevDemo_AslContourRibbonMaxWhiteSegs = 6000;
	static constexpr float DevDemo_AslContourZNudgeCm = 1200.f; // was 80 — buried by beach lip / sheer; raise for fly ASL
	static constexpr float DevDemo_AslContourPlus25Meters = 25.f;
	static constexpr float DevDemo_AslContourRibbonPlus25ThicknessCm = 720.f;
	static const FColor DevDemo_AslContourGoldColor = FColor(0xFF, 0xDF, 0x20);
	static const FColor DevDemo_AslContourWhiteColor = FColor(0xEC, 0xFD, 0xF5);
	static const FColor DevDemo_AslContourShelfMagentaColor = FColor(0xC6, 0x18, 0x5C);

	inline bool IsDevDemoAslContourLinesEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return bDevDemo_AslContourLines && DevView_AreContoursEnabled();
#endif
	}

	/** Underwater white isolines: same Contours checkbox as gold/magenta ASL ribbons. */
	inline bool IsDevDemoAslContourIncludeUnderwaterEnabled()
	{
		// Contours ON ⇒ bake/show underwater whites; Contours OFF (default) ⇒ omit at seed regen.
		return IsDevDemoAslContourLinesEnabled();
	}

	inline bool IsDevDemoAslContourMeshRibbonsEnabled()
	{
		return IsDevDemoAslContourLinesEnabled() && bDevDemo_AslContourMeshRibbons;
	}

	/**
	 * Contour-Guided Sector Fabric prototype touchpoint (IH-DEC-023/027/029/031). Default OFF -
	 * algorithm-validation pass only, gated the same way as every other dev-only visualization here.
	 */
	static constexpr bool bDevDemo_SectorFabricPrototype = false;
	inline bool IsDevDemoSectorFabricPrototypeEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return bDevDemo_SectorFabricPrototype;
#endif
	}

	/**
	 * TEMP DEV (2026-07-12): force entire SeaRoots frustum to opaque #A684FF (BasicShapeMaterial)
	 * so PIE proves Color/tint on walls. Was translucent-unlit (see-through/ghosting). Restore: false.
	 */
	static constexpr bool bDevDemo_FrustumProveTintUnlit = false;
	/** #A684FF → sRGB 166,132,255 */
	static const FLinearColor DevDemo_FrustumProveTintColor =
		FLinearColor(166.f / 255.f, 132.f / 255.f, 1.f, 1.f);

	inline bool IsDevDemoFrustumProveTintUnlitEnabled()
	{
		return bDevDemo_FrustumProveTintUnlit;
	}

	/**
	 * TEMP DEV dry-view contrast (2026-07-12): IslandMesh + SeaRoots share the same opaque
	 * BasicShapeMaterial profile (Roughness=1, Specular=0, Metallic=0, Opacity=1) — only Color/tint
	 * differs so XY flush overhang is readable. Not alpha. Restore: false with HideOcean.
	 */
	static constexpr bool bDevDemo_IslandFrustumSharedMaterialContrast = true;
	/** Cool slate — IslandMesh / WWF shelf hull. */
	static const FLinearColor DevDemo_IslandMeshContrastTint =
		FLinearColor(0.38f, 0.44f, 0.52f, 1.f);
	/** Warm sand — SeaRoots frustum (distinct from island slate). */
	static const FLinearColor DevDemo_FrustumContrastTint =
		FLinearColor(0.86f, 0.72f, 0.42f, 1.f);

	inline bool IsDevDemoIslandFrustumSharedMaterialContrastEnabled()
	{
		return bDevDemo_IslandFrustumSharedMaterialContrast && bDevDemo_HideOceanWaterActors;
	}

	/**
	 * WB / game lighting path (2026-07-16): opaque Default Lit materials for SeaRoots sand +
	 * IslandMesh/WWF (BasicShape / TOPO tiers). Disables dry-view LevelColorationUnlit matte.
	 * Casts shadows + occludes. Pair with AslSlopeTierOverlays for vegetative ASL/slope cover.
	 */
	static constexpr bool bDevDemo_LitOpaqueTerrainMaterials = true;

	inline bool IsDevDemoLitOpaqueTerrainMaterialsEnabled()
	{
#if UE_BUILD_SHIPPING
		return true;
#else
		return bDevDemo_LitOpaqueTerrainMaterials;
#endif
	}

	/**
	 * TEMP / C5 follow-on: spawn one SeaRoots frustum actor per non-main FeaturePresentationExtent
	 * (detached islets on IslandMesh). Uses same ICE-02t A3 path as main. Restore/gate later if needed.
	 */
	static constexpr bool bCoastICE02_IsletFrustumActorsEnabled = true;

	inline bool IsCoastICE02IsletFrustumActorsEnabled()
	{
		return IsCoastICE02IslandFrustumEnabled()
			&& IsCoastC5IsletTerrainMeshEnabled()
			&& bCoastICE02_IsletFrustumActorsEnabled;
	}

	/**
	 * TEMP DEV DEMO ONLY: when true, GetDevDemoOceanSurfaceZCm returns ShelfFloor (−25 m).
	 * Gate0 AIH_P1C12_OceanPlane stays at ASL 0 — keep false so Place/sail/Contours share waterline.
	 * Shelf inspection: Contours magenta + Ocean OFF (not a lowered Gerstner plane).
	 */
	static constexpr bool bDevDemo_LowerOceanToShelfFloor = false;

	inline bool IsDevDemoLowerOceanToShelfFloorEnabled()
	{
		return bDevDemo_LowerOceanToShelfFloor && !bDevDemo_HideOceanWaterActors;
	}

	/** Ocean surface Z (cm). Canon 0; demo −2500 when bDevDemo_LowerOceanToShelfFloor. */
	inline float GetDevDemoOceanSurfaceZCm()
	{
		return IsDevDemoLowerOceanToShelfFloorEnabled()
			? (ShelfFloorMeters * 100.f)
			: (SeaLevelMeters * 100.f);
	}

	// --- B2b dev: ih.StampGallerySpawn review gallery (non-shipping) ---

	static constexpr bool bCoastB2b_StampGalleryMinimapMarkerEnabled = true;
	static constexpr float StampGalleryOriginXCm = 0.f;
	static constexpr float StampGalleryTankWallHalfThicknessCm = 200.f;
	/** Clearance north of the north tank wall outer face (cm). */
	static constexpr float StampGalleryNorthPadBeyondTankWallCm = 100000.f;
	static constexpr float StampGalleryGroundZCm = 200.f;
	/** Dev gallery grid spacing (m) — fits largest catalog footprint at full radius. */
	static constexpr float StampGalleryPreviewCellSpacingMeters = 350.f;
	static constexpr float StampGalleryCellSpacingCm = StampGalleryPreviewCellSpacingMeters * 100.f;
	/** G4: heightfield gallery pad (ApplyStampToHeightGrid — same path as island placement). */
	static constexpr int32 StampGalleryHeightfieldGridSide = 64;
	static constexpr float StampGalleryHeightfieldLandBaselineAzgaar = 25.f;
	/** Linear gallery relief: localZ cm = (Azgaar - baseline) * cmPerAzgaar * zScale. */
	static constexpr float StampGalleryHeightfieldCmPerAzgaarUnit = 100.f;
	static constexpr float StampGalleryHeightfieldWorldZScale = 1.5f;
	static constexpr float StampGalleryHeightfieldExtentPadding = 1.05f;
	static constexpr int32 StampGalleryGridColumns = 7;
	/** Canonical 22 + invert review row band (approximate minimap footprint). */
	static constexpr int32 StampGalleryGridRowsApprox = 6;
	static constexpr float StampGalleryReviewFocusHeightCm = 600.f;
	static constexpr float StampGalleryReviewCameraBackCm = 55000.f;
	static constexpr float StampGalleryReviewCameraSideCm = 80000.f;
	static constexpr float StampGalleryReviewCameraUpCm = 16000.f;
	/** Dev wireframe tint for gallery heightfield previews (non-shipping). */
	static const FLinearColor StampGalleryWireframeVerticalColor = FLinearColor(0.25f, 1.f, 0.45f, 1.f);
	static const FLinearColor StampGalleryWireframeInvertedColor = FLinearColor(1.f, 0.62f, 0.12f, 1.f);
	/** B2b inverted stamp overlay — cyan, must NOT match CoastalLowlandDevFillColor (orange). */
	static const FLinearColor StampPickOverlayInvertedColor = FLinearColor(0.05f, 0.85f, 1.f, 1.f);
	/** B2b pick overlay — magenta, contrasts SSTO navy + lime dev prop (#FF40FF). */
	static const FLinearColor StampPickOverlayColor = FLinearColor(1.f, 0.25f, 1.f, 1.f);
	static const FLinearColor StampPickOverlaySelectedColor = FLinearColor(1.f, 0.55f, 0.08f, 1.f);
	/** Lift proc-mesh pick dome above deformed SSTO + dev prop slab (legacy radial preview only). */
	static constexpr float StampPickMeshLiftCm = 1200.f;
	/** Rim Z offset for placed/drag stamp overlay proc mesh (cm above actor surface). */
	static constexpr float StampPlacedPreviewSurfaceOffsetCm = 800.f;
	/** Dev-only vertical exaggeration for placed stamp overlay mesh (not height-grid apply). */
	static constexpr float StampPlacedPreviewVisualExaggeration = 10.f;
	/** Preview bowl depth scale — matches gallery heightfield relief. */
	static float GetStampPlacedPreviewDepthCm(float AmplitudeAzgaar)
	{
		return FMath::Abs(AmplitudeAzgaar)
			* StampGalleryHeightfieldCmPerAzgaarUnit
			* StampGalleryHeightfieldWorldZScale
			* StampPlacedPreviewVisualExaggeration;
	}

	inline bool IsStampGalleryMinimapMarkerEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return bCoastB2b_StampGalleryMinimapMarkerEnabled;
#endif
	}

	inline float GetStampGalleryOriginYCm(const float RealmHalfExtentNSKm)
	{
		const float RealmHalfExtentNSCm = RealmHalfExtentNSKm * 100000.f;
		return RealmHalfExtentNSCm + StampGalleryTankWallHalfThicknessCm + StampGalleryNorthPadBeyondTankWallCm;
	}

	inline FVector GetStampGalleryWorldOriginCm(const float RealmHalfExtentNSKm)
	{
		return FVector(
			StampGalleryOriginXCm,
			GetStampGalleryOriginYCm(RealmHalfExtentNSKm),
			StampGalleryGroundZCm);
	}

	inline void GetStampGalleryWorldFootprintXYCm(
		const float RealmHalfExtentNSKm,
		float& OutMinX,
		float& OutMinY,
		float& OutMaxX,
		float& OutMaxY)
	{
		const float OriginY = GetStampGalleryOriginYCm(RealmHalfExtentNSKm);
		OutMinX = StampGalleryOriginXCm;
		OutMinY = OriginY;
		OutMaxX = StampGalleryOriginXCm
			+ static_cast<float>(StampGalleryGridColumns - 1) * StampGalleryCellSpacingCm;
		OutMaxY = OriginY
			+ static_cast<float>(StampGalleryGridRowsApprox - 1) * StampGalleryCellSpacingCm;
	}

	inline bool IsCoastPieBandPresentationEnabled()
	{
		return IsCoastPhaseB2SlopeTierShelfActive() && bCoastPieBandPresentationEnabled;
	}

	inline bool IsMinimapSeaDepthBandFillsEnabled()
	{
		return bMinimapSeaDepthBandFillsEnabled;
	}

	inline bool IsCoastStrictLandQuadSeaClipEnabled()
	{
		return bCoastStrictLandQuadSeaClipEnabled;
	}

	inline bool IsCoastStrokePieDisplayEnabled()
	{
		return bCoastStrokePieDisplayEnabled;
	}

	inline bool IsCoastStrokeOnIslandMeshPrimary()
	{
		return IsCoastStrokePieDisplayEnabled()
			&& bCoastStrokeOnIslandMeshPrimary
			&& !bCoastStrokeInstancedMeshEnabled;
	}

	inline bool IsCoastStrokeOnPresentationMeshPrimary()
	{
		return IsCoastStrokePieDisplayEnabled()
			&& bCoastStrokeOnPresentationMeshPrimary
			&& !bCoastStrokeInstancedMeshEnabled;
	}

	inline bool IsCoastStrokeProcMeshEnabled()
	{
		return IsCoastStrokePieDisplayEnabled() && !bCoastStrokeInstancedMeshEnabled;
	}

	inline bool IsCoastStrokeInstancedMeshEnabled()
	{
		return IsCoastStrokePieDisplayEnabled() && bCoastStrokeInstancedMeshEnabled;
	}

	inline bool IsCoastStrokeMirrorOnPresentationMesh()
	{
		return IsCoastStrokePieDisplayEnabled() && bCoastStrokeMirrorOnPresentationMesh;
	}

	inline bool IsCoastStrokeVerticalWallsEnabled()
	{
		return IsCoastStrokePieDisplayEnabled() && bCoastStrokeVerticalWallsEnabled;
	}

	inline bool IsPieShoreFlatAnnuliOverlayEnabled()
	{
		return IsCoastPieBandPresentationEnabled() && bPieShoreFlatAnnuliOverlayEnabled;
	}

	inline bool IsPieShorePresentationSlopedShelfEnabled()
	{
		if (IsCoastFloatingIcebergEnabled())
		{
			return false;
		}
		return IsCoastPieBandPresentationEnabled() && bPieShorePresentationSlopedShelfEnabled;
	}

	inline bool IsPieShoreOverlayVerticalWallsEnabled()
	{
		return IsCoastPieBandPresentationEnabled() && bPieShoreOverlayVerticalWallsEnabled;
	}

	inline bool IsPieShoreMirrorBandsOnIslandMesh()
	{
		return IsCoastPieBandPresentationEnabled() && bPieShoreMirrorBandsOnIslandMesh;
	}

	inline bool IsPieShoreBandsOnIslandMeshPrimary()
	{
		return IsCoastPieBandPresentationEnabled() && bPieShoreBandsOnIslandMeshPrimary;
	}

	inline bool IsPieShoreHideSeaRootsMeshInPie()
	{
		if (IsCoastICE02IslandFrustumEnabled() && bCoastICE02_KeepFrustumVisibleInPie)
		{
			return false;
		}
		return bPieShoreHideSeaRootsMeshInPie;
	}

	inline bool IsCoastStrokeWorldScreenOverlayEnabled()
	{
		return bCoastStrokeWorldScreenOverlayEnabled;
	}

	inline bool IsIslandBaseDevPropEnabled()
	{
		if (IsCoastFloatingIcebergEnabled())
		{
			return false;
		}
		return bIslandBaseDevPropEnabled;
	}

	inline float GetIslandBaseDevPropBottomZOffsetCm()
	{
		return IslandBaseDevPropBottomZOffsetCm;
	}

	inline float GetIslandBaseDevPropHeightCm()
	{
		return IslandBaseDevPropHeightCm;
	}

	/** SSTO coast Z reconcile target — waterline / Island Base Dev Prop bottom (flat cap at shelf handoff). */
	inline float GetSstoCoastReconcileZCm()
	{
		return GetIslandBaseDevPropBottomZOffsetCm();
	}

	inline bool IsAdminFillTemplateDepressionWaterEnabled()
	{
		return bAdminFillTemplateDepressionWater;
	}

	inline bool IsCoastalLowlandMaskBuildEnabled()
	{
		// Overlay is forced off under floating iceberg — mask build is unused deadwood.
		if (IsCoastFloatingIcebergEnabled())
		{
			return false;
		}
		return bCoastalLowlandMaskBuildEnabled;
	}

	inline bool IsIslandSelectionDebugRingEnabled()
	{
		return bIslandSelectionDebugRingEnabled;
	}

	inline bool IsCoastalLowlandDevOverlayEnabled()
	{
		if (IsCoastFloatingIcebergEnabled())
		{
			return false;
		}
		if (bCoastGAProductionPresentation)
		{
			return false;
		}
		return bCoastalLowlandDevOverlayEnabled;
	}

	inline bool IsIslandBaseDevPropLowlandSectionEnabled()
	{
		return bIslandBaseDevPropLowlandSectionEnabled;
	}

	inline bool IsPieShoreOverlayInletCullEnabled()
	{
		return IsCoastPieBandPresentationEnabled() && bPieShoreOverlayInletCullEnabled;
	}

	// --- Phase B2 sea roots — slope-tier outward displacements (P1C09_SeaRoots_SlopeLookup_Design.md) ---

	static constexpr float SeaRootsTierBeachMaxSlopeDeg = 5.f;
	static constexpr float SeaRootsTierGentleMaxSlopeDeg = 40.f;
	static constexpr float SeaRootsTierSteepMaxSlopeDeg = 60.f;
	static constexpr float SeaRootsDispBeachMeters = 450.f;
	static constexpr float SeaRootsDispGentleMeters = 350.f;
	static constexpr float SeaRootsDispSteepMeters = 250.f;
	static constexpr float SeaRootsDispSheerMeters = 100.f;
	static constexpr float SeaRootsLakeMaxOutwardMeters = 100.f;
	static constexpr float SeaRootsLakeMaxFunctionalDepthMeters = 100.f;
	static constexpr float SeaRootsSlopeSampleInlandMeters = 40.f;
	/** Treat as beach tier when |Δz| over inland sample span is below this (m ASL). */
	static constexpr float SeaRootsSlopeFlatDeltaMeters = 0.5f;
	static constexpr int32 SeaRootsAzimuthSampleCount = 192;
	static constexpr float SeaRootsStampRecomputeDebounceSeconds = 1.0f;
	static constexpr float SeaRootsTanOuterFraction = 0.29f;
	static constexpr float SeaRootsCyanOuterFraction = 0.71f;
	/** Azimuth radii smooth passes after outer shelf polygon resample (B2). */
	static constexpr int32 SeaRootsAzimuthSmoothPasses = 6;
	/** B2c: light smooth on miter-joined offset rings before shelf/minimap consume (reduces self-cross). */
	static constexpr int32 SeaRootsOffsetRingSmoothIterations = 2;
	/** B2c: crisp Azgaar coast stroke width outward from true coast (m). */
	static constexpr float CoastStrokeOutwardWidthMeters = 4.f;
	/** PIE gold ribbon floor width (m) when capped width is off. Was 120 — caused chunky dark coast ring. */
	static constexpr float CoastStrokePieDisplayWidthMeters = 8.f;
	/** Validation pass: fixed full ribbon width (m) — minimap-scale gold line, not tank-scale band. */
	static constexpr float CoastStrokePieDisplayCappedWidthMeters = 10.f;
	/** Min gold ribbon width as fraction of island waterline radius (tank-scale overhead). */
	static constexpr float CoastStrokePieDisplayMinRadiusFraction = 0.012f;
	/** Min outward span for pie band annulus quads (m); azimuth rings fail centroid tests at 1.5 m. */
	static constexpr float PieShoreOverlayMinOutwardMeters = 0.25f;
	/** B2c: PIE shore overlay tan band Z above waterline (cm). */
	static constexpr float PieShoreOverlayTanZOffsetCm = 120.f;
	/** B2c: cyan / deep overlay Z below waterline (cm). */
	static constexpr float PieShoreOverlayCyanZOffsetCm = -50.f;
	static constexpr float PieShoreOverlayDeepZOffsetCm = -150.f;
	/** Vertical half-height of PIE coast band walls (cm) — visible edge-on from sea level. */
	static constexpr float PieShoreBandVerticalHalfHeightCm = 400.f;
	/** Vertical half-height of PIE gold coast stroke (cm). */
	static constexpr float CoastStrokeVerticalHalfHeightCm = 500.f;
	/** Gold stroke sits above tan band (cm above waterline) — legacy proc-mesh stacking. */
	static constexpr float CoastStrokeAboveTanBandCm = 90.f;
	/** PIE proc-mesh gold stroke draw Z (cm) — presentation only; not for screen overlay. */
	static constexpr float CoastStrokeWaterlineOffsetCm = 140.f;
	/** Screen-projected gold stroke Z (cm) — authority waterline @ z=0 per Placement Z checklist. */
	static constexpr float CoastStrokeScreenProjectZCm = 0.f;
	/** B2c: gold coast stroke — shared by PIE proc mesh (section 3) and minimap 2D replica. */
	static const FLinearColor CoastStrokeColor = FLinearColor(1.f, 0.86f, 0.28f, 1.f);
	/** Minimap-only: solid landmass fill drawn under the gold stroke, so islands/islets read as
	 * filled shapes instead of unfilled outlines. Darker/desaturated vs. the stroke so the gold
	 * outline still reads as a distinct edge on top of it. */
	static const FLinearColor MinimapLandFillColor = FLinearColor(0.5f, 0.42f, 0.2f, 0.92f);
	/** Minimap-only: inland-sea (interior water hole) rim, distinct from the gold coast/islet
	 * stroke - user-specified cyan #2C92B8. */
	static const FLinearColor MinimapInlandSeaStrokeColor = FLinearColor(0.1725f, 0.5725f, 0.7216f, 1.f);
	/** 2D replica band fills — match `BuildPieshoreBandOverlayProcMesh` opaque tints. */
	static const FLinearColor MinimapBandTanFillColor = FLinearColor(0.82f, 0.72f, 0.48f, 0.72f);
	static const FLinearColor MinimapBandCyanFillColor = FLinearColor(0.22f, 0.68f, 0.72f, 0.78f);
	static const FLinearColor MinimapBandDeepFillColor = FLinearColor(0.06f, 0.32f, 0.78f, 0.85f);
	/** Minimap camera heading glyph (V) — bright lime for contrast on cyan sea + gold coast. */
	static const FLinearColor MinimapCameraViewGlyphColor = FLinearColor(0.55f, 1.f, 0.05f, 1.f);
	static constexpr float MinimapCameraViewGlyphLineThicknessPx = 4.f;
	static constexpr float MinimapCameraViewGlyphApexDotRadiusPx = 4.f;
	/** PIE proc-mesh band tints (opaque — Tan/Cyan/Deep shelf only; land/wetland use IslandBase + CoastalLowland dev colors). */
	static const FLinearColor PieBandTanDisplayColor = FLinearColor(1.f, 0.88f, 0.45f, 1.f);
	static const FLinearColor PieBandCyanDisplayColor = FLinearColor(0.35f, 0.96f, 1.f, 1.f);
	static const FLinearColor PieBandDeepDisplayColor = FLinearColor(0.18f, 0.52f, 1.f, 1.f);

	inline FLinearColor GetPieBandDisplayColor(const int32 BandIdx)
	{
		switch (BandIdx)
		{
		case 0:
			return PieBandTanDisplayColor;
		case 1:
			return PieBandCyanDisplayColor;
		default:
			return PieBandDeepDisplayColor;
		}
	}
	/** B2 perf: uniform arc-length resample of MainCoast for land mesh + coast stroke (matches minimap silhouette). */
	static constexpr int32 CoastRenderPolyVerts = 768;
	/** @deprecated Use CoastRenderPolyVerts — kept for mask decimation cap. */
	static constexpr int32 CoastPolyInteriorMaxExteriorVerts = CoastRenderPolyVerts;
	/** @deprecated Use CoastRenderPolyVerts. */
	static constexpr int32 CoastStrokeMaxPolyVerts = CoastRenderPolyVerts;
	/** B2 perf: max lake holes in ear-clip (many lakes hang or fail triangulation). */
	static constexpr int32 CoastB2_MaxLakeHolesForEarClip = 16;
	/** B2c: land interior mask inset from coast ring — bridge skirt + shelf own the shore strip. */
	static constexpr float B2LandMaskInsetFromCoastMeters = 25.f;
	/** Sloped shelf: land to MainCoast waterline (0 inset); shelf owns seaward strip only. */
	inline float GetB2LandMaskInsetFromCoastMeters()
	{
		if (IsCoastFloatingIcebergEnabled() || IsPieShorePresentationSlopedShelfEnabled())
		{
			return 0.f;
		}
		return B2LandMaskInsetFromCoastMeters;
	}
	/** B2c: reject shelf quads when outward offset is below this (m). */
	static constexpr float SeaRootsMinValidShelfOutwardMeters = 1.5f;
	/** Phase A: sand lip above z=0 waterline (m ASL) for visible shore edge. */
	static constexpr float CoastPhaseA_WaterlineLipMeters = 0.15f;
	/** Phase A: lake holes smaller than this (km²) are filled as land for ear-clip stability. */
	static constexpr float CoastPhaseA_MinLakeHoleAreaKm2 = 0.008f;
	/** Phase A dev perf: max lake holes passed to ear-clip (POKED125 has many; 48 is very slow). */
	static constexpr int32 CoastPhaseA_MaxLakeHolesForTriangulation = 16;
	/** Phase A dev perf: waterline skirt samples (2048 is quality; 1024 is faster). */
	static constexpr int32 CoastPhaseA_WaterlineSkirtSampleCount = 1024;
	/** Phase A dev perf: max ear-clip retries when dropping smallest lake holes. */
	static constexpr int32 CoastPhaseA_MaxTriangulationAttempts = 2;
	/** Phase A perf: ear-clip on game thread can take minutes with lake holes — use fine grid instead. */
	static constexpr bool CoastPhaseA_SkipEarClipInterior = true;
	/** Phase A perf: max lake holes in fine-grid point-in-polygon (mask + interior). */
	static constexpr int32 CoastPhaseA_FineGridMaxLakeHoles = 16;
	/** Phase A: try ear-clip only when hole count <= this and a single land feature (if SkipEarClip false). */
	static constexpr int32 CoastPhaseA_MaxEarClipLakeHoles = 2;
	/** Max verts per closed polyline used for land-region mesh mask (point-in-polygon). */
	static constexpr int32 CoastMeshMaskMaxPolyVerts = 2048;
	/** Conforming shore band: 0 = match minimap/refined coast (no extra Chaikin). */
	static constexpr int32 CoastConformingBandDisplaySmoothIterations = 0;
	/** Chaikin iterations after edge noise (Phase F3: 1→2 when refine-noise path enabled). */
	static constexpr int32 CoastPolylineSmoothIterations = 2;
	/** Upsample height grid for coast extract only (3 = finer boundary before A+++). */
	static constexpr int32 CoastExtractUpsampleFactor = 3;
	/** Densify spacing before coast-edge noise (km). */
	static constexpr float CoastEdgeNoiseDensifySpacingKm = 0.05f;
	/** Max straight polyline segment before breaker inserts perturbations (km). */
	static constexpr float CoastMaxStraightSegmentKm = 0.18f;
	/** Multi-octave normal-offset amplitudes (km) — macro / meso / micro. */
	static constexpr float CoastNoiseMacroAmpKm = 0.28f;
	static constexpr float CoastNoiseMesoAmpKm = 0.09f;
	static constexpr float CoastNoiseMicroAmpKm = 0.022f;
	static constexpr float CoastNoiseMacroWavelengthKm = 2.2f;
	static constexpr float CoastNoiseMesoWavelengthKm = 0.55f;
	static constexpr float CoastNoiseMicroWavelengthKm = 0.09f;
	/** Extra Chaikin pass ratio on coast/lake loops (A++). */
	static constexpr float CoastPolylineSmoothCutRatio = 0.25f;
	/** Densify spacing before coast-band mesh (km). */
	static constexpr float CoastContourDensifySpacingKm = 0.03f;
	/** Inward coast/lake-shore band depth (multiples of fine cell size). */
	static constexpr float CoastInwardBandDepthFactor = 2.5f;
	/** Shelf skirt strip spacing along smoothed coast (km). */
	static constexpr float CoastShelfBandDensifySpacingKm = 0.025f;
	/** Macro shelf guide poly: Chaikin on copy of coast — used for outer ring offsets only (not inner edge). */
	static constexpr int32 ShelfGuideSmoothIterations = 3;
	static constexpr float ShelfGuideSmoothCutRatio = 0.32f;
	/** Post-offset Chaikin per shelf contour (0=detail coast, no smooth … 3=deep outer). */
	static constexpr int32 ShelfRing0PostOffsetSmoothIterations = 0;
	static constexpr int32 ShelfRing1PostOffsetSmoothIterations = 1;
	static constexpr int32 ShelfRing2PostOffsetSmoothIterations = 3;
	static constexpr int32 ShelfRing3PostOffsetSmoothIterations = 4;
	static constexpr float ShelfRingPostOffsetSmoothCutRatio = 0.28f;
	/** Interior lakes: light smooth only (no A+++ edge noise — avoids N× lake refine cost). */
	static constexpr float CoastLakeSmoothDensifySpacingKm = 0.05f;
	static constexpr int32 CoastLakeSmoothIterations = 0;
	/** Tank layout spacing — max coast extent over semi-major so island meshes do not merge. */
	static constexpr float LayoutCollisionRadiusFactor = 1.48f;

	// --- PIE interior topography PBR (M_IslandTopography) ---

	/** TOPO-01: bake height/slope vertex colors + drive M_IslandTopography MID params. */
	static constexpr bool bCoastTOPO01_IslandTopographyEnabled = true;
	/** Beach sand band upper bound (m ASL) — matches material BeachZMax. */
	static constexpr float TopographyBeachZMaxMeters = 15.f;
	/** Prefer Sand tier on the walkable lip (ASL 0–few m) so Beach arcs read continuous to waterline. */
	static constexpr float TopographyBeachLipSandMaxMeters = 4.f;
	/**
	 * Weld skirt demoted: IslandMesh waterline clamp seals Z-omit teeth; apron is optional polish only.
	 */
	static constexpr bool bSandApronEnabled = false;
	inline bool IsSandApronEnabled()
	{
		return bSandApronEnabled;
	}
	static constexpr float SandApronWidthCells = 0.85f;
	/** Fallback only: push gold outer seaward past MS tooth tips. */
	static constexpr float SandApronSeawardCells = 0.25f;
	static constexpr float SandApronZCm = 12.f;
	static constexpr int32 SandApronMaxRingVerts = 320;
	/** IslandMesh: keep coast-straddle tris with Z=max(Z,0); omit all-wet only (no open rim teeth). */
	static constexpr bool bIslandWaterlineClampEnabled = true;
	inline bool IsIslandWaterlineClampEnabled()
	{
		return bIslandWaterlineClampEnabled;
	}
	/**
	 * ContourGold rim-strip v2 (inlet-safe): omit Chebyshev≤1 MS rim quads when enabled;
	 * ContourGold↔dry strip with max-edge + wet mid-edge reject (no ASL-0 flood-fill chords).
	 * Method FAIL for silhouette (high reject + omit half-state) — keep OFF. Use coast belt.
	 * Do No Harm: do not re-enable without a new inlet-safe design + ask.
	 */
	static constexpr bool bContourGoldRimStripEnabled = false;
	inline bool IsContourGoldRimStripEnabled()
	{
		return bContourGoldRimStripEnabled;
	}
	/**
	 * ContourGold coastal-belt remesh v1 (Problem A): ContourGold outer ↔ inland offset strip
	 * appended to IslandMesh. No global Cheb omit (avoids rim v2 holes). Max-edge + wet mid
	 * reject for inlet safety. Not SandApron. Do No Harm: if C1 chords → set false immediately.
	 */
	static constexpr bool bContourGoldCoastBeltEnabled = true;
	inline bool IsContourGoldCoastBeltEnabled()
	{
		return bContourGoldCoastBeltEnabled;
	}
	/** Cap strip ring verts (Chaikin ContourGold can be dense). */
	static constexpr int32 ContourGoldRimStripMaxRingVerts = 480;
	/** Shared legacy rim-strip max-edge mul (rim v2 OFF; kept for reference). */
	static constexpr float ContourGoldRimStripMaxEdgeSpacingMul = 2.5f;
	/**
	 * Coast belt (Problem A polish): accept longer ContourGold outer edges so reject rate
	 * drops and IslandMesh rim fills ContourGold authority (Minecraft stairs). Inlet-safe
	 * wet mid reject still applies. Do not unlock via samples-1025.
	 */
	static constexpr float ContourGoldCoastBeltMaxEdgeSpacingMul = 4.0f;
	/** Coast belt inland offset as multiple of sample spacing (meters). Wider = more fill. */
	static constexpr float ContourGoldCoastBeltInlandSpacingMul = 1.5f;
	/** Coast belt ring vert cap (higher than rim strip — less long-edge reject after decimate). */
	static constexpr int32 ContourGoldCoastBeltMaxRingVerts = 720;
	/**
	 * Unitary WWF collision: ShelfMesh QueryAndPhysics so hard-hat / pier / cofferdam can use
	 * gold→magenta shelf. Visual cyan loft stays on ShelfMesh; IslandMesh remains dry TOPO.
	 * Interim Contours PASS used NoCollision — this promotes the agreed final gameplay goal.
	 */
	static constexpr bool bWwfShelfCollisionEnabled = true;
	inline bool IsWwfShelfCollisionEnabled()
	{
		return bWwfShelfCollisionEnabled;
	}

	inline bool IsCoastTOPO01IslandTopographyEnabled()
	{
		return bCoastTOPO01_IslandTopographyEnabled;
	}

	/** World-aligned MWAM UV scale (cm) — smaller = finer ground texture. */
	static constexpr float TopographyTextureWorldScaleCm = 4200.f;
	/** Vertex-color macro relief blend — proc mesh cannot use VertexColor expr (see pie-band comment). */
	static constexpr float TopographyVertexColorReliefBlend = 0.f;
	/** Keep 0 — ForceVertexColorOnly=1 reads white on proc mesh unless verts are explicitly tinted. */
	static constexpr float TopographyForceVertexColorOnly = 0.f;

	/** DEV ONLY step 1: ForceVertexColorOnly + all verts #9AE630 — FAILED on IslandMesh top 2026-07-03. */
	static constexpr bool bTopographyDevVertexColorOnlyProof = false;

	inline float GetTopographyForceVertexColorOnly()
	{
		return bTopographyDevVertexColorOnlyProof ? 1.f : TopographyForceVertexColorOnly;
	}
	/** Use MWAM texture samples in height material (off = solid tier fallbacks). */
	static constexpr bool bCoastTOPO01_UseMwamTextures = false;
	/** TOPO-01f: constant unlit tier section MIDs — ENABLED 2026-07-03 (step 1 material proof). */
	static constexpr bool bCoastTOPO01_SectionConstantMaterialsEnabled = true;
	/** TOPO-01g: Default Lit tier section MIDs (SandyGrey-style parent) — ENABLED 2026-07-03 (step 2). */
	static constexpr bool bCoastTOPO01_SectionLitMaterialsEnabled = true;
/**
	 * Hull presentation pass (2026-07-09): when lit TOPO tiers are on, still draw IslandMesh in the
	 * main pass with SandyGrey lit (SeaRoots-preferred look) so ICE-01m walls + −25 m shelf caps are
	 * visible. TOPO tier overlay meshes are skipped while this is on (same verts = shimmer/z-fight).
	 * OFF when AslSlopeTierOverlays is on (WB/game lighting vegetative cover path).
	 */
	static constexpr bool bCoastTOPO01_HullMainPassWithLitTiersEnabled = true;
	/**
	 * WB / game: lit TOPO tier overlays (Sand/Grass/Dirt/Rock/Snow) from ASL + slope classify.
	 * When true with LitOpaqueTerrainMaterials, hull-only SandyGrey pass is skipped so tiers render.
	 */
	static constexpr bool bCoastTOPO01_AslSlopeTierOverlaysEnabled = true;
	/** TOPO-01j: hide ICE-02 solid frustum in PIE — PARKED (off = restore ICE-02d wireframe grid). */
	static constexpr bool bCoastTOPO01_HideSeaRootsSolidMeshInPie = false;
	/** TOPO-01h step 4: warmer/saturated tier display RGB on lit BasicShapeMaterial MIDs. */
	static constexpr bool bCoastTOPO01_Step4TunedTierColorsEnabled = true;
	/** TOPO-01j step 5: force rock tier on steep cliff triangles (face-normal slope). */
	static constexpr bool bCoastTOPO01_Step5CliffTriangleRockEnabled = true;
	/** TOPO-01j step 5b: force rock on high Z-span tread tris (landing-cove sawtooth rim). */
	static constexpr bool bCoastTOPO01_Step5bCliffZSpanRockEnabled = true;
	/** Min vertex Z span (cm) for step 5b cliff tread rock — per-triangle tread at §2b wedge (not total cliff height). */
	static constexpr float TopographyCliffZSpanRockMinCm = 400.f;
	/** Step 5b: min Z-span / max horizontal edge for sawtooth tread tris (was 1.0 — too steep for rim caps). */
	static constexpr float TopographyCliffZSpanPerHorizontalMin = 0.12f;

	inline bool IsCoastTOPO01Step4TunedTierColorsEnabled()
	{
		return bCoastTOPO01_Step4TunedTierColorsEnabled;
	}

	inline bool IsCoastTOPO01Step5CliffTriangleRockEnabled()
	{
		return bCoastTOPO01_Step5CliffTriangleRockEnabled;
	}

	inline bool IsCoastTOPO01Step5bCliffZSpanRockEnabled()
	{
		return bCoastTOPO01_Step5bCliffZSpanRockEnabled;
	}

	inline bool IsCoastTOPO01SectionConstantMaterialsEnabled()
	{
		return bCoastTOPO01_IslandTopographyEnabled && bCoastTOPO01_SectionConstantMaterialsEnabled;
	}

	inline bool IsCoastTOPO01SectionLitMaterialsEnabled()
	{
		return IsCoastTOPO01SectionConstantMaterialsEnabled() && bCoastTOPO01_SectionLitMaterialsEnabled;
	}

	inline bool IsCoastTOPO01HullMainPassWithLitTiersEnabled()
	{
		// Lit opaque terrain + ASL/slope overlays → full TOPO tier split under game lighting.
		if (IsDevDemoLitOpaqueTerrainMaterialsEnabled()
			&& bCoastTOPO01_AslSlopeTierOverlaysEnabled)
		{
			return false;
		}
		return IsCoastTOPO01SectionLitMaterialsEnabled()
			&& bCoastTOPO01_HullMainPassWithLitTiersEnabled;
	}

	inline bool IsCoastTOPO01AslSlopeTierOverlaysEnabled()
	{
		return IsDevDemoLitOpaqueTerrainMaterialsEnabled()
			&& bCoastTOPO01_AslSlopeTierOverlaysEnabled
			&& IsCoastTOPO01SectionLitMaterialsEnabled();
	}

	inline bool IsCoastTOPO01HideSeaRootsSolidMeshInPie()
	{
		return IsCoastTOPO01SectionConstantMaterialsEnabled()
			&& bCoastTOPO01_HideSeaRootsSolidMeshInPie
			&& IsCoastICE02IslandFrustumEnabled();
	}

	static constexpr int32 TopographyTierCount = 5;

	/** DEV ONLY: neon proof tint on rock tier (#9AE630). OFF during vertex-color proof. */
	static constexpr bool bTopographyDevRockColorTest = false;
	/** sRGB #9AE630 — shared dev proof chroma */
	static constexpr float TopographyDevNeonProofColorR = 154.f / 255.f;
	static constexpr float TopographyDevNeonProofColorG = 230.f / 255.f;
	static constexpr float TopographyDevNeonProofColorB = 48.f / 255.f;

	inline FLinearColor GetTopographyDevNeonProofColor()
	{
		return FLinearColor(
			TopographyDevNeonProofColorR,
			TopographyDevNeonProofColorG,
			TopographyDevNeonProofColorB,
			1.f);
	}

	inline FLinearColor GetTopographyTierDisplayColor(const int32 TierIdx)
	{
		if (bCoastTOPO01_Step4TunedTierColorsEnabled)
		{
			switch (TierIdx)
			{
			case 0:
				return FLinearColor(0.76f, 0.68f, 0.48f, 1.f);
			case 1:
				return FLinearColor(0.18f, 0.62f, 0.14f, 1.f);
			case 2:
				return FLinearColor(0.55f, 0.38f, 0.22f, 1.f);
			case 3:
				return bTopographyDevRockColorTest
					? GetTopographyDevNeonProofColor()
					: FLinearColor(0.45f, 0.42f, 0.38f, 1.f);
			default:
				return FLinearColor(0.93f, 0.95f, 0.98f, 1.f);
			}
		}

		switch (TierIdx)
		{
		case 0:
			return FLinearColor(0.82f, 0.72f, 0.50f, 1.f);
		case 1:
			return FLinearColor(0.22f, 0.55f, 0.18f, 1.f);
		case 2:
			return FLinearColor(0.48f, 0.40f, 0.30f, 1.f);
		case 3:
			return bTopographyDevRockColorTest
				? GetTopographyDevNeonProofColor()
				: FLinearColor(0.52f, 0.48f, 0.42f, 1.f);
		default:
			return FLinearColor(0.92f, 0.94f, 0.98f, 1.f);
		}
	}
	/** Unlit emissive scale on relief albedo — pie-band parity (~12) keeps tiers readable without sun. */
	static constexpr float TopographyReliefEmissiveScale = 12.0f;
	/** TOPO-01g: per-section MID emissive boost (M_IslandTopoSection / engine unlit parent). */
	static constexpr float TopographySectionEmissiveBoost = 24.0f;
	/** N·L shade strength for heightfield relief (higher = more contrast). */
	static constexpr float TopographyReliefShadeStrength = 0.82f;
	/** Minimum emissive on interior albedo (keeps relief readable without sun). */
	static constexpr float TopographyMinAmbient = 0.42f;
	/** WB DEV GrabContrast ON: albedo multiply (vs washout from sun+bright TOPO). */
	static constexpr float TopographyGrabContrastAlbedoScale = 0.72f;
	/** Slope steepness (1-|N·Up|) where rock weight begins / saturates. */
	static constexpr float TopographySlopeRockStart = 0.15f;
	static constexpr float TopographySlopeRockFull = 0.48f;
	static constexpr float TopographyColorBoost = 1.0f;
	static constexpr float TopographyRoughness = 0.66f;
	static constexpr float TopographySpecular = 0.2f;

	/** Square height-grid coast polylines can reach ~√2 × cell-map half-extent from island center. */
	static constexpr float SquareGridCoastEnvelopeFactor = 1.414213562f;
	/** Highest terrain apex (m ASL). */
	static constexpr float MountainApexMeters = 2400.f;

	// --- Town Grid GIS Blueprint Layer — road overlay (map overlay; not UE5 Blueprint graphs) ---

	/**
	 * Canonical focus / selection outline blue (IH_P1C08_DevPanelStyle::RowSelectionOutlineColor).
	 * Hex #66BFFF · sRGB ~(102, 191, 255) · FLinearColor(0.4, 0.75, 1.0, 1).
	 */
	static const FLinearColor TownGridFocusOutlineBlue = FLinearColor(0.4f, 0.75f, 1.0f, 1.f);

	/** Visual hierarchy for Town Grid road splines; line thickness encodes class at Game Optimal / grid-overlay zoom. */
	enum class ETownGridRoadOverlayClass : uint8
	{
		MajorArtery = 0,
		MinorArtery,
		Collector,
		LocalStreet,
		BackAlley,
	};

	/** Overlay stroke width (screen px at Town Grid GIS Blueprint Layer reference zoom). */
	static constexpr float TownGridRoadOverlayThicknessPx_MajorArtery = 4.0f;
	static constexpr float TownGridRoadOverlayThicknessPx_MinorArtery = 3.0f;
	static constexpr float TownGridRoadOverlayThicknessPx_Collector = 2.5f;
	static constexpr float TownGridRoadOverlayThicknessPx_LocalStreet = 2.0f;
	static constexpr float TownGridRoadOverlayThicknessPx_BackAlley = 1.5f;

	/** Procedural road corridor width (world meters); aligns with 8/12/16/20 m parcel frontage grid. */
	static constexpr float TownGridRoadWorldWidthM_MajorArtery = 16.f;
	static constexpr float TownGridRoadWorldWidthM_MinorArtery = 12.f;
	static constexpr float TownGridRoadWorldWidthM_Collector = 10.f;
	static constexpr float TownGridRoadWorldWidthM_LocalStreet = 8.f;
	static constexpr float TownGridRoadWorldWidthM_BackAlley = 4.f;

	/** Alpha multipliers on TownGridFocusOutlineBlue (same hue family; thinner tiers slightly softer). */
	static constexpr float TownGridRoadOverlayAlpha_MajorArtery = 1.00f;
	static constexpr float TownGridRoadOverlayAlpha_MinorArtery = 0.95f;
	static constexpr float TownGridRoadOverlayAlpha_Collector = 0.90f;
	static constexpr float TownGridRoadOverlayAlpha_LocalStreet = 0.85f;
	static constexpr float TownGridRoadOverlayAlpha_BackAlley = 0.75f;

	inline float GetTownGridRoadOverlayThicknessPx(ETownGridRoadOverlayClass Class)
	{
		switch (Class)
		{
		case ETownGridRoadOverlayClass::MajorArtery: return TownGridRoadOverlayThicknessPx_MajorArtery;
		case ETownGridRoadOverlayClass::MinorArtery: return TownGridRoadOverlayThicknessPx_MinorArtery;
		case ETownGridRoadOverlayClass::Collector: return TownGridRoadOverlayThicknessPx_Collector;
		case ETownGridRoadOverlayClass::LocalStreet: return TownGridRoadOverlayThicknessPx_LocalStreet;
		default: return TownGridRoadOverlayThicknessPx_BackAlley;
		}
	}

	inline float GetTownGridRoadWorldWidthMeters(ETownGridRoadOverlayClass Class)
	{
		switch (Class)
		{
		case ETownGridRoadOverlayClass::MajorArtery: return TownGridRoadWorldWidthM_MajorArtery;
		case ETownGridRoadOverlayClass::MinorArtery: return TownGridRoadWorldWidthM_MinorArtery;
		case ETownGridRoadOverlayClass::Collector: return TownGridRoadWorldWidthM_Collector;
		case ETownGridRoadOverlayClass::LocalStreet: return TownGridRoadWorldWidthM_LocalStreet;
		default: return TownGridRoadWorldWidthM_BackAlley;
		}
	}

	inline FLinearColor GetTownGridRoadOverlayColor(ETownGridRoadOverlayClass Class)
	{
		float Alpha = TownGridRoadOverlayAlpha_BackAlley;
		switch (Class)
		{
		case ETownGridRoadOverlayClass::MajorArtery: Alpha = TownGridRoadOverlayAlpha_MajorArtery; break;
		case ETownGridRoadOverlayClass::MinorArtery: Alpha = TownGridRoadOverlayAlpha_MinorArtery; break;
		case ETownGridRoadOverlayClass::Collector: Alpha = TownGridRoadOverlayAlpha_Collector; break;
		case ETownGridRoadOverlayClass::LocalStreet: Alpha = TownGridRoadOverlayAlpha_LocalStreet; break;
		default: break;
		}
		return FLinearColor(
			TownGridFocusOutlineBlue.R, TownGridFocusOutlineBlue.G, TownGridFocusOutlineBlue.B, Alpha);
	}

	/**
	 * M3 DrawDebug line thickness (world cm) from road corridor width — readable at bird's-eye on 128 m grids.
	 * M4 replaces DrawDebug with translucent mesh; this scales meters→cm via WidthScale (default 8%).
	 */
	inline float GetTownGridDebugOverlayLineThicknessCm(
		ETownGridRoadOverlayClass Class,
		float WidthScale = 0.08f,
		float MinThicknessCm = 24.f)
	{
		const float WidthCm = GetTownGridRoadWorldWidthMeters(Class) * 100.f;
		return FMath::Max(MinThicknessCm, WidthCm * WidthScale);
	}

	/**
	 * roadwayType → overlay class (Roadway Network table enum: Major, Minor, Path, Trail):
	 *   Major → MajorArtery
	 *   Minor → MinorArtery (Collector when FRoadSegment marks collector role)
	 *   Path  → LocalStreet
	 *   Trail → BackAlley
	 *
	 * Display suffix (GamePlay Manual §8.1 Step 5) encodes network role / geometry, not compass.
	 */
	enum class ERoadNetworkLayer : uint8
	{
		TownGrid = 0,
		Regional,
	};

	enum class ERoadDisplaySuffix : uint8
	{
		/** Urban rear/service access within blocks (~4 m ROW). */
		Lane = 0,
		/** Collector only (~10–13 m); never 8 m local frontage roads. */
		Street,
		/** Artery following active town grid (orthogonal / template-aligned). */
		Avenue,
		/** Artery breaking grid (diagonal, radial, ring); same ROW band as Avenue. */
		Boulevard,
		/** Rural winding path; obstacle boundary (contour, shore, foreign perimeter). */
		Way,
		/** Inter-jurisdiction connector; named for lower-tier jurisdiction (Regional layer). */
		Road,
	};

	/** True when suffix should be Avenue (grid-aligned artery) vs Boulevard (off-grid). */
	inline bool IsGridAlignedArtery(ERoadDisplaySuffix Suffix)
	{
		return Suffix == ERoadDisplaySuffix::Avenue;
	}

	/** Regional Roads use lower-tier jurisdiction name + " Road" (all roads lead to Rome). */
	inline bool IsRegionalInterJurisdictionRoad(ERoadDisplaySuffix Suffix, ERoadNetworkLayer Layer)
	{
		return Layer == ERoadNetworkLayer::Regional && Suffix == ERoadDisplaySuffix::Road;
	}

	// --- Town Grid GIS Blueprint Layer — five canonical templates (GamePlay Manual §8.1 Step 2) ---

	enum class ETownGridTemplate : uint8
	{
		/** T1 — Orthogonal grid, central commons; pioneer settlements. */
		Squared = 0,
		/** T2 — Golden-rectangle blocks (φ lookup); chartered towns/cities. */
		Harmonic,
		/** T3 — Radial Boulevards + 3 Collector rings + roundabouts. */
		Radial,
		/** T4 — Hillock elliptical MIL ROW wall; cloistered interior grid. */
		Citadel,
		/** T5 — Valley / saddle / cleft spine; contour-aligned blocks. */
		Valley,
	};

	/** Radial template: 6, 8, or 12 spokes (default 12). */
	static constexpr int32 TownGridRadialSpokeCountDefault = 12;

	/** Citadel template: elliptical MIL rampart aspect ratio (major/minor axis). */
	static constexpr float TownGridCitadelEllipseAspectDefault = 1.618f;

	/** Citadel gates auto-match count to nearest Regional Road endpoints when adaptive. */
	static constexpr int32 TownGridCitadelGateCountMax = 6;

	/** Default central commons / plaza zoning: CIV + SPD (all templates). */
	static constexpr int32 TownGridDefaultCommonsZoneCount = 2;

	/** Max tangent angle (degrees) for auto-link at grid merge; design band 12–15°. Exceed → cul-de-sac on newer grid. */
	static constexpr float TownGridMergeLinkMaxAngleDeg = 15.f;

	/** Global max terrain slope (degrees) for grid stretch; refuse above → Way contour boundary. */
	static constexpr float TownGridMaxStretchSlopeDeg = 12.f;

	/** Valley template: cross-slope Lane/Way grip exception. */
	static constexpr float TownGridValleyCrossSlopeMaxDeg = 18.f;

	/** Minimum fillet radius when Collectors curve to intersect a Way boundary (one 8 m module). */
	static constexpr float TownGridCollectorWayFilletMinM = 8.f;

	enum class ETownGridEditMode : uint8
	{
		Place = 0,
		Move,
		GripEdit,
	};

	enum class ETownGridEdgeFlash : uint8
	{
		Valid = 0,
		MergeOwn,
		ForeignRefuse,
		SlopeRefuse,
	};

	enum class EWayBoundaryCause : uint8
	{
		TerrainContour = 0,
		Shoreline,
		WaterBodyPerimeter,
		ForeignGridPerimeter,
	};

	/**
	 * Merge: yellow = own/allied older grid. Orange = foreign-owned refuse (not wilderness).
	 * Red/orange → Way boundary; Collectors curve to intersect (min TownGridCollectorWayFilletMinM).
	 * Blueprint Layer stretch is free (no Volunteer Hours). Radial v1: outer ring only.
	 */
	enum class ETownGridMergeLinkClass : uint8
	{
		ArteryToArtery = 0,
		CollectorToCollector,
		RegionalRoadToBoulevard,
		TrimOrCulDeSac,
	};

	// --- Right Build Palette (InvisibleHand_RightBuildPalette_Catalog.md) ---

	enum class EBuildPaletteTab : uint8
	{
		Grid = 0,
		World,
		Build,
		Convey,
		Defense,
	};

	enum class EBuildPaletteInteraction : uint8
	{
		DropActor = 0,
		SplineOpen,
		SplineClosed,
		PaintBrush,
		GripTemplate,
		CompositePackage,
		SliderPanel,
	};

	enum class EBuildPaletteLevel : uint8
	{
		WorldBuilder = 0,
		MainGame,
		Combat,
	};

	enum class ETerrainStampFamily : uint8
	{
		Vertical = 0,
		Inverted,
	};

	/** DT_TerrainStamp row count (canonical). */
	static constexpr int32 TerrainStampCount = 22;
}
