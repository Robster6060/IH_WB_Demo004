// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHCalendarTypes.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_PlayAtmosphericsWidget.generated.h"

class UBorder;
class UButton;
class USlider;
class UTextBlock;
class UIH_P1C08_GameDateTimeWidget;
class UIH_P1C08_WeatherPreviewComboBox;

/** DEV-only transport control (Month|Day|Hour dial + Play/Pause/Stop + Speed) that auto-advances
 * the calendar Hour bracket (then Day, Month, Year at the canonical boundaries) starting from
 * whatever's dialed in here, until Pause or Stop. Sits immediately right of the (now read-only)
 * Game Date|Time widget, which it keeps refreshed live while playing (IH-DEC-040 follow-on). */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_PlayAtmosphericsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool IsPanelVisible() const { return bPanelVisible; }
	void SetPanelVisible(bool bVisible);
	void UpdatePanelLayout();
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const;
	/** Set once by the PlayerController right after both widgets are constructed, so this widget
	 * can keep the read-only Date display refreshed while playing, and resync it on Stop. */
	void SetDateWidget(UIH_P1C08_GameDateTimeWidget* InDateWidget) { DateWidgetRef = InDateWidget; }

private:
	UFUNCTION()
	void HandleMonthSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleDaySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleHourSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandlePlayClicked();
	UFUNCTION()
	void HandlePauseClicked();
	UFUNCTION()
	void HandleStopClicked();
	UFUNCTION()
	void HandleSpeedChanged(float Value);
	UFUNCTION()
	void HandleDateRefreshTick();

	void EnsureWidgetTree();
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	float CurrentSecondsPerStep() const;
	void ApplyDialedCalendarChange();
	void SyncCombosFromGameInstance();
	void StartDateRefreshTimer();
	void StopDateRefreshTimer();
	static const TCHAR* MonthAbbrev(int32 Month);
	static int32 MonthFromAbbrev(const FString& Abbrev);
	static FString TimeBracketLabel(EIHTimeBracket Bracket);
	static EIHTimeBracket TimeBracketFromLabel(const FString& Label);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_WeatherPreviewComboBox> MonthCombo;
	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_WeatherPreviewComboBox> DayCombo;
	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_WeatherPreviewComboBox> HourCombo;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PauseButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> StopButton;
	UPROPERTY(Transient)
	TObjectPtr<USlider> SpeedSlider;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	TWeakObjectPtr<UIH_P1C08_GameDateTimeWidget> DateWidgetRef;
	FTimerHandle DateRefreshTimer;
	bool bPanelVisible = true;
};
