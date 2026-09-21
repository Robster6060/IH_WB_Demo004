// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Character.h"
#include "IH_P1C08_SelectableMannequin.h"
#include "IH_P1C08_MannequinActor.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/** DEV-only scale-reference NPC, placed via the "Mannequin" HUD button (click-to-place on
 * IslandMesh, mirroring "Place Ship"'s click-to-place-on-water). Selectable and walkable to a
 * clicked land point (IIH_P1C08_SelectableMannequin, driven by UIH_P1C08_MannequinRegistrySubsystem
 * - the ship box-select/move-order system's own pattern, mirrored as a parallel, independent system
 * per this project's Do No Harm convention).
 *
 * 2026-09-11/12: switched from Waterline's legacy UE4 SK_Mannequin to UE5.8's own bundled Manny
 * (Content/Characters/Mannequins/) while chasing a rendering bug later found to be an
 * AnimationBlueprint-mode-with-no-AnimClass pose issue affecting either mesh equally.
 * 2026-09-13: switched back to the legacy SK_Mannequin now that the actual bug is fixed, to avoid
 * an animation-retarget artifact from running ThirdPerson_AnimBP on a merely-compatible skeleton
 * rather than its native UE4_Mannequin_Skeleton target - see this session's pipeline-doc history. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C08_MannequinActor : public ACharacter, public IIH_P1C08_SelectableMannequin
{
	GENERATED_BODY()

public:
	AIH_P1C08_MannequinActor();

	//~ Begin IIH_P1C08_SelectableMannequin
	virtual UPrimitiveComponent* GetMannequinSelectionPrimitive() const override;
	virtual void SetMannequinSelected(bool bSelected) override;
	virtual void CommandWalkTo(const FVector& WorldDestination) override;
	virtual bool HasArrivedAtDestination() const override;
	virtual FVector GetTrueDestinationWorld() const override { return TrueDestinationWorld; }
	virtual FVector GetMannequinFeetLocation() const override { return GetActorLocation(); }
	virtual void EnqueueWalkWaypoint(const FVector& WorldDestination) override;
	virtual void AdvanceToNextWaypoint() override;
	virtual void ClearWaypointQueue() override { PendingWaypoints.Reset(); }
	//~ End IIH_P1C08_SelectableMannequin

	bool IsMannequinSelected() const { return bSelected; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 2026-09-12: true-scale capsule outline, visible but see-through, so the actor can be spotted
	// from a review-camera distance and so its collision capsule's alignment with the terrain/mesh
	// can be checked visually (e.g. to rule out sinking below IslandMesh) - NOT a "spot from far
	// away" beacon like the retired LocatorBeacon, since it's sized to the real capsule, not
	// exaggerated.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CapsuleVisualizer;

	bool bSelected = false;
	FVector TrueDestinationWorld = FVector::ZeroVector;
	bool bHasActiveDestination = false;

	/** 2026-09-13: breadcrumb waypoint chain - queued stops beyond the current one, in order.
	 * Advanced one at a time by AdvanceToNextWaypoint (called from AIH_P1C08_MoveDestinationFlag's
	 * own Tick, the instant this Mannequin arrives at the flag it's currently tracked by). */
	TArray<FVector> PendingWaypoints;

	// Materials cached the moment the selection tint is first applied, restored on deselect -
	// mirrors AIH_TerrainStampActor::RefreshStampVisualTint's own cache/restore pattern.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> CachedSourceMaterials;

	static constexpr float ArrivalRadiusCm = 150.f;

	// Navigation-invoker radii (see BeginPlay) - generous relative to human scale since a troop
	// move-order can reasonably span a local beach/area, not just a few footsteps.
	static constexpr float InvokerTileGenerationRadiusCm = 10000.f;
	static constexpr float InvokerTileRemovalRadiusCm = 15000.f;
};
