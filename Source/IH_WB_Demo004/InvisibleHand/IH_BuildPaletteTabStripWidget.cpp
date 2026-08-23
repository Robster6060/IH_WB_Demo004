// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_BuildPaletteTabStripWidget.h"

#include "IH_BuildPaletteHostWidget.h"
#include "IH_BuildPalettePanelStyle.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IH_BuildPaletteSubsystem.h"
#include "IH_Cube2FlyPlayerController.h"
#include "IHUIColorSchemeLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Spacer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SViewport.h"

namespace BuildPaletteTabStripPrivate
{
	static constexpr int32 TabCount = 5;

	static bool TryAbsoluteToViewportLocal(
		const AIH_Cube2FlyPlayerController* PC,
		const FVector2D& AbsolutePos,
		FVector2D& OutViewportPos)
	{
		if (!PC)
		{
			return false;
		}

		const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
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

		OutViewportPos = ViewportWidget->GetTickSpaceGeometry().AbsoluteToLocal(AbsolutePos);
		return true;
	}

	static const TCHAR* TabKeyLetters[TabCount] = {
		TEXT("G"), TEXT("W"), TEXT("B"), TEXT("C"), TEXT("D"),
	};

	static EIHBuildPaletteTab TabIndexToEnum(int32 TabIndex)
	{
		switch (TabIndex)
		{
		case 0: return EIHBuildPaletteTab::Grid;
		case 1: return EIHBuildPaletteTab::World;
		case 2: return EIHBuildPaletteTab::Build;
		case 3: return EIHBuildPaletteTab::Convey;
		case 4: return EIHBuildPaletteTab::Defense;
		default: return EIHBuildPaletteTab::Grid;
		}
	}

	static int32 EnumToTabIndex(EIHBuildPaletteTab Tab)
	{
		switch (Tab)
		{
		case EIHBuildPaletteTab::Grid: return 0;
		case EIHBuildPaletteTab::World: return 1;
		case EIHBuildPaletteTab::Build: return 2;
		case EIHBuildPaletteTab::Convey: return 3;
		case EIHBuildPaletteTab::Defense: return 4;
		default: return INDEX_NONE;
		}
	}
}

UIH_BuildPaletteTabStripWidget::UIH_BuildPaletteTabStripWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedPaint = true;
}

void UIH_BuildPaletteTabStripWidget::EnsureMinimalWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	WidgetTree->RootWidget = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("TabStripSpacer"));
}

TSharedRef<SWidget> UIH_BuildPaletteTabStripWidget::RebuildWidget()
{
	EnsureMinimalWidgetTree();
	return Super::RebuildWidget();
}

void UIH_BuildPaletteTabStripWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureMinimalWidgetTree();
	// Paint-only overlay; PlayerController routes pointer hits via HandleScreenPointerDown.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FGeometry UIH_BuildPaletteTabStripWidget::GetBaseGeometry() const
{
	if (const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
			{
				if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())
				{
					return ViewportWidget->GetTickSpaceGeometry();
				}
			}
		}
	}

	if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		return Cached->GetTickSpaceGeometry();
	}

	return FGeometry();
}

FGeometry UIH_BuildPaletteTabStripWidget::ResolveBaseGeometry(const FGeometry& AllottedGeometry) const
{
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

FVector2D UIH_BuildPaletteTabStripWidget::ResolveOverlaySize(const FGeometry& Geometry) const
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

FVector2D UIH_BuildPaletteTabStripWidget::GetTabStripSize() const
{
	static constexpr float TabRowHeight = 18.f;
	const float StripH = static_cast<float>(BuildPaletteTabStripPrivate::TabCount) * TabRowHeight + 12.f;
	return FVector2D(IH_BuildPalettePanelStyle::TabStripWidth, StripH);
}

FVector2D UIH_BuildPaletteTabStripWidget::GetTabStripTopLeftLocal(const FGeometry& Geometry) const
{
	const FVector2D ViewSize = ResolveOverlaySize(Geometry);
	const FVector2D StripSize = GetTabStripSize();
	return FVector2D(
		FMath::Max(0.f, ViewSize.X - StripSize.X - IH_BuildPalettePanelStyle::RightHUDInset),
		IH_BuildPalettePanelStyle::TabStripTopMargin);
}

FGeometry UIH_BuildPaletteTabStripWidget::MakeOverlayGeometry(const FGeometry& AllottedGeometry) const
{
	const FGeometry BaseGeometry = ResolveBaseGeometry(AllottedGeometry);
	const FVector2D OverlaySize = ResolveOverlaySize(BaseGeometry);
	return BaseGeometry.MakeChild(
		FVector2f(OverlaySize),
		FSlateLayoutTransform(FVector2f::ZeroVector));
}

FGeometry UIH_BuildPaletteTabStripWidget::MakeTabStripGeometry(const FGeometry& AllottedGeometry) const
{
	const FVector2D StripOrigin = GetTabStripTopLeftLocal(MakeOverlayGeometry(AllottedGeometry));
	const FVector2D StripSize = GetTabStripSize();
	return MakeOverlayGeometry(AllottedGeometry).MakeChild(
		FVector2f(StripSize),
		FSlateLayoutTransform(FVector2f(StripOrigin)));
}

FSlateRect UIH_BuildPaletteTabStripWidget::GetTabStripScreenRect(const FGeometry& Geometry) const
{
	const FGeometry StripGeometry = MakeTabStripGeometry(Geometry);
	const FVector2D StripSize = StripGeometry.GetLocalSize();
	const FVector2D TopLeft = StripGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = StripGeometry.LocalToAbsolute(StripSize);
	return FSlateRect(TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
}

FSlateRect UIH_BuildPaletteTabStripWidget::GetTabStripViewportRect(const FGeometry& Geometry) const
{
	const FVector2D StripSize = GetTabStripSize();
	const FVector2D TopLeft = GetTabStripTopLeftLocal(MakeOverlayGeometry(Geometry));
	return FSlateRect(
		TopLeft.X,
		TopLeft.Y,
		TopLeft.X + StripSize.X,
		TopLeft.Y + StripSize.Y);
}

FSlateRect UIH_BuildPaletteTabStripWidget::GetTabRowRect(const FGeometry& TabStripGeometry, int32 TabIndex) const
{
	static constexpr float TabRowHeight = 18.f;
	const FVector2D StripSize = TabStripGeometry.GetLocalSize();
	const float RowTop = 6.f + static_cast<float>(TabIndex) * TabRowHeight;
	return FSlateRect(
		0.f,
		RowTop,
		StripSize.X,
		FMath::Min(StripSize.Y, RowTop + TabRowHeight));
}

bool UIH_BuildPaletteTabStripWidget::IsPointInTabStrip(const FGeometry& Geometry, const FVector2D& ScreenPos) const
{
	return GetTabStripScreenRect(Geometry).ContainsPoint(ScreenPos);
}

void UIH_BuildPaletteTabStripWidget::DrawSolidLocalRect(
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

void UIH_BuildPaletteTabStripWidget::InitializeTabStrip(
	UIH_BuildPaletteSubsystem* InSubsystem,
	AIH_Cube2FlyPlayerController* InPC)
{
	BuildPaletteSubsystem = InSubsystem;
	OwnerPC = InPC;
	bLoggedPaintGeometryOnce = false;
	RequestRepaint();
}

void UIH_BuildPaletteTabStripWidget::RequestRepaint()
{
	if (TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
	{
		SlateWidget->Invalidate(EInvalidateWidget::Paint);
	}
}

void UIH_BuildPaletteTabStripWidget::InvalidateHitCache()
{
	bHasCachedTabStripScreenRect = false;
	bHasCachedTabStripViewportRect = false;
}

FGeometry UIH_BuildPaletteTabStripWidget::GetHitTestGeometry() const
{
	const FGeometry ViewportGeometry = GetBaseGeometry();
	if (ViewportGeometry.GetLocalSize().X >= 1.f && ViewportGeometry.GetLocalSize().Y >= 1.f)
	{
		return ViewportGeometry;
	}

	if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		return ResolveBaseGeometry(Cached->GetTickSpaceGeometry());
	}

	return ViewportGeometry;
}

bool UIH_BuildPaletteTabStripWidget::TryGetTabStripLeftViewportX(float& OutLeftViewportX) const
{
	FSlateRect ViewportRect;
	if (!TryGetTabStripViewportRect(ViewportRect))
	{
		return false;
	}

	OutLeftViewportX = ViewportRect.Left;
	return OutLeftViewportX >= 0.f;
}

bool UIH_BuildPaletteTabStripWidget::TryGetTabStripViewportRect(FSlateRect& OutRect) const
{
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}

	if (bHasCachedTabStripViewportRect)
	{
		OutRect = CachedTabStripViewportRect;
		return (OutRect.Right - OutRect.Left) >= 1.f && (OutRect.Bottom - OutRect.Top) >= 1.f;
	}

	const FGeometry Base = GetHitTestGeometry();
	const FVector2D ViewSize = ResolveOverlaySize(Base);
	if (ViewSize.X < 1.f || ViewSize.Y < 1.f)
	{
		return false;
	}

	OutRect = GetTabStripViewportRect(Base);
	return (OutRect.Right - OutRect.Left) >= 1.f && (OutRect.Bottom - OutRect.Top) >= 1.f;
}

bool UIH_BuildPaletteTabStripWidget::TryGetTabStripScreenRect(FSlateRect& OutRect) const
{
	FSlateRect ViewportRect;
	if (!TryGetTabStripViewportRect(ViewportRect))
	{
		return false;
	}

	const FGeometry Base = GetHitTestGeometry();
	const FVector2D ViewportAbs = Base.GetAbsolutePosition();
	OutRect = FSlateRect(
		ViewportAbs.X + ViewportRect.Left,
		ViewportAbs.Y + ViewportRect.Top,
		ViewportAbs.X + ViewportRect.Right,
		ViewportAbs.Y + ViewportRect.Bottom);
	return true;
}

bool UIH_BuildPaletteTabStripWidget::IsViewportPointOverTabStrip(const FVector2D& ViewportLocal) const
{
	FSlateRect StripRect;
	if (!TryGetTabStripViewportRect(StripRect))
	{
		return false;
	}

	return StripRect.ContainsPoint(ViewportLocal);
}

bool UIH_BuildPaletteTabStripWidget::IsScreenPointOverTabStrip(const FVector2D& ScreenAbsolute) const
{
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}

	if (const AIH_Cube2FlyPlayerController* PC = OwnerPC.Get())
	{
		FVector2D ViewportLocal = ScreenAbsolute;
		if (BuildPaletteTabStripPrivate::TryAbsoluteToViewportLocal(PC, ScreenAbsolute, ViewportLocal))
		{
			return IsViewportPointOverTabStrip(ViewportLocal);
		}
	}

	FSlateRect StripRect;
	if (TryGetTabStripScreenRect(StripRect))
	{
		return StripRect.ContainsPoint(ScreenAbsolute);
	}

	return GetTabStripScreenRect(GetHitTestGeometry()).ContainsPoint(ScreenAbsolute);
}

int32 UIH_BuildPaletteTabStripWidget::HitTestTabIndexAtViewport(const FVector2D& ViewportLocal) const
{
	FSlateRect StripRect;
	if (!TryGetTabStripViewportRect(StripRect))
	{
		return INDEX_NONE;
	}

	if (!StripRect.ContainsPoint(ViewportLocal))
	{
		return INDEX_NONE;
	}

	const float LocalY = ViewportLocal.Y - StripRect.Top;
	const float StripH = StripRect.Bottom - StripRect.Top;
	static constexpr float TabRowHeight = 18.f;
	static constexpr float TabTopPadding = 6.f;
	if (LocalY < TabTopPadding || LocalY > StripH - TabTopPadding)
	{
		return INDEX_NONE;
	}

	const int32 TabIndex = FMath::Clamp(
		FMath::FloorToInt((LocalY - TabTopPadding) / TabRowHeight),
		0,
		BuildPaletteTabStripPrivate::TabCount - 1);
	const float RowTop = TabTopPadding + static_cast<float>(TabIndex) * TabRowHeight;
	const float RowBottom = RowTop + TabRowHeight;
	if (LocalY >= RowTop && LocalY < RowBottom)
	{
		return TabIndex;
	}

	return INDEX_NONE;
}

int32 UIH_BuildPaletteTabStripWidget::HitTestTabIndex(const FVector2D& ScreenAbsolute) const
{
	FSlateRect StripRect;
	if (bHasCachedTabStripScreenRect)
	{
		StripRect = CachedTabStripScreenRect;
	}
	else if (!TryGetTabStripScreenRect(StripRect))
	{
		return INDEX_NONE;
	}

	if (!StripRect.ContainsPoint(ScreenAbsolute))
	{
		return INDEX_NONE;
	}

	const float LocalY = ScreenAbsolute.Y - StripRect.Top;
	const float StripH = StripRect.Bottom - StripRect.Top;
	static constexpr float TabRowHeight = 18.f;
	static constexpr float TabTopPadding = 6.f;
	if (LocalY < TabTopPadding || LocalY > StripH - TabTopPadding)
	{
		return INDEX_NONE;
	}

	const int32 TabIndex = FMath::Clamp(
		FMath::FloorToInt((LocalY - TabTopPadding) / TabRowHeight),
		0,
		BuildPaletteTabStripPrivate::TabCount - 1);
	const float RowTop = TabTopPadding + static_cast<float>(TabIndex) * TabRowHeight;
	const float RowBottom = RowTop + TabRowHeight;
	if (LocalY >= RowTop && LocalY < RowBottom)
	{
		return TabIndex;
	}

	return INDEX_NONE;
}

bool UIH_BuildPaletteTabStripWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	const int32 TabIndex = HitTestTabIndex(ScreenAbsolute);
	if (TabIndex == INDEX_NONE)
	{
		return false;
	}

	UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get();
	AIH_Cube2FlyPlayerController* PC = OwnerPC.Get();
	if (!Subsystem && OwnerPC.IsValid())
	{
		if (UGameInstance* GI = OwnerPC->GetGameInstance())
		{
			Subsystem = GI->GetSubsystem<UIH_BuildPaletteSubsystem>();
		}
	}
	if (!PC && Subsystem)
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			PC = Cast<AIH_Cube2FlyPlayerController>(World->GetFirstPlayerController());
		}
	}
	if (!Subsystem || !PC)
	{
		return false;
	}

	const EIHBuildPaletteTab Tab = BuildPaletteTabStripPrivate::TabIndexToEnum(TabIndex);
	Subsystem->ToggleTabFlyOut(Tab, PC);
	if (UIH_BuildPaletteHostWidget* Host = Subsystem->GetBuildPaletteWidget())
	{
		Host->RequestLayoutRefresh();
	}
	RequestRepaint();
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("BuildPaletteTabStrip: click tab=%s flyOutOpen=%d"),
		BuildPaletteTabStripPrivate::TabKeyLetters[TabIndex],
		Subsystem->IsFlyOutOpen() ? 1 : 0);
	return true;
}

int32 UIH_BuildPaletteTabStripWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return MaxLayer;
	}

	const FGeometry BaseGeometry = ResolveBaseGeometry(AllottedGeometry);
	const FVector2D OverlaySize = ResolveOverlaySize(BaseGeometry);
	const FGeometry StripGeometry = MakeTabStripGeometry(BaseGeometry);
	const FVector2D StripSize = GetTabStripSize();
	const FVector2D StripOrigin = GetTabStripTopLeftLocal(MakeOverlayGeometry(BaseGeometry));
	const FVector2D StripAbs = StripGeometry.GetAbsolutePosition();

	if (StripSize.X >= 1.f && StripSize.Y >= 1.f)
	{
		const FVector2D ViewportAbs = BaseGeometry.GetAbsolutePosition();
		CachedTabStripScreenRect = FSlateRect(
			StripAbs.X,
			StripAbs.Y,
			StripAbs.X + StripSize.X,
			StripAbs.Y + StripSize.Y);
		CachedTabStripViewportRect = FSlateRect(
			StripAbs.X - ViewportAbs.X,
			StripAbs.Y - ViewportAbs.Y,
			StripAbs.X - ViewportAbs.X + StripSize.X,
			StripAbs.Y - ViewportAbs.Y + StripSize.Y);
		bHasCachedTabStripScreenRect = true;
		bHasCachedTabStripViewportRect = true;

		if (!bLoggedPaintGeometryOnce)
		{
			bLoggedPaintGeometryOnce = true;
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("BuildPaletteTabStrip: VISIBLE at abs=(%.0f,%.0f) size=(%.0f,%.0f) viewport=(%.0f,%.0f)"),
				StripAbs.X,
				StripAbs.Y,
				StripSize.X,
				StripSize.Y,
				OverlaySize.X,
				OverlaySize.Y);
		}
	}

	if (StripSize.X < 1.f || StripSize.Y < 1.f)
	{
		return MaxLayer;
	}

	const UIH_BuildPaletteSubsystem* Subsystem = BuildPaletteSubsystem.Get();
	const int32 ActiveTabIndex = (Subsystem && Subsystem->IsFlyOutOpen())
		? BuildPaletteTabStripPrivate::EnumToTabIndex(Subsystem->GetActiveTab())
		: INDEX_NONE;

	const FLinearColor PanelBackground = UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(
		FName(TEXT("PanelBackground")), IH_P1C08_DevPanelStyle::PanelBackgroundAlpha * 0.85f);
	DrawSolidLocalRect(
		OutDrawElements,
		MaxLayer + 1,
		StripGeometry,
		FSlateRect(0.f, 0.f, StripSize.X, StripSize.Y),
		PanelBackground);

	const FSlateFontInfo TabFont = FCoreStyle::GetDefaultFontStyle(
		"Bold", IH_P1C08_DevPanelStyle::CompactLabelFontSize);
	const FLinearColor DefaultText = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")));

	for (int32 TabIndex = 0; TabIndex < BuildPaletteTabStripPrivate::TabCount; ++TabIndex)
	{
		const FSlateRect RowRect = GetTabRowRect(StripGeometry, TabIndex);
		const bool bActive = TabIndex == ActiveTabIndex;
		const FLinearColor TextColor = bActive
			? IH_BuildPalettePanelStyle::FocusBlue
			: DefaultText;

		FSlateDrawElement::MakeText(
			OutDrawElements,
			MaxLayer + 2,
			StripGeometry.ToPaintGeometry(
				FVector2f(RowRect.Right - RowRect.Left, RowRect.Bottom - RowRect.Top),
				FSlateLayoutTransform(FVector2f(RowRect.Left + 10.f, RowRect.Top + 1.f))),
			BuildPaletteTabStripPrivate::TabKeyLetters[TabIndex],
			TabFont,
			ESlateDrawEffect::None,
			TextColor);
	}

	(void)StripOrigin;
	return MaxLayer + 2;
}

FReply UIH_BuildPaletteTabStripWidget::NativeOnMouseButtonDown(
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
