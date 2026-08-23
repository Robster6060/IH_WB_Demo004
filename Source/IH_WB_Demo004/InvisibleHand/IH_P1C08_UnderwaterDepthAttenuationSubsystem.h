// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "IH_P1C08_UnderwaterDepthAttenuationSubsystem.generated.h"

class UExponentialHeightFogComponent;
class UWaterBodyComponent;

/**
 * Attenuates underwater sunlight diffusion (fog inscattering, volumetric scatter, water PP)
 * when the view camera is between -35 m and -250 m ASL, with a smooth fade from full above -25 m.
 */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_UnderwaterDepthAttenuationSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static float ComputeSunlightDiffusionScale(float CameraZCm);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	void CacheBaselinesIfNeeded();
	void ApplyAttenuation(float DiffusionScale);

	bool bBaselinesCached = false;

	TWeakObjectPtr<UExponentialHeightFogComponent> CachedFogComponent;
	TWeakObjectPtr<UWaterBodyComponent> CachedWaterBodyComponent;

	float BaselineFogDensity = 0.f;
	float BaselineDirectionalInscatteringExponent = 0.f;
	FLinearColor BaselineDirectionalInscatteringLuminance = FLinearColor::Black;
	float BaselineVolumetricFogExtinctionScale = 0.f;
	FLinearColor BaselineVolumetricFogEmissive = FLinearColor::Black;
	float BaselineUnderwaterPostProcessBlendWeight = 1.f;
	FLinearColor BaselineScatteringCoefficients = FLinearColor::Black;
	FLinearColor BaselineColorScaleBehindWater = FLinearColor::White;

	float LastAppliedScale = -1.f;
	float WarmupSecondsRemaining = 0.65f;
};
