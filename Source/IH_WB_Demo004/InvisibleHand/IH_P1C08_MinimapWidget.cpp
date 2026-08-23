// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_P1C08_MinimapWidget.h"

#include "IH_BuildPaletteSubsystem.h"
#include "IH_P1C08_MinimapCoastline.h"
#include "IH_P1C08_MinimapSubsystem.h"

#include "IH_P1C08_MinimapTypes.h"
#include "IHCoastPolylineSmoothing.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IHP1C10_AzgaarTypes.h"

#include "IH_P1C08_IslandNavSubsystem.h"

#include "IH_WB_IslandActor.h"

#include "IH_WB_Demo004GameMode.h"
#include "IH_WB_Demo004GameInstance.h"

#include "IH_Cube2FlyPlayerController.h"

#include "IH_P1C07_ShipRegistrySubsystem.h"

#include "IH_P1C07_MoveDestinationBuoy.h"
#include "IH_P1C07_SelectableShip.h"
#include "IH_P1C07_CommandableShipActor.h"
#include "IH_P1C07_NavAvoidanceParticipant.h"
#include "IHUIColorSchemeLibrary.h"

#include "GameFramework/Pawn.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Spacer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SViewport.h"

namespace
{
	static float ResolveShipMapGlyphDegrees(const AActor* Ship)
	{
		FVector WorldDir = FVector::ZeroVector;
		if (const IIH_P1C07_NavAvoidanceParticipant* Nav = Cast<IIH_P1C07_NavAvoidanceParticipant>(Ship))
		{
			const FVector Velocity = Nav->GetNavVelocity2D();
			if (Velocity.SizeSquared2D() > FMath::Square(10.f))
			{
				WorldDir = Velocity.GetSafeNormal2D();
			}
		}

		if (WorldDir.IsNearlyZero())
		{
			if (const AIH_P1C07_CommandableShipActor* CommandableShip = Cast<AIH_P1C07_CommandableShipActor>(Ship))
			{
				WorldDir = CommandableShip->GetHullForwardWorld2D(Ship->GetActorRotation());
			}
			else
			{
				WorldDir = Ship->GetActorForwardVector().GetSafeNormal2D();
			}
		}

		return IH_P1C08_Minimap::FView::WorldDirectionToMapGlyphDegrees(FVector2D(WorldDir.X, WorldDir.Y));
	}
}



UIH_P1C08_MinimapWidget::UIH_P1C08_MinimapWidget(const FObjectInitializer& ObjectInitializer)

	: Super(ObjectInitializer)

{

	bHasScriptImplementedPaint = true;

}



void UIH_P1C08_MinimapWidget::EnsureMinimalWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	WidgetTree->RootWidget = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("MinimapSpacer"));
}

IH_P1C08_Minimap::FPanelLayout UIH_P1C08_MinimapWidget::ResolvePanelLayout() const
{
	float RealmHalfExtentEWCm = IH_P1C08_Minimap::DefaultRealmHalfExtentEWCm;
	float RealmHalfExtentNSCm = IH_P1C08_Minimap::DefaultRealmHalfExtentNSCm;
	if (const UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get())
	{
		RealmHalfExtentEWCm = Subsystem->GetRealmHalfExtentEWCm();
		RealmHalfExtentNSCm = Subsystem->GetRealmHalfExtentNSCm();
	}

	return IH_P1C08_Minimap::FPanelLayout::Compute(RealmHalfExtentEWCm, RealmHalfExtentNSCm);
}

void UIH_P1C08_MinimapWidget::SyncSpacerToPanelLayout()
{
	EnsureMinimalWidgetTree();
	if (USpacer* Spacer = Cast<USpacer>(WidgetTree ? WidgetTree->RootWidget : nullptr))
	{
		const FVector2D PanelSize = ResolvePanelLayout().PanelSize;
		Spacer->SetSize(PanelSize);
	}
}

TSharedRef<SWidget> UIH_P1C08_MinimapWidget::RebuildWidget()
{
	EnsureMinimalWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_MinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureMinimalWidgetTree();
	SetVisibility(ESlateVisibility::Collapsed);

	// Plan Addendum 15: north-up compass rose sprite, top-left corner of the minimap panel.
	CompassRoseTexture = LoadObject<UTexture2D>(
		nullptr, TEXT("/Game/InvisibleHand/UI/Icons/T_MinimapCompassRose.T_MinimapCompassRose"));
}

FGeometry UIH_P1C08_MinimapWidget::GetBaseGeometry() const
{
	if (const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
	{
		const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
		const UGameViewportClient* ViewportClient = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
		if (ViewportClient)
		{
			const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
			if (ViewportWidget.IsValid())
			{
				return ViewportWidget->GetTickSpaceGeometry();
			}
		}
	}

	if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		return ResolveBaseGeometry(Cached->GetTickSpaceGeometry());
	}

	return FGeometry();
}

FGeometry UIH_P1C08_MinimapWidget::ResolveBaseGeometry(const FGeometry& AllottedGeometry) const
{
	// Anchor overlay math to the game viewport; the fullscreen widget allotment can differ in PIE.
	const FGeometry ViewportGeometry = GetBaseGeometry();
	if (ViewportGeometry.GetLocalSize().X >= 1.f && ViewportGeometry.GetLocalSize().Y >= 1.f)
	{
		return ViewportGeometry;
	}

	if (AllottedGeometry.GetLocalSize().X >= 1.f && AllottedGeometry.GetLocalSize().Y >= 1.f)
	{
		return AllottedGeometry;
	}

	return ViewportGeometry;
}



FVector2D UIH_P1C08_MinimapWidget::ResolveOverlaySize(const FGeometry& Geometry) const
{
	const FGeometry ViewportGeometry = GetBaseGeometry();
	const FVector2D ViewportSize = ViewportGeometry.GetLocalSize();
	if (ViewportSize.X >= 1.f && ViewportSize.Y >= 1.f)
	{
		return ViewportSize;
	}

	FVector2D Size = Geometry.GetLocalSize();
	if (Size.X >= 1.f && Size.Y >= 1.f)
	{
		return Size;
	}

	if (const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
	{
		int32 ViewX = 0;
		int32 ViewY = 0;
		PC->GetViewportSize(ViewX, ViewY);
		if (ViewX > 0 && ViewY > 0)
		{
			return FVector2D(static_cast<float>(ViewX), static_cast<float>(ViewY));
		}
	}

	return FVector2D(1280.f, 720.f);
}



void UIH_P1C08_MinimapWidget::InitializeMinimap(
	UIH_P1C08_MinimapSubsystem* InSubsystem,
	AIH_Cube2FlyPlayerController* InPC)
{
	MinimapSubsystem = InSubsystem;
	OwnerPC = InPC;
	SyncSpacerToPanelLayout();
}



void UIH_P1C08_MinimapWidget::RequestRepaint()

{

	if (TSharedPtr<SWidget> SlateWidget = GetCachedWidget())

	{

		SlateWidget->Invalidate(EInvalidateWidget::Paint);

	}

}



void UIH_P1C08_MinimapWidget::RequestLayoutRefresh()
{
	SyncSpacerToPanelLayout();
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
	RequestRepaint();
}



bool UIH_P1C08_MinimapWidget::HitTestPanelAtScreen(const FVector2D& ScreenPos) const
{
	return IsScreenPointerOverPanel(ScreenPos);
}

FGeometry UIH_P1C08_MinimapWidget::GetHitTestGeometry() const
{
	if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		return ResolveBaseGeometry(Cached->GetTickSpaceGeometry());
	}

	return GetBaseGeometry();
}

bool UIH_P1C08_MinimapWidget::IsScreenPointerOverPanel(const FVector2D& ScreenPos) const
{
	const FGeometry BaseGeometry = GetHitTestGeometry();
	if (BaseGeometry.GetLocalSize().X < 1.f && BaseGeometry.GetAbsoluteSize().X < 1.f)
	{
		return false;
	}
	return IsPointInPanel(BaseGeometry, ScreenPos);
}

bool UIH_P1C08_MinimapWidget::TryGetMapLocalFromScreen(
	const FVector2D& ScreenPos,
	FVector2D& OutMapLocal,
	FVector2D& OutMapSize) const
{
	const FGeometry BaseGeometry = GetHitTestGeometry();
	if (!IsPointInPanel(BaseGeometry, ScreenPos))
	{
		return false;
	}

	const FGeometry PanelGeometry = MakePanelGeometry(BaseGeometry);
	const FVector2D PanelLocal = PanelGeometry.AbsoluteToLocal(ScreenPos);
	const FSlateRect MapRect = GetMapContentRect(PanelGeometry);
	if (!MapRect.ContainsPoint(PanelLocal))
	{
		return false;
	}

	OutMapSize = GetMapContentSize();
	OutMapLocal = FVector2D(
		FMath::Clamp(PanelLocal.X, MapRect.Left, MapRect.Right) - MapRect.Left,
		FMath::Clamp(PanelLocal.Y, MapRect.Top, MapRect.Bottom) - MapRect.Top);
	return true;
}

bool UIH_P1C08_MinimapWidget::TryGetWorldXYFromScreen(
	const FVector2D& ScreenPos,
	FVector2D& OutWorldXY) const
{
	const FGeometry BaseGeometry = GetHitTestGeometry();
	if (!IsPointInPanel(BaseGeometry, ScreenPos))
	{
		return false;
	}

	const FGeometry PanelGeometry = MakePanelGeometry(BaseGeometry);
	const FVector2D PanelLocal = PanelGeometry.AbsoluteToLocal(ScreenPos);
	const FSlateRect MapRect = GetMapContentRect(PanelGeometry);
	if (!MapRect.ContainsPoint(PanelLocal))
	{
		return false;
	}

	return MapLocalToWorld(PanelGeometry, PanelLocal, OutWorldXY);
}

bool UIH_P1C08_MinimapWidget::HandleScreenPointerDown(const FVector2D& ScreenPos)
{
	UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get();
	AIH_Cube2FlyPlayerController* PC = OwnerPC.Get();
	if (!Subsystem || !PC || !Subsystem->IsMinimapOpen())
	{
		return false;
	}

	const FGeometry BaseGeometry = GetHitTestGeometry();
	if (!IsPointInPanel(BaseGeometry, ScreenPos))
	{
		return false;
	}

	const FGeometry PanelGeometry = MakePanelGeometry(BaseGeometry);
	const FVector2D Local = PanelGeometry.AbsoluteToLocal(ScreenPos);

	if (GetCloseButtonRect(PanelGeometry, /*bIncludeHitPadding*/ true).ContainsPoint(Local))
	{
		Subsystem->CloseMinimap();
		return true;
	}

	if (GetTitleBarRect(PanelGeometry).ContainsPoint(Local)
		&& !GetCloseButtonRect(PanelGeometry).ContainsPoint(Local))
	{
		if (!Subsystem->UsesFloatingPosition())
		{
			Subsystem->SetFloatingScreenPosition(GetPanelTopLeftLocal(MakeOverlayGeometry(BaseGeometry)));
		}
		bDraggingPanel = true;
		DragGrabOffset = Local;
		return true;
	}

	if (GetMapContentRect(PanelGeometry).ContainsPoint(Local))
	{
		FVector2D WorldXY = FVector2D::ZeroVector;
		if (MapLocalToWorld(PanelGeometry, Local, WorldXY))
		{
			if (UGameInstance* GI = PC->GetGameInstance())
			{
				if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
				{
					if (BuildPalette->IsDragActive())
					{
						BuildPalette->UpdateDragGhostFromWorldXY(PC, WorldXY);
						RequestRepaint();
						return true;
					}
				}
			}

			const bool bShiftDown = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
			if (bShiftDown)
			{
				int32 HitIslandIndex = INDEX_NONE;
				if (PC->TryGetIslandIndexAtWorldXY(WorldXY, HitIslandIndex))
				{
					PC->RequestFocusIsland(HitIslandIndex);
				}
				return true;
			}

			Subsystem->FocusCameraOnWorldXY(PC, WorldXY);
		}
		return true;
	}

	return true;
}

void UIH_P1C08_MinimapWidget::HandleScreenPointerMove(const FVector2D& ScreenPos)
{
	UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get();
	AIH_Cube2FlyPlayerController* PC = OwnerPC.Get();
	if (Subsystem && Subsystem->IsMinimapOpen())
	{
		Subsystem->SetMouseOverMinimap(IsScreenPointerOverPanel(ScreenPos));
	}

	if (PC && Subsystem && Subsystem->IsMinimapOpen())
	{
		if (UGameInstance* GI = PC->GetGameInstance())
		{
			if (UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
			{
				if (BuildPalette->IsDragActive())
				{
					FVector2D WorldXY = FVector2D::ZeroVector;
					if (TryGetWorldXYFromScreen(ScreenPos, WorldXY))
					{
						BuildPalette->UpdateDragGhostFromWorldXY(PC, WorldXY);
						RequestRepaint();
					}
				}
			}
		}
	}

	if (!bDraggingPanel || !Subsystem)
	{
		return;
	}

	const FGeometry BaseGeometry = GetHitTestGeometry();
	const FGeometry OverlayGeometry = MakeOverlayGeometry(BaseGeometry);
	const FVector2D TopLeftLocal = OverlayGeometry.AbsoluteToLocal(ScreenPos) - DragGrabOffset;
	Subsystem->UpdateFloatingPosition(TopLeftLocal);
	RequestRepaint();
}

void UIH_P1C08_MinimapWidget::HandleScreenPointerUp(const FVector2D& ScreenPos)
{
	if (UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get())
	{
		Subsystem->SetMouseOverMinimap(IsScreenPointerOverPanel(ScreenPos));
	}
	bDraggingPanel = false;
}

void UIH_P1C08_MinimapWidget::CancelPointerInteraction()
{
	bDraggingPanel = false;
	if (UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get())
	{
		Subsystem->SetMouseOverMinimap(false);
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
}



void UIH_P1C08_MinimapWidget::ResetPaintDiagnostics()

{

	bLoggedPaintGeometryOnce = false;

}



FVector2D UIH_P1C08_MinimapWidget::GetPanelTopLeftLocal(const FGeometry& Geometry) const

{

	const UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get();

	const FVector2D ViewSize = ResolveOverlaySize(Geometry);

	const FVector2D PanelSize = GetPanelSize();

	const float Margin = IH_P1C08_Minimap::DefaultMarginPx;



	if (Subsystem && Subsystem->UsesFloatingPosition())

	{

		return Subsystem->GetFloatingScreenPosition();

	}



	return FVector2D(

		FMath::Max(0.f, ViewSize.X - PanelSize.X - Margin),

		FMath::Max(0.f, ViewSize.Y - PanelSize.Y - Margin));

}



FGeometry UIH_P1C08_MinimapWidget::MakeOverlayGeometry(const FGeometry& AllottedGeometry) const
{
	const FGeometry BaseGeometry = ResolveBaseGeometry(AllottedGeometry);
	const FVector2D OverlaySize = ResolveOverlaySize(BaseGeometry);
	return BaseGeometry.MakeChild(
		FVector2f(OverlaySize),
		FSlateLayoutTransform(FVector2f::ZeroVector));
}

FGeometry UIH_P1C08_MinimapWidget::MakePanelGeometry(const FGeometry& AllottedGeometry) const

{

	const FVector2D PanelOrigin = GetPanelTopLeftLocal(MakeOverlayGeometry(AllottedGeometry));

	const FVector2D PanelSize = GetPanelSize();

	return MakeOverlayGeometry(AllottedGeometry).MakeChild(

		FVector2f(PanelSize),

		FSlateLayoutTransform(FVector2f(PanelOrigin)));

}



FSlateRect UIH_P1C08_MinimapWidget::GetPanelScreenRect(const FGeometry& Geometry) const
{
	const FGeometry PanelGeometry = MakePanelGeometry(Geometry);
	const FVector2D PanelSize = PanelGeometry.GetLocalSize();
	const FVector2D TopLeft = PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = PanelGeometry.LocalToAbsolute(PanelSize);
	return FSlateRect(TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
}

bool UIH_P1C08_MinimapWidget::IsPointInPanel(const FGeometry& Geometry, const FVector2D& ScreenPos) const

{

	return GetPanelScreenRect(Geometry).ContainsPoint(ScreenPos);

}



FVector2D UIH_P1C08_MinimapWidget::ScreenToPanelLocal(

	const FGeometry& Geometry,

	const FVector2D& ScreenPos) const

{

	return MakePanelGeometry(Geometry).AbsoluteToLocal(ScreenPos);

}



FVector2D UIH_P1C08_MinimapWidget::GetMapContentSize() const
{
	return ResolvePanelLayout().MapSize;
}

FVector2D UIH_P1C08_MinimapWidget::GetPanelSize() const
{
	return ResolvePanelLayout().PanelSize;
}

FVector2D UIH_P1C08_MinimapWidget::GetMapContentOrigin() const
{
	return ResolvePanelLayout().MapOrigin;
}



FSlateRect UIH_P1C08_MinimapWidget::GetTitleBarRect(const FGeometry& PanelGeometry) const

{

	const FVector2D Size = PanelGeometry.GetLocalSize();

	return FSlateRect(0.f, 0.f, Size.X, IH_P1C08_Minimap::TitleBarHeightPx);

}



FSlateRect UIH_P1C08_MinimapWidget::GetCloseButtonRect(const FGeometry& PanelGeometry, bool bIncludeHitPadding) const
{
	const FVector2D Size = PanelGeometry.GetLocalSize();
	const float Button = IH_P1C08_Minimap::CloseButtonSizePx;
	const float RightInset = 4.f;
	const float TopInset = FMath::Max(0.f, (IH_P1C08_Minimap::TitleBarHeightPx - Button) * 0.5f);

	FSlateRect CloseRect(
		Size.X - Button - RightInset,
		TopInset,
		Size.X - RightInset,
		TopInset + Button);

	if (bIncludeHitPadding)
	{
		const float HitPadding = IH_P1C08_Minimap::CloseButtonHitPaddingPx;
		CloseRect = CloseRect.ExtendBy(FMargin(HitPadding));
		CloseRect.Top = FMath::Max(0.f, CloseRect.Top);
		CloseRect.Right = FMath::Min(Size.X, CloseRect.Right);
		CloseRect.Bottom = FMath::Min(IH_P1C08_Minimap::TitleBarHeightPx, CloseRect.Bottom);
	}

	return CloseRect;
}



FSlateRect UIH_P1C08_MinimapWidget::GetMapContentRect(const FGeometry& PanelGeometry) const

{

	const FVector2D Origin = GetMapContentOrigin();

	const FVector2D MapSize = GetMapContentSize();

	return FSlateRect(

		Origin.X,

		Origin.Y,

		Origin.X + MapSize.X,

		Origin.Y + MapSize.Y);

}



bool UIH_P1C08_MinimapWidget::MapLocalToWorld(

	const FGeometry& PanelGeometry,

	const FVector2D& MapLocal,

	FVector2D& OutWorldXY) const

{

	const UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get();

	const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get();

	if (!Subsystem || !PC)

	{

		return false;

	}



	const FSlateRect MapRect = GetMapContentRect(PanelGeometry);

	const FVector2D MapLocalClamped(

		FMath::Clamp(MapLocal.X, MapRect.Left, MapRect.Right),

		FMath::Clamp(MapLocal.Y, MapRect.Top, MapRect.Bottom));

	const FVector2D MapSize = GetMapContentSize();

	const FVector2D Relative(

		MapLocalClamped.X - MapRect.Left,

		MapLocalClamped.Y - MapRect.Top);

	const IH_P1C08_Minimap::FView View = Subsystem->BuildCurrentView(PC);

	return View.MapLocalToWorld(Relative, MapSize, OutWorldXY);

}



void UIH_P1C08_MinimapWidget::DrawClosedPolyline(

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FGeometry& Geometry,

	const TArray<FVector2D>& LocalPoints,

	const FLinearColor& Color,

	float Thickness,

	bool bAntialias) const

{

	if (LocalPoints.Num() < 2)

	{

		return;

	}



	TArray<FVector2D> LinePoints;

	LinePoints.Reserve(LocalPoints.Num() + 1);

	for (const FVector2D& Point : LocalPoints)

	{

		LinePoints.Add(Point);

	}

	if (!LocalPoints[0].Equals(LocalPoints.Last(), KINDA_SMALL_NUMBER))
	{
		LinePoints.Add(LocalPoints[0]);
	}



	FSlateDrawElement::MakeLines(

		OutDrawElements,

		LayerId,

		Geometry.ToPaintGeometry(),

		LinePoints,

		ESlateDrawEffect::None,

		Color,

		bAntialias,

		Thickness);

}

void UIH_P1C08_MinimapWidget::DrawDashedClosedPolyline(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& Geometry,
	const TArray<FVector2D>& LocalPoints,
	const FLinearColor& Color,
	float Thickness,
	float DashLengthPx,
	float GapLengthPx) const
{
	if (LocalPoints.Num() < 2 || DashLengthPx <= KINDA_SMALL_NUMBER)
	{
		DrawClosedPolyline(OutDrawElements, LayerId, Geometry, LocalPoints, Color, Thickness);
		return;
	}

	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
	const int32 NumPoints = LocalPoints.Num();
	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		const FVector2D& Start = LocalPoints[PointIdx];
		const FVector2D& End = LocalPoints[(PointIdx + 1) % NumPoints];
		const FVector2D Segment = End - Start;
		const float SegmentLength = Segment.Size();
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector2D Dir = Segment / SegmentLength;
		float Traveled = 0.f;
		while (Traveled < SegmentLength)
		{
			const float DashEndDist = FMath::Min(Traveled + DashLengthPx, SegmentLength);
			const TArray<FVector2D> DashSegment = {
				Start + Dir * Traveled,
				Start + Dir * DashEndDist,
			};
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				PaintGeometry,
				DashSegment,
				ESlateDrawEffect::None,
				Color,
				true,
				Thickness);
			Traveled = DashEndDist + GapLengthPx;
		}
	}
}



void UIH_P1C08_MinimapWidget::DrawShipGlyph(

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FGeometry& Geometry,

	const FVector2D& TipLocal,

	float YawDegrees,

	bool bSelected) const

{
	const float TipLength = bSelected ? 16.f : 14.f;
	const float HalfWidth = bSelected ? 6.f : 5.f;

	const float YawRad = FMath::DegreesToRadians(YawDegrees);
	const FVector2D Forward(FMath::Cos(YawRad), FMath::Sin(YawRad));
	const FVector2D Right(-Forward.Y, Forward.X);

	const FVector2D Tip = TipLocal;
	const FVector2D Back = Tip - Forward * TipLength;
	const FVector2D Left = Back + Right * HalfWidth;
	const FVector2D RightPt = Back - Right * HalfWidth;

	const FLinearColor ArrowRed(1.f, 0.f, 0.f, 1.f);
	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();

	if (bSelected)
	{
		const TArray<FVector2D> OutlinePoints = { Tip, Left, RightPt };
		DrawClosedPolyline(
			OutDrawElements,
			LayerId,
			Geometry,
			OutlinePoints,
			ArrowRed,
			2.f);
	}
	else
	{
		constexpr int32 NumFillLines = 6;
		for (int32 LineIdx = 0; LineIdx <= NumFillLines; ++LineIdx)
		{
			const float Alpha = static_cast<float>(LineIdx) / static_cast<float>(NumFillLines);
			const FVector2D FillLeft = FMath::Lerp(Left, Tip, Alpha);
			const FVector2D FillRight = FMath::Lerp(RightPt, Tip, Alpha);
			const TArray<FVector2D> FillSegment = { FillLeft, FillRight };
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				PaintGeometry,
				FillSegment,
				ESlateDrawEffect::None,
				ArrowRed,
				true,
				3.f);
		}

		const TArray<FVector2D> OutlinePoints = { Tip, Left, RightPt };
		DrawClosedPolyline(
			OutDrawElements,
			LayerId,
			Geometry,
			OutlinePoints,
			ArrowRed,
			1.5f);
	}
}



void UIH_P1C08_MinimapWidget::DrawCameraV(

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FGeometry& Geometry,

	const FVector2D& ApexLocal,

	float YawDegrees) const

{

	const float ArmLength = 14.f;

	const float OpenAngleDeg = 28.f;
	const float LineThickness = IHInvisibleHandSpec::MinimapCameraViewGlyphLineThicknessPx;
	const float ApexDotRadius = IHInvisibleHandSpec::MinimapCameraViewGlyphApexDotRadiusPx;

	const float YawRad = FMath::DegreesToRadians(YawDegrees);

	const FVector2D Forward(FMath::Cos(YawRad), FMath::Sin(YawRad));



	auto Rot = [](const FVector2D& V, float AngleRad) {

		return FVector2D(

			V.X * FMath::Cos(AngleRad) - V.Y * FMath::Sin(AngleRad),

			V.X * FMath::Sin(AngleRad) + V.Y * FMath::Cos(AngleRad));

	};



	const FVector2D LeftDir = Rot(Forward, FMath::DegreesToRadians(OpenAngleDeg));

	const FVector2D RightDir = Rot(Forward, FMath::DegreesToRadians(-OpenAngleDeg));



	TArray<FVector2D> LeftArm = { ApexLocal, ApexLocal + LeftDir * ArmLength };

	TArray<FVector2D> RightArm = { ApexLocal, ApexLocal + RightDir * ArmLength };

	const FLinearColor CameraColor = IHInvisibleHandSpec::MinimapCameraViewGlyphColor;



	FSlateDrawElement::MakeLines(

		OutDrawElements, LayerId, Geometry.ToPaintGeometry(), LeftArm,

		ESlateDrawEffect::None, CameraColor, true, LineThickness);

	FSlateDrawElement::MakeLines(

		OutDrawElements, LayerId, Geometry.ToPaintGeometry(), RightArm,

		ESlateDrawEffect::None, CameraColor, true, LineThickness);

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2f DotSize(ApexDotRadius * 2.f, ApexDotRadius * 2.f);
	const FVector2f DotPos(ApexLocal.X - ApexDotRadius, ApexLocal.Y - ApexDotRadius);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		Geometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPos)),
		WhiteBrush,
		ESlateDrawEffect::None,
		CameraColor);

}

void UIH_P1C08_MinimapWidget::DrawMapDot(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& Geometry,
	const FVector2D& CenterLocal,
	float Radius,
	const FLinearColor& Color) const
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2f Size(Radius * 2.f, Radius * 2.f);
	const FVector2f Pos(CenterLocal.X - Radius, CenterLocal.Y - Radius);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)),
		WhiteBrush,
		ESlateDrawEffect::None,
		Color);
}

void UIH_P1C08_MinimapWidget::DrawMapPlus(
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FGeometry& Geometry,
	const FVector2D& CenterLocal,
	const float HalfLengthPx,
	const float LineThicknessPx,
	const FLinearColor& Color) const
{
	TArray<FVector2D> Horizontal = {
		FVector2D(CenterLocal.X - HalfLengthPx, CenterLocal.Y),
		FVector2D(CenterLocal.X + HalfLengthPx, CenterLocal.Y),
	};
	TArray<FVector2D> Vertical = {
		FVector2D(CenterLocal.X, CenterLocal.Y - HalfLengthPx),
		FVector2D(CenterLocal.X, CenterLocal.Y + HalfLengthPx),
	};
	FSlateDrawElement::MakeLines(
		OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Horizontal,
		ESlateDrawEffect::None, Color, true, LineThicknessPx);
	FSlateDrawElement::MakeLines(
		OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Vertical,
		ESlateDrawEffect::None, Color, true, LineThicknessPx);
}

void UIH_P1C08_MinimapWidget::DrawSolidLocalRect(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& Geometry,
	const FSlateRect& LocalRect,
	const FLinearColor& Color) const
{
	const FVector2f Size(LocalRect.Right - LocalRect.Left, LocalRect.Bottom - LocalRect.Top);
	if (Size.X < 1.f || Size.Y < 1.f)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(LocalRect.Left, LocalRect.Top))),
		WhiteBrush,
		ESlateDrawEffect::None,
		Color);
}

namespace IHMinimapBandFillPrivate
{
	using FSpan = TPair<float, float>;

	static void AppendScanlineIntersections(const TArray<FVector2D>& Poly, const float Y, TArray<float>& OutX)
	{
		const int32 NumPoints = Poly.Num();
		if (NumPoints < 3)
		{
			return;
		}

		for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
		{
			const FVector2D& A = Poly[PointIdx];
			const FVector2D& B = Poly[(PointIdx + 1) % NumPoints];
			const float MinEdgeY = FMath::Min(A.Y, B.Y);
			const float MaxEdgeY = FMath::Max(A.Y, B.Y);
			if (Y < MinEdgeY || Y >= MaxEdgeY)
			{
				continue;
			}
			if (FMath::IsNearlyEqual(A.Y, B.Y))
			{
				continue;
			}
			const float T = (Y - A.Y) / (B.Y - A.Y);
			OutX.Add(FMath::Lerp(A.X, B.X, T));
		}
	}

	static TArray<FSpan> BuildInsideSpansFromIntersections(TArray<float> Intersections)
	{
		TArray<FSpan> Spans;
		if (Intersections.Num() < 2)
		{
			return Spans;
		}

		Intersections.Sort();
		for (int32 Idx = 0; Idx + 1 < Intersections.Num(); Idx += 2)
		{
			const float Left = Intersections[Idx];
			const float Right = Intersections[Idx + 1];
			if (Right - Left > 0.01f)
			{
				Spans.Add(FSpan(Left, Right));
			}
		}
		return Spans;
	}

	static TArray<FSpan> SubtractSpans(const TArray<FSpan>& BaseSpans, const TArray<FSpan>& HoleSpans)
	{
		TArray<FSpan> Result = BaseSpans;
		for (const FSpan& Hole : HoleSpans)
		{
			TArray<FSpan> Next;
			for (const FSpan& Span : Result)
			{
				if (Hole.Value <= Span.Key || Hole.Key >= Span.Value)
				{
					Next.Add(Span);
					continue;
				}
				if (Hole.Key > Span.Key + 0.01f)
				{
					Next.Add(FSpan(Span.Key, Hole.Key));
				}
				if (Hole.Value < Span.Value - 0.01f)
				{
					Next.Add(FSpan(Hole.Value, Span.Value));
				}
			}
			Result = MoveTemp(Next);
		}
		return Result;
	}

	static void ComputePolyVerticalBounds(
		const TArray<FVector2D>& Poly,
		float& OutMinY,
		float& OutMaxY)
	{
		OutMinY = TNumericLimits<float>::Max();
		OutMaxY = TNumericLimits<float>::Lowest();
		for (const FVector2D& Point : Poly)
		{
			OutMinY = FMath::Min(OutMinY, Point.Y);
			OutMaxY = FMath::Max(OutMaxY, Point.Y);
		}
	}
}

void UIH_P1C08_MinimapWidget::DrawFilledAnnulus(
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FGeometry& Geometry,
	const TArray<FVector2D>& InnerLocal,
	const TArray<FVector2D>& OuterLocal,
	const FLinearColor& Color,
	const bool bLenientMinimapFill) const
{
	const int32 NumPoints = InnerLocal.Num();
	if (NumPoints < 3 || OuterLocal.Num() != NumPoints)
	{
		return;
	}

	if (bLenientMinimapFill)
	{
		// Signed-off POKED125 path (2026-06): full-ring scanline via MakeBox.
		using namespace IHMinimapBandFillPrivate;

		float MinY = 0.f;
		float MaxY = 0.f;
		ComputePolyVerticalBounds(OuterLocal, MinY, MaxY);
		float InnerMinY = 0.f;
		float InnerMaxY = 0.f;
		ComputePolyVerticalBounds(InnerLocal, InnerMinY, InnerMaxY);
		MinY = FMath::Min(MinY, InnerMinY);
		MaxY = FMath::Max(MaxY, InnerMaxY);

		const int32 StartY = FMath::FloorToInt(MinY);
		const int32 EndY = FMath::CeilToInt(MaxY);
		if (EndY < StartY)
		{
			return;
		}

		for (int32 ScanY = StartY; ScanY <= EndY; ++ScanY)
		{
			const float Y = static_cast<float>(ScanY) + 0.5f;
			TArray<float> OuterXs;
			TArray<float> InnerXs;
			AppendScanlineIntersections(OuterLocal, Y, OuterXs);
			AppendScanlineIntersections(InnerLocal, Y, InnerXs);

			const TArray<FSpan> OuterSpans = BuildInsideSpansFromIntersections(MoveTemp(OuterXs));
			const TArray<FSpan> InnerSpans = BuildInsideSpansFromIntersections(MoveTemp(InnerXs));
			const TArray<FSpan> FillSpans = SubtractSpans(OuterSpans, InnerSpans);
			for (const FSpan& Span : FillSpans)
			{
				DrawSolidLocalRect(
					OutDrawElements,
					LayerId,
					Geometry,
					FSlateRect(Span.Key, Y - 0.5f, Span.Value, Y + 0.5f),
					Color);
			}
		}
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateResourceHandle ResourceHandle = WhiteBrush->GetRenderingResource();
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
	Vertices.Reserve(NumPoints * 4);
	Indices.Reserve(NumPoints * 6);

	const float Alpha = FMath::Clamp(Color.A, 0.f, 1.f);
	const FColor FillColor = FLinearColor(Color.R * Alpha, Color.G * Alpha, Color.B * Alpha, Alpha).ToFColor(true);
	const FSlateRenderTransform& AccumulatedRenderTransform = Geometry.GetAccumulatedRenderTransform();
	const float MinOutwardDistance = IHInvisibleHandSpec::SeaRootsMinValidShelfOutwardMeters / 1000.f;

	FVector2D InnerCentroid = FVector2D::ZeroVector;
	for (const FVector2D& P : InnerLocal)
	{
		InnerCentroid += P;
	}
	InnerCentroid /= static_cast<float>(NumPoints);

	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		const int32 NextIdx = (PointIdx + 1) % NumPoints;
		const FVector2D& InnerA = InnerLocal[PointIdx];
		const FVector2D& InnerB = InnerLocal[NextIdx];
		const FVector2D& OuterA = OuterLocal[PointIdx];
		const FVector2D& OuterB = OuterLocal[NextIdx];
		const FVector2D SegmentOutward =
			FVector2D(InnerB.Y - InnerA.Y, InnerA.X - InnerB.X).GetSafeNormal();
		if (!FIHCoastPolylineSmoothing::IsQuadOutwardValid(
			InnerA, InnerB, OuterA, OuterB, SegmentOutward, MinOutwardDistance))
		{
			continue;
		}
		if (FVector2D::DistSquared(OuterA, InnerCentroid) < FVector2D::DistSquared(InnerA, InnerCentroid)
			|| FVector2D::DistSquared(OuterB, InnerCentroid) < FVector2D::DistSquared(InnerB, InnerCentroid))
		{
			continue;
		}

		const int32 VertexBase = Vertices.Num();
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			AccumulatedRenderTransform, FVector2f(InnerA), FVector2f::ZeroVector, FillColor));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			AccumulatedRenderTransform, FVector2f(OuterA), FVector2f::ZeroVector, FillColor));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			AccumulatedRenderTransform, FVector2f(InnerB), FVector2f::ZeroVector, FillColor));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			AccumulatedRenderTransform, FVector2f(OuterB), FVector2f::ZeroVector, FillColor));

		Indices.Add(VertexBase);
		Indices.Add(VertexBase + 2);
		Indices.Add(VertexBase + 1);
		Indices.Add(VertexBase + 1);
		Indices.Add(VertexBase + 2);
		Indices.Add(VertexBase + 3);
	}

	if (Vertices.Num() < 6 || Indices.Num() < 6)
	{
		return;
	}

	FSlateDrawElement::MakeCustomVerts(
		OutDrawElements,
		LayerId,
		ResourceHandle,
		Vertices,
		Indices,
		nullptr,
		0,
		0,
		ESlateDrawEffect::PreMultipliedAlpha);
}

void UIH_P1C08_MinimapWidget::DrawFilledPolygon(
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FGeometry& Geometry,
	const TArray<FVector2D>& RingLocal,
	const FLinearColor& Color) const
{
	// Same scanline technique as DrawFilledAnnulus's lenient-fill path, minus the inner-ring
	// subtraction - a plain single-ring fill, robust to the concave/nested-inlet shapes real
	// coastlines have (unlike a simple triangle fan, which breaks on concave polygons).
	using namespace IHMinimapBandFillPrivate;

	if (RingLocal.Num() < 3)
	{
		return;
	}

	float MinY = 0.f;
	float MaxY = 0.f;
	ComputePolyVerticalBounds(RingLocal, MinY, MaxY);

	const int32 StartY = FMath::FloorToInt(MinY);
	const int32 EndY = FMath::CeilToInt(MaxY);
	if (EndY < StartY)
	{
		return;
	}

	for (int32 ScanY = StartY; ScanY <= EndY; ++ScanY)
	{
		const float Y = static_cast<float>(ScanY) + 0.5f;
		TArray<float> Xs;
		AppendScanlineIntersections(RingLocal, Y, Xs);
		const TArray<FSpan> Spans = BuildInsideSpansFromIntersections(MoveTemp(Xs));
		for (const FSpan& Span : Spans)
		{
			DrawSolidLocalRect(
				OutDrawElements,
				LayerId,
				Geometry,
				FSlateRect(Span.Key, Y - 0.5f, Span.Value, Y + 0.5f),
				Color);
		}
	}
}

int32 UIH_P1C08_MinimapWidget::NativePaint(

	const FPaintArgs& Args,

	const FGeometry& AllottedGeometry,

	const FSlateRect& MyCullingRect,

	FSlateWindowElementList& OutDrawElements,

	int32 LayerId,

	const FWidgetStyle& InWidgetStyle,

	bool bParentEnabled) const

{

	int32 MaxLayer = LayerId;

	const UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get();
	const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get();
	if (!Subsystem || !PC || !Subsystem->IsMinimapOpen())
	{
		return MaxLayer;
	}

	const FGeometry BaseGeometry = ResolveBaseGeometry(AllottedGeometry);
	const FVector2D OverlaySize = ResolveOverlaySize(BaseGeometry);
	const IH_P1C08_Minimap::FPanelLayout Layout = ResolvePanelLayout();
	const FGeometry PanelGeometry = MakePanelGeometry(BaseGeometry);
	const FVector2D PanelSize = Layout.PanelSize;
	const FVector2D PanelOrigin = GetPanelTopLeftLocal(MakeOverlayGeometry(BaseGeometry));
	const FSlateRect MapRect = GetMapContentRect(PanelGeometry);
	const FVector2D MapSize = Layout.MapSize;

	if (!bLoggedPaintGeometryOnce)
	{
		bLoggedPaintGeometryOnce = true;

		const float VerticalSlack = PanelSize.Y
			- MapSize.Y
			- (IH_P1C08_Minimap::UniformFrameMarginPx * 2.f);

		UE_LOG(
			LogTemp, Warning,
			TEXT("Minimap NativePaint layout (overlay=%s allotted=%s panel=%s map=%s mapRect=%s origin=%s slackY=%.2f open=%d visible=%d)"),
			*OverlaySize.ToString(),
			*AllottedGeometry.GetLocalSize().ToString(),
			*PanelSize.ToString(),
			*MapSize.ToString(),
			*FVector2D(MapRect.Right - MapRect.Left, MapRect.Bottom - MapRect.Top).ToString(),
			*PanelOrigin.ToString(),
			VerticalSlack,
			Subsystem->IsMinimapOpen() ? 1 : 0,
			GetVisibility() == ESlateVisibility::Visible ? 1 : 0);
	}

	if (PanelSize.X < 1.f || PanelSize.Y < 1.f)
	{
		return MaxLayer;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const IH_P1C08_Minimap::FView View = Subsystem->BuildCurrentView(PC);
	const FLinearColor PanelBackground = UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(
		FName(TEXT("PanelBackground")), 0.92f);
	const float FrameMargin = IH_P1C08_Minimap::UniformFrameMarginPx;
	const float TitleBar = IH_P1C08_Minimap::TitleBarHeightPx;

	DrawSolidLocalRect(
		OutDrawElements, MaxLayer + 1, PanelGeometry,
		FSlateRect(0.f, 0.f, PanelSize.X, TitleBar),
		PanelBackground);
	DrawSolidLocalRect(
		OutDrawElements, MaxLayer + 1, PanelGeometry,
		FSlateRect(FrameMargin, TitleBar, PanelSize.X - FrameMargin, FrameMargin),
		PanelBackground);
	DrawSolidLocalRect(
		OutDrawElements, MaxLayer + 1, PanelGeometry,
		FSlateRect(0.f, TitleBar, FrameMargin, PanelSize.Y),
		PanelBackground);
	DrawSolidLocalRect(
		OutDrawElements, MaxLayer + 1, PanelGeometry,
		FSlateRect(PanelSize.X - FrameMargin, TitleBar, PanelSize.X, PanelSize.Y),
		PanelBackground);
	DrawSolidLocalRect(
		OutDrawElements, MaxLayer + 1, PanelGeometry,
		FSlateRect(0.f, PanelSize.Y - FrameMargin, PanelSize.X, PanelSize.Y),
		PanelBackground);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer + 2,
		PanelGeometry.ToPaintGeometry(
			FVector2f(MapSize),
			FSlateLayoutTransform(FVector2f(MapRect.Left, MapRect.Top))),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.03f, 0.16f, 0.34f, 1.f));

	// Plan Addendum 15/16: north-up compass rose, top-left corner. Base height = 1/10 of the
	// panel's own vertical extent (original user spec), doubled per follow-up direction. Width
	// kept at the source texture's native aspect ratio so the rose isn't stretched.
	if (CompassRoseTexture)
	{
		const float RoseHeight = PanelSize.Y * 0.1f * 2.f;
		const float TextureAspect =
			static_cast<float>(CompassRoseTexture->GetSizeX()) / static_cast<float>(FMath::Max(1, CompassRoseTexture->GetSizeY()));
		const float RoseWidth = RoseHeight * TextureAspect;
		const FVector2D RosePos(MapRect.Left + FrameMargin, MapRect.Top + FrameMargin);

		FSlateBrush RoseBrush;
		RoseBrush.SetResourceObject(CompassRoseTexture);
		RoseBrush.ImageSize = FVector2D(RoseWidth, RoseHeight);
		RoseBrush.DrawAs = ESlateBrushDrawType::Image;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			MaxLayer + 3,
			PanelGeometry.ToPaintGeometry(
				FVector2f(RoseWidth, RoseHeight),
				FSlateLayoutTransform(FVector2f(RosePos))),
			&RoseBrush,
			ESlateDrawEffect::None,
			FLinearColor::White);
	}



	const FLinearColor BorderColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));

	TArray<FVector2D> OuterBorder = {

		FVector2D(0.f, 0.f),

		FVector2D(PanelSize.X, 0.f),

		FVector2D(PanelSize.X, PanelSize.Y),

		FVector2D(0.f, PanelSize.Y),

		FVector2D(0.f, 0.f),

	};

	FSlateDrawElement::MakeLines(

		OutDrawElements, MaxLayer + 3, PanelGeometry.ToPaintGeometry(), OuterBorder,

		ESlateDrawEffect::None, BorderColor, true, 2.f);



	TArray<FVector2D> MapBorder = {

		FVector2D(MapRect.Left, MapRect.Top),

		FVector2D(MapRect.Right, MapRect.Top),

		FVector2D(MapRect.Right, MapRect.Bottom),

		FVector2D(MapRect.Left, MapRect.Bottom),

		FVector2D(MapRect.Left, MapRect.Top),

	};

	FSlateDrawElement::MakeLines(

		OutDrawElements, MaxLayer + 3, PanelGeometry.ToPaintGeometry(), MapBorder,

		ESlateDrawEffect::None, FLinearColor(0.85f, 0.88f, 0.92f, 0.95f), true, 1.f);



	const auto WorldToMapLocal = [&](const FVector2D& WorldXY) {

		const FVector2D Relative = View.WorldToMapLocal(WorldXY, MapSize);

		return FVector2D(MapRect.Left + Relative.X, MapRect.Top + Relative.Y);

	};

	const FPaintGeometry MapClipGeometry = PanelGeometry.ToPaintGeometry(
		FVector2f(MapSize),
		FSlateLayoutTransform(FVector2f(MapRect.Left, MapRect.Top)));
	OutDrawElements.PushClip(FSlateClippingZone(MapClipGeometry));

	// Tank boundary (φ rectangle at full zoom; clipped when zoomed in).
	{
		const float RealmHalfExtentEWCm = Subsystem->GetRealmHalfExtentEWCm();
		const float RealmHalfExtentNSCm = Subsystem->GetRealmHalfExtentNSCm();
		const TArray<FVector2D> TankBoundsWorld = {
			FVector2D(-RealmHalfExtentEWCm, -RealmHalfExtentNSCm),
			FVector2D(RealmHalfExtentEWCm, -RealmHalfExtentNSCm),
			FVector2D(RealmHalfExtentEWCm, RealmHalfExtentNSCm),
			FVector2D(-RealmHalfExtentEWCm, RealmHalfExtentNSCm),
		};
		TArray<FVector2D> LocalTankBounds;
		LocalTankBounds.Reserve(TankBoundsWorld.Num());
		for (const FVector2D& Point : TankBoundsWorld)
		{
			LocalTankBounds.Add(WorldToMapLocal(Point));
		}
		DrawClosedPolyline(
			OutDrawElements, MaxLayer + 4, PanelGeometry, LocalTankBounds,
			FLinearColor(0.55f, 0.72f, 0.88f, 0.85f), 1.5f);
	}



	if (IHInvisibleHandSpec::IsMinimapSeaDepthBandFillsEnabled())
	{
		for (const UIH_P1C08_MinimapSubsystem::FSeaRootsLayerEntry& Layer : Subsystem->GetSeaRootsLayers())
		{
			const FIHSeaRootsExtent& Extent = Layer.Extent;
			if (!HasValidSeaRootsExtentForPresentation(Extent))
			{
				continue;
			}

			auto BuildMapLocalRingFromRadii = [&](const TArray<float>& RadiiCm, TArray<FVector2D>& OutLocal) {
				TArray<FVector2D> WorldRing;
				IH_P1C08_MinimapCoastline::BuildWorldPolygonFromAzimuthRadiiCm(
					Layer.CenterWorldCm, Layer.YawDegrees, Extent.AzimuthOriginLocalCm, RadiiCm, WorldRing);
				OutLocal.Reset();
				OutLocal.Reserve(WorldRing.Num());
				for (const FVector2D& Point : WorldRing)
				{
					OutLocal.Add(WorldToMapLocal(Point));
				}
			};

			auto BuildMapLocalRingFromPolyline = [&](const TArray<FVector2D>& LocalPolylineCm, TArray<FVector2D>& OutLocal) {
				TArray<FVector2D> WorldRing;
				IH_P1C08_MinimapCoastline::BuildWorldPolygonFromLocalPolylineCm(
					Layer.CenterWorldCm, Layer.YawDegrees, LocalPolylineCm, WorldRing);
				OutLocal.Reset();
				OutLocal.Reserve(WorldRing.Num());
				for (const FVector2D& Point : WorldRing)
				{
					OutLocal.Add(WorldToMapLocal(Point));
				}
			};

			TArray<FVector2D> CoastLocal;
			TArray<FVector2D> TanLocal;
			TArray<FVector2D> CyanLocal;
			TArray<FVector2D> DeepLocal;
			const int32 SampleCount = Extent.CoastSamplesLocalCm.Num();
			const bool bUseSmoothedAzimuthRings =
				Extent.CoastRadiiCm.Num() >= 8
				&& Extent.CoastRadiiCm.Num() == Extent.TanOuterRadiiCm.Num()
				&& Extent.CyanOuterRadiiCm.Num() == Extent.CoastRadiiCm.Num()
				&& Extent.DeepOuterRadiiCm.Num() == Extent.CoastRadiiCm.Num();
			const bool bUseBakedRingPolylines =
				!bUseSmoothedAzimuthRings
				&& SampleCount >= 8
				&& Extent.TanOuterSamplesLocalCm.Num() == SampleCount
				&& Extent.CyanOuterSamplesLocalCm.Num() == SampleCount
				&& Extent.DeepOuterSamplesLocalCm.Num() == SampleCount;
			if (bUseSmoothedAzimuthRings)
			{
				BuildMapLocalRingFromRadii(Extent.CoastRadiiCm, CoastLocal);
				BuildMapLocalRingFromRadii(Extent.TanOuterRadiiCm, TanLocal);
				BuildMapLocalRingFromRadii(Extent.CyanOuterRadiiCm, CyanLocal);
				BuildMapLocalRingFromRadii(Extent.DeepOuterRadiiCm, DeepLocal);
			}
			else if (bUseBakedRingPolylines)
			{
				BuildMapLocalRingFromPolyline(Extent.CoastSamplesLocalCm, CoastLocal);
				BuildMapLocalRingFromPolyline(Extent.TanOuterSamplesLocalCm, TanLocal);
				BuildMapLocalRingFromPolyline(Extent.CyanOuterSamplesLocalCm, CyanLocal);
				BuildMapLocalRingFromPolyline(Extent.DeepOuterSamplesLocalCm, DeepLocal);
			}
			else
			{
				BuildMapLocalRingFromRadii(Extent.CoastRadiiCm, CoastLocal);
				BuildMapLocalRingFromRadii(Extent.TanOuterRadiiCm, TanLocal);
				BuildMapLocalRingFromRadii(Extent.CyanOuterRadiiCm, CyanLocal);
				BuildMapLocalRingFromRadii(Extent.DeepOuterRadiiCm, DeepLocal);
			}

			TArray<FVector2D> TanStrokeLocal;
			TArray<FVector2D> CyanStrokeLocal;
			TArray<FVector2D> DeepStrokeLocal;
			IH_P1C08_MinimapCoastline::PrepareSeaRootsBandRingForMinimapDraw(TanLocal, TanStrokeLocal);
			IH_P1C08_MinimapCoastline::PrepareSeaRootsBandRingForMinimapDraw(CyanLocal, CyanStrokeLocal);
			IH_P1C08_MinimapCoastline::PrepareSeaRootsBandRingForMinimapDraw(DeepLocal, DeepStrokeLocal);

			const FLinearColor TanFill = IHInvisibleHandSpec::MinimapBandTanFillColor;
			const FLinearColor CyanFill = IHInvisibleHandSpec::MinimapBandCyanFillColor;
			const FLinearColor DeepFill = IHInvisibleHandSpec::MinimapBandDeepFillColor;

			const int32 BandLayerDeep = MaxLayer + 4;
			const int32 BandLayerCyan = MaxLayer + 5;
			const int32 BandLayerTan = MaxLayer + 6;

			// Outermost first; tan (shallowest) last so it wins at concave self-overlaps.
			DrawFilledAnnulus(
				OutDrawElements, BandLayerDeep, PanelGeometry, CyanLocal, DeepLocal, DeepFill, true);
			DrawFilledAnnulus(
				OutDrawElements, BandLayerCyan, PanelGeometry, TanLocal, CyanLocal, CyanFill, true);
			DrawFilledAnnulus(
				OutDrawElements, BandLayerTan, PanelGeometry, CoastLocal, TanLocal, TanFill, true);

			const int32 BandStrokeLayer = MaxLayer + 8;
			const float BandStrokeThickness = FMath::Max(
				1.f,
				IH_P1C08_Minimap::ResolveCoastOutlineThicknessPx(
					Subsystem->GetZoomFactor(), Subsystem->GetRealmHalfExtentEWCm(), Subsystem->GetRealmHalfExtentNSCm())
					* 0.55f);
			DrawClosedPolyline(
				OutDrawElements, BandStrokeLayer, PanelGeometry, TanStrokeLocal,
				IHInvisibleHandSpec::GetPieBandDisplayColor(0), BandStrokeThickness, true);
			DrawClosedPolyline(
				OutDrawElements, BandStrokeLayer, PanelGeometry, CyanStrokeLocal,
				IHInvisibleHandSpec::GetPieBandDisplayColor(1), BandStrokeThickness, true);
			DrawClosedPolyline(
				OutDrawElements, BandStrokeLayer, PanelGeometry, DeepStrokeLocal,
				IHInvisibleHandSpec::GetPieBandDisplayColor(2), BandStrokeThickness, true);
		}
	}

	const bool bLogCoastlineRingsThisFrame =
		LastLoggedCoastlineRingCount != Subsystem->GetCoastlinePolylinesWorld().Num();
	if (bLogCoastlineRingsThisFrame)
	{
		LastLoggedCoastlineRingCount = Subsystem->GetCoastlinePolylinesWorld().Num();
		UE_LOG(LogTemp, Warning, TEXT("Minimap: coastline ring count=%d RealmHalfExtentEWCm=%.1f RealmHalfExtentNSCm=%.1f"),
			LastLoggedCoastlineRingCount, Subsystem->GetRealmHalfExtentEWCm(), Subsystem->GetRealmHalfExtentNSCm());
		if (Subsystem->GetCoastlinePolylinesWorld().Num() > 0)
		{
			const TArray<FVector2D>& FirstRing = Subsystem->GetCoastlinePolylinesWorld()[0];
			FVector2D MinW(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
			FVector2D MaxW(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());
			for (const FVector2D& P : FirstRing)
			{
				MinW.X = FMath::Min(MinW.X, P.X); MinW.Y = FMath::Min(MinW.Y, P.Y);
				MaxW.X = FMath::Max(MaxW.X, P.X); MaxW.Y = FMath::Max(MaxW.Y, P.Y);
			}
			UE_LOG(LogTemp, Warning,
				TEXT("Minimap: ring[0] RAW WORLD bounds Min=(%.1f,%.1f) Max=(%.1f,%.1f) SpanCm=(%.1f,%.1f)"),
				MinW.X, MinW.Y, MaxW.X, MaxW.Y, MaxW.X - MinW.X, MaxW.Y - MinW.Y);
		}
	}

	int32 CoastlineRingIdx = -1;
	for (const TArray<FVector2D>& Coastline : Subsystem->GetCoastlinePolylinesWorld())

	{
		++CoastlineRingIdx;

		TArray<FVector2D> LocalCoastline;

		LocalCoastline.Reserve(Coastline.Num());

		for (const FVector2D& Point : Coastline)

		{

			LocalCoastline.Add(WorldToMapLocal(Point));

		}

		const bool bIsInlandSea = Subsystem->GetCoastlineIsInlandSeaWorld().IsValidIndex(CoastlineRingIdx)
			&& Subsystem->GetCoastlineIsInlandSeaWorld()[CoastlineRingIdx];

		// Canonical: ALL rings (main coastline, islets, inland seas) receive the EXACT same
		// stroke thickness at a given zoom, with no per-type or per-ring modifier of any kind.
		// The zoom-to-px scale is the only input - a property of the current VIEW, never of the
		// individual feature being drawn - so every future SSOT-derived 2D overlay (Jurisdiction,
		// Contour, Trade Route, Zoning, Military, etc.) can reuse this same function unmodified
		// and get one parsimonious, feature-independent stroke rule.
		const float ZoomFactor = Subsystem->GetZoomFactor();
		const float CoastThickness = IH_P1C08_Minimap::ResolveCoastOutlineThicknessPx(
			ZoomFactor, Subsystem->GetRealmHalfExtentEWCm(), Subsystem->GetRealmHalfExtentNSCm());
		TArray<FVector2D> StrokeCoastline;
		IH_P1C08_MinimapCoastline::PrepareCoastlineForMinimapDraw(LocalCoastline, ZoomFactor, StrokeCoastline);

		// Shoelace area in RAW WORLD cm^2 (from Coastline, before WorldToMapLocal projection) -
		// NOT screen px^2. A screen-space threshold is zoom-dependent: at a zoomed-out full-realm
		// view every island's on-screen area is naturally tiny (52x33km compressed into one small
		// panel), so a fixed px^2 cutoff filtered out real islands, not just degenerate slivers -
		// confirmed by a user cross-check placing ships at gold-arrow coastline points and
		// matching them against the minimap's own ship markers. World-space area stays constant
		// regardless of zoom, so it correctly separates true noise (near-zero real area) from
		// real land (real area, however small it currently LOOKS on screen).
		double ShoelaceSumWorldCm2 = 0.0;
		for (int32 PointIdx = 0; PointIdx < Coastline.Num(); ++PointIdx)
		{
			const FVector2D& A = Coastline[PointIdx];
			const FVector2D& B = Coastline[(PointIdx + 1) % Coastline.Num()];
			ShoelaceSumWorldCm2 += (static_cast<double>(A.X) * B.Y) - (static_cast<double>(B.X) * A.Y);
		}
		const double RingAreaWorldCm2 = FMath::Abs(ShoelaceSumWorldCm2) * 0.5;
		const bool bSkipRing = RingAreaWorldCm2 < IH_P1C08_Minimap::CoastMinimapMinRingFillAreaWorldCm2;

		if (bLogCoastlineRingsThisFrame)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Minimap: ring[%d] rawVerts=%d strokeVerts=%d areaWorldM2=%.2f %s"),
				CoastlineRingIdx, Coastline.Num(), StrokeCoastline.Num(), RingAreaWorldCm2 / 10000.0,
				bSkipRing ? TEXT("SKIPPED(too small)") : TEXT("drawn"));
		}

		if (bSkipRing)
		{
			continue;
		}

		if (!bIsInlandSea)
		{
			// Inland-sea holes stay unfilled - the panel's own ocean-blue background already
			// reads correctly as water; painting land-fill color over it would hide that.
			DrawFilledPolygon(
				OutDrawElements, MaxLayer + 6, PanelGeometry, StrokeCoastline,
				IHInvisibleHandSpec::MinimapLandFillColor);
		}
		DrawClosedPolyline(
			OutDrawElements, MaxLayer + 7, PanelGeometry, StrokeCoastline,
			bIsInlandSea ? IHInvisibleHandSpec::MinimapInlandSeaStrokeColor : IHInvisibleHandSpec::CoastStrokeColor,
			CoastThickness, true);

	}

	int32 LaneIslandA = INDEX_NONE;
	int32 LaneIslandB = INDEX_NONE;
	if (PC->TryGetLaneViolationPair(LaneIslandA, LaneIslandB))
	{
		const AIH_WB_Demo004GameMode* GM = PC->GetWorld()
			? PC->GetWorld()->GetAuthGameMode<AIH_WB_Demo004GameMode>()
			: nullptr;
		const AIH_WB_IslandActor* IslandA = GM ? GM->GetSpawnedIsland(LaneIslandA) : nullptr;
		const AIH_WB_IslandActor* IslandB = GM ? GM->GetSpawnedIsland(LaneIslandB) : nullptr;
		if (IslandA && IslandB)
		{
			const FVector2D CenterA(IslandA->GetActorLocation().X, IslandA->GetActorLocation().Y);
			const FVector2D CenterB(IslandB->GetActorLocation().X, IslandB->GetActorLocation().Y);
			const TArray<FVector2D> LaneBand = { WorldToMapLocal(CenterA), WorldToMapLocal(CenterB) };
			const float Pulse = 0.55f + 0.35f * FMath::Sin(PC->GetWorld()->GetTimeSeconds() * 8.f);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				MaxLayer + 4,
				PanelGeometry.ToPaintGeometry(),
				LaneBand,
				ESlateDrawEffect::None,
				FLinearColor(1.f, 0.12f, 0.12f, Pulse),
				true,
				3.f);
		}
	}

	if (UGameInstance* GI = PC->GetGameInstance())

	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			if (Nav->HasSelectedIsland() && PC->ShouldShowIslandSelectionVisual())
			{
				const AIH_WB_Demo004GameMode* GM = PC->GetWorld()
					? PC->GetWorld()->GetAuthGameMode<AIH_WB_Demo004GameMode>()
					: nullptr;
				const AIH_WB_IslandActor* SelectedIsland = GM
					? GM->GetSpawnedIsland(Nav->GetSelectedIslandIndex())
					: nullptr;
				if (SelectedIsland && !IHInvisibleHandSpec::IsIslandMeshSelectionGlowEnabled())
				{
					TArray<FVector2D> ShorelineWorld;
					const int32 SelectedIndex = Nav->GetSelectedIslandIndex();
					if (!Subsystem->TryGetCoastlineWorldXYForIsland(SelectedIndex, ShorelineWorld))
					{
						SelectedIsland->GetShorelinePolygonWorldCm(ShorelineWorld);
					}
					if (ShorelineWorld.Num() >= 3)
					{
						TArray<FVector2D> LocalSelection;
						LocalSelection.Reserve(ShorelineWorld.Num());
						for (const FVector2D& Point : ShorelineWorld)
						{
							LocalSelection.Add(WorldToMapLocal(Point));
						}
						const float ZoomFactor = Subsystem->GetZoomFactor();
						const float CoastThickness = IH_P1C08_Minimap::ResolveCoastOutlineThicknessPx(
							ZoomFactor, Subsystem->GetRealmHalfExtentEWCm(), Subsystem->GetRealmHalfExtentNSCm());
						TArray<FVector2D> StrokeSelection;
						IH_P1C08_MinimapCoastline::PrepareCoastlineForMinimapDraw(LocalSelection, ZoomFactor, StrokeSelection);
						DrawClosedPolyline(
							OutDrawElements,
							MaxLayer + 5,
							PanelGeometry,
							StrokeSelection,
							FLinearColor(1.f, 0.55f, 0.f, 1.f),
							CoastThickness + 0.25f,
							true);
					}
				}
			}
		}

		if (UIH_P1C07_ShipRegistrySubsystem* Registry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())

		{

			for (const TWeakObjectPtr<AActor>& ShipPtr : Registry->GetRegisteredShips())

			{

				AActor* Ship = ShipPtr.Get();

				if (!Ship)

				{

					continue;

				}



				const FVector2D ShipLocal = WorldToMapLocal(FVector2D(Ship->GetActorLocation().X, Ship->GetActorLocation().Y));

				DrawShipGlyph(
					OutDrawElements, MaxLayer + 5, PanelGeometry, ShipLocal,
					ResolveShipMapGlyphDegrees(Ship),
					Registry->IsShipSelected(Ship));

			}

			const FLinearColor BuoyDotColor(1.f, 0.55f, 0.1f, 1.f);
			for (AIH_P1C07_MoveDestinationBuoy* Buoy : Registry->GetActiveFleetBuoys())
			{
				if (!Buoy)
				{
					continue;
				}

				const FVector BuoyLoc = Buoy->GetActorLocation();
				const FVector2D BuoyLocal = WorldToMapLocal(FVector2D(BuoyLoc.X, BuoyLoc.Y));
				DrawMapDot(OutDrawElements, MaxLayer + 5, PanelGeometry, BuoyLocal, 4.5f, BuoyDotColor);
			}

		}

		if (IHInvisibleHandSpec::IsCoastC2bDevPOIMarkersEnabled())
		{
			for (const FIHGate2bDevPOIMarker& Marker : Subsystem->GetGate2bDevPOIMarkers())
			{
				const FVector2D MarkerLocal = WorldToMapLocal(Marker.WorldXY);
				const FLinearColor MarkerColor = GetGate2bDevPOIMarkerDisplayColor(Marker.Kind);

				DrawMapDot(OutDrawElements, MaxLayer + 6, PanelGeometry, MarkerLocal, 4.5f, MarkerColor);
				DrawMapPlus(OutDrawElements, MaxLayer + 6, PanelGeometry, MarkerLocal, 8.f, 2.5f, MarkerColor);
			}
		}

	}

	if (const UGameInstance* GI = PC->GetGameInstance())
	{
		if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
		{
			if (BuildPalette->IsDragActive()
				&& BuildPalette->GetDragPayload().paletteTab == EIHBuildPaletteTab::Build)
			{
				FVector2D MarkerWorldXY = FVector2D::ZeroVector;
				bool bHaveMarker = false;
				if (BuildPalette->HasValidDragGhostLocation())
				{
					const FVector GhostLoc = BuildPalette->GetDragGhostWorldLocation();
					MarkerWorldXY = FVector2D(GhostLoc.X, GhostLoc.Y);
					bHaveMarker = true;
				}
				else if (FSlateApplication::IsInitialized())
				{
					FVector2D PickWorldXY = FVector2D::ZeroVector;
					if (TryGetWorldXYFromScreen(FSlateApplication::Get().GetCursorPos(), PickWorldXY))
					{
						MarkerWorldXY = PickWorldXY;
						bHaveMarker = true;
					}
				}

				if (bHaveMarker)
				{
					const FVector2D CenterLocal = WorldToMapLocal(MarkerWorldXY);
					FVector FootprintCm(800.f, 600.f, 400.f);
					BuildPalette->GetActiveDragFootprintCm(FootprintCm);

					const FVector2D MapScale(
						(MapSize.X * 0.5f) / FMath::Max(View.HalfExtentWorld.X, 1.f),
						(MapSize.Y * 0.5f) / FMath::Max(View.HalfExtentWorld.Y, 1.f));
					const float HalfW = FMath::Max(5.f, FootprintCm.X * 0.5f * MapScale.X);
					const float HalfH = FMath::Max(5.f, FootprintCm.Y * 0.5f * MapScale.Y);

					const FSlateRect FootprintRect(
						CenterLocal.X - HalfW,
						CenterLocal.Y - HalfH,
						CenterLocal.X + HalfW,
						CenterLocal.Y + HalfH);
					DrawSolidLocalRect(
						OutDrawElements,
						MaxLayer + 7,
						PanelGeometry,
						FootprintRect,
						FLinearColor(0.12f, 0.55f, 1.f, 0.5f));
					DrawClosedPolyline(
						OutDrawElements,
						MaxLayer + 8,
						PanelGeometry,
						{
							FVector2D(FootprintRect.Left, FootprintRect.Top),
							FVector2D(FootprintRect.Right, FootprintRect.Top),
							FVector2D(FootprintRect.Right, FootprintRect.Bottom),
							FVector2D(FootprintRect.Left, FootprintRect.Bottom),
						},
						FLinearColor(0.f, 0.78f, 1.f, 1.f),
						2.5f);
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (IHInvisibleHandSpec::IsStampGalleryMinimapMarkerEnabled())
	{
		float RealmHalfExtentNSKm = IHInvisibleHandSpec::DefaultRealmHalfExtentNSKm;
		if (const UGameInstance* GI = PC->GetGameInstance())
		{
			if (const UIH_WB_Demo004GameInstance* AzGI = Cast<UIH_WB_Demo004GameInstance>(GI))
			{
				RealmHalfExtentNSKm = AzGI->GetRealmHalfExtentNSKm();
			}
		}

		float GalleryMinX = 0.f;
		float GalleryMinY = 0.f;
		float GalleryMaxX = 0.f;
		float GalleryMaxY = 0.f;
		IHInvisibleHandSpec::GetStampGalleryWorldFootprintXYCm(
			RealmHalfExtentNSKm, GalleryMinX, GalleryMinY, GalleryMaxX, GalleryMaxY);

		const FVector2D GalleryCornerMin = WorldToMapLocal(FVector2D(GalleryMinX, GalleryMinY));
		const FVector2D GalleryCornerMax = WorldToMapLocal(FVector2D(GalleryMaxX, GalleryMaxY));
		const FSlateRect GalleryRect(
			FMath::Min(GalleryCornerMin.X, GalleryCornerMax.X),
			FMath::Min(GalleryCornerMin.Y, GalleryCornerMax.Y),
			FMath::Max(GalleryCornerMin.X, GalleryCornerMax.X),
			FMath::Max(GalleryCornerMin.Y, GalleryCornerMax.Y));

		static const FLinearColor StampGalleryMinimapFill(1.f, 0.f, 1.f, 0.5f);
		static const FLinearColor StampGalleryMinimapOutline(1.f, 0.15f, 1.f, 1.f);
		DrawSolidLocalRect(
			OutDrawElements,
			MaxLayer + 6,
			PanelGeometry,
			GalleryRect,
			StampGalleryMinimapFill);
		DrawClosedPolyline(
			OutDrawElements,
			MaxLayer + 7,
			PanelGeometry,
			{
				FVector2D(GalleryRect.Left, GalleryRect.Top),
				FVector2D(GalleryRect.Right, GalleryRect.Top),
				FVector2D(GalleryRect.Right, GalleryRect.Bottom),
				FVector2D(GalleryRect.Left, GalleryRect.Bottom),
			},
			StampGalleryMinimapOutline,
			2.f);
	}
#endif

	if (const APawn* ViewPawn = PC->GetPawn())

	{

		const FVector2D CameraLocal = WorldToMapLocal(FVector2D(ViewPawn->GetActorLocation().X, ViewPawn->GetActorLocation().Y));

		const FRotator CameraYaw(0.f, PC->GetControlRotation().Yaw, 0.f);
		const FVector CameraLookDir = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
		DrawCameraV(
			OutDrawElements, MaxLayer + 6, PanelGeometry, CameraLocal,
			IH_P1C08_Minimap::FView::WorldDirectionToMapGlyphDegrees(
				FVector2D(CameraLookDir.X, CameraLookDir.Y)));

	}

	OutDrawElements.PopClip();



	const FSlateRect CloseRect = GetCloseButtonRect(PanelGeometry);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer + 7,
		PanelGeometry.ToPaintGeometry(
			FVector2f(CloseRect.Right - CloseRect.Left, CloseRect.Bottom - CloseRect.Top),
			FSlateLayoutTransform(FVector2f(CloseRect.Left, CloseRect.Top))),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.55f, 0.12f, 0.1f, 0.95f));



	TArray<FVector2D> XGlyph = {

		FVector2D(CloseRect.Left + 3.f, CloseRect.Top + 3.f),

		FVector2D(CloseRect.Right - 3.f, CloseRect.Bottom - 3.f),

	};

	TArray<FVector2D> XGlyph2 = {

		FVector2D(CloseRect.Right - 3.f, CloseRect.Top + 3.f),

		FVector2D(CloseRect.Left + 3.f, CloseRect.Bottom - 3.f),

	};

	FSlateDrawElement::MakeLines(

		OutDrawElements, MaxLayer + 8, PanelGeometry.ToPaintGeometry(), XGlyph,

		ESlateDrawEffect::None, FLinearColor::White, true, 1.5f);

	FSlateDrawElement::MakeLines(

		OutDrawElements, MaxLayer + 8, PanelGeometry.ToPaintGeometry(), XGlyph2,

		ESlateDrawEffect::None, FLinearColor::White, true, 1.5f);



	return MaxLayer + 8;

}



FReply UIH_P1C08_MinimapWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	if (HandleScreenPointerDown(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply UIH_P1C08_MinimapWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		HandleScreenPointerUp(InMouseEvent.GetScreenSpacePosition());
		if (HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UIH_P1C08_MinimapWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	HandleScreenPointerMove(InMouseEvent.GetScreenSpacePosition());
	if (bDraggingPanel)
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}



FReply UIH_P1C08_MinimapWidget::NativeOnMouseWheel(

	const FGeometry& InGeometry,

	const FPointerEvent& InMouseEvent)

{

	if (UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get())

	{

		const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
		const FGeometry BaseGeometry = GetHitTestGeometry();

		if (!IsPointInPanel(BaseGeometry, ScreenPos))
		{
			return FReply::Unhandled();
		}

		const FGeometry PanelGeometry = MakePanelGeometry(BaseGeometry);
		const FVector2D Local = ScreenToPanelLocal(BaseGeometry, ScreenPos);

		if (GetMapContentRect(PanelGeometry).ContainsPoint(Local))

		{

			Subsystem->HandleMouseWheelZoom(InMouseEvent.GetWheelDelta(), ScreenPos);

			return FReply::Handled();

		}

	}

	return FReply::Unhandled();

}



void UIH_P1C08_MinimapWidget::NativeOnMouseEnter(

	const FGeometry& InGeometry,

	const FPointerEvent& InMouseEvent)

{

	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

}



void UIH_P1C08_MinimapWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)

{

	Super::NativeOnMouseLeave(InMouseEvent);

	if (UIH_P1C08_MinimapSubsystem* Subsystem = MinimapSubsystem.Get())

	{

		Subsystem->SetMouseOverMinimap(false);

	}

}

