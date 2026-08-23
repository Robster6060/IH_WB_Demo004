// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Components/ActorComponent.h"
#include "IH_P1C07_ShipWakeComponent.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;
class AIH_P1C07_CommandableShipActor;

/** Stern trail + bow V-wake foam on the water surface. */
UCLASS(ClassGroup = (P1C07), meta = (BlueprintSpawnableComponent))
class IH_WB_DEMO004_API UIH_P1C07_ShipWakeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UIH_P1C07_ShipWakeComponent();

	void UpdateWake(float SpeedCmPerSec, bool bEmit);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsurePool();
	float ResolveBeamWidthCm() const;
	float ResolveKeelLengthCm() const;
	void ApplySliceAppearance(UStaticMeshComponent* Slice, float Alpha) const;
	bool TrySpawnSlice(
		const FVector& WorldPos,
		float PlaneYawDeg,
		float WidthCm,
		float LengthCm,
		float LifetimeSec,
		float Alpha,
		bool bBowLeg = false);
	void EmitSternWake(float BeamWidthCm, float KeelLengthCm, float SpeedCmPerSec, float SpeedFactor, const AIH_P1C07_CommandableShipActor* Ship);
	void EmitBowVWake(float BeamWidthCm, float KeelLengthCm, float SpeedCmPerSec, float SpeedFactor, const AIH_P1C07_CommandableShipActor* Ship);
	void TickSlices(float DeltaTime);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PoolMeshes;

	TArray<float> SliceAgeSec;
	TArray<float> SliceLifetimeSec;
	TArray<FVector> SliceInitialScale;
	TArray<uint8> SliceIsBowLeg;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WakeMaterialParent;

	bool bWakeMaterialUsesAlpha = false;

	float DistanceSinceLastSliceCm = 0.f;

	static constexpr int32 PoolSize = 72;
	static constexpr float MinSpeedToEmitCmPerSec = 80.f;
	static constexpr float DefaultBeamWidthCm = 900.f;
	static constexpr float DefaultKeelLengthCm = 3780.f;
	static constexpr float TrailLengthKeelMultiplier = 1.5f;
	static constexpr float SliceSpacingKeelFraction = 0.07f;
	static constexpr float BowWakeTrailKeelFraction = 0.75f;
	static constexpr float BowWakeWidthBeamMultiplier = 4.f;
	static constexpr int32 BowWakeSegmentsPerEmit = 6;
	static constexpr float WaterSurfaceOffsetCm = 3.f;
};
