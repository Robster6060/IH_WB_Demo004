// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_CameraAslWidget.h"

#include "IH_BuildPalettePanelStyle.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UIH_P1C08_CameraAslWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AslRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AslPanel"));
	PanelBorder->SetPadding(FMargin(12.f, 10.f));
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

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AslVBox"));

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AslTitle"));
	TitleText->SetText(FText::FromString(TEXT("ASL")));
	TitleText->SetColorAndOpacity(
		FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")))));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);
	VB->AddChildToVerticalBox(TitleText);

	ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AslValue"));
	ValueText->SetColorAndOpacity(
		FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("SecondaryText")))));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(ValueText);
	if (UVerticalBoxSlot* ValueSlot = VB->AddChildToVerticalBox(ValueText))
	{
		ValueSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	PanelBorder->AddChild(VB);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		// Left of Game Speed in ASL | Game Speed | DEV View (tabs sit below this row).
		const float AslRightX =
			-(IH_BuildPalettePanelStyle::TopRightHudClusterRightClearPx
				+ IH_BuildPalettePanelStyle::TopRightHudDevViewW
				+ IH_BuildPalettePanelStyle::TopRightHudClusterGapPx
				+ IH_BuildPalettePanelStyle::TopRightHudGameSpeedApproxW
				+ IH_BuildPalettePanelStyle::TopRightHudClusterGapPx);
		CSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		CSlot->SetAlignment(FVector2D(1.f, 0.f));
		CSlot->SetPosition(FVector2D(AslRightX, IH_BuildPalettePanelStyle::TopRightHudClusterTopY));
		CSlot->SetAutoSize(true);
	}
}

TSharedRef<SWidget> UIH_P1C08_CameraAslWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_CameraAslWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(1.f, 0.f)); // Plan Addendum 19
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(ValueText);
	UpdateAltitudeMeters(0);
}

void UIH_P1C08_CameraAslWidget::UpdateAltitudeMeters(const int32 AltitudeMeters)
{
	EnsureWidgetTree();
	if (ValueText)
	{
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%d m"), AltitudeMeters)));
	}
}
