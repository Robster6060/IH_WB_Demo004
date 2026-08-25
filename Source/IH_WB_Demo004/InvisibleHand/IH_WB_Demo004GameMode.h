// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_IslandManualTransform.h"
#include "IHCalendarTypes.h"
#include "GameFramework/GameModeBase.h"
#include "IH_WB_Demo004GameMode.generated.h"

class AIH_P1C12_OceanPlane;
class AIH_P1C08_AltitudeStoryStickActor;
class AIH_WB_IslandActor;
class AIH_P1C10_IslandBaseDevPropActor;
class AIH_TerrainStampGalleryActor;
class AIH_P1C07_BuoyantCubeActor;
class AIH_P1C07_MerchantmanShipActor;

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
	/** Drives UDS's Time of Day/Year/Month/Day/Season from GameInstance's calendar snapshot (IH-DEC-040). */
	void ApplyCalendarFromGameInstance();
	/** Weather Preview DEV widget: pushes only Time of Day for the given bracket, bypassing
	 * GameInstance entirely — a non-persisted lighting preview, not a calendar change. */
	void ApplyPreviewTimeOfDay(EIHTimeBracket Bracket);
	/** Switches TankWeatherActor to a specific stock UDS Weather Preset by name (Weather Preview DEV widget). */
	bool ApplyWeatherPreset(const FString& PresetName);
	/** Hands control back to UDS's own Random Weather Variation after a Weather Preview preset pick. */
	void ResumeRandomWeatherVariation();
	/** DEV View "Clouds" toggle: drives UDS's own Cloud Coverage (IH-DEC-040 follow-on — the old
	 * actor-hide toggle has no effect on UDS's real volumetric clouds). Caches the prior value on
	 * hide so re-enabling restores it rather than guessing a fixed "on" number. */
	void ApplyCloudsVisible(bool bVisible);

	/** Play Atmospherics DEV widget: auto-advances Hour bracket (wrapping into the next Day, then
	 * Month, then Year at the canonical 30-day/12-month boundaries) every SecondsPerStep, pushing
	 * each step to UDS. Remembers the dialed starting Year/Month/Day/Hour on first Start so Stop
	 * can return to it (Pause leaves the current position as-is). */
	void StartAtmosphericsPlayback(float SecondsPerStep);
	void PauseAtmosphericsPlayback();
	void StopAtmosphericsPlayback();
	bool IsAtmosphericsPlaying() const { return bAtmosphericsPlaying; }

	AIH_WB_IslandActor* GetSpawnedIsland(int32 IslandIndex) const;
	const TArray<TObjectPtr<AIH_WB_IslandActor>>& GetSpawnedIslands() const { return SpawnedIslands; }

	FVector2D GetSeedBaseCenterCm(int32 IslandIndex) const;
	void ApplyIslandManualTransform(int32 IslandIndex, const FIHIslandManualTransform& Transform, bool bRefreshMinimap = true);

protected:
	virtual void StartPlay() override;
	void EnsureMinimalWorldForBlankMap();
	void ConfigureUltraDynamicSky();
	void SpawnIslandsFromGameInstance();
	/** IH-DEC-033: pull every spawned island toward the Story Stick origin (world center) by a
	 * single uniform scale factor, closed-form solved so every island pair's actual coastline gap
	 * clears the 1.5km floor with minimum leftover ocean. Run after all islands are spawned and
	 * built, before dev props (which snapshot island transforms) are placed. */
	void CompactIslandsTowardStoryStickOrigin();
	void DestroyAllIslandBaseDevProps();
	void SpawnIslandBaseDevPropsForSpawnedIslands();
	void RefreshTankGeometry(float RealmHalfExtentNSKm);
	/** THROWAWAY DEV SPIKE (Waterline Part 2 verification, see plan): spawns one
	 * BP_Shore_Manager_Gen4 positioned/sized against the first real generated island, to observe
	 * whether it auto-detects the procedural coastline or needs manual spline authoring. Not
	 * wired into any permanent gameplay path — remove once the verification question is answered. */
	void SpawnShoreManagerVerificationSpike();

	UFUNCTION()
	void DeferredApplyStartCamera();
	UFUNCTION()
	void DeferredSpawnBuoyantCube();
	UFUNCTION()
	void DeferredSpawnMerchantmanShip();
	UFUNCTION()
	void AdvanceAtmosphericsStep();

	UPROPERTY(Transient) FTimerHandle StartCameraTimer;
	UPROPERTY(Transient) FTimerHandle AtmosphericsPlaybackTimer;
	bool bAtmosphericsPlaying = false;
	int32 AtmosphericsStartYear = 1000;
	int32 AtmosphericsStartMonth = 4;
	int32 AtmosphericsStartDay = 1;
	EIHTimeBracket AtmosphericsStartHourBracket = EIHTimeBracket::Afternoon;
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
	/** Ultra Dynamic Sky/Weather (Content-only Blueprint pack, no native C++ type — IH-DEC-040). */
	UPROPERTY(Transient) TObjectPtr<AActor> TankSkyActor = nullptr;
	UPROPERTY(Transient) TObjectPtr<AActor> TankWeatherActor = nullptr;
	/** Cached UDS weather values (probed defaults), restored when the Clouds toggle re-enables.
	 * Covers Rain/Snow/Fog/Dust too, not just Cloud Coverage — a Weather Preview preset selection
	 * (Rain/Snow/Sand family) bundles its own values for all of these, which otherwise silently
	 * overrides a prior Clouds-off toggle. */
	float CachedSkyCloudCoverage = 3.8f;
	float CachedWeatherCloudCoverage = 7.5f;
	float CachedWeatherRain = 0.f;
	float CachedWeatherSnow = 0.f;
	float CachedWeatherFog = 3.f;
	float CachedWeatherDust = 0.f;
	UPROPERTY(Transient) TObjectPtr<AIH_TerrainStampGalleryActor> StampGallery = nullptr;
};
