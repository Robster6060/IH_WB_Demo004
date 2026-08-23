// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_ConfirmRevertWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UIH_P1C08_ConfirmRevertWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ConfirmRoot"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, 0.92f);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmVBox"));

	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("ConfirmTitle"), TEXT("Confirm Revert"), FName(TEXT("HeadingText")));
	if (UVerticalBoxSlot* TitleSlot = VB->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	BodyText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree,
		TEXT("ConfirmBody"),
		TEXT("Discard uncommitted coastline and position changes?"),
		FName(TEXT("SecondaryText")));
	BodyText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* BodySlot = VB->AddChildToVerticalBox(BodyText))
	{
		BodySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmButtons"));
	UButton* YesBtn = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("ConfirmYes"), TEXT("Yes"), FName(TEXT("HandleYesClicked")));
	UButton* NoBtn = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("ConfirmNo"), TEXT("No"), FName(TEXT("HandleNoClicked")));
	if (UHorizontalBoxSlot* YesSlot = Buttons->AddChildToHorizontalBox(YesBtn))
	{
		YesSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	Buttons->AddChildToHorizontalBox(NoBtn);
	VB->AddChildToVerticalBox(Buttons);

	PanelBorder->AddChild(VB);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		CSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CSlot->SetAutoSize(true);
		CSlot->SetPosition(FVector2D::ZeroVector);
	}
}

TSharedRef<SWidget> UIH_P1C08_ConfirmRevertWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_ConfirmRevertWidget::ShowDialog(FOnConfirmRevertChoice InOnChoice)
{
	ShowDialog(TEXT("Confirm Revert"), TEXT("Discard uncommitted coastline and position changes?"), MoveTemp(InOnChoice));
}

void UIH_P1C08_ConfirmRevertWidget::ShowDialog(
	const FString& Title,
	const FString& Body,
	FOnConfirmRevertChoice InOnChoice)
{
	EnsureWidgetTree();
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Title));
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(Body));
	}
	OnChoice = MoveTemp(InOnChoice);
	SetVisibility(ESlateVisibility::Visible);
}

void UIH_P1C08_ConfirmRevertWidget::HideDialog()
{
	SetVisibility(ESlateVisibility::Collapsed);
	OnChoice.Unbind();
}

void UIH_P1C08_ConfirmRevertWidget::HandleYesClicked()
{
	FOnConfirmRevertChoice Choice = OnChoice;
	HideDialog();
	if (Choice.IsBound())
	{
		Choice.Execute(true);
	}
}

void UIH_P1C08_ConfirmRevertWidget::HandleNoClicked()
{
	FOnConfirmRevertChoice Choice = OnChoice;
	HideDialog();
	if (Choice.IsBound())
	{
		Choice.Execute(false);
	}
}
