// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

namespace IH_P1C08_MinimapCoastline
{
	/** Slice a procedural mesh at a world-Z plane; returns an ordered closed coastline polyline (world XY cm). */
	bool BuildWaterlinePolylineFromMeshSection(
		UProceduralMeshComponent* MeshComp,
		int32 SectionIndex,
		float WorldZSliceCm,
		TArray<FVector2D>& OutWorldXY,
		float MergeToleranceCm = 150.f);

	/** Drop vertices closer than MinSpacingPx; cap to MaxVertices (0 = no cap). Open loop OK for drawing. */
	void DecimatePolylineMapPx(
		const TArray<FVector2D>& InPoints,
		float MinSpacingPx,
		int32 MaxVertices,
		TArray<FVector2D>& OutPoints);

	/** Build closed world XY polygon from azimuth radii (cm from AzimuthOriginLocalCm, not actor pivot). */
	void BuildWorldPolygonFromAzimuthRadiiCm(
		const FVector2D& CenterWorldCm,
		float YawDegrees,
		const FVector2D& AzimuthOriginLocalCm,
		const TArray<float>& RadiiCm,
		TArray<FVector2D>& OutWorldXY);

	/** Build closed world XY polygon from island-local cm polyline (yaw + origin). */
	void BuildWorldPolygonFromLocalPolylineCm(
		const FVector2D& CenterWorldCm,
		float YawDegrees,
		const TArray<FVector2D>& LocalPolylineCm,
		TArray<FVector2D>& OutWorldXY);

	/** Inverse of BuildWorldPolygonFromLocalPolylineCm — world XY cm → actor-local cm polyline. */
	void BuildLocalPolylineFromWorldPolygonCm(
		const FVector2D& CenterWorldCm,
		float YawDegrees,
		const TArray<FVector2D>& WorldPolylineXY,
		TArray<FVector2D>& OutLocalPolylineCm);

	/** Insert points along each closed-loop edge so segment length <= MaxSegmentLen (any unit). */
	void DensifyClosedPolylineForStroke(
		const TArray<FVector2D>& InPoints,
		float MaxSegmentLen,
		TArray<FVector2D>& OutPoints,
		int32 MaxOutputVerts = 0);

	/** Uniform index sample on a closed loop (safe for large N — uses int64 indexing). */
	void CapClosedPolylineUniformCount(
		const TArray<FVector2D>& InPoints,
		int32 MaxVertices,
		TArray<FVector2D>& OutPoints);

	/** Minimap + world overlay: decimate (if needed) then densify for continuous Slate stroke. */
	void PrepareCoastlineForMinimapDraw(
		const TArray<FVector2D>& LocalCoastline,
		float ZoomFactor,
		TArray<FVector2D>& OutStrokeCoastline);

	/** Cap then densify closed band ring for minimap stroke (SSOT consumer; no mid-ring abort). */
	void PrepareSeaRootsBandRingForMinimapDraw(
		const TArray<FVector2D>& LocalBandRing,
		TArray<FVector2D>& OutStrokeRing);

	/** PIE screen overlay — never skips decimation (projected screen pts are not authority). */
	void PrepareCoastlineForScreenOverlayDraw(
		const TArray<FVector2D>& ScreenCoastline,
		float ZoomFactor,
		TArray<FVector2D>& OutStrokeCoastline);

	/** World overlay: densify authority loop in cm + display-only hairpin removal (no bake write). */
	void PrepareCoastlineForWorldStrokeDraw(
		const TArray<FVector2D>& WorldCoastlineCm,
		float MaxSegmentWorldCm,
		TArray<FVector2D>& OutStrokeWorldCm);

	/** Remove near-180° hairpin vertices (closed loop). */
	void RemoveHairpinBacktrackVertices(
		TArray<FVector2D>& InOutPoints,
		float MaxBacktrackDot);

	/** Light Chaikin + hairpin pass for screen/world stroke corners (display only). */
	void PolishClosedPolylineCornersForStroke(TArray<FVector2D>& InOutPoints);
}
