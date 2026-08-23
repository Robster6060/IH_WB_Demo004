// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_UnderwaterDepthAttenuationSubsystem.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"

namespace
{
	/** Full diffusion above -25 m ASL (cm). */
	static constexpr float FullDiffusionAboveZCm = -2500.f;
	/** Near-zero diffusion below -35 m ASL (cm). */
	static constexpr float AttenuatedBelowZCm = -3500.f;
	/** Residual scale in the deep band (-35 m .. -250 m). */
	static constexpr float DeepBandMinScale = 0.02f;
}

float UIH_P1C08_UnderwaterDepthAttenuationSubsystem::ComputeSunlightDiffusionScale(float CameraZCm)
{
	if (CameraZCm >= FullDiffusionAboveZCm)
	{
		return 1.f;
	}
	if (CameraZCm <= AttenuatedBelowZCm)
	{
		return DeepBandMinScale;
	}
	return FMath::GetRangePct(AttenuatedBelowZCm, FullDiffusionAboveZCm, CameraZCm);
}

void UIH_P1C08_UnderwaterDepthAttenuationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UIH_P1C08_UnderwaterDepthAttenuationSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

TStatId UIH_P1C08_UnderwaterDepthAttenuationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UIH_P1C08_UnderwaterDepthAttenuationSubsystem, STATGROUP_Tickables);
}

bool UIH_P1C08_UnderwaterDepthAttenuationSubsystem::IsTickable() const
{
	return GetWorld() != nullptr;
}

void UIH_P1C08_UnderwaterDepthAttenuationSubsystem::CacheBaselinesIfNeeded()
{
	if (bBaselinesCached)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CachedFogComponent.IsValid())
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			if (UExponentialHeightFogComponent* FC = It->GetComponent())
			{
				CachedFogComponent = FC;
				break;
			}
		}
	}

	if (!CachedWaterBodyComponent.IsValid())
	{
		for (TActorIterator<AWaterBody> It(World); It; ++It)
		{
			if (UWaterBodyComponent* Wbc = It->GetWaterBodyComponent())
			{
				CachedWaterBodyComponent = Wbc;
				break;
			}
		}
	}

	if (!CachedFogComponent.IsValid() && !CachedWaterBodyComponent.IsValid())
	{
		return;
	}

	if (UExponentialHeightFogComponent* FC = CachedFogComponent.Get())
	{
		BaselineFogDensity = FC->FogDensity;
		BaselineDirectionalInscatteringExponent = FC->DirectionalInscatteringExponent;
		BaselineDirectionalInscatteringLuminance = FC->DirectionalInscatteringLuminance;
		BaselineVolumetricFogExtinctionScale = FC->VolumetricFogExtinctionScale;
		BaselineVolumetricFogEmissive = FC->VolumetricFogEmissive;
	}

	if (UWaterBodyComponent* Wbc = CachedWaterBodyComponent.Get())
	{
		BaselineUnderwaterPostProcessBlendWeight = Wbc->UnderwaterPostProcessSettings.BlendWeight;
		if (UMaterialInstanceDynamic* MID = Wbc->GetUnderwaterPostProcessMaterialInstance())
		{
			MID->GetVectorParameterValue(FName(TEXT("ScatteringCoefficients")), BaselineScatteringCoefficients);
			MID->GetVectorParameterValue(FName(TEXT("ColorScaleBehindWater")), BaselineColorScaleBehindWater);
		}
	}

	bBaselinesCached = true;
}

void UIH_P1C08_UnderwaterDepthAttenuationSubsystem::ApplyAttenuation(float DiffusionScale)
{
	if (UExponentialHeightFogComponent* FC = CachedFogComponent.Get())
	{
		FC->SetFogDensity(BaselineFogDensity * DiffusionScale);
		FC->DirectionalInscatteringExponent = FMath::Lerp(2.f, BaselineDirectionalInscatteringExponent, DiffusionScale);
		FC->DirectionalInscatteringLuminance = BaselineDirectionalInscatteringLuminance * DiffusionScale;
		FC->VolumetricFogExtinctionScale = FMath::Lerp(0.05f, BaselineVolumetricFogExtinctionScale, DiffusionScale);
		FC->VolumetricFogEmissive = BaselineVolumetricFogEmissive * DiffusionScale;
		FC->MarkRenderStateDirty();
	}

	if (UWaterBodyComponent* Wbc = CachedWaterBodyComponent.Get())
	{
		Wbc->UnderwaterPostProcessSettings.BlendWeight = BaselineUnderwaterPostProcessBlendWeight * DiffusionScale;
		if (UMaterialInstanceDynamic* MID = Wbc->GetUnderwaterPostProcessMaterialInstance())
		{
			MID->SetVectorParameterValue(
				FName(TEXT("ScatteringCoefficients")), BaselineScatteringCoefficients * DiffusionScale);
			const FLinearColor ScaledColor(
				BaselineColorScaleBehindWater.R,
				BaselineColorScaleBehindWater.G,
				BaselineColorScaleBehindWater.B,
				BaselineColorScaleBehindWater.A * DiffusionScale);
			MID->SetVectorParameterValue(FName(TEXT("ColorScaleBehindWater")), ScaledColor);
		}
	}
}

void UIH_P1C08_UnderwaterDepthAttenuationSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (WarmupSecondsRemaining > 0.f)
	{
		WarmupSecondsRemaining -= DeltaTime;
		return;
	}

	CacheBaselinesIfNeeded();
	if (!bBaselinesCached)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	const float DiffusionScale = ComputeSunlightDiffusionScale(ViewLoc.Z);
	if (FMath::IsNearlyEqual(DiffusionScale, LastAppliedScale, 0.005f))
	{
		return;
	}

	LastAppliedScale = DiffusionScale;
	ApplyAttenuation(DiffusionScale);
}
