// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_WaterChoppinessSubsystem.h"
#include "IH_WB_Demo004.h"
#include "IH_OceanFromZoneComponent.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterWaves.h"
#include "GerstnerWaterWaves.h"

namespace
{
	static constexpr float AmplitudeGain = 5.f;
	static constexpr float SteepnessDelta = 0.55f;
}

UGerstnerWaterWaves* UIH_P1C07_WaterChoppinessSubsystem::ResolveGerstnerFromBase(UWaterWavesBase* Root)
{
	if (!Root)
	{
		return nullptr;
	}
	if (UGerstnerWaterWaves* G = Cast<UGerstnerWaterWaves>(Root))
	{
		return G;
	}
	return Cast<UGerstnerWaterWaves>(Root->GetWaterWaves());
}

void UIH_P1C07_WaterChoppinessSubsystem::RegisterTankWaterBody(AWaterBody* WaterBody)
{
	CachedWaterBody = WaterBody;
	RuntimeGerstner = nullptr;
	SimpleGenerator = nullptr;
	SpectrumGenerator = nullptr;
	BaselineOctaveAmplitudeScales.Reset();
	bGeneratorSupported = false;

	if (!WaterBody)
	{
		return;
	}

	UGerstnerWaterWaves* Source = ResolveGerstnerFromBase(WaterBody->GetWaterWaves());
	if (!Source)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("[P1C07:CHOP] No Gerstner waves on water body; choppiness disabled."));
		return;
	}

	UGerstnerWaterWaves* Dup = DuplicateObject<UGerstnerWaterWaves>(Source, WaterBody);
	if (!Dup)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("[P1C07:CHOP] DuplicateObject(Gerstner) failed."));
		return;
	}

	WaterBody->SetWaterWaves(Dup);
	RuntimeGerstner = Dup;

	if (UGerstnerWaterWaveGeneratorSimple* Simple = Cast<UGerstnerWaterWaveGeneratorSimple>(Dup->GerstnerWaveGenerator))
	{
		SimpleGenerator = Simple;
		BaseMinAmplitude = Simple->MinAmplitude;
		BaseMaxAmplitude = Simple->MaxAmplitude;
		BaseSmallWaveSteepness = Simple->SmallWaveSteepness;
		BaseLargeWaveSteepness = Simple->LargeWaveSteepness;
		bGeneratorSupported = true;
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("[P1C07:CHOP] Registered GerstnerWaterWaveGeneratorSimple (runtime duplicate)."));
	}
	else if (UGerstnerWaterWaveGeneratorSpectrum* Spec = Cast<UGerstnerWaterWaveGeneratorSpectrum>(Dup->GerstnerWaveGenerator))
	{
		SpectrumGenerator = Spec;
		BaselineOctaveAmplitudeScales.Reserve(Spec->Octaves.Num());
		for (const FGerstnerWaveOctave& O : Spec->Octaves)
		{
			BaselineOctaveAmplitudeScales.Add(O.AmplitudeScale);
		}
		bGeneratorSupported = true;
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("[P1C07:CHOP] Registered GerstnerWaterWaveGeneratorSpectrum (%d octaves)."), Spec->Octaves.Num());
	}
	else
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("[P1C07:CHOP] Unsupported wave generator type %s; choppiness disabled."),
			*GetNameSafe(Dup->GerstnerWaveGenerator));
	}

	ApplyChoppiness();
}

void UIH_P1C07_WaterChoppinessSubsystem::SetChoppiness(float NormalizedChoppiness)
{
	CurrentChoppiness = FMath::Clamp(NormalizedChoppiness, 0.f, 1.f);
	ApplyChoppiness();
}

void UIH_P1C07_WaterChoppinessSubsystem::ApplyChoppiness()
{
	if (!RuntimeGerstner || !bGeneratorSupported)
	{
		return;
	}

	const float A = CurrentChoppiness;

	if (SimpleGenerator)
	{
		SimpleGenerator->MinAmplitude = FMath::Max(BaseMinAmplitude * (1.f + AmplitudeGain * A), 0.0001f);
		SimpleGenerator->MaxAmplitude = FMath::Max(BaseMaxAmplitude * (1.f + AmplitudeGain * A), 0.0001f);
		SimpleGenerator->SmallWaveSteepness = FMath::Clamp(BaseSmallWaveSteepness + A * SteepnessDelta, 0.f, 1.f);
		SimpleGenerator->LargeWaveSteepness = FMath::Clamp(BaseLargeWaveSteepness + A * SteepnessDelta, 0.f, 1.f);
	}
	else if (SpectrumGenerator)
	{
		const int32 N = FMath::Min(SpectrumGenerator->Octaves.Num(), BaselineOctaveAmplitudeScales.Num());
		for (int32 i = 0; i < N; ++i)
		{
			SpectrumGenerator->Octaves[i].AmplitudeScale =
				FMath::Max(BaselineOctaveAmplitudeScales[i] * (1.f + AmplitudeGain * A), 0.0001f);
		}
	}

	RuntimeGerstner->RecomputeWaves(/* bAllowBPScript = */ false);

	// Custom ocean plane / stub — WaterZone OceanFromZone path not active in IH_WB_Demo004.
	(void)CachedWaterBody;
}
