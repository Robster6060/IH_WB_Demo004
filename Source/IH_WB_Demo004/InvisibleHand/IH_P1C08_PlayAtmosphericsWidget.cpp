// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_PlayAtmosphericsWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IH_P1C08_GameDateTimeWidget.h"
#include "IH_P1C08_WeatherPreviewWidget.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IH_WB_Demo004GameMode.h"
#include "IHUIColorSchemeLibrary.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UIH_P1C08_PlayAtmosphericsWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayAtmosphericsRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlayAtmosphericsPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, IH_P1C08_DevPanelStyle::PanelBackgroundAlpha, /*bCompactPadding=*/true);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayAtmosphericsVBox"));

	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("PlayAtmosphericsTitle"), TEXT("Play Atmospherics"), FName(TEXT("SecondaryText")));
	VB->AddChildToVerticalBox(TitleText);

	// Row 2: Month | Day | Hour dial — moved here from the (now read-only again) Date widget.
	UHorizontalBox* DialRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PlayAtmosphericsDialRow"));

	MonthCombo = WidgetTree->ConstructWidget<UIH_P1C08_WeatherPreviewComboBox>(
		UIH_P1C08_WeatherPreviewComboBox::StaticClass(), TEXT("PlayAtmosphericsMonthCombo"));
	MonthCombo->ConfigureStyle();
	for (int32 M = 1; M <= 12; ++M)
	{
		MonthCombo->AddOption(MonthAbbrev(M));
	}
	MonthCombo->SetSelectedOption(MonthAbbrev(4));
	MonthCombo->OnSelectionChanged.AddDynamic(this, &UIH_P1C08_PlayAtmosphericsWidget::HandleMonthSelectionChanged);
	if (UHorizontalBoxSlot* S = DialRow->AddChildToHorizontalBox(MonthCombo)) { S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }

	DayCombo = WidgetTree->ConstructWidget<UIH_P1C08_WeatherPreviewComboBox>(
		UIH_P1C08_WeatherPreviewComboBox::StaticClass(), TEXT("PlayAtmosphericsDayCombo"));
	DayCombo->ConfigureStyle();
	for (int32 D = 1; D <= 30; ++D)
	{
		DayCombo->AddOption(FString::FromInt(D));
	}
	DayCombo->SetSelectedOption(TEXT("1"));
	DayCombo->OnSelectionChanged.AddDynamic(this, &UIH_P1C08_PlayAtmosphericsWidget::HandleDaySelectionChanged);
	if (UHorizontalBoxSlot* S = DialRow->AddChildToHorizontalBox(DayCombo)) { S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }

	HourCombo = WidgetTree->ConstructWidget<UIH_P1C08_WeatherPreviewComboBox>(
		UIH_P1C08_WeatherPreviewComboBox::StaticClass(), TEXT("PlayAtmosphericsHourCombo"));
	HourCombo->ConfigureStyle();
	for (int32 Index = 0; Index < 10; ++Index)
	{
		HourCombo->AddOption(TimeBracketLabel(static_cast<EIHTimeBracket>(Index)));
	}
	HourCombo->SetSelectedOption(TimeBracketLabel(EIHTimeBracket::Afternoon));
	HourCombo->OnSelectionChanged.AddDynamic(this, &UIH_P1C08_PlayAtmosphericsWidget::HandleHourSelectionChanged);
	DialRow->AddChildToHorizontalBox(HourCombo);

	if (UVerticalBoxSlot* DialRowSlot = VB->AddChildToVerticalBox(DialRow))
	{
		DialRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
	}

	// Row 3: Play | Pause | Stop.
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PlayAtmosphericsButtonRow"));
	PlayButton = IH_P1C08_DevPanelStyle::MakeHUDButton(WidgetTree, this, TEXT("PlayButton"), TEXT("Play"), FName("HandlePlayClicked"), /*bCompact=*/true);
	PauseButton = IH_P1C08_DevPanelStyle::MakeHUDButton(WidgetTree, this, TEXT("PauseButton"), TEXT("Pause"), FName("HandlePauseClicked"), /*bCompact=*/true);
	StopButton = IH_P1C08_DevPanelStyle::MakeHUDButton(WidgetTree, this, TEXT("StopButton"), TEXT("Stop"), FName("HandleStopClicked"), /*bCompact=*/true);
	if (UHorizontalBoxSlot* S = ButtonRow->AddChildToHorizontalBox(PlayButton)) { S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }
	if (UHorizontalBoxSlot* S = ButtonRow->AddChildToHorizontalBox(PauseButton)) { S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }
	ButtonRow->AddChildToHorizontalBox(StopButton);
	if (UVerticalBoxSlot* RowSlot = VB->AddChildToVerticalBox(ButtonRow))
	{
		RowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
	}

	// Row 4: Speed.
	VB->AddChildToVerticalBox(IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("PlayAtmosphericsSpeedLabel"), TEXT("Speed")));
	SpeedSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("PlayAtmosphericsSpeedSlider"));
	SpeedSlider->SetMinValue(0.f);
	SpeedSlider->SetMaxValue(1.f);
	SpeedSlider->SetStepSize(0.01f);
	SpeedSlider->SetValue(0.7f); // 0=slowest (2s/step), 1=fastest (0.1s/step) - see CurrentSecondsPerStep
	SpeedSlider->OnValueChanged.AddDynamic(this, &UIH_P1C08_PlayAtmosphericsWidget::HandleSpeedChanged);
	UIHUIColorSchemeLibrary::ApplyHUDSliderStyle(SpeedSlider);
	VB->AddChildToVerticalBox(SpeedSlider);

	PanelBorder->AddChild(VB);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		// Left edge sits at screen-center, so this panel sits immediately right of the Game
		// Date|Time panel (whose right edge is anchored to that same center point) regardless of
		// either panel's rendered width.
		IH_P1C08_DevPanelStyle::ApplyTopCenterPanelSlot(CSlot, IH_P1C08_DevPanelStyle::TopMargin, 0.f, 0.f);
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::SetPanelVisible(bool bVisible)
{
	bPanelVisible = bVisible;
	SetVisibility(bPanelVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bPanelVisible)
	{
		SyncCombosFromGameInstance();
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::UpdatePanelLayout()
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
				IH_P1C08_DevPanelStyle::ApplyTopCenterPanelSlot(CSlot, IH_P1C08_DevPanelStyle::TopMargin, 0.f, 0.f);
			}
		}
	}
}

bool UIH_P1C08_PlayAtmosphericsWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return PanelBorder && bPanelVisible && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_PlayAtmosphericsWidget::IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return IsPointOverPanel(ScreenAbsolute);
}

bool UIH_P1C08_PlayAtmosphericsWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute))
	{
		return false;
	}

	if ((MonthCombo && MonthCombo->IsOpen()) || (DayCombo && DayCombo->IsOpen()) || (HourCombo && HourCombo->IsOpen()))
	{
		return false;
	}

	if (MonthCombo && MonthCombo->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		MonthCombo->OpenDropdown();
		return true;
	}
	if (DayCombo && DayCombo->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		DayCombo->OpenDropdown();
		return true;
	}
	if (HourCombo && HourCombo->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		HourCombo->OpenDropdown();
		return true;
	}
	if (PlayButton && PlayButton->GetIsEnabled() && PlayButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		PlayButton->OnClicked.Broadcast();
		return true;
	}
	if (PauseButton && PauseButton->GetIsEnabled() && PauseButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		PauseButton->OnClicked.Broadcast();
		return true;
	}
	if (StopButton && StopButton->GetIsEnabled() && StopButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
	{
		StopButton->OnClicked.Broadcast();
		return true;
	}
	// Slider drag isn't wired through this manual system (matches the rest of this codebase's
	// combo/button-only convention) - consuming here still prevents world click-through.
	return true;
}

TSharedRef<SWidget> UIH_P1C08_PlayAtmosphericsWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_PlayAtmosphericsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.5f, 0.f));
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);
	SyncCombosFromGameInstance();
	SetPanelVisible(true);
}

const TCHAR* UIH_P1C08_PlayAtmosphericsWidget::MonthAbbrev(int32 Month)
{
	static const TCHAR* Names[12] = {
		TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"),
		TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")
	};
	return Names[FMath::Clamp(Month, 1, 12) - 1];
}

int32 UIH_P1C08_PlayAtmosphericsWidget::MonthFromAbbrev(const FString& Abbrev)
{
	for (int32 M = 1; M <= 12; ++M)
	{
		if (Abbrev == MonthAbbrev(M))
		{
			return M;
		}
	}
	return 4;
}

FString UIH_P1C08_PlayAtmosphericsWidget::TimeBracketLabel(EIHTimeBracket Bracket)
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

EIHTimeBracket UIH_P1C08_PlayAtmosphericsWidget::TimeBracketFromLabel(const FString& Label)
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

void UIH_P1C08_PlayAtmosphericsWidget::ApplyDialedCalendarChange()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->ApplyCalendarFromGameInstance();
		}
	}
	if (UIH_P1C08_GameDateTimeWidget* DateWidget = DateWidgetRef.Get())
	{
		DateWidget->RefreshFromGameInstance();
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::SyncCombosFromGameInstance()
{
	const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>();
	if (!GI)
	{
		return;
	}
	if (MonthCombo) { MonthCombo->SetSelectedOption(MonthAbbrev(GI->GetRealmMonth())); }
	if (DayCombo) { DayCombo->SetSelectedOption(FString::FromInt(GI->GetRealmDay())); }
	if (HourCombo) { HourCombo->SetSelectedOption(TimeBracketLabel(GI->GetRealmHourBracket())); }
}

void UIH_P1C08_PlayAtmosphericsWidget::HandleMonthSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		GI->SetRealmMonth(MonthFromAbbrev(SelectedItem));
		ApplyDialedCalendarChange();
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::HandleDaySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		GI->SetRealmDay(FCString::Atoi(*SelectedItem));
		ApplyDialedCalendarChange();
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::HandleHourSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		GI->SetRealmHourBracket(TimeBracketFromLabel(SelectedItem));
		ApplyDialedCalendarChange();
	}
}

float UIH_P1C08_PlayAtmosphericsWidget::CurrentSecondsPerStep() const
{
	const float T = SpeedSlider ? SpeedSlider->GetValue() : 0.7f;
	// T=0 -> 2.0s/step (slowest), T=1 -> 0.1s/step (fastest).
	return FMath::Lerp(2.f, 0.1f, FMath::Clamp(T, 0.f, 1.f));
}

void UIH_P1C08_PlayAtmosphericsWidget::StartDateRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		// Same cadence as the GameMode's own advance step, so the read-only Date display stays
		// visually in sync with the auto-advancing calendar without GameMode needing to know
		// about UI widgets at all.
		World->GetTimerManager().SetTimer(
			DateRefreshTimer, this, &UIH_P1C08_PlayAtmosphericsWidget::HandleDateRefreshTick,
			FMath::Max(CurrentSecondsPerStep(), 0.05f), true);
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::StopDateRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DateRefreshTimer);
	}
}

void UIH_P1C08_PlayAtmosphericsWidget::HandleDateRefreshTick()
{
	if (UIH_P1C08_GameDateTimeWidget* DateWidget = DateWidgetRef.Get())
	{
		DateWidget->RefreshFromGameInstance();
	}
	SyncCombosFromGameInstance();
}

void UIH_P1C08_PlayAtmosphericsWidget::HandlePlayClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->StartAtmosphericsPlayback(CurrentSecondsPerStep());
		}
	}
	StartDateRefreshTimer();
}

void UIH_P1C08_PlayAtmosphericsWidget::HandlePauseClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->PauseAtmosphericsPlayback();
		}
	}
	StopDateRefreshTimer();
}

void UIH_P1C08_PlayAtmosphericsWidget::HandleStopClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			GM->StopAtmosphericsPlayback();
		}
	}
	StopDateRefreshTimer();
	if (UIH_P1C08_GameDateTimeWidget* DateWidget = DateWidgetRef.Get())
	{
		DateWidget->RefreshFromGameInstance();
	}
	SyncCombosFromGameInstance();
}

void UIH_P1C08_PlayAtmosphericsWidget::HandleSpeedChanged(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
		{
			if (GM->IsAtmosphericsPlaying())
			{
				// Re-Start at the new rate - harmless no-op restart if not currently playing.
				GM->StartAtmosphericsPlayback(CurrentSecondsPerStep());
				StartDateRefreshTimer();
			}
		}
	}
}
