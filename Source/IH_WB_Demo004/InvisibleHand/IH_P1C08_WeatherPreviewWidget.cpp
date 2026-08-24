// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_WeatherPreviewWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IH_WB_Demo004GameMode.h"
#include "IHUIColorSchemeLibrary.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

void UIH_P1C08_WeatherPreviewComboBox::ConfigureStyle()
{
	SetContentPadding(FMargin(4.f, 1.f, 2.f, 1.f));

	FComboBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	Style.SetContentPadding(FMargin(4.f, 1.f, 2.f, 1.f));
	Style.SetMenuRowPadding(FMargin(6.f, 2.f, 8.f, 2.f));

	FComboButtonStyle ButtonStyle = Style.ComboButtonStyle;
	ButtonStyle.SetContentPadding(FMargin(0.f));
	ButtonStyle.SetDownArrowPadding(FMargin(2.f, 0.f, 0.f, 0.f));
	FButtonStyle InnerButton = ButtonStyle.ButtonStyle;
	InnerButton.Normal.TintColor = FSlateColor(FLinearColor(0.04f, 0.05f, 0.06f, 1.f));
	InnerButton.Hovered.TintColor = FSlateColor(FLinearColor(0.06f, 0.07f, 0.08f, 1.f));
	InnerButton.Pressed.TintColor = InnerButton.Hovered.TintColor;
	ButtonStyle.SetButtonStyle(InnerButton);
	Style.SetComboButtonStyle(ButtonStyle);

	SetWidgetStyle(Style);
	OnGenerateWidgetEvent.BindUFunction(this, FName("HandleGenerateComboWidget"));
}

void UIH_P1C08_WeatherPreviewComboBox::OpenDropdown()
{
	if (MyComboBox.IsValid() && !MyComboBox->IsOpen())
	{
		MyComboBox->SetIsOpen(true, true);
	}
}

void UIH_P1C08_WeatherPreviewComboBox::CloseDropdown()
{
	if (MyComboBox.IsValid() && MyComboBox->IsOpen())
	{
		MyComboBox->SetIsOpen(false, false);
	}
}

UWidget* UIH_P1C08_WeatherPreviewComboBox::HandleGenerateComboWidget(FString Item)
{
	UTextBlock* Text = NewObject<UTextBlock>(this);
	Text->SetText(FText::FromString(Item));
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", IH_P1C08_DevPanelStyle::OriginComboFontSize));
	Text->SetJustification(ETextJustify::Left);
	Text->SetColorAndOpacity(FSlateColor(
		Item == GetSelectedOption()
			? IH_P1C08_DevPanelStyle::IslandNavOriginActiveTextColor
			: IH_P1C08_DevPanelStyle::IslandNavOriginInactiveTextColor));
	return Text;
}

void UIH_P1C08_WeatherPreviewWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WeatherPreviewRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WeatherPreviewPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WeatherPreviewVBox"));

	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("WeatherPreviewTitle"), TEXT("Weather Preview"));
	VB->AddChildToVerticalBox(TitleText);

	VB->AddChildToVerticalBox(IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("WeatherPresetLabel"), TEXT("Preset")));
	PresetCombo = WidgetTree->ConstructWidget<UIH_P1C08_WeatherPreviewComboBox>(
		UIH_P1C08_WeatherPreviewComboBox::StaticClass(), TEXT("WeatherPresetCombo"));
	PresetCombo->ConfigureStyle();
	for (const FString& Name : IHInvisibleHandSpec::GetUdsWeatherPresetNames())
	{
		PresetCombo->AddOption(Name);
	}
	PresetCombo->SetSelectedOption(TEXT("Clear_Skies"));
	PresetCombo->OnSelectionChanged.AddDynamic(this, &UIH_P1C08_WeatherPreviewWidget::HandlePresetSelectionChanged);
	if (UVerticalBoxSlot* PresetSlot = VB->AddChildToVerticalBox(PresetCombo))
	{
		PresetSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
	}

	VB->AddChildToVerticalBox(IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("WeatherHourLabel"), TEXT("Hour")));
	HourCombo = WidgetTree->ConstructWidget<UIH_P1C08_WeatherPreviewComboBox>(
		UIH_P1C08_WeatherPreviewComboBox::StaticClass(), TEXT("WeatherHourCombo"));
	HourCombo->ConfigureStyle();
	for (int32 Index = 0; Index < 10; ++Index)
	{
		HourCombo->AddOption(TimeBracketLabel(static_cast<EIHTimeBracket>(Index)));
	}
	HourCombo->SetSelectedOption(TimeBracketLabel(EIHTimeBracket::Afternoon));
	HourCombo->OnSelectionChanged.AddDynamic(this, &UIH_P1C08_WeatherPreviewWidget::HandleHourSelectionChanged);
	if (UVerticalBoxSlot* HourSlot = VB->AddChildToVerticalBox(HourCombo))
	{
		HourSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
	}

	ResumeRandomButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("ResumeRandomButton"), TEXT("Resume Random Weather"),
		FName("HandleResumeRandomClicked"));
	VB->AddChildToVerticalBox(ResumeRandomButton);

	PanelBorder->AddChild(VB);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		IH_P1C08_DevPanelStyle::ConfigureTopLeftPanelSlot(
			CSlot,
			IH_P1C08_DevPanelStyle::EStackSlot::WeatherPreview,
			IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(IH_P1C08_DevPanelStyle::EStackSlot::WeatherPreview));
	}
}

void UIH_P1C08_WeatherPreviewWidget::TogglePanelVisible()
{
	SetPanelVisible(!bPanelVisible);
}

void UIH_P1C08_WeatherPreviewWidget::SetPanelVisible(bool bVisible)
{
	bPanelVisible = bVisible;
	SetVisibility(bPanelVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UIH_P1C08_WeatherPreviewWidget::ApplyDevPanelStackPosition(float TopY, float ContentHeight)
{
	if (!PanelBorder || !WidgetTree || !WidgetTree->RootWidget)
	{
		return;
	}
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		if (Canvas->GetSlots().Num() > 0)
		{
			if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Canvas->GetSlots()[0]))
			{
				IH_P1C08_DevPanelStyle::ApplyTopLeftPanelSlotAtY(CSlot, TopY, ContentHeight);
			}
		}
	}
}

void UIH_P1C08_WeatherPreviewWidget::UpdatePanelLayout()
{
	ApplyDevPanelStackPosition(
		IH_P1C08_DevPanelStyle::GetStackPosition(IH_P1C08_DevPanelStyle::EStackSlot::WeatherPreview).Y,
		IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(IH_P1C08_DevPanelStyle::EStackSlot::WeatherPreview));
}

bool UIH_P1C08_WeatherPreviewWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return PanelBorder && bPanelVisible && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_WeatherPreviewWidget::IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return IsPointOverPanel(ScreenAbsolute);
}

bool UIH_P1C08_WeatherPreviewWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute))
	{
		return false;
	}

	// Native Slate click dispatch doesn't reach the CLOSED combo button (same as every other HUD
	// panel in this codebase — manual hit-test needed to open it in the first place). But once
	// open, the popup list renders as a separate top-level Slate layer outside this panel's
	// hit-test path entirely, with its own native hover/click/outside-click-closes behavior — do
	// NOT manually geometry-test or close it here. An earlier version tried to detect "click
	// outside the combo, close it" using the closed button's own narrow geometry, which doesn't
	// cover the open popup's list rows and prematurely ate most selection clicks before Slate's
	// native handling could register them. Simplest correct fix: once open, get out of the way.
	if ((PresetCombo && PresetCombo->IsOpen()) || (HourCombo && HourCombo->IsOpen()))
	{
		return false;
	}

	if (PresetCombo && PresetCombo->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		PresetCombo->OpenDropdown();
		return true;
	}
	if (HourCombo && HourCombo->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		HourCombo->OpenDropdown();
		return true;
	}
	if (ResumeRandomButton && ResumeRandomButton->GetIsEnabled()
		&& ResumeRandomButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		ResumeRandomButton->OnClicked.Broadcast();
		return true;
	}

	return true;
}

TSharedRef<SWidget> UIH_P1C08_WeatherPreviewWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_WeatherPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.f, 0.f));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);
	SetPanelVisible(true);
}

FString UIH_P1C08_WeatherPreviewWidget::TimeBracketLabel(EIHTimeBracket Bracket)
{
	switch (Bracket)
	{
	case EIHTimeBracket::Midnight: return TEXT("Midnight");
	case EIHTimeBracket::Daybreak: return TEXT("Daybreak");
	case EIHTimeBracket::Sunrise: return TEXT("Sunrise");
	case EIHTimeBracket::Midmorning: return TEXT("Midmorning");
	case EIHTimeBracket::HighNoon: return TEXT("High-Noon");
	case EIHTimeBracket::Afternoon: return TEXT("Afternoon");
	case EIHTimeBracket::Eventide: return TEXT("Eventide");
	case EIHTimeBracket::Sunset: return TEXT("Sunset");
	case EIHTimeBracket::Twilight: return TEXT("Twilight");
	case EIHTimeBracket::Nightfall: return TEXT("Nightfall");
	default: return TEXT("Afternoon");
	}
}

EIHTimeBracket UIH_P1C08_WeatherPreviewWidget::TimeBracketFromLabel(const FString& Label)
{
	for (int32 Index = 0; Index < 10; ++Index)
	{
		if (TimeBracketLabel(static_cast<EIHTimeBracket>(Index)) == Label)
		{
			return static_cast<EIHTimeBracket>(Index);
		}
	}
	return EIHTimeBracket::Afternoon;
}

void UIH_P1C08_WeatherPreviewWidget::HandlePresetSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->ApplyWeatherPreset(SelectedItem);
		}
	}
}

void UIH_P1C08_WeatherPreviewWidget::HandleHourSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			// Preview-only time change: does not touch GameInstance's calendar snapshot.
			GM->ApplyPreviewTimeOfDay(TimeBracketFromLabel(SelectedItem));
		}
	}
}

void UIH_P1C08_WeatherPreviewWidget::HandleResumeRandomClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->ResumeRandomWeatherVariation();
		}
	}
}
