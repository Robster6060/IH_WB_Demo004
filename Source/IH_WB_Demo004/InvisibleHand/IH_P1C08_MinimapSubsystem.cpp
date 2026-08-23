// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_P1C08_MinimapSubsystem.h"

#include "IH_P1C08_MinimapWidget.h"
#include "IH_P1C08_WorldCoastStrokeWidget.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IHP1C10_AzgaarTypes.h"

#include "IH_P1C08_MinimapCoastline.h"
#include "IH_WB_IslandActor.h"

#include "IH_Cube2FlyPlayerController.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IHSeedIslandLibrary.h"

#include "GameFramework/Pawn.h"

void UIH_P1C08_MinimapSubsystem::Initialize(FSubsystemCollectionBase& Collection)

{

	Super::Initialize(Collection);

	ZoomFactor = 0.f;

	ZoomStepIndex = 0;

	ViewCenterWorld = FVector2D::ZeroVector;

}



void UIH_P1C08_MinimapSubsystem::RegisterCoastlinePolylineWorld(
	int32 IslandIndex,
	const TArray<FVector2D>& PolylineWorld,
	const int32 FeatureId,
	const bool bIsInlandSea)
{
	if (PolylineWorld.Num() < 3 || IslandIndex == INDEX_NONE)
	{
		return;
	}
	// Replace existing polyline for this island+feature (avoid stacking on refresh).
	for (int32 I = 0; I < CoastlineIslandIndices.Num(); ++I)
	{
		if (CoastlineIslandIndices[I] == IslandIndex
			&& CoastlineFeatureIds.IsValidIndex(I)
			&& CoastlineFeatureIds[I] == FeatureId)
		{
			CoastlinePolylinesWorld[I] = PolylineWorld;
			if (CoastlineIsInlandSea.IsValidIndex(I))
			{
				CoastlineIsInlandSea[I] = bIsInlandSea;
			}
			++CoastlineRevision;
			return;
		}
	}
	CoastlinePolylinesWorld.Add(PolylineWorld);
	CoastlineIslandIndices.Add(IslandIndex);
	CoastlineFeatureIds.Add(FeatureId);
	CoastlineIsInlandSea.Add(bIsInlandSea);
	++CoastlineRevision;
}

void UIH_P1C08_MinimapSubsystem::RegisterSeaRootsExtentWorld(
	const int32 IslandIndex,
	const FVector2D& CenterWorldCm,
	const float YawDegrees,
	const FIHSeaRootsExtent& Extent,
	const int32 FeatureId,
	const bool bIsMainFeature)
{
	if (IslandIndex == INDEX_NONE || !HasValidSeaRootsExtentForPresentation(Extent))
	{
		return;
	}

	// Read-only copy of island SSOT bake (already prepared on regen — do not re-derive rings here).
	const FIHSeaRootsExtent ReplicaExtent = Extent;

	for (FSeaRootsLayerEntry& Entry : SeaRootsLayers)
	{
		if (Entry.IslandIndex == IslandIndex && Entry.FeatureId == FeatureId)
		{
			Entry.CenterWorldCm = CenterWorldCm;
			Entry.YawDegrees = YawDegrees;
			Entry.bIsMainFeature = bIsMainFeature;
			Entry.Extent = ReplicaExtent;
			return;
		}
	}

	FSeaRootsLayerEntry NewEntry;
	NewEntry.IslandIndex = IslandIndex;
	NewEntry.FeatureId = FeatureId;
	NewEntry.bIsMainFeature = bIsMainFeature;
	NewEntry.CenterWorldCm = CenterWorldCm;
	NewEntry.YawDegrees = YawDegrees;
	NewEntry.Extent = ReplicaExtent;
	SeaRootsLayers.Add(NewEntry);
}

bool UIH_P1C08_MinimapSubsystem::TryGetSeaRootsLayerForIsland(
	const int32 IslandIndex,
	FSeaRootsLayerEntry& OutLayer) const
{
	for (const FSeaRootsLayerEntry& Entry : SeaRootsLayers)
	{
		if (Entry.IslandIndex == IslandIndex && Entry.bIsMainFeature)
		{
			OutLayer = Entry;
			return Entry.Extent.CoastRadiiCm.Num() >= 8
				|| HasValidBakedSeaRootsRingPolylines(Entry.Extent);
		}
	}
	return false;
}

bool UIH_P1C08_MinimapSubsystem::TryGetCoastlineWorldXYForIsland(
	int32 IslandIndex,
	TArray<FVector2D>& OutWorldXY) const
{
	OutWorldXY.Reset();
	if (IslandIndex == INDEX_NONE)
	{
		return false;
	}

	// Prefer MainCoast (FeatureId 0); fall back to first entry for that island.
	int32 FallbackIdx = INDEX_NONE;
	for (int32 Index = 0; Index < CoastlineIslandIndices.Num(); ++Index)
	{
		if (CoastlineIslandIndices[Index] != IslandIndex
			|| !CoastlinePolylinesWorld.IsValidIndex(Index))
		{
			continue;
		}
		const int32 Fid = CoastlineFeatureIds.IsValidIndex(Index) ? CoastlineFeatureIds[Index] : 0;
		if (Fid == 0)
		{
			OutWorldXY = CoastlinePolylinesWorld[Index];
			return OutWorldXY.Num() >= 3;
		}
		if (FallbackIdx == INDEX_NONE)
		{
			FallbackIdx = Index;
		}
	}
	if (FallbackIdx != INDEX_NONE)
	{
		OutWorldXY = CoastlinePolylinesWorld[FallbackIdx];
		return OutWorldXY.Num() >= 3;
	}
	return false;
}

void UIH_P1C08_MinimapSubsystem::UnregisterCoastlineForOwner(const AActor* Owner)
{
	if (!Owner)
	{
		return;
	}

	const AIH_WB_IslandActor* IslandOwner = Cast<const AIH_WB_IslandActor>(Owner);
	if (IslandOwner)
	{
		UnregisterCoastlineForIsland(IslandOwner->GetTankIslandIndex());
	}
}

void UIH_P1C08_MinimapSubsystem::UnregisterCoastlineForIsland(int32 IslandIndex)
{
	if (IslandIndex == INDEX_NONE)
	{
		return;
	}

	for (int32 Index = CoastlineIslandIndices.Num() - 1; Index >= 0; --Index)
	{
		if (CoastlineIslandIndices[Index] == IslandIndex)
		{
			CoastlinePolylinesWorld.RemoveAt(Index);
			CoastlineIslandIndices.RemoveAt(Index);
			if (CoastlineFeatureIds.IsValidIndex(Index))
			{
				CoastlineFeatureIds.RemoveAt(Index);
			}
			if (CoastlineIsInlandSea.IsValidIndex(Index))
			{
				CoastlineIsInlandSea.RemoveAt(Index);
			}
			++CoastlineRevision;
		}
	}
}

void UIH_P1C08_MinimapSubsystem::SetGate2bDevPOIMarkersForIsland(
	const int32 IslandIndex,
	const TArray<FIHGate2bDevPOIMarker>& Markers)
{
	if (IslandIndex == INDEX_NONE)
	{
		return;
	}

	UnregisterGate2bDevPOIMarkersForIsland(IslandIndex);
	Gate2bDevPOIMarkers.Reserve(Gate2bDevPOIMarkers.Num() + Markers.Num());
	for (const FIHGate2bDevPOIMarker& Marker : Markers)
	{
		FIHGate2bDevPOIMarker Entry = Marker;
		Entry.IslandIndex = IslandIndex;
		Gate2bDevPOIMarkers.Add(Entry);
	}
}

void UIH_P1C08_MinimapSubsystem::UnregisterGate2bDevPOIMarkersForIsland(const int32 IslandIndex)
{
	if (IslandIndex == INDEX_NONE)
	{
		return;
	}

	for (int32 Index = Gate2bDevPOIMarkers.Num() - 1; Index >= 0; --Index)
	{
		if (Gate2bDevPOIMarkers[Index].IslandIndex == IslandIndex)
		{
			Gate2bDevPOIMarkers.RemoveAt(Index);
		}
	}
}

void UIH_P1C08_MinimapSubsystem::UnregisterSeaRootsExtentForIsland(const int32 IslandIndex)
{
	if (IslandIndex == INDEX_NONE)
	{
		return;
	}

	for (int32 Index = SeaRootsLayers.Num() - 1; Index >= 0; --Index)
	{
		if (SeaRootsLayers[Index].IslandIndex == IslandIndex)
		{
			SeaRootsLayers.RemoveAt(Index);
		}
	}
}

void UIH_P1C08_MinimapSubsystem::ClearCoastlines()
{
	CoastlinePolylinesWorld.Reset();
	CoastlineIslandIndices.Reset();
	CoastlineFeatureIds.Reset();
	CoastlineIsInlandSea.Reset();
	SeaRootsLayers.Reset();
	Gate2bDevPOIMarkers.Reset();
	++CoastlineRevision;
}

void UIH_P1C08_MinimapSubsystem::RequestMinimapWidgetRepaint()
{
	if (MinimapWidget)
	{
		MinimapWidget->RequestRepaint();
	}
}

void UIH_P1C08_MinimapSubsystem::RequestMinimapRepaint()
{
	RequestMinimapWidgetRepaint();
	if (WorldCoastStrokeWidget && !bMinimapOpen)
	{
		WorldCoastStrokeWidget->RequestRepaint();
	}
}



void UIH_P1C08_MinimapSubsystem::EnsureWidget(AIH_Cube2FlyPlayerController* PC)

{

	if (MinimapWidget || !PC)

	{

		return;

	}



	MinimapWidget = CreateWidget<UIH_P1C08_MinimapWidget>(PC, UIH_P1C08_MinimapWidget::StaticClass());

	if (MinimapWidget)

	{

		MinimapWidget->InitializeMinimap(this, PC);

		MinimapWidget->SetIsFocusable(false);

		MinimapWidget->SetIsEnabled(true);

		MinimapWidget->SetRenderOpacity(1.f);

	}

	else

	{

		UE_LOG(LogTemp, Warning, TEXT("EnsureWidget: failed to create UIH_P1C08_MinimapWidget"));

	}

}



void UIH_P1C08_MinimapSubsystem::EnsureWorldCoastStrokeWidget(AIH_Cube2FlyPlayerController* PC)
{
	if (!IHInvisibleHandSpec::IsCoastStrokeWorldScreenOverlayEnabled() || WorldCoastStrokeWidget || !PC)
	{
		return;
	}

	WorldCoastStrokeWidget = CreateWidget<UIH_P1C08_WorldCoastStrokeWidget>(
		PC, UIH_P1C08_WorldCoastStrokeWidget::StaticClass());
	if (!WorldCoastStrokeWidget)
	{
		return;
	}

	WorldCoastStrokeWidget->InitializeOverlay(this, PC);
	WorldCoastStrokeWidget->SetIsFocusable(false);
	WorldCoastStrokeWidget->SetIsEnabled(true);
	WorldCoastStrokeWidget->SetRenderOpacity(1.f);

	if (!WorldCoastStrokeWidget->IsInViewport())
	{
		WorldCoastStrokeWidget->AddToViewport(49);
		WorldCoastStrokeWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
		WorldCoastStrokeWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
		WorldCoastStrokeWidget->SetPositionInViewport(FVector2D::ZeroVector);
	}

	WorldCoastStrokeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UIH_P1C08_MinimapSubsystem::PrepareMinimapWidget(AIH_Cube2FlyPlayerController* PC)

{

	EnsureWidget(PC);
	EnsureWorldCoastStrokeWidget(PC);

	if (!MinimapWidget || !PC)

	{

		return;

	}



	if (!MinimapWidget->IsInViewport())

	{

		MinimapWidget->AddToViewport(50);

		MinimapWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));

		MinimapWidget->SetAlignmentInViewport(FVector2D::ZeroVector);

		MinimapWidget->SetPositionInViewport(FVector2D::ZeroVector);

	}



	MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);

}



void UIH_P1C08_MinimapSubsystem::OpenMinimap(AIH_Cube2FlyPlayerController* PC)

{

	UE_LOG(LogTemp, Warning, TEXT("OpenMinimap called (PC=%s)"), PC ? *PC->GetName() : TEXT("null"));



	PrepareMinimapWidget(PC);

	if (!MinimapWidget || !PC)

	{

		UE_LOG(

			LogTemp, Warning, TEXT("OpenMinimap aborted: widget=%s PC=%s"),

			MinimapWidget ? *MinimapWidget->GetName() : TEXT("null"),

			PC ? *PC->GetName() : TEXT("null"));

		return;

	}

	// Each open via M snaps to default bottom-right; drag position applies only while open.
	bUseFloatingPosition = false;
	FloatingScreenPosition = FVector2D::ZeroVector;
	ViewCenterWorld = FVector2D::ZeroVector;
	ZoomFactor = 0.f;
	ZoomStepIndex = 0;

	MinimapWidget->ResetPaintDiagnostics();

	MinimapWidget->SetIsEnabled(true);

	MinimapWidget->SetRenderOpacity(1.f);

	MinimapWidget->SetVisibility(ESlateVisibility::Visible);

	bMinimapOpen = true;

	if (WorldCoastStrokeWidget)
	{
		WorldCoastStrokeWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	MinimapWidget->RequestLayoutRefresh();



	UE_LOG(

		LogTemp, Warning,

		TEXT("Minimap opened (inViewport=%d visible=%d floating=%d)"),

		MinimapWidget->IsInViewport(),

		MinimapWidget->GetVisibility() == ESlateVisibility::Visible,

		bUseFloatingPosition ? 1 : 0);



	if (UWorld* World = PC->GetWorld())

	{

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(MinimapWidget, [Widget = MinimapWidget]() {

			if (Widget)

			{

				Widget->RequestLayoutRefresh();

			}

		}));

	}

}



void UIH_P1C08_MinimapSubsystem::CloseMinimap()

{

	bMinimapOpen = false;

	bMouseOverMinimap = false;

	if (MinimapWidget)

	{

		MinimapWidget->CancelPointerInteraction();
		MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);

	}

	if (WorldCoastStrokeWidget)
	{
		WorldCoastStrokeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		WorldCoastStrokeWidget->RequestRepaint();
	}

}



void UIH_P1C08_MinimapSubsystem::ToggleMinimap(AIH_Cube2FlyPlayerController* PC)

{

	if (bMinimapOpen)

	{

		CloseMinimap();

		UE_LOG(LogTemp, Warning, TEXT("Minimap toggled: CLOSED"));

	}

	else

	{

		OpenMinimap(PC);

		UE_LOG(LogTemp, Warning, TEXT("Minimap toggled: OPEN"));

	}

}



void UIH_P1C08_MinimapSubsystem::SetMouseOverMinimap(bool bOver)

{

	bMouseOverMinimap = bOver;

}



bool UIH_P1C08_MinimapSubsystem::IsScreenPointOverMinimap(const FVector2D& ScreenPos) const

{

	return bMinimapOpen && MinimapWidget && MinimapWidget->HitTestPanelAtScreen(ScreenPos);

}



void UIH_P1C08_MinimapSubsystem::SetFloatingScreenPosition(const FVector2D& ViewportLocalPos)

{

	bUseFloatingPosition = true;

	FloatingScreenPosition = ViewportLocalPos;

}



void UIH_P1C08_MinimapSubsystem::UpdateFloatingPosition(const FVector2D& ViewportLocalPos)

{

	SetFloatingScreenPosition(ViewportLocalPos);

	if (MinimapWidget)

	{

		MinimapWidget->RequestRepaint();

	}

}



void UIH_P1C08_MinimapSubsystem::ApplyZoomWithMapPivot(
	float NewZoomFactor,
	const FVector2D& MapPivotLocal,
	const FVector2D& MapSizePx)
{
	NewZoomFactor = FMath::Clamp(NewZoomFactor, 0.f, 1.f);

	if (FMath::IsNearlyEqual(NewZoomFactor, ZoomFactor))
	{
		return;
	}

	if (NewZoomFactor <= KINDA_SMALL_NUMBER)
	{
		ZoomFactor = 0.f;
		ZoomStepIndex = 0;
		ViewCenterWorld = FVector2D::ZeroVector;
		if (MinimapWidget)
		{
			MinimapWidget->RequestRepaint();
		}
		return;
	}

	const FVector2D OldHalfExtent = IH_P1C08_Minimap::HalfExtentFromZoomFactor(
		ZoomFactor, GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());
	const FVector2D NewHalfExtent = IH_P1C08_Minimap::HalfExtentFromZoomFactor(
		NewZoomFactor, GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());
	const FVector2D OldCenter = (ZoomFactor <= KINDA_SMALL_NUMBER)
		? FVector2D::ZeroVector
		: ViewCenterWorld;

	const FVector2D MapHalfPx = MapSizePx * 0.5f;
	if (MapHalfPx.X <= KINDA_SMALL_NUMBER || MapHalfPx.Y <= KINDA_SMALL_NUMBER
		|| OldHalfExtent.X <= KINDA_SMALL_NUMBER || OldHalfExtent.Y <= KINDA_SMALL_NUMBER
		|| NewHalfExtent.X <= KINDA_SMALL_NUMBER || NewHalfExtent.Y <= KINDA_SMALL_NUMBER)
	{
		ZoomFactor = NewZoomFactor;
		ZoomStepIndex = IH_P1C08_Minimap::ZoomStepFromFactor(ZoomFactor);
		if (MinimapWidget)
		{
			MinimapWidget->RequestRepaint();
		}
		return;
	}

	const FVector2D OldScale(
		MapHalfPx.X / OldHalfExtent.X,
		MapHalfPx.Y / OldHalfExtent.Y);
	const FVector2D NewScale(
		MapHalfPx.X / NewHalfExtent.X,
		MapHalfPx.Y / NewHalfExtent.Y);
	const FVector2D PivotOffset = MapPivotLocal - MapSizePx * 0.5f;
	const FVector2D WorldAnchor = OldCenter - FVector2D(
		PivotOffset.X / OldScale.X,
		PivotOffset.Y / OldScale.Y);
	ViewCenterWorld = IH_P1C08_Minimap::ClampViewCenterToRealm(
		WorldAnchor + FVector2D(PivotOffset.X / NewScale.X, PivotOffset.Y / NewScale.Y),
		NewHalfExtent, GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());

	ZoomFactor = NewZoomFactor;
	ZoomStepIndex = IH_P1C08_Minimap::ZoomStepFromFactor(ZoomFactor);

	if (MinimapWidget)
	{
		MinimapWidget->RequestRepaint();
	}
}



void UIH_P1C08_MinimapSubsystem::HandleMouseWheelZoom(float WheelDelta, const FVector2D& ScreenPos)

{

	if (FMath::IsNearlyZero(WheelDelta))

	{

		return;

	}



	const FVector2D MapSizePx = IH_P1C08_Minimap::GetMapContentSizePx(
		GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());
	FVector2D MapPivotLocal = MapSizePx * 0.5f;
	if (MinimapWidget)
	{
		FVector2D ResolvedMapSize = MapSizePx;
		MinimapWidget->TryGetMapLocalFromScreen(ScreenPos, MapPivotLocal, ResolvedMapSize);
	}

	const float Step = FMath::Clamp(FMath::Abs(WheelDelta) * 0.08f, 0.02f, 0.12f);
	ApplyZoomWithMapPivot(
		ZoomFactor + ((WheelDelta > 0.f) ? Step : -Step),
		MapPivotLocal,
		MapSizePx);

}



void UIH_P1C08_MinimapSubsystem::StepZoom(int32 Direction)

{

	if (Direction == 0)

	{

		return;

	}



	ZoomStepIndex = FMath::Clamp(

		ZoomStepIndex + Direction, 0, IH_P1C08_Minimap::ZoomStepCount - 1);

	const float NewZoomFactor = IH_P1C08_Minimap::ZoomFactorFromStep(ZoomStepIndex);
	const FVector2D MapSizePx = IH_P1C08_Minimap::GetMapContentSizePx(
		GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());
	ApplyZoomWithMapPivot(NewZoomFactor, MapSizePx * 0.5f, MapSizePx);

}



IH_P1C08_Minimap::FView UIH_P1C08_MinimapSubsystem::BuildCurrentView(

	const AIH_Cube2FlyPlayerController* PC) const

{

	return IH_P1C08_Minimap::FView::Build(ViewCenterWorld, ZoomFactor, GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());

}



float UIH_P1C08_MinimapSubsystem::GetRealmHalfExtentEWCm() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_WB_Demo004GameInstance* StoryGI = Cast<UIH_WB_Demo004GameInstance>(GI))
		{
			return UIHSeedIslandLibrary::GetRealmHalfExtentEWCm(StoryGI->GetRealmHalfExtentEWKm());
		}
	}

	return IH_P1C08_Minimap::DefaultRealmHalfExtentEWCm;
}

float UIH_P1C08_MinimapSubsystem::GetRealmHalfExtentNSCm() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_WB_Demo004GameInstance* StoryGI = Cast<UIH_WB_Demo004GameInstance>(GI))
		{
			return UIHSeedIslandLibrary::GetRealmHalfExtentNSCm(StoryGI->GetRealmHalfExtentNSKm());
		}
	}

	return IH_P1C08_Minimap::DefaultRealmHalfExtentNSCm;
}



void UIH_P1C08_MinimapSubsystem::FocusCameraOnWorldXY(

	AIH_Cube2FlyPlayerController* PC,

	const FVector2D& WorldXY)

{

	if (!PC)

	{

		return;

	}



	APawn* ViewPawn = PC->GetPawn();

	if (!ViewPawn)

	{

		return;

	}



	const FVector CurrentLoc = ViewPawn->GetActorLocation();

	ViewPawn->SetActorLocation(FVector(WorldXY.X, WorldXY.Y, CurrentLoc.Z));

	if (ZoomFactor > KINDA_SMALL_NUMBER)
	{
		const FVector2D HalfExtent = IH_P1C08_Minimap::HalfExtentFromZoomFactor(
			ZoomFactor, GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());
		ViewCenterWorld = IH_P1C08_Minimap::ClampViewCenterToRealm(
			WorldXY, HalfExtent, GetRealmHalfExtentEWCm(), GetRealmHalfExtentNSCm());
		if (MinimapWidget)
		{
			MinimapWidget->RequestRepaint();
		}
	}

}

