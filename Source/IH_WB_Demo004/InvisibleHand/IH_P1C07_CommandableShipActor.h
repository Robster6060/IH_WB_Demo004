// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C07_SelectableShip.h"
#include "IH_P1C07_NavAvoidanceParticipant.h"
#include "IH_P1C07_CommandableShipActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UBuoyancyComponent;
class UIH_P1C07_ShipWakeComponent;
class UIH_P1C07_NavAvoidanceSubsystem;

UENUM()
enum class EIH_P1C07_ShipNavState : uint8
{
	Idle,
	Sailing,
	Arriving,
};

/**
 * Base commandable ship: kinematic sailing, Gerstner height/bob, selection, wake.
 * Subclass per hull mesh (Merchantman, future traders); reuse for varying scale/color via ApplyShipAppearance.
 */
UCLASS(Abstract, NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C07_CommandableShipActor : public AActor, public IIH_P1C07_SelectableShip, public IIH_P1C07_NavAvoidanceParticipant
{
	GENERATED_BODY()

public:
	AIH_P1C07_CommandableShipActor();

	/** Uniform length scale multiplier on top of TargetHullLengthCm fit. */
	UFUNCTION(BlueprintCallable, Category = "P1C07|Ship")
	void ApplyShipAppearance(float LengthScaleMultiplier, const FVector& NonUniformScale, const FLinearColor& HullTint);

	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship")
	TSoftObjectPtr<UStaticMesh> HullMeshOverride;

	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship")
	float TargetHullLengthCm = 3780.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P1C07|Ship")
	float DefaultWaterlineOffsetZCm = -100.f;

	/** Imported Merchantman bow is mesh -Y. Degrees added to travel heading to get actor yaw. */
	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship")
	float HullMeshForwardYawOffsetDeg = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship", meta = (ClampMin = "0.1", ClampMax = "0.6"))
	float PhysicsHullDraftFraction = 0.32f;

	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship|Navigation")
	float SailSpeedCmPerSec = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship|Navigation")
	float TurnRateDegPerSec = 32.f;

	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship|Navigation")
	float ArrivalRadiusCm = 2000.f;

	/** Intermediate go-around waypoint reach radius (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship|Navigation")
	float WaypointArrivalRadiusCm = 3500.f;

	/** Player-commanded ships use stand-on priority over NPC traffic. */
	UPROPERTY(EditDefaultsOnly, Category = "P1C07|Ship|Navigation")
	bool bPlayerStandOnPriority = true;

	// IIH_P1C07_SelectableShip
	virtual UPrimitiveComponent* GetShipSelectionPrimitive() const override;
	virtual void SetShipSelected(bool bSelected) override;
	virtual void CommandSailTo(const FVector& WorldDestination, const FRotator& FinalApproachHeading) override;
	/** Replace player path (clears queued breadcrumbs) and sail to Dest. */
	void ReplaceSailOrder(const FVector& WorldDestination, const FRotator& InFinalApproachHeading);
	/** Append breadcrumb; starts sail immediately if idle. */
	void EnqueueSailWaypoint(const FVector& WorldDestination, const FRotator& InFinalApproachHeading);
	virtual bool HasArrivedAtDestination() const override;
	virtual bool HasActiveTransitOrder() const override { return bTransitUntilTrueDestination; }
	virtual FVector GetTrueDestinationWorld() const override { return TrueDestinationWorld; }
	virtual FVector GetShipFeetLocation() const override;

	void SetMoveOrderGroupId(uint32 InGroupId) { MoveOrderGroupId = InGroupId; }

	/** World-space hull bow heading (mesh +Y) for wake and navigation. */
	FVector GetHullForwardWorld2D(const FRotator& ActorRotation) const;
	FRotator WorldDirectionToHullRotation(const FVector& WorldDir2D) const;

	/** Full width of physics hull (beam) in cm — used for wake trail length. */
	float GetHullBeamWidthCm() const;
	/** Full length of physics hull in cm — stern offset for wake emission. */
	float GetHullLengthCm() const;

	/** Stern centerline point on the water surface (keel is below this Z). */
	FVector GetSternWakeEmitLocationWorld() const;

	/** Bow centerline point on the water surface (keel entry / stem). */
	FVector GetBowWakeEmitLocationWorld() const;

	// IIH_P1C07_NavAvoidanceParticipant
	virtual AActor* GetNavActor() const override { return const_cast<AIH_P1C07_CommandableShipActor*>(this); }
	virtual EIH_P1C07_NavDomainMode GetNavDomainMode() const override;
	virtual bool IsNavAvoidanceActive() const override;
	virtual float GetNavAvoidanceRadiusCm() const override;
	virtual FVector GetNavVelocity2D() const override;
	virtual uint32 GetNavMoveOrderGroupId() const override { return MoveOrderGroupId; }
	virtual int32 GetNavStandOnPriority() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	virtual void ResolveHullMesh();
	virtual float ComputeVisualUniformScale() const;
	void ConfigurePhysicsHullFromMesh();
	void AlignToWaterSurface();
	void GetScaledMeshVerticalMetrics(float& OutMinZ, float& OutHeight) const;

	void UpdateKinematicNavigation(float DeltaTime);
	void UpdateIdleWaveBob(float DeltaTime);
	bool SampleWaterAt(const FVector& WorldXY, FVector& OutSurfaceLoc, FVector& OutNormal, FVector& OutVelocity) const;

	void ClearGoAroundWaypoints();
	/** Clears contingency waypoints, shore-follow inject state, and stuck timers for a new player order. */
	void ResetContingencyNavigationState();
	float ComputeShoreFollowArcBiasRadians(float ExtraBiasRadians = 0.f) const;
	FVector GetActiveNavigationTarget() const;
	void AdvanceGoAroundWaypointIfReached(const FVector& ShipLoc);
	void PruneStaleGoAroundWaypoints(const FVector& ShipLoc);
	void ClampShipToRealmBounds(FVector& InOutLoc) const;
	void TryBuildGoAroundPathIfNeeded(const FVector& ShipLoc, bool bForceReplan = false);
	void TryInjectShoreFollowStep(const FVector& ShipLoc);
	bool CanCompleteTransitOrderAt(
		const FVector& Loc,
		UIH_P1C07_NavAvoidanceSubsystem* Avoidance,
		float HullRadiusCm) const;
	/** After arriving at current true dest, pop next player breadcrumb if any. */
	void TryAdvancePlayerSailQueue();

	struct FPlayerSailWaypoint
	{
		FVector WorldDestination = FVector::ZeroVector;
		FRotator ApproachHeading = FRotator::ZeroRotator;
	};
	TArray<FPlayerSailWaypoint> PlayerSailQueue;

	UPROPERTY(VisibleAnywhere, Category = "P1C07|Ship")
	TObjectPtr<UBoxComponent> PhysicsHull;

	UPROPERTY(VisibleAnywhere, Category = "P1C07|Ship")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "P1C07|Ship")
	TObjectPtr<UBuoyancyComponent> Buoyancy;

	UPROPERTY(VisibleAnywhere, Category = "P1C07|Ship")
	TObjectPtr<UIH_P1C07_ShipWakeComponent> WakeComponent;

	float ScaledMeshBottomLocalZ = 0.f;
	float LengthScaleMultiplier = 1.f;
	FVector ExtraNonUniformScale = FVector::OneVector;
	FLinearColor HullTint = FLinearColor::White;

	EIH_P1C07_ShipNavState NavState = EIH_P1C07_ShipNavState::Idle;
	/** Player-ordered goal (RMB water). Never mutated by go-around replans. */
	FVector TrueDestinationWorld = FVector::ZeroVector;
	FRotator FinalApproachHeading = FRotator::ZeroRotator;
	/** Do-until: keep transiting through contingency waypoints until true destination is reached. */
	bool bTransitUntilTrueDestination = false;
	/** Island go-around / contingency legs only — not a new player order. */
	TArray<FVector> GoAroundWaypointQueue;
	float GoAroundReplanCooldownSec = 0.f;
	float GoAroundStuckTimeSec = 0.f;
	float GoAroundPrevDistToActiveWpCm = MAX_FLT;
	FVector LastProgressLoc = FVector::ZeroVector;
	FVector LastShoreFollowWaypoint = FVector::ZeroVector;
	int32 ShoreFollowInjectCount = 0;
	uint32 MoveOrderGroupId = 0;
	bool bSelected = false;
	float CurrentSpeedCmPerSec = 0.f;
	float BobPhase = 0.f;
	float SmoothedTransitPitch = 0.f;
	float SmoothedTransitRoll = 0.f;
};
