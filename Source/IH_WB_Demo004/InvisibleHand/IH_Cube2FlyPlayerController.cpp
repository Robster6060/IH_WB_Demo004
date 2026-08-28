// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_Cube2FlyPlayerController.h"
#include "IH_P1C07_SelectableShip.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IH_P1C07_ShipRegistrySubsystem.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_P1C07_NavAvoidanceSubsystem.h"
#include "IH_P1C08_MinimapSubsystem.h"
#include "IH_P1C08_MinimapCoastline.h"
#include "IH_BuildPaletteSubsystem.h"
#include "IH_TerrainStampActor.h"
#include "IH_StructurePlacementActor.h"
#include "IH_BuildPaletteHostWidget.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridManager.h"
#include "IH_P1C08_MinimapWidget.h"
#include "IH_P1C08_CoastlineTuningWidget.h"
#include "IH_P1C07_SelectionLassoWidget.h"
#include "IH_P1C08_GameSpeedWidget.h"
#include "IH_P1C08_DevViewWidget.h"
#include "IH_P1C08_CameraAslWidget.h"
#include "IH_P1C08_PlaceShipWidget.h"
#include "IH_P1C08_MannequinWidget.h"
#include "IH_P1C08_MannequinActor.h"
#include "Components/CapsuleComponent.h"
#include "IH_P1C08_TopDownViewWidget.h"
#include "IH_P1C07_MerchantmanShipActor.h"
#include "IH_P1C08_WeatherPreviewWidget.h"
#include "IH_P1C08_GameDateTimeWidget.h"
#include "IH_P1C08_PlayAtmosphericsWidget.h"
#include "IH_P1C08_DevSeedPanelWidget.h"
#include "IH_P1C08_TemplateGalleryWidget.h"
#include "IH_P1C08_IslandNavWidget.h"
#include "IH_P1C08_IslandNavSubsystem.h"
#include "IH_P1C08_IslandCaptionWidget.h"
#include "IH_P1C08_ConfirmRevertWidget.h"
#include "IH_P1C08_IslandEditHintWidget.h"
#include "IH_P1C08_RealmRegenProgressWidget.h"
#include "IH_P1C08_CoastlineTuningSubsystem.h"
#include "IHSeedIslandLibrary.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IHInvisibleHandDesignSpec.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "IH_WB_Demo004GameMode.h"
#include "IH_WB_IslandActor.h"
#include "IH_P1C07_WaterQueryHelpers.h"
#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "WaterBodyActor.h"
#include "Widgets/SViewport.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winuser.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace IH_Cube2FlyPlayerControllerPrivate
{
	static TAutoConsoleVariable<int32> CVarDevDrawMousePointerEcho(
		TEXT("ih.Dev.DrawMousePointerEcho"),
		0,
		TEXT("When 1, draw dev mouse pointer echo spheres/lines (off by default; click bursts always draw)."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarDevShowCoastTuning(
		TEXT("ih.Dev.ShowCoastTuning"),
		0,
		TEXT("When 1, show legacy relief panel (summit altitude only; coast shape is seed-driven height field)."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarDevShowTemplateGallery(
		TEXT("ih.Dev.ShowTemplateGallery"),
		0,
		TEXT("When 1, show Template Gallery review panel (superseded by in-game seed regen)."),
		ECVF_Default);

#if PLATFORM_WINDOWS
	static bool IsPhysicalKeyDownWin32(int32 VirtualKey)
	{
		return (GetAsyncKeyState(VirtualKey) & 0x8000) != 0;
	}
#endif

	static int32 GetVirtualKeyForGlobalHUDKey(FKey Key)
	{
		if (Key == EKeys::M)
		{
			return 0x4D;
		}
		if (Key == EKeys::P)
		{
			return 0x50;
		}
		if (Key == EKeys::L)
		{
			return 0x4C;
		}
		if (Key == EKeys::G)
		{
			return 0x47;
		}
		if (Key == EKeys::W)
		{
			return 0x57;
		}
		if (Key == EKeys::B)
		{
			return 0x42;
		}
		if (Key == EKeys::C)
		{
			return 0x43;
		}
		if (Key == EKeys::D)
		{
			return 0x44;
		}
		if (Key == EKeys::E)
		{
			return 0x45;
		}
		if (Key == EKeys::Q)
		{
			return 0x51;
		}
		if (Key == EKeys::Up)
		{
			return 0x26;
		}
		if (Key == EKeys::Down)
		{
			return 0x28;
		}
		if (Key == EKeys::Left)
		{
			return 0x25;
		}
		if (Key == EKeys::Right)
		{
			return 0x27;
		}
		if (Key == EKeys::PageUp)
		{
			return 0x21;
		}
		if (Key == EKeys::PageDown)
		{
			return 0x22;
		}
		if (Key == EKeys::SpaceBar)
		{
			return 0x20;
		}
		return 0;
	}

	static bool IsFlyKeyDownAnywhere(AIH_Cube2FlyPlayerController* PC, FKey Key)
	{
		if (!PC)
		{
			return false;
		}
		if (PC->IsInputKeyDown(Key))
		{
			return true;
		}
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
			{
				if (const FViewport* Viewport = ViewportClient->Viewport)
				{
					if (Viewport->KeyState(Key))
					{
						return true;
					}
				}
			}
		}
#if PLATFORM_WINDOWS
		if (const int32 VirtualKey = GetVirtualKeyForGlobalHUDKey(Key))
		{
			if (IsPhysicalKeyDownWin32(VirtualKey))
			{
				return true;
			}
		}
#endif
		return false;
	}
}

void AIH_Cube2FlyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureViewportKeyboardFocus();

	if (IH_Cube2FlyPlayerControllerPrivate::CVarDevShowCoastTuning.GetValueOnGameThread() != 0)
	{
		if (UIH_P1C08_CoastlineTuningWidget* CoastWidget =
				CreateWidget<UIH_P1C08_CoastlineTuningWidget>(this, UIH_P1C08_CoastlineTuningWidget::StaticClass()))
		{
			CoastWidget->SetIsFocusable(false);
			CoastWidget->AddToViewport(4);
			CoastlineTuningWidget = CoastWidget;
		}
	}
	if (UIH_P1C08_GameSpeedWidget* SpeedWidget =
			CreateWidget<UIH_P1C08_GameSpeedWidget>(this, UIH_P1C08_GameSpeedWidget::StaticClass()))
	{
		SpeedWidget->SetIsFocusable(false);
		SpeedWidget->AddToViewport(4);
		GameSpeedWidget = SpeedWidget;
	}
#if !UE_BUILD_SHIPPING
	if (UIH_P1C08_DevViewWidget* ViewWidget =
			CreateWidget<UIH_P1C08_DevViewWidget>(this, UIH_P1C08_DevViewWidget::StaticClass()))
	{
		ViewWidget->SetIsFocusable(false);
		ViewWidget->AddToViewport(4);
		DevViewWidget = ViewWidget;
	}
	if (UIH_P1C08_PlaceShipWidget* PlaceWidget =
			CreateWidget<UIH_P1C08_PlaceShipWidget>(this, UIH_P1C08_PlaceShipWidget::StaticClass()))
	{
		PlaceWidget->SetIsFocusable(false);
		PlaceWidget->AddToViewport(4);
		PlaceShipWidget = PlaceWidget;
	}
	if (UIH_P1C08_MannequinWidget* MannequinW =
			CreateWidget<UIH_P1C08_MannequinWidget>(this, UIH_P1C08_MannequinWidget::StaticClass()))
	{
		MannequinW->SetIsFocusable(false);
		MannequinW->AddToViewport(4);
		MannequinWidget = MannequinW;
	}
	if (UIH_P1C08_TopDownViewWidget* TopDownWidget =
			CreateWidget<UIH_P1C08_TopDownViewWidget>(this, UIH_P1C08_TopDownViewWidget::StaticClass()))
	{
		TopDownWidget->SetIsFocusable(false);
		TopDownWidget->AddToViewport(4);
		TopDownViewWidget = TopDownWidget;
	}
#endif
	if (UIH_P1C08_CameraAslWidget* AslWidget =
			CreateWidget<UIH_P1C08_CameraAslWidget>(this, UIH_P1C08_CameraAslWidget::StaticClass()))
	{
		AslWidget->SetIsFocusable(false);
		AslWidget->AddToViewport(4);
		CameraAslWidget = AslWidget;
	}
	if (UIH_P1C08_WeatherPreviewWidget* WeatherWidget =
			CreateWidget<UIH_P1C08_WeatherPreviewWidget>(this, UIH_P1C08_WeatherPreviewWidget::StaticClass()))
	{
		WeatherWidget->SetIsFocusable(false);
		WeatherWidget->AddToViewport(4);
		WeatherWidget->SetPanelVisible(true);
		WeatherPreviewWidget = WeatherWidget;
	}
	if (UIH_P1C08_GameDateTimeWidget* DateTimeWidget =
			CreateWidget<UIH_P1C08_GameDateTimeWidget>(this, UIH_P1C08_GameDateTimeWidget::StaticClass()))
	{
		DateTimeWidget->SetIsFocusable(false);
		DateTimeWidget->AddToViewport(4);
		DateTimeWidget->SetPanelVisible(true);
		GameDateTimeWidget = DateTimeWidget;
	}
	if (UIH_P1C08_PlayAtmosphericsWidget* AtmosphericsWidget =
			CreateWidget<UIH_P1C08_PlayAtmosphericsWidget>(this, UIH_P1C08_PlayAtmosphericsWidget::StaticClass()))
	{
		AtmosphericsWidget->SetIsFocusable(false);
		AtmosphericsWidget->AddToViewport(4);
		AtmosphericsWidget->SetPanelVisible(true);
		AtmosphericsWidget->SetDateWidget(GameDateTimeWidget);
		PlayAtmosphericsWidget = AtmosphericsWidget;
	}
	if (UWorld* World = GetWorld())
	{
		if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Editor)
		{
			if (UIH_P1C08_DevSeedPanelWidget* SeedPanel =
					CreateWidget<UIH_P1C08_DevSeedPanelWidget>(this, UIH_P1C08_DevSeedPanelWidget::StaticClass()))
			{
				SeedPanel->SetIsFocusable(false);
				SeedPanel->AddToViewport(10);
				DevSeedPanelWidget = SeedPanel;
			}
		}
	}
	if (UIH_P1C08_IslandNavWidget* NavWidget =
			CreateWidget<UIH_P1C08_IslandNavWidget>(this, UIH_P1C08_IslandNavWidget::StaticClass()))
	{
		NavWidget->SetIsFocusable(false);
		NavWidget->AddToViewport(4);
		IslandNavWidget = NavWidget;
	}
	if (UWorld* GalleryWorld = GetWorld())
	{
		if (GalleryWorld->WorldType == EWorldType::PIE || GalleryWorld->WorldType == EWorldType::Editor)
		{
			if (IH_Cube2FlyPlayerControllerPrivate::CVarDevShowTemplateGallery.GetValueOnGameThread() != 0)
			{
				if (UIH_P1C08_TemplateGalleryWidget* GalleryWidget =
						CreateWidget<UIH_P1C08_TemplateGalleryWidget>(this, UIH_P1C08_TemplateGalleryWidget::StaticClass()))
				{
					GalleryWidget->SetIsFocusable(false);
					GalleryWidget->AddToViewport(4);
					TemplateGalleryWidget = GalleryWidget;
				}
			}
		}
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			IslandNavChangedHandle = Nav->OnIslandNavChanged.AddUObject(
				this, &AIH_Cube2FlyPlayerController::RefreshDevPanelStackLayout);
			IslandSelectionChangedHandle = Nav->OnSelectionChanged.AddUObject(
				this, &AIH_Cube2FlyPlayerController::HandleIslandSelectionChanged);
		}
	}
	if (UIH_P1C08_IslandCaptionWidget* CaptionWidget =
			CreateWidget<UIH_P1C08_IslandCaptionWidget>(this, UIH_P1C08_IslandCaptionWidget::StaticClass()))
	{
		CaptionWidget->SetIsFocusable(false);
		CaptionWidget->AddToViewport(15);
		IslandCaptionWidget = CaptionWidget;
	}
	if (UIH_P1C08_ConfirmRevertWidget* ConfirmWidget =
			CreateWidget<UIH_P1C08_ConfirmRevertWidget>(this, UIH_P1C08_ConfirmRevertWidget::StaticClass()))
	{
		ConfirmWidget->SetIsFocusable(false);
		ConfirmWidget->AddToViewport(50);
		ConfirmWidget->HideDialog();
		ConfirmRevertWidget = ConfirmWidget;
	}
	if (UIH_P1C08_IslandEditHintWidget* HintWidget =
			CreateWidget<UIH_P1C08_IslandEditHintWidget>(this, UIH_P1C08_IslandEditHintWidget::StaticClass()))
	{
		HintWidget->SetIsFocusable(false);
		HintWidget->AddToViewport(6);
		if (UWorld* World = GetWorld())
		{
			if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Editor)
			{
				HintWidget->SetPieDevHint();
			}
		}
		IslandEditHintWidget = HintWidget;
	}
	if (UIH_P1C08_RealmRegenProgressWidget* ProgressWidget =
			CreateWidget<UIH_P1C08_RealmRegenProgressWidget>(this, UIH_P1C08_RealmRegenProgressWidget::StaticClass()))
	{
		ProgressWidget->SetIsFocusable(false);
		ProgressWidget->AddToViewport(1000);
		ProgressWidget->CompleteAndHide();
		RealmRegenProgressWidget = ProgressWidget;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			ManualTransformChangedHandle = Tuning->OnManualTransformChanged.AddUObject(
				this, &AIH_Cube2FlyPlayerController::HandleManualTransformChanged);
		}
	}
	if (UWorld* LayoutWorld = GetWorld())
	{
		LayoutWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() {
			RefreshDevPanelStackLayout();
		}));
	}
	LassoWidget = CreateWidget<UIH_P1C07_SelectionLassoWidget>(this, UIH_P1C07_SelectionLassoWidget::StaticClass());
	if (LassoWidget)
	{
		LassoWidget->SetIsFocusable(false);
		LassoWidget->SetDragRect(FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		LassoWidget->AddToViewport(20);
	}
	ApplyPresentationInputMode();
	KeyboardFocusWarmupTicksRemaining = 120;
	MouseCaptureWarmupTicksRemaining = 120;
	if (UWorld* World = GetWorld())
	{
		// HUD AddToViewport can steal Slate focus/capture; restore free presentation input after widgets mount.
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() {
			ApplyPresentationInputMode();
		}));
	}
	TryGetViewportMousePosition(PrevMousePixels);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->PrepareMinimapWidget(this);
		}
		if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			BuildPalette->PrepareBuildPaletteWidget(this);
		}
	}
}

void AIH_Cube2FlyPlayerController::RefreshDevPanelStackLayout()
{
	int32 IslandCount = 3;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		IslandCount = GI->GetProceduralIslandCount();
	}

	float StackY = IH_P1C08_DevPanelStyle::TopMargin;

	if (DevSeedPanelWidget && DevSeedPanelWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float ContentHeight = IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::RealmSeed, IslandCount);
		DevSeedPanelWidget->ApplyDevPanelStackPosition(StackY, ContentHeight);
		StackY += ContentHeight + IH_P1C08_DevPanelStyle::PanelSpacing;
	}

	if (IslandNavWidget && IslandNavWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float ContentHeight = IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::IslandNav, IslandCount);
		IslandNavWidget->ApplyDevPanelStackPosition(StackY, ContentHeight);
		StackY += ContentHeight + IH_P1C08_DevPanelStyle::PanelSpacing;
	}

	if (TemplateGalleryWidget && TemplateGalleryWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float ContentHeight = IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::TemplateGallery, IslandCount);
		TemplateGalleryWidget->ApplyDevPanelStackPosition(StackY, ContentHeight);
		StackY += ContentHeight + IH_P1C08_DevPanelStyle::PanelSpacing;
	}

	if (CoastlineTuningWidget)
	{
		const float ContentHeight = CoastlineTuningWidget->GetStackContentHeight();
		CoastlineTuningWidget->ApplyDevPanelStackPosition(StackY, ContentHeight);
		StackY += ContentHeight + IH_P1C08_DevPanelStyle::PanelSpacing;
	}

	if (WeatherPreviewWidget && WeatherPreviewWidget->IsPanelVisible())
	{
		const float ContentHeight = IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::WeatherPreview, IslandCount);
		WeatherPreviewWidget->ApplyDevPanelStackPosition(StackY, ContentHeight);
	}
	if (GameDateTimeWidget && GameDateTimeWidget->IsPanelVisible())
	{
		GameDateTimeWidget->UpdatePanelLayout();
	}
	if (PlayAtmosphericsWidget && PlayAtmosphericsWidget->IsPanelVisible())
	{
		PlayAtmosphericsWidget->UpdatePanelLayout();
	}
}

void AIH_Cube2FlyPlayerController::RefreshIslandNavFromSubsystem()
{
	if (IslandNavWidget)
	{
		IslandNavWidget->RefreshTableFromSubsystem();
	}
}

FVector AIH_Cube2FlyPlayerController::ComputeIslandCaptionAnchorCm(int32 IslandIndex) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			if (const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex))
			{
				// Plan Addendum 10: anchor to the real landmass centroid (not the actor's
				// pre-generation origin - same "not the real landmass" gap Task 1 fixed for the
				// reticle).
				const FVector Loc = Island->GetMainLandCentroidWorldCm();
				const float SemiMajor = Island->GetSemiMajorAxisCm();

				// Top-Down: the camera sits nearly straight overhead, so "toward camera" (XY)
				// shrinks toward zero and the anchor direction becomes noise-sensitive - exactly
				// the "odd inconsistent location" reported. Use a fixed world-space offset instead,
				// stable regardless of camera position when looking straight down.
				const bool bTopDown = TopDownViewWidget && TopDownViewWidget->IsTopDownActive();
				if (bTopDown)
				{
					return Loc + FVector(0.f, -1.f, 0.f) * (SemiMajor * 0.55f);
				}

				const FVector CamLoc = PlayerCameraManager
					? PlayerCameraManager->GetCameraLocation()
					: (GetPawn() ? GetPawn()->GetActorLocation() : Loc + FVector(0.f, -SemiMajor, 0.f));
				FVector ToCamera = CamLoc - Loc;
				ToCamera.Z = 0.f;
				if (ToCamera.IsNearlyZero(1.f))
				{
					ToCamera = FVector(0.f, -1.f, 0.f);
				}
				ToCamera.Normalize();
				return Loc + ToCamera * (SemiMajor * 0.55f);
			}
		}
	}
	return FVector::ZeroVector;
}

void AIH_Cube2FlyPlayerController::ShowIslandCaptionForNavIndex(int32 IslandIndex)
{
	if (IslandIndex == INDEX_NONE || !IslandCaptionWidget)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	if (!Nav)
	{
		return;
	}

	FIHIslandNavRecord Record;
	if (Nav->TryGetNavRecord(IslandIndex, Record))
	{
		const FVector Anchor = ComputeIslandCaptionAnchorCm(IslandIndex);
		IslandCaptionWidget->ShowForIsland(IslandIndex, Record.Name, Record.Transliteration, Anchor);
	}
}

void AIH_Cube2FlyPlayerController::HandleIslandSelectionChanged(int32 IslandIndex)
{
	if (IslandNavWidget)
	{
		IslandNavWidget->SyncSelectionFromSubsystem(IslandIndex);
	}
	if (CoastlineTuningWidget)
	{
		CoastlineTuningWidget->UpdatePanelLayout();
	}

	if (IslandIndex != INDEX_NONE && IsViewportIslandSelectionBlocked())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase island selection visual blocked — W fly-out open (index=%d)"),
			IslandIndex);
#endif
		bShowIslandSelectionVisual = false;
		if (IslandCaptionWidget)
		{
			IslandCaptionWidget->HideCaption();
		}
		SyncIslandSelectionMeshGlow();
		UpdateEditingHint();
		return;
	}

	if (IslandIndex == INDEX_NONE)
	{
		bCameraFlyActive = false;
		SetIslandSelectionVisualVisible(false);
		if (IslandCaptionWidget)
		{
			IslandCaptionWidget->HideCaption();
		}
		UpdateEditingHint();
		return;
	}

	bShowIslandSelectionVisual = true;
	// Do not fly/zoom camera on select — keep caption + axes; player keeps viewport.
	bCameraFlyActive = false;
	SyncIslandSelectionMeshGlow();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			SetLastValidDraftOffsetCm(IslandIndex, Tuning->GetActiveManualTransform().OffsetXYCm);
		}
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			ShowIslandCaptionForNavIndex(IslandIndex);
		}
	}

	UpdateEditingHint();
}

void AIH_Cube2FlyPlayerController::BeginCameraFlyToIsland(int32 IslandIndex)
{
	if (IsViewportIslandSelectionBlocked())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase island camera fly blocked — W fly-out open (index=%d)"),
			IslandIndex);
#endif
		return;
	}

	APawn* ViewPawn = GetPawn();
	if (!ViewPawn)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	const AIH_WB_IslandActor* Island = GM ? GM->GetSpawnedIsland(IslandIndex) : nullptr;
	if (!Island)
	{
		return;
	}

	// Plan Addendum 10: Addendum 8's pure XY-pan-preserve-everything was tried per confirmed
	// direction at the time, but after seeing it in practice the user asked for the canonical
	// offshore establishing shot back - reinstating fixed, deliberate per-mode framing (to be
	// formalized as an IH-DEC canon entry once confirmed). Anchored on the real landmass centroid
	// (GetMainLandCentroidWorldCm, Addendum 10) rather than the actor's pre-generation origin.
	const FVector IslandCenter = Island->GetMainLandCentroidWorldCm();
	const float SemiMajor = Island->GetSemiMajorAxisCm();
	const float FrameDist = FMath::Max(SemiMajor * IslandCameraDistanceScale, IslandCameraMinDistanceCm);

	CameraFlyStartLoc = ViewPawn->GetActorLocation();
	CameraFlyStartRot = GetControlRotation();

	const bool bTopDown = TopDownViewWidget && TopDownViewWidget->IsTopDownActive();
	if (bTopDown)
	{
		// Plan Addendum 11: frame the REALIZED extents, not the pre-generation layout envelope -
		// SemiMajorAxisCm can be much larger than what actually rendered (this session's logs show
		// landFraction as low as 0.021), which over-frames a small island in a lot of empty water.
		// Regular View's FrameDist (above) is intentionally untouched per explicit direction.
		//
		// First cut reused IslandCameraDistanceScale (1.35) for the altitude, but that constant was
		// tuned for Regular View's OBLIQUE offshore distance - a different geometric relationship
		// than a straight-down orthographic fit, and it framed too close (user report - full island
		// extent not visible without manually zooming out). Derive altitude properly instead: for a
		// camera looking straight down with vertical half-FOV a at altitude H, the visible ground
		// half-extent is H*tan(a) - solve for H so the island's footprint radius fits, plus a
		// generous padding factor so the full island (not just up to its cell-center footprint
		// radius) sits comfortably inside frame with room to spare.
		const float FovDegrees = PlayerCameraManager ? PlayerCameraManager->GetFOVAngle() : 90.f;
		const float HalfFovRad = FMath::DegreesToRadians(FMath::Max(FovDegrees, 1.f) * 0.5f);
		constexpr float TopDownFramingPadding = 1.5f;
		const float TopDownFrameDist = FMath::Max(
			(Island->GetMainLandFootprintRadiusCm() * TopDownFramingPadding) / FMath::Tan(HalfFovRad),
			IslandCameraMinDistanceCm);
		// Overhead "fit to frame": pan X/Y to center, keep pitch locked at -90 (unchanged).
		// Plan Addendum 14: force Yaw=90 (north-up) instead of preserving the free-fly camera's
		// prior arbitrary heading, so every Top-Down entry point matches the minimap's
		// +Y=north=up convention consistently (user-confirmed direction).
		CameraFlyTargetLoc = FVector(IslandCenter.X, IslandCenter.Y, IslandCenter.Z + TopDownFrameDist);
		CameraFlyTargetRot = FRotator(-90.f, 90.f, 0.f);
	}
	else
	{
		// Offshore establishing shot: fixed distance/pitch, approaching from whichever horizontal
		// direction the camera already roughly is (avoids spinning the player to an arbitrary
		// compass heading on every focus).
		FVector OffsetDir = CameraFlyStartLoc - IslandCenter;
		OffsetDir.Z = 0.f;
		if (OffsetDir.IsNearlyZero(1.f))
		{
			OffsetDir = FVector(0.f, -1.f, 0.f);
		}
		OffsetDir.Normalize();
		const float CamHeight = FrameDist * FMath::Tan(FMath::DegreesToRadians(FMath::Abs(IslandCameraPitchDeg)));
		CameraFlyTargetLoc = IslandCenter + OffsetDir * FrameDist;
		CameraFlyTargetLoc.Z = IslandCenter.Z + CamHeight;
		CameraFlyTargetRot = FRotator(IslandCameraPitchDeg, (IslandCenter - CameraFlyTargetLoc).Rotation().Yaw, 0.f);
	}

	CameraFlyElapsedSec = 0.f;
	bCameraFlyActive = true;
}

void AIH_Cube2FlyPlayerController::TickCameraFly(float DeltaTime)
{
	if (!bCameraFlyActive)
	{
		return;
	}

	APawn* ViewPawn = GetPawn();
	if (!ViewPawn)
	{
		bCameraFlyActive = false;
		return;
	}

	CameraFlyElapsedSec += DeltaTime;
	const float Alpha = FMath::Clamp(CameraFlyElapsedSec / IslandCameraFlyDurationSec, 0.f, 1.f);
	const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);

	ViewPawn->SetActorLocation(FMath::Lerp(CameraFlyStartLoc, CameraFlyTargetLoc, Smooth), false);
	SetControlRotation(FMath::Lerp(CameraFlyStartRot, CameraFlyTargetRot, Smooth));

	if (Alpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		bCameraFlyActive = false;
	}
}

void AIH_Cube2FlyPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureViewportKeyboardFocus();
}

void AIH_Cube2FlyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			if (IslandNavChangedHandle.IsValid())
			{
				Nav->OnIslandNavChanged.Remove(IslandNavChangedHandle);
				IslandNavChangedHandle.Reset();
			}
			if (IslandSelectionChangedHandle.IsValid())
			{
				Nav->OnSelectionChanged.Remove(IslandSelectionChangedHandle);
				IslandSelectionChangedHandle.Reset();
			}
		}
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (ManualTransformChangedHandle.IsValid())
			{
				Tuning->OnManualTransformChanged.Remove(ManualTransformChangedHandle);
				ManualTransformChangedHandle.Reset();
			}
		}
	}
	EndLassoDragCapture();
	ApplyPresentationInputMode();
	Super::EndPlay(EndPlayReason);
}

void AIH_Cube2FlyPlayerController::ReleaseUnwantedMouseCapture()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* GVC = World->GetGameViewport())
		{
			GVC->SetMouseLockMode(EMouseLockMode::DoNotLock);
			GVC->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		}
	}
}

void AIH_Cube2FlyPlayerController::ApplyFreeMouseViewportSettings()
{
	const bool bKeepDragCapture = bLassoDragCaptureActive;
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* GVC = World->GetGameViewport())
		{
			GVC->SetMouseLockMode(EMouseLockMode::DoNotLock);
			GVC->SetMouseCaptureMode(
				bKeepDragCapture ? EMouseCaptureMode::CaptureDuringMouseDown : EMouseCaptureMode::NoCapture);
		}
	}

	if (!bKeepDragCapture && !bMouseLookActive)
	{
		ReleaseUnwantedMouseCapture();
	}
}

void AIH_Cube2FlyPlayerController::BeginLassoDragCapture()
{
	if (bLassoDragCaptureActive)
	{
		return;
	}

	bLassoDragCaptureActive = true;
	ApplyFreeMouseViewportSettings();
}

void AIH_Cube2FlyPlayerController::EndLassoDragCapture()
{
	if (!bLassoDragCaptureActive)
	{
		return;
	}

	bLassoDragCaptureActive = false;
	ApplyFreeMouseViewportSettings();
}

void AIH_Cube2FlyPlayerController::EnsureViewportKeyboardFocus()
{
	SetShowMouseCursor(true);
	bShowMouseCursor = true;

	// GameAndUI + DoNotLock: keyboard to viewport, cursor free to leave PIE window / use editor UI.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	ApplyFreeMouseViewportSettings();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
		if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
			{
				if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())
				{
					FSlateApplication::Get().SetKeyboardFocus(ViewportWidget, EFocusCause::SetDirectly);
				}
			}
		}
	}
}

void AIH_Cube2FlyPlayerController::ApplyPresentationInputMode()
{
	bMouseLookActive = false;
	if (!bLeftMouseDown)
	{
		EndLassoDragCapture();
		bMinimapPointerCapture = false;
		bBuildPalettePointerCapture = false;
		bBuildPaletteDragFromPalette = false;
		bHUDSliderPointerCapture = false;
		bLeftMouseStartedOverMinimap = false;
	}
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	EnsureViewportKeyboardFocus();
}

void AIH_Cube2FlyPlayerController::ApplyMouseLookInputMode()
{
	bMouseLookActive = true;
	EnsureViewportKeyboardFocus();
}

void AIH_Cube2FlyPlayerController::BindFlyMovementKeys()
{
	if (!InputComponent)
	{
		return;
	}

	const auto BindFlyKey = [this](FKey Key) {
		FInputKeyBinding PressedBinding(Key, IE_Pressed);
		PressedBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Key]() {
			PressedFlyKeys.Add(Key);
		});
		InputComponent->KeyBindings.Add(PressedBinding);

		FInputKeyBinding ReleasedBinding(Key, IE_Released);
		ReleasedBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Key]() {
			PressedFlyKeys.Remove(Key);
		});
		InputComponent->KeyBindings.Add(ReleasedBinding);
	};

	BindFlyKey(EKeys::W);
	BindFlyKey(EKeys::S);
	BindFlyKey(EKeys::A);
	BindFlyKey(EKeys::D);
	BindFlyKey(EKeys::Up);
	BindFlyKey(EKeys::Down);
	BindFlyKey(EKeys::Left);
	BindFlyKey(EKeys::Right);
	BindFlyKey(EKeys::E);
	BindFlyKey(EKeys::Q);
	BindFlyKey(EKeys::SpaceBar);
	BindFlyKey(EKeys::LeftControl);
	BindFlyKey(EKeys::PageUp);
	BindFlyKey(EKeys::PageDown);
}

void AIH_Cube2FlyPlayerController::BindNavDebugToggleKeys()
{
	if (!InputComponent)
	{
		return;
	}

	const auto BindToggle = [this](FKey Key) {
		FInputKeyBinding PressedBinding(Key, IE_Pressed);
		PressedBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this]() {
			bNavDebugDrawPersistent = !bNavDebugDrawPersistent;
			UE_LOG(
				LogTemp, Log, TEXT("Nav collision debug envelopes: %s"),
				bNavDebugDrawPersistent ? TEXT("ON") : TEXT("OFF"));
		});
		InputComponent->KeyBindings.Add(PressedBinding);
	};

	BindToggle(EKeys::O);
	BindToggle(EKeys::Zero);
	BindToggle(EKeys::NumPadZero);
}

void AIH_Cube2FlyPlayerController::BindGlobalHUDKeys()
{
	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AIH_Cube2FlyPlayerController::HandleBuildPaletteGridTogglePressed);
}

bool AIH_Cube2FlyPlayerController::IsKeyDownAnywhere(FKey Key) const
{
	if (IsInputKeyDown(Key) || PressedFlyKeys.Contains(Key))
	{
		return true;
	}

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
		{
			if (const FViewport* Viewport = ViewportClient->Viewport)
			{
				if (Viewport->KeyState(Key))
				{
					return true;
				}
			}
		}
	}

#if PLATFORM_WINDOWS
	if (const int32 VirtualKey = IH_Cube2FlyPlayerControllerPrivate::GetVirtualKeyForGlobalHUDKey(Key))
	{
		if (IH_Cube2FlyPlayerControllerPrivate::IsPhysicalKeyDownWin32(VirtualKey))
		{
			return true;
		}
	}
#endif

	return false;
}

void AIH_Cube2FlyPlayerController::TryMinimapToggleFromTick()
{
	const bool bMinimapKeyDown = IsKeyDownAnywhere(EKeys::M);
	if (bMinimapKeyDown && !bPrevMinimapKeyDown)
	{
		HandleMinimapTogglePressed();
	}
	bPrevMinimapKeyDown = bMinimapKeyDown;
}

void AIH_Cube2FlyPlayerController::HandleBuildPaletteGridTogglePressed()
{
	HandleBuildPaletteTabKeyPressed(EIHBuildPaletteTab::Grid);
}

void AIH_Cube2FlyPlayerController::HandleBuildPaletteTabKeyPressed(EIHBuildPaletteTab Tab)
{
	if (IsHUDSliderConsumingKeyboard())
	{
		return;
	}

	static uint64 LastToggleFrame = static_cast<uint64>(-1);
	if (GFrameCounter == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = GFrameCounter;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			BuildPalette->EnsureBuildPaletteReady(this);
			const bool bWasOpen = BuildPalette->IsFlyOutOpen() && BuildPalette->GetActiveTab() == Tab;
			BuildPalette->ToggleTabFlyOut(Tab, this);
			if (UIH_BuildPaletteHostWidget* Host = BuildPalette->GetBuildPaletteWidget())
			{
				Host->RequestLayoutRefresh();
			}
			UE_LOG(
				LogTemp, Warning,
				TEXT("BuildPalette tab key — tab=%d wasOpen=%d nowOpen=%d"),
				static_cast<int32>(Tab),
				bWasOpen ? 1 : 0,
				BuildPalette->IsFlyOutOpen() ? 1 : 0);

			const int32 TabIndex = static_cast<int32>(Tab);
			if (TabIndex >= 0 && TabIndex < UE_ARRAY_COUNT(bPrevBuildPaletteTabKeyDown))
			{
				bPrevBuildPaletteTabKeyDown[TabIndex] = true;
			}
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("HandleBuildPaletteTabKeyPressed: BuildPaletteSubsystem missing"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleBuildPaletteTabKeyPressed: GameInstance missing"));
	}
}

void AIH_Cube2FlyPlayerController::TryBuildPaletteTabKeysFromTick()
{
	static const FKey TabKeys[] = {
		EKeys::G,
		EKeys::W,
		EKeys::B,
		EKeys::C,
		EKeys::D,
	};
	static const EIHBuildPaletteTab TabEnums[] = {
		EIHBuildPaletteTab::Grid,
		EIHBuildPaletteTab::World,
		EIHBuildPaletteTab::Build,
		EIHBuildPaletteTab::Convey,
		EIHBuildPaletteTab::Defense,
	};

	UGameInstance* GI = GetGameInstance();
	UIH_BuildPaletteSubsystem* BuildPalette = GI ? GI->GetSubsystem<UIH_BuildPaletteSubsystem>() : nullptr;
	if (!BuildPalette)
	{
		for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(TabKeys); ++TabIndex)
		{
			bPrevBuildPaletteTabKeyDown[TabIndex] = IsKeyDownAnywhere(TabKeys[TabIndex]);
		}
		return;
	}

	BuildPalette->EnsureBuildPaletteReady(this);

	// EnhancedInput BindKey handles G when the viewport has focus; tick poll is the fallback.
	const bool bGridKeyDown = IsKeyDownAnywhere(EKeys::G);
	if (bGridKeyDown && !bPrevBuildPaletteTabKeyDown[0])
	{
		HandleBuildPaletteGridTogglePressed();
	}
	bPrevBuildPaletteTabKeyDown[0] = bGridKeyDown;

	if (!BuildPalette->HasTabStripEnabled())
	{
		for (int32 TabIndex = 1; TabIndex < UE_ARRAY_COUNT(TabKeys); ++TabIndex)
		{
			bPrevBuildPaletteTabKeyDown[TabIndex] = IsKeyDownAnywhere(TabKeys[TabIndex]);
		}
		return;
	}

	for (int32 TabIndex = 1; TabIndex < UE_ARRAY_COUNT(TabKeys); ++TabIndex)
	{
		const bool bKeyDown = IsKeyDownAnywhere(TabKeys[TabIndex]);
		if (bKeyDown && !bPrevBuildPaletteTabKeyDown[TabIndex])
		{
			HandleBuildPaletteTabKeyPressed(TabEnums[TabIndex]);
		}
		bPrevBuildPaletteTabKeyDown[TabIndex] = bKeyDown;
	}
}

void AIH_Cube2FlyPlayerController::TryMinimapCloseFromTick()
{
	UIH_P1C08_MinimapSubsystem* Minimap = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>();
	}

	if (!Minimap || !Minimap->IsMinimapOpen())
	{
		bPrevMinimapCloseKeyDown = false;
		return;
	}

	const bool bCloseKeyDown = IsInputKeyDown(EKeys::X);
	if (bCloseKeyDown && !bPrevMinimapCloseKeyDown && !IsHUDSliderConsumingKeyboard())
	{
		Minimap->CloseMinimap();
	}
	bPrevMinimapCloseKeyDown = bCloseKeyDown;
}

void AIH_Cube2FlyPlayerController::TryPauseToggleFromTick()
{
	const bool bPauseKeyDown = IsKeyDownAnywhere(EKeys::P);
	if (bPauseKeyDown && !bPrevPauseKeyDown)
	{
		HandlePauseTogglePressed();
	}
	bPrevPauseKeyDown = bPauseKeyDown;
}

void AIH_Cube2FlyPlayerController::HandlePauseTogglePressed()
{
	if (GameSpeedWidget)
	{
		GameSpeedWidget->ToggleKeyboardPause();
	}
}

void AIH_Cube2FlyPlayerController::HandleMinimapTogglePressed()
{
	UE_LOG(LogTemp, Warning, TEXT("HandleMinimapTogglePressed (PC=%s)"), *GetName());

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->ToggleMinimap(this);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("HandleMinimapTogglePressed: MinimapSubsystem missing on GameInstance"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleMinimapTogglePressed: GameInstance missing"));
	}
}

void AIH_Cube2FlyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindFlyMovementKeys();
	BindNavDebugToggleKeys();
	BindGlobalHUDKeys();
}

void AIH_Cube2FlyPlayerController::ApplyKeyboardFlyMovement(float DeltaTime)
{
	if (!GetPawn() || IsHUDSliderConsumingKeyboard())
	{
		return;
	}

	const auto IsFlyKeyDown = [this](FKey Key) -> bool {
		return PressedFlyKeys.Contains(Key)
			|| IH_Cube2FlyPlayerControllerPrivate::IsFlyKeyDownAnywhere(this, Key);
	};

	const FRotator HeadYaw(0.f, GetControlRotation().Yaw, 0.f);
	const FVector ForwardXY = FRotationMatrix(HeadYaw).GetUnitAxis(EAxis::X);
	const FVector RightXY = FRotationMatrix(HeadYaw).GetUnitAxis(EAxis::Y);
	FVector Wish = FVector::ZeroVector;
	if (IsFlyKeyDown(EKeys::W) || IsFlyKeyDown(EKeys::Up)) Wish += ForwardXY;
	if (IsFlyKeyDown(EKeys::S) || IsFlyKeyDown(EKeys::Down)) Wish -= ForwardXY;
	if (IsFlyKeyDown(EKeys::D) || IsFlyKeyDown(EKeys::Right)) Wish += RightXY;
	if (IsFlyKeyDown(EKeys::A) || IsFlyKeyDown(EKeys::Left)) Wish -= RightXY;
	if (IsFlyKeyDown(EKeys::E) || IsFlyKeyDown(EKeys::SpaceBar) || IsFlyKeyDown(EKeys::PageUp)) Wish += FVector::UpVector;
	if (IsFlyKeyDown(EKeys::Q) || IsFlyKeyDown(EKeys::LeftControl) || IsFlyKeyDown(EKeys::PageDown)) Wish -= FVector::UpVector;
	if (!Wish.IsNearlyZero(1e-4f))
	{
		GetPawn()->AddActorWorldOffset(Wish.GetSafeNormal() * KeyboardFlySpeedCmPerSec * DeltaTime);
	}
}

bool AIH_Cube2FlyPlayerController::AbsoluteToViewportLocal(
	const FVector2D& AbsolutePos,
	FVector2D& OutViewportPos) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameViewportClient* ViewportClient = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
	if (!ViewportClient)
	{
		return false;
	}

	const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
	if (!ViewportWidget.IsValid())
	{
		return false;
	}

	// Tick-space geometry tracks cursor motion during NoCapture; cached geometry can freeze.
	OutViewportPos = ViewportWidget->GetTickSpaceGeometry().AbsoluteToLocal(AbsolutePos);
	return true;
}

bool AIH_Cube2FlyPlayerController::TryGetViewportMousePosition(FVector2D& OutViewportPos) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	if (AbsoluteToViewportLocal(FSlateApplication::Get().GetCursorPos(), OutViewportPos))
	{
		return true;
	}

	float LocationX = 0.f;
	float LocationY = 0.f;
	if (GetMousePosition(LocationX, LocationY))
	{
		OutViewportPos = FVector2D(LocationX, LocationY);
		return true;
	}

	return false;
}

bool AIH_Cube2FlyPlayerController::IsLeftMouseButtonDown() const
{
	if (IsInputKeyDown(EKeys::LeftMouseButton))
	{
		return true;
	}

	if (FSlateApplication::IsInitialized())
	{
		return FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
	}

	return false;
}

bool AIH_Cube2FlyPlayerController::IsMiddleMouseButtonDown() const
{
	if (IsInputKeyDown(EKeys::MiddleMouseButton))
	{
		return true;
	}

	if (FSlateApplication::IsInitialized())
	{
		return FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::MiddleMouseButton);
	}

	return false;
}

bool AIH_Cube2FlyPlayerController::IsRightMouseButtonDown() const
{
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		return true;
	}

	if (FSlateApplication::IsInitialized())
	{
		return FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::RightMouseButton);
	}

	return false;
}

void AIH_Cube2FlyPlayerController::ApplyFlyCameraRotationDelta(
	float DeltaX,
	float DeltaY,
	bool bAllowPitch)
{
	if (FMath::IsNearlyZero(DeltaX) && (!bAllowPitch || FMath::IsNearlyZero(DeltaY)))
	{
		return;
	}

	FRotator ViewRotation = GetControlRotation();
	ViewRotation.Yaw += DeltaX * LookDegPerMousePixel;
	if (bAllowPitch)
	{
		ViewRotation.Pitch = FMath::Clamp(
			ViewRotation.Pitch - DeltaY * LookDegPerMousePixel, -89.f, 89.f);
	}
	SetControlRotation(ViewRotation);
}

bool AIH_Cube2FlyPlayerController::IsHUDSliderConsumingKeyboard() const
{
	if (CoastlineTuningWidget
		&& (CoastlineTuningWidget->IsSliderCapturingInput() || CoastlineTuningWidget->IsKeyboardFocusActive()))
	{
		return true;
	}
	if (GameSpeedWidget
		&& (GameSpeedWidget->IsSliderCapturingInput() || GameSpeedWidget->IsKeyboardFocusActive()))
	{
		return true;
	}
	if (DevSeedPanelWidget && DevSeedPanelWidget->IsConsumingKeyboard())
	{
		return true;
	}
	return false;
}

void AIH_Cube2FlyPlayerController::TickHUDSliderKeyboardFocus(float DeltaTime)
{
	if (CoastlineTuningWidget && CoastlineTuningWidget->IsKeyboardFocusActive())
	{
		CoastlineTuningWidget->TickKeyboardFocusInput(this, DeltaTime);
	}
	else if (GameSpeedWidget && GameSpeedWidget->IsKeyboardFocusActive())
	{
		GameSpeedWidget->TickKeyboardFocusInput(this, DeltaTime);
	}
}

void AIH_Cube2FlyPlayerController::ProcessEarlyHUDPanelPointerDown(
	const FVector2D& ViewportCur, const FVector2D& CursorAbsolute)
{
	const bool bLeftMouseNow = IsLeftMouseButtonDown();
	const bool bLeftMouseJustPressed = bLeftMouseNow && !bPrevLeftMouseDown;
	if (!bLeftMouseJustPressed)
	{
		return;
	}

	UIH_P1C08_MinimapSubsystem* Minimap = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>();
	}

	if (Minimap && Minimap->IsScreenPointOverMinimap(CursorAbsolute))
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (BuildPalette->IsTabStripVisible())
			{
				if (const UIH_BuildPaletteHostWidget* Host = BuildPalette->GetBuildPaletteWidget())
				{
					if (Host->IsScreenPointOverTabStrip(CursorAbsolute)
						|| Host->IsScreenPointOverBuildPalette(CursorAbsolute))
					{
						return;
					}
				}
			}
		}
	}

	if (CoastlineTuningWidget && CoastlineTuningWidget->TryActivateKeyboardFocusFromPanelClick(CursorAbsolute))
	{
		return;
	}

	if (GameSpeedWidget && GameSpeedWidget->TryActivateKeyboardFocusFromPanelClick(CursorAbsolute))
	{
		return;
	}

	// Dev seed panel (−/+, Sculpt, Random Realm) is handled in PlayerTick pointer-down only;
	// routing it here too double-fired OnClicked and skipped island counts (3→5).

	(void)ViewportCur;
}

void AIH_Cube2FlyPlayerController::CancelActiveHUDKeyboardFocus()
{
	if (CoastlineTuningWidget && CoastlineTuningWidget->IsKeyboardFocusActive())
	{
		CoastlineTuningWidget->CancelKeyboardFocus();
	}
	if (GameSpeedWidget && GameSpeedWidget->IsKeyboardFocusActive())
	{
		GameSpeedWidget->CancelKeyboardFocus();
	}
}

void AIH_Cube2FlyPlayerController::HandleBuildPalettePointerPress(const FVector2D& CursorAbsolute)
{
	UGameInstance* GI = GetGameInstance();
	UIH_BuildPaletteSubsystem* BuildPalette = GI ? GI->GetSubsystem<UIH_BuildPaletteSubsystem>() : nullptr;
	if (!BuildPalette)
	{
		return;
	}

	BuildPalette->EnsureBuildPaletteReady(this);
	if (!BuildPalette->IsTabStripVisible())
	{
		return;
	}

	if (UIH_BuildPaletteHostWidget* PaletteWidget = BuildPalette->GetBuildPaletteWidget())
	{
		if (PaletteWidget->HandleScreenPointerDown(CursorAbsolute))
		{
			bLeftMouseConsumedByHUDPanel = true;
			if (BuildPalette->IsDragActive())
			{
				bBuildPalettePointerCapture = true;
				bBuildPaletteDragFromPalette = true;
			}
			return;
		}
	}
}

void AIH_Cube2FlyPlayerController::TickHUDSliderPointerMove(const FVector2D& CursorAbsolute)
{
	if (CoastlineTuningWidget && CoastlineTuningWidget->IsSliderCapturingInput())
	{
		CoastlineTuningWidget->HandleScreenPointerMove(CursorAbsolute);
	}
	else if (GameSpeedWidget && GameSpeedWidget->IsSliderCapturingInput())
	{
		GameSpeedWidget->HandleScreenPointerMove(CursorAbsolute);
	}
}

void AIH_Cube2FlyPlayerController::FinishHUDSliderPointerUp(const FVector2D& CursorAbsolute)
{
	if (CoastlineTuningWidget && CoastlineTuningWidget->IsSliderCapturingInput())
	{
		CoastlineTuningWidget->HandleScreenPointerUp(CursorAbsolute);
	}
	else if (GameSpeedWidget && GameSpeedWidget->IsSliderCapturingInput())
	{
		GameSpeedWidget->HandleScreenPointerUp(CursorAbsolute);
	}
}

void AIH_Cube2FlyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (CameraAslWidget)
	{
		int32 AslM = 0;
		if (const APawn* ViewPawn = GetPawn())
		{
			AslM = FMath::RoundToInt(ViewPawn->GetActorLocation().Z / 100.f);
		}
		CameraAslWidget->UpdateAltitudeMeters(AslM);
	}

	// Altitude-aware FOV: wide near sea, tighter at high ASL so flat ocean doesn't fish-eye.
	if (PlayerCameraManager)
	{
		float AslM = 0.f;
		if (const APawn* ViewPawn = GetPawn())
		{
			AslM = ViewPawn->GetActorLocation().Z / 100.f;
		}
		const float Span = FMath::Max(1.f, FlyFovLerpEndAslM - FlyFovLerpStartAslM);
		const float T = FMath::Clamp((AslM - FlyFovLerpStartAslM) / Span, 0.f, 1.f);
		PlayerCameraManager->SetFOV(FMath::Lerp(FlyFovNearSeaDeg, FlyFovHighAltDeg, T));
	}

	TryMinimapToggleFromTick();
	TryBuildPaletteTabKeysFromTick();
	TryMinimapCloseFromTick();
	TryPauseToggleFromTick();

	if (KeyboardFocusWarmupTicksRemaining > 0 && !bMouseLookActive && !IsHUDSliderConsumingKeyboard())
	{
		--KeyboardFocusWarmupTicksRemaining;
		if ((KeyboardFocusWarmupTicksRemaining % 15) == 0)
		{
			EnsureViewportKeyboardFocus();
		}
	}

	if (MouseCaptureWarmupTicksRemaining > 0 && !bMouseLookActive && !bLeftMouseDown && !bLassoDragCaptureActive)
	{
		--MouseCaptureWarmupTicksRemaining;
		if ((MouseCaptureWarmupTicksRemaining % 15) == 0)
		{
			ApplyFreeMouseViewportSettings();
		}
	}

	UIH_P1C08_MinimapSubsystem* Minimap = nullptr;
	UIH_BuildPaletteSubsystem* BuildPalette = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>();
		BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>();
	}

	const float Wheel = GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	if (!FMath::IsNearlyZero(Wheel))
	{
		if (Minimap && Minimap->IsMinimapOpen() && Minimap->IsMouseOverMinimap())
		{
			const FVector2D WheelCursorAbsolute = FSlateApplication::IsInitialized()
				? FSlateApplication::Get().GetCursorPos()
				: FVector2D::ZeroVector;
			Minimap->HandleMouseWheelZoom(Wheel, WheelCursorAbsolute);
		}
		else if (BuildPalette && BuildPalette->HasSelectedTerrainStamp() && BuildPalette->IsWorldStampEditModeActive())
		{
			BuildPalette->ApplySelectedStampMouseWheel(this, Wheel);
		}
		else if (AIH_TownGridManager* TownGrid = GetSelectedTownGridManager())
		{
			TownGrid->ApplyWheelYaw(Wheel);
		}
		else if (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift))
		{
			bool bRotatedIsland = false;
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
				{
					const int32 Selected = Nav->GetSelectedIslandIndex();
					if (Selected != INDEX_NONE)
					{
						if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
						{
							const float Sign = Wheel > 0.f ? 1.f : -1.f;
							Tuning->AddDraftManualYawDeg(Sign * IslandShiftWheelRotateDeg);
							bRotatedIsland = true;
						}
					}
				}
			}
			if (!bRotatedIsland)
			{
				if (APawn* ViewPawn = GetPawn())
				{
					const FVector Forward = GetControlRotation().Vector();
					ViewPawn->AddActorWorldOffset(Forward * Wheel * ZoomCmPerWheelUnit);
				}
			}
		}
		else if (APawn* ViewPawn = GetPawn())
		{
			const FVector Forward = GetControlRotation().Vector();
			ViewPawn->AddActorWorldOffset(Forward * Wheel * ZoomCmPerWheelUnit);
		}
	}

	bPrevMinimapPageUp = false;
	bPrevMinimapPageDown = false;

	FVector2D ViewportCur = FVector2D::ZeroVector;
	const bool bHasViewportMouse = TryGetViewportMousePosition(ViewportCur);
	if (bHasViewportMouse && FSlateApplication::IsInitialized())
	{
		ProcessEarlyHUDPanelPointerDown(ViewportCur, FSlateApplication::Get().GetCursorPos());
	}

	TickHUDSliderKeyboardFocus(DeltaTime);
	if (!bCameraFlyActive)
	{
		ApplyKeyboardFlyMovement(DeltaTime);
	}
	else
	{
		TickCameraFly(DeltaTime);
	}
	if (IslandCaptionWidget)
	{
		IslandCaptionWidget->TickCaption(DeltaTime, this);
	}
	TickBuildPaletteAndTownGrid(DeltaTime);
	TickDevMousePointerEcho(DeltaTime);
	TickIslandManipulationInput(DeltaTime);
	TickIslandManipulationGizmo(DeltaTime);
	TickTerrainStampManipulationInput(DeltaTime);
	TickTerrainStampManipulationGizmo(DeltaTime);
	DrawLaneViolationFlash(DeltaTime);
	UpdateNavCollisionDebugDraw(DeltaTime);

	FVector2D MouseDragDelta = FVector2D::ZeroVector;
	if (bHasViewportMouse)
	{
		MouseDragDelta = ViewportCur - PrevMousePixels;
		PrevMousePixels = ViewportCur;
	}

	const bool bRightMouse = IsRightMouseButtonDown();
	const bool bMiddleMouse = IsMiddleMouseButtonDown();
	const bool bRightMouseJustPressed = bRightMouse && !bPrevRightMouseDown;
	const bool bRightMouseJustReleased = !bRightMouse && bPrevRightMouseDown;
	const bool bMiddleMouseJustPressed = bMiddleMouse && !bPrevMiddleMouseDown;
	bPrevRightMouseDown = bRightMouse;
	bPrevMiddleMouseDown = bMiddleMouse;

	if (bRightMouseJustPressed || bMiddleMouseJustPressed)
	{
		MouseDragDelta = FVector2D::ZeroVector;
	}
	if (bRightMouseJustPressed && bHasViewportMouse)
	{
		RightMouseDragStart = ViewportCur;
	}

	bool bShipsSelected = false;
	if (UGameInstance* GIForShips = GetGameInstance())
	{
		if (UIH_P1C07_ShipRegistrySubsystem* Reg =
				GIForShips->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())
		{
			bShipsSelected = Reg->GetSelectedShips().Num() > 0;
		}
	}

	if (bRightMouseJustReleased && bHasViewportMouse && bShipsSelected)
	{
		HandleRightMouseReleaseForShipOrders(ViewportCur);
	}

	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (CoastlineTuningWidget && CoastlineTuningWidget->IsKeyboardFocusActive())
		{
			CoastlineTuningWidget->CancelKeyboardFocus();
		}
		else if (GameSpeedWidget && GameSpeedWidget->IsKeyboardFocusActive())
		{
			GameSpeedWidget->CancelKeyboardFocus();
		}
		else if (BuildPalette && BuildPalette->IsDragActive())
		{
			BuildPalette->CancelDrag();
			bBuildPalettePointerCapture = false;
			bBuildPaletteDragFromPalette = false;
		}
		else if (BuildPalette && BuildPalette->IsFlyOutOpen())
		{
			BuildPalette->CloseFlyOut();
		}
		else if (UGameInstance* GI = GetGameInstance())
		{
			if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
			{
				if (Nav->HasSelectedIsland() && HasUncommittedIslandDraft())
				{
					RevertActiveIslandDraft();
					UpdateEditingHint();
					return;
				}
				if (Nav->HasSelectedIsland())
				{
					RequestDeselectIsland();
					return;
				}
			}
			if (BuildPalette && BuildPalette->HasSelectedTerrainStamp())
			{
				BuildPalette->ClearTerrainStampSelection();
				return;
			}
			if (UIH_P1C07_ShipRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())
			{
				if (Registry->GetSelectedShips().Num() > 0)
				{
					Registry->ClearSelection();
					return;
				}
			}
		}
		else if (GetSelectedTownGridManager())
		{
			bTownGridMovePointerCapture = false;
			DeselectTownGridManager();
		}
		else if (bMouseLookActive)
		{
			ApplyPresentationInputMode();
		}
	}

	if (bRightMouseJustPressed && GetSelectedTownGridManager())
	{
		bTownGridMovePointerCapture = false;
		DeselectTownGridManager();
	}

	// TW-style: while commandable units selected, RMB is orders-only — suspend camera look.
	const bool bAllowRmbLook = !bShipsSelected;

	if (bAllowRmbLook && bRightMouse && !bMouseLookActive)
	{
		ApplyMouseLookInputMode();
	}
	else if ((!bRightMouse || !bAllowRmbLook) && bMouseLookActive)
	{
		ApplyPresentationInputMode();
	}

	if (bAllowRmbLook && bRightMouse && !bIslandDragActive && !(BuildPalette && BuildPalette->IsDragActive()))
	{
		ApplyFlyCameraRotationDelta(MouseDragDelta.X, MouseDragDelta.Y, true);
		return;
	}

	if (bAllowRmbLook && bRightMouse)
	{
		ApplyFlyCameraRotationDelta(MouseDragDelta.X, MouseDragDelta.Y, true);
	}

	if (bMiddleMouse)
	{
		ApplyFlyCameraRotationDelta(MouseDragDelta.X, 0.f, false);

		if (APawn* ViewPawn = GetPawn())
		{
			if (!FMath::IsNearlyZero(MouseDragDelta.Y))
			{
				ViewPawn->AddActorWorldOffset(-FVector::UpVector * MouseDragDelta.Y * PanCmPerMousePixel);
			}
		}
	}

	if (!bHasViewportMouse)
	{
		return;
	}

	if (!Minimap || !Minimap->IsMinimapOpen())
	{
		bMinimapPointerCapture = false;
	}

	const FVector2D CursorAbsolute = FSlateApplication::Get().GetCursorPos();

	if (Minimap && Minimap->IsMinimapOpen())
	{
		if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
		{
			MinimapWidget->HandleScreenPointerMove(CursorAbsolute);
		}
	}

	if (BuildPalette && BuildPalette->IsTabStripVisible())
	{
		if (UIH_BuildPaletteHostWidget* PaletteWidget = BuildPalette->GetBuildPaletteWidget())
		{
			PaletteWidget->HandleScreenPointerMove(CursorAbsolute);
		}
	}

	const bool bLeftMouseNow = IsLeftMouseButtonDown();
	const bool bLeftMouseJustPressed = bLeftMouseNow && !bPrevLeftMouseDown;
	const bool bLeftMouseJustReleased = !bLeftMouseNow && bPrevLeftMouseDown;
	bPrevLeftMouseDown = bLeftMouseNow;

	if (bLeftMouseJustPressed)
	{
		bLeftMouseDown = true;
		LeftMouseDragStart = ViewportCur;
		LeftMouseDragStartAbsolute = CursorAbsolute;
		bLeftMouseStartedOverMinimap = Minimap && Minimap->IsScreenPointOverMinimap(CursorAbsolute);
		bLeftMouseConsumedByHUDPanel = false;
		bHUDSliderPointerCapture = false;
		bBuildPalettePointerCapture = false;
		bBuildPaletteDragFromPalette = false;

		HandleBuildPalettePointerPress(CursorAbsolute);

		if (!bLeftMouseConsumedByHUDPanel && bLeftMouseStartedOverMinimap && Minimap)
		{
			if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
			{
				bMinimapPointerCapture = MinimapWidget->HandleScreenPointerDown(CursorAbsolute);
				if (bMinimapPointerCapture)
				{
					bLeftMouseConsumedByHUDPanel = true;
				}
			}
		}

		if (!bLeftMouseConsumedByHUDPanel && !bLeftMouseStartedOverMinimap && !bMinimapPointerCapture)
		{
			const bool bShiftDown = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
			if (bShiftDown && BuildPalette)
			{
				AIH_TerrainStampActor* HitStamp = nullptr;
				if (BuildPalette->TryFindTerrainStampAtScreen(this, ViewportCur, HitStamp) && HitStamp)
				{
					BuildPalette->BeginStampMoveDrag(HitStamp);
					bStampMovePointerCapture = true;
					bLeftMouseConsumedByHUDPanel = true;
				}
			}

			int32 HitIslandIndex = INDEX_NONE;
			if (bShiftDown && !bStampMovePointerCapture && !IsViewportIslandSelectionBlocked())
			{
				if (!TryResolveIslandIndexAtScreen(ViewportCur, HitIslandIndex))
				{
					HitIslandIndex = INDEX_NONE;
				}
				if (HitIslandIndex == INDEX_NONE && !IsScreenPointOverInteractiveHUDPanel(CursorAbsolute))
				{
					if (UGameInstance* GI = GetGameInstance())
					{
						if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
						{
							if (Nav->HasSelectedIsland())
							{
								HitIslandIndex = Nav->GetSelectedIslandIndex();
							}
						}
					}
				}
			}
			if (bShiftDown && HitIslandIndex != INDEX_NONE && !IsViewportIslandSelectionBlocked())
			{
				if (UGameInstance* GI = GetGameInstance())
				{
					UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
					UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>();
					if (Nav && Tuning)
					{
						if (Nav->GetSelectedIslandIndex() != HitIslandIndex)
						{
							if (Nav->HasSelectedIsland() && HasUncommittedIslandDraft())
							{
								RequestFocusIsland(HitIslandIndex);
							}
							else
							{
								CommitSelectionChange(HitIslandIndex);
								SetIslandSelectionVisualVisible(true);
								Tuning->LoadActiveIslandFromSelection();
								UpdateEditingHint();
							}
						}
						else
						{
							bShowIslandSelectionVisual = true;
						}

						if (!bAwaitingConfirmRevert && Nav->GetSelectedIslandIndex() == HitIslandIndex)
						{
							if (Tuning->GetActiveIslandIndex() != HitIslandIndex)
							{
								Tuning->SyncActiveFromIsland(HitIslandIndex);
							}
							bIslandDragActive = true;
							IslandDragIndex = HitIslandIndex;
							LaneFlashRemainingSec = 0.f;
							ApplyFreeMouseViewportSettings();
							if (const AIH_WB_Demo004GameMode* GM = GetWorld()->GetAuthGameMode<AIH_WB_Demo004GameMode>())
							{
								if (const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(HitIslandIndex))
								{
									const FVector2D IslandCenterCm(
										Island->GetActorLocation().X, Island->GetActorLocation().Y);
									const FVector2D SeedCenter = GM->GetSeedBaseCenterCm(HitIslandIndex);
									IslandDragStartOffsetCm = IslandCenterCm - SeedCenter;
									SetLastValidDraftOffsetCm(HitIslandIndex, IslandDragStartOffsetCm);

									FIHIslandManualTransform Transform = Tuning->GetActiveManualTransform();
									Transform.OffsetXYCm = IslandDragStartOffsetCm;
									Transform.bUserMoved = true;
									Tuning->SetDraftManualTransformPreview(Transform);

									FVector CursorWorldAtDown;
									if (TryGetWorldPointOnWaterPlane(ViewportCur, CursorWorldAtDown))
									{
										IslandDragStartWorldCm = FVector2D(CursorWorldAtDown.X, CursorWorldAtDown.Y);
									}
									else
									{
										IslandDragStartWorldCm = IslandCenterCm;
									}
								}
							}
							bLeftMouseConsumedByHUDPanel = true;
						}
					}
				}
			}
		}

		if (!bLeftMouseConsumedByHUDPanel && !bLeftMouseStartedOverMinimap && !bMinimapPointerCapture)
		{
			bool bConsumedBySliderPanel = false;
			bool bOverIslandNavPanel = false;
			if (IslandNavWidget)
			{
				bOverIslandNavPanel = IslandNavWidget->HandleScreenPointerDown(CursorAbsolute);
			}

			if (CoastlineTuningWidget && CoastlineTuningWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				if (GameSpeedWidget && GameSpeedWidget->IsKeyboardFocusActive())
				{
					GameSpeedWidget->CancelKeyboardFocus();
				}
				bLeftMouseConsumedByHUDPanel = true;
				bHUDSliderPointerCapture = CoastlineTuningWidget->IsSliderCapturingInput();
				bConsumedBySliderPanel = true;
			}
			else if (PlaceShipWidget && PlaceShipWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (MannequinWidget && MannequinWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (TopDownViewWidget && TopDownViewWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				// Same one-shot teleport+snap pattern as ih.CameraTopDown (IH_WB_Demo004GameMode.cpp) -
				// toggled here instead of one-way, so there's an actual "exit top-down" the user asked
				// for, not just documentation of a console command.
				if (APawn* ControlledPawn = GetPawn())
				{
					if (TopDownViewWidget->IsTopDownActive())
					{
						constexpr float TopDownAltitudeMeters = 300.f;
						PreTopDownViewLocation = ControlledPawn->GetActorLocation();
						PreTopDownViewRotation = GetControlRotation();
						FVector TopDownLoc = PreTopDownViewLocation;
						TopDownLoc.Z = TopDownAltitudeMeters * 100.f;
						ControlledPawn->SetActorLocation(TopDownLoc, false, nullptr, ETeleportType::TeleportPhysics);
						// Plan Addendum 14: Yaw=90 to match the minimap's +Y=north=up convention.
						SetControlRotation(FRotator(-90.f, 90.f, 0.f));
					}
					else
					{
						ControlledPawn->SetActorLocation(
							PreTopDownViewLocation, false, nullptr, ETeleportType::TeleportPhysics);
						SetControlRotation(PreTopDownViewRotation);
					}
				}
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (GameSpeedWidget && GameSpeedWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				if (CoastlineTuningWidget && CoastlineTuningWidget->IsKeyboardFocusActive())
				{
					CoastlineTuningWidget->CancelKeyboardFocus();
				}
				bLeftMouseConsumedByHUDPanel = true;
				bHUDSliderPointerCapture = GameSpeedWidget->IsSliderCapturingInput();
				bConsumedBySliderPanel = true;
			}
#if !UE_BUILD_SHIPPING
			else if (DevViewWidget && DevViewWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
#endif
			else if (WeatherPreviewWidget && WeatherPreviewWidget->IsPanelVisible()
				&& WeatherPreviewWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				// No keyboard-focus/slider-capture machinery — native UComboBoxString/UButton
				// handle their own click/keyboard, this just gates world-click for this frame.
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (PlayAtmosphericsWidget && PlayAtmosphericsWidget->IsPanelVisible()
				&& PlayAtmosphericsWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (TemplateGalleryWidget && TemplateGalleryWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (DevSeedPanelWidget && DevSeedPanelWidget->HandleScreenPointerDown(CursorAbsolute))
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}
			else if (bOverIslandNavPanel)
			{
				bLeftMouseConsumedByHUDPanel = true;
				bConsumedBySliderPanel = true;
			}

			if (!bConsumedBySliderPanel)
			{
				bool bStartedTownGridInteraction = false;
				bool bPaletteDragActive = false;
				if (UGameInstance* InputGI = GetGameInstance())
				{
					if (UIH_BuildPaletteSubsystem* PaletteSubsystem = InputGI->GetSubsystem<UIH_BuildPaletteSubsystem>())
					{
						bPaletteDragActive = PaletteSubsystem->IsDragActive();
					}
				}

				if (!bPaletteDragActive)
				{
					FVector WorldPoint = FVector::ZeroVector;
					if (TryTraceTerrainAtScreen(ViewportCur, WorldPoint))
					{
						AIH_TownGridManager* HitTownGrid = nullptr;
						if (!TryFindTownGridManagerAtScreen(ViewportCur, HitTownGrid))
						{
							if (AIH_TownGridManager* SelectedManager = SelectedTownGridManager.Get())
							{
								if (SelectedManager->ContainsWorldPointXY(WorldPoint))
								{
									HitTownGrid = SelectedManager;
								}
							}
						}

						if (HitTownGrid)
						{
							SelectTownGridManager(HitTownGrid);

							EIHTownGridGripHandle Grip = EIHTownGridGripHandle::None;
							if (HitTownGrid->TryHitGripAtWorld(WorldPoint, Grip))
							{
								HitTownGrid->BeginGripDrag(Grip, WorldPoint);
								bStartedTownGridInteraction = true;
							}
							else if (HitTownGrid->ContainsWorldPointXY(WorldPoint))
							{
								HitTownGrid->BeginMoveDrag(WorldPoint);
								bTownGridMovePointerCapture = true;
								bStartedTownGridInteraction = true;
							}
						}
					}
				}

				if (bPaletteDragActive)
				{
					bLeftMouseConsumedByHUDPanel = true;
				}
				else if (bStartedTownGridInteraction)
				{
					bLeftMouseConsumedByHUDPanel = true;
				}

				if (!bStartedTownGridInteraction && !bPaletteDragActive
					&& !IsScreenPointOverInteractiveHUDPanel(CursorAbsolute))
				{
					CancelActiveHUDKeyboardFocus();
				}
			}
		}
	}

	if (bMinimapPointerCapture && bLeftMouseNow && Minimap)
	{
		if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
		{
			MinimapWidget->HandleScreenPointerMove(CursorAbsolute);
		}
	}

	if (bHUDSliderPointerCapture && bLeftMouseNow)
	{
		TickHUDSliderPointerMove(CursorAbsolute);
	}

	if (bIslandDragActive && bLeftMouseNow)
	{
		FVector WorldPoint;
		if (TryGetWorldPointOnWaterPlane(ViewportCur, WorldPoint))
		{
			const FVector2D Delta(WorldPoint.X - IslandDragStartWorldCm.X, WorldPoint.Y - IslandDragStartWorldCm.Y);
			const FVector2D ProposedOffset = IslandDragStartOffsetCm + Delta;
			ApplyIslandDragOffsetPreview(IslandDragIndex, ProposedOffset);

			int32 ViolatingIndex = INDEX_NONE;
			if (IsIslandOffsetPlacementValid(IslandDragIndex, ProposedOffset, ViolatingIndex))
			{
				SetLastValidDraftOffsetCm(IslandDragIndex, ProposedOffset);
				LaneFlashRemainingSec = 0.f;
			}
			else
			{
				LaneFlashOtherIndex = ViolatingIndex != INDEX_NONE ? ViolatingIndex : IslandDragIndex;
				LaneFlashRemainingSec = LaneFlashDurationSec;
			}
		}
	}

	if (bStampMovePointerCapture && bLeftMouseNow && BuildPalette)
	{
		BuildPalette->UpdateStampMoveDrag(this, ViewportCur);
	}

	if (bIslandDragActive)
	{
		ApplyFreeMouseViewportSettings();
	}

	const bool bStructureBuildDragActive = BuildPalette && BuildPalette->IsStructureBuildDragActive();
	if (bLeftMouseDown && LassoWidget && !bLeftMouseStartedOverMinimap && !bMinimapPointerCapture
		&& !bHUDSliderPointerCapture && !bBuildPalettePointerCapture && !bLeftMouseConsumedByHUDPanel
		&& !bIslandDragActive && !bBuildPaletteDragFromPalette
		&& !(BuildPalette && BuildPalette->IsDragActive()) && !bStructureBuildDragActive)
	{
		const float DragDistViewport = FVector2D::Distance(ViewportCur, LeftMouseDragStart);
		const float DragDistAbsolute = FVector2D::Distance(CursorAbsolute, LeftMouseDragStartAbsolute);
		const bool bDragActive = DragDistViewport >= DragSelectThresholdPx
			|| DragDistAbsolute >= DragSelectThresholdPx;
		if (bDragActive && !bLassoDragCaptureActive)
		{
			BeginLassoDragCapture();
		}
		LassoWidget->SetDragRect(LeftMouseDragStartAbsolute, CursorAbsolute, bDragActive);
	}

	if (bLeftMouseJustReleased)
	{
		const bool bReleaseConsumedByHUD = bLeftMouseConsumedByHUDPanel;

		FVector2D ReleaseViewportPick = ViewportCur;
		if (bHasViewportMouse)
		{
			TryGetViewportMousePosition(ReleaseViewportPick);
		}

		if (bIslandDragActive)
		{
			const int32 FinishedDragIndex = IslandDragIndex;
			bIslandDragActive = false;
			IslandDragIndex = INDEX_NONE;
			FinalizeIslandDrag(FinishedDragIndex);
		}

		if (bHUDSliderPointerCapture)
		{
			FinishHUDSliderPointerUp(CursorAbsolute);
			bHUDSliderPointerCapture = false;
			ReleaseUnwantedMouseCapture();
		}

		if (bMinimapPointerCapture && Minimap)
		{
			if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
			{
				MinimapWidget->HandleScreenPointerUp(CursorAbsolute);
			}
			bMinimapPointerCapture = false;
		}

		if (BuildPalette && (bBuildPalettePointerCapture || BuildPalette->IsDragActive()))
		{
			if (BuildPalette->IsDragActive())
			{
				bool bPlaced = false;
				if (BuildPalette->IsStructureBuildDragActive())
				{
					if (BuildPalette->HasValidDragGhostLocation())
					{
						bPlaced = BuildPalette->TryCommitStructureDropAtStoredPlacement(this);
					}
					if (!bPlaced)
					{
						BuildPalette->CancelDrag();
					}
				}
				else if (BuildPalette->IsTerrainStampDragActive())
				{
					if (BuildPalette->HasValidDragGhostLocation())
					{
						bPlaced = BuildPalette->TryCommitTerrainStampDropAtStoredPlacement(this);
					}
					if (!bPlaced)
					{
						BuildPalette->CancelDrag();
					}
				}
				else
				{
					if (Minimap && Minimap->IsMinimapOpen())
					{
						if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
						{
							FVector2D WorldXY = FVector2D::ZeroVector;
							if (MinimapWidget->TryGetWorldXYFromScreen(CursorAbsolute, WorldXY))
							{
								bPlaced = BuildPalette->TryCompleteDropAtWorldXY(this, WorldXY);
							}
						}
					}
					if (!bPlaced)
					{
						bPlaced = BuildPalette->TryCompleteDropAtScreen(this, ViewportCur);
					}
					if (!bPlaced)
					{
						BuildPalette->CancelDrag();
					}
				}
			}
			bBuildPalettePointerCapture = false;
			bBuildPaletteDragFromPalette = false;
		}

		if (bLeftMouseDown)
		{
			bLeftMouseDown = false;
			const bool bStartedOverMinimap = bLeftMouseStartedOverMinimap;
			bLeftMouseStartedOverMinimap = false;

			if (LassoWidget)
			{
				LassoWidget->SetDragRect(FVector2D::ZeroVector, FVector2D::ZeroVector, false);
			}

			if (!bStartedOverMinimap && !bReleaseConsumedByHUD)
			{
				HandleLeftMouseRelease(ReleaseViewportPick);
			}
		}
		bLeftMouseConsumedByHUDPanel = false;
		EndLassoDragCapture();
		ReleaseUnwantedMouseCapture();
	}
}

bool AIH_Cube2FlyPlayerController::DeprojectScreenToWorldRay(
	const FVector2D& ScreenPos,
	FVector& OutOrigin,
	FVector& OutDirection) const
{
	return DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, OutOrigin, OutDirection);
}

AActor* AIH_Cube2FlyPlayerController::TraceSelectableShipAtScreen(const FVector2D& ScreenPos) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		return nullptr;
	}

	const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C07ShipClick), true, this);

	TArray<FHitResult> Hits;
	if (World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params))
	{
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		for (const FHitResult& Hit : Hits)
		{
			if (AActor* HitActor = Hit.GetActor())
			{
				if (HitActor->Implements<UIH_P1C07_SelectableShip>())
				{
					return HitActor;
				}
			}
			if (UPrimitiveComponent* Comp = Hit.GetComponent())
			{
				if (AActor* HitOwner = Comp->GetOwner())
				{
					if (HitOwner->Implements<UIH_P1C07_SelectableShip>())
					{
						return HitOwner;
					}
				}
			}
		}
	}

	return nullptr;
}

AActor* AIH_Cube2FlyPlayerController::FindNearestRegisteredShipAtScreen(
	const FVector2D& ScreenPos,
	const float ScreenRadiusPx) const
{
	UGameInstance* GI = GetGameInstance();
	if (!GI || ScreenRadiusPx <= 0.f)
	{
		return nullptr;
	}
	UIH_P1C07_ShipRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>();
	if (!Registry)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(ScreenRadiusPx);
	for (const TWeakObjectPtr<AActor>& Ptr : Registry->GetRegisteredShips())
	{
		AActor* Ship = Ptr.Get();
		if (!Ship || !Ship->Implements<UIH_P1C07_SelectableShip>())
		{
			continue;
		}
		FVector2D ShipScreen;
		if (!ProjectWorldLocationToScreen(Ship->GetActorLocation(), ShipScreen, true))
		{
			continue;
		}
		const float DistSq = FVector2D::DistSquared(ScreenPos, ShipScreen);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Ship;
		}
	}
	return Best;
}

bool AIH_Cube2FlyPlayerController::TryPlaceMerchantmanAtScreen(const FVector2D& ScreenPos)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!PlaceShipWidget || !PlaceShipWidget->IsPlaceModeActive())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	if (!World)
	{
		return false;
	}

	UIH_P1C07_IslandCollisionSubsystem* IslandCollision = GI
		? GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>()
		: nullptr;

	FVector WorldOrigin;
	FVector WorldDirection;
	FVector CandidatePoint = FVector::ZeroVector;
	bool bFoundCandidate = false;
	FVector RawHitPointDiag = FVector::ZeroVector;
	const TCHAR* PathDiag = TEXT("none");
	if (DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaceShipClick), true, this);
		TArray<FHitResult> Hits;
		World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params);
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() && Hit.GetActor()->Implements<UIH_P1C07_SelectableShip>())
			{
				continue;
			}
			if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(UIH_P1C07_IslandCollisionSubsystem::IslandActorTag))
			{
				if (Hit.ImpactPoint.Z > 150.f)
				{
					continue;
				}
			}
			FVector OpenOceanDest;
			if (IH_P1C07WaterQuery::ResolveOpenOceanMoveDestination(
					World, Hit.ImpactPoint, OpenOceanDest, IslandCollision))
			{
				CandidatePoint = OpenOceanDest;
				bFoundCandidate = true;
				RawHitPointDiag = Hit.ImpactPoint;
				PathDiag = TEXT("trace");
				break;
			}
		}
		if (!bFoundCandidate)
		{
			FVector PlanePoint = FVector::ZeroVector;
			if (TryGetWorldPointOnWaterPlane(ScreenPos, PlanePoint))
			{
				FVector OpenOceanDest;
				if (IH_P1C07WaterQuery::ResolveOpenOceanMoveDestination(
						World, PlanePoint, OpenOceanDest, IslandCollision))
				{
					CandidatePoint = OpenOceanDest;
					bFoundCandidate = true;
					RawHitPointDiag = PlanePoint;
					PathDiag = TEXT("planeFallback");
				}
			}
		}
	}

	if (!bFoundCandidate)
	{
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Place Ship: resolve failed — keep Click Water mode"));
		return false;
	}

	// DEV diagnostic (2026-08-28): pinpoints exactly where the resolved spawn point diverges from
	// the click, for the "ship spawns out of frame" regression — screen input, which resolution
	// path fired, the raw (pre-ResolveOpenOceanMoveDestination) hit point, and the final candidate.
	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("Place Ship DIAG: screenPos=(%.0f,%.0f) path=%s rawHit=(%.0f,%.0f,%.0f) candidate=(%.0f,%.0f,%.0f)"),
		ScreenPos.X, ScreenPos.Y, PathDiag,
		RawHitPointDiag.X, RawHitPointDiag.Y, RawHitPointDiag.Z,
		CandidatePoint.X, CandidatePoint.Y, CandidatePoint.Z);

	const float WaterlineOffsetCm =
		GetDefault<AIH_P1C07_MerchantmanShipActor>()->DefaultWaterlineOffsetZCm;
	const FVector ShipLoc(CandidatePoint.X, CandidatePoint.Y, CandidatePoint.Z + WaterlineOffsetCm);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIH_P1C07_MerchantmanShipActor* Ship = World->SpawnActor<AIH_P1C07_MerchantmanShipActor>(
		AIH_P1C07_MerchantmanShipActor::StaticClass(), ShipLoc, FRotator::ZeroRotator, Params);
	if (!Ship)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Place Ship: SpawnActor failed"));
		return false;
	}
#if WITH_EDITOR
	Ship->SetActorLabel(TEXT("P1C07_Merchantman_Placed"));
#endif
	if (UIH_P1C07_ShipRegistrySubsystem* Registry =
			GI ? GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>() : nullptr)
	{
		TArray<AActor*> Sel;
		Sel.Add(Ship);
		Registry->SetSelection(Sel);
	}
	PlaceShipWidget->ClearPlaceMode();
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Place Ship: Merchantman at (%.0f,%.0f,%.0f)"),
		ShipLoc.X, ShipLoc.Y, ShipLoc.Z);
	return true;
#endif
}

bool AIH_Cube2FlyPlayerController::TryPlaceMannequinAtScreen(const FVector2D& ScreenPos)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!MannequinWidget || !MannequinWidget->IsPlaceModeActive())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Opposite of Place Ship's dry-land rejection: accept the first hit tagged as island terrain
	// (any Z — mannequins stand anywhere on land, unlike the ship's open-ocean-only placement).
	FVector WorldOrigin;
	FVector WorldDirection;
	FVector CandidatePoint = FVector::ZeroVector;
	bool bFoundCandidate = false;
	if (DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaceMannequinClick), true, this);
		TArray<FHitResult> Hits;
		World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params);
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(UIH_P1C07_IslandCollisionSubsystem::IslandActorTag))
			{
				CandidatePoint = Hit.ImpactPoint;
				bFoundCandidate = true;
				break;
			}
		}
	}

	// ShelfMesh (the Sea Shelf WWF band) is SetCollisionEnabled(NoCollision) by design
	// (IH_WB_IslandActor.cpp) — a line trace can never hit it, so a click over the shelf always
	// misses the loop above even though it's canonically "walkable" terrain for dev purposes. Fall
	// back to the same flat water-plane math Place Ship already uses (TryGetWorldPointOnWaterPlane)
	// so Mannequin can be placed anywhere on the WWF too, standing at approximately sea level there.
	if (!bFoundCandidate)
	{
		bFoundCandidate = TryGetWorldPointOnWaterPlane(ScreenPos, CandidatePoint);
	}

	if (!bFoundCandidate)
	{
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Mannequin: resolve failed (no land hit) — keep Click Land mode"));
		return false;
	}

	const float CapsuleHalfHeightCm =
		GetDefault<AIH_P1C08_MannequinActor>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector SpawnLoc(CandidatePoint.X, CandidatePoint.Y, CandidatePoint.Z + CapsuleHalfHeightCm);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIH_P1C08_MannequinActor* Mannequin = World->SpawnActor<AIH_P1C08_MannequinActor>(
		AIH_P1C08_MannequinActor::StaticClass(), SpawnLoc, FRotator::ZeroRotator, Params);
	if (!Mannequin)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Mannequin: SpawnActor failed"));
		return false;
	}
#if WITH_EDITOR
	Mannequin->SetActorLabel(TEXT("P1C08_Mannequin_Placed"));
#endif
	MannequinWidget->ClearPlaceMode();
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Mannequin: placed at (%.0f,%.0f,%.0f)"),
		SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);
	return true;
#endif
}

bool AIH_Cube2FlyPlayerController::TryIssueMoveOrderAtScreen(
	const FVector2D& ScreenPos,
	UIH_P1C07_ShipRegistrySubsystem* Registry,
	const bool bAppendWaypoint)
{
	if (!Registry || Registry->GetSelectedShips().Num() == 0)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		return false;
	}

	UIH_P1C07_IslandCollisionSubsystem* IslandCollision = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		IslandCollision = GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>();
	}

	const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C07ShipClick), true, this);
	TArray<FHitResult> Hits;
	World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

	FVector CandidatePoint = WorldOrigin;
	bool bFoundCandidate = false;
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.GetActor() && Hit.GetActor()->Implements<UIH_P1C07_SelectableShip>())
		{
			continue;
		}

		CandidatePoint = Hit.ImpactPoint;

		if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(UIH_P1C07_IslandCollisionSubsystem::IslandActorTag))
		{
			FVector SurfaceLoc;
			FVector Normal;
			FVector Velocity;
			if (!IH_P1C07WaterQuery::QueryBestSurfaceInWorld(World, Hit.ImpactPoint, SurfaceLoc, Normal, Velocity))
			{
				continue; // keep searching multi-hits / water-plane fallback
			}

			// Dry land / high coastline — skip this hit, try later water hits.
			if (Hit.ImpactPoint.Z > SurfaceLoc.Z + 150.f)
			{
				continue;
			}
		}

		FVector OpenOceanDest;
		if (IH_P1C07WaterQuery::ResolveOpenOceanMoveDestination(World, CandidatePoint, OpenOceanDest, IslandCollision))
		{
			CandidatePoint = OpenOceanDest;
			bFoundCandidate = true;
			break;
		}
	}

	if (!bFoundCandidate)
	{
		const float T = (0.f - WorldOrigin.Z) / WorldDirection.Z;
		if (T > 0.f)
		{
			CandidatePoint = WorldOrigin + WorldDirection * T;
		}

		FVector OpenOceanDest;
		if (!IH_P1C07WaterQuery::ResolveOpenOceanMoveDestination(World, CandidatePoint, OpenOceanDest, IslandCollision))
		{
			return false;
		}
		CandidatePoint = OpenOceanDest;
	}

	const FRotator Approach(0.f, GetControlRotation().Yaw, 0.f);
	const bool bIssued = Registry->IssueMoveOrderToSelection(this, CandidatePoint, Approach, bAppendWaypoint);
	if (bIssued)
	{
		NavDebugDrawRemainingSec = FMath::Max(NavDebugDrawRemainingSec, NavDebugDrawAfterMoveOrderSec);
	}
	return bIssued;
}

void AIH_Cube2FlyPlayerController::HandleRightMouseReleaseForShipOrders(const FVector2D& ViewportPick)
{
	UGameInstance* GI = GetGameInstance();
	UIH_P1C07_ShipRegistrySubsystem* Registry =
		GI ? GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>() : nullptr;
	if (!Registry || Registry->GetSelectedShips().Num() == 0)
	{
		return;
	}

	const float DragDist = FVector2D::Distance(ViewportPick, RightMouseDragStart);
	if (DragDist >= DragSelectThresholdPx)
	{
		// Reserved: Shift+RMB+drag curve path (future). v1 ignores drag while selected.
		return;
	}

	const bool bAppend =
		IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if (TryIssueMoveOrderAtScreen(ViewportPick, Registry, bAppend))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Phase ship move order — RMB %s selected=%d"),
			bAppend ? TEXT("append") : TEXT("replace"),
			Registry->GetSelectedShips().Num());
#endif
	}
	else
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Phase ship move order — RMB failed (selection kept) selected=%d"),
			Registry->GetSelectedShips().Num());
#endif
	}
}

void AIH_Cube2FlyPlayerController::UpdateNavCollisionDebugDraw(float DeltaTime)
{
	UGameInstance* GI = GetGameInstance();
	UIH_P1C07_ShipRegistrySubsystem* Registry =
		GI ? GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>() : nullptr;
	const TArray<AActor*> Selected = Registry ? Registry->GetSelectedShips() : TArray<AActor*>();

	bool bShowNavDebug = bNavDebugDrawPersistent || Selected.Num() > 0;
	if (NavDebugDrawRemainingSec > 0.f)
	{
		NavDebugDrawRemainingSec = FMath::Max(0.f, NavDebugDrawRemainingSec - DeltaTime);
		bShowNavDebug = true;
	}

	if (!bShowNavDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !GI)
	{
		return;
	}

	TArray<AActor*> ShipsToDraw;
	if (Selected.Num() > 0)
	{
		ShipsToDraw = Selected;
	}
	else if (Registry)
	{
		for (const TWeakObjectPtr<AActor>& Ptr : Registry->GetRegisteredShips())
		{
			if (Ptr.IsValid())
			{
				ShipsToDraw.Add(Ptr.Get());
			}
		}
	}

	if (UIH_P1C07_NavAvoidanceSubsystem* Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>())
	{
		Avoidance->DrawCollisionEnvelopesDebug(World, ShipsToDraw, FColor::Orange, DeltaTime + 0.05f);
	}
}

void AIH_Cube2FlyPlayerController::DrawDevMousePointerEchoWorld(
	UWorld* World,
	const FVector2D& ViewportCur,
	float DeltaTime)
{
	if (!World)
	{
		return;
	}

	(void)DeltaTime;

	if (IH_Cube2FlyPlayerControllerPrivate::CVarDevDrawMousePointerEcho.GetValueOnGameThread() == 0)
	{
		return;
	}

	FVector TraceImpact = FVector::ZeroVector;
	const bool bHitTerrain = TryTraceTerrainAtScreen(ViewportCur, TraceImpact);
	FVector EchoPoint = TraceImpact;
	FColor EchoColor(0, 255, 255, 255);
	if (!bHitTerrain)
	{
		if (!TryGetWorldPointOnWaterPlane(ViewportCur, EchoPoint))
		{
			FVector RayOrigin = FVector::ZeroVector;
			FVector RayDirection = FVector::ZeroVector;
			if (DeprojectScreenToWorldRay(ViewportCur, RayOrigin, RayDirection))
			{
				EchoPoint = RayOrigin + RayDirection * 25000.f;
				EchoColor = FColor(255, 0, 255, 255);
			}
			else
			{
				return;
			}
		}
		else
		{
			EchoColor = FColor(255, 220, 0, 255);
		}
	}

	bool bBuildDragActive = false;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			bBuildDragActive = BuildPalette->IsDragActive()
				&& BuildPalette->GetDragPayload().paletteTab == EIHBuildPaletteTab::Build;
		}
	}

	if (bBuildDragActive)
	{
		EchoColor = FColor(100, 180, 255, 255);
	}

	constexpr uint8 EchoDPG = 0;
	const float SphereRadius = bBuildDragActive ? 450.f : 350.f;
	DrawDebugSphere(World, EchoPoint, SphereRadius, 16, EchoColor, false, -1.f, EchoDPG, 30.f);
	DrawDebugLine(
		World,
		EchoPoint,
		EchoPoint + FVector(0.f, 0.f, 8000.f),
		EchoColor,
		false,
		-1.f,
		EchoDPG,
		35.f);
	DrawDebugLine(
		World,
		EchoPoint + FVector(-600.f, 0.f, 0.f),
		EchoPoint + FVector(600.f, 0.f, 0.f),
		EchoColor,
		false,
		-1.f,
		EchoDPG,
		25.f);
	DrawDebugLine(
		World,
		EchoPoint + FVector(0.f, -600.f, 0.f),
		EchoPoint + FVector(0.f, 600.f, 0.f),
		EchoColor,
		false,
		-1.f,
		EchoDPG,
		25.f);
}

void AIH_Cube2FlyPlayerController::TickDevMousePointerEcho(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (IH_Cube2FlyPlayerControllerPrivate::CVarDevDrawMousePointerEcho.GetValueOnGameThread() == 0)
	{
		return;
	}

	FVector2D ViewportCur = FVector2D::ZeroVector;
	if (!TryGetViewportMousePosition(ViewportCur))
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!GetMousePosition(MouseX, MouseY))
		{
			return;
		}
		ViewportCur = FVector2D(MouseX, MouseY);
	}

	DrawDevMousePointerEchoWorld(World, ViewportCur, DeltaTime);
}

void AIH_Cube2FlyPlayerController::HandleLeftMouseRelease(const FVector2D& ViewportPick)
{
	if (!GetWorld() || !GetLocalPlayer())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();

	const float DragDistViewport = FVector2D::Distance(ViewportPick, LeftMouseDragStart);
	const bool bShortClick = DragDistViewport < DragSelectThresholdPx;

	if (GI)
	{
		if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (BuildPalette->IsDragActive() && bShortClick)
			{
				if (BuildPalette->IsStructureBuildDragActive())
				{
					if (BuildPalette->HasValidDragGhostLocation())
					{
						BuildPalette->TryCommitStructureDropAtStoredPlacement(this);
					}
					else
					{
						BuildPalette->CancelDrag();
					}
				}
				else
				{
					bool bPlaced = false;
					if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
					{
						if (Minimap->IsMinimapOpen())
						{
							if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
							{
								FVector2D WorldXY = FVector2D::ZeroVector;
								const FVector2D CursorAbsolute = FSlateApplication::IsInitialized()
									? FSlateApplication::Get().GetCursorPos()
									: LeftMouseDragStartAbsolute;
								if (MinimapWidget->TryGetWorldXYFromScreen(CursorAbsolute, WorldXY))
								{
									bPlaced = BuildPalette->TryCompleteDropAtWorldXY(this, WorldXY);
								}
							}
						}
					}
					if (!bPlaced)
					{
						BuildPalette->TryCompleteDropAtScreen(this, ViewportPick);
					}
				}
				bBuildPalettePointerCapture = false;
				bBuildPaletteDragFromPalette = false;
				return;
			}
		}
	}

	const bool bHadTownGridMoveCapture = bTownGridMovePointerCapture;
	if (AIH_TownGridManager* SelectedGrid = SelectedTownGridManager.Get())
	{
		SelectedGrid->EndMoveDrag();
		SelectedGrid->EndGripDrag();
	}
	bTownGridMovePointerCapture = false;

	if (bStampMovePointerCapture)
	{
		if (GI)
		{
			if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
			{
				BuildPalette->EndStampMoveDrag();
			}
		}
		bStampMovePointerCapture = false;
	}

	bBuildPalettePointerCapture = false;
	bBuildPaletteDragFromPalette = false;

	const bool bShiftSelect =
		IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

	if (!bShortClick)
	{
		if (bHadTownGridMoveCapture)
		{
			return;
		}
		if (GI)
		{
			if (UIH_P1C07_ShipRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())
			{
				// LMB drag = replace box; Shift+drag = additive union (IH select/move canon).
				Registry->SelectShipsInScreenRect(this, LeftMouseDragStart, ViewportPick, bShiftSelect);
			}
		}
		return;
	}

	const bool bWorldStampEditMode = IsViewportIslandSelectionBlocked();

#if !UE_BUILD_SHIPPING
	// Place Ship: consume click only on successful spawn; fail falls through so sail still works.
	if (PlaceShipWidget && PlaceShipWidget->IsPlaceModeActive())
	{
		if (TryPlaceMerchantmanAtScreen(ViewportPick))
		{
			return;
		}
	}
	// Mannequin: same consume-only-on-success pattern, land instead of water.
	if (MannequinWidget && MannequinWidget->IsPlaceModeActive())
	{
		if (TryPlaceMannequinAtScreen(ViewportPick))
		{
			return;
		}
	}
#endif

	UIH_P1C07_ShipRegistrySubsystem* Registry = GI
		? GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>()
		: nullptr;

	// Ship pick (before island / move).
	if (Registry)
	{
		AActor* HitShip = TraceSelectableShipAtScreen(ViewportPick);
		if (!HitShip)
		{
			HitShip = FindNearestRegisteredShipAtScreen(ViewportPick, ShipScreenPickRadiusPx);
		}
		if (HitShip)
		{
			if (bShiftSelect)
			{
				Registry->ToggleSelectShip(HitShip);
			}
			else
			{
				TArray<AActor*> Sel;
				Sel.Add(HitShip);
				Registry->SetSelection(Sel);
			}
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("Phase ship select — actor=%s selectedCount=%d shift=%d"),
				*HitShip->GetName(), Registry->GetSelectedShips().Num(), bShiftSelect ? 1 : 0);
#endif
			return;
		}
	}

	// When ships selected: LMB is select-layer only (TW-style). Land → island; empty → deselect.
	if (Registry && Registry->GetSelectedShips().Num() > 0)
	{
		int32 LandIslandIndex = INDEX_NONE;
		if (!TryResolveIslandIndexAtScreen(ViewportPick, LandIslandIndex))
		{
			Registry->ClearSelection();
#if !UE_BUILD_SHIPPING
			UE_LOG(LogIH_WB_Demo004, Log,
				TEXT("Phase ship deselect — LMB empty (sail continues)"));
#endif
			return;
		}
		// Land hit: clear ships then fall through to island select.
		Registry->ClearSelection();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogIH_WB_Demo004, Log,
			TEXT("Phase ship deselect + island click — island=%d"), LandIslandIndex);
#endif
	}

	if (!bWorldStampEditMode)
	{
		if (TryHandleIslandSelectionClickAtViewport(ViewportPick))
		{
			return;
		}
	}
	else if (GI)
	{
		if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (BuildPalette->TryHandleStampSelectionClickAtViewport(this, ViewportPick))
			{
				return;
			}
		}
	}

	AIH_TownGridManager* HitTownGrid = nullptr;
	if (TryFindTownGridManagerAtScreen(ViewportPick, HitTownGrid))
	{
		SelectTownGridManager(HitTownGrid);
		return;
	}

	if (TryIsOpenWaterClickAtScreen(ViewportPick))
	{
		if (GI)
		{
			if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
			{
				BuildPalette->ClearTerrainStampSelection();
			}
		}
		if (!bWorldStampEditMode)
		{
			RequestDeselectIsland();
		}
#if !UE_BUILD_SHIPPING
		FVector WaterPoint = FVector::ZeroVector;
		TryGetWorldPointOnWaterPlane(ViewportPick, WaterPoint);
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase island open-water click (outside select-core) — waterPlane=%s"),
			*WaterPoint.ToString());
#endif
		return;
	}

	if (GetSelectedTownGridManager())
	{
		DeselectTownGridManager();
	}

	if (!bWorldStampEditMode)
	{
		RequestDeselectIsland();
	}
}

bool AIH_Cube2FlyPlayerController::IsViewportIslandSelectionBlocked() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			return BuildPalette->IsViewportIslandSelectionBlocked();
		}
	}
	return false;
}

bool AIH_Cube2FlyPlayerController::TryHandleIslandSelectionClickAtViewport(const FVector2D& ViewportPick)
{
	if (IsViewportIslandSelectionBlocked())
	{
		int32 BlockedHitIslandIndex = INDEX_NONE;
		const bool bHitIsland = TryResolveIslandIndexAtScreen(ViewportPick, BlockedHitIslandIndex);
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase island click blocked — W fly-out open (hitIsland=%d index=%d)"),
			bHitIsland ? 1 : 0,
			BlockedHitIslandIndex);
#endif
		return bHitIsland;
	}

	int32 HitIslandIndex = INDEX_NONE;
	if (!TryResolveIslandIndexAtScreen(ViewportPick, HitIslandIndex))
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bDoubleClick = HitIslandIndex == LastClickedIslandIndex
		&& (Now - LastIslandClickTimeSec) <= IslandDoubleClickWindowSec;
	LastClickedIslandIndex = HitIslandIndex;
	LastIslandClickTimeSec = Now;

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase island click — index=%d dblClick=%d (surface resolve, viewportPick)"),
		HitIslandIndex,
		bDoubleClick ? 1 : 0);
#endif

	if (bDoubleClick)
	{
		RequestFocusIsland(HitIslandIndex);
		return true;
	}

	const UGameInstance* GI = GetGameInstance();
	const UIH_P1C08_IslandNavSubsystem* Nav = GI
		? GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>()
		: nullptr;
	// Same-island reclick: focus like Nav (do not require InnerCore).
	if (Nav && Nav->HasSelectedIsland() && Nav->GetSelectedIslandIndex() == HitIslandIndex)
	{
		RequestFocusIsland(HitIslandIndex);
		return true;
	}

	RequestFocusIsland(HitIslandIndex);
	return true;
}

bool AIH_Cube2FlyPlayerController::HasUncommittedIslandDraft() const
{
	if (CoastlineTuningWidget && CoastlineTuningWidget->HasUncommittedDraft())
	{
		return true;
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			return Tuning->HasUncommittedDraft();
		}
	}
	return false;
}

void AIH_Cube2FlyPlayerController::RevertActiveIslandDraft()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			Tuning->RevertActiveDraft();
			if (Tuning->HasActiveIsland())
			{
				ApplyDraftTransformPreview(Tuning->GetActiveIslandIndex());
			}
		}
	}
	if (CoastlineTuningWidget)
	{
		CoastlineTuningWidget->SyncSlidersFromActiveTuning();
		CoastlineTuningWidget->UpdateDraftStatusText();
	}
}

void AIH_Cube2FlyPlayerController::CommitActiveIslandDraft()
{
	if (!HasUncommittedIslandDraft())
	{
		return;
	}

	if (CoastlineTuningWidget)
	{
		CoastlineTuningWidget->CommitActiveDraftOnly();
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			Tuning->ApplyActiveDraft();
		}
	}
}

void AIH_Cube2FlyPlayerController::CommitSelectionChange(int32 NewIslandIndex)
{
	if (NewIslandIndex != INDEX_NONE && IsViewportIslandSelectionBlocked())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase island selection blocked — W fly-out open (CommitSelectionChange index=%d)"),
			NewIslandIndex);
#endif
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			Nav->SetSelectedIslandIndex(NewIslandIndex);
		}
	}
	if (NewIslandIndex != INDEX_NONE)
	{
		SetIslandSelectionVisualVisible(true);
		ShowIslandCaptionForNavIndex(NewIslandIndex);
		SyncIslandSelectionMeshGlow();
	}
	else if (IslandCaptionWidget)
	{
		IslandCaptionWidget->HideCaption();
		SyncIslandSelectionMeshGlow();
	}
	PendingSelectionIndex = INDEX_NONE;
}

void AIH_Cube2FlyPlayerController::ApplyPendingSelectionChange()
{
	if (PendingSelectionIndex == INDEX_NONE && !HasUncommittedIslandDraft())
	{
		CommitSelectionChange(INDEX_NONE);
		return;
	}
	CommitSelectionChange(PendingSelectionIndex);
}

void AIH_Cube2FlyPlayerController::ShowConfirmRevertDialog(TFunction<void(bool bRevertConfirmed)> OnComplete)
{
	ShowConfirmDialog(
		TEXT("Confirm Revert"),
		TEXT("Discard uncommitted coastline and position changes?"),
		MoveTemp(OnComplete));
}

void AIH_Cube2FlyPlayerController::ShowConfirmDialog(
	const FString& Title,
	const FString& Body,
	TFunction<void(bool bConfirmed)> OnComplete)
{
	if (!ConfirmRevertWidget)
	{
		OnComplete(true);
		return;
	}

	bAwaitingConfirmRevert = true;
	ConfirmRevertWidget->ShowDialog(
		Title,
		Body,
		FOnConfirmRevertChoice::CreateLambda(
			[this, OnComplete](bool bConfirmed)
			{
				bAwaitingConfirmRevert = false;
				OnComplete(bConfirmed);
			}));
}

void AIH_Cube2FlyPlayerController::SetIslandSelectionVisualVisible(bool bVisible)
{
	if (bVisible && IsViewportIslandSelectionBlocked())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase island selection visual blocked — W fly-out open"));
#endif
		bVisible = false;
	}

	bShowIslandSelectionVisual = bVisible;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->RequestMinimapRepaint();
		}
	}
	SyncIslandSelectionMeshGlow();
}

void AIH_Cube2FlyPlayerController::SyncIslandSelectionMeshGlow()
{
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UIH_P1C08_IslandNavSubsystem* Nav = GI ? GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>() : nullptr;
	const bool bSelectionActive = Nav
		&& Nav->HasSelectedIsland()
		&& !IsViewportIslandSelectionBlocked();
	const int32 SelectedIndex = bSelectionActive ? Nav->GetSelectedIslandIndex() : INDEX_NONE;

	for (AIH_WB_IslandActor* Island : GM->GetSpawnedIslands())
	{
		if (!Island)
		{
			continue;
		}
		const bool bHighlight = bSelectionActive && Island->GetTankIslandIndex() == SelectedIndex;
		Island->SetSelectionHighlighted(bHighlight);
	}
}

bool AIH_Cube2FlyPlayerController::ShouldSuspendWorldCoastStrokeOverlay() const
{
	return bMouseLookActive || bIslandDragActive || bShowIslandSelectionVisual;
}

void AIH_Cube2FlyPlayerController::BeginRealmRegenProgress(const FString& Label)
{
	// Slim path: no center fake progress bar (regen is fast; bar hung at 0.94 + mojibake).
	// Label is retained for call-site compatibility; RealmSeed panel owns status text.
	(void)Label;
	RealmRegenFakeProgress = 0.f;
	RealmRegenProgressShownAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RealmRegenFinishTimer);
		World->GetTimerManager().ClearTimer(RealmRegenProgressTimer);
	}
	if (RealmRegenProgressWidget && RealmRegenProgressWidget->IsInViewport())
	{
		RealmRegenProgressWidget->CompleteAndHide();
	}
}

void AIH_Cube2FlyPlayerController::EndRealmRegenProgress()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RealmRegenProgressTimer);
		World->GetTimerManager().ClearTimer(RealmRegenFinishTimer);
		World->GetTimerManager().ClearTimer(RealmRegenWorkTimer);
	}
	if (RealmRegenProgressWidget && RealmRegenProgressWidget->IsInViewport())
	{
		RealmRegenProgressWidget->CompleteAndHide();
	}
	RealmRegenFakeProgress = 0.f;
}

void AIH_Cube2FlyPlayerController::ScheduleEndRealmRegenProgress()
{
	TFunction<void()> Callback = MoveTemp(PendingRealmRegenCompleteCallback);
	PendingRealmRegenCompleteCallback = nullptr;
	EndRealmRegenProgress();
	if (Callback)
	{
		Callback();
	}
}

void AIH_Cube2FlyPlayerController::HandleRealmRegenWorkTick()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->RegenerateIslandsFromSeed();
		}
	}
	ScheduleEndRealmRegenProgress();
}

void AIH_Cube2FlyPlayerController::HandleRealmRegenFinishTimer()
{
	TFunction<void()> Callback = MoveTemp(PendingRealmRegenCompleteCallback);
	PendingRealmRegenCompleteCallback = nullptr;
	EndRealmRegenProgress();
	if (Callback)
	{
		Callback();
	}
}

void AIH_Cube2FlyPlayerController::TickRealmRegenFakeProgress()
{
	// Overlay retired — no fake percent.
}

void AIH_Cube2FlyPlayerController::StartRealmRegenWork(bool bShowProgress)
{
	(void)bShowProgress;
	BeginRealmRegenProgress(TEXT("Building islands..."));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AIH_Cube2FlyPlayerController::HandleRealmRegenWorkTick));
	}
	else
	{
		HandleRealmRegenWorkTick();
	}
}

void AIH_Cube2FlyPlayerController::PrepareRealmRegenFromSeed(
	const FString& NormalizedSeed, TFunction<void()> OnComplete)
{
	PendingRegenSeedWord = NormalizedSeed;
	PendingRealmRegenCompleteCallback = MoveTemp(OnComplete);
	BeginRealmRegenProgress(TEXT("Preparing realm..."));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AIH_Cube2FlyPlayerController::HandleSeedPanelRegenPrepareTick));
	}
	else
	{
		HandleSeedPanelRegenPrepareTick();
	}
}

void AIH_Cube2FlyPlayerController::HandleSeedPanelRegenPrepareTick()
{
	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		if (!PendingRegenSeedWord.IsEmpty())
		{
			GI->SetCurrentWorldSeed(PendingRegenSeedWord);
			PendingRegenSeedWord.Reset();
		}
	}

	if (HasUncommittedIslandDraft())
	{
		ShowConfirmDialog(
			TEXT("Confirm Regenerate"),
			TEXT("Uncommitted island coastline or position edits will be lost. Regenerate anyway?"),
			[this](bool bConfirmed)
			{
				if (bConfirmed)
				{
					StartRealmRegenWork(false);
				}
				else
				{
					PendingRealmRegenCompleteCallback = nullptr;
					EndRealmRegenProgress();
				}
			});
		return;
	}

	StartRealmRegenWork(false);
}

void AIH_Cube2FlyPlayerController::RequestRegenerateIslandsFromSeed(TFunction<void()> OnComplete)
{
	PendingRealmRegenCompleteCallback = MoveTemp(OnComplete);
	StartRealmRegenWork(true);
}

void AIH_Cube2FlyPlayerController::RequestFocusIsland(int32 IslandIndex)
{
	if (bAwaitingConfirmRevert)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
	{
		if (BuildPalette->IsViewportIslandSelectionBlocked())
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("Phase island focus blocked — W fly-out open (index=%d)"),
				IslandIndex);
#endif
			return;
		}
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	if (!Nav)
	{
		return;
	}

	const int32 Current = Nav->GetSelectedIslandIndex();
	if (Current == IslandIndex)
	{
		SetIslandSelectionVisualVisible(true);
		bCameraFlyActive = false;
		ShowIslandCaptionForNavIndex(IslandIndex);
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			Tuning->LoadActiveIslandFromSelection();
			SetLastValidDraftOffsetCm(IslandIndex, Tuning->GetActiveManualTransform().OffsetXYCm);
		}
		UpdateEditingHint();
		return;
	}

	if (Current != INDEX_NONE && Current != IslandIndex && HasUncommittedIslandDraft())
	{
		CommitActiveIslandDraft();
	}

	CommitSelectionChange(IslandIndex);
	SetIslandSelectionVisualVisible(true);
	bCameraFlyActive = false;
#if !UE_BUILD_SHIPPING
	FString IslandName = FString::Printf(TEXT("Island %d"), IslandIndex + 1);
	if (FIHIslandNavRecord Record; Nav->TryGetNavRecord(IslandIndex, Record) && !Record.Name.IsEmpty())
	{
		IslandName = Record.Name;
	}
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase island selected — index=%d name=%s dblClick=%d"),
		IslandIndex,
		*IslandName,
		Current == IslandIndex ? 1 : 0);
#endif
	UpdateEditingHint();
}

void AIH_Cube2FlyPlayerController::RequestDeselectIsland()
{
	if (bAwaitingConfirmRevert)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	if (!Nav || !Nav->HasSelectedIsland())
	{
		return;
	}

	if (HasUncommittedIslandDraft())
	{
		CommitActiveIslandDraft();
	}

	CommitSelectionChange(INDEX_NONE);
	SetIslandSelectionVisualVisible(false);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase island deselected — click water / Esc"));
#endif
	UpdateEditingHint();
}

void AIH_Cube2FlyPlayerController::ResetIslandViewportDoubleClickTracking()
{
	LastClickedIslandIndex = INDEX_NONE;
	LastIslandClickTimeSec = -1.f;
}

void AIH_Cube2FlyPlayerController::UpdateEditingHint()
{
	if (!IslandEditHintWidget)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		IslandEditHintWidget->ClearHint();
		return;
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	if (!Nav || !Nav->HasSelectedIsland())
	{
		IslandEditHintWidget->ClearHint();
		return;
	}

	FString Name = FString::Printf(TEXT("Island %d"), Nav->GetSelectedIslandIndex() + 1);
	FIHIslandNavRecord Record;
	if (Nav->TryGetNavRecord(Nav->GetSelectedIslandIndex(), Record) && !Record.Name.IsEmpty())
	{
		Name = Record.Name;
	}

	IslandEditHintWidget->SetHintText(FString::Printf(
		TEXT("Editing: %s — Shift+drag move · Shift+wheel rotate · click water to finish · Esc undo · Enter apply"),
		*Name));
}

void AIH_Cube2FlyPlayerController::HandleManualTransformChanged(int32 IslandIndex)
{
	// Preview only — commit path refreshes coast on mouse-up / Enter (Engineering Canon §7).
	const bool bRefreshMinimap = false;
	ApplyDraftTransformPreview(IslandIndex, bRefreshMinimap);
	if (CoastlineTuningWidget && !bIslandDragActive)
	{
		CoastlineTuningWidget->UpdateDraftStatusText();
	}

	if (bRefreshMinimap)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
			{
				Minimap->RequestMinimapRepaint();
			}
		}
	}
}

bool AIH_Cube2FlyPlayerController::TryGetLaneViolationPair(int32& OutIslandA, int32& OutIslandB) const
{
	OutIslandA = INDEX_NONE;
	OutIslandB = INDEX_NONE;
	if (LaneFlashRemainingSec <= 0.f)
	{
		return false;
	}

	const UGameInstance* GI = GetGameInstance();
	const UIH_P1C08_IslandNavSubsystem* Nav = GI ? GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>() : nullptr;
	if (!Nav || !Nav->HasSelectedIsland())
	{
		return false;
	}

	OutIslandA = Nav->GetSelectedIslandIndex();
	OutIslandB = LaneFlashOtherIndex;
	return OutIslandB != INDEX_NONE;
}

void AIH_Cube2FlyPlayerController::ApplyDraftTransformPreview(int32 IslandIndex, bool bRefreshMinimap)
{
	UWorld* World = GetWorld();
	AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			const FIHIslandManualTransform Transform = (Tuning->GetActiveIslandIndex() == IslandIndex)
				? Tuning->GetActiveManualTransform()
				: GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>()->GetCommittedManualTransform(IslandIndex);
			GM->ApplyIslandManualTransform(IslandIndex, Transform, bRefreshMinimap);
		}
	}
}

bool AIH_Cube2FlyPlayerController::TryGetWorldPointOnWaterPlane(const FVector2D& ScreenPos, FVector& OutPoint) const
{
	FVector Origin;
	FVector Direction;
	if (!DeprojectScreenToWorldRay(ScreenPos, Origin, Direction))
	{
		return false;
	}

	if (FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}

	const float T = (0.f - Origin.Z) / Direction.Z;
	if (T <= 0.f)
	{
		return false;
	}

	OutPoint = Origin + Direction * T;
	return true;
}

bool AIH_Cube2FlyPlayerController::TryTraceTerrainAtScreen(const FVector2D& ScreenPos, FVector& OutImpactPoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		return false;
	}

	const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C08BuildPaletteTerrain), true, this);
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (AActor* DragPreview = BuildPalette->GetDragPreviewIgnoreActor())
			{
				Params.AddIgnoredActor(DragPreview);
			}
		}
	}
	TArray<FHitResult> Hits;
	if (World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params))
	{
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor->IsA(AWaterBody::StaticClass()))
			{
				continue;
			}
			if (HitActor->Implements<UIH_P1C07_SelectableShip>())
			{
				continue;
			}
			OutImpactPoint = Hit.ImpactPoint;
			return true;
		}
	}

	return TryGetWorldPointOnWaterPlane(ScreenPos, OutImpactPoint);
}

bool AIH_Cube2FlyPlayerController::TryTraceSolidSurfaceAtScreen(
	const FVector2D& ScreenPos,
	FVector& OutImpactPoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		return false;
	}

	const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C08BuildPaletteSolidTerrain), true, this);
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (AActor* DragPreview = BuildPalette->GetDragPreviewIgnoreActor())
			{
				Params.AddIgnoredActor(DragPreview);
			}
		}
	}

	TArray<FHitResult> Hits;
	if (World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params))
	{
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor->IsA(AWaterBody::StaticClass()))
			{
				continue;
			}
			if (HitActor->Implements<UIH_P1C07_SelectableShip>())
			{
				continue;
			}
			OutImpactPoint = Hit.ImpactPoint;
			return true;
		}
	}

	return false;
}

bool AIH_Cube2FlyPlayerController::TrySampleIslandSurfaceAtScreen(
	const FVector2D& ScreenPos,
	FVector& OutIslandSurface,
	AActor** OutIslandActor) const
{
	FVector ProbePoint = FVector::ZeroVector;
	if (!TryGetWorldPointOnWaterPlane(ScreenPos, ProbePoint))
	{
		if (!TryTraceSolidSurfaceAtScreen(ScreenPos, ProbePoint))
		{
			return false;
		}
	}

	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return false;
	}

	const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
		GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>();
	if (!IslandCollision)
	{
		return false;
	}

	const AActor* IgnoreActor = nullptr;
	if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
	{
		IgnoreActor = BuildPalette->GetDragPreviewIgnoreActor();
	}

	float ReferenceZ = ProbePoint.Z;
	if (const APawn* ViewPawn = GetPawn())
	{
		ReferenceZ = ViewPawn->GetActorLocation().Z;
	}

	return IslandCollision->TrySampleIslandSurfaceAtXY(
		FVector2D(ProbePoint.X, ProbePoint.Y),
		ReferenceZ,
		0.f,
		IgnoreActor,
		OutIslandSurface,
		OutIslandActor);
}

bool AIH_Cube2FlyPlayerController::TryResolveStructurePlacementAtScreen(
	const FVector2D& ScreenPos,
	FName PaletteItemID,
	FVector& OutActorOriginWorld) const
{
	FVector SurfacePoint = FVector::ZeroVector;
	if (!TryTraceSolidSurfaceAtScreen(ScreenPos, SurfacePoint))
	{
		return false;
	}

	FVector PlacementSurface = SurfacePoint;
	if (FVector IslandSurface = FVector::ZeroVector;
		TrySampleIslandSurfaceAtScreen(ScreenPos, IslandSurface))
	{
		PlacementSurface = IslandSurface;
	}

	return AIH_StructurePlacementActor::ComputePlacementOriginFromSurface(
		PaletteItemID, PlacementSurface, OutActorOriginWorld);
}

bool AIH_Cube2FlyPlayerController::TryResolveStructurePlacementAtWorldXY(
	const FVector2D& WorldXY,
	FName PaletteItemID,
	FVector& OutActorOriginWorld) const
{
	float ReferenceZ = 0.f;
	if (const APawn* ViewPawn = GetPawn())
	{
		ReferenceZ = ViewPawn->GetActorLocation().Z;
	}

	const AActor* IgnoreActor = nullptr;
	FVector PlacementSurface = FVector::ZeroVector;
	bool bFoundSurface = false;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			IgnoreActor = BuildPalette->GetDragPreviewIgnoreActor();
		}
		if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
			GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
		{
			FVector IslandSurface = FVector::ZeroVector;
			if (IslandCollision->TrySampleIslandSurfaceAtXY(
				WorldXY, ReferenceZ, 0.f, IgnoreActor, IslandSurface))
			{
				PlacementSurface = IslandSurface;
				bFoundSurface = true;
			}
		}
	}

	if (!bFoundSurface)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		const FVector TraceStart(WorldXY.X, WorldXY.Y, ReferenceZ + 500000.f);
		const FVector TraceEnd(WorldXY.X, WorldXY.Y, ReferenceZ - 500000.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C08BuildPaletteWorldXY), true, this);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)
			&& Hit.GetActor() && !Hit.GetActor()->IsA(AWaterBody::StaticClass()))
		{
			PlacementSurface = Hit.ImpactPoint;
			bFoundSurface = true;
		}
	}

	if (!bFoundSurface)
	{
		return false;
	}

	return AIH_StructurePlacementActor::ComputePlacementOriginFromSurface(
		PaletteItemID, PlacementSurface, OutActorOriginWorld);
}

static float GetIslandLaneRadiusCm(const AIH_WB_IslandActor* Island)
{
	if (!Island)
	{
		return 0.f;
	}

	float FootprintMajor = 0.f;
	float FootprintMinor = 0.f;
	Island->GetWaterlineFootprintCm(FootprintMajor, FootprintMinor);
	if (FootprintMajor <= KINDA_SMALL_NUMBER)
	{
		FootprintMajor = Island->GetSemiMajorAxisCm();
		FootprintMinor = FootprintMajor;
	}

	return FMath::Max(FootprintMajor, FootprintMinor);
}

/** Lane/wall validation during Shift+drag — match seed layout semi-major, not jagged coast extent. */
static float GetIslandDragLaneRadiusCm(const AIH_WB_IslandActor* Island)
{
	return Island ? Island->GetSemiMajorAxisCm() : 0.f;
}

namespace
{
	static bool PointInPolygon2D(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return false;
		}

		bool bInside = false;
		for (int32 i = 0, j = Polygon.Num() - 1; i < Polygon.Num(); j = i++)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[j];
			const bool bCrossesLatitude = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (bCrossesLatitude)
			{
				const float IntersectX = (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X;
				if (Point.X < IntersectX)
				{
					bInside = !bInside;
				}
			}
		}
		return bInside;
	}

	static float DistancePointToSegment2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float Denom = FVector2D::DotProduct(AB, AB);
		if (Denom <= KINDA_SMALL_NUMBER)
		{
			return FVector2D::Distance(Point, A);
		}
		const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / Denom, 0.f, 1.f);
		return FVector2D::Distance(Point, A + AB * T);
	}

	static float MinDistanceToPolygonEdges2D(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 2)
		{
			return TNumericLimits<float>::Max();
		}

		float MinDist = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Polygon.Num(); ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[(i + 1) % Polygon.Num()];
			MinDist = FMath::Min(MinDist, DistancePointToSegment2D(Point, A, B));
		}
		return MinDist;
	}

	static void ExpandPolygonOutward2D(const TArray<FVector2D>& In, float OffsetCm, TArray<FVector2D>& Out)
	{
		Out = In;
		if (In.Num() < 3 || FMath::IsNearlyZero(OffsetCm))
		{
			return;
		}

		float SignedArea = 0.f;
		for (int32 i = 0; i < In.Num(); ++i)
		{
			const FVector2D& A = In[i];
			const FVector2D& B = In[(i + 1) % In.Num()];
			SignedArea += (B.X - A.X) * (B.Y + A.Y);
		}
		const float WindingSign = SignedArea > 0.f ? -1.f : 1.f;

		Out.SetNum(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			const FVector2D& Prev = In[(i - 1 + In.Num()) % In.Num()];
			const FVector2D& Curr = In[i];
			const FVector2D& Next = In[(i + 1) % In.Num()];

			const FVector2D E0 = (Curr - Prev).GetSafeNormal();
			const FVector2D E1 = (Next - Curr).GetSafeNormal();
			const FVector2D N0(-E0.Y * WindingSign, E0.X * WindingSign);
			const FVector2D N1(-E1.Y * WindingSign, E1.X * WindingSign);
			FVector2D Bisector = N0 + N1;
			if (Bisector.IsNearlyZero())
			{
				Bisector = N0;
			}
			else
			{
				Bisector.Normalize();
			}

			const float Dot = FMath::Abs(FVector2D::DotProduct(Bisector, N0));
			const float MiterScale = Dot > 0.01f ? (1.f / Dot) : 1.f;
			Out[i] = Curr + Bisector * OffsetCm * FMath::Min(MiterScale, 4.f);
		}
	}

	static void ScalePolygonFromCentroid2D(const TArray<FVector2D>& In, float Scale, TArray<FVector2D>& Out)
	{
		Out = In;
		if (In.Num() < 3 || FMath::IsNearlyEqual(Scale, 1.f))
		{
			return;
		}

		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& Point : In)
		{
			Centroid += Point;
		}
		Centroid /= static_cast<float>(In.Num());

		Out.SetNum(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out[i] = Centroid + (In[i] - Centroid) * Scale;
		}
	}

	static void DrawDashedDebugLine(
		const UWorld* World,
		const FVector& Start,
		const FVector& End,
		const FColor& Color,
		float Thickness,
		float DashLength,
		float GapLength)
	{
		const FVector Segment = End - Start;
		const float TotalLength = Segment.Size();
		if (TotalLength <= KINDA_SMALL_NUMBER || DashLength <= KINDA_SMALL_NUMBER)
		{
			DrawDebugLine(World, Start, End, Color, false, -1.f, 0, Thickness);
			return;
		}

		const FVector Dir = Segment / TotalLength;
		float Traveled = 0.f;
		while (Traveled < TotalLength)
		{
			const float DashEndDist = FMath::Min(Traveled + DashLength, TotalLength);
			DrawDebugLine(
				World,
				Start + Dir * Traveled,
				Start + Dir * DashEndDist,
				Color,
				false,
				-1.f,
				0,
				Thickness);
			Traveled = DashEndDist + GapLength;
		}
	}
}

bool AIH_Cube2FlyPlayerController::TryGetIslandIndexAtWorldXY(
	const FVector2D& WorldXY,
	int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	int32 BestCoastlineIndex = INDEX_NONE;
	float BestCoastlineDist = IslandCoastlinePickThresholdCm;
	for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
	{
		const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex);
		if (!Island)
		{
			continue;
		}

		TArray<FVector2D> ShorelineWorld;
		Island->GetShorelinePolygonWorldCm(ShorelineWorld);
		if (ShorelineWorld.Num() < 3)
		{
			continue;
		}

		if (PointInPolygon2D(WorldXY, ShorelineWorld))
		{
			OutIslandIndex = IslandIndex;
			return true;
		}

		const float EdgeDist = MinDistanceToPolygonEdges2D(WorldXY, ShorelineWorld);
		if (EdgeDist <= BestCoastlineDist)
		{
			BestCoastlineDist = EdgeDist;
			BestCoastlineIndex = IslandIndex;
		}
	}

	if (BestCoastlineIndex != INDEX_NONE)
	{
		OutIslandIndex = BestCoastlineIndex;
		return true;
	}

	float BestCenterDistSq = TNumericLimits<float>::Max();
	int32 BestCenterIndex = INDEX_NONE;
	for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
	{
		const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex);
		if (!Island)
		{
			continue;
		}

		const FVector2D Center(Island->GetActorLocation().X, Island->GetActorLocation().Y);
		const float PickRadius = GetIslandLaneRadiusCm(Island) + IslandCoastlinePickThresholdCm;
		const float DistSq = FVector2D::DistSquared(WorldXY, Center);
		if (DistSq <= FMath::Square(PickRadius) && DistSq < BestCenterDistSq)
		{
			BestCenterDistSq = DistSq;
			BestCenterIndex = IslandIndex;
		}
	}

	if (BestCenterIndex != INDEX_NONE)
	{
		OutIslandIndex = BestCenterIndex;
		return true;
	}

	return false;
}

bool AIH_Cube2FlyPlayerController::TryGetIslandIndexAtWorldXYSelectCore(
	const FVector2D& WorldXY,
	int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	const float CoreFrac = FMath::Clamp(IHInvisibleHandSpec::WBIslandSelectCoreFraction, 0.05f, 1.f);
	for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
	{
		const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex);
		if (!Island)
		{
			continue;
		}

		TArray<FVector2D> ShorelineWorld;
		Island->GetShorelinePolygonWorldCm(ShorelineWorld);
		if (ShorelineWorld.Num() < 3)
		{
			continue;
		}

		const FVector2D Center(Island->GetActorLocation().X, Island->GetActorLocation().Y);
		TArray<FVector2D> CorePoly;
		CorePoly.Reserve(ShorelineWorld.Num());
		for (const FVector2D& P : ShorelineWorld)
		{
			CorePoly.Add(Center + (P - Center) * CoreFrac);
		}
		if (PointInPolygon2D(WorldXY, CorePoly))
		{
			OutIslandIndex = IslandIndex;
			return true;
		}
	}
	return false;
}

bool AIH_Cube2FlyPlayerController::TryGetIslandIndexAtWorldXYStrict(
	const FVector2D& WorldXY,
	int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
	{
		const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex);
		if (!Island)
		{
			continue;
		}

		TArray<FVector2D> ShorelineWorld;
		Island->GetShorelinePolygonWorldCm(ShorelineWorld);
		if (ShorelineWorld.Num() >= 3 && PointInPolygon2D(WorldXY, ShorelineWorld))
		{
			OutIslandIndex = IslandIndex;
			return true;
		}
	}
	return false;
}

bool AIH_Cube2FlyPlayerController::TryIsOpenWaterClickAtScreen(const FVector2D& ScreenPos) const
{
	const UGameInstance* GI = GetGameInstance();
	const UIH_P1C08_IslandNavSubsystem* Nav = GI
		? GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>()
		: nullptr;
	const bool bIslandEditActive = Nav && Nav->HasSelectedIsland();

	FVector IslandSurface = FVector::ZeroVector;
	const bool bOnIslandSurface = TrySampleIslandSurfaceAtScreen(ScreenPos, IslandSurface);

	if (bIslandEditActive)
	{
		if (bOnIslandSurface)
		{
			int32 InnerCoreIslandIndex = INDEX_NONE;
			if (TryGetIslandIndexAtWorldXYInnerCore(
				FVector2D(IslandSurface.X, IslandSurface.Y), InnerCoreIslandIndex))
			{
				return false;
			}
			return true;
		}

		FVector WaterPoint = FVector::ZeroVector;
		if (TryGetWorldPointOnWaterPlane(ScreenPos, WaterPoint))
		{
			int32 InnerCoreIslandIndex = INDEX_NONE;
			if (TryGetIslandIndexAtWorldXYInnerCore(
				FVector2D(WaterPoint.X, WaterPoint.Y), InnerCoreIslandIndex))
			{
				return false;
			}
		}
		return true;
	}

	// Match DevClickBurst: collision heightfield hits count as on-island even when the
	// water-plane deproject fails (common on oblique island views).
	if (bOnIslandSurface)
	{
		if (IHInvisibleHandSpec::bWBIslandSelectCoreOnly && IslandSurface.Z < -100.f)
		{
			// Submerged shelf / harbor floor — treat as open water for ships.
		}
		else
		{
			return false;
		}
	}

	FVector WaterPoint = FVector::ZeroVector;
	if (!TryGetWorldPointOnWaterPlane(ScreenPos, WaterPoint))
	{
		return true;
	}

	const FVector2D WaterXY(WaterPoint.X, WaterPoint.Y);
	if (IHInvisibleHandSpec::bWBIslandSelectCoreOnly)
	{
		// Outer ring + harbors (outside MainCoast×2/3) count as open water for ships.
		int32 CoreIslandIndex = INDEX_NONE;
		return !TryGetIslandIndexAtWorldXYSelectCore(WaterXY, CoreIslandIndex);
	}
	int32 StrictIslandIndex = INDEX_NONE;
	return !TryGetIslandIndexAtWorldXYStrict(WaterXY, StrictIslandIndex);
}

bool AIH_Cube2FlyPlayerController::TryGetIslandIndexAtWorldXYInnerCore(
	const FVector2D& WorldXY,
	int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	float BestDistSq = TNumericLimits<float>::Max();
	int32 BestIndex = INDEX_NONE;
	for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
	{
		const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex);
		if (!Island)
		{
			continue;
		}

		const FVector2D Center(Island->GetActorLocation().X, Island->GetActorLocation().Y);
		const float InnerRadiusCm = GetIslandLaneRadiusCm(Island) * IslandInnerPickRadiusFraction;
		if (InnerRadiusCm <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float DistSq = FVector2D::DistSquared(WorldXY, Center);
		if (DistSq <= FMath::Square(InnerRadiusCm) && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = IslandIndex;
		}
	}

	if (BestIndex != INDEX_NONE)
	{
		OutIslandIndex = BestIndex;
		return true;
	}
	return false;
}

bool AIH_Cube2FlyPlayerController::TryResolveIslandIndexAtScreen(
	const FVector2D& ScreenPos,
	int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;

	if (IHInvisibleHandSpec::bWBIslandSelectCoreOnly)
	{
		// WB: dry IslandMesh surface OR MainCoast scaled to 2/3 — never circular water orbit.
		FVector IslandSurface = FVector::ZeroVector;
		AActor* IslandActor = nullptr;
		if (TrySampleIslandSurfaceAtScreen(ScreenPos, IslandSurface, &IslandActor))
		{
			// Shelf / submerged samples are not island-select (ships own harbors).
			if (IslandSurface.Z >= -100.f)
			{
				const UWorld* World = GetWorld();
				const AIH_WB_Demo004GameMode* GM =
					World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
				if (const AIH_WB_IslandActor* HitIsland = Cast<AIH_WB_IslandActor>(IslandActor))
				{
					OutIslandIndex = HitIsland->GetTankIslandIndex();
					if (OutIslandIndex != INDEX_NONE)
					{
						return true;
					}
				}
				if (GM && IslandActor)
				{
					for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
					{
						if (GM->GetSpawnedIslands()[IslandIndex].Get() == IslandActor)
						{
							OutIslandIndex = IslandIndex;
							return true;
						}
					}
				}
				if (TryGetIslandIndexAtWorldXYSelectCore(
						FVector2D(IslandSurface.X, IslandSurface.Y), OutIslandIndex))
				{
					return true;
				}
			}
		}

		FVector WaterPoint = FVector::ZeroVector;
		if (TryGetWorldPointOnWaterPlane(ScreenPos, WaterPoint))
		{
			return TryGetIslandIndexAtWorldXYSelectCore(
				FVector2D(WaterPoint.X, WaterPoint.Y), OutIslandIndex);
		}
		return false;
	}

	FVector IslandSurface = FVector::ZeroVector;
	AActor* IslandActor = nullptr;
	if (!TrySampleIslandSurfaceAtScreen(ScreenPos, IslandSurface, &IslandActor))
	{
		return TryGetIslandIndexAtScreen(ScreenPos, OutIslandIndex);
	}

	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	if (const AIH_WB_IslandActor* HitIsland = Cast<AIH_WB_IslandActor>(IslandActor))
	{
		OutIslandIndex = HitIsland->GetTankIslandIndex();
		if (OutIslandIndex != INDEX_NONE)
		{
			return true;
		}
	}

	if (IslandActor)
	{
		for (int32 IslandIndex = 0; IslandIndex < GM->GetSpawnedIslands().Num(); ++IslandIndex)
		{
			if (GM->GetSpawnedIslands()[IslandIndex].Get() == IslandActor)
			{
				OutIslandIndex = IslandIndex;
				return true;
			}
		}
	}

	if (TryGetIslandIndexAtWorldXY(FVector2D(IslandSurface.X, IslandSurface.Y), OutIslandIndex))
	{
		return true;
	}

	return TryGetIslandIndexAtScreen(ScreenPos, OutIslandIndex);
}

bool AIH_Cube2FlyPlayerController::TryGetIslandIndexAtScreenInnerCore(
	const FVector2D& ScreenPos,
	int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	FVector WorldPoint = FVector::ZeroVector;
	if (TryGetWorldPointOnWaterPlane(ScreenPos, WorldPoint))
	{
		if (TryGetIslandIndexAtWorldXYInnerCore(FVector2D(WorldPoint.X, WorldPoint.Y), OutIslandIndex))
		{
			return true;
		}
	}

	FVector SurfaceWorld = FVector::ZeroVector;
	if (TrySampleIslandSurfaceAtScreen(ScreenPos, SurfaceWorld))
	{
		return TryGetIslandIndexAtWorldXYInnerCore(FVector2D(SurfaceWorld.X, SurfaceWorld.Y), OutIslandIndex);
	}
	return false;
}

bool AIH_Cube2FlyPlayerController::TryGetIslandIndexAtScreen(const FVector2D& ScreenPos, int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	FVector WorldPoint;
	if (TryGetWorldPointOnWaterPlane(ScreenPos, WorldPoint))
	{
		if (TryGetIslandIndexAtWorldXY(FVector2D(WorldPoint.X, WorldPoint.Y), OutIslandIndex))
		{
			return true;
		}
	}

	FVector IslandSurface = FVector::ZeroVector;
	AActor* IslandActor = nullptr;
	if (TrySampleIslandSurfaceAtScreen(ScreenPos, IslandSurface, &IslandActor))
	{
		if (IslandActor)
		{
			for (int32 i = 0; i < GM->GetSpawnedIslands().Num(); ++i)
			{
				if (GM->GetSpawnedIslands()[i].Get() == IslandActor)
				{
					OutIslandIndex = i;
					return true;
				}
			}
		}
		if (TryGetIslandIndexAtWorldXY(FVector2D(IslandSurface.X, IslandSurface.Y), OutIslandIndex))
		{
			return true;
		}
	}

	FVector Origin;
	FVector Direction;
	if (!DeprojectScreenToWorldRay(ScreenPos, Origin, Direction))
	{
		return false;
	}

	const FVector TraceEnd = Origin + Direction * 5.0e8f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C08IslandPick), true, this);
	TArray<FHitResult> Hits;
	if (!World->LineTraceMultiByChannel(Hits, Origin, TraceEnd, ECC_Visibility, Params))
	{
		return false;
	}

	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor->IsA(AWaterBody::StaticClass()))
		{
			continue;
		}

		for (int32 i = 0; i < GM->GetSpawnedIslands().Num(); ++i)
		{
			if (GM->GetSpawnedIslands()[i].Get() == HitActor)
			{
				OutIslandIndex = i;
				return true;
			}
		}
	}
	return false;
}

FVector2D AIH_Cube2FlyPlayerController::GetLastValidDraftOffsetCm(int32 IslandIndex) const
{
	if (const FVector2D* Found = LastValidDraftOffsetCm.Find(IslandIndex))
	{
		return *Found;
	}
	return FVector2D::ZeroVector;
}

void AIH_Cube2FlyPlayerController::SetLastValidDraftOffsetCm(int32 IslandIndex, const FVector2D& OffsetCm)
{
	LastValidDraftOffsetCm.Add(IslandIndex, OffsetCm);
}

bool AIH_Cube2FlyPlayerController::IsScreenPointOverInteractiveHUDPanel(const FVector2D& CursorAbsolute) const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			if (Minimap->IsScreenPointOverMinimap(CursorAbsolute))
			{
				return true;
			}
		}
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (BuildPalette->IsTabStripVisible())
			{
				if (const UIH_BuildPaletteHostWidget* PaletteWidget = BuildPalette->GetBuildPaletteWidget())
				{
					if (PaletteWidget->IsScreenPointOverTabStrip(CursorAbsolute)
						|| PaletteWidget->IsScreenPointOverBuildPalette(CursorAbsolute))
					{
						return true;
					}
				}
			}
		}
	}

	if (IslandNavWidget && IslandNavWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (CoastlineTuningWidget && CoastlineTuningWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (GameSpeedWidget && GameSpeedWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
#if !UE_BUILD_SHIPPING
	if (PlaceShipWidget && PlaceShipWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (TopDownViewWidget && TopDownViewWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (DevViewWidget && DevViewWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
#endif
	if (WeatherPreviewWidget && WeatherPreviewWidget->IsPanelVisible() && WeatherPreviewWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (GameDateTimeWidget && GameDateTimeWidget->IsPanelVisible() && GameDateTimeWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (PlayAtmosphericsWidget && PlayAtmosphericsWidget->IsPanelVisible() && PlayAtmosphericsWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (TemplateGalleryWidget && TemplateGalleryWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}
	if (DevSeedPanelWidget && DevSeedPanelWidget->IsScreenPointOverPanel(CursorAbsolute))
	{
		return true;
	}

	return false;
}

bool AIH_Cube2FlyPlayerController::TryFindTownGridManagerAtScreen(
	const FVector2D& ScreenPos,
	AIH_TownGridManager*& OutManager) const
{
	OutManager = nullptr;
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ImpactPoint = FVector::ZeroVector;
	if (!TryTraceTerrainAtScreen(ScreenPos, ImpactPoint))
	{
		return false;
	}

	AIH_TownGridManager* Best = nullptr;
	float BestDistSq = MAX_FLT;
	for (TActorIterator<AIH_TownGridManager> It(World); It; ++It)
	{
		AIH_TownGridManager* Manager = *It;
		if (!IsValid(Manager))
		{
			continue;
		}
		if (!Manager->ContainsWorldPointXY(ImpactPoint))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Manager->GetActorLocation(), ImpactPoint);
		if (!Best || DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Manager;
		}
	}

	if (Best)
	{
		OutManager = Best;
		return true;
	}
	return false;
}

void AIH_Cube2FlyPlayerController::SelectTownGridManager(AIH_TownGridManager* Manager)
{
	if (SelectedTownGridManager.Get() == Manager)
	{
		return;
	}

	if (AIH_TownGridManager* Previous = SelectedTownGridManager.Get())
	{
		Previous->SetSelected(false);
	}

	SelectedTownGridManager = Manager;
	if (Manager)
	{
		Manager->SetSelected(true);
	}
}

void AIH_Cube2FlyPlayerController::DeselectTownGridManager()
{
	SelectTownGridManager(nullptr);
}

void AIH_Cube2FlyPlayerController::TickBuildPaletteAndTownGrid(float DeltaTime)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>();
	if (!BuildPalette)
	{
		return;
	}

	FVector2D ViewportCur = FVector2D::ZeroVector;
	const bool bHasViewportMouse = TryGetViewportMousePosition(ViewportCur);
	const FVector2D CursorAbsolute = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetCursorPos()
		: FVector2D::ZeroVector;

	if (BuildPalette->IsDragActive())
	{
		bool bUpdatedGhost = false;
		UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>();
		if (Minimap && Minimap->IsMinimapOpen())
		{
			if (UIH_P1C08_MinimapWidget* MinimapWidget = Minimap->GetMinimapWidget())
			{
				FVector2D WorldXY = FVector2D::ZeroVector;
				if (MinimapWidget->TryGetWorldXYFromScreen(CursorAbsolute, WorldXY))
				{
					BuildPalette->UpdateDragGhostFromWorldXY(this, WorldXY);
					Minimap->RequestMinimapRepaint();
					bUpdatedGhost = true;
				}
			}
		}

		if (!bUpdatedGhost && bHasViewportMouse)
		{
			BuildPalette->UpdateDragGhostFromScreen(this, ViewportCur);
		}

		if (UWorld* World = GetWorld())
		{
			BuildPalette->DrawDragGhost(World, this);
		}

		if (UIH_BuildPaletteHostWidget* HostWidget = BuildPalette->GetBuildPaletteWidget())
		{
			HostWidget->RequestLayoutRefresh();
		}
	}

	if (UIH_BuildPaletteHostWidget* PaletteWidget = BuildPalette->GetBuildPaletteWidget())
	{
		if (bBuildPalettePointerCapture && IsLeftMouseButtonDown())
		{
			PaletteWidget->HandleScreenPointerMove(CursorAbsolute);
		}
	}

	if (AIH_TownGridManager* Manager = SelectedTownGridManager.Get())
	{
		if (Manager->IsMoveDragActive() && IsLeftMouseButtonDown() && bHasViewportMouse)
		{
			FVector WorldPoint = FVector::ZeroVector;
			if (TryTraceTerrainAtScreen(ViewportCur, WorldPoint))
			{
				Manager->UpdateMoveDrag(WorldPoint);
			}
		}
		else if (Manager->IsGripDragActive() && IsLeftMouseButtonDown() && bHasViewportMouse)
		{
			FVector WorldPoint = FVector::ZeroVector;
			if (TryTraceTerrainAtScreen(ViewportCur, WorldPoint))
			{
				Manager->UpdateGripDrag(WorldPoint);
			}
		}
	}
}

bool AIH_Cube2FlyPlayerController::IsIslandOffsetPlacementValid(
	int32 IslandIndex, const FVector2D& ProposedOffsetCm, int32& OutViolatingIndex) const
{
	OutViolatingIndex = INDEX_NONE;
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	const FVector2D SeedCenter = GM->GetSeedBaseCenterCm(IslandIndex);
	const FVector2D ProposedCenter = SeedCenter + ProposedOffsetCm;

	TArray<FVector2D> AllCenters;
	TArray<float> AllLaneRadii;
	TArray<int32> AllIslandIndices;
	for (int32 i = 0; i < GM->GetSpawnedIslands().Num(); ++i)
	{
		const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(i);
		if (!Island)
		{
			continue;
		}

		FVector2D Center(Island->GetActorLocation().X, Island->GetActorLocation().Y);
		if (i == IslandIndex)
		{
			Center = ProposedCenter;
		}

		AllIslandIndices.Add(i);
		AllCenters.Add(Center);
		AllLaneRadii.Add(GetIslandDragLaneRadiusCm(Island));
	}

	const int32 MovingArrayIndex = AllIslandIndices.IndexOfByKey(IslandIndex);
	const float MovingLaneRadius = AllLaneRadii.IsValidIndex(MovingArrayIndex)
		? AllLaneRadii[MovingArrayIndex]
		: 60000.f;
	float RealmHalfExtentNSKm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;
	if (const UIH_WB_Demo004GameInstance* StoryGI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		RealmHalfExtentNSKm = StoryGI->GetRealmHalfExtentNSKm();
	}

	static constexpr float DragWallCollisionRadiusFactor = 1.0f;
	return UIHSeedIslandLibrary::ValidateIslandCenterPlacementCm(
		IslandIndex, ProposedCenter, MovingLaneRadius, AllCenters, AllLaneRadii, OutViolatingIndex, AllIslandIndices,
		RealmHalfExtentNSKm, 0.f, DragWallCollisionRadiusFactor);
}

void AIH_Cube2FlyPlayerController::ApplyIslandDragOffsetPreview(
	int32 IslandIndex, const FVector2D& ProposedOffsetCm)
{
	AIH_WB_Demo004GameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	UIH_P1C08_CoastlineTuningSubsystem* Tuning = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>()
		: nullptr;
	if (!GM || !Tuning)
	{
		return;
	}

	if (Tuning->GetActiveIslandIndex() != IslandIndex)
	{
		Tuning->SyncActiveFromIsland(IslandIndex);
	}

	const FIHIslandManualTransform CurrentTransform = Tuning->GetActiveManualTransform();
	if (FVector2D::DistSquared(CurrentTransform.OffsetXYCm, ProposedOffsetCm) <= 1.f)
	{
		return;
	}

	FIHIslandManualTransform Transform = CurrentTransform;
	Transform.OffsetXYCm = ProposedOffsetCm;
	Transform.bUserMoved = true;
	Tuning->SetDraftManualTransformPreview(Transform);
	GM->ApplyIslandManualTransform(IslandIndex, Transform, true);
}

float AIH_Cube2FlyPlayerController::ComputeWorldSizeForScreenPixels(
	const FVector& WorldPoint, float ScreenPixels) const
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	FVector CameraLocation;
	FRotator CameraRotation;
	GetPlayerViewPoint(CameraLocation, CameraRotation);

	const float DistanceCm = FVector::Dist(CameraLocation, WorldPoint);
	const float FovDegrees = PlayerCameraManager ? PlayerCameraManager->GetFOVAngle() : 90.f;
	const float WorldHeightAtDistance = 2.f * DistanceCm * FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f));
	const float WorldUnitsPerPixel = WorldHeightAtDistance / FMath::Max(static_cast<float>(ViewportSizeY), 1.f);
	return FMath::Max(WorldUnitsPerPixel * ScreenPixels, 1.f);
}

void AIH_Cube2FlyPlayerController::FinalizeIslandDrag(int32 IslandIndex)
{
	if (IslandIndex == INDEX_NONE || !GetGameInstance())
	{
		return;
	}

	UIH_P1C08_CoastlineTuningSubsystem* Tuning = GetGameInstance()->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>();
	AIH_WB_Demo004GameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!Tuning || !GM)
	{
		return;
	}

	if (Tuning->GetActiveIslandIndex() != IslandIndex)
	{
		Tuning->SyncActiveFromIsland(IslandIndex);
	}

	const FVector2D CurrentOffset = Tuning->GetActiveManualTransform().OffsetXYCm;
	SetLastValidDraftOffsetCm(IslandIndex, CurrentOffset);
	FIHIslandManualTransform Transform = Tuning->GetActiveManualTransform();
	Transform.bUserMoved = true;
	Tuning->SetDraftManualTransform(Transform);

	if (AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIndex))
	{
		Island->RefreshMinimapCoastline();
	}
	if (UIH_P1C08_MinimapSubsystem* MinimapSubsystem = GetGameInstance()->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
	{
		MinimapSubsystem->RequestMinimapRepaint();
	}
	if (CoastlineTuningWidget)
	{
		CoastlineTuningWidget->UpdateDraftStatusText();
	}

	ApplyFreeMouseViewportSettings();
}

bool AIH_Cube2FlyPlayerController::ValidateAndApplyDraftOffset(int32 IslandIndex, const FVector2D& ProposedOffsetCm)
{
	UIH_P1C08_CoastlineTuningSubsystem* Tuning = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>()
		: nullptr;
	if (!Tuning)
	{
		return false;
	}

	if (Tuning->GetActiveIslandIndex() != IslandIndex)
	{
		Tuning->SyncActiveFromIsland(IslandIndex);
	}

	const FIHIslandManualTransform CurrentTransform = Tuning->GetActiveManualTransform();
	if (FVector2D::DistSquared(CurrentTransform.OffsetXYCm, ProposedOffsetCm) <= 1.f)
	{
		return true;
	}

	int32 ViolatingIndex = INDEX_NONE;
	if (!IsIslandOffsetPlacementValid(IslandIndex, ProposedOffsetCm, ViolatingIndex))
	{
		LaneFlashOtherIndex = ViolatingIndex != INDEX_NONE ? ViolatingIndex : IslandIndex;
		LaneFlashRemainingSec = LaneFlashDurationSec;
		return false;
	}

	SetLastValidDraftOffsetCm(IslandIndex, ProposedOffsetCm);
	FIHIslandManualTransform Transform = CurrentTransform;
	Transform.OffsetXYCm = ProposedOffsetCm;
	Transform.bUserMoved = true;
	Tuning->SetDraftManualTransform(Transform);
	return true;
}

void AIH_Cube2FlyPlayerController::TickIslandManipulationInput(float DeltaTime)
{
	if (IsHUDSliderConsumingKeyboard())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>();
	if (!Nav || !Tuning || !Nav->HasSelectedIsland())
	{
		return;
	}

	if (WasInputKeyJustPressed(EKeys::R))
	{
		const float Delta = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)
			? -IslandRotateStepDeg : IslandRotateStepDeg;
		Tuning->AddDraftManualYawDeg(Delta);
	}

	if (WasInputKeyJustPressed(EKeys::Enter) && !bAwaitingConfirmRevert && HasUncommittedIslandDraft())
	{
		if (CoastlineTuningWidget)
		{
			CoastlineTuningWidget->ApplyChanges();
		}
	}
}

void AIH_Cube2FlyPlayerController::TickIslandManipulationGizmo(float DeltaTime)
{
	(void)DeltaTime;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	// ICE-02g: draw gizmo whenever an island is selected and W fly-out is closed (same gate as mesh glow).
	if (!Nav || !Nav->HasSelectedIsland() || IsViewportIslandSelectionBlocked())
	{
		return;
	}

	const AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>();
	const AIH_WB_IslandActor* Island = GM ? GM->GetSpawnedIsland(Nav->GetSelectedIslandIndex()) : nullptr;
	if (!Island)
	{
		return;
	}

	const FVector IslandLoc = Island->GetActorLocation();

	// Plan Addendum 11 tried a per-tick unlit DrawDebug beacon here (for constant brightness
	// regardless of scene lighting) but the sphere+shaft combo didn't read well in practice -
	// reverted per direct feedback, back to the plain UArrowComponent with per-tick screen-constant
	// scaling (Addendum 10), which already benefits from this round's position/recenter fix.
	if (UArrowComponent* Reticle = Island->GetSelectionReticleComponent())
	{
		constexpr float ReticleTargetScreenPx = 32.f;
		const float DesiredWorldSizeCm = ComputeWorldSizeForScreenPixels(Reticle->GetComponentLocation(), ReticleTargetScreenPx);
		const float NewArrowSize = DesiredWorldSizeCm / 100.f;
		if (!FMath::IsNearlyEqual(Reticle->ArrowSize, NewArrowSize, 0.05f))
		{
			Reticle->ArrowSize = NewArrowSize;
			Reticle->MarkRenderStateDirty();
		}
	}

	if (IHInvisibleHandSpec::IsIslandSelectionDebugRingEnabled())
	{
		TArray<FVector2D> ShorelineWorld;
		Island->GetShorelinePolygonWorldCm(ShorelineWorld);
		if (ShorelineWorld.Num() >= 3)
		{
			const FVector RingPoint(IslandLoc.X, IslandLoc.Y, SelectionRingLiftCm);
			const float LineThicknessCm = ComputeWorldSizeForScreenPixels(RingPoint, SelectionRingScreenThicknessPx);
			const float DashLengthCm = ComputeWorldSizeForScreenPixels(RingPoint, SelectionRingDashLengthPx);
			const float DashGapCm = ComputeWorldSizeForScreenPixels(RingPoint, SelectionRingDashGapPx);
			const FColor SelectionOrange(255, 140, 0);
			for (int32 i = 0; i < ShorelineWorld.Num(); ++i)
			{
				const FVector2D& A = ShorelineWorld[i];
				const FVector2D& B = ShorelineWorld[(i + 1) % ShorelineWorld.Num()];
				DrawDashedDebugLine(
					World,
					FVector(A.X, A.Y, SelectionRingLiftCm),
					FVector(B.X, B.Y, SelectionRingLiftCm),
					SelectionOrange,
					LineThicknessCm,
					DashLengthCm,
					DashGapCm);
			}
		}
	}

	const FColor CrossCyan(0, 255, 255);
	const FColor CrossRed(255, 32, 64);
	const FColor YawColor(255, 220, 64);
	const uint8 GizmoDepthPriority = SDPG_Foreground;
	const float GizmoZCm = IHInvisibleHandSpec::IslandSelectionAxisHubZCm;
	const float WaterlineZCm = IslandLoc.Z;
	const FVector GizmoHub(IslandLoc.X, IslandLoc.Y, WaterlineZCm + GizmoZCm);

	float FootprintSemiMajorCm = 0.f;
	float FootprintSemiMinorCm = 0.f;
	Island->GetWaterlineFootprintCm(FootprintSemiMajorCm, FootprintSemiMinorCm);
	const float CrossArmCm = FMath::Max(
		FMath::Max(FootprintSemiMajorCm, FootprintSemiMinorCm) * 0.55f,
		GizmoMoveHandleRadiusCm);
	const float YawRingCm = FMath::Clamp(FootprintSemiMajorCm * 0.88f, GizmoYawRingRadiusCm, FootprintSemiMajorCm);

	const float CrossThickCm = FMath::Clamp(
		ComputeWorldSizeForScreenPixels(GizmoHub, 48.f),
		320.f,
		640.f);
	const float BeamThickCm = FMath::Clamp(
		ComputeWorldSizeForScreenPixels(GizmoHub, 10.f),
		80.f,
		160.f);
	const float YawThickCm = FMath::Clamp(
		ComputeWorldSizeForScreenPixels(GizmoHub, 8.f),
		64.f,
		140.f);

	const float IridescentPhase = FMath::Fmod(
		static_cast<float>(FPlatformTime::Seconds()) * 2.4f, 1.f);

	auto DrawIridescentAxis = [&](
		const FVector& A,
		const FVector& B,
		const FColor& CoreColor,
		const FColor& FringeColor,
		const float ThicknessCm)
	{
		const FColor Iridescent(
			static_cast<uint8>(FMath::Clamp(CoreColor.R * 0.5f + FringeColor.R * 0.5f + IridescentPhase * 100.f, 0.f, 255.f)),
			static_cast<uint8>(FMath::Clamp(CoreColor.G * 0.5f + FringeColor.G * 0.5f + (1.f - IridescentPhase) * 100.f, 0.f, 255.f)),
			static_cast<uint8>(FMath::Clamp(CoreColor.B * 0.5f + FringeColor.B * 0.5f + 120.f, 0.f, 255.f)));
		DrawDebugLine(World, A, B, Iridescent, false, -1.f, GizmoDepthPriority, ThicknessCm);
	};

	DrawIridescentAxis(
		GizmoHub + FVector(-CrossArmCm, 0.f, 0.f),
		GizmoHub + FVector(CrossArmCm, 0.f, 0.f),
		CrossCyan,
		FColor(255, 64, 220),
		CrossThickCm);
	DrawIridescentAxis(
		GizmoHub + FVector(0.f, -CrossArmCm, 0.f),
		GizmoHub + FVector(0.f, CrossArmCm, 0.f),
		CrossRed,
		FColor(255, 180, 64),
		CrossThickCm);

	const float SummitZCm = FMath::Max(Island->GetSummitTopZCm(), 5000.f);
	FVector CameraLocation;
	FRotator CameraRotation;
	GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector BehindOffset = GizmoHub - CameraLocation;
	BehindOffset.Z = 0.f;
	if (!BehindOffset.Normalize())
	{
		BehindOffset = FVector(-1.f, 0.f, 0.f);
	}
	const float BehindDistCm = FMath::Clamp(FootprintSemiMajorCm * 0.08f, 1200.f, 4500.f);
	const FVector BeamBase = GizmoHub + BehindOffset * BehindDistCm + FVector(0.f, 0.f, 120.f);
	const FVector BeamTop = BeamBase + FVector(0.f, 0.f, SummitZCm + 1800.f);
	DrawDebugSolidBox(
		World,
		BeamBase + FVector(0.f, 0.f, 140.f),
		FVector(240.f, 240.f, 140.f),
		FColor(255, 120, 32),
		false,
		-1.f,
		GizmoDepthPriority);

	static const FColor BeamColors[] = {
		FColor(255, 48, 48),
		FColor(255, 180, 48),
		FColor(255, 220, 180),
		FColor(220, 200, 255),
	};
	constexpr int32 BeamSegments = UE_ARRAY_COUNT(BeamColors);
	for (int32 BeamSeg = 0; BeamSeg < BeamSegments; ++BeamSeg)
	{
		const float T0 = static_cast<float>(BeamSeg) / static_cast<float>(BeamSegments);
		const float T1 = static_cast<float>(BeamSeg + 1) / static_cast<float>(BeamSegments);
		DrawDebugLine(
			World,
			FMath::Lerp(BeamBase, BeamTop, T0),
			FMath::Lerp(BeamBase, BeamTop, T1),
			BeamColors[BeamSeg],
			false,
			-1.f,
			GizmoDepthPriority,
			BeamThickCm);
	}

	const float YawRad = FMath::DegreesToRadians(Island->GetActorRotation().Yaw);
	const FVector YawX = FVector(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f) * YawRingCm;
	for (int32 Seg = 0; Seg < 32; ++Seg)
	{
		const float A0 = (static_cast<float>(Seg) / 32.f) * 2.f * PI;
		const float A1 = (static_cast<float>(Seg + 1) / 32.f) * 2.f * PI;
		const FVector P0 = FVector(
			GizmoHub.X + FMath::Cos(A0) * YawRingCm,
			GizmoHub.Y + FMath::Sin(A0) * YawRingCm,
			GizmoHub.Z);
		const FVector P1 = FVector(
			GizmoHub.X + FMath::Cos(A1) * YawRingCm,
			GizmoHub.Y + FMath::Sin(A1) * YawRingCm,
			GizmoHub.Z);
		DrawDebugLine(World, P0, P1, YawColor, false, -1.f, GizmoDepthPriority, YawThickCm);
	}
	DrawDebugLine(World, GizmoHub, GizmoHub + YawX, YawColor, false, -1.f, GizmoDepthPriority, YawThickCm * 1.35f);
}

void AIH_Cube2FlyPlayerController::TickTerrainStampManipulationInput(float DeltaTime)
{
	(void)DeltaTime;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			BuildPalette->TickTerrainStampManipulation(this);
		}
	}
}

void AIH_Cube2FlyPlayerController::TickTerrainStampManipulationGizmo(float DeltaTime)
{
	(void)DeltaTime;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (BuildPalette->IsWorldStampEditModeActive())
			{
				BuildPalette->DrawSelectedTerrainStampGizmo(World, this);
			}
		}
	}
}

void AIH_Cube2FlyPlayerController::DrawLaneViolationFlash(float DeltaTime)
{
	if (LaneFlashRemainingSec <= 0.f)
	{
		return;
	}

	LaneFlashRemainingSec = FMath::Max(0.f, LaneFlashRemainingSec - DeltaTime);
	const UWorld* World = GetWorld();
	const AIH_WB_Demo004GameMode* GM = World ? World->GetAuthGameMode<AIH_WB_Demo004GameMode>() : nullptr;
	if (!World || !GM)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UIH_P1C08_IslandNavSubsystem* Nav = GI ? GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>() : nullptr;
	if (!Nav || !Nav->HasSelectedIsland())
	{
		return;
	}

	const int32 A = Nav->GetSelectedIslandIndex();
	const int32 B = LaneFlashOtherIndex;
	const AIH_WB_IslandActor* IslandA = GM->GetSpawnedIsland(A);
	const AIH_WB_IslandActor* IslandB = GM->GetSpawnedIsland(B);
	if (!IslandA || !IslandB)
	{
		return;
	}

	const FVector LocA = IslandA->GetActorLocation();
	const FVector LocB = IslandB->GetActorLocation();
	const float Pulse = 0.35f + 0.25f * FMath::Sin(World->GetTimeSeconds() * 8.f);
	const FColor BandColor(255, 40, 40, static_cast<uint8>(Pulse * 255.f));
	const FVector Lift(0.f, 0.f, 300.f);
	DrawDebugLine(World, LocA + Lift, LocB + Lift, BandColor, false, -1.f, SDPG_Foreground, 600.f);
	DrawDebugLine(World, LocA + Lift + FVector(0.f, 0.f, 400.f), LocB + Lift + FVector(0.f, 0.f, 400.f), BandColor, false, -1.f, SDPG_Foreground, 400.f);
}

