// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "IH_WB_Demo004.h"
#include "IHInvisibleHandDesignSpec.h"

const FName UIH_P1C07_IslandCollisionSubsystem::IslandActorTag(TEXT("IH_Island"));

/** Surface ships ignore submerged sea-roots below WWF floor (cm). WWF shelf itself remains hittable. */
static float IslandSurfaceBlockMinImpactZCm()
{
	return IHInvisibleHandSpec::ShelfFloorMeters * 100.f - 50.f;
}

namespace
{
	using FIslandCompList = TArray<TWeakObjectPtr<UPrimitiveComponent>>;
	using FIslandCompMap = TMap<TWeakObjectPtr<AActor>, FIslandCompList>;

	bool IsRegisteredIslandHit(
		const FHitResult& Hit,
		const FIslandCompMap& RegisteredIslands)
	{
		const AActor* HitActor = Hit.GetActor();
		const UPrimitiveComponent* HitComp = Hit.GetComponent();
		if (!HitActor || !HitComp || !HitActor->ActorHasTag(UIH_P1C07_IslandCollisionSubsystem::IslandActorTag))
		{
			return false;
		}

		if (Hit.ImpactPoint.Z < IslandSurfaceBlockMinImpactZCm())
		{
			return false;
		}

		const FIslandCompList* Comps = RegisteredIslands.Find(HitActor);
		if (!Comps)
		{
			return false;
		}
		for (const TWeakObjectPtr<UPrimitiveComponent>& CompPtr : *Comps)
		{
			if (CompPtr.Get() == HitComp)
			{
				return true;
			}
		}
		return false;
	}

	bool SweepIslandComponentsXY(
		const UWorld* World,
		const FIslandCompMap& RegisteredIslands,
		const AActor* ShipActor,
		const FVector& From,
		const FVector& To,
		float SweepRadius,
		FHitResult& OutBestHit)
	{
		bool bAnyHit = false;
		float BestTime = 1.f;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(IHIslandShipSweep), false, ShipActor);
		Params.bTraceComplex = true;

		const FCollisionShape Sphere = FCollisionShape::MakeSphere(SweepRadius);

		for (const TPair<TWeakObjectPtr<AActor>, FIslandCompList>& Pair : RegisteredIslands)
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& CompPtr : Pair.Value)
			{
				UPrimitiveComponent* IslandComp = CompPtr.Get();
				if (!IslandComp || !IslandComp->IsCollisionEnabled())
				{
					continue;
				}

				FHitResult Hit;
				if (!IslandComp->SweepComponent(Hit, From, To, FQuat::Identity, Sphere, true))
				{
					continue;
				}

				if (!IsRegisteredIslandHit(Hit, RegisteredIslands))
				{
					continue;
				}

				if (!bAnyHit || Hit.Time < BestTime)
				{
					OutBestHit = Hit;
					BestTime = Hit.Time;
					bAnyHit = true;
				}
			}
		}

		return bAnyHit;
	}

	bool OverlapIslandComponentsXY(
		const FIslandCompMap& RegisteredIslands,
		const AActor* ShipActor,
		const FVector& At,
		float OverlapRadius,
		FHitResult& OutBestHit)
	{
		bool bAnyOverlap = false;
		float BestPenDepth = -MAX_FLT;

		const FCollisionShape Sphere = FCollisionShape::MakeSphere(OverlapRadius);

		for (const TPair<TWeakObjectPtr<AActor>, FIslandCompList>& Pair : RegisteredIslands)
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& CompPtr : Pair.Value)
			{
				UPrimitiveComponent* IslandComp = CompPtr.Get();
				if (!IslandComp || !IslandComp->IsCollisionEnabled())
				{
					continue;
				}

				FHitResult Hit;
				if (!IslandComp->SweepComponent(Hit, At, At, FQuat::Identity, Sphere, true))
				{
					continue;
				}

				if (!Hit.bStartPenetrating || !IsRegisteredIslandHit(Hit, RegisteredIslands))
				{
					continue;
				}

				if (Hit.ImpactPoint.Z < IslandSurfaceBlockMinImpactZCm())
				{
					continue;
				}

				const float PenDepth = FMath::Max(Hit.PenetrationDepth, OverlapRadius * 0.05f);
				if (!bAnyOverlap || PenDepth > BestPenDepth)
				{
					OutBestHit = Hit;
					BestPenDepth = PenDepth;
					bAnyOverlap = true;
				}
			}
		}

		return bAnyOverlap;
	}
}

void UIH_P1C07_IslandCollisionSubsystem::RegisterIslandCollision(
	AActor* IslandActor,
	UPrimitiveComponent* CollisionMesh)
{
	if (!IslandActor || !CollisionMesh)
	{
		return;
	}

	IslandActor->Tags.AddUnique(IslandActorTag);
	FIslandCompList& Comps = RegisteredIslands.FindOrAdd(IslandActor);
	for (const TWeakObjectPtr<UPrimitiveComponent>& Existing : Comps)
	{
		if (Existing.Get() == CollisionMesh)
		{
			return;
		}
	}
	Comps.Add(CollisionMesh);
}

void UIH_P1C07_IslandCollisionSubsystem::UnregisterIslandCollision(AActor* IslandActor)
{
	if (!IslandActor)
	{
		return;
	}

	RegisteredIslands.Remove(IslandActor);
}

bool UIH_P1C07_IslandCollisionSubsystem::ResolveSingleSweepStepAgainstIslands(
	const AActor* ShipActor,
	const FVector& CurrentLoc,
	FVector& InOutProposedLoc,
	float HullRadiusCm,
	float WaterSurfaceZCm) const
{
	const UWorld* World = GetWorld();
	if (!World || HullRadiusCm <= KINDA_SMALL_NUMBER || RegisteredIslands.Num() == 0)
	{
		return false;
	}

	FVector Delta = InOutProposedLoc - CurrentLoc;
	Delta.Z = 0.f;
	const float MoveDist = Delta.Size2D();
	if (MoveDist <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector MoveDir = Delta / MoveDist;
	const float SweepRadius = FMath::Max(HullRadiusCm, 25.f);

	const float SurfaceZ = WaterSurfaceZCm;
	const FVector SweepFrom(CurrentLoc.X, CurrentLoc.Y, SurfaceZ);
	const FVector SweepTo(InOutProposedLoc.X, InOutProposedLoc.Y, SurfaceZ);

	FHitResult Hit;
	if (!SweepIslandComponentsXY(World, RegisteredIslands, ShipActor, SweepFrom, SweepTo, SweepRadius, Hit))
	{
		return false;
	}

	const float SkinAlongMove = FMath::Min(ShipIslandSkinWidthCm / MoveDist, FMath::Max(0.f, Hit.Time - KINDA_SMALL_NUMBER));
	const float SafeFraction = FMath::Clamp(Hit.Time - SkinAlongMove, 0.f, 1.f);
	FVector SafeLoc = CurrentLoc + MoveDir * (MoveDist * SafeFraction);

	const float MovedCm = FVector::Dist2D(SafeLoc, CurrentLoc);
	if (MovedCm < 1.f)
	{
		InOutProposedLoc.X = CurrentLoc.X;
		InOutProposedLoc.Y = CurrentLoc.Y;
		return true;
	}

	FVector Remaining = MoveDir * (MoveDist * (1.f - SafeFraction));
	Remaining.Z = 0.f;
	const FVector NormalXY = Hit.ImpactNormal.GetSafeNormal2D();
	if (!NormalXY.IsNearlyZero() && Remaining.SizeSquared2D() > 1.f)
	{
		const float IntoNormal = FVector::DotProduct(Remaining, NormalXY);
		if (IntoNormal < 0.f)
		{
			Remaining -= NormalXY * IntoNormal;
		}

		const FVector SlideTarget = SafeLoc + Remaining;
		const FVector SlideFrom(SafeLoc.X, SafeLoc.Y, SurfaceZ);
		const FVector SlideTo(SlideTarget.X, SlideTarget.Y, SurfaceZ);

		FHitResult SlideHit;
		if (SweepIslandComponentsXY(World, RegisteredIslands, ShipActor, SlideFrom, SlideTo, SweepRadius, SlideHit))
		{
			const float SlideDist = FVector::Dist2D(SafeLoc, SlideTarget);
			const float SlideSkinAlong = FMath::Min(
				ShipIslandSkinWidthCm / FMath::Max(SlideDist, 1.f),
				FMath::Max(0.f, SlideHit.Time - KINDA_SMALL_NUMBER));
			const float SlideSafeFraction = FMath::Clamp(SlideHit.Time - SlideSkinAlong, 0.f, 1.f);
			SafeLoc = SafeLoc + (SlideTarget - SafeLoc) * SlideSafeFraction;
		}
		else
		{
			SafeLoc = SlideTarget;
		}
	}

	InOutProposedLoc.X = SafeLoc.X;
	InOutProposedLoc.Y = SafeLoc.Y;
	return true;
}

bool UIH_P1C07_IslandCollisionSubsystem::ResolveShipMovementAgainstIslands(
	const AActor* ShipActor,
	const FVector& CurrentLoc,
	FVector& InOutProposedLoc,
	float HullRadiusCm,
	float WaterSurfaceZCm) const
{
	if (HullRadiusCm <= KINDA_SMALL_NUMBER || RegisteredIslands.Num() == 0)
	{
		return false;
	}

	FVector Delta = InOutProposedLoc - CurrentLoc;
	Delta.Z = 0.f;
	const float MoveDist = Delta.Size2D();
	if (MoveDist <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float SubStepCm = FMath::Max(MaxSweepSubStepCm, HullRadiusCm * 0.35f);
	const int32 NumSubSteps = FMath::Clamp(
		FMath::CeilToInt(MoveDist / SubStepCm), 1, 4);

	bool bAnyConstrained = false;
	FVector StepFrom = CurrentLoc;

	for (int32 StepIdx = 0; StepIdx < NumSubSteps; ++StepIdx)
	{
		const float StepAlpha1 = static_cast<float>(StepIdx + 1) / static_cast<float>(NumSubSteps);
		FVector StepTarget = CurrentLoc + Delta * StepAlpha1;

		if (ResolveSingleSweepStepAgainstIslands(
				ShipActor, StepFrom, StepTarget, HullRadiusCm, WaterSurfaceZCm))
		{
			bAnyConstrained = true;
		}

		StepFrom = StepTarget;
		InOutProposedLoc = StepTarget;
	}

	return bAnyConstrained;
}

bool UIH_P1C07_IslandCollisionSubsystem::CorrectShipPositionIfInsideIslands(
	const AActor* ShipActor,
	FVector& InOutLoc,
	float HullRadiusCm,
	float WaterSurfaceZCm) const
{
	if (HullRadiusCm <= KINDA_SMALL_NUMBER || RegisteredIslands.Num() == 0)
	{
		return false;
	}

	const float SweepRadius = FMath::Max(HullRadiusCm, 25.f);
	const float SurfaceZ = WaterSurfaceZCm;
	FVector CorrectedLoc = InOutLoc;
	bool bCorrected = false;

	for (int32 Iter = 0; Iter < 4; ++Iter)
	{
		const FVector OverlapAt(CorrectedLoc.X, CorrectedLoc.Y, SurfaceZ);
		FHitResult OverlapHit;
		if (!OverlapIslandComponentsXY(RegisteredIslands, ShipActor, OverlapAt, SweepRadius, OverlapHit))
		{
			break;
		}

		const FVector NormalXY = OverlapHit.ImpactNormal.GetSafeNormal2D();
		if (NormalXY.IsNearlyZero())
		{
			break;
		}

		const float PushCm = FMath::Max(
			OverlapHit.PenetrationDepth + ShipIslandSkinWidthCm,
			ShipIslandSkinWidthCm);
		CorrectedLoc += NormalXY * PushCm;
		bCorrected = true;
	}

	if (bCorrected)
	{
		InOutLoc.X = CorrectedLoc.X;
		InOutLoc.Y = CorrectedLoc.Y;
	}

	return bCorrected;
}

bool UIH_P1C07_IslandCollisionSubsystem::IsDryLandAtWaterSurface(
	const FVector& WorldXY,
	float WaterSurfaceZCm,
	float ProbeRadiusCm) const
{
	if (RegisteredIslands.Num() == 0 || ProbeRadiusCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OverlapAt(WorldXY.X, WorldXY.Y, WaterSurfaceZCm);
	FHitResult OverlapHit;
	return OverlapIslandComponentsXY(RegisteredIslands, nullptr, OverlapAt, ProbeRadiusCm, OverlapHit);
}

bool UIH_P1C07_IslandCollisionSubsystem::TrySampleIslandSurfaceAtXY(
	const FVector2D& WorldXY,
	float ReferenceZ,
	float SurfaceLiftCm,
	const AActor* IgnoreActor,
	FVector& OutLocation,
	AActor** OutIslandActor) const
{
	const UWorld* World = GetWorld();
	if (!World || RegisteredIslands.Num() == 0)
	{
		return false;
	}

	static constexpr float TraceHalfHeightCm = 500000.f;
	const FVector TraceStart(WorldXY.X, WorldXY.Y, ReferenceZ + TraceHalfHeightCm);
	const FVector TraceEnd(WorldXY.X, WorldXY.Y, ReferenceZ - TraceHalfHeightCm);

	bool bFound = false;
	float BestDistance = MAX_FLT;
	FHitResult BestHit;

	for (const TPair<TWeakObjectPtr<AActor>, FIslandCompList>& Pair : RegisteredIslands)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& CompPtr : Pair.Value)
		{
			UPrimitiveComponent* IslandComp = CompPtr.Get();
			if (!IslandComp || !IslandComp->IsCollisionEnabled())
			{
				continue;
			}

			FCollisionQueryParams Params(SCENE_QUERY_STAT(IHTownGridIslandSurface), false, IgnoreActor);
			Params.bTraceComplex = true;

			FHitResult Hit;
			if (!IslandComp->LineTraceComponent(Hit, TraceStart, TraceEnd, Params))
			{
				continue;
			}

			if (!IsRegisteredIslandHit(Hit, RegisteredIslands))
			{
				continue;
			}

			if (!bFound || Hit.Distance < BestDistance)
			{
				BestHit = Hit;
				BestDistance = Hit.Distance;
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	OutLocation = BestHit.ImpactPoint + BestHit.ImpactNormal.GetSafeNormal() * SurfaceLiftCm;
	if (OutIslandActor)
	{
		*OutIslandActor = BestHit.GetActor();
	}
	return true;
}
