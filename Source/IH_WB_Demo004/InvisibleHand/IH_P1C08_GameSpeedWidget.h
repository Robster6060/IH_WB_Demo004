// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "IH_P1C08_HUDSliderKeyboardFocus.h"

#include "Blueprint/UserWidget.h"

#include "IH_P1C08_GameSpeedWidget.generated.h"



class UBorder;

class USlider;

class UTextBlock;



/** PIE game-speed selector: 0.25x … 8.0x via slider (top-right HUD). */

UCLASS()

class IH_WB_DEMO004_API UIH_P1C08_GameSpeedWidget : public UUserWidget

{

	GENERATED_BODY()



protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeConstruct() override;



public:

	bool IsSliderCapturingInput() const { return bSliderCaptureActive; }

	bool IsKeyboardFocusActive() const { return KeyboardFocus.IsActive(); }



	bool ProcessPanelPointerDown(const FVector2D& ScreenAbsolute);

	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	void HandleScreenPointerMove(const FVector2D& ScreenAbsolute);
	void HandleScreenPointerUp(const FVector2D& ScreenAbsolute);

	bool TryActivateKeyboardFocusFromPanelClick(const FVector2D& ScreenAbsolute);

	void TickKeyboardFocusInput(class APlayerController* PC, float DeltaTime);

	void CancelKeyboardFocus();

	void CommitKeyboardFocus(bool bOnlyIfDirty = false);

	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const { return IsPointOverPanel(ScreenAbsolute); }

	void ToggleKeyboardPause();
	bool IsKeyboardPauseActive() const { return bKeyboardPauseActive; }

	static constexpr float SpeedSliderMin = 0.25f;

	static constexpr float SpeedSliderMax = 8.f;

	static constexpr float SpeedSliderStep = 0.25f;

	static constexpr float DefaultSpeed = 1.f;



private:

	UFUNCTION()

	void HandleSpeedChanged(float Value);



	UFUNCTION()

	void HandleSliderCaptureBegin();



	UFUNCTION()

	void HandleSliderCaptureEnd();



	void EnsureWidgetTree();

	TArray<float> GatherSliderValues() const;

	void SyncSlidersFromValues(const TArray<float>& NormalizedValues);

	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;

	bool IsPointOverSlider(const FVector2D& ScreenAbsolute) const;

	void EnterKeyboardFocus();

	void ApplySpeedFromNormalized(float NormalizedValue);
	void UpdatePanelChrome();

	void ApplySliderValueFromScreen(USlider* Slider, const FVector2D& ScreenAbsolute);

	static float SnapSpeed(float Speed);

	static FString FormatSpeedLabel(float Speed);



	UPROPERTY(Transient)

	TObjectPtr<UBorder> PanelBorder;



	UPROPERTY(Transient, meta = (BindWidgetOptional))

	TObjectPtr<USlider> SpeedSlider;



	UPROPERTY(Transient, meta = (BindWidgetOptional))

	TObjectPtr<UTextBlock> TitleText;



	UPROPERTY(Transient, meta = (BindWidgetOptional))

	TObjectPtr<UTextBlock> ValueText;



	FIH_P1C08_HUDSliderKeyboardFocus KeyboardFocus;



	bool bSliderCaptureActive = false;

	bool bSuppressSliderValueChanged = false;

	bool bActiveSliderDrag = false;

	static constexpr int32 SliderCount = 1;

	static constexpr float NavigationKeyRepeatSec = 0.05f;



	float LastLeftNudgeTime = -1.f;

	float LastRightNudgeTime = -1.f;

	bool bKeyboardPauseActive = false;

	float SpeedBeforeKeyboardPause = DefaultSpeed;

	static constexpr float PauseFocusBoxWidthPx = 4.f;

};

