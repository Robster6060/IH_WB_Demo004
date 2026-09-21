// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IH_P1C08_SelectableMannequin.generated.h"

class UPrimitiveComponent;

// Mirrors IIH_P1C07_SelectableShip exactly - see UIH_P1C08_MannequinRegistrySubsystem for why this
// is a parallel, independent interface rather than reusing the ship one (Do No Harm: never touch
// the signed-off ship selection system).
UINTERFACE(MinimalAPI, BlueprintType)
class UIH_P1C08_SelectableMannequin : public UInterface
{
	GENERATED_BODY()
};

class IIH_P1C08_SelectableMannequin
{
	GENERATED_BODY()

public:
	virtual UPrimitiveComponent* GetMannequinSelectionPrimitive() const = 0;
	virtual void SetMannequinSelected(bool bSelected) = 0;
	virtual void CommandWalkTo(const FVector& WorldDestination) = 0;
	virtual bool HasArrivedAtDestination() const = 0;
	virtual FVector GetTrueDestinationWorld() const = 0;
	virtual FVector GetMannequinFeetLocation() const = 0;

	/**
	 * 2026-09-13: breadcrumb waypoint chain (Shift+click adds an intermediate stop instead of
	 * replacing the current order outright). If idle, walks there immediately (matches CommandWalkTo);
	 * if already mid-walk toward an earlier waypoint, queues it to start automatically once the
	 * current leg is reached (see AdvanceToNextWaypoint, called by AIH_P1C08_MoveDestinationFlag).
	 */
	virtual void EnqueueWalkWaypoint(const FVector& WorldDestination) = 0;
	/** Pops and starts walking the next queued waypoint, if any - called by the flag that owns the
	 * CURRENT destination the instant this Mannequin arrives there. No-op if the queue is empty. */
	virtual void AdvanceToNextWaypoint() = 0;
	/** Clears any queued waypoints - a fresh, non-appended order replaces the whole chain. */
	virtual void ClearWaypointQueue() = 0;
};
