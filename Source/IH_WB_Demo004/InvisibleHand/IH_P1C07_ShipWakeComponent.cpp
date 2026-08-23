// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_ShipWakeComponent.h"

#include "IH_P1C07_CommandableShipActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

UIH_P1C07_ShipWakeComponent::UIH_P1C07_ShipWakeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UIH_P1C07_ShipWakeComponent::BeginPlay()
{
	Super::BeginPlay();
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

	WakeMaterialParent = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Engine/EngineMaterials/DefaultTextMaterialTranslucent.DefaultTextMaterialTranslucent"));
	bWakeMaterialUsesAlpha = WakeMaterialParent != nullptr;
	if (!WakeMaterialParent)
	{
		WakeMaterialParent = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	EnsurePool();
}

void UIH_P1C07_ShipWakeComponent::EnsurePool()
{
	if (!GetOwner() || PoolMeshes.Num() > 0)
	{
		return;
	}

	PoolMeshes.Reserve(PoolSize);
	SliceAgeSec.SetNum(PoolSize);
	SliceLifetimeSec.SetNum(PoolSize);
	SliceInitialScale.SetNum(PoolSize);
	SliceIsBowLeg.SetNumZeroed(PoolSize);

	for (int32 Idx = 0; Idx < PoolSize; ++Idx)
	{
		UStaticMeshComponent* Slice = NewObject<UStaticMeshComponent>(GetOwner());
		Slice->SetMobility(EComponentMobility::Movable);
		Slice->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slice->SetCastShadow(false);
		Slice->SetHiddenInGame(true);
		Slice->SetTranslucentSortPriority(100);
		if (PlaneMesh)
		{
			Slice->SetStaticMesh(PlaneMesh);
		}
		if (WakeMaterialParent)
		{
			Slice->SetMaterial(0, WakeMaterialParent);
		}
		Slice->RegisterComponent();

		PoolMeshes.Add(Slice);
		SliceAgeSec[Idx] = 999.f;
		SliceLifetimeSec[Idx] = 1.f;
		SliceInitialScale[Idx] = FVector::OneVector;
	}
}

float UIH_P1C07_ShipWakeComponent::ResolveBeamWidthCm() const
{
	if (const AIH_P1C07_CommandableShipActor* Ship = Cast<AIH_P1C07_CommandableShipActor>(GetOwner()))
	{
		return FMath::Max(Ship->GetHullBeamWidthCm(), 200.f);
	}
	return DefaultBeamWidthCm;
}

float UIH_P1C07_ShipWakeComponent::ResolveKeelLengthCm() const
{
	if (const AIH_P1C07_CommandableShipActor* Ship = Cast<AIH_P1C07_CommandableShipActor>(GetOwner()))
	{
		return FMath::Max(Ship->GetHullLengthCm(), 500.f);
	}
	return DefaultKeelLengthCm;
}

void UIH_P1C07_ShipWakeComponent::ApplySliceAppearance(UStaticMeshComponent* Slice, float Alpha) const
{
	if (!Slice)
	{
		return;
	}

	UMaterialInstanceDynamic* Mid = Slice->CreateAndSetMaterialInstanceDynamic(0);
	if (!Mid)
	{
		return;
	}

	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	const FLinearColor FoamColor(0.98f, 1.f, 1.f, ClampedAlpha);
	Mid->SetVectorParameterValue(FName(TEXT("Color")), FoamColor);
	Mid->SetVectorParameterValue(FName(TEXT("BaseColor")), FoamColor);
	if (bWakeMaterialUsesAlpha)
	{
		Mid->SetScalarParameterValue(FName(TEXT("Opacity")), ClampedAlpha);
	}
}

bool UIH_P1C07_ShipWakeComponent::TrySpawnSlice(
	const FVector& WorldPos,
	float PlaneYawDeg,
	float WidthCm,
	float LengthCm,
	float LifetimeSec,
	float Alpha,
	bool bBowLeg)
{
	for (int32 Idx = 0; Idx < PoolMeshes.Num(); ++Idx)
	{
		if (SliceAgeSec[Idx] < SliceLifetimeSec[Idx])
		{
			continue;
		}

		UStaticMeshComponent* Mesh = PoolMeshes[Idx];
		const FVector InitialScale(WidthCm * 0.01f, LengthCm * 0.01f, 1.f);

		if (Mesh->GetAttachParent())
		{
			Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}

		Mesh->SetWorldLocation(WorldPos);
		Mesh->SetWorldRotation(FRotator(0.f, PlaneYawDeg, 0.f));
		Mesh->SetWorldScale3D(InitialScale);
		Mesh->SetHiddenInGame(false);
		SliceAgeSec[Idx] = 0.f;
		SliceLifetimeSec[Idx] = LifetimeSec;
		SliceInitialScale[Idx] = InitialScale;
		SliceIsBowLeg[Idx] = bBowLeg ? 1 : 0;
		ApplySliceAppearance(Mesh, Alpha);
		return true;
	}

	return false;
}

static float WakePlaneYawTransverseAft(float ActorYawDeg)
{
	// Stern uses ActorYaw+90 (slice length points toward bow). At the bow, flip 180° so length points aft.
	return ActorYawDeg - 90.f;
}

void UIH_P1C07_ShipWakeComponent::EmitSternWake(
	float BeamWidthCm,
	float KeelLengthCm,
	float SpeedCmPerSec,
	float SpeedFactor,
	const AIH_P1C07_CommandableShipActor* Ship)
{
	if (!Ship)
	{
		return;
	}

	const float TrailLengthCm = KeelLengthCm * TrailLengthKeelMultiplier;
	const float SliceSpacingCm = FMath::Max(KeelLengthCm * SliceSpacingKeelFraction, 180.f);
	const float LifetimeSec = FMath::Clamp(
		TrailLengthCm / FMath::Max(SpeedCmPerSec, MinSpeedToEmitCmPerSec),
		0.5f,
		8.f);

	FVector Stern = Ship->GetSternWakeEmitLocationWorld();
	Stern.Z += WaterSurfaceOffsetCm;

	const float WakeWidthCm = BeamWidthCm;
	const float WakeLengthCm = SliceSpacingCm * 1.08f;
	const float PlaneYawDeg = Ship->GetActorRotation().Yaw + 90.f;
	const float Alpha = FMath::Lerp(0.65f, 0.95f, SpeedFactor);

	TrySpawnSlice(Stern, PlaneYawDeg, WakeWidthCm, WakeLengthCm, LifetimeSec, Alpha);
}

void UIH_P1C07_ShipWakeComponent::EmitBowVWake(
	float BeamWidthCm,
	float KeelLengthCm,
	float SpeedCmPerSec,
	float SpeedFactor,
	const AIH_P1C07_CommandableShipActor* Ship)
{
	if (!Ship)
	{
		return;
	}

	const FVector HullForward = Ship->GetHullForwardWorld2D(Ship->GetActorRotation());
	const FVector HullPort = -FVector::CrossProduct(HullForward, FVector::UpVector).GetSafeNormal2D();
	const FVector Aft = -HullForward;
	const float BowTrailCm = KeelLengthCm * BowWakeTrailKeelFraction;
	const float MaxHalfWidthCm = BeamWidthCm * BowWakeWidthBeamMultiplier * 0.5f;
	const float BaseLifetimeSec = FMath::Clamp(
		BowTrailCm / FMath::Max(SpeedCmPerSec, MinSpeedToEmitCmPerSec),
		0.35f,
		8.f);
	const float Alpha = FMath::Lerp(0.65f, 0.95f, SpeedFactor);
	const float AftTransverseYawDeg = WakePlaneYawTransverseAft(Ship->GetActorRotation().Yaw);
	const float SegmentLengthCm = FMath::Max(BowTrailCm / BowWakeSegmentsPerEmit, BeamWidthCm * 0.18f);

	FVector Bow = Ship->GetBowWakeEmitLocationWorld();
	Bow.Z += WaterSurfaceOffsetCm;

	// Apex splash at the stem — transverse, length points aft (not forward).
	TrySpawnSlice(
		Bow,
		AftTransverseYawDeg,
		BeamWidthCm * 0.18f,
		BeamWidthCm * 0.12f,
		FMath::Clamp(BaseLifetimeSec * 0.2f, 0.15f, 1.f),
		Alpha,
		true);

	// V arms: place transverse slices aft of the bow, widening to 4× beam at 0.75× keel.
	for (int32 Seg = 0; Seg < BowWakeSegmentsPerEmit; ++Seg)
	{
		const float T = static_cast<float>(Seg + 1) / static_cast<float>(BowWakeSegmentsPerEmit);
		const float AftDistCm = BowTrailCm * T;
		const float HalfWidthCm = MaxHalfWidthCm * T;
		const float SliceWidthCm = BeamWidthCm * FMath::Lerp(0.22f, 0.55f, T);
		const float SegAlpha = Alpha * FMath::Lerp(0.75f, 1.f, T);
		const float SegLifetimeSec = BaseLifetimeSec * FMath::Lerp(0.45f, 1.f, 1.f - T);

		const FVector CenterAft = Bow + Aft * AftDistCm;
		const FVector PortPos = CenterAft + HullPort * HalfWidthCm;
		const FVector StbdPos = CenterAft - HullPort * HalfWidthCm;

		TrySpawnSlice(PortPos, AftTransverseYawDeg, SliceWidthCm, SegmentLengthCm, SegLifetimeSec, SegAlpha, true);
		TrySpawnSlice(StbdPos, AftTransverseYawDeg, SliceWidthCm, SegmentLengthCm, SegLifetimeSec, SegAlpha, true);
	}
}

void UIH_P1C07_ShipWakeComponent::UpdateWake(float SpeedCmPerSec, bool bEmit)
{
	if (!bEmit || SpeedCmPerSec < MinSpeedToEmitCmPerSec)
	{
		DistanceSinceLastSliceCm = 0.f;
		return;
	}

	const float BeamWidthCm = ResolveBeamWidthCm();
	const float KeelLengthCm = ResolveKeelLengthCm();
	const float SliceSpacingCm = FMath::Max(KeelLengthCm * SliceSpacingKeelFraction, 180.f);

	const float TravelCm = SpeedCmPerSec * GetWorld()->GetDeltaSeconds();
	DistanceSinceLastSliceCm += TravelCm;
	if (DistanceSinceLastSliceCm < SliceSpacingCm)
	{
		return;
	}

	DistanceSinceLastSliceCm = 0.f;
	const AIH_P1C07_CommandableShipActor* Ship = Cast<AIH_P1C07_CommandableShipActor>(GetOwner());
	if (!Ship)
	{
		return;
	}

	const float SpeedFactor = FMath::Clamp(SpeedCmPerSec / 500.f, 0.35f, 1.5f);
	EmitSternWake(BeamWidthCm, KeelLengthCm, SpeedCmPerSec, SpeedFactor, Ship);
	EmitBowVWake(BeamWidthCm, KeelLengthCm, SpeedCmPerSec, SpeedFactor, Ship);
}

void UIH_P1C07_ShipWakeComponent::TickSlices(float DeltaTime)
{
	for (int32 Idx = 0; Idx < PoolMeshes.Num(); ++Idx)
	{
		UStaticMeshComponent* Mesh = PoolMeshes[Idx];
		if (!Mesh)
		{
			continue;
		}
		if (SliceAgeSec[Idx] >= SliceLifetimeSec[Idx])
		{
			Mesh->SetHiddenInGame(true);
			continue;
		}

		SliceAgeSec[Idx] += DeltaTime;
		const float T = FMath::Clamp(SliceAgeSec[Idx] / SliceLifetimeSec[Idx], 0.f, 1.f);
		const float Fade = FMath::Square(1.f - T);

		FVector Scaled = SliceInitialScale[Idx] * FMath::Lerp(0.35f, 1.f, Fade);
		if (SliceIsBowLeg[Idx])
		{
			// Bow V legs widen toward max spread before fading out aft of the stem.
			const float SpreadGrow = FMath::Lerp(0.4f, 1.35f, FMath::Sin(T * PI * 0.5f));
			Scaled.X *= SpreadGrow;
		}
		Mesh->SetWorldScale3D(Scaled);
		ApplySliceAppearance(Mesh, 0.95f * Fade);

		if (T >= 1.f)
		{
			Mesh->SetHiddenInGame(true);
			SliceAgeSec[Idx] = SliceLifetimeSec[Idx];
		}
	}
}

void UIH_P1C07_ShipWakeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickSlices(DeltaTime);
}
