// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C08_HUDSliderKeyboardFocus.h"
#include "IH_P1C08_IslandCoastlineTuning.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_CoastlineTuningWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class USlider;
class UTextBlock;
class UVerticalBox;

UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_CoastlineTuningWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

public:
	bool IsSliderCapturingInput() const { return bSliderCaptureActive; }
	bool IsKeyboardFocusActive() const { return KeyboardFocus.IsActive(); }
	bool HasUncommittedDraft() const;

	bool ProcessPanelPointerDown(const FVector2D& ScreenAbsolute);
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool TryActivateKeyboardFocusFromPanelClick(const FVector2D& ScreenAbsolute);
	void HandleScreenPointerMove(const FVector2D& ScreenAbsolute);
	void HandleScreenPointerUp(const FVector2D& ScreenAbsolute);

	void TickKeyboardFocusInput(class APlayerController* PC, float DeltaTime);
	void CancelKeyboardFocus();
	void CommitKeyboardFocus(bool bOnlyIfDirty = false);
	void CommitActiveDraftOnly();
	void ApplyChanges();
	void UpdatePanelLayout();
	float GetStackContentHeight() const;
	void ApplyDevPanelStackPosition(float TopY, float ContentHeight);
	void SyncSlidersFromActiveTuning();
	void UpdateDraftStatusText();
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const { return IsPointOverPanel(ScreenAbsolute); }

private:
	UFUNCTION()
	void HandleAmplitudeChanged(float Value);
	UFUNCTION()
	void HandleFrequencyChanged(float Value);
	UFUNCTION()
	void HandleWarpChanged(float Value);
	UFUNCTION()
	void HandleLobeChanged(float Value);
	UFUNCTION()
	void HandleRippleChanged(float Value);
	UFUNCTION()
	void HandleSummitAltitudeChanged(float Value);
	UFUNCTION()
	void HandleSliderCaptureBegin();
	UFUNCTION()
	void HandleSliderCaptureEnd();
	UFUNCTION()
	void HandleApplyChangesClicked();
	UFUNCTION()
	void HandleFlyoutToggleClicked();
	UFUNCTION()
	void HandleResetToTemplateClicked();

	void EnsureWidgetTree();
	void UpdateFlyoutVisibility();
	void ApplyPreviewValues(const TArray<float>& NormalizedValues);
	TArray<float> GatherSliderValues() const;
	void SyncSlidersFromValues(const TArray<float>& NormalizedValues);
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	bool IsPointOverAnySlider(const FVector2D& ScreenAbsolute) const;
	bool IsPointOverApplyButton(const FVector2D& ScreenAbsolute) const;
	bool IsPointOverFlyoutToggle(const FVector2D& ScreenAbsolute) const;
	bool IsPointOverResetButton(const FVector2D& ScreenAbsolute) const;
	void EnterKeyboardFocus();
	void RefreshKeyboardFocusHighlight();
	void ApplySliderValueFromScreen(USlider* Slider, const FVector2D& ScreenAbsolute);
	void BindSubsystems();
	void UnbindSubsystems();
	void HandleIslandSelectionChanged(int32 IslandIndex);
	void HandleIslandNavChanged();
	void HandleCoastlineTuningChanged(int32 IslandIndex);
	void UpdatePanelTitle();

	static float AmplitudeToSlider(float Value);
	static float FrequencyToSlider(float Value);
	static float WarpToSlider(float Value);
	static float SummitAltitudeToSlider(float Value);
	static float SummitAltitudeFromSlider(float Slider);
	static TArray<float> ActiveTuningToSliderValues(const FIHIslandCoastlineTuning& Tuning);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<USlider> AmplitudeSlider;
	UPROPERTY(Transient)
	TObjectPtr<USlider> FrequencySlider;
	UPROPERTY(Transient)
	TObjectPtr<USlider> WarpSlider;
	UPROPERTY(Transient)
	TObjectPtr<USlider> LobeSlider;
	UPROPERTY(Transient)
	TObjectPtr<USlider> RippleSlider;
	UPROPERTY(Transient)
	TObjectPtr<USlider> SummitAltitudeSlider;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentVBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> FlyoutToggleButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ResetToTemplateButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ApplyChangesButton;

	bool bFlyoutExpanded = false;

	FIH_P1C08_HUDSliderKeyboardFocus KeyboardFocus;
	TArray<TObjectPtr<USlider>> SliderWidgets;

	static constexpr FLinearColor KeyboardFocusOutlineColor = FLinearColor(0.4f, 0.75f, 1.0f, 1.f);
	static constexpr float KeyboardFocusOutlineThickness = 2.f;
	static constexpr float KeyboardFocusOutlinePadding = 1.f;
	static constexpr float KeyboardFocusOutlineVerticalOffset = 8.f;
	static constexpr float KeyboardFocusOutlineHorizontalOffset = 4.f;
	static constexpr float KeyboardFocusOutlineHorizontalRightPadding = 10.f;

	bool bSliderCaptureActive = false;
	bool bSuppressSliderValueChanged = false;
	int32 ActiveDragSliderIndex = INDEX_NONE;
	static constexpr int32 SliderCount = 6;
	static constexpr float NavigationKeyRepeatSec = 0.05f;
	static constexpr float IslandNudgeCm = 5000.f;

	float LastLeftNudgeTime = -1.f;
	float LastRightNudgeTime = -1.f;
	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle IslandNavChangedHandle;
	FDelegateHandle CoastlineTuningChangedHandle;
};
