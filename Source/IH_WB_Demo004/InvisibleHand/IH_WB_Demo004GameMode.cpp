// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_WB_Demo004GameMode.h"

#include "IH_P1C08_CoastlineTuningSubsystem.h"
#include "IH_P1C08_IslandManualTransform.h"
#include "IH_P1C08_IslandNavSubsystem.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_AltitudeStoryStickActor.h"
#include "IH_WB_IslandActor.h"
#include "IH_P1C10_IslandBaseDevPropActor.h"
#include "IH_TerrainStampGalleryActor.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IHSeedIslandLibrary.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IH_P1C08_MinimapSubsystem.h"
#include "IH_P1C08_MinimapTypes.h"
#include "IHMapSeedFrameworkLibrary.h"
#include "IHMapSeedFrameworkTypes.h"
#include "IHCoastGenerationTypes.h"
#include "IH_P1C07_BuoyantCubeActor.h"
#include "IH_P1C07_MerchantmanShipActor.h"
#include "IH_P1C12_OceanPlane.h"
#include "IHDevViewRuntime.h"
#include "IH_Cube2FlyPlayerController.h"
#include "IH_Cube2FlyPawn.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/**
	 * Gate 0 (P1C12 Arbor): destroy any AWaterBodyOcean / AWaterZone actors that were
	 * baked into the P1C10 level file and carried over in the copy.
	 * Uses class-name matching so NO Water Plugin headers are needed here.
	 * Do No Harm: this is pure actor removal — zero runtime Water Plugin configuration.
	 */
	/**
	 * P1C12 Arbor: keep AWaterBodyOcean and AWaterZone — they provide the GPU Gerstner waves.
	 * Only destroy finite-tank helper bodies (River, Lake, Custom) which have no place in
	 * an endless-ocean world.  AWaterBodyOcean + AWaterZone + WaterMeshComponent are
	 * intentionally preserved so ship buoyancy and shore lapping continue to work.
	 */
	static void DestroyInheritedWaterPluginActors(UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		// Only remove non-ocean water bodies (rivers, lakes, custom) that have no purpose here.
		// Do NOT remove WaterBodyOcean, WaterZone, or WaterMeshComponent — they are the GPU ocean.
		static const TArray<FString> RemoveSubstrings = {
			TEXT("WaterBodyRiver"),
			TEXT("WaterBodyLake"),
			TEXT("WaterBodyCustom"),
		};
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor)) { continue; }
			const FString ClassName = Actor->GetClass()->GetName();
			for (const FString& Sub : RemoveSubstrings)
			{
				if (ClassName.Contains(Sub, ESearchCase::IgnoreCase))
				{
					ToDestroy.Add(Actor);
					break;
				}
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			UE_LOG(LogIH_WB_Demo004, Log,
				TEXT("P1C12 Gate0: Removing non-ocean Water Plugin actor '%s' (%s)."),
				*GetNameSafe(Actor), *Actor->GetClass()->GetName());
			World->DestroyActor(Actor);
		}
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("P1C12 Gate0: AWaterBodyOcean + AWaterZone preserved — GPU Gerstner waves active."));
	}

	static bool IsLikelyEngineTemplateFloor(AStaticMeshActor* SMA)
	{
		if (!SMA)
		{
			return false;
		}
		UStaticMeshComponent* MC = SMA->GetStaticMeshComponent();
		UStaticMesh* SM = MC ? MC->GetStaticMesh() : nullptr;
		if (!SM)
		{
			return false;
		}
		const FString MeshName = SM->GetName();
		const FString MeshPath = SM->GetPathName();
#if WITH_EDITOR
		const FString Label = SMA->GetActorLabel();
		if (Label.Contains(TEXT("Floor"), ESearchCase::IgnoreCase))
		{
			return true;
		}
#endif
		if (MeshPath.Contains(TEXT("/Engine/Maps/Templates/"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (MeshName.Contains(TEXT("Template_Map"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		return MeshName.Equals(TEXT("Floor"), ESearchCase::IgnoreCase);
	}

	/** Template_Default.umap ships a flat floor; tank rig replaces need for it — hide + strip collision. */
	static void HideEngineTemplateFloor(UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* SMA = *It;
			if (!IsValid(SMA))
			{
				continue;
			}
			if (!IsLikelyEngineTemplateFloor(SMA))
			{
				continue;
			}
			if (UStaticMeshComponent* MC = SMA->GetStaticMeshComponent())
			{
				MC->SetHiddenInGame(true);
				MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				MC->SetCastShadow(false);
				MC->SetCastHiddenShadow(false);
				MC->SetCastInsetShadow(false);
			}
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("P1C07: Hid Template_Default-style floor actor '%s' (not needed — custom ocean plane)."),
				*GetNameSafe(SMA));
		}
	}

	/** Inner wall top Z (cm): +35 m ASL. */
	static constexpr float TankRimTopZCm = 3500.f;
	/** Canonical sea level: world Z = 0 cm (35 m below rim). */
	static constexpr float WaterSurfaceZCm = 0.f;
	/** Viewport: 1000 m above tank rim (cm). */
	static constexpr float CameraHeightAboveRimCm = 100000.f;
	static constexpr float StartCameraZCm = TankRimTopZCm + CameraHeightAboveRimCm;
	/** PIE viewport start pitch (UE: negative = look down). */
	static constexpr float StartCameraPitchDeg = -15.f;
	/** Trolley-in camera: 250 m south of inner south wall (was 1000 m). */
	static constexpr float CameraSouthOffsetBeyondTankCm = 25000.f;

	/** After water zone rebuild (~0.16s) so SLW + buoyancy see a valid surface. */
	static constexpr float BuoyantCubeSpawnDelaySec = 0.5f;
	static constexpr float StartCameraDelaySec = 0.55f;
	static constexpr float MerchantmanSpawnDelaySec = 1.55f;

	/** Dev props anchored at tank center (0, 0). */
	static constexpr float CubeCenterXCm = 0.f;
	static constexpr float CubeCenterYCm = 0.f;
	static constexpr float StoryStickCenterXCm = 0.f;
	static constexpr float StoryStickCenterYCm = 0.f;

	/** 100 m buoyant cube half-extent (cm). */
	static constexpr float BuoyantCubeHalfExtentCm = 5000.f;
	/** Static cube: bottom at -25 m ASL (partly submerged at sea level 0). */
	static constexpr float CubeBottomZCm = -2500.f;
	static constexpr float CubeCenterZCm = CubeBottomZCm + BuoyantCubeHalfExtentCm;

	/** Both Merchantmen ~200 m in front of cube (camera south: toward camera = -Y). */
	static constexpr float ShipFrontOffsetCm = 20000.f;

	/** WT-A: one-time cleanup of leftover aquarium presentation actors from older PIE sessions. */
	static void DestroyOrphanTankPresentationActors(UWorld* World)
	{
#if WITH_EDITOR
		if (!World)
		{
			return;
		}
		TArray<AStaticMeshActor*> ToDestroy;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* SMA = *It;
			if (!IsValid(SMA))
			{
				continue;
			}
			const FString Label = SMA->GetActorLabel();
			if (Label == TEXT("P1C07_TankFloor") || Label.StartsWith(TEXT("P1C07_TankWall_")))
			{
				ToDestroy.Add(SMA);
			}
		}
		for (AStaticMeshActor* SMA : ToDestroy)
		{
			World->DestroyActor(SMA, true);
		}
#endif
	}

	static void ComputePerIslandTankLayoutCm(
		int32 IslandCount,
		int32 MasterSeed,
		float RealmHalfExtentNSKm,
		float RealmHalfExtentEWKm,
		float DevLandAreaFraction,
		const TArray<FIHIslandCoastlineTuning>& PerIslandTuning,
		const TArray<float>& PerIslandLayoutExtentKm,
		const FIHIslandLayoutSolveResult* LayoutSolve,
		TArray<FVector2D>& OutCentersWorldCm,
		TArray<float>& OutSemiMajorCm,
		TArray<float>& OutBaseSemiMajorCm)
	{
		OutCentersWorldCm.Reset();
		OutSemiMajorCm.Reset();
		OutBaseSemiMajorCm.Reset();

		IslandCount = FMath::Clamp(
			IslandCount,
			IHInvisibleHandSpec::LandformCountMin,
			IHInvisibleHandSpec::LandformCountMax);
		if (IslandCount <= 0)
		{
			return;
		}

		if (LayoutSolve && LayoutSolve->bSuccess && LayoutSolve->CentersKm.Num() == IslandCount)
		{
			UIHSeedIslandLibrary::ApplyIslandLayoutSolveToTankCm(*LayoutSolve, OutCentersWorldCm, OutSemiMajorCm);
			OutBaseSemiMajorCm = OutSemiMajorCm;
			return;
		}

		const bool bUseSizeStar =
			PerIslandLayoutExtentKm.Num() == IslandCount;

		TArray<FVector2D> GridCentersCm;
		TArray<FVector2D> ScatterCentersCm;
		TArray<float> GridSemiMajorCm;
		TArray<float> ScatterSemiMajorCm;

		if (bUseSizeStar)
		{
			UIHSeedIslandLibrary::ComputeTankIslandLayoutCmWithLayoutExtents(
				IslandCount, MasterSeed, GridCentersCm, GridSemiMajorCm, RealmHalfExtentNSKm, RealmHalfExtentEWKm, 0.f,
				DevLandAreaFraction, PerIslandLayoutExtentKm);
			if (IslandCount != 3)
			{
				UIHSeedIslandLibrary::ComputeTankIslandLayoutCmWithLayoutExtents(
					IslandCount, MasterSeed, ScatterCentersCm, ScatterSemiMajorCm, RealmHalfExtentNSKm, RealmHalfExtentEWKm,
					1.f, DevLandAreaFraction, PerIslandLayoutExtentKm);
			}
		}
		else
		{
			UIHSeedIslandLibrary::ComputeTankIslandLayoutCm(
				IslandCount, MasterSeed, GridCentersCm, GridSemiMajorCm, RealmHalfExtentNSKm, RealmHalfExtentEWKm, 0.f, 1.f,
				DevLandAreaFraction);
			if (IslandCount != 3)
			{
				UIHSeedIslandLibrary::ComputeTankIslandLayoutCm(
					IslandCount, MasterSeed, ScatterCentersCm, ScatterSemiMajorCm, RealmHalfExtentNSKm, RealmHalfExtentEWKm, 1.f,
					1.f, DevLandAreaFraction);
			}
		}

		const int32 LayoutCount = FMath::Min3(IslandCount, GridCentersCm.Num(), GridSemiMajorCm.Num());
		if (LayoutCount <= 0)
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogIH_WB_Demo004, Error,
				TEXT("ComputePerIslandTankLayoutCm: empty tank layout (requested=%d gridCenters=%d gridRadii=%d)."),
				IslandCount, GridCentersCm.Num(), GridSemiMajorCm.Num());
#endif
			return;
		}

		OutCentersWorldCm.SetNum(LayoutCount);
		OutBaseSemiMajorCm.SetNum(LayoutCount);
		OutSemiMajorCm.SetNum(LayoutCount);

		for (int32 i = 0; i < LayoutCount; ++i)
		{
			const float Scatter = PerIslandTuning.IsValidIndex(i)
				? FMath::Clamp(PerIslandTuning[i].PlacementScatter, 0.f, 1.f)
				: 1.f;
			const FVector2D GridCenter = GridCentersCm.IsValidIndex(i) ? GridCentersCm[i] : FVector2D::ZeroVector;
			const FVector2D ScatterCenter = ScatterCentersCm.IsValidIndex(i) ? ScatterCentersCm[i] : GridCenter;
			OutCentersWorldCm[i] = (IslandCount == 3) ? GridCenter : FMath::Lerp(GridCenter, ScatterCenter, Scatter);

			const float BaseSemiMajor = GridSemiMajorCm.IsValidIndex(i) ? GridSemiMajorCm[i] : 30000.f;
			OutBaseSemiMajorCm[i] = BaseSemiMajor;
			// Size* layout extents already include template footprint — do not apply IslandSizeMultiplier again.
			OutSemiMajorCm[i] = BaseSemiMajor;
		}
	}
}

AIH_WB_Demo004GameMode::AIH_WB_Demo004GameMode()
{
	DefaultPawnClass = AIH_Cube2FlyPawn::StaticClass();
	PlayerControllerClass = AIH_Cube2FlyPlayerController::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AIH_WB_Demo004GameMode::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	for (AIH_WB_IslandActor* Island : SpawnedIslands)
	{
		if (IsValid(Island))
		{
			Island->TickStampRecompute(DeltaTime);
		}
	}
}

void AIH_WB_Demo004GameMode::SpawnStampGalleryForReview()
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (IsValid(StampGallery))
	{
		StampGallery->Destroy();
		StampGallery = nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	const float RealmHalfExtentNSKm = GI ? GI->GetRealmHalfExtentNSKm() : IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;
	const FVector GalleryLoc = IHInvisibleHandSpec::GetStampGalleryWorldOriginCm(RealmHalfExtentNSKm);
	StampGallery = World->SpawnActor<AIH_TerrainStampGalleryActor>(
		AIH_TerrainStampGalleryActor::StaticClass(),
		GalleryLoc,
		FRotator::ZeroRotator,
		Params);
	if (!StampGallery)
	{
		return;
	}
	StampGallery->BuildGallery(true);
	FocusPlayerOnStampGallery();
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase B2b stamp gallery spawned at (%.0f, %.0f, %.0f) north of tank wall (halfDepth=%.1f km) — camera moved to review pad"),
		GalleryLoc.X, GalleryLoc.Y, GalleryLoc.Z, RealmHalfExtentNSKm);
#endif
}

void AIH_WB_Demo004GameMode::FocusPlayerOnStampGallery() const
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World || !IsValid(StampGallery))
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("ih.StampGalleryGo: no stamp gallery spawned yet"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pawn)
	{
		return;
	}

	const FVector Focus = StampGallery->GetReviewFocusWorldLocation();
	const FVector CamLoc = Focus + FVector(
		IHInvisibleHandSpec::StampGalleryReviewCameraSideCm,
		-IHInvisibleHandSpec::StampGalleryReviewCameraBackCm,
		IHInvisibleHandSpec::StampGalleryReviewCameraUpCm);
	Pawn->SetActorLocation(CamLoc, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation((Focus - CamLoc).Rotation());
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("StampGallery camera focus=%s from=%s"),
		*Focus.ToString(),
		*CamLoc.ToString());
#endif
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand GCmdStampGallerySpawn(
	TEXT("ih.StampGallerySpawn"),
	TEXT("Spawn terrain stamp mesh gallery north of the north tank wall and move camera to the review pad."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			if (AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr)
			{
				GM->SpawnStampGalleryForReview();
			}
			else
			{
				UE_LOG(
					LogIH_WB_Demo004, Warning,
					TEXT("ih.StampGallerySpawn: no AIH_WB_Demo004GameMode on world %s"),
					World ? *World->GetName() : TEXT("(null)"));
			}
		}));

static FAutoConsoleCommand GCmdStampGalleryGo(
	TEXT("ih.StampGalleryGo"),
	TEXT("Move camera to the dev stamp gallery review pad (spawn first with ih.StampGallerySpawn)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			if (AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr)
			{
				GM->FocusPlayerOnStampGallery();
			}
			else
			{
				UE_LOG(
					LogIH_WB_Demo004, Warning,
					TEXT("ih.StampGalleryGo: no AIH_WB_Demo004GameMode on world %s"),
					World ? *World->GetName() : TEXT("(null)"));
			}
		}));

static FAutoConsoleCommand GCmdStampGalleryRefreshMeshes(
	TEXT("ih.StampGalleryRefreshMeshes"),
	TEXT("Reapply gallery preview materials (after toggling ih.StampGalleryWireframe)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			if (AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr)
			{
				if (AIH_TerrainStampGalleryActor* Gallery = GM->GetStampGalleryForReview())
				{
					Gallery->RefreshAllPreviewMeshes();
					UE_LOG(LogIH_WB_Demo004, Log, TEXT("StampGallery preview meshes refreshed"));
				}
				else
				{
					UE_LOG(LogIH_WB_Demo004, Warning, TEXT("ih.StampGalleryRefreshMeshes: gallery not spawned"));
				}
			}
		}));
#endif

void AIH_WB_Demo004GameMode::EnsureMinimalWorldForBlankMap()
{
	ConfigureTankSunLight();
}

FRotator AIH_WB_Demo004GameMode::ComputeSunRotation(float TimeOfDay, float LatitudeDeg)
{
	const float T = FMath::Clamp(TimeOfDay, 0.f, 1.f);
	constexpr float LowSunPitch = -8.f;
	constexpr float NoonPitch = -80.f;
	constexpr float SunriseYaw = 90.f;
	constexpr float NoonYaw = 0.f;
	constexpr float SunsetYaw = -90.f;

	float Pitch;
	float Yaw;
	if (T <= 0.5f)
	{
		const float Alpha = T / 0.5f;
		Pitch = FMath::Lerp(LowSunPitch, NoonPitch, Alpha);
		Yaw = FMath::Lerp(SunriseYaw, NoonYaw, Alpha);
	}
	else
	{
		const float Alpha = (T - 0.5f) / 0.5f;
		Pitch = FMath::Lerp(NoonPitch, LowSunPitch, Alpha);
		Yaw = FMath::Lerp(NoonYaw, SunsetYaw, Alpha);
	}

	// Future latitude hook: modulate Pitch/Yaw from LatitudeDeg (e.g. polar vs equatorial arc).
	(void)LatitudeDeg;
	return FRotator(Pitch, Yaw, 0.f);
}

void AIH_WB_Demo004GameMode::ApplySunFromGameInstance()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (!TankSunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			TankSunLight = *It;
			break;
		}
	}

	if (!TankSunLight)
	{
		return;
	}

	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	const float TimeOfDay = GI ? GI->GetSunTimeOfDay() : 0.22f;
	TankSunLight->SetActorRotation(ComputeSunRotation(TimeOfDay));
}

void AIH_WB_Demo004GameMode::ConfigureTankSunLight()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (!TankSunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			TankSunLight = *It;
			break;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!TankSunLight)
	{
		TankSunLight = World->SpawnActor<ADirectionalLight>(
			ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}

	if (TankSunLight)
	{
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(TankSunLight->GetLightComponent()))
		{
			C->SetIntensity(IHInvisibleHandSpec::TankSunIntensityPie);
			C->SetLightColor(FLinearColor(1.f, 0.96f, 0.88f));
			C->SetSpecularScale(0.45f);
			C->SetUseTemperature(true);
			C->SetTemperature(5600.f);
			C->SetCastShadows(true);
			C->SetMobility(EComponentMobility::Movable);
		}
		ApplySunFromGameInstance();
	}
}

void AIH_WB_Demo004GameMode::ApplyGrabContrastSunIntensity(const bool bGrabContrast)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (!TankSunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			TankSunLight = *It;
			break;
		}
	}
	if (!TankSunLight)
	{
		return;
	}
	if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(TankSunLight->GetLightComponent()))
	{
		C->SetIntensity(bGrabContrast
			? IHInvisibleHandSpec::TankSunIntensityGrabContrast
			: IHInvisibleHandSpec::TankSunIntensityPie);
	}
}

void AIH_WB_Demo004GameMode::StartPlay()
{
	AGameModeBase::StartPlay();
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	EnsureMinimalWorldForBlankMap();
	HideEngineTemplateFloor(World);
#if !UE_BUILD_SHIPPING
	IHDevViewRuntime::ApplyCloudsVisibilityToWorld(World);
#endif

	// P1C12 Arbor: clean up non-ocean Water Plugin bodies (rivers, lakes) while preserving
	// AWaterBodyOcean + AWaterZone — these deliver the GPU Gerstner waves.
	// Do No Harm: this function intentionally does NOT destroy AWaterBodyOcean.
	DestroyInheritedWaterPluginActors(World);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Gate 0 (WT-A): custom ocean plane only — WaterTankRig aquarium path retired.
	if (IHInvisibleHandSpec::IsGate0CustomOceanPlaneEnabled())
	{
		if (AIH_P1C12_OceanPlane* OceanActor = World->SpawnActor<AIH_P1C12_OceanPlane>(
				AIH_P1C12_OceanPlane::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
		{
			CustomOceanPlane = OceanActor;
			OceanActor->ConfigureEndlessSea();
			UE_LOG(LogIH_WB_Demo004, Log,
				TEXT("Gate 0: Spawned AIH_P1C12_OceanPlane (endless camera-follow Gerstner tile)."));
		}
	}
	else
	{
		UE_LOG(LogIH_WB_Demo004, Warning,
			TEXT("Gate 0: bGate0UseCustomOceanPlane is false — no ocean surface spawned (WaterTankRig retired)."));
	}

	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	const float RealmHalfExtentNSKm = GI ? GI->GetRealmHalfExtentNSKm() : IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;
	RefreshTankGeometry(RealmHalfExtentNSKm);

	if (AIH_P1C08_AltitudeStoryStickActor* Stick = World->SpawnActor<AIH_P1C08_AltitudeStoryStickActor>(
			AIH_P1C08_AltitudeStoryStickActor::StaticClass(),
			FVector(StoryStickCenterXCm, StoryStickCenterYCm, 0.f),
			FRotator::ZeroRotator,
			Params))
	{
		AltitudeStoryStick = Stick;
#if WITH_EDITOR
		Stick->SetActorLabel(TEXT("altitudeStoryStick"));
#endif
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("P1C07: Altitude story stick at (%.0f, %.0f) | top +2400 m ASL (+240000 cm)."),
			StoryStickCenterXCm, StoryStickCenterYCm);
	}

	const int32 IslandCount = GI ? GI->GetProceduralIslandCount() : 3;
	const int32 MasterSeed = GI ? GI->GetMasterSeedInt() : 0;

	SpawnIslandsFromGameInstance();

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("P1C08: Spawned %d seed islands (count=%d, masterSeed=%d, tankDepthHalf=%.1f km, totalAcres=%d) in tank."),
		SpawnedIslands.Num(), IslandCount, MasterSeed, RealmHalfExtentNSKm,
		GI ? GI->GetTotalLandAcres() : 0);

	World->GetTimerManager().SetTimer(
		BuoyantCubeSpawnTimer, this, &AIH_WB_Demo004GameMode::DeferredSpawnBuoyantCube, BuoyantCubeSpawnDelaySec, false);
	World->GetTimerManager().SetTimer(StartCameraTimer, this, &AIH_WB_Demo004GameMode::DeferredApplyStartCamera, StartCameraDelaySec, false);
	World->GetTimerManager().SetTimer(
		MerchantmanSpawnTimer, this, &AIH_WB_Demo004GameMode::DeferredSpawnMerchantmanShip, MerchantmanSpawnDelaySec, false);
}

void AIH_WB_Demo004GameMode::RegenerateIslandsFromSeed()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// SetCurrentWorldSeed (Dev Seed panel / gallery preview) already calls RebuildMapSeedPhase1;
	// skip duplicate Phase1 rebuild here to avoid double layout solve on regen.

	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UIH_P1C08_MinimapSubsystem* Minimap = GameInst->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->ClearCoastlines();
		}
	}

	DestroyAllIslandBaseDevProps();

	for (AIH_WB_IslandActor* Island : SpawnedIslands)
	{
		if (IsValid(Island))
		{
			World->DestroyActor(Island, true);
		}
	}

	SpawnedIslands.Reset();
	IslandBaseSemiMajorCm.Reset();
	SeedBaseCentersCm.Reset();
	Island01 = nullptr;

	SpawnIslandsFromGameInstance();

	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("P1C08: Regenerated %d islands in-place (seed=%s, count=%d, masterSeed=%d)."),
		SpawnedIslands.Num(),
		GI ? *GI->GetCurrentWorldSeed() : TEXT("?"),
		GI ? GI->GetProceduralIslandCount() : 0,
		GI ? GI->GetMasterSeedInt() : 0);
}

void AIH_WB_Demo004GameMode::ApplyActiveGalleryPreview()
{
	RegenerateIslandsFromSeed();
}

void AIH_WB_Demo004GameMode::SpawnIslandsFromGameInstance()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	const int32 IslandCount = GI ? GI->GetProceduralIslandCount() : 3;
	const int32 MasterSeed = GI ? GI->GetMasterSeedInt() : 0;
	const float RealmHalfExtentNSKm = GI ? GI->GetRealmHalfExtentNSKm() : IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;
	const float RealmHalfExtentEWKm = GI ? GI->GetRealmHalfExtentEWKm() : UIHSeedIslandLibrary::ComputeRealmHalfExtentEWKmFromNS(RealmHalfExtentNSKm);
	const float DevLandFraction = GI ? GI->GetAchievedEffectiveLandFraction() : IHInvisibleHandSpec::DefaultTargetEffectiveLandFraction;
	const float LayoutSolveScale = (GI && GI->GetMapSeedPhase1().LayoutSolve.bSuccess)
		? GI->GetMapSeedPhase1().LayoutSolve.UniformAreaScale
		: 1.f;
	const FIHIslandLayoutSolveResult* LayoutSolve = (GI && GI->GetMapSeedPhase1().LayoutSolve.bSuccess)
		? &GI->GetMapSeedPhase1().LayoutSolve
		: nullptr;

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("P1C08: SpawnIslands target=%.0f%% achieved=%.0f%% areaScale=%.3f | count=%d masterSeed=%d tankHalfDepth=%.1f km"),
		GI ? GI->GetTargetEffectiveLandFraction() * 100.f : 30.f,
		DevLandFraction * 100.f,
		LayoutSolveScale,
		IslandCount, MasterSeed, RealmHalfExtentNSKm);
#endif

	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* IslandNav = GameInst->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			IslandNav->AssignIslandsOnSpawn(IslandCount, MasterSeed, RealmHalfExtentNSKm, true);
		}
	}


	TArray<FIHIslandCoastlineTuning> PerIslandTuning;
	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GameInst->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			PerIslandTuning = Tuning->GetAllIslandTuning(IslandCount);
		}
	}
	if (PerIslandTuning.Num() == 0)
	{
		PerIslandTuning.SetNum(IslandCount);
		for (int32 i = 0; i < IslandCount; ++i)
		{
			PerIslandTuning[i] = FIHIslandCoastlineTuning::SeedBaseline();
		}
	}

	TArray<float> IslandAreasKm2;
	if (LayoutSolve && LayoutSolve->IslandAreasKm2.Num() == IslandCount)
	{
		IslandAreasKm2 = LayoutSolve->IslandAreasKm2;
	}
	else if (GI && GI->GetMapSeedPhase1().bSuccess)
	{
		IslandAreasKm2.SetNum(IslandCount);
		for (const FIHIslandSpawnPlan& Plan : GI->GetMapSeedPhase1().SpawnPlans)
		{
			if (Plan.IslandIndex >= 0 && Plan.IslandIndex < IslandCount)
			{
				IslandAreasKm2[Plan.IslandIndex] = Plan.AreaBudgetKm2;
			}
		}
	}
	else
	{
		UIHSeedIslandLibrary::ComputeIslandAreasKM2FromRealm(
			RealmHalfExtentNSKm, RealmHalfExtentEWKm, IslandCount, IslandAreasKm2, DevLandFraction);
	}

	TArray<float> SummitTopZCm;
	UIHSeedIslandLibrary::ComputeSummitTopZCmForAreas(IslandAreasKm2, SummitTopZCm);

	TArray<float> LayoutExtentKm;
	if (GI && GI->GetMapSeedPhase1().bSuccess)
	{
		UIHMapSeedFrameworkLibrary::GatherLayoutCoastExtentsKmFromPhase1(GI->GetMapSeedPhase1(), LayoutExtentKm);
	}

	TArray<FVector2D> IslandCentersCm;
	TArray<float> IslandSemiMajorCm;
	TArray<float> BaseSemiMajorCm;
	ComputePerIslandTankLayoutCm(
		IslandCount, MasterSeed, RealmHalfExtentNSKm, RealmHalfExtentEWKm, DevLandFraction, PerIslandTuning, LayoutExtentKm,
		LayoutSolve,
		IslandCentersCm, IslandSemiMajorCm, BaseSemiMajorCm);
	IslandBaseSemiMajorCm = BaseSemiMajorCm;
	SeedBaseCentersCm = IslandCentersCm;

	TArray<FIHIslandManualTransform> ManualTransforms;
	ManualTransforms.SetNumZeroed(IslandCount);

	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UIH_P1C08_MinimapSubsystem* Minimap = GameInst->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->ClearCoastlines();
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (IHInvisibleHandSpec::bDevDemo_AslContourDeferRibbonUntilSpawnBatch)
	{
		AIH_WB_IslandActor::SetAslContourRibbonBakeDeferred(true);
	}

	for (int32 IslandIdx = 0; IslandIdx < IslandCentersCm.Num(); ++IslandIdx)
	{
		const FIHIslandManualTransform Manual = ManualTransforms.IsValidIndex(IslandIdx)
			? ManualTransforms[IslandIdx]
			: FIHIslandManualTransform();
		const FVector2D SeedCenter = IslandCentersCm[IslandIdx];
		const FVector2D WorldXY = SeedCenter + Manual.OffsetXYCm;
		const FVector IslandLoc(WorldXY.X, WorldXY.Y, 0.f);
		const float SemiMajorCm = IslandSemiMajorCm.IsValidIndex(IslandIdx) ? IslandSemiMajorCm[IslandIdx] : 30000.f;
		const float AreaKm2 = IslandAreasKm2.IsValidIndex(IslandIdx) ? IslandAreasKm2[IslandIdx] : 1.f;
		EIHIslandProfile Profile = IHPickIslandProfile321(MasterSeed, IslandIdx);
		if (GI && GI->GetMapSeedPhase1().bSuccess && GI->GetMapSeedPhase1().SpawnPlans.IsValidIndex(IslandIdx))
		{
			switch (GI->GetMapSeedPhase1().SpawnPlans[IslandIdx].TemplateType)
			{
			case EIHIslandTemplateType::High: Profile = EIHIslandProfile::High; break;
			case EIHIslandTemplateType::Volcanic: Profile = EIHIslandProfile::Volc; break;
			default: Profile = EIHIslandProfile::Low; break;
			}
		}

		const float OrganicSummitM = IHComputeTargetSummitMeters(AreaKm2, Profile);
		const float SummitZCm = OrganicSummitM * 100.f;

		AIH_WB_IslandActor* Island = World->SpawnActor<AIH_WB_IslandActor>(
			AIH_WB_IslandActor::StaticClass(),
			IslandLoc,
			FRotator(0.f, Manual.YawDeg, 0.f),
			Params);
		if (!Island)
		{
			continue;
		}

		Island->ApplyTankLayout(IslandIdx, SemiMajorCm, SummitZCm, AreaKm2, MasterSeed, Profile);
		SpawnedIslands.Add(Island);

#if WITH_EDITOR
		Island->SetActorLabel(FString::Printf(TEXT("Island_%d"), IslandIdx));
#endif
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("P1C10: Island %d at (%.0f, %.0f) | semi-major %.0f cm | summit +%.0f m | coastPts=%d"),
			IslandIdx, IslandLoc.X, IslandLoc.Y, SemiMajorCm, SummitZCm / 100.f,
			Island->GetMainCoastPolylineLocalCm().Num());
	}

	CompactIslandsTowardStoryStickOrigin();

	if (IHInvisibleHandSpec::bDevDemo_AslContourDeferRibbonUntilSpawnBatch)
	{
		const double RibbonBatchT0 = FPlatformTime::Seconds();
		AIH_WB_IslandActor::SetAslContourRibbonBakeDeferred(false);
		for (const TObjectPtr<AIH_WB_IslandActor>& Island : SpawnedIslands)
		{
			if (IsValid(Island))
			{
				Island->FlushDeferredAslContourRibbonBake();
			}
		}
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase DEV-WWF aslRibbonBatchFlush islands=%d sec=%.2f"),
			SpawnedIslands.Num(),
			FPlatformTime::Seconds() - RibbonBatchT0);
#endif
	}

	if (SpawnedIslands.Num() > 0)
	{
		Island01 = SpawnedIslands[0];
	}

	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GameInst->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			Tuning->SyncActiveFromIsland(INDEX_NONE);
		}
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() {
		for (const TObjectPtr<AIH_WB_IslandActor>& Island : SpawnedIslands)
		{
			if (IsValid(Island))
			{
				Island->RefreshMinimapCoastline();
			}
		}
	}));

	SpawnIslandBaseDevPropsForSpawnedIslands();
}

void AIH_WB_Demo004GameMode::CompactIslandsTowardStoryStickOrigin()
{
	// IH-DEC-033: no canon governed inter-island spacing until this - the layout solver's
	// CompactLayoutPatternFillFactor only sizes each island's a-priori ALLOTTED envelope, not its
	// actual realized footprint (typically much smaller, per this session's own logs). This pass
	// recovers that slack: pull every island toward the Story Stick origin (world center) by one
	// uniform scale factor, small enough that every island pair's actual coastline gap still
	// clears the 1.5km floor.
	constexpr double MinInterIslandGapCm = 150000.0; // 1.5 km

	if (SpawnedIslands.Num() < 2)
	{
		return;
	}

	struct FIslandCompactionInfo
	{
		AIH_WB_IslandActor* Island = nullptr;
		FVector2D CenterXY = FVector2D::ZeroVector;
		double RadiusCm = 0.0;
	};

	TArray<FIslandCompactionInfo> Infos;
	Infos.Reserve(SpawnedIslands.Num());
	for (const TObjectPtr<AIH_WB_IslandActor>& IslandPtr : SpawnedIslands)
	{
		AIH_WB_IslandActor* Island = IslandPtr.Get();
		if (!IsValid(Island))
		{
			continue;
		}
		const FVector Loc = Island->GetActorLocation();
		Infos.Add(FIslandCompactionInfo{Island, FVector2D(Loc.X, Loc.Y), static_cast<double>(Island->GetMainLandFootprintRadiusCm())});
	}

	if (Infos.Num() < 2)
	{
		return;
	}

	// Closed-form: for every pair, the scale factor that would bring that pair's gap down to
	// exactly the floor is (Floor + Ri + Rj) / Dist(Ci, Cj). Every pair has a different distance,
	// so the pair that becomes critical under scaling isn't necessarily the one with the smallest
	// gap today - check all pairs (trivially cheap at 2-7 islands) rather than assume.
	//
	// Bug fixed here (Plan Addendum 17): ScaleFactor was previously seeded at 1.0 before this
	// loop, which made it mathematically impossible for the pass to ever compact anything -
	// min(max(1.0, x1, x2, ...), 1.0) equals 1.0 for ANY input, so the log always read
	// "scale=1.00" regardless of actual island spacing. Confirmed via 6-seed headless test
	// (ABBEY3/POKED2/ALERT4/JUNKS5/MAILS6/GIZMO7) before this fix, all identically scale=1.00.
	// Seed from the first real ratio instead so the max-loop can find the true tightest-pair
	// requirement; the final Min(...,1.0) clamp still (correctly) prevents ever spreading
	// islands further apart than their current a-priori layout positions.
	double ScaleFactor = 0.0;
	bool bAnyPair = false;
	for (int32 i = 0; i < Infos.Num(); ++i)
	{
		for (int32 j = i + 1; j < Infos.Num(); ++j)
		{
			const double DistCm = FVector2D::Distance(Infos[i].CenterXY, Infos[j].CenterXY);
			if (DistCm <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const double RequiredScale = (MinInterIslandGapCm + Infos[i].RadiusCm + Infos[j].RadiusCm) / DistCm;
			ScaleFactor = FMath::Max(ScaleFactor, RequiredScale);
			bAnyPair = true;
		}
	}
	if (!bAnyPair)
	{
		return;
	}
	ScaleFactor = FMath::Min(ScaleFactor, 1.0); // never spread islands further apart than today

	if (ScaleFactor >= 1.0 - KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("IH-DEC-033 compaction: scale=1.00 (already at or tighter than the 1.5km floor, no compaction applied)"));
		return;
	}

	for (const FIslandCompactionInfo& Info : Infos)
	{
		const FVector2D NewCenterXY = Info.CenterXY * ScaleFactor; // Story Stick origin = world (0,0)
		const FVector OldLoc = Info.Island->GetActorLocation();
		Info.Island->SetActorLocation(FVector(NewCenterXY.X, NewCenterXY.Y, OldLoc.Z));
		Info.Island->RefreshMinimapCoastline();
	}

	UE_LOG(LogIH_WB_Demo004, Log, TEXT("IH-DEC-033 compaction: scale=%.4f applied across %d islands"), ScaleFactor, Infos.Num());
}

AIH_WB_IslandActor* AIH_WB_Demo004GameMode::GetSpawnedIsland(int32 IslandIndex) const
{
	return SpawnedIslands.IsValidIndex(IslandIndex) ? SpawnedIslands[IslandIndex].Get() : nullptr;
}

void AIH_WB_Demo004GameMode::DestroyAllIslandBaseDevProps()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (AIH_P1C10_IslandBaseDevPropActor* Prop : SpawnedIslandBaseDevProps)
	{
		if (IsValid(Prop))
		{
			World->DestroyActor(Prop, true);
		}
	}
	SpawnedIslandBaseDevProps.Reset();
}

void AIH_WB_Demo004GameMode::SpawnIslandBaseDevPropsForSpawnedIslands()
{
	if (!IHInvisibleHandSpec::IsIslandBaseDevPropEnabled())
	{
		DestroyAllIslandBaseDevProps();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	DestroyAllIslandBaseDevProps();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 IslandIdx = 0; IslandIdx < SpawnedIslands.Num(); ++IslandIdx)
	{
		AIH_WB_IslandActor* Island = SpawnedIslands[IslandIdx].Get();
		if (!IsValid(Island))
		{
			continue;
		}

		const TArray<FVector2D>& MainCoastLocalCm = Island->GetMainCoastPolylineLocalCm();
		if (MainCoastLocalCm.Num() < 3)
		{
			continue;
		}

		AIH_P1C10_IslandBaseDevPropActor* Prop = World->SpawnActor<AIH_P1C10_IslandBaseDevPropActor>(
			AIH_P1C10_IslandBaseDevPropActor::StaticClass(),
			Island->GetActorLocation(),
			Island->GetActorRotation(),
			Params);
		if (!Prop)
		{
			continue;
		}

		Prop->InitializeForIsland(Island);
		Prop->SyncTransformFromIsland(Island);
#if WITH_EDITOR
		Prop->SetActorLabel(FString::Printf(TEXT("Island_%d_BaseDevProp"), IslandIdx));
#endif
		SpawnedIslandBaseDevProps.Add(Prop);
	}
}

FVector2D AIH_WB_Demo004GameMode::GetSeedBaseCenterCm(int32 IslandIndex) const
{
	return SeedBaseCentersCm.IsValidIndex(IslandIndex) ? SeedBaseCentersCm[IslandIndex] : FVector2D::ZeroVector;
}

void AIH_WB_Demo004GameMode::ApplyIslandManualTransform(
	int32 IslandIndex, const FIHIslandManualTransform& Transform, bool bRefreshMinimap)
{
	AIH_WB_IslandActor* Island = GetSpawnedIsland(IslandIndex);
	if (!Island)
	{
		return;
	}

	const FVector2D WorldXY = GetSeedBaseCenterCm(IslandIndex) + Transform.OffsetXYCm;
	const FVector NewLoc(WorldXY.X, WorldXY.Y, 0.f);
	const FRotator NewRot(0.f, Transform.YawDeg, 0.f);

	Island->SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);
	if (bRefreshMinimap)
	{
		Island->RefreshMinimapCoastline();
	}
	else
	{
		Island->UpdateMinimapCoastlineTransformOnly();
	}

	if (SpawnedIslandBaseDevProps.IsValidIndex(IslandIndex))
	{
		if (AIH_P1C10_IslandBaseDevPropActor* Prop = SpawnedIslandBaseDevProps[IslandIndex].Get())
		{
			Prop->SyncTransformFromIsland(Island);
		}
	}
}

void AIH_WB_Demo004GameMode::RegenerateSingleIsland(int32 IslandIndex)
{
	AIH_WB_IslandActor* Island = GetSpawnedIsland(IslandIndex);
	if (!Island)
	{
		return;
	}
	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	const int32 MasterSeed = GI ? GI->GetMasterSeedInt() : 0;
	Island->ApplyTankLayout(
		IslandIndex,
		Island->GetSemiMajorAxisCm(),
		Island->GetSummitTopZCm(),
		1.f,
		MasterSeed);
	Island->RefreshMinimapCoastline();

	if (IHInvisibleHandSpec::IsIslandBaseDevPropEnabled())
	{
		if (SpawnedIslandBaseDevProps.IsValidIndex(IslandIndex))
		{
			if (AIH_P1C10_IslandBaseDevPropActor* Prop = SpawnedIslandBaseDevProps[IslandIndex].Get())
			{
				Prop->InitializeForIsland(Island);
				Prop->SyncTransformFromIsland(Island);
			}
		}
		else
		{
			SpawnIslandBaseDevPropsForSpawnedIslands();
		}
	}
}
void AIH_WB_Demo004GameMode::DeferredSpawnMerchantmanShip()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	const float WaterlineOffsetCm = GetDefault<AIH_P1C07_MerchantmanShipActor>()->DefaultWaterlineOffsetZCm;
	const FVector CubeLoc = BuoyantCube
		? BuoyantCube->GetActorLocation()
		: FVector(CubeCenterXCm, CubeCenterYCm, CubeCenterZCm);
	const FVector ShipLoc(
		CubeLoc.X,
		CubeLoc.Y - ShipFrontOffsetCm,
		WaterSurfaceZCm + WaterlineOffsetCm);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AIH_P1C07_MerchantmanShipActor* Ship = World->SpawnActor<AIH_P1C07_MerchantmanShipActor>(
			AIH_P1C07_MerchantmanShipActor::StaticClass(), ShipLoc, FRotator::ZeroRotator, Params))
	{
		MerchantmanShip = Ship;
#if WITH_EDITOR
		Ship->SetActorLabel(TEXT("P1C07_Merchantman"));
#endif
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("P1C08: Merchantman spawned 200 m in front of cube at (%.0f,%.0f,%.0f) cm | cube (%.0f,%.0f) water Z=%.0f."),
			ShipLoc.X, ShipLoc.Y, ShipLoc.Z, CubeLoc.X, CubeLoc.Y, WaterSurfaceZCm);

		// Second hull for multi-select / formation testing (smaller, tinted).
		const FVector Ship2Loc = ShipLoc + FVector(12000.f, 0.f, 0.f);
		if (AIH_P1C07_MerchantmanShipActor* Ship2 = World->SpawnActor<AIH_P1C07_MerchantmanShipActor>(
				AIH_P1C07_MerchantmanShipActor::StaticClass(), Ship2Loc, FRotator(0.f, 45.f, 0.f), Params))
		{
			Ship2->ApplyShipAppearance(0.75f, FVector(1.f, 1.f, 0.95f), FLinearColor(0.55f, 0.72f, 0.95f));
#if WITH_EDITOR
			Ship2->SetActorLabel(TEXT("P1C07_Merchantman_B"));
#endif
		}
	}
}

void AIH_WB_Demo004GameMode::DeferredSpawnBuoyantCube()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || BuoyantCube)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AIH_P1C07_BuoyantCubeActor* Cube = World->SpawnActor<AIH_P1C07_BuoyantCubeActor>(
			AIH_P1C07_BuoyantCubeActor::StaticClass(),
			FVector(CubeCenterXCm, CubeCenterYCm, CubeCenterZCm),
			FRotator::ZeroRotator,
			Params))
	{
		BuoyantCube = Cube;
		Cube->SetActorRotation(FRotator::ZeroRotator);
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Cube->GetRootComponent()))
		{
			RootPrim->SetSimulatePhysics(false);
		}
		Cube->SetActorTickEnabled(false);
#if WITH_EDITOR
		Cube->SetActorLabel(TEXT("P1C07_100m_BuoyantCube"));
#endif
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("P1C07: Static red cube at (%.0f, %.0f, %.0f) cm | bottom Z=%.0f cm (−25 m ASL)."),
			CubeCenterXCm, CubeCenterYCm, CubeCenterZCm, CubeBottomZCm);
	}
}

void AIH_WB_Demo004GameMode::DeferredApplyStartCamera()
{
	ApplyStartCamera();
}

void AIH_WB_Demo004GameMode::ApplyStartCamera() const
{
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		const float RealmHalfExtentNSCm = UIHSeedIslandLibrary::GetRealmHalfExtentNSCm(GI->GetRealmHalfExtentNSKm());
		const float CameraY = -(RealmHalfExtentNSCm + CameraSouthOffsetBeyondTankCm);
		const FVector CamLoc(0.f, CameraY, StartCameraZCm);
		const FVector LookAt(0.f, 0.f, WaterSurfaceZCm);
		FRotator ControlRot = UKismetMathLibrary::FindLookAtRotation(CamLoc, LookAt);
		ControlRot.Pitch = StartCameraPitchDeg;
		if (UWorld* W = GetWorld())
		{
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (APawn* Pawn = PC->GetPawn())
					{
						Pawn->SetActorLocation(CamLoc, false, nullptr, ETeleportType::TeleportPhysics);
						Pawn->SetActorRotation(ControlRot);
						if (UFloatingPawnMovement* Mov = Pawn->FindComponentByClass<UFloatingPawnMovement>())
						{
							Mov->StopMovementImmediately();
						}
						PC->SetControlRotation(ControlRot);
					}
				}
			}
		}
	}
}

void AIH_WB_Demo004GameMode::RefreshTankGeometry(float RealmHalfExtentNSKm)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	(void)RealmHalfExtentNSKm;

	// WT-A: destroy leftover aquarium walls/floor; do not spawn tank presentation geometry.
	DestroyOrphanTankPresentationActors(World);
	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("Gate 0: Orphan tank walls/floor cleaned up (WT-A aquarium retirement)."));

	ApplyStartCamera();
}

void AIH_WB_Demo004GameMode::ApplyTankPresetAndRegenerate(float NewRealmHalfExtentNSKm)
{
	// WT-A thin wrapper: GI half-extent only (regen/rename land in later WT phases).
	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		GI->SetRealmHalfExtentNSKm(NewRealmHalfExtentNSKm);
	}
}

#if !UE_BUILD_SHIPPING
// WB-phase dev tool only (per user request 2026-08-12) - a true top-down grab angle for
// judging cell-graph/coastline shape. Deliberately does NOT touch the canonical fly-camera
// pitch/approach-distance clamps in IH_Cube2FlyPlayerController (IH-DEC-014) - this is a
// one-shot teleport+snap using the same SetActorLocation/SetControlRotation pattern already
// proven by FocusPlayerOnStampGallery() above, not a change to how the camera behaves under
// normal input. Revisit/restrict for actual gameplay in IH GA per the user's own note.
static FAutoConsoleCommandWithWorldAndArgs GCmdCameraTopDown(
	TEXT("ih.CameraTopDown"),
	TEXT("WB dev tool: snaps the camera to look straight down from the given ASL altitude "
		"(meters, default 300) at its current XY position. Usage: ih.CameraTopDown [altitudeMeters]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!PC || !Pawn)
			{
				UE_LOG(LogIH_WB_Demo004, Warning, TEXT("ih.CameraTopDown: no player controller/pawn"));
				return;
			}

			const float AltitudeMeters = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 300.f;
			FVector Loc = Pawn->GetActorLocation();
			Loc.Z = AltitudeMeters * 100.f;
			Pawn->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
			// Plan Addendum 14: Yaw=90 (not 0) so screen-up matches the minimap's documented
			// +Y=north=up convention (FView::WorldToMapLocal) - confirmed via UE rotation-matrix
			// math (Pitch=-90,Yaw=90 => Up=(0,1,0)=+Y).
			PC->SetControlRotation(FRotator(-90.f, 90.f, 0.f));

			UE_LOG(LogIH_WB_Demo004, Log,
				TEXT("ih.CameraTopDown: snapped to (%.0f, %.0f, %.0f) cm, pitch=-90 yaw=90"),
				Loc.X, Loc.Y, Loc.Z);
		}));
#endif
