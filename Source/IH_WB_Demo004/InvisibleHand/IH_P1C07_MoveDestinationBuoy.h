// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C07_MoveDestinationBuoy.generated.h"

class UStaticMeshComponent;
class IIH_P1C07_SelectableShip;

/** Temporary orange marker at a fleet move order anchor; removed when all assigned ships arrive. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C07_MoveDestinationBuoy : public AActor
{
	GENERATED_BODY()

public:
	AIH_P1C07_MoveDestinationBuoy();

	void InitOrder(const FVector& AnchorWorld, const TArray<TScriptInterface<IIH_P1C07_SelectableShip>>& AssignedShips);

	/** Drop ships that received a newer replace-order; destroy self when no ships remain. */
	void RemoveTrackedShips(const TArray<AActor*>& Ships);

	FVector GetAnchorWorld() const { return AnchorWorldCm; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BuoyMesh;

	TArray<TWeakObjectPtr<AActor>> TrackedShips;
	FVector AnchorWorldCm = FVector::ZeroVector;
	float AliveTimeSec = 0.f;

	static constexpr float MinAliveSeconds = 2.f;
	/** Ship considered arrived at this buoy when within this 2D radius (cm). */
	static constexpr float ArrivalRadiusCm = 2500.f;
};
