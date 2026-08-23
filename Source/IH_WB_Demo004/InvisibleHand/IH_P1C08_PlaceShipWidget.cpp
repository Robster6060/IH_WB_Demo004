// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_PlaceShipWidget.h"

#include "IH_BuildPalettePanelStyle.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UIH_P1C08_PlaceShipWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlaceShipRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlaceShipPanel"));
	PanelBorder->SetPadding(FMargin(10.f, 10.f));
	{
		FSlateBrush PanelBrush;
		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PanelBrush.TintColor = FSlateColor(
			UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName(TEXT("PanelBackground")), 0.82f));
		PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PanelBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
		PanelBrush.OutlineSettings.Color =
			UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));
		PanelBrush.OutlineSettings.Width = 1.f;
		PanelBorder->SetBrush(PanelBrush);
	}

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlaceShipVBox"));
	LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlaceShipLabel"));
	LabelText->SetText(FText::FromString(TEXT("Place Ship")));
	LabelText->SetColorAndOpacity(
		FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")))));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(LabelText);
	VB->AddChildToVerticalBox(LabelText);
	PanelBorder->AddChild(VB);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		// Immediate left of ASL in Place Ship | ASL | Game Speed | DEV View.
		const float PlaceShipRightX =
			-(IH_BuildPalettePanelStyle::TopRightHudClusterRightClearPx
				+ IH_BuildPalettePanelStyle::TopRightHudDevViewW
				+ IH_BuildPalettePanelStyle::TopRightHudClusterGapPx
				+ IH_BuildPalettePanelStyle::TopRightHudGameSpeedApproxW
				+ IH_BuildPalettePanelStyle::TopRightHudClusterGapPx
				+ IH_BuildPalettePanelStyle::TopRightHudAslApproxW
				+ IH_BuildPalettePanelStyle::TopRightHudClusterGapPx);
		CSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		CSlot->SetAlignment(FVector2D(1.f, 0.f));
		CSlot->SetPosition(FVector2D(PlaceShipRightX, IH_BuildPalettePanelStyle::TopRightHudClusterTopY));
		CSlot->SetAutoSize(true);
	}
}

TSharedRef<SWidget> UIH_P1C08_PlaceShipWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_PlaceShipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(1.f, 0.f)); // Plan Addendum 19
	RefreshVisual();
}

void UIH_P1C08_PlaceShipWidget::RefreshVisual()
{
	if (!PanelBorder || !LabelText) return;
	FSlateBrush PanelBrush;
	PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PanelBrush.TintColor = FSlateColor(
		UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName(TEXT("PanelBackground")), 0.82f));
	PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	PanelBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
	if (bPlaceModeActive)
	{
		PanelBrush.OutlineSettings.Color = FLinearColor(0.95f, 0.75f, 0.2f, 1.f);
		PanelBrush.OutlineSettings.Width = 2.f;
		LabelText->SetText(FText::FromString(TEXT("Click Water")));
	}
	else
	{
		PanelBrush.OutlineSettings.Color =
			UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));
		PanelBrush.OutlineSettings.Width = 1.f;
		LabelText->SetText(FText::FromString(TEXT("Place Ship")));
	}
	PanelBorder->SetBrush(PanelBrush);
}

void UIH_P1C08_PlaceShipWidget::SetPlaceModeActive(const bool bActive)
{
	bPlaceModeActive = bActive;
	RefreshVisual();
}

bool UIH_P1C08_PlaceShipWidget::IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	if (!PanelBorder) return false;
	return PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_PlaceShipWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsScreenPointOverPanel(ScreenAbsolute)) return false;
	SetPlaceModeActive(!bPlaceModeActive);
	return true;
}
