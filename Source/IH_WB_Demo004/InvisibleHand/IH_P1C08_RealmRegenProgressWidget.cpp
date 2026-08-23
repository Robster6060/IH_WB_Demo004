// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_RealmRegenProgressWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UIH_P1C08_RealmRegenProgressWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RegenProgressRoot"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RegenProgressBorder"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, 0.94f, true);

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RegenProgressSize"));
	SizeBox->SetWidthOverride(300.f);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RegenProgressVBox"));

	LabelText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("RegenProgressLabel"), TEXT("Generating realm..."));
	LabelText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* LabelSlot = VB->AddChildToVerticalBox(LabelText))
	{
		LabelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("RegenProgressBar"));
	ProgressBar->SetPercent(0.f);
	{
		FProgressBarStyle BarStyle = ProgressBar->GetWidgetStyle();
		BarStyle.BackgroundImage.TintColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("ProgressBarBackground")));
		BarStyle.FillImage.TintColor = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("ProgressBarFill")));
		ProgressBar->SetWidgetStyle(BarStyle);
	}
	if (UVerticalBoxSlot* BarSlot = VB->AddChildToVerticalBox(ProgressBar))
	{
		BarSlot->SetPadding(FMargin(0.f));
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	PanelBorder->AddChild(SizeBox);
	SizeBox->AddChild(VB);

	if (UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}
}

TSharedRef<SWidget> UIH_P1C08_RealmRegenProgressWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_RealmRegenProgressWidget::SetDisplayPercent(const float Percent)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}
}

void UIH_P1C08_RealmRegenProgressWidget::ShowProgress(const FString& Label)
{
	EnsureWidgetTree();
	if (LabelText)
	{
		LabelText->SetText(FText::FromString(Label));
	}
	SetDisplayPercent(0.05f);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (PanelBorder)
	{
		PanelBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UIH_P1C08_RealmRegenProgressWidget::SetProgressLabel(const FString& Label)
{
	EnsureWidgetTree();
	if (LabelText)
	{
		LabelText->SetText(FText::FromString(Label));
	}
}

void UIH_P1C08_RealmRegenProgressWidget::CompleteAndHide()
{
	SetDisplayPercent(1.f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UIH_P1C08_RealmRegenProgressWidget::UpdateFakeProgress(const float Percent)
{
	SetDisplayPercent(Percent);
}
