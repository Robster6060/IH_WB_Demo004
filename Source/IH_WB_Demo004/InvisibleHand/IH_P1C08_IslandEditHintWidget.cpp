// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_IslandEditHintWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UIH_P1C08_IslandEditHintWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HintRoot"));
	WidgetTree->RootWidget = Canvas;

	HintText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("EditHint"), TEXT(""), FName(TEXT("SecondaryText")));
	HintText->SetJustification(ETextJustify::Center);

	if (UCanvasPanelSlot* HintSlot = Canvas->AddChildToCanvas(HintText))
	{
		HintSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		HintSlot->SetAlignment(FVector2D(0.5f, 1.f));
		HintSlot->SetAutoSize(true);
		HintSlot->SetPosition(FVector2D(0.f, -28.f));
	}
}

TSharedRef<SWidget> UIH_P1C08_IslandEditHintWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_IslandEditHintWidget::ApplyHintText(const FString& Text)
{
	EnsureWidgetTree();
	if (HintText)
	{
		HintText->SetText(FText::FromString(Text));
	}
	SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UIH_P1C08_IslandEditHintWidget::SetHintText(const FString& Text)
{
	ApplyHintText(Text);
}

void UIH_P1C08_IslandEditHintWidget::SetPieDevHint()
{
	bPieDevHintActive = true;
	PieDevHintText = TEXT("Press L for Sun");
	ApplyHintText(PieDevHintText);
}

void UIH_P1C08_IslandEditHintWidget::ClearHint()
{
	if (bPieDevHintActive)
	{
		ApplyHintText(PieDevHintText);
		return;
	}

	ApplyHintText(FString());
}
