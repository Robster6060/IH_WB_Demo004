// Copyright Epic Games, Inc. All Rights Reserved.



#include "IH_P1C08_GameSpeedWidget.h"

#include "IH_BuildPalettePanelStyle.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IHUIColorSchemeLibrary.h"

#include "Components/Border.h"

#include "Components/CanvasPanel.h"

#include "Components/CanvasPanelSlot.h"

#include "Components/Slider.h"

#include "Components/TextBlock.h"

#include "Components/VerticalBox.h"

#include "Components/VerticalBoxSlot.h"

#include "Blueprint/WidgetTree.h"

#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"

#include "Styling/CoreStyle.h"



namespace

{

	static void ConfigureSpeedSlider(USlider* Slider)

	{

		if (!Slider)

		{

			return;

		}

		Slider->SetMinValue(UIH_P1C08_GameSpeedWidget::SpeedSliderMin);

		Slider->SetMaxValue(UIH_P1C08_GameSpeedWidget::SpeedSliderMax);

		Slider->SetStepSize(UIH_P1C08_GameSpeedWidget::SpeedSliderStep);

	}

}



float UIH_P1C08_GameSpeedWidget::SnapSpeed(float Speed)

{

	const float Clamped = FMath::Clamp(Speed, SpeedSliderMin, SpeedSliderMax);

	if (SpeedSliderStep <= 0.f)

	{

		return Clamped;

	}

	return SpeedSliderMin

		+ FMath::RoundToFloat((Clamped - SpeedSliderMin) / SpeedSliderStep) * SpeedSliderStep;

}



FString UIH_P1C08_GameSpeedWidget::FormatSpeedLabel(float Speed)

{

	if (Speed < 1.f)

	{

		return FString::Printf(TEXT("%.2fx"), Speed);

	}

	return FString::Printf(TEXT("%.1fx"), Speed);

}



void UIH_P1C08_GameSpeedWidget::EnsureWidgetTree()

{

	if (!WidgetTree || WidgetTree->RootWidget)

	{

		return;

	}



	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpeedRootCanvas"));

	WidgetTree->RootWidget = Canvas;



	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SpeedPanel"));

	PanelBorder->SetPadding(FMargin(12.f, 10.f));

	{

		FSlateBrush PanelBrush;

		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;

		PanelBrush.TintColor = FSlateColor(

			UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName(TEXT("PanelBackground")), 0.82f));

		PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

		PanelBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);

		PanelBrush.OutlineSettings.Color =

			UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));

		PanelBrush.OutlineSettings.Width = 1.f;

		PanelBorder->SetBrush(PanelBrush);

	}



	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SpeedVBox"));



	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeedTitle"));

	TitleText->SetText(FText::FromString(TEXT("Game Speed")));

	TitleText->SetColorAndOpacity(

		FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("HeadingText")))));

	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);

	VB->AddChildToVerticalBox(TitleText);



	ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeedValue"));

	ValueText->SetColorAndOpacity(

		FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("SecondaryText")))));

	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(ValueText);

	if (UVerticalBoxSlot* ValueSlot = VB->AddChildToVerticalBox(ValueText))

	{

		ValueSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	}



	SpeedSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("SpeedSlider"));

	ConfigureSpeedSlider(SpeedSlider);

	SpeedSlider->SetValue(DefaultSpeed);

	SpeedSlider->OnValueChanged.AddDynamic(this, &UIH_P1C08_GameSpeedWidget::HandleSpeedChanged);

	SpeedSlider->OnMouseCaptureBegin.AddDynamic(this, &UIH_P1C08_GameSpeedWidget::HandleSliderCaptureBegin);

	SpeedSlider->OnMouseCaptureEnd.AddDynamic(this, &UIH_P1C08_GameSpeedWidget::HandleSliderCaptureEnd);

	UIHUIColorSchemeLibrary::ApplyHUDSliderStyle(SpeedSlider);

	if (UVerticalBoxSlot* SliderSlot = VB->AddChildToVerticalBox(SpeedSlider))

	{

		SliderSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	}



	PanelBorder->AddChild(VB);



	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))

	{

		CSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));

		CSlot->SetAlignment(FVector2D(1.f, 0.f));

		// Middle of ASL | Game Speed | DEV View — left of DEV View (tabs below row).
		const float GameSpeedRightX =
			-(IH_BuildPalettePanelStyle::TopRightHudClusterRightClearPx
				+ IH_BuildPalettePanelStyle::TopRightHudDevViewW
				+ IH_BuildPalettePanelStyle::TopRightHudClusterGapPx);
		CSlot->SetPosition(FVector2D(
			GameSpeedRightX,
			IH_BuildPalettePanelStyle::TopRightHudClusterTopY));

		CSlot->SetAutoSize(true);

	}

}



TSharedRef<SWidget> UIH_P1C08_GameSpeedWidget::RebuildWidget()

{

	EnsureWidgetTree();

	return Super::RebuildWidget();

}



void UIH_P1C08_GameSpeedWidget::NativeConstruct()

{

	Super::NativeConstruct();

	EnsureWidgetTree();

	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(1.f, 0.f)); // Plan Addendum 19

	ConfigureSpeedSlider(SpeedSlider);

	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);

	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(ValueText);

	HandleSpeedChanged(SpeedSlider ? SpeedSlider->GetValue() : DefaultSpeed);

	UpdatePanelChrome();

}



TArray<float> UIH_P1C08_GameSpeedWidget::GatherSliderValues() const

{

	return { SpeedSlider ? SpeedSlider->GetValue() : DefaultSpeed };

}



void UIH_P1C08_GameSpeedWidget::SyncSlidersFromValues(const TArray<float>& NormalizedValues)

{

	bSuppressSliderValueChanged = true;

	if (SpeedSlider && NormalizedValues.IsValidIndex(0))

	{

		SpeedSlider->SetValue(NormalizedValues[0]);

		ApplySpeedFromNormalized(NormalizedValues[0]);

	}

	bSuppressSliderValueChanged = false;

}



void UIH_P1C08_GameSpeedWidget::ApplySpeedFromNormalized(float NormalizedValue)

{

	const float Speed = SnapSpeed(NormalizedValue);



	if (SpeedSlider && !FMath::IsNearlyEqual(SpeedSlider->GetValue(), Speed))

	{

		SpeedSlider->SetValue(Speed);

	}



	if (ValueText)

	{

		ValueText->SetText(FText::FromString(FormatSpeedLabel(Speed)));

	}



	if (UWorld* World = GetWorld())

	{

		UGameplayStatics::SetGlobalTimeDilation(World, Speed);

	}

}



void UIH_P1C08_GameSpeedWidget::UpdatePanelChrome()

{

	if (!PanelBorder)

	{

		return;

	}



	FSlateBrush PanelBrush;

	PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;

	PanelBrush.TintColor = FSlateColor(

		UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName(TEXT("PanelBackground")), 0.82f));

	PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

	PanelBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);

	if (bKeyboardPauseActive)

	{

		PanelBrush.OutlineSettings.Color = FLinearColor(1.f, 0.f, 0.f, 1.f);

		PanelBrush.OutlineSettings.Width = PauseFocusBoxWidthPx;

	}

	else

	{

		PanelBrush.OutlineSettings.Color =

			UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));

		PanelBrush.OutlineSettings.Width = 1.f;

	}

	PanelBorder->SetBrush(PanelBrush);

}



void UIH_P1C08_GameSpeedWidget::ToggleKeyboardPause()

{

	if (!SpeedSlider)

	{

		return;

	}



	if (!bKeyboardPauseActive)

	{

		SpeedBeforeKeyboardPause = SnapSpeed(SpeedSlider->GetValue());

		bKeyboardPauseActive = true;

		bSuppressSliderValueChanged = true;

		if (ValueText)

		{

			ValueText->SetText(FText::FromString(TEXT("Paused")));

		}

		bSuppressSliderValueChanged = false;

		if (UWorld* World = GetWorld())

		{

			UGameplayStatics::SetGlobalTimeDilation(World, 0.f);

		}

	}

	else

	{

		bKeyboardPauseActive = false;

		ApplySpeedFromNormalized(SpeedBeforeKeyboardPause);

	}



	UpdatePanelChrome();

}



void UIH_P1C08_GameSpeedWidget::ApplySliderValueFromScreen(USlider* Slider, const FVector2D& ScreenAbsolute)

{

	if (!Slider)

	{

		return;

	}



	const FGeometry& Geo = Slider->GetCachedGeometry();

	const FVector2D Local = Geo.AbsoluteToLocal(ScreenAbsolute);

	const float Width = Geo.GetLocalSize().X;

	const float T = Width > KINDA_SMALL_NUMBER ? FMath::Clamp(Local.X / Width, 0.f, 1.f) : 0.f;

	const float Min = Slider->GetMinValue();

	const float Max = Slider->GetMaxValue();

	float Value = FMath::Lerp(Min, Max, T);

	const float Step = Slider->GetStepSize();

	if (Step > 0.f)

	{

		Value = Min + FMath::RoundToFloat((Value - Min) / Step) * Step;

		Value = FMath::Clamp(Value, Min, Max);

	}

	Slider->SetValue(Value);

}



bool UIH_P1C08_GameSpeedWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const

{

	return PanelBorder && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);

}



bool UIH_P1C08_GameSpeedWidget::IsPointOverSlider(const FVector2D& ScreenAbsolute) const

{

	return SpeedSlider && SpeedSlider->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);

}



void UIH_P1C08_GameSpeedWidget::EnterKeyboardFocus()

{

	KeyboardFocus.BeginFocus(GatherSliderValues(), 0);

}



bool UIH_P1C08_GameSpeedWidget::TryActivateKeyboardFocusFromPanelClick(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute) || IsPointOverSlider(ScreenAbsolute))
	{
		return false;
	}

	EnterKeyboardFocus();
	return true;
}

bool UIH_P1C08_GameSpeedWidget::ProcessPanelPointerDown(const FVector2D& ScreenAbsolute)

{

	return HandleScreenPointerDown(ScreenAbsolute);

}



bool UIH_P1C08_GameSpeedWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)

{

	if (KeyboardFocus.IsActive())

	{

		if (IsPointOverPanel(ScreenAbsolute))

		{

			if (IsPointOverSlider(ScreenAbsolute))

			{

				KeyboardFocus.SetFocusedSliderIndex(0, SliderCount);

				return true;

			}



			EnterKeyboardFocus();

			return true;

		}



		CommitKeyboardFocus(true);

		return false;

	}



	if (IsPointOverSlider(ScreenAbsolute))

	{

		bActiveSliderDrag = true;

		ApplySliderValueFromScreen(SpeedSlider, ScreenAbsolute);

		HandleSliderCaptureBegin();

		return true;

	}



	if (IsPointOverPanel(ScreenAbsolute))

	{

		EnterKeyboardFocus();

		return true;

	}



	return false;

}



void UIH_P1C08_GameSpeedWidget::HandleScreenPointerMove(const FVector2D& ScreenAbsolute)

{

	if (!bActiveSliderDrag || !SpeedSlider)

	{

		return;

	}



	ApplySliderValueFromScreen(SpeedSlider, ScreenAbsolute);

}



void UIH_P1C08_GameSpeedWidget::HandleScreenPointerUp(const FVector2D& ScreenAbsolute)

{

	if (!bActiveSliderDrag || !SpeedSlider)

	{

		return;

	}



	ApplySliderValueFromScreen(SpeedSlider, ScreenAbsolute);

	bActiveSliderDrag = false;

	HandleSliderCaptureEnd();

}



void UIH_P1C08_GameSpeedWidget::TickKeyboardFocusInput(APlayerController* PC, float DeltaTime)

{

	if (!PC || !KeyboardFocus.IsActive())

	{

		return;

	}



	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;



	const auto TryNudge = [&](FKey Key, float& LastNudgeTime) {

		if (!PC->IsInputKeyDown(Key))

		{

			return;

		}



		const bool bJustPressed = PC->WasInputKeyJustPressed(Key);

		if (bJustPressed || (Now - LastNudgeTime) >= NavigationKeyRepeatSec)

		{

			KeyboardFocus.TryHandleNavigationKey(

				Key, !bJustPressed, SliderCount, SpeedSliderStep, SpeedSliderMin, SpeedSliderMax);

			SyncSlidersFromValues(KeyboardFocus.BufferedValues);

			LastNudgeTime = Now;

		}

	};



	TryNudge(EKeys::Left, LastLeftNudgeTime);

	TryNudge(EKeys::Right, LastRightNudgeTime);



	if (PC->WasInputKeyJustPressed(EKeys::Enter))

	{

		CommitKeyboardFocus();

	}

}



void UIH_P1C08_GameSpeedWidget::CancelKeyboardFocus()

{

	if (!KeyboardFocus.IsActive())

	{

		return;

	}



	KeyboardFocus.RevertToCommitted([this](const TArray<float>& Values) {

		SyncSlidersFromValues(Values);

	});

	KeyboardFocus.EndFocus();

}



void UIH_P1C08_GameSpeedWidget::CommitKeyboardFocus(bool bOnlyIfDirty)

{

	if (!KeyboardFocus.IsActive())

	{

		return;

	}



	if (!bOnlyIfDirty || KeyboardFocus.IsDirty())

	{

		KeyboardFocus.CommitToSubsystem([this](const TArray<float>& Values) {

			SyncSlidersFromValues(Values);

		});

	}

	KeyboardFocus.EndFocus();

}



void UIH_P1C08_GameSpeedWidget::HandleSpeedChanged(float Value)

{

	if (bSuppressSliderValueChanged)

	{

		return;

	}



	if (bKeyboardPauseActive)

	{

		bKeyboardPauseActive = false;

		UpdatePanelChrome();

	}



	ApplySpeedFromNormalized(Value);

}



void UIH_P1C08_GameSpeedWidget::HandleSliderCaptureBegin()

{

	if (KeyboardFocus.IsActive())

	{

		CommitKeyboardFocus();

	}

	bSliderCaptureActive = true;

}



void UIH_P1C08_GameSpeedWidget::HandleSliderCaptureEnd()

{

	bSliderCaptureActive = false;

}

