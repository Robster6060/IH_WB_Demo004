// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C07_IslandCollisionSubsystem.generated.h"

class UPrimitiveComponent;

/**
 * Hard collision for ships against island procedural meshes.
 *
 * Future island actors: tag with IH_Island, enable BlockAll on collision mesh(es), and call
 * RegisterIslandCollision(this, MeshComponent) from BeginPlay / UnregisterIslandCollision in EndPlay.
 * Nav elliptical repulsion remains soft steering; sweep resolution here is collision-proof.
 */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C07_IslandCollisionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static const FName IslandActorTag;

	void RegisterIslandCollision(AActor* IslandActor, UPrimitiveComponent* CollisionMesh);
	void UnregisterIslandCollision(AActor* IslandActor);

	/**
	 * Sweeps a spherical hull from CurrentLoc toward ProposedLoc against registered IH_Island meshes only.
	 * Sweep is flattened to WaterSurfaceZCm; submerged hits are ignored. Adjusts ProposedLoc XY (block + slide).
	 * Returns true when movement was constrained by a hit.
	 */
	bool ResolveShipMovementAgainstIslands(
		const AActor* ShipActor,
		const FVector& CurrentLoc,
		FVector& InOutProposedLoc,
		float HullRadiusCm,
		float WaterSurfaceZCm = 0.f) const;

	/** If hull center overlaps island geometry, push to nearest safe exterior point. */
	bool CorrectShipPositionIfInsideIslands(
		const AActor* ShipActor,
		FVector& InOutLoc,
		float HullRadiusCm,
		float WaterSurfaceZCm = 0.f) const;

	/** True when registered island geometry blocks the water surface at WorldXY (dry land / coastline). */
	bool IsDryLandAtWaterSurface(
		const FVector& WorldXY,
		float WaterSurfaceZCm,
		float ProbeRadiusCm = 40.f) const;

	/**
	 * Vertical ray cast against registered IH_Island procedural meshes (complex collision).
	 * Returns the topmost surface hit at WorldXY, offset along ImpactNormal by SurfaceLiftCm.
	 */
	bool TrySampleIslandSurfaceAtXY(
		const FVector2D& WorldXY,
		float ReferenceZ,
		float SurfaceLiftCm,
		const AActor* IgnoreActor,
		FVector& OutLocation,
		AActor** OutIslandActor = nullptr) const;

	/** Standoff from island surface after sweep hit (cm). */
	UPROPERTY(EditAnywhere, Category = "P1C07|IslandCollision")
	float ShipIslandSkinWidthCm = 80.f;

	/** Max sweep segment length (cm); long moves are sub-stepped to prevent tunneling. */
	UPROPERTY(EditAnywhere, Category = "P1C07|IslandCollision")
	float MaxSweepSubStepCm = 250.f;

private:
	bool ResolveSingleSweepStepAgainstIslands(
		const AActor* ShipActor,
		const FVector& CurrentLoc,
		FVector& InOutProposedLoc,
		float HullRadiusCm,
		float WaterSurfaceZCm) const;

	/** Runtime registration only — not serialized. Multiple comps per island (land + WWF shelf). */
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UPrimitiveComponent>>> RegisteredIslands;
};
