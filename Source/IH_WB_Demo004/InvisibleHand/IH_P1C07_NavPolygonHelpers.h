// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_P1C07_NavAvoidanceTypes.h"

namespace IH_P1C07NavPolygon
{
	bool PointInsidePolygon(const FVector2D& P, const TArray<FVector2D>& Vertices);

	float ClosestPointOnPolygonBoundary(
		const FVector2D& P,
		const TArray<FVector2D>& Vertices,
		FVector2D& OutClosest,
		FVector2D& OutOutwardNormal);

	bool ComputePolygonRepulsion(
		const FVector& SelfLoc,
		float SelfHullRadiusCm,
		const FIH_P1C07_NavPolygonObstacle& Obstacle,
		float DefaultClearanceMarginCm,
		float DefaultRepulsionBandCm,
		float HardStopGapCm,
		FVector& OutRepulsionWorld,
		float& OutGapCm,
		bool& OutHardStop);

	bool ClampPointOutsidePolygon(
		FVector& InOutWorldLoc,
		float HullRadiusCm,
		const FIH_P1C07_NavPolygonObstacle& Obstacle,
		float DefaultClearanceMarginCm);

	bool DoesSegmentIntersectExpandedPolygon(
		const FVector& AWorld,
		const FVector& BWorld,
		float HullRadiusCm,
		const FIH_P1C07_NavPolygonObstacle& Obstacle,
		float DefaultClearanceMarginCm);

	bool TryBuildGoAroundWaypointsForPolygon(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm,
		const FIH_P1C07_NavPolygonObstacle& Obstacle,
		float DefaultClearanceMarginCm,
		float RingScale,
		TArray<FVector>& OutWaypoints);

	bool TryBuildShoreFollowStepForPolygon(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm,
		const FIH_P1C07_NavPolygonObstacle& Obstacle,
		float DefaultClearanceMarginCm,
		float RingScale,
		FVector& OutWaypoint,
		float ExtraArcBiasRadians = 0.f);
}
