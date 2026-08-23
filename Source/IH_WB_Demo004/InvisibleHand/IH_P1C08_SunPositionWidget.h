// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_HUDSliderKeyboardFocus.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_SunPositionWidget.generated.h"

class UBorder;
class USlider;
class UTextBlock;

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_SunPositionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool IsSliderCapturingInput() const { return bSliderCaptureActive; }
	bool IsKeyboardFocusActive() const { return KeyboardFocus.IsActive(); }
	bool IsPanelVisible() const { return bPanelVisible; }

	void TogglePanelVisible();
	void SetPanelVisible(bool bVisible);
	void UpdatePanelLayout();
	void ApplyDevPanelStackPosition(float TopY, float ContentHeight);

	bool ProcessPanelPointerDown(const FVector2D& ScreenAbsolute);
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	void HandleScreenPointerMove(const FVector2D& ScreenAbsolute);
	void HandleScreenPointerUp(const FVector2D& ScreenAbsolute);
	bool TryActivateKeyboardFocusFromPanelClick(const FVector2D& ScreenAbsolute);
	void TickKeyboardFocusInput(class APlayerController* PC, float DeltaTime);
	void CancelKeyboardFocus();
	void CommitKeyboardFocus(bool bOnlyIfDirty = false);
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const;

private:
	UFUNCTION()
	void HandleTimeOfDayChanged(float Value);
	UFUNCTION()
	void HandleSliderCaptureBegin();
	UFUNCTION()
	void HandleSliderCaptureEnd();

	void EnsureWidgetTree();
	TArray<float> GatherSliderValues() const;
	void SyncSliderFromGameInstance();
	void SyncSliderFromValue(float TimeOfDay);
	void UpdateStatusText(float TimeOfDay);
	void ApplySunTimeOfDay(float TimeOfDay);
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	bool IsPointOverSlider(const FVector2D& ScreenAbsolute) const;
	void EnterKeyboardFocus();
	void ApplyPreviewValues(const TArray<float>& NormalizedValues);
	void ApplySliderValueFromScreen(USlider* Slider, const FVector2D& ScreenAbsolute);

	static FString FormatTimeOfDayLabel(float TimeOfDay);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<USlider> TimeOfDaySlider;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	FIH_P1C08_HUDSliderKeyboardFocus KeyboardFocus;

	bool bSliderCaptureActive = false;
	bool bSuppressSliderValueChanged = false;
	bool bActiveSliderDrag = false;
	bool bPanelVisible = true;

	static constexpr int32 SliderCount = 1;
	static constexpr float NavigationKeyRepeatSec = 0.05f;

	float LastLeftNudgeTime = -1.f;
	float LastRightNudgeTime = -1.f;
};
