// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_CommandableShipActor.h"
#include "IH_P1C07_ShipRegistrySubsystem.h"
#include "IH_P1C07_NavAvoidanceSubsystem.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_P1C07_ShipWakeComponent.h"
#include "IH_P1C07_WaterQueryHelpers.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IHSeedIslandLibrary.h"
#include "IH_WB_Demo004GameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "BuoyancyComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AIH_P1C07_CommandableShipActor::AIH_P1C07_CommandableShipActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	PhysicsHull = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsHull"));
	SetRootComponent(PhysicsHull);
	PhysicsHull->SetMobility(EComponentMobility::Movable);
	PhysicsHull->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PhysicsHull->SetCollisionObjectType(ECC_Pawn);
	PhysicsHull->SetGenerateOverlapEvents(false);
	PhysicsHull->SetCollisionResponseToAllChannels(ECR_Ignore);
	PhysicsHull->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PhysicsHull->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	PhysicsHull->SetSimulatePhysics(false);
	PhysicsHull->SetEnableGravity(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PhysicsHull);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetSimulatePhysics(false);

	Buoyancy = CreateDefaultSubobject<UBuoyancyComponent>(TEXT("Buoyancy"));
	Buoyancy->SetCanBeActive(false);

	WakeComponent = CreateDefaultSubobject<UIH_P1C07_ShipWakeComponent>(TEXT("Wake"));
}

void AIH_P1C07_CommandableShipActor::ApplyShipAppearance(
	float InLengthScaleMultiplier,
	const FVector& NonUniformScale,
	const FLinearColor& Tint)
{
	LengthScaleMultiplier = FMath::Max(InLengthScaleMultiplier, 0.05f);
	ExtraNonUniformScale = NonUniformScale;
	HullTint = Tint;
	ConfigurePhysicsHullFromMesh();
	AlignToWaterSurface();

	if (Mesh)
	{
		if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetVectorParameterValue(FName(TEXT("BaseColor")), HullTint);
		}
	}
}

UPrimitiveComponent* AIH_P1C07_CommandableShipActor::GetShipSelectionPrimitive() const
{
	return PhysicsHull;
}

void AIH_P1C07_CommandableShipActor::SetShipSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bSelected);
		if (bSelected)
		{
			Mesh->SetCustomDepthStencilValue(252);
		}
	}
}

void AIH_P1C07_CommandableShipActor::CommandSailTo(const FVector& WorldDestination, const FRotator& InFinalApproachHeading)
{
	TrueDestinationWorld = WorldDestination;
	FinalApproachHeading = InFinalApproachHeading;
	bTransitUntilTrueDestination = true;
	SmoothedTransitPitch = 0.f;
	SmoothedTransitRoll = 0.f;
	ResetContingencyNavigationState();
	LastProgressLoc = GetActorLocation();
	NavState = EIH_P1C07_ShipNavState::Sailing;
	CurrentSpeedCmPerSec = SailSpeedCmPerSec * 0.2f;

	const float HullRadiusCm = GetNavAvoidanceRadiusCm() / 1.35f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
		{
			TryBuildGoAroundPathIfNeeded(GetActorLocation(), /*bForceReplan=*/true);
		}
	}

	if (PhysicsHull)
	{
		PhysicsHull->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PhysicsHull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		PhysicsHull->SetSimulatePhysics(false);
	}
}

void AIH_P1C07_CommandableShipActor::ReplaceSailOrder(
	const FVector& WorldDestination,
	const FRotator& InFinalApproachHeading)
{
	PlayerSailQueue.Reset();
	CommandSailTo(WorldDestination, InFinalApproachHeading);
}

void AIH_P1C07_CommandableShipActor::EnqueueSailWaypoint(
	const FVector& WorldDestination,
	const FRotator& InFinalApproachHeading)
{
	if (!bTransitUntilTrueDestination)
	{
		ReplaceSailOrder(WorldDestination, InFinalApproachHeading);
		return;
	}

	FPlayerSailWaypoint Wp;
	Wp.WorldDestination = WorldDestination;
	Wp.ApproachHeading = InFinalApproachHeading;
	PlayerSailQueue.Add(Wp);
}

void AIH_P1C07_CommandableShipActor::TryAdvancePlayerSailQueue()
{
	if (PlayerSailQueue.Num() == 0)
	{
		return;
	}

	const FPlayerSailWaypoint Next = PlayerSailQueue[0];
	PlayerSailQueue.RemoveAt(0);
	CommandSailTo(Next.WorldDestination, Next.ApproachHeading);
}

bool AIH_P1C07_CommandableShipActor::HasArrivedAtDestination() const
{
	return !bTransitUntilTrueDestination;
}

FVector AIH_P1C07_CommandableShipActor::GetShipFeetLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, ScaledMeshBottomLocalZ);
}

void AIH_P1C07_CommandableShipActor::ResolveHullMesh()
{
	if (!Mesh || Mesh->GetStaticMesh() || HullMeshOverride.IsNull())
	{
		return;
	}
	if (UStaticMesh* SM = HullMeshOverride.LoadSynchronous())
	{
		Mesh->SetStaticMesh(SM);
	}
}

float AIH_P1C07_CommandableShipActor::ComputeVisualUniformScale() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return 1.f;
	}

	const FVector RawHalf = Mesh->GetStaticMesh()->GetBoundingBox().GetExtent();
	const float RawLengthCm = 2.f * FMath::Max(RawHalf.Y, 1.f);
	return (TargetHullLengthCm * LengthScaleMultiplier) / RawLengthCm;
}

void AIH_P1C07_CommandableShipActor::GetScaledMeshVerticalMetrics(float& OutMinZ, float& OutHeight) const
{
	OutMinZ = 0.f;
	OutHeight = 0.f;
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}

	const FBox LocalBounds = Mesh->GetStaticMesh()->GetBoundingBox();
	const FVector S = Mesh->GetRelativeScale3D();
	const float Uniform = S.X;
	OutMinZ = LocalBounds.Min.Z * Uniform;
	OutHeight = (LocalBounds.Max.Z - LocalBounds.Min.Z) * Uniform;
}

void AIH_P1C07_CommandableShipActor::ConfigurePhysicsHullFromMesh()
{
	if (!PhysicsHull || !Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}

	const FBox LocalBounds = Mesh->GetStaticMesh()->GetBoundingBox();
	const FVector RawHalf = LocalBounds.GetExtent();
	const float VisualScale = ComputeVisualUniformScale();
	const FVector FinalScale = FVector(VisualScale) * ExtraNonUniformScale;
	Mesh->SetRelativeScale3D(FinalScale);

	const float MeshMinZ = LocalBounds.Min.Z * FinalScale.Z;
	const float MeshMaxZ = LocalBounds.Max.Z * FinalScale.Z;
	const float FullHalfZ = FMath::Max(75.f, 0.5f * (MeshMaxZ - MeshMinZ));

	const FVector HalfExtent(RawHalf.X * FinalScale.X, RawHalf.Y * FinalScale.Y, FullHalfZ);
	PhysicsHull->SetBoxExtent(HalfExtent, true);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -FullHalfZ - MeshMinZ));
	ScaledMeshBottomLocalZ = -FullHalfZ;
}

void AIH_P1C07_CommandableShipActor::AlignToWaterSurface()
{
	if (!PhysicsHull || !Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}

	FVector SurfaceLoc;
	FVector Normal;
	FVector Velocity;
	if (!SampleWaterAt(GetActorLocation(), SurfaceLoc, Normal, Velocity))
	{
		SurfaceLoc.Z = 0.f;
	}

	float TargetBottomZ = SurfaceLoc.Z + DefaultWaterlineOffsetZCm;
	const float TargetActorZ = TargetBottomZ - ScaledMeshBottomLocalZ;
	FVector Loc = GetActorLocation();
	Loc.Z = TargetActorZ;
	SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
}

bool AIH_P1C07_CommandableShipActor::SampleWaterAt(
	const FVector& WorldXY,
	FVector& OutSurfaceLoc,
	FVector& OutNormal,
	FVector& OutVelocity) const
{
	return IH_P1C07WaterQuery::QueryBestSurfaceInWorld(GetWorld(), WorldXY, OutSurfaceLoc, OutNormal, OutVelocity);
}

void AIH_P1C07_CommandableShipActor::BeginPlay()
{
	Super::BeginPlay();

	ResolveHullMesh();
	ConfigurePhysicsHullFromMesh();
	AlignToWaterSurface();

	if (Mesh && !HullTint.Equals(FLinearColor::White))
	{
		if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetVectorParameterValue(FName(TEXT("BaseColor")), HullTint);
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C07_ShipRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())
		{
			Registry->RegisterShip(this);
		}
		if (UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
		{
			Avoidance->RegisterParticipant(this);
		}
	}
}

void AIH_P1C07_CommandableShipActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C07_ShipRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())
		{
			Registry->UnregisterShip(this);
		}
		if (UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
		{
			Avoidance->UnregisterParticipant(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FVector AIH_P1C07_CommandableShipActor::GetHullForwardWorld2D(const FRotator& ActorRotation) const
{
	// Bow heading in world XY (mesh -Y).
	return ActorRotation.RotateVector(-FVector::YAxisVector).GetSafeNormal2D();
}

FRotator AIH_P1C07_CommandableShipActor::WorldDirectionToHullRotation(const FVector& WorldDir2D) const
{
	FRotator Rot = WorldDir2D.GetSafeNormal2D().Rotation();
	Rot.Yaw += HullMeshForwardYawOffsetDeg;
	return Rot;
}

float AIH_P1C07_CommandableShipActor::GetHullBeamWidthCm() const
{
	if (PhysicsHull)
	{
		const FVector Ext = PhysicsHull->GetScaledBoxExtent();
		return 2.f * FMath::Min(Ext.X, Ext.Y);
	}
	return 900.f;
}

float AIH_P1C07_CommandableShipActor::GetHullLengthCm() const
{
	if (PhysicsHull)
	{
		const FVector Ext = PhysicsHull->GetScaledBoxExtent();
		return 2.f * FMath::Max(Ext.X, Ext.Y);
	}
	return TargetHullLengthCm * LengthScaleMultiplier;
}

FVector AIH_P1C07_CommandableShipActor::GetSternWakeEmitLocationWorld() const
{
	const FVector HullForward = GetHullForwardWorld2D(GetActorRotation());
	const float HullLengthCm = GetHullLengthCm();
	// Actor pivot is hull-center; stern is aft on the centerline at the water surface, not at keel depth.
	FVector EmitPoint = GetActorLocation() - HullForward * (HullLengthCm * 0.46f);

	FVector SurfaceLoc;
	FVector Normal;
	FVector Velocity;
	if (SampleWaterAt(EmitPoint, SurfaceLoc, Normal, Velocity))
	{
		EmitPoint.Z = SurfaceLoc.Z;
	}
	else
	{
		// Keel sits below the surface by |DefaultWaterlineOffsetZCm| (negative offset).
		EmitPoint.Z = GetShipFeetLocation().Z - DefaultWaterlineOffsetZCm;
	}
	return EmitPoint;
}

FVector AIH_P1C07_CommandableShipActor::GetBowWakeEmitLocationWorld() const
{
	const FVector HullForward = GetHullForwardWorld2D(GetActorRotation());
	const float HullLengthCm = GetHullLengthCm();
	FVector EmitPoint = GetActorLocation() + HullForward * (HullLengthCm * 0.46f);

	FVector SurfaceLoc;
	FVector Normal;
	FVector Velocity;
	if (SampleWaterAt(EmitPoint, SurfaceLoc, Normal, Velocity))
	{
		EmitPoint.Z = SurfaceLoc.Z;
	}
	else
	{
		EmitPoint.Z = GetShipFeetLocation().Z - DefaultWaterlineOffsetZCm;
	}
	return EmitPoint;
}

EIH_P1C07_NavDomainMode AIH_P1C07_CommandableShipActor::GetNavDomainMode() const
{
	return EIH_P1C07_NavDomainMode::OpenSurface;
}

bool AIH_P1C07_CommandableShipActor::IsNavAvoidanceActive() const
{
	return bTransitUntilTrueDestination;
}

float AIH_P1C07_CommandableShipActor::GetNavAvoidanceRadiusCm() const
{
	if (PhysicsHull)
	{
		const FVector Extent = PhysicsHull->GetUnscaledBoxExtent();
		return FMath::Max(Extent.X, Extent.Y) * 1.35f;
	}
	return 2000.f;
}

FVector AIH_P1C07_CommandableShipActor::GetNavVelocity2D() const
{
	return GetHullForwardWorld2D(GetActorRotation()) * CurrentSpeedCmPerSec;
}

void AIH_P1C07_CommandableShipActor::ClearGoAroundWaypoints()
{
	GoAroundWaypointQueue.Reset();
	GoAroundReplanCooldownSec = 0.f;
	GoAroundPrevDistToActiveWpCm = MAX_FLT;
}

void AIH_P1C07_CommandableShipActor::ResetContingencyNavigationState()
{
	ClearGoAroundWaypoints();
	GoAroundStuckTimeSec = 0.f;
	LastShoreFollowWaypoint = FVector::ZeroVector;
	ShoreFollowInjectCount = 0;
}

float AIH_P1C07_CommandableShipActor::ComputeShoreFollowArcBiasRadians(float ExtraBiasRadians) const
{
	const float SideSign = ((GetUniqueID() & 1) != 0) ? 1.f : -1.f;
	return (static_cast<float>(ShoreFollowInjectCount) * FMath::DegreesToRadians(30.f) + ExtraBiasRadians) * SideSign;
}

FVector AIH_P1C07_CommandableShipActor::GetActiveNavigationTarget() const
{
	if (GoAroundWaypointQueue.Num() > 0)
	{
		return GoAroundWaypointQueue[0];
	}
	return TrueDestinationWorld;
}

void AIH_P1C07_CommandableShipActor::PruneStaleGoAroundWaypoints(const FVector& ShipLoc)
{
	const FVector ToDestDir = (TrueDestinationWorld - ShipLoc).GetSafeNormal2D();
	const float DistShipToDest = FVector::Dist2D(ShipLoc, TrueDestinationWorld);

	while (GoAroundWaypointQueue.Num() > 0)
	{
		const FVector& Wp = GoAroundWaypointQueue[0];
		const float DistToWp = FVector::Dist2D(ShipLoc, Wp);
		const float WpDistToDest = FVector::Dist2D(Wp, TrueDestinationWorld);
		const FVector ToWpDir = (Wp - ShipLoc).GetSafeNormal2D();

		const bool bWaypointBehind =
			!ToDestDir.IsNearlyZero() && !ToWpDir.IsNearlyZero()
			&& FVector::DotProduct(ToWpDir, ToDestDir) < 0.15f;
		const bool bWaypointStale = WpDistToDest > DistShipToDest + WaypointArrivalRadiusCm * 0.5f;

		if ((bWaypointBehind && DistToWp > WaypointArrivalRadiusCm * 0.35f) || bWaypointStale)
		{
			GoAroundWaypointQueue.RemoveAt(0);
			GoAroundPrevDistToActiveWpCm = MAX_FLT;
			continue;
		}
		break;
	}
}

void AIH_P1C07_CommandableShipActor::ClampShipToRealmBounds(FVector& InOutLoc) const
{
	float HalfNSCm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm * 100000.f;
	float HalfEWCm = HalfNSCm * static_cast<float>(IHInvisibleHandSpec::GoldenRatioPhi);
	if (const UWorld* World = GetWorld())
	{
		if (const UIH_WB_Demo004GameInstance* GI = World->GetGameInstance<UIH_WB_Demo004GameInstance>())
		{
			HalfNSCm = UIHSeedIslandLibrary::GetRealmHalfExtentNSCm(GI->GetRealmHalfExtentNSKm());
			HalfEWCm = UIHSeedIslandLibrary::GetRealmHalfExtentEWCm(GI->GetRealmHalfExtentEWKm());
		}
	}
	InOutLoc.X = FMath::Clamp(InOutLoc.X, -HalfEWCm, HalfEWCm);
	InOutLoc.Y = FMath::Clamp(InOutLoc.Y, -HalfNSCm, HalfNSCm);
}

void AIH_P1C07_CommandableShipActor::AdvanceGoAroundWaypointIfReached(const FVector& ShipLoc)
{
	if (GoAroundWaypointQueue.Num() == 0)
	{
		GoAroundPrevDistToActiveWpCm = MAX_FLT;
		return;
	}

	const float Dist2D = FVector::Dist2D(ShipLoc, GoAroundWaypointQueue[0]);
	const bool bGettingCloser = Dist2D < GoAroundPrevDistToActiveWpCm - 75.f;
	GoAroundPrevDistToActiveWpCm = Dist2D;

	const bool bWithinReach = Dist2D <= WaypointArrivalRadiusCm;
	const bool bStuckOnWaypoint = GoAroundStuckTimeSec > 1.f && !bGettingCloser;

	bool bDirectPathClear = false;
	if (bStuckOnWaypoint)
	{
		const float HullRadiusCm = GetNavAvoidanceRadiusCm() / 1.35f;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
			{
				bDirectPathClear = Avoidance->CanResumeDirectSailToDestination(
					ShipLoc, TrueDestinationWorld, HullRadiusCm);
			}
		}
	}

	if (bStuckOnWaypoint && bDirectPathClear)
	{
		ResetContingencyNavigationState();
		return;
	}

	const bool bOvershotOrUnreachable =
		Dist2D > WaypointArrivalRadiusCm * 1.5f && GoAroundStuckTimeSec > 0.75f;

	const bool bPopWaypoint =
		(bWithinReach && (bGettingCloser || GoAroundStuckTimeSec < 0.75f))
		|| (bStuckOnWaypoint && (GoAroundStuckTimeSec > 1.25f || bOvershotOrUnreachable))
		|| (GoAroundStuckTimeSec > 1.75f && !bGettingCloser);

	if (bPopWaypoint)
	{
		GoAroundWaypointQueue.RemoveAt(0);
		GoAroundPrevDistToActiveWpCm = MAX_FLT;
		if (bStuckOnWaypoint)
		{
			GoAroundStuckTimeSec = 0.f;
		}
	}
}

void AIH_P1C07_CommandableShipActor::TryInjectShoreFollowStep(const FVector& ShipLoc)
{
	const float HullRadiusCm = GetNavAvoidanceRadiusCm() / 1.35f;
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>();
	if (!Avoidance)
	{
		return;
	}

	if (ShoreFollowInjectCount >= 6
		&& Avoidance->CanResumeDirectSailToDestination(ShipLoc, TrueDestinationWorld, HullRadiusCm))
	{
		ResetContingencyNavigationState();
		return;
	}

	auto TryBuildUniqueShoreStep = [&](float ExtraArcBias, FVector& OutStep) -> bool
	{
		return Avoidance->TryBuildShoreFollowStep(
			ShipLoc, TrueDestinationWorld, HullRadiusCm, OutStep,
			ComputeShoreFollowArcBiasRadians(ExtraArcBias));
	};

	FVector ShoreStep;
	if (!TryBuildUniqueShoreStep(0.f, ShoreStep))
	{
		return;
	}

	// Avoid injecting the same ring point repeatedly (causes endless orbit for fleet mates).
	const bool bDuplicateRingPoint = LastShoreFollowWaypoint != FVector::ZeroVector
		&& FVector::Dist2D(ShoreStep, LastShoreFollowWaypoint) < 4000.f;
	if (bDuplicateRingPoint)
	{
		FVector EscalatedStep;
		if (TryBuildUniqueShoreStep(FMath::DegreesToRadians(55.f), EscalatedStep)
			&& FVector::Dist2D(EscalatedStep, LastShoreFollowWaypoint) >= 4000.f)
		{
			ShoreStep = EscalatedStep;
		}
		else if (GoAroundWaypointQueue.Num() > 0)
		{
			GoAroundWaypointQueue.RemoveAt(0);
			GoAroundPrevDistToActiveWpCm = MAX_FLT;
			GoAroundStuckTimeSec = 0.f;
			ShoreFollowInjectCount++;
			return;
		}
		else if (Avoidance->CanResumeDirectSailToDestination(ShipLoc, TrueDestinationWorld, HullRadiusCm))
		{
			ResetContingencyNavigationState();
			return;
		}
		else
		{
			return;
		}
	}

	LastShoreFollowWaypoint = ShoreStep;
	ShoreFollowInjectCount++;

	GoAroundWaypointQueue.Reset();
	GoAroundWaypointQueue.Add(ShoreStep);

	// Queue a second arc point so the ship advances along the ring instead of orbiting one tangent.
	FVector NextShoreStep;
	if (TryBuildUniqueShoreStep(FMath::DegreesToRadians(40.f), NextShoreStep)
		&& FVector::Dist2D(NextShoreStep, ShoreStep) > 2500.f)
	{
		GoAroundWaypointQueue.Add(NextShoreStep);
	}

	GoAroundReplanCooldownSec = 0.35f;
	GoAroundStuckTimeSec = 0.f;
	GoAroundPrevDistToActiveWpCm = MAX_FLT;
	LastProgressLoc = ShipLoc;
}

void AIH_P1C07_CommandableShipActor::TryBuildGoAroundPathIfNeeded(
	const FVector& ShipLoc,
	bool bForceReplan)
{
	if (!bForceReplan && GoAroundReplanCooldownSec > 0.f)
	{
		return;
	}

	const float HullRadiusCm = GetNavAvoidanceRadiusCm() / 1.35f;
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>();
	if (!Avoidance)
	{
		return;
	}

	// Direct path is clear — drop stale arc waypoints and resume sailing to the original destination.
	if (GoAroundWaypointQueue.Num() > 0
		&& Avoidance->CanResumeDirectSailToDestination(ShipLoc, TrueDestinationWorld, HullRadiusCm))
	{
		ResetContingencyNavigationState();
		return;
	}

	if (!Avoidance->NeedsIslandReroute(ShipLoc, TrueDestinationWorld, HullRadiusCm))
	{
		return;
	}

	TArray<FVector> NewWaypoints;
	if (!Avoidance->TryBuildGoAroundWaypoints(ShipLoc, TrueDestinationWorld, HullRadiusCm, NewWaypoints))
	{
		TryInjectShoreFollowStep(ShipLoc);
		return;
	}

	GoAroundWaypointQueue = MoveTemp(NewWaypoints);
	GoAroundReplanCooldownSec = 0.5f;
	GoAroundStuckTimeSec = 0.f;
	GoAroundPrevDistToActiveWpCm = MAX_FLT;
	LastProgressLoc = ShipLoc;
}

bool AIH_P1C07_CommandableShipActor::CanCompleteTransitOrderAt(
	const FVector& Loc,
	UIH_P1C07_NavAvoidanceSubsystem* Avoidance,
	float HullRadiusCm) const
{
	if (FVector::Dist2D(Loc, TrueDestinationWorld) > ArrivalRadiusCm)
	{
		return false;
	}

	if (!Avoidance)
	{
		return true;
	}

	if (Avoidance->DoesSegmentIntersectExpandedObstacle(Loc, TrueDestinationWorld, HullRadiusCm))
	{
		return false;
	}

	if (Avoidance->IsWithinIslandProximity(Loc, HullRadiusCm))
	{
		if (!Avoidance->CanResumeDirectSailToDestination(Loc, TrueDestinationWorld, HullRadiusCm))
		{
			return false;
		}
		if (GoAroundStuckTimeSec > 0.5f)
		{
			return false;
		}
	}

	return true;
}

int32 AIH_P1C07_CommandableShipActor::GetNavStandOnPriority() const
{
	if (bPlayerStandOnPriority)
	{
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
			{
				return static_cast<int32>(Avoidance->PlayerStandOnPriority);
			}
		}
		return 100;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
		{
			return static_cast<int32>(Avoidance->NPCStandOnPriority);
		}
	}
	return 50;
}

void AIH_P1C07_CommandableShipActor::UpdateKinematicNavigation(float DeltaTime)
{
	if (!bTransitUntilTrueDestination)
	{
		return;
	}

	FVector Loc = GetActorLocation();
	FRotator Rot = GetActorRotation();

	if (GoAroundReplanCooldownSec > 0.f)
	{
		GoAroundReplanCooldownSec = FMath::Max(0.f, GoAroundReplanCooldownSec - DeltaTime);
	}

	AdvanceGoAroundWaypointIfReached(Loc);
	PruneStaleGoAroundWaypoints(Loc);

	const FVector PreFrameLoc = Loc;
	float WaterSurfaceZCm = PreFrameLoc.Z;
	{
		FVector SurfaceLoc;
		FVector Normal;
		FVector Velocity;
		if (SampleWaterAt(PreFrameLoc, SurfaceLoc, Normal, Velocity))
		{
			WaterSurfaceZCm = SurfaceLoc.Z;
		}
	}

	const float HullRadiusCm = FMath::Max(GetHullBeamWidthCm(), GetHullLengthCm()) * 0.5f;
	UIH_P1C07_NavAvoidanceSubsystem* Avoidance = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>();
	}

	// Resume direct sail once past the island and the true-destination leg is unobstructed.
	if (Avoidance && GoAroundWaypointQueue.Num() > 0
		&& Avoidance->CanResumeDirectSailToDestination(Loc, TrueDestinationWorld, HullRadiusCm))
	{
		ResetContingencyNavigationState();
	}

	const FVector ActiveNavTarget = GetActiveNavigationTarget();

	if (Avoidance && GoAroundWaypointQueue.Num() == 0
		&& Avoidance->DoesSegmentIntersectExpandedObstacle(Loc, TrueDestinationWorld, HullRadiusCm))
	{
		TryBuildGoAroundPathIfNeeded(Loc);
	}

	const bool bFollowingWaypoints = GoAroundWaypointQueue.Num() > 0;

	auto ApplyIslandConstraintsIfNeeded = [&](const FVector& FromLoc, FVector& InOutLoc) -> bool
	{
		if (!Avoidance)
		{
			return false;
		}

		const bool bNearFrom = Avoidance->IsWithinIslandProximity(FromLoc, HullRadiusCm);
		const bool bNearTo = Avoidance->IsWithinIslandProximity(InOutLoc, HullRadiusCm);
		const bool bPathCrossesIsland = Avoidance->DoesSegmentIntersectExpandedObstacle(
			FromLoc, InOutLoc, HullRadiusCm);
		if (!bNearFrom && !bNearTo && !bPathCrossesIsland)
		{
			return false;
		}

		bool bConstrained = false;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UIH_P1C07_IslandCollisionSubsystem* IslandCollision = GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
			{
				if (IslandCollision->ResolveShipMovementAgainstIslands(
						this, FromLoc, InOutLoc, HullRadiusCm, WaterSurfaceZCm))
				{
					bConstrained = true;
				}
				else if ((bNearFrom || bNearTo)
					&& IslandCollision->CorrectShipPositionIfInsideIslands(
						this, InOutLoc, HullRadiusCm, WaterSurfaceZCm))
				{
					bConstrained = true;
				}
			}
		}
		return bConstrained;
	};

	const FVector ToDest = ActiveNavTarget - Loc;
	const float Dist2D = ToDest.Size2D();
	const FVector ToDestDir = ToDest.GetSafeNormal2D();
	const float StopRadiusCm = ArrivalRadiusCm;

	// Do-until: only complete the order at the true destination, never at a contingency waypoint.
	if (!bFollowingWaypoints && CanCompleteTransitOrderAt(Loc, Avoidance, HullRadiusCm))
	{
		NavState = EIH_P1C07_ShipNavState::Idle;
		bTransitUntilTrueDestination = false;
		CurrentSpeedCmPerSec = 0.f;
		Loc.X = TrueDestinationWorld.X;
		Loc.Y = TrueDestinationWorld.Y;
		ApplyIslandConstraintsIfNeeded(PreFrameLoc, Loc);

		const FVector BerthDir = FinalApproachHeading.Vector().GetSafeNormal2D();
		if (!BerthDir.IsNearlyZero())
		{
			Rot = FMath::RInterpConstantTo(Rot, WorldDirectionToHullRotation(BerthDir), DeltaTime, TurnRateDegPerSec);
		}

		FVector SurfaceLoc;
		FVector Normal;
		FVector Velocity;
		if (SampleWaterAt(Loc, SurfaceLoc, Normal, Velocity))
		{
			Loc.Z = SurfaceLoc.Z - ScaledMeshBottomLocalZ + DefaultWaterlineOffsetZCm * 0.25f;
		}

		SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
		if (WakeComponent)
		{
			WakeComponent->UpdateWake(0.f, false);
		}
		LastProgressLoc = Loc;
		GoAroundStuckTimeSec = 0.f;

		// FIFO player breadcrumbs: immediately begin next queued sail (keeps selection-independent).
		TryAdvancePlayerSailQueue();
		return;
	}

	if (!bFollowingWaypoints && FVector::Dist2D(Loc, TrueDestinationWorld) <= ArrivalRadiusCm)
	{
		NavState = EIH_P1C07_ShipNavState::Arriving;
	}
	else if (bFollowingWaypoints)
	{
		NavState = EIH_P1C07_ShipNavState::Sailing;
	}

	FVector SteerDir = ToDestDir;
	float AvoidSpeedScale = 1.f;
	if (!ToDestDir.IsNearlyZero() && Avoidance)
	{
		FIH_P1C07_NavIntent Intent;
		Intent.DesiredDirection2D = ToDestDir;
		Intent.DesiredSpeedCmPerSec = SailSpeedCmPerSec;
		FIH_P1C07_NavAvoidanceResult AvoidResult;
		if (Avoidance->ComputeAvoidance(this, Intent, AvoidResult, /*bIncludeStaticObstacleAvoidance=*/false))
		{
			if (!AvoidResult.SteerDirection2D.IsNearlyZero())
			{
				SteerDir = AvoidResult.SteerDirection2D;
			}
			AvoidSpeedScale = AvoidResult.SpeedScale;
			if (AvoidResult.bHardStop)
			{
				const float HardStopFloor = Dist2D <= ArrivalRadiusCm ? 0.12f : 0.25f;
				AvoidSpeedScale = FMath::Max(AvoidSpeedScale, HardStopFloor);
			}
		}
	}

	if (bFollowingWaypoints)
	{
		// Go-around legs run at full speed unless another ship is in the congested envelope.
		AvoidSpeedScale = FMath::Max(AvoidSpeedScale, 0.85f);
	}

	if (!SteerDir.IsNearlyZero())
	{
		const FRotator DesiredRot = WorldDirectionToHullRotation(SteerDir);
		Rot = FMath::RInterpConstantTo(Rot, DesiredRot, DeltaTime, TurnRateDegPerSec);
	}

	float SpeedScale = AvoidSpeedScale;
	if (NavState == EIH_P1C07_ShipNavState::Arriving && !bFollowingWaypoints)
	{
		SpeedScale *= FMath::Clamp(Dist2D / ArrivalRadiusCm, 0.15f, 1.f);
	}

	const FVector Forward = GetHullForwardWorld2D(Rot);
	CurrentSpeedCmPerSec = SailSpeedCmPerSec * SpeedScale;
	const float StepCm = CurrentSpeedCmPerSec * DeltaTime;

	// Go-around legs only: slide toward steer target (open-ocean uses hull-forward for stable heading).
	const bool bUseDirectSteerMove = bFollowingWaypoints;
	const FVector MoveDir = bUseDirectSteerMove && !SteerDir.IsNearlyZero() ? SteerDir : Forward;

	if (StepCm >= Dist2D)
	{
		Loc.X = ActiveNavTarget.X;
		Loc.Y = ActiveNavTarget.Y;
	}
	else if (!MoveDir.IsNearlyZero())
	{
		Loc += MoveDir * StepCm;
	}

	const bool bIslandConstrainedThisFrame = ApplyIslandConstraintsIfNeeded(PreFrameLoc, Loc);

	const float MovedCm = FVector::Dist2D(Loc, PreFrameLoc);
	if (MovedCm > 100.f)
	{
		const float PrevDistToTrueDest = FVector::Dist2D(LastProgressLoc, TrueDestinationWorld);
		const float CurDistToTrueDest = FVector::Dist2D(Loc, TrueDestinationWorld);
		LastProgressLoc = Loc;
		GoAroundStuckTimeSec = 0.f;
		if (CurDistToTrueDest < PrevDistToTrueDest - 50.f)
		{
			ShoreFollowInjectCount = FMath::Max(0, ShoreFollowInjectCount - 1);
		}
	}
	else
	{
		GoAroundStuckTimeSec += DeltaTime;
	}

	if (bIslandConstrainedThisFrame && MovedCm < FMath::Max(25.f, StepCm * 0.15f))
	{
		TryBuildGoAroundPathIfNeeded(Loc, /*bForceReplan=*/true);
	}
	else if (GoAroundStuckTimeSec > 1.5f && Avoidance)
	{
		const bool bPathBlocked = Avoidance->DoesSegmentIntersectExpandedObstacle(
			Loc, TrueDestinationWorld, HullRadiusCm);

		if (bPathBlocked)
		{
			TryBuildGoAroundPathIfNeeded(Loc, /*bForceReplan=*/true);
			if (GoAroundWaypointQueue.Num() == 0)
			{
				TryInjectShoreFollowStep(Loc);
			}
		}
	}

	AdvanceGoAroundWaypointIfReached(Loc);
	PruneStaleGoAroundWaypoints(Loc);

	ClampShipToRealmBounds(Loc);

	FVector SurfaceLoc;
	FVector Normal;
	FVector Velocity;
	if (SampleWaterAt(Loc, SurfaceLoc, Normal, Velocity))
	{
		float MeshMinZ = 0.f;
		float MeshHeight = 0.f;
		GetScaledMeshVerticalMetrics(MeshMinZ, MeshHeight);
		Loc.Z = SurfaceLoc.Z - ScaledMeshBottomLocalZ + DefaultWaterlineOffsetZCm * 0.25f;

		const float Chop = FMath::Clamp(Velocity.Size() * 0.02f, 0.f, 1.f);
		const float TargetPitch =
			FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Normal.X, -1.f, 1.f))) * 0.05f * Chop;
		const float TargetRoll =
			FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Normal.Y, -1.f, 1.f))) * 0.05f * Chop;
		SmoothedTransitPitch = FMath::FInterpTo(SmoothedTransitPitch, TargetPitch, DeltaTime, 2.5f);
		SmoothedTransitRoll = FMath::FInterpTo(SmoothedTransitRoll, TargetRoll, DeltaTime, 2.5f);
		const float SavedYaw = Rot.Yaw;
		Rot.Pitch = SmoothedTransitPitch;
		Rot.Roll = SmoothedTransitRoll;
		Rot.Yaw = SavedYaw;
	}

	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);

	if (WakeComponent)
	{
		WakeComponent->UpdateWake(CurrentSpeedCmPerSec, CurrentSpeedCmPerSec > 80.f);
	}
}

void AIH_P1C07_CommandableShipActor::UpdateIdleWaveBob(float DeltaTime)
{
	FVector SurfaceLoc;
	FVector Normal;
	FVector Velocity;
	FVector Loc = GetActorLocation();
	if (!SampleWaterAt(Loc, SurfaceLoc, Normal, Velocity))
	{
		return;
	}

	float MeshMinZ = 0.f;
	float MeshHeight = 0.f;
	GetScaledMeshVerticalMetrics(MeshMinZ, MeshHeight);
	Loc.Z = SurfaceLoc.Z - ScaledMeshBottomLocalZ + DefaultWaterlineOffsetZCm;

	const float Chop = FMath::Clamp(Velocity.Size() * 0.015f, 0.f, 1.f);
	BobPhase += DeltaTime * FMath::Lerp(0.4f, 1.6f, Chop);
	const float Bob = FMath::Sin(BobPhase) * 12.f * Chop;
	Loc.Z += Bob;

	FRotator Rot = GetActorRotation();
	Rot.Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Normal.X, -1.f, 1.f))) * 0.18f * Chop;
	Rot.Roll = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Normal.Y, -1.f, 1.f))) * 0.18f * Chop;

	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentSpeedCmPerSec = 0.f;

	if (WakeComponent)
	{
		WakeComponent->UpdateWake(0.f, false);
	}
}

void AIH_P1C07_CommandableShipActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bTransitUntilTrueDestination)
	{
		UpdateKinematicNavigation(DeltaTime);
	}
	else
	{
		UpdateIdleWaveBob(DeltaTime);
	}
}
