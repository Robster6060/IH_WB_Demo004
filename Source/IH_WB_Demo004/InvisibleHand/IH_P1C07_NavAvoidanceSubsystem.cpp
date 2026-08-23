// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_NavAvoidanceSubsystem.h"
#include "IH_P1C07_NavAvoidanceParticipant.h"
#include "IH_P1C07_NavPolygonHelpers.h"
#include "DrawDebugHelpers.h"

namespace
{
	FVector StarboardOf(const FVector& Forward2D)
	{
		const FVector Fwd = Forward2D.GetSafeNormal2D();
		if (Fwd.IsNearlyZero())
		{
			return FVector::RightVector;
		}
		return FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal2D();
	}

	bool PredictCPA(
		const FVector& SelfPos,
		const FVector& SelfVel,
		const FVector& OtherPos,
		const FVector& OtherVel,
		float HorizonSec,
		float& OutTimeSec,
		float& OutMinDistCm)
	{
		const FVector RelPos = OtherPos - SelfPos;
		const FVector RelVel = OtherVel - SelfVel;
		const float RelSpeedSq = RelVel.SizeSquared2D();
		if (RelSpeedSq < 1.f)
		{
			OutTimeSec = 0.f;
			OutMinDistCm = RelPos.Size2D();
			return OutMinDistCm > KINDA_SMALL_NUMBER;
		}

		OutTimeSec = -FVector::DotProduct(RelPos, RelVel) / RelSpeedSq;
		OutTimeSec = FMath::Clamp(OutTimeSec, 0.f, HorizonSec);
		const FVector Closest = RelPos + RelVel * OutTimeSec;
		OutMinDistCm = Closest.Size2D();
		return true;
	}
}

void UIH_P1C07_NavAvoidanceSubsystem::RegisterParticipant(AActor* Actor)
{
	if (!Actor || !Actor->Implements<UIH_P1C07_NavAvoidanceParticipant>())
	{
		return;
	}

	Participants.AddUnique(Actor);
	if (Participants.Num() > MaxAvoidingParticipants)
	{
		Participants.RemoveAt(0, Participants.Num() - MaxAvoidingParticipants);
	}
}

void UIH_P1C07_NavAvoidanceSubsystem::UnregisterParticipant(AActor* Actor)
{
	Participants.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Ptr) {
		return !Ptr.IsValid() || Ptr.Get() == Actor;
	});
}

void UIH_P1C07_NavAvoidanceSubsystem::RegisterEllipticalObstacle(
	AActor* Source,
	const FIH_P1C07_NavEllipticalObstacle& Obstacle)
{
	if (!Source || Obstacle.SemiMajorCm <= KINDA_SMALL_NUMBER || Obstacle.SemiMinorCm <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	EllipticalObstacles.Add(Source, Obstacle);
}

void UIH_P1C07_NavAvoidanceSubsystem::UnregisterEllipticalObstacle(AActor* Source)
{
	if (!Source)
	{
		return;
	}

	EllipticalObstacles.Remove(Source);
}

void UIH_P1C07_NavAvoidanceSubsystem::RegisterPolygonObstacle(
	AActor* Source,
	const FIH_P1C07_NavPolygonObstacle& Obstacle)
{
	if (!Source || Obstacle.WorldVertices.Num() < 3)
	{
		return;
	}
	PolygonObstacles.Add(Source, Obstacle);
}

void UIH_P1C07_NavAvoidanceSubsystem::UnregisterPolygonObstacle(AActor* Source)
{
	if (!Source)
	{
		return;
	}
	PolygonObstacles.Remove(Source);
}

uint32 UIH_P1C07_NavAvoidanceSubsystem::AllocateMoveOrderGroupId()
{
	return NextMoveOrderGroupId++;
}

void UIH_P1C07_NavAvoidanceSubsystem::GatherNeighbors(
	const IIH_P1C07_NavAvoidanceParticipant* Self,
	TArray<const IIH_P1C07_NavAvoidanceParticipant*>& OutNeighbors) const
{
	if (!Self)
	{
		return;
	}

	const AActor* SelfActor = Self->GetNavActor();
	if (!SelfActor)
	{
		return;
	}

	const FVector SelfLoc = SelfActor->GetActorLocation();
	const float QueryRadius = FMath::Max(SeparationRadiusCm * 1.25f, CongestedEnvelopeRadiusCm);

	for (const TWeakObjectPtr<AActor>& Ptr : Participants)
	{
		AActor* OtherActor = Ptr.Get();
		if (!OtherActor || OtherActor == SelfActor)
		{
			continue;
		}

		const IIH_P1C07_NavAvoidanceParticipant* Other =
			Cast<IIH_P1C07_NavAvoidanceParticipant>(OtherActor);
		if (!Other || !Other->IsNavAvoidanceActive())
		{
			continue;
		}

		if (Other->GetNavDomainMode() != Self->GetNavDomainMode())
		{
			continue;
		}

		const float Dist = FVector::Dist2D(SelfLoc, OtherActor->GetActorLocation());
		if (Dist <= QueryRadius)
		{
			OutNeighbors.Add(Other);
		}
	}
}

namespace
{
	void LocalXYFromWorld(
		const FVector& WorldLoc,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		float& OutLocalX,
		float& OutLocalY)
	{
		const FVector Offset = WorldLoc - Obstacle.CenterWorld;
		const float YawRad = FMath::DegreesToRadians(Obstacle.YawDegrees);
		const float CosYaw = FMath::Cos(YawRad);
		const float SinYaw = FMath::Sin(YawRad);
		OutLocalX = Offset.X * CosYaw + Offset.Y * SinYaw;
		OutLocalY = -Offset.X * SinYaw + Offset.Y * CosYaw;
	}

	FVector WorldXYFromLocal(float LocalX, float LocalY, const FIH_P1C07_NavEllipticalObstacle& Obstacle)
	{
		const float YawRad = FMath::DegreesToRadians(Obstacle.YawDegrees);
		const float CosYaw = FMath::Cos(YawRad);
		const float SinYaw = FMath::Sin(YawRad);
		const float WorldX = Obstacle.CenterWorld.X + LocalX * CosYaw - LocalY * SinYaw;
		const float WorldY = Obstacle.CenterWorld.Y + LocalX * SinYaw + LocalY * CosYaw;
		return FVector(WorldX, WorldY, Obstacle.CenterWorld.Z);
	}

	bool ComputeEllipticalRepulsion(
		const FVector& SelfLoc,
		float SelfHullRadiusCm,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
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

		const float SemiMajor = Obstacle.SemiMajorCm;
		const float SemiMinor = Obstacle.SemiMinorCm;
		if (SemiMajor <= KINDA_SMALL_NUMBER || SemiMinor <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		float LocalX = 0.f;
		float LocalY = 0.f;
		LocalXYFromWorld(SelfLoc, Obstacle, LocalX, LocalY);

		const float NormDist = FMath::Sqrt(
			FMath::Square(LocalX / SemiMajor) + FMath::Square(LocalY / SemiMinor));
		const float Theta = FMath::Atan2(LocalY / SemiMinor, LocalX / SemiMajor);
		const float BoundaryX = SemiMajor * FMath::Cos(Theta);
		const float BoundaryY = SemiMinor * FMath::Sin(Theta);

		FVector2D OutwardLocal;
		float DistToBoundaryCm = 0.f;
		if (NormDist < 1.f - KINDA_SMALL_NUMBER)
		{
			OutwardLocal = FVector2D(
				LocalX / (SemiMajor * SemiMajor),
				LocalY / (SemiMinor * SemiMinor)).GetSafeNormal();
			DistToBoundaryCm = 0.f;
		}
		else
		{
			const FVector2D ToShip(LocalX - BoundaryX, LocalY - BoundaryY);
			DistToBoundaryCm = ToShip.Size();
			if (DistToBoundaryCm <= KINDA_SMALL_NUMBER)
			{
				OutwardLocal = FVector2D(
					BoundaryX / (SemiMajor * SemiMajor),
					BoundaryY / (SemiMinor * SemiMinor)).GetSafeNormal();
			}
			else
			{
				OutwardLocal = ToShip / DistToBoundaryCm;
			}
		}

		const float ClearanceMarginCm = Obstacle.ClearanceMarginCm > 0.f
			? Obstacle.ClearanceMarginCm
			: DefaultClearanceMarginCm;
		const float RepulsionBandCm = Obstacle.RepulsionBandCm > 0.f
			? Obstacle.RepulsionBandCm
			: DefaultRepulsionBandCm;
		const float StandoffCm = SelfHullRadiusCm + ClearanceMarginCm;
		const float GapCm = DistToBoundaryCm - StandoffCm;
		OutGapCm = GapCm;

		if (GapCm >= RepulsionBandCm)
		{
			return false;
		}

		const float DeficitCm = StandoffCm - DistToBoundaryCm;
		const float Push = FMath::Clamp(DeficitCm / FMath::Max(RepulsionBandCm, 1.f), 0.f, 1.f);
		const FVector OutwardWorld = WorldXYFromLocal(
			OutwardLocal.X, OutwardLocal.Y, Obstacle) - Obstacle.CenterWorld;
		OutRepulsionWorld = OutwardWorld.GetSafeNormal2D() * Push;

		if (GapCm <= -HardStopGapCm)
		{
			OutHardStop = true;
		}

		return !OutRepulsionWorld.IsNearlyZero();
	}
}

bool UIH_P1C07_NavAvoidanceSubsystem::ComputeStaticObstacleAvoidance(
	const IIH_P1C07_NavAvoidanceParticipant* Self,
	const FVector& SteerIn,
	FVector& InOutSteer,
	float& InOutSpeedScale,
	bool& InOutHardStop) const
{
	if (!Self || (EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0))
	{
		return false;
	}

	const AActor* SelfActor = Self->GetNavActor();
	if (!SelfActor)
	{
		return false;
	}

	const FVector SelfLoc = SelfActor->GetActorLocation();
	const float SelfHullRadiusCm = Self->GetNavAvoidanceRadiusCm() / 1.35f;

	FVector RepulsionSteer = FVector::ZeroVector;
	float MinGapCm = MAX_FLT;
	bool bAnyHardStop = false;
	bool bAnyRepulsion = false;

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		FVector RepulsionWorld = FVector::ZeroVector;
		float GapCm = MAX_FLT;
		bool bHardStop = false;
		if (ComputeEllipticalRepulsion(
				SelfLoc, SelfHullRadiusCm, Pair.Value,
				StaticObstacleClearanceMarginCm, StaticObstacleRepulsionBandCm, StaticObstacleHardStopGapCm,
				RepulsionWorld, GapCm, bHardStop))
		{
			RepulsionSteer += RepulsionWorld;
			bAnyRepulsion = true;
		}

		MinGapCm = FMath::Min(MinGapCm, GapCm);
		bAnyHardStop |= bHardStop;
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle>& Pair : PolygonObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		FVector RepulsionWorld = FVector::ZeroVector;
		float GapCm = MAX_FLT;
		bool bHardStop = false;
		if (IH_P1C07NavPolygon::ComputePolygonRepulsion(
				SelfLoc, SelfHullRadiusCm, Pair.Value,
				StaticObstacleClearanceMarginCm, StaticObstacleRepulsionBandCm, StaticObstacleHardStopGapCm,
				RepulsionWorld, GapCm, bHardStop))
		{
			RepulsionSteer += RepulsionWorld;
			bAnyRepulsion = true;
		}

		MinGapCm = FMath::Min(MinGapCm, GapCm);
		bAnyHardStop |= bHardStop;
	}

	if (!bAnyRepulsion && !bAnyHardStop)
	{
		return false;
	}

	FVector Steer = InOutSteer.IsNearlyZero() ? SteerIn : InOutSteer;
	if (!RepulsionSteer.IsNearlyZero())
	{
		Steer = (Steer + RepulsionSteer.GetSafeNormal2D() * 0.75f).GetSafeNormal2D();
	}

	if (bAnyHardStop)
	{
		InOutHardStop = true;
		InOutSpeedScale = FMath::Min(InOutSpeedScale, 0.3f);
		const FVector Starboard = StarboardOf(Steer);
		Steer = (Steer * 0.15f + Starboard * 0.85f).GetSafeNormal2D();
	}
	else if (MinGapCm < StaticObstacleRepulsionBandCm)
	{
		const float T = FMath::Clamp(
			(MinGapCm + StaticObstacleHardStopGapCm)
				/ FMath::Max(StaticObstacleRepulsionBandCm + StaticObstacleHardStopGapCm, 1.f),
			0.f, 1.f);
		InOutSpeedScale = FMath::Min(InOutSpeedScale, FMath::Lerp(0.25f, 1.f, T));
	}

	if (!Steer.IsNearlyZero())
	{
		InOutSteer = Steer;
	}

	return true;
}

bool UIH_P1C07_NavAvoidanceSubsystem::HasCongestedTraffic(
	const IIH_P1C07_NavAvoidanceParticipant* Self,
	float& OutNearestNeighborDistCm) const
{
	OutNearestNeighborDistCm = MAX_FLT;

	if (!Self || !Self->IsNavAvoidanceActive())
	{
		return false;
	}

	const AActor* SelfActor = Self->GetNavActor();
	if (!SelfActor || Self->GetNavDomainMode() != EIH_P1C07_NavDomainMode::OpenSurface)
	{
		return false;
	}

	const FVector SelfLoc = SelfActor->GetActorLocation();
	const uint32 SelfGroup = Self->GetNavMoveOrderGroupId();
	bool bCongested = false;

	for (const TWeakObjectPtr<AActor>& Ptr : Participants)
	{
		AActor* OtherActor = Ptr.Get();
		if (!OtherActor || OtherActor == SelfActor)
		{
			continue;
		}

		const IIH_P1C07_NavAvoidanceParticipant* Other =
			Cast<IIH_P1C07_NavAvoidanceParticipant>(OtherActor);
		if (!Other || !Other->IsNavAvoidanceActive())
		{
			continue;
		}

		if (Other->GetNavDomainMode() != Self->GetNavDomainMode())
		{
			continue;
		}

		const float Dist = FVector::Dist2D(SelfLoc, OtherActor->GetActorLocation());
		OutNearestNeighborDistCm = FMath::Min(OutNearestNeighborDistCm, Dist);
		if (Dist <= CongestedEnvelopeRadiusCm)
		{
			const bool bSameMoveGroup =
				SelfGroup != 0 && SelfGroup == Other->GetNavMoveOrderGroupId();
			if (!bSameMoveGroup)
			{
				bCongested = true;
			}
		}
	}

	return bCongested;
}

bool UIH_P1C07_NavAvoidanceSubsystem::IsWithinIslandProximity(
	const FVector& WorldLoc,
	float HullRadiusCm) const
{
	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		FVector RepulsionWorld = FVector::ZeroVector;
		float GapCm = MAX_FLT;
		bool bHardStop = false;
		ComputeEllipticalRepulsion(
			WorldLoc, HullRadiusCm, Pair.Value,
			StaticObstacleClearanceMarginCm, IslandProximityBandCm, StaticObstacleHardStopGapCm,
			RepulsionWorld, GapCm, bHardStop);

		if (GapCm < IslandProximityBandCm)
		{
			return true;
		}
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle>& Pair : PolygonObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		FVector RepulsionWorld = FVector::ZeroVector;
		float GapCm = MAX_FLT;
		bool bHardStop = false;
		IH_P1C07NavPolygon::ComputePolygonRepulsion(
			WorldLoc, HullRadiusCm, Pair.Value,
			StaticObstacleClearanceMarginCm, IslandProximityBandCm, StaticObstacleHardStopGapCm,
			RepulsionWorld, GapCm, bHardStop);

		if (GapCm < IslandProximityBandCm)
		{
			return true;
		}
	}

	return false;
}

bool UIH_P1C07_NavAvoidanceSubsystem::ComputeAvoidance(
	const IIH_P1C07_NavAvoidanceParticipant* Self,
	const FIH_P1C07_NavIntent& Intent,
	FIH_P1C07_NavAvoidanceResult& OutResult,
	const bool bIncludeStaticObstacleAvoidance) const
{
	OutResult = FIH_P1C07_NavAvoidanceResult();
	OutResult.SteerDirection2D = Intent.DesiredDirection2D.GetSafeNormal2D();
	OutResult.SpeedScale = 1.f;

	if (!Self || !Self->IsNavAvoidanceActive())
	{
		return false;
	}

	const AActor* SelfActor = Self->GetNavActor();
	if (!SelfActor || Self->GetNavDomainMode() != EIH_P1C07_NavDomainMode::OpenSurface)
	{
		return false;
	}

	float NearestNeighborDistCm = MAX_FLT;
	if (!HasCongestedTraffic(Self, NearestNeighborDistCm))
	{
		return false;
	}

	TArray<const IIH_P1C07_NavAvoidanceParticipant*> Neighbors;
	GatherNeighbors(Self, Neighbors);

	const FVector SelfLoc = SelfActor->GetActorLocation();
	const FVector SelfVel = Self->GetNavVelocity2D();
	const uint32 SelfGroup = Self->GetNavMoveOrderGroupId();
	const int32 SelfPriority = Self->GetNavStandOnPriority();

	FVector SeparationSteer = FVector::ZeroVector;
	float MinDistForHardStopCm = MAX_FLT;
	bool bGiveWayCPA = false;
	bool bModified = false;

	if (Neighbors.Num() > 0)
	{
		for (const IIH_P1C07_NavAvoidanceParticipant* Other : Neighbors)
		{
			const AActor* OtherActor = Other->GetNavActor();
			if (!OtherActor)
			{
				continue;
			}

			const FVector OtherLoc = OtherActor->GetActorLocation();
			const FVector ToOther = OtherLoc - SelfLoc;
			const float Dist = ToOther.Size2D();

			const bool bSameMoveGroup =
				SelfGroup != 0 && SelfGroup == Other->GetNavMoveOrderGroupId();
			if (bSameMoveGroup)
			{
				// Formation spacing is handled at order time; fleet mates do not repel each other.
				continue;
			}

			MinDistForHardStopCm = FMath::Min(MinDistForHardStopCm, Dist);

			const int32 OtherPriority = Other->GetNavStandOnPriority();
			const bool bSelfGiveWay = SelfPriority < OtherPriority;
			if (bSelfGiveWay)
			{
				float Tcpa = 0.f;
				float CpaDist = 0.f;
				if (PredictCPA(
						SelfLoc, SelfVel, OtherLoc, Other->GetNavVelocity2D(), CPAHorizonSec, Tcpa, CpaDist)
					&& CpaDist < CPAMinDistanceCm)
				{
					bGiveWayCPA = true;
				}
			}
		}
	}

	FVector Steer = OutResult.SteerDirection2D;
	if (!SeparationSteer.IsNearlyZero())
	{
		Steer = (Steer + SeparationSteer.GetSafeNormal2D() * 0.55f).GetSafeNormal2D();
		bModified = true;
	}

	if (bGiveWayCPA)
	{
		const FVector Starboard = StarboardOf(Steer);
		Steer = (Steer + Starboard * 0.85f).GetSafeNormal2D();
		OutResult.SpeedScale = FMath::Min(OutResult.SpeedScale, 0.65f);
		bModified = true;
	}

	if (MinDistForHardStopCm <= CriticalRadiusCm)
	{
		OutResult.bHardStop = true;
		const float T = FMath::Clamp(MinDistForHardStopCm / FMath::Max(CriticalRadiusCm, 1.f), 0.f, 1.f);
		OutResult.SpeedScale = FMath::Min(OutResult.SpeedScale, FMath::Lerp(0.22f, 0.55f, T));
		const FVector Starboard = StarboardOf(Steer);
		Steer = (Steer * 0.25f + Starboard * 0.75f).GetSafeNormal2D();
		bModified = true;
	}
	else if (MinDistForHardStopCm <= WarningRadiusCm)
	{
		const float T = (MinDistForHardStopCm - CriticalRadiusCm)
			/ FMath::Max(WarningRadiusCm - CriticalRadiusCm, 1.f);
		OutResult.SpeedScale = FMath::Min(OutResult.SpeedScale, FMath::Lerp(0.35f, 1.f, T));
		bModified = true;
	}

	if (!Steer.IsNearlyZero())
	{
		OutResult.SteerDirection2D = Steer;
	}

	if (bIncludeStaticObstacleAvoidance
		&& ComputeStaticObstacleAvoidance(
			Self, Intent.DesiredDirection2D, OutResult.SteerDirection2D,
			OutResult.SpeedScale, OutResult.bHardStop))
	{
		bModified = true;
	}

	return bModified;
}

bool UIH_P1C07_NavAvoidanceSubsystem::ClampPointOutsideStaticObstacles(
	FVector& InOutWorldLoc,
	float HullRadiusCm,
	float ExtraClearanceMarginCm) const
{
	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	bool bModified = false;

	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle>& Pair : PolygonObstacles)
		{
			if (!Pair.Key.IsValid())
			{
				continue;
			}
			if (IH_P1C07NavPolygon::ClampPointOutsidePolygon(
					InOutWorldLoc, HullRadiusCm, Pair.Value, StaticObstacleClearanceMarginCm + ExtraClearanceMarginCm))
			{
				bModified = true;
			}
		}

		for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
		{
			if (!Pair.Key.IsValid())
			{
				continue;
			}

			const FIH_P1C07_NavEllipticalObstacle& Obstacle = Pair.Value;
			const float SemiMajor = Obstacle.SemiMajorCm;
			const float SemiMinor = Obstacle.SemiMinorCm;
			if (SemiMajor <= KINDA_SMALL_NUMBER || SemiMinor <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			float LocalX = 0.f;
			float LocalY = 0.f;
			LocalXYFromWorld(InOutWorldLoc, Obstacle, LocalX, LocalY);

			const float NormDist = FMath::Sqrt(
				FMath::Square(LocalX / SemiMajor) + FMath::Square(LocalY / SemiMinor));
			const float Theta = FMath::Atan2(LocalY / SemiMinor, LocalX / SemiMajor);
			const float BoundaryX = SemiMajor * FMath::Cos(Theta);
			const float BoundaryY = SemiMinor * FMath::Sin(Theta);

			FVector2D OutwardLocal;
			float DistToBoundaryCm = 0.f;
			if (NormDist < 1.f - KINDA_SMALL_NUMBER)
			{
				OutwardLocal = FVector2D(
					LocalX / (SemiMajor * SemiMajor),
					LocalY / (SemiMinor * SemiMinor)).GetSafeNormal();
				DistToBoundaryCm = 0.f;
			}
			else
			{
				const FVector2D ToShip(LocalX - BoundaryX, LocalY - BoundaryY);
				DistToBoundaryCm = ToShip.Size();
				if (DistToBoundaryCm <= KINDA_SMALL_NUMBER)
				{
					OutwardLocal = FVector2D(
						BoundaryX / (SemiMajor * SemiMajor),
						BoundaryY / (SemiMinor * SemiMinor)).GetSafeNormal();
				}
				else
				{
					OutwardLocal = ToShip / DistToBoundaryCm;
				}
			}

			const float ClearanceMarginCm = Obstacle.ClearanceMarginCm > 0.f
				? Obstacle.ClearanceMarginCm
				: StaticObstacleClearanceMarginCm;
			const float StandoffCm = HullRadiusCm + ClearanceMarginCm + ExtraClearanceMarginCm;
			const float GapCm = DistToBoundaryCm - StandoffCm;
			if (GapCm >= 0.f)
			{
				continue;
			}

			const FVector OutwardWorld = (
				WorldXYFromLocal(OutwardLocal.X, OutwardLocal.Y, Obstacle) - Obstacle.CenterWorld).GetSafeNormal2D();
			if (OutwardWorld.IsNearlyZero())
			{
				continue;
			}

			if (NormDist < 1.f - KINDA_SMALL_NUMBER)
			{
				const FVector BoundaryWorld = WorldXYFromLocal(BoundaryX, BoundaryY, Obstacle);
				InOutWorldLoc = BoundaryWorld + OutwardWorld * StandoffCm;
			}
			else
			{
				InOutWorldLoc += OutwardWorld * (-GapCm);
			}

			bModified = true;
		}
	}

	return bModified;
}

namespace IH_P1C07GoAroundNav
{
	void LocalXYFromWorld(
		const FVector& WorldLoc,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		float& OutLocalX,
		float& OutLocalY)
	{
		const FVector Offset = WorldLoc - Obstacle.CenterWorld;
		const float YawRad = FMath::DegreesToRadians(Obstacle.YawDegrees);
		const float CosYaw = FMath::Cos(YawRad);
		const float SinYaw = FMath::Sin(YawRad);
		OutLocalX = Offset.X * CosYaw + Offset.Y * SinYaw;
		OutLocalY = -Offset.X * SinYaw + Offset.Y * CosYaw;
	}

	FVector WorldXYFromLocal(
		float LocalX,
		float LocalY,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle)
	{
		const float YawRad = FMath::DegreesToRadians(Obstacle.YawDegrees);
		const float CosYaw = FMath::Cos(YawRad);
		const float SinYaw = FMath::Sin(YawRad);
		const float WorldX = Obstacle.CenterWorld.X + LocalX * CosYaw - LocalY * SinYaw;
		const float WorldY = Obstacle.CenterWorld.Y + LocalX * SinYaw + LocalY * CosYaw;
		return FVector(WorldX, WorldY, Obstacle.CenterWorld.Z);
	}

	float GetObstacleClearanceCm(
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		float DefaultClearanceMarginCm)
	{
		return Obstacle.ClearanceMarginCm > 0.f
			? Obstacle.ClearanceMarginCm
			: DefaultClearanceMarginCm;
	}

	void GetExpandedSemiAxes(
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		float HullRadiusCm,
		float DefaultClearanceMarginCm,
		float& OutExpandedMajorCm,
		float& OutExpandedMinorCm)
	{
		const float StandoffCm = HullRadiusCm + GetObstacleClearanceCm(Obstacle, DefaultClearanceMarginCm);
		OutExpandedMajorCm = Obstacle.SemiMajorCm + StandoffCm;
		OutExpandedMinorCm = Obstacle.SemiMinorCm + StandoffCm;
	}

	float EllipseNormSq(float LocalX, float LocalY, float SemiMajorCm, float SemiMinorCm)
	{
		if (SemiMajorCm <= KINDA_SMALL_NUMBER || SemiMinorCm <= KINDA_SMALL_NUMBER)
		{
			return MAX_FLT;
		}
		return FMath::Square(LocalX / SemiMajorCm) + FMath::Square(LocalY / SemiMinorCm);
	}

	bool SegmentIntersectsExpandedEllipse(
		const FVector& AWorld,
		const FVector& BWorld,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		float ExpandedMajorCm,
		float ExpandedMinorCm)
	{
		if (ExpandedMajorCm <= KINDA_SMALL_NUMBER || ExpandedMinorCm <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		float Ax = 0.f;
		float Ay = 0.f;
		float Bx = 0.f;
		float By = 0.f;
		LocalXYFromWorld(AWorld, Obstacle, Ax, Ay);
		LocalXYFromWorld(BWorld, Obstacle, Bx, By);

		const float NormA = EllipseNormSq(Ax, Ay, ExpandedMajorCm, ExpandedMinorCm);
		const float NormB = EllipseNormSq(Bx, By, ExpandedMajorCm, ExpandedMinorCm);
		if (NormA < 1.f - KINDA_SMALL_NUMBER || NormB < 1.f - KINDA_SMALL_NUMBER)
		{
			return true;
		}

		const float Dx = Bx - Ax;
		const float Dy = By - Ay;
		const float SegLenSq = Dx * Dx + Dy * Dy;
		if (SegLenSq <= KINDA_SMALL_NUMBER)
		{
			return NormA < 1.f + KINDA_SMALL_NUMBER;
		}

		const float Ux = Dx / ExpandedMajorCm;
		const float Uy = Dy / ExpandedMinorCm;
		const float AxN = Ax / ExpandedMajorCm;
		const float AyN = Ay / ExpandedMinorCm;

		const float AQuad = Ux * Ux + Uy * Uy;
		const float BQuad = 2.f * (AxN * Ux + AyN * Uy);
		const float CQuad = AxN * AxN + AyN * AyN - 1.f;

		if (CQuad <= 0.f)
		{
			return true;
		}

		const float Discriminant = BQuad * BQuad - 4.f * AQuad * CQuad;
		if (Discriminant < 0.f)
		{
			return false;
		}

		const float SqrtDisc = FMath::Sqrt(Discriminant);
		const float Inv2A = 0.5f / FMath::Max(AQuad, KINDA_SMALL_NUMBER);
		const float T0 = (-BQuad - SqrtDisc) * Inv2A;
		const float T1 = (-BQuad + SqrtDisc) * Inv2A;

		auto RootOnSegment = [](float T) {
			return T >= -KINDA_SMALL_NUMBER && T <= 1.f + KINDA_SMALL_NUMBER;
		};

		if (RootOnSegment(T0) || RootOnSegment(T1))
		{
			return true;
		}

		const float Mx = Ax + Dx * 0.5f;
		const float My = Ay + Dy * 0.5f;
		return EllipseNormSq(Mx, My, ExpandedMajorCm, ExpandedMinorCm) < 1.f - KINDA_SMALL_NUMBER;
	}

	FVector BoundaryWorldFromEllipseAngle(
		float ThetaRad,
		float ExpandedMajorCm,
		float ExpandedMinorCm,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle)
	{
		const float LocalX = ExpandedMajorCm * FMath::Cos(ThetaRad);
		const float LocalY = ExpandedMinorCm * FMath::Sin(ThetaRad);
		return WorldXYFromLocal(LocalX, LocalY, Obstacle);
	}

	float ShortestArcDelta(float FromTheta, float ToTheta)
	{
		float Delta = FMath::UnwindRadians(ToTheta - FromTheta);
		if (FMath::Abs(Delta) > PI)
		{
			Delta -= FMath::Sign(Delta) * TWO_PI;
		}
		return Delta;
	}

	bool ComputeGoAroundWaypointsForObstacle(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm,
		float DefaultClearanceMarginCm,
		float WaypointRingScale,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		TArray<FVector>& OutWaypoints)
	{
		OutWaypoints.Reset();

		float ExpandedMajorCm = 0.f;
		float ExpandedMinorCm = 0.f;
		GetExpandedSemiAxes(
			Obstacle, HullRadiusCm, DefaultClearanceMarginCm, ExpandedMajorCm, ExpandedMinorCm);
		if (ExpandedMajorCm <= KINDA_SMALL_NUMBER || ExpandedMinorCm <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float WaypointMajorCm = ExpandedMajorCm * WaypointRingScale;
		const float WaypointMinorCm = ExpandedMinorCm * WaypointRingScale;

		float ShipLocalX = 0.f;
		float ShipLocalY = 0.f;
		float DestLocalX = 0.f;
		float DestLocalY = 0.f;
		LocalXYFromWorld(ShipPos, Obstacle, ShipLocalX, ShipLocalY);
		LocalXYFromWorld(DestPos, Obstacle, DestLocalX, DestLocalY);

		const bool bSegmentCrosses = SegmentIntersectsExpandedEllipse(
			ShipPos, DestPos, Obstacle, ExpandedMajorCm, ExpandedMinorCm);
		const float ShipNormSq = EllipseNormSq(
			ShipLocalX, ShipLocalY, ExpandedMajorCm, ExpandedMinorCm);

		if (!bSegmentCrosses && ShipNormSq >= FMath::Square(1.f + 0.22f))
		{
			return false;
		}

		auto ProjectToBoundaryAngle = [&](float& InOutLocalX, float& InOutLocalY, float& OutTheta) -> bool
		{
			const float NormSq = EllipseNormSq(
				InOutLocalX, InOutLocalY, WaypointMajorCm, WaypointMinorCm);
			if (NormSq < 1.f - KINDA_SMALL_NUMBER)
			{
				const float Theta = FMath::Atan2(
					InOutLocalY / WaypointMinorCm, InOutLocalX / WaypointMajorCm);
				const float BoundaryX = WaypointMajorCm * FMath::Cos(Theta);
				const float BoundaryY = WaypointMinorCm * FMath::Sin(Theta);
				const FVector2D Outward(
					(InOutLocalX - BoundaryX) / (WaypointMajorCm * WaypointMajorCm),
					(InOutLocalY - BoundaryY) / (WaypointMinorCm * WaypointMinorCm));
				const FVector2D OutwardN = Outward.GetSafeNormal();
				InOutLocalX = BoundaryX + OutwardN.X * WaypointMajorCm * 0.03f;
				InOutLocalY = BoundaryY + OutwardN.Y * WaypointMinorCm * 0.03f;
			}

			OutTheta = FMath::Atan2(InOutLocalY / WaypointMinorCm, InOutLocalX / WaypointMajorCm);
			return true;
		};

		float ThetaShip = 0.f;
		float ThetaDest = 0.f;
		if (!ProjectToBoundaryAngle(ShipLocalX, ShipLocalY, ThetaShip)
			|| !ProjectToBoundaryAngle(DestLocalX, DestLocalY, ThetaDest))
		{
			return false;
		}

		float ArcDelta = ShortestArcDelta(ThetaShip, ThetaDest);
		if (!bSegmentCrosses && FMath::Abs(ArcDelta) < FMath::DegreesToRadians(12.f))
		{
			ArcDelta = FMath::Sign(ArcDelta != 0.f ? ArcDelta : 1.f) * FMath::DegreesToRadians(40.f);
		}

		const float AbsArc = FMath::Abs(ArcDelta);
		const int32 NumArcWaypoints = FMath::Clamp(
			FMath::CeilToInt(AbsArc / FMath::DegreesToRadians(28.f)), 2, 6);

		for (int32 Idx = 0; Idx < NumArcWaypoints; ++Idx)
		{
			const float Fraction = static_cast<float>(Idx + 1) / static_cast<float>(NumArcWaypoints + 1);
			const float Theta = ThetaShip + ArcDelta * Fraction;
			FVector Wp = BoundaryWorldFromEllipseAngle(
				Theta, WaypointMajorCm, WaypointMinorCm, Obstacle);
			Wp.Z = ShipPos.Z;
			OutWaypoints.Add(Wp);
		}

		{
			FVector ExitWp = BoundaryWorldFromEllipseAngle(
				ThetaDest, WaypointMajorCm, WaypointMinorCm, Obstacle);
			ExitWp.Z = DestPos.Z;
			if (OutWaypoints.Num() == 0 || FVector::Dist2D(OutWaypoints.Last(), ExitWp) > 500.f)
			{
				OutWaypoints.Add(ExitWp);
			}
		}

		return OutWaypoints.Num() > 0;
	}

	bool ComputeShoreFollowStepForObstacle(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm,
		float DefaultClearanceMarginCm,
		float WaypointRingScale,
		float ExtraArcBiasRadians,
		const FIH_P1C07_NavEllipticalObstacle& Obstacle,
		FVector& OutWaypoint)
	{
		float ExpandedMajorCm = 0.f;
		float ExpandedMinorCm = 0.f;
		GetExpandedSemiAxes(
			Obstacle, HullRadiusCm, DefaultClearanceMarginCm, ExpandedMajorCm, ExpandedMinorCm);
		if (ExpandedMajorCm <= KINDA_SMALL_NUMBER || ExpandedMinorCm <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float WaypointMajorCm = ExpandedMajorCm * WaypointRingScale;
		const float WaypointMinorCm = ExpandedMinorCm * WaypointRingScale;

		float ShipLocalX = 0.f;
		float ShipLocalY = 0.f;
		float DestLocalX = 0.f;
		float DestLocalY = 0.f;
		LocalXYFromWorld(ShipPos, Obstacle, ShipLocalX, ShipLocalY);
		LocalXYFromWorld(DestPos, Obstacle, DestLocalX, DestLocalY);

		const float ThetaShip = FMath::Atan2(ShipLocalY / WaypointMinorCm, ShipLocalX / WaypointMajorCm);
		const float ThetaDest = FMath::Atan2(DestLocalY / WaypointMinorCm, DestLocalX / WaypointMajorCm);
		float ArcDelta = ShortestArcDelta(ThetaShip, ThetaDest);
		if (FMath::Abs(ArcDelta) < FMath::DegreesToRadians(8.f))
		{
			ArcDelta = FMath::Sign(ArcDelta != 0.f ? ArcDelta : 1.f) * FMath::DegreesToRadians(35.f);
		}
		else
		{
			ArcDelta = FMath::Clamp(ArcDelta, -FMath::DegreesToRadians(55.f), FMath::DegreesToRadians(55.f));
		}

		ArcDelta += ExtraArcBiasRadians;
		ArcDelta = FMath::Clamp(ArcDelta, -FMath::DegreesToRadians(95.f), FMath::DegreesToRadians(95.f));

		const float ThetaStep = ThetaShip + ArcDelta;
		OutWaypoint = BoundaryWorldFromEllipseAngle(
			ThetaStep, WaypointMajorCm, WaypointMinorCm, Obstacle);
		OutWaypoint.Z = ShipPos.Z;
		return true;
	}
}

bool UIH_P1C07_NavAvoidanceSubsystem::DoesSegmentIntersectExpandedObstacle(
	const FVector& AWorld,
	const FVector& BWorld,
	float HullRadiusCm) const
{
	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle>& Pair : PolygonObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}
		if (IH_P1C07NavPolygon::DoesSegmentIntersectExpandedPolygon(
				AWorld, BWorld, HullRadiusCm, Pair.Value, StaticObstacleClearanceMarginCm))
		{
			return true;
		}
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		float ExpandedMajorCm = 0.f;
		float ExpandedMinorCm = 0.f;
		IH_P1C07GoAroundNav::GetExpandedSemiAxes(
			Pair.Value, HullRadiusCm, StaticObstacleClearanceMarginCm,
			ExpandedMajorCm, ExpandedMinorCm);

		if (IH_P1C07GoAroundNav::SegmentIntersectsExpandedEllipse(
				AWorld, BWorld, Pair.Value, ExpandedMajorCm, ExpandedMinorCm))
		{
			return true;
		}
	}

	return false;
}

bool UIH_P1C07_NavAvoidanceSubsystem::NeedsIslandReroute(
	const FVector& ShipPos,
	const FVector& DestPos,
	float HullRadiusCm) const
{
	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (DoesSegmentIntersectExpandedObstacle(ShipPos, DestPos, HullRadiusCm))
	{
		return true;
	}

	return false;
}

bool UIH_P1C07_NavAvoidanceSubsystem::CanResumeDirectSailToDestination(
	const FVector& ShipPos,
	const FVector& DestPos,
	float HullRadiusCm) const
{
	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	return !DoesSegmentIntersectExpandedObstacle(ShipPos, DestPos, HullRadiusCm);
}

bool UIH_P1C07_NavAvoidanceSubsystem::TryBuildShoreFollowStep(
	const FVector& ShipPos,
	const FVector& DestPos,
	float HullRadiusCm,
	FVector& OutWaypoint,
	float ExtraArcBiasRadians) const
{
	OutWaypoint = FVector::ZeroVector;

	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle>& Pair : PolygonObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		FVector CandidateWaypoint;
		if (IH_P1C07NavPolygon::TryBuildShoreFollowStepForPolygon(
				ShipPos, DestPos, HullRadiusCm, Pair.Value, StaticObstacleClearanceMarginCm,
				GoAroundWaypointRingScale, CandidateWaypoint, ExtraArcBiasRadians))
		{
			OutWaypoint = CandidateWaypoint;
			return true;
		}
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		FVector CandidateWaypoint;
		if (IH_P1C07GoAroundNav::ComputeShoreFollowStepForObstacle(
				ShipPos, DestPos, HullRadiusCm, StaticObstacleClearanceMarginCm,
				GoAroundWaypointRingScale, ExtraArcBiasRadians, Pair.Value, CandidateWaypoint))
		{
			OutWaypoint = CandidateWaypoint;
			return true;
		}
	}

	return false;
}

bool UIH_P1C07_NavAvoidanceSubsystem::TryBuildGoAroundWaypoints(
	const FVector& ShipPos,
	const FVector& DestPos,
	float HullRadiusCm,
	TArray<FVector>& OutWaypoints) const
{
	OutWaypoints.Reset();

	if ((EllipticalObstacles.Num() == 0 && PolygonObstacles.Num() == 0) || HullRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float BestPathLen = MAX_FLT;
	TArray<FVector> BestWaypoints;

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle>& Pair : PolygonObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		TArray<FVector> CandidateWaypoints;
		if (!IH_P1C07NavPolygon::TryBuildGoAroundWaypointsForPolygon(
				ShipPos, DestPos, HullRadiusCm, Pair.Value, StaticObstacleClearanceMarginCm,
				GoAroundWaypointRingScale, CandidateWaypoints))
		{
			continue;
		}

		float PathLen = FVector::Dist2D(ShipPos, CandidateWaypoints[0]);
		for (int32 Idx = 1; Idx < CandidateWaypoints.Num(); ++Idx)
		{
			PathLen += FVector::Dist2D(CandidateWaypoints[Idx - 1], CandidateWaypoints[Idx]);
		}
		PathLen += FVector::Dist2D(CandidateWaypoints.Last(), DestPos);

		if (PathLen < BestPathLen)
		{
			BestPathLen = PathLen;
			BestWaypoints = MoveTemp(CandidateWaypoints);
		}
	}

	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		TArray<FVector> CandidateWaypoints;
		if (!IH_P1C07GoAroundNav::ComputeGoAroundWaypointsForObstacle(
				ShipPos, DestPos, HullRadiusCm, StaticObstacleClearanceMarginCm,
				GoAroundWaypointRingScale, Pair.Value, CandidateWaypoints))
		{
			continue;
		}

		float PathLen = FVector::Dist2D(ShipPos, CandidateWaypoints[0]);
		for (int32 Idx = 1; Idx < CandidateWaypoints.Num(); ++Idx)
		{
			PathLen += FVector::Dist2D(CandidateWaypoints[Idx - 1], CandidateWaypoints[Idx]);
		}
		PathLen += FVector::Dist2D(CandidateWaypoints.Last(), DestPos);

		if (PathLen < BestPathLen)
		{
			BestPathLen = PathLen;
			BestWaypoints = MoveTemp(CandidateWaypoints);
		}
	}

	if (BestWaypoints.Num() == 0)
	{
		return false;
	}

	OutWaypoints = MoveTemp(BestWaypoints);
	return true;
}

namespace IH_P1C07NavDebugDraw
{
	void DrawEllipseXY(
		UWorld* World,
		const FVector& Center,
		float SemiMajorCm,
		float SemiMinorCm,
		float YawDegrees,
		FColor Color,
		float DurationSec,
		float Thickness,
		int32 Segments = 64)
	{
		if (!World || SemiMajorCm <= KINDA_SMALL_NUMBER || SemiMinorCm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float YawRad = FMath::DegreesToRadians(YawDegrees);
		const float CosYaw = FMath::Cos(YawRad);
		const float SinYaw = FMath::Sin(YawRad);
		FVector PrevPoint = FVector::ZeroVector;
		for (int32 Idx = 0; Idx <= Segments; ++Idx)
		{
			const float Theta = (static_cast<float>(Idx) / Segments) * 2.f * PI;
			const float LocalX = SemiMajorCm * FMath::Cos(Theta);
			const float LocalY = SemiMinorCm * FMath::Sin(Theta);
			const FVector Point(
				Center.X + LocalX * CosYaw - LocalY * SinYaw,
				Center.Y + LocalX * SinYaw + LocalY * CosYaw,
				Center.Z);
			if (Idx > 0)
			{
				DrawDebugLine(World, PrevPoint, Point, Color, false, DurationSec, 0, Thickness);
			}
			PrevPoint = Point;
		}
	}
}

void UIH_P1C07_NavAvoidanceSubsystem::DrawCollisionEnvelopesDebug(
	UWorld* World,
	const TArray<AActor*>& ShipsToDraw,
	FColor Color,
	float DurationSec) const
{
	if (!World)
	{
		return;
	}

	const float DrawDuration = FMath::Max(DurationSec, 0.05f);
	float ReferenceHullRadiusCm = 1500.f;
	for (AActor* Actor : ShipsToDraw)
	{
		if (!Actor)
		{
			continue;
		}

		const IIH_P1C07_NavAvoidanceParticipant* Participant =
			Cast<IIH_P1C07_NavAvoidanceParticipant>(Actor);
		if (!Participant)
		{
			continue;
		}

		const float AvoidRadiusCm = Participant->GetNavAvoidanceRadiusCm();
		const float HullRadiusCm = AvoidRadiusCm / 1.35f;
		ReferenceHullRadiusCm = FMath::Max(ReferenceHullRadiusCm, HullRadiusCm);

		const FVector Loc = Actor->GetActorLocation();
		const FVector DrawCenter(Loc.X, Loc.Y, Loc.Z);
		DrawDebugCircle(
			World, DrawCenter, HullRadiusCm, 48, Color, false, DrawDuration, 0, 8.f,
			FVector::ForwardVector, FVector::RightVector, false);
		DrawDebugCircle(
			World, DrawCenter, AvoidRadiusCm, 48, Color, false, DrawDuration, 0, 12.f,
			FVector::ForwardVector, FVector::RightVector, false);
		DrawDebugCircle(
			World, DrawCenter, CongestedEnvelopeRadiusCm, 48, FColor(255, 140, 0), false, DrawDuration, 0, 6.f,
			FVector::ForwardVector, FVector::RightVector, false);
	}

	const FColor IslandColor = Color;
	const FColor GoAroundColor(255, 120, 0);
	for (const TPair<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle>& Pair : EllipticalObstacles)
	{
		const FIH_P1C07_NavEllipticalObstacle& Obstacle = Pair.Value;
		const FVector Center = Obstacle.CenterWorld;

		IH_P1C07NavDebugDraw::DrawEllipseXY(
			World, Center, Obstacle.SemiMajorCm, Obstacle.SemiMinorCm, Obstacle.YawDegrees,
			FColor(255, 180, 80), DrawDuration, 4.f);

		float ExpandedMajorCm = 0.f;
		float ExpandedMinorCm = 0.f;
		IH_P1C07GoAroundNav::GetExpandedSemiAxes(
			Obstacle, ReferenceHullRadiusCm, StaticObstacleClearanceMarginCm,
			ExpandedMajorCm, ExpandedMinorCm);
		IH_P1C07NavDebugDraw::DrawEllipseXY(
			World, Center, ExpandedMajorCm, ExpandedMinorCm, Obstacle.YawDegrees,
			IslandColor, DrawDuration, 12.f);

		IH_P1C07NavDebugDraw::DrawEllipseXY(
			World, Center, ExpandedMajorCm * GoAroundWaypointRingScale,
			ExpandedMinorCm * GoAroundWaypointRingScale, Obstacle.YawDegrees,
			GoAroundColor, DrawDuration, 6.f);
	}
}
