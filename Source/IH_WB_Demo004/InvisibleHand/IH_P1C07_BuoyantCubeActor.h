// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "GameFramework/Actor.h"

#include "Engine/HitResult.h"

#include "IH_P1C07_BuoyantCubeActor.generated.h"



class UPrimitiveComponent;

class UStaticMeshComponent;

class UBuoyancyComponent;

class USphereComponent;



/** 100 m cube with Water-plugin buoyancy; root must be a primitive for UBuoyancyComponent. */

UCLASS(NotPlaceable)

class IH_WB_DEMO004_API AIH_P1C07_BuoyantCubeActor : public AActor

{

	GENERATED_BODY()



public:

	AIH_P1C07_BuoyantCubeActor();



protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	/** Box-volume Archimedes lift; sparse pontoons cannot float a 100 m cube at its target density. */
	void ApplyArchimedesBuoyancy(float DeltaTime);



	/**

	 * Register water bodies via mesh overlaps plus surface queries.

	 * Overlap-only misses while WaterInfoMeshes / collision finish building after cube spawn.

	 */

	void SyncWaterBodiesFromOverlaps();



	/**

	 * Re-run Sync periodically until buoyancy attaches to a WaterBodyComponent or retries exhaust.

	 * UBuoyancyComponent only runs physics while IsActive() (~EnteredWaterBody was called).

	 */

	UFUNCTION()

	void RetryRegisterWaterBodiesForBuoyancy();



	/** Backup path: physics ↔ QueryOnly water overlaps do not always reach AWaterBody::NotifyActorBeginOverlap. */

	UFUNCTION()

	void HandleMeshBeginOverlap(

		UPrimitiveComponent* OverlappedComponent,

		AActor* OtherActor,

		UPrimitiveComponent* OtherComp,

		int32 OtherBodyIndex,

		bool bFromSweep,

		const FHitResult& SweepResult);



	UFUNCTION()

	void HandleMeshEndOverlap(

		UPrimitiveComponent* OverlappedComponent,

		AActor* OtherActor,

		UPrimitiveComponent* OtherComp,

		int32 OtherBodyIndex);



	UPROPERTY(VisibleAnywhere, Category = "P1C07")

	TObjectPtr<UStaticMeshComponent> Mesh;



	UPROPERTY(VisibleAnywhere, Category = "P1C07")

	TObjectPtr<UBuoyancyComponent> Buoyancy;



	/** Tagged "Ocean_POV" — Waterline's BP_Shore_Manager_Gen4 searches for this tag/class to find its follow target. */

	UPROPERTY(VisibleAnywhere, Category = "P1C07")

	TObjectPtr<USphereComponent> OceanPovSphere;



private:

	FTimerHandle WaterBuoyancyPollTimerHandle;

	int32 WaterBuoyancyPollRemaining = 0;

};


