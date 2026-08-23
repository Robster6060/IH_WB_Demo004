// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_NavPolygonHelpers.h"

namespace
{
	static FVector2D ToXY(const FVector& V)
	{
		return FVector2D(V.X, V.Y);
	}

	static FVector FromXY(const FVector2D& V, float Z)
	{
		return FVector(V.X, V.Y, Z);
	}

	static TArray<FVector2D> ExpandPolygonOutward(const TArray<FVector2D>& In, float ExpandCm)
	{
		if (In.Num() < 3 || ExpandCm <= KINDA_SMALL_NUMBER)
		{
			return In;
		}
		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& V : In)
		{
			Centroid += V;
		}
		Centroid /= static_cast<float>(In.Num());

		TArray<FVector2D> Out;
		Out.Reserve(In.Num());
		for (const FVector2D& V : In)
		{
			const FVector2D Dir = (V - Centroid).GetSafeNormal();
			Out.Add(V + Dir * ExpandCm);
		}
		return Out;
	}
}

bool IH_P1C07NavPolygon::PointInsidePolygon(const FVector2D& P, const TArray<FVector2D>& Vertices)
{
	if (Vertices.Num() < 3)
	{
		return false;
	}
	bool bInside = false;
	const int32 N = Vertices.Num();
	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		const FVector2D& Vi = Vertices[i];
		const FVector2D& Vj = Vertices[j];
		if (((Vi.Y > P.Y) != (Vj.Y > P.Y))
			&& (P.X < (Vj.X - Vi.X) * (P.Y - Vi.Y) / FMath::Max(Vj.Y - Vi.Y, KINDA_SMALL_NUMBER) + Vi.X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

float IH_P1C07NavPolygon::ClosestPointOnPolygonBoundary(
	const FVector2D& P,
	const TArray<FVector2D>& Vertices,
	FVector2D& OutClosest,
	FVector2D& OutOutwardNormal)
{
	OutClosest = P;
	OutOutwardNormal = FVector2D(1.f, 0.f);
	if (Vertices.Num() < 2)
	{
		return MAX_FLT;
	}

	float BestDistSq = MAX_FLT;
	const int32 N = Vertices.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D A = Vertices[i];
		const FVector2D B = Vertices[(i + 1) % N];
		const FVector2D AB = B - A;
		const float ABLenSq = AB.SizeSquared();
		float T = 0.f;
		if (ABLenSq > KINDA_SMALL_NUMBER)
		{
			T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / ABLenSq, 0.f, 1.f);
		}
		const FVector2D Closest = A + AB * T;
		const float DistSq = (P - Closest).SizeSquared();
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutClosest = Closest;
			FVector2D EdgeNormal(-AB.Y, AB.X);
			if (!EdgeNormal.IsNearlyZero())
			{
				EdgeNormal.Normalize();
				if (FVector2D::DotProduct(P - Closest, EdgeNormal) < 0.f)
				{
					EdgeNormal *= -1.f;
				}
				OutOutwardNormal = EdgeNormal;
			}
		}
	}
	return FMath::Sqrt(BestDistSq);
}

bool IH_P1C07NavPolygon::ComputePolygonRepulsion(
	const FVector& SelfLoc,
	float SelfHullRadiusCm,
	const FIH_P1C07_NavPolygonObstacle& Obstacle,
	float DefaultClearanceMarginCm,
	float DefaultRepulsionBandCm,
	float HardStopGapCm,
	FVector& OutRepulsionWorld,
	float& OutGapCm,
	bool& OutHardStop)
{
	OutRepulsionWorld = FVector::ZeroVector;
	OutGapCm = MAX_FLT;
	OutHardStop = false;

	if (Obstacle.WorldVertices.Num() < 3)
	{
		return false;
	}

	const FVector2D SelfXY = ToXY(SelfLoc);
	FVector2D Closest;
	FVector2D OutwardNormal;
	const float DistToBoundaryCm = ClosestPointOnPolygonBoundary(
		SelfXY, Obstacle.WorldVertices, Closest, OutwardNormal);

	const float ClearanceMarginCm = Obstacle.ClearanceMarginCm > 0.f
		? Obstacle.ClearanceMarginCm
		: DefaultClearanceMarginCm;
	const float RepulsionBandCm = Obstacle.RepulsionBandCm > 0.f
		? Obstacle.RepulsionBandCm
		: DefaultRepulsionBandCm;
	const float StandoffCm = SelfHullRadiusCm + ClearanceMarginCm;

	const bool bInside = PointInsidePolygon(SelfXY, Obstacle.WorldVertices);
	const float SignedDistCm = bInside ? -DistToBoundaryCm : DistToBoundaryCm;
	const float GapCm = SignedDistCm - StandoffCm;
	OutGapCm = GapCm;

	if (GapCm >= RepulsionBandCm)
	{
		return false;
	}

	const float DeficitCm = StandoffCm - SignedDistCm;
	const float Push = FMath::Clamp(DeficitCm / FMath::Max(RepulsionBandCm, 1.f), 0.f, 1.f);
	OutRepulsionWorld = FromXY(OutwardNormal, 0.f).GetSafeNormal2D() * Push;

	if (GapCm <= -HardStopGapCm)
	{
		OutHardStop = true;
	}

	return !OutRepulsionWorld.IsNearlyZero();
}

bool IH_P1C07NavPolygon::ClampPointOutsidePolygon(
	FVector& InOutWorldLoc,
	float HullRadiusCm,
	const FIH_P1C07_NavPolygonObstacle& Obstacle,
	float DefaultClearanceMarginCm)
{
	if (Obstacle.WorldVertices.Num() < 3)
	{
		return false;
	}

	const float ClearanceMarginCm = Obstacle.ClearanceMarginCm > 0.f
		? Obstacle.ClearanceMarginCm
		: DefaultClearanceMarginCm;
	const float StandoffCm = HullRadiusCm + ClearanceMarginCm;

	const FVector2D SelfXY = ToXY(InOutWorldLoc);
	FVector2D Closest;
	FVector2D OutwardNormal;
	const float DistCm = ClosestPointOnPolygonBoundary(
		SelfXY, Obstacle.WorldVertices, Closest, OutwardNormal);
	const bool bInside = PointInsidePolygon(SelfXY, Obstacle.WorldVertices);
	const float SignedDistCm = bInside ? -DistCm : DistCm;
	if (SignedDistCm >= StandoffCm)
	{
		return false;
	}

	const FVector2D Target = Closest + OutwardNormal * StandoffCm;
	InOutWorldLoc.X = Target.X;
	InOutWorldLoc.Y = Target.Y;
	return true;
}

bool IH_P1C07NavPolygon::DoesSegmentIntersectExpandedPolygon(
	const FVector& AWorld,
	const FVector& BWorld,
	float HullRadiusCm,
	const FIH_P1C07_NavPolygonObstacle& Obstacle,
	float DefaultClearanceMarginCm)
{
	if (Obstacle.WorldVertices.Num() < 3)
	{
		return false;
	}

	const float ClearanceMarginCm = Obstacle.ClearanceMarginCm > 0.f
		? Obstacle.ClearanceMarginCm
		: DefaultClearanceMarginCm;
	const TArray<FVector2D> Expanded = ExpandPolygonOutward(
		Obstacle.WorldVertices, HullRadiusCm + ClearanceMarginCm);

	const FVector2D A = ToXY(AWorld);
	const FVector2D B = ToXY(BWorld);
	if (PointInsidePolygon(A, Expanded) || PointInsidePolygon(B, Expanded))
	{
		return true;
	}

	const int32 N = Expanded.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D E0 = Expanded[i];
		const FVector2D E1 = Expanded[(i + 1) % N];
		const FVector2D S1 = B - A;
		const FVector2D S2 = E1 - E0;
		const float Denom = S1.X * S2.Y - S1.Y * S2.X;
		if (FMath::Abs(Denom) <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector2D D = E0 - A;
		const float T = (D.X * S2.Y - D.Y * S2.X) / Denom;
		const float U = (D.X * S1.Y - D.Y * S1.X) / Denom;
		if (T >= 0.f && T <= 1.f && U >= 0.f && U <= 1.f)
		{
			return true;
		}
	}
	return false;
}

bool IH_P1C07NavPolygon::TryBuildGoAroundWaypointsForPolygon(
	const FVector& ShipPos,
	const FVector& DestPos,
	float HullRadiusCm,
	const FIH_P1C07_NavPolygonObstacle& Obstacle,
	float DefaultClearanceMarginCm,
	float RingScale,
	TArray<FVector>& OutWaypoints)
{
	OutWaypoints.Reset();
	if (Obstacle.WorldVertices.Num() < 3)
	{
		return false;
	}

	const float ClearanceMarginCm = Obstacle.ClearanceMarginCm > 0.f
		? Obstacle.ClearanceMarginCm
		: DefaultClearanceMarginCm;
	const TArray<FVector2D> Ring = ExpandPolygonOutward(
		Obstacle.WorldVertices, (HullRadiusCm + ClearanceMarginCm) * RingScale);

	FVector2D Centroid = FVector2D::ZeroVector;
	for (const FVector2D& V : Ring)
	{
		Centroid += V;
	}
	Centroid /= static_cast<float>(Ring.Num());

	const auto AngleFromCentroid = [&](const FVector& W) {
		const FVector2D D = ToXY(W) - Centroid;
		return FMath::Atan2(D.Y, D.X);
	};

	const float ShipTheta = AngleFromCentroid(ShipPos);
	const float DestTheta = AngleFromCentroid(DestPos);
	float Delta = FMath::UnwindRadians(DestTheta - ShipTheta);
	if (FMath::Abs(Delta) > PI)
	{
		Delta = Delta > 0.f ? Delta - 2.f * PI : Delta + 2.f * PI;
	}

	const int32 N = Ring.Num();
	const int32 Step = Delta >= 0.f ? 1 : -1;
	int32 StartIdx = 0;
	float BestAngleDiff = MAX_FLT;
	for (int32 i = 0; i < N; ++i)
	{
		const float VTheta = FMath::Atan2(Ring[i].Y - Centroid.Y, Ring[i].X - Centroid.X);
		const float Diff = FMath::Abs(FMath::UnwindRadians(VTheta - ShipTheta));
		if (Diff < BestAngleDiff)
		{
			BestAngleDiff = Diff;
			StartIdx = i;
		}
	}

	const int32 WaypointCount = FMath::Clamp(FMath::CeilToInt(FMath::Abs(Delta) / (PI / 4.f)), 1, 2);
	for (int32 W = 1; W <= WaypointCount; ++W)
	{
		const int32 Idx = (StartIdx + Step * W * (N / (WaypointCount + 1)) + N * 8) % N;
		OutWaypoints.Add(FromXY(Ring[Idx], ShipPos.Z));
	}
	return OutWaypoints.Num() > 0;
}

bool IH_P1C07NavPolygon::TryBuildShoreFollowStepForPolygon(
	const FVector& ShipPos,
	const FVector& DestPos,
	float HullRadiusCm,
	const FIH_P1C07_NavPolygonObstacle& Obstacle,
	float DefaultClearanceMarginCm,
	float RingScale,
	FVector& OutWaypoint,
	float ExtraArcBiasRadians)
{
	TArray<FVector> Wps;
	if (!TryBuildGoAroundWaypointsForPolygon(
			ShipPos, DestPos, HullRadiusCm, Obstacle, DefaultClearanceMarginCm, RingScale, Wps))
	{
		return false;
	}
	OutWaypoint = Wps[0];
	(void)ExtraArcBiasRadians;
	return true;
}
