// Copyright Epic Games, Inc. All Rights Reserved.
// Coast polyline refinement: densify, edge noise, straight-segment break, light smooth.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"

class IH_WB_DEMO004_API FIHCoastPolylineSmoothing
{
public:
	/** Smooth a closed polyline in km; removes duplicate closing vertex if present. */
	static void SmoothClosedPolylineKm(TArray<FVector2D>& InOutPolylineKm, int32 Iterations, float CutRatio = 0.25f);

	/** Insert vertices so no segment exceeds MaxSpacingKm (closed loop). */
	static void DensifyClosedPolylineKm(TArray<FVector2D>& InOutPolylineKm, float MaxSpacingKm);

	/** Resample closed polyline to a fixed vertex count (uniform arc-length spacing). */
	static void ResampleClosedPolylineUniformCount(
		const TArray<FVector2D>& PolylineKm,
		int32 TargetCount,
		TArray<FVector2D>& OutPolylineKm);

	/** Resample SourceKm to ReferenceKm.Num() points at matching fractional arc-length keys. */
	static void ResampleClosedPolylineToReferenceArcLength(
		const TArray<FVector2D>& ReferenceKm,
		const TArray<FVector2D>& SourceKm,
		TArray<FVector2D>& OutPolylineKm);

	/** @deprecated Use ResampleClosedPolylineToReferenceArcLength when a detail-coast reference exists. */
	static void ResampleClosedPolylineToArcLengthKeys(
		const TArray<FVector2D>& SourceKm,
		int32 ReferenceCount,
		TArray<FVector2D>& OutPolylineKm);

	/**
	 * For each point in ReferenceKm, finds its nearest point on the closed OuterKm polyline
	 * (perpendicular projection onto each of Outer's segments, not just nearest vertex) - a
	 * genuinely geographic correspondence, unlike ResampleClosedPolylineToReferenceArcLength's
	 * proportional-arc-length one, which only guarantees similar POSITION-ALONG-LOOP-LENGTH, not
	 * geographic proximity (IH-DEC-039: root cause of a 50-60% quad-rejection rate in the WWF shelf
	 * ramp on real, deliberately-irregular coastlines, where the two loops' arc-length-to-geography
	 * relationships diverge). O(ReferenceKm.Num() * OuterKm.Num()) - no spatial acceleration; measure
	 * real generation time before adding one.
	 */
	static void ProjectClosedPolylineOntoNearestPoint(
		const TArray<FVector2D>& ReferenceKm,
		const TArray<FVector2D>& OuterKm,
		TArray<FVector2D>& OutPairedKm);

	static bool IsPointInsideClosedPolylineKm(const FVector2D& PointKm, const TArray<FVector2D>& PolylineKm);

	/** Signed area (km²); positive = CCW. Reverses poly when CW so inside tests stay valid. */
	static float EnsureCounterClockwiseClosedPolylineKm(TArray<FVector2D>& InOutPolylineKm);

	/** Constant-width outward offset with miter/bevel corners (km) — shelf rings. */
	static void OffsetClosedPolylineOutwardKm(
		const TArray<FVector2D>& PolylineKm,
		float OutwardOffsetKm,
		bool bLandInsidePoly,
		TArray<FVector2D>& OutOffsetKm);

	/** Variable per-vertex outward offset with miter/bevel corners (km) — SeaRoots tier rings. */
	static void OffsetClosedPolylineVariableOutwardKm(
		const TArray<FVector2D>& PolylineKm,
		const TArray<float>& VertexOffsetKm,
		bool bLandInsidePoly,
		TArray<FVector2D>& OutOffsetKm);

	/** True when an annulus quad radiates outward along the segment normal (rejects loop-back). */
	static bool IsQuadOutwardValid(
		const FVector2D& InnerA,
		const FVector2D& InnerB,
		const FVector2D& OuterA,
		const FVector2D& OuterB,
		const FVector2D& OutwardNormal,
		float MinOutwardKm);

	/** Same test with MinOutwardDistance in the same units as Inner/Outer coordinates (cm, map px, etc.). */
	static bool IsShelfAnnulusQuadOutwardValid(
		const FVector2D& InnerA,
		const FVector2D& InnerB,
		const FVector2D& OuterA,
		const FVector2D& OuterB,
		const FVector2D& OutwardNormal,
		float MinOutwardDistance);

	/**
	 * A+++ pipeline: densify → break long straights → multi-octave edge noise → light Chaikin.
	 * @param bLandInsidePoly True for exterior land coast; false for enclosed lake holes.
	 */
	static void RefineCoastPolylineKm(
		TArray<FVector2D>& InOutPolylineKm,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		bool bLandInsidePoly,
		float NoiseAmpScale = 1.f);

	/** Lakes: densify, break grid stairsteps (no A+++ noise), light Chaikin. */
	static void LightRefineEnclosedLakePolylineKm(
		TArray<FVector2D>& InOutPolylineKm,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt);

	/** C1d: metrics for grid-aligned stairstep coast artifacts (km space). */
	struct FIHCoastGridArtifactMetrics
	{
		float MaxCardinalRunKm = 0.f;
		int32 Prominent90CornerCount = 0;
		float CardinalPerimeterFraction = 0.f;
	};

	static void ComputeCoastGridArtifactMetricsKm(
		const TArray<FVector2D>& PolylineKm,
		FIHCoastGridArtifactMetrics& OutMetrics);

	/** C1d: collinear cardinal run breaker + optional 90° chamfer (pre-authority resample). */
	static void ApplyCoastC1dGridArtifactRemedyKm(
		TArray<FVector2D>& InOutPolylineKm,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		bool bLandInsidePoly,
		float NoiseAmpScale);

	/** Ear-clip triangulation of a CCW simple polygon; emits triangle vertex indices. */
	static bool TriangulateSimplePolygonCCW(
		const TArray<FVector2D>& PolygonKm,
		TArray<int32>& OutTriangleVertexIndices);

	/** Drop degenerate/collinear verts before ear-clip (matches TriangulateSimplePolygonCCW input). */
	static void SanitizeClosedPolylineForTriangulationKm(TArray<FVector2D>& InOutPolylineKm);

	/**
	 * Triangulate CCW outer with CW holes: bridge each hole into outer, then ear-clip.
	 * @param OuterKm CCW exterior boundary (no duplicate closing vertex).
	 * @param HolesKm CW hole loops (no duplicate closing vertex).
	 */
	static bool TriangulatePolygonWithHoles(
		const TArray<FVector2D>& OuterKm,
		const TArray<TArray<FVector2D>>& HolesKm,
		TArray<int32>& OutTriangleVertexIndices,
		TArray<FVector2D>& OutVerticesKm);

	/**
	 * Bake-once MainCoast: resample to MaxVerts, macro silhouette, loop/hairpin sanitize.
	 */
	static void PrepareMainCoastAuthorityPolylineKm(
		const TArray<FVector2D>& SourceKm,
		int32 MaxVerts,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		bool bLandInsidePoly,
		float NoiseAmpScale,
		TArray<FVector2D>& OutAuthorityKm);

	/** Local-cm wrapper for PrepareMainCoastAuthorityPolylineKm. */
	static void PrepareMainCoastAuthorityPolylineLocalCm(
		const TArray<FVector2D>& SourceLocalCm,
		int32 MaxVerts,
		int32 MasterSeed,
		int32 IslandIndex,
		int32 PolySalt,
		bool bLandInsidePoly,
		float NoiseAmpScale,
		TArray<FVector2D>& OutAuthorityLocalCm);

	/**
	 * Stroke/minimap sanitization (resample + hairpin/spur/loop removal).
	 */
	static void PrepareClosedPolylineForCoastStrokeKm(
		const TArray<FVector2D>& SourceKm,
		int32 MaxVerts,
		TArray<FVector2D>& OutStrokeKm);

	/** Hairpin/spur/loop sanitize only — no resample (baked authority stroke path). */
	static void SanitizeClosedPolylineForCoastStrokeKm(TArray<FVector2D>& InOutPolylineKm);

	/** Local-cm wrapper for PrepareClosedPolylineForCoastStrokeKm. */
	static void PrepareClosedPolylineForCoastStrokeLocalCm(
		const TArray<FVector2D>& SourceLocalCm,
		int32 MaxVerts,
		TArray<FVector2D>& OutStrokeLocalCm);

	/**
	 * PIE coast stroke + minimap M1: baked authority sanitize, else full stroke prep @768.
	 */
	static void ResolveCoastStrokePolylineLocalCm(
		const TArray<FVector2D>& SourceLocalCm,
		TArray<FVector2D>& OutStrokeLocalCm);
};
