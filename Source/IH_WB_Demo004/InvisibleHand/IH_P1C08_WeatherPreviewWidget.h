// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHCalendarTypes.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "IH_P1C08_WeatherPreviewWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

/** Simple styled ComboBoxString shared by both dropdowns on this widget — no per-row/highlight
 * machinery needed (unlike IH_P1C08_IslandNavWidget's OriginComboBox), just the dark-field look. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_WeatherPreviewComboBox : public UComboBoxString
{
	GENERATED_BODY()

public:
	void ConfigureStyle();
	void OpenDropdown();
	void CloseDropdown();

private:
	UFUNCTION()
	UWidget* HandleGenerateComboWidget(FString Item);
};

/** DEV-only interactive preview: pick any of UDS's 13 stock Weather Presets crossed with any of
 * the 10 time brackets, to review lighting/weather combinations at will. Independent of the live
 * Random Weather Variation system running in the background — "Resume Random Weather" hands
 * control back to it. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_WeatherPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool IsPanelVisible() const { return bPanelVisible; }
	void TogglePanelVisible();
	void SetPanelVisible(bool bVisible);
	void UpdatePanelLayout();
	void ApplyDevPanelStackPosition(float TopY, float ContentHeight);
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const;

private:
	UFUNCTION()
	void HandlePresetSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleHourSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleResumeRandomClicked();

	void EnsureWidgetTree();
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	static FString TimeBracketLabel(EIHTimeBracket Bracket);
	static EIHTimeBracket TimeBracketFromLabel(const FString& Label);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_WeatherPreviewComboBox> PresetCombo;
	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_WeatherPreviewComboBox> HourCombo;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeRandomButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	bool bPanelVisible = true;
};
