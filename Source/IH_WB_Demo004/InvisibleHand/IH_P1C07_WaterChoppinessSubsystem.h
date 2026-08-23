// Copyright Epic Games, Inc. All Rights Reserved.
// Runtime Gerstner wave tuning for the tank water body (slider scales amplitudes / steepness from calm baseline).

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C07_WaterChoppinessSubsystem.generated.h"

class AWaterBody;
class UGerstnerWaterWaves;
class UGerstnerWaterWaveGeneratorSimple;
class UGerstnerWaterWaveGeneratorSpectrum;

UCLASS()
class IH_WB_DEMO004_API UIH_P1C07_WaterChoppinessSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Duplicate engine wave asset onto the body (so we do not mutate shared content) and snapshot calm parameters. */
	void RegisterTankWaterBody(AWaterBody* WaterBody);

	UFUNCTION(BlueprintCallable, Category = "P1C07|Water")
	void SetChoppiness(float NormalizedChoppiness);

	float GetChoppiness() const { return CurrentChoppiness; }

	bool IsDrivingWaves() const { return RuntimeGerstner != nullptr && bGeneratorSupported; }

private:
	void ApplyChoppiness();

	static UGerstnerWaterWaves* ResolveGerstnerFromBase(class UWaterWavesBase* Root);

	UPROPERTY(Transient)
	TWeakObjectPtr<AWaterBody> CachedWaterBody;

	UPROPERTY(Transient)
	TObjectPtr<UGerstnerWaterWaves> RuntimeGerstner;

	UPROPERTY(Transient)
	TObjectPtr<UGerstnerWaterWaveGeneratorSimple> SimpleGenerator;

	UPROPERTY(Transient)
	TObjectPtr<UGerstnerWaterWaveGeneratorSpectrum> SpectrumGenerator;

	TArray<float> BaselineOctaveAmplitudeScales;

	float BaseMinAmplitude = 0.f;
	float BaseMaxAmplitude = 0.f;
	float BaseSmallWaveSteepness = 0.f;
	float BaseLargeWaveSteepness = 0.f;

	bool bGeneratorSupported = false;

	float CurrentChoppiness = 0.f;
};
