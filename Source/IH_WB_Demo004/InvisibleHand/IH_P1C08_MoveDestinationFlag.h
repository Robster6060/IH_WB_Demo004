// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C08_MoveDestinationFlag.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class USceneComponent;
class IIH_P1C08_SelectableMannequin;

/** Temporary red pennant flag at a troop move-order anchor - mirrors
 * AIH_P1C07_MoveDestinationBuoy's exact lifecycle (persist until every assigned Mannequin arrives,
 * self-destroy after a minimum alive time once empty), reskinned for land per the user's own
 * explicit choice (a flag, not a buoy). */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C08_MoveDestinationFlag : public AActor
{
	GENERATED_BODY()

public:
	AIH_P1C08_MoveDestinationFlag();

	void InitOrder(const FVector& AnchorWorld, const TArray<TScriptInterface<IIH_P1C08_SelectableMannequin>>& AssignedMannequins);

	/** Drop mannequins that received a newer replace-order; destroy self when none remain. */
	void RemoveTrackedMannequins(const TArray<AActor*>& Mannequins);

	FVector GetAnchorWorld() const { return AnchorWorldCm; }

protected:
	virtual void Tick(float DeltaTime) override;

	// Unscaled root that PoleMesh and FlagMesh both attach to as siblings, NOT to each other -
	// PoleMesh's own non-uniform scale (thin cylinder stretched tall) would otherwise be inherited
	// by any child attached to it, silently collapsing/distorting that child's own scale and offset.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FlagRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PoleMesh;

	// Isosceles triangle pennant, built as a small procedural mesh (2 tris - front + back winding,
	// so it's visible from either side, not just its front-facing normal) rather than a scaled
	// engine primitive, since no built-in shape is triangular.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> FlagMesh;

	TArray<TWeakObjectPtr<AActor>> TrackedMannequins;
	FVector AnchorWorldCm = FVector::ZeroVector;
	float AliveTimeSec = 0.f;

	static constexpr float MinAliveSeconds = 2.f;
	/** Mannequin considered arrived at this flag when within this 2D radius (cm) - a human-scale
	 * unit, so much tighter than the ship buoy's own ArrivalRadiusCm. */
	static constexpr float ArrivalRadiusCm = 150.f;

	// Pole + pennant geometry, shared between the constructor (initial build) and Tick (flutter
	// animation) - see .cpp for how these lay out the triangle.
	static constexpr float PoleHeightCm = 300.f;
	static constexpr float PoleDiameterCm = 8.f;
	static constexpr float FlagWidthCm = 300.f;
	static constexpr float FlagHeightCm = 180.f;
	static constexpr float FlagFlutterAmplitudeCm = 40.f;
	static constexpr float FlagFlutterFrequencyHz = 1.6f;

	/** Randomized per-instance so multiple flags don't flap in lockstep. */
	float FlutterPhaseOffset = 0.f;
};
