// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHDevViewRuntime.h"

#include "IHInvisibleHandDesignSpec.h"
#include "IH_P1C12_OceanPlane.h"
#include "IH_WaterlineOceanAdapter.h"
#include "IH_WB_Demo004.h"
#include "IH_WB_Demo004GameMode.h"
#include "IH_WB_IslandActor.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#if !UE_BUILD_SHIPPING
namespace
{
	bool GOceanVisible = !IHInvisibleHandSpec::bDevDemo_HideOceanWaterActors;
	bool GContoursVisible = false;
	bool GFeaturesVisible = false;
	/** WB productivity default: clouds hidden (re-enable via DEV View). */
	bool GCloudsVisible = false;
	/** Fidelity grabs: lower sun + darker TOPO (default OFF so L1 can A/B). */
	bool GGrabContrastEnabled = false;
}
#endif

#if !UE_BUILD_SHIPPING
bool IHInvisibleHandSpec::DevView_IsHideOceanEnabled()
{
	return !GOceanVisible;
}

bool IHInvisibleHandSpec::DevView_IsDryShoulderHullHideEnabled()
{
	return false;
}

bool IHInvisibleHandSpec::DevView_AreContoursEnabled()
{
	return GContoursVisible;
}
#endif

namespace IHDevViewRuntime
{
#if UE_BUILD_SHIPPING
	bool IsOceanVisible() { return true; }
	void SetOceanVisible(bool) {}
	bool AreContoursVisible() { return false; }
	void SetContoursVisible(bool) {}
	bool AreFeaturesVisible() { return false; }
	void SetFeaturesVisible(bool) {}
	bool AreCloudsVisible() { return true; }
	void SetCloudsVisible(bool) {}
	bool IsGrabContrastEnabled() { return false; }
	void SetGrabContrastEnabled(bool) {}
	void ApplyContoursVisibilityToWorld(UWorld*) {}
	void ApplyFeaturesVisibilityToWorld(UWorld*) {}
	void ApplyOceanVisibilityToWorld(UWorld*) {}
	void ApplyCloudsVisibilityToWorld(UWorld*) {}
	void ApplyGrabContrastToWorld(UWorld*) {}
#else
	bool IsOceanVisible() { return GOceanVisible; }
	void SetOceanVisible(const bool bVisible) { GOceanVisible = bVisible; }

	bool AreContoursVisible() { return GContoursVisible; }
	void SetContoursVisible(const bool bVisible) { GContoursVisible = bVisible; }

	bool AreFeaturesVisible() { return GFeaturesVisible; }
	void SetFeaturesVisible(const bool bVisible) { GFeaturesVisible = bVisible; }

	bool AreCloudsVisible() { return GCloudsVisible; }
	void SetCloudsVisible(const bool bVisible) { GCloudsVisible = bVisible; }

	bool IsGrabContrastEnabled() { return GGrabContrastEnabled; }
	void SetGrabContrastEnabled(const bool bEnabled) { GGrabContrastEnabled = bEnabled; }

	void ApplyContoursVisibilityToWorld(UWorld* World)
	{
		if (!World) return;
		const bool bShow = AreContoursVisible();
		for (TActorIterator<AIH_WB_IslandActor> It(World); It; ++It)
		{
			It->ApplyDevContoursVisibility(bShow);
			It->RefreshIslandActorTickEnabled();
		}
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase DEV-WWF viewToggle contours=%d"), bShow ? 1 : 0);
	}

	void ApplyFeaturesVisibilityToWorld(UWorld* World)
	{
		if (!World) return;
		const bool bShow = AreFeaturesVisible();
		for (TActorIterator<AIH_WB_IslandActor> It(World); It; ++It)
		{
			It->ApplyDevFeaturesVisibility(bShow);
		}
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase DEV-WWF viewToggle features=%d"), bShow ? 1 : 0);
	}

	void ApplyOceanVisibilityToWorld(UWorld* World)
	{
		if (!World) return;
		const bool bShow = IsOceanVisible();
		for (TActorIterator<AIH_P1C12_OceanPlane> It(World); It; ++It)
		{
			It->ApplyDevOceanVisibility(bShow);
		}
		// IH-DEC-043 (Waterline PRO 6 conversion): handle whichever ocean provider is actually
		// active — either can be spawned depending on IHInvisibleHandSpec::GetGate0OceanProvider().
		for (TActorIterator<AIH_WaterlineOceanAdapter> It(World); It; ++It)
		{
			It->SetOceanVisible(bShow);
		}
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase DEV-WWF viewToggle ocean=%d"), bShow ? 1 : 0);
	}

	void ApplyCloudsVisibilityToWorld(UWorld* World)
	{
		if (!World) return;
		const bool bShow = AreCloudsVisible();
		int32 HiddenActors = 0;

		// UDS's real volumetric clouds aren't a hideable actor — drive them via Cloud Coverage.
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->ApplyCloudsVisible(bShow);
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			// Ultra Dynamic Sky/Weather (IH-DEC-040) own real VolumetricCloudComponents and manage
			// their own cloud rendering — do not blanket hide/disable the whole actor for them.
			if (Actor->GetClass()->GetPathName().Contains(TEXT("UltraDynamicSky"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			bool bIsCloudOrSkyNoise = false;
			if (Actor->FindComponentByClass<UVolumetricCloudComponent>())
			{
				bIsCloudOrSkyNoise = true;
			}
			else if (AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor))
			{
				if (UStaticMeshComponent* SMC = SMA->GetStaticMeshComponent())
				{
					const FString MeshPath = SMC->GetStaticMesh()
						? SMC->GetStaticMesh()->GetPathName()
						: FString();
					const FString ActorName = Actor->GetName();
					if (MeshPath.Contains(TEXT("Cloud"), ESearchCase::IgnoreCase)
						|| ActorName.Contains(TEXT("Cloud"), ESearchCase::IgnoreCase)
						|| ActorName.Contains(TEXT("SkySphere"), ESearchCase::IgnoreCase)
						|| ActorName.Contains(TEXT("Sky_Sphere"), ESearchCase::IgnoreCase)
						|| MeshPath.Contains(TEXT("SkySphere"), ESearchCase::IgnoreCase)
						|| MeshPath.Contains(TEXT("SM_SkySphere"), ESearchCase::IgnoreCase))
					{
						bIsCloudOrSkyNoise = true;
					}
				}
			}

			if (bIsCloudOrSkyNoise)
			{
				Actor->SetActorHiddenInGame(!bShow);
				Actor->SetActorEnableCollision(false);
				++HiddenActors;
			}
		}

		if (IHInvisibleHandSpec::IsDevDemoClearAtmosphericFogEnabled() && !bShow)
		{
			for (TActorIterator<AExponentialHeightFog> FogIt(World); FogIt; ++FogIt)
			{
				FogIt->SetActorHiddenInGame(true);
			}
		}
		else if (bShow)
		{
			for (TActorIterator<AExponentialHeightFog> FogIt(World); FogIt; ++FogIt)
			{
				FogIt->SetActorHiddenInGame(false);
			}
		}

		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase DEV-WWF viewToggle clouds=%d affectedActors=%d"),
			bShow ? 1 : 0, HiddenActors);
	}

	void ApplyGrabContrastToWorld(UWorld* World)
	{
		if (!World) return;
		const bool bGrab = IsGrabContrastEnabled();
		// Sun-intensity toggle retired (IH-DEC-040): Ultra Dynamic Sky's own auto-exposure
		// replaces the old TankSunIntensityPie/GrabContrast manual dimming. Material-side
		// grab contrast on islands is unaffected.
		for (TActorIterator<AIH_WB_IslandActor> It(World); It; ++It)
		{
			It->ApplyDevGrabContrastMaterials(bGrab);
		}
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Phase DEV-WWF viewToggle grabContrast=%d"), bGrab ? 1 : 0);
	}
#endif
}
