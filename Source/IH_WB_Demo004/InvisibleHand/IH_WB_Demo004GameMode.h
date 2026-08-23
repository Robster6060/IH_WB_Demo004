// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_IslandManualTransform.h"
#include "GameFramework/GameModeBase.h"
#include "IH_WB_Demo004GameMode.generated.h"

class AIH_P1C12_OceanPlane;
class AIH_P1C08_AltitudeStoryStickActor;
class AIH_WB_IslandActor;
class AIH_P1C10_IslandBaseDevPropActor;
class AIH_TerrainStampGalleryActor;
class AIH_P1C07_BuoyantCubeActor;
class AIH_P1C07_MerchantmanShipActor;
class ADirectionalLight;

/** φ landscape tank — P1C10 Azgaar cell pipeline harness. */
UCLASS()
class IH_WB_DEMO004_API AIH_WB_Demo004GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIH_WB_Demo004GameMode();

	virtual void Tick(float DeltaTime) override;

	void ApplyStartCamera() const;
	void RegenerateIslandsFromSeed();
	void ApplyActiveGalleryPreview();
	/** WT-A thin wrapper: sets GI realm half-extent NS only. */
	void ApplyTankPresetAndRegenerate(float NewRealmHalfExtentNSKm);
	void RegenerateSingleIsland(int32 IslandIndex);
	void SpawnStampGalleryForReview();
	void FocusPlayerOnStampGallery() const;
	AIH_TerrainStampGalleryActor* GetStampGalleryForReview() const { return StampGallery; }
	void ApplySunFromGameInstance();
	/** DEV GrabContrast: TankSun Intensity 5.5 vs pie 12. */
	void ApplyGrabContrastSunIntensity(bool bGrabContrast);
	static FRotator ComputeSunRotation(float TimeOfDay, float LatitudeDeg = 0.f);

	AIH_WB_IslandActor* GetSpawnedIsland(int32 IslandIndex) const;
	const TArray<TObjectPtr<AIH_WB_IslandActor>>& GetSpawnedIslands() const { return SpawnedIslands; }

	FVector2D GetSeedBaseCenterCm(int32 IslandIndex) const;
	void ApplyIslandManualTransform(int32 IslandIndex, const FIHIslandManualTransform& Transform, bool bRefreshMinimap = true);

protected:
	virtual void StartPlay() override;
	void EnsureMinimalWorldForBlankMap();
	void ConfigureTankSunLight();
	void SpawnIslandsFromGameInstance();
	/** IH-DEC-033: pull every spawned island toward the Story Stick origin (world center) by a
	 * single uniform scale factor, closed-form solved so every island pair's actual coastline gap
	 * clears the 1.5km floor with minimum leftover ocean. Run after all islands are spawned and
	 * built, before dev props (which snapshot island transforms) are placed. */
	void CompactIslandsTowardStoryStickOrigin();
	void DestroyAllIslandBaseDevProps();
	void SpawnIslandBaseDevPropsForSpawnedIslands();
	void RefreshTankGeometry(float RealmHalfExtentNSKm);

	UFUNCTION()
	void DeferredApplyStartCamera();
	UFUNCTION()
	void DeferredSpawnBuoyantCube();
	UFUNCTION()
	void DeferredSpawnMerchantmanShip();

	UPROPERTY(Transient) FTimerHandle StartCameraTimer;
	UPROPERTY(Transient) FTimerHandle BuoyantCubeSpawnTimer;
	UPROPERTY(Transient) FTimerHandle MerchantmanSpawnTimer;
	UPROPERTY(Transient) TObjectPtr<AIH_P1C07_BuoyantCubeActor> BuoyantCube = nullptr;
	UPROPERTY(Transient) TObjectPtr<AIH_P1C07_MerchantmanShipActor> MerchantmanShip = nullptr;
	UPROPERTY(Transient) TObjectPtr<AIH_P1C08_AltitudeStoryStickActor> AltitudeStoryStick = nullptr;
	UPROPERTY(Transient) TObjectPtr<AIH_WB_IslandActor> Island01 = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<AIH_WB_IslandActor>> SpawnedIslands;
	UPROPERTY(Transient) TArray<TObjectPtr<AIH_P1C10_IslandBaseDevPropActor>> SpawnedIslandBaseDevProps;
	UPROPERTY(Transient) TArray<float> IslandBaseSemiMajorCm;
	UPROPERTY(Transient) TArray<FVector2D> SeedBaseCentersCm;
	/** Gate 0 (P1C12 Arbor): custom ocean plane — active when bGate0UseCustomOceanPlane = true. */
	UPROPERTY(Transient) TObjectPtr<AIH_P1C12_OceanPlane> CustomOceanPlane = nullptr;
	UPROPERTY(Transient) TObjectPtr<ADirectionalLight> TankSunLight = nullptr;
	UPROPERTY(Transient) TObjectPtr<AIH_TerrainStampGalleryActor> StampGallery = nullptr;
};
