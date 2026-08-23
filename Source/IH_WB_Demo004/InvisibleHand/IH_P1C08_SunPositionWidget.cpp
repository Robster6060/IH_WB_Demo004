// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_SunPositionWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IH_WB_Demo004GameMode.h"
#include "IHUIColorSchemeLibrary.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

void UIH_P1C08_SunPositionWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SunRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SunPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder);

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SunVBox"));

	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("SunTitle"), TEXT("Sun Position"));
	VB->AddChildToVerticalBox(TitleText);

	VB->AddChildToVerticalBox(IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("SunTimeLabel"), TEXT("Time of day")));

	TimeOfDaySlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("SunTimeSlider"));
	TimeOfDaySlider->SetMinValue(0.f);
	TimeOfDaySlider->SetMaxValue(1.f);
	TimeOfDaySlider->SetStepSize(0.01f);
	TimeOfDaySlider->SetValue(0.22f);
	TimeOfDaySlider->OnValueChanged.AddDynamic(this, &UIH_P1C08_SunPositionWidget::HandleTimeOfDayChanged);
	TimeOfDaySlider->OnMouseCaptureBegin.AddDynamic(this, &UIH_P1C08_SunPositionWidget::HandleSliderCaptureBegin);
	TimeOfDaySlider->OnMouseCaptureEnd.AddDynamic(this, &UIH_P1C08_SunPositionWidget::HandleSliderCaptureEnd);
	UIHUIColorSchemeLibrary::ApplyHUDSliderStyle(TimeOfDaySlider);
	if (UVerticalBoxSlot* SliderSlot = VB->AddChildToVerticalBox(TimeOfDaySlider))
	{
		SliderSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	StatusText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("SunStatus"), TEXT(""), FName(TEXT("SecondaryText")));
	if (UVerticalBoxSlot* StatusSlot = VB->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	PanelBorder->AddChild(VB);

	// Plan Addendum 21: force this panel to the same width as Realm Seed/Island Nav (user
	// reported the three top-left panels rendering at different widths despite sharing the same
	// CanvasSlot PanelWidth) - wrapping in a SizeBox with an explicit width override guarantees
	// it regardless of whether the Border's own content happens to be narrower.
	USizeBox* WidthLock = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SunPositionWidthLock"));
	WidthLock->SetWidthOverride(IH_P1C08_DevPanelStyle::PanelWidth);
	WidthLock->AddChild(PanelBorder);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(WidthLock))
	{
		int32 IslandCount = 3;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UIH_WB_Demo004GameInstance* StoryGI = Cast<UIH_WB_Demo004GameInstance>(GI))
			{
				IslandCount = StoryGI->GetProceduralIslandCount();
			}
		}
		IH_P1C08_DevPanelStyle::ConfigureTopLeftPanelSlot(
			CSlot,
			IH_P1C08_DevPanelStyle::EStackSlot::SunPosition,
			IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
				IH_P1C08_DevPanelStyle::EStackSlot::SunPosition, IslandCount),
			IslandCount);
	}
}

void UIH_P1C08_SunPositionWidget::ApplyDevPanelStackPosition(float TopY, float ContentHeight)
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

void UIH_P1C08_SunPositionWidget::UpdatePanelLayout()
{
	int32 IslandCount = 3;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		IslandCount = GI->GetProceduralIslandCount();
	}

	ApplyDevPanelStackPosition(
		IH_P1C08_DevPanelStyle::GetStackPosition(
			IH_P1C08_DevPanelStyle::EStackSlot::SunPosition, IslandCount).Y,
		IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(
			IH_P1C08_DevPanelStyle::EStackSlot::SunPosition, IslandCount));
}

void UIH_P1C08_SunPositionWidget::TogglePanelVisible()
{
	SetPanelVisible(!bPanelVisible);
}

void UIH_P1C08_SunPositionWidget::SetPanelVisible(bool bVisible)
{
	bPanelVisible = bVisible;
	SetVisibility(bPanelVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bPanelVisible)
	{
		SyncSliderFromGameInstance();
	}
}

TSharedRef<SWidget> UIH_P1C08_SunPositionWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_SunPositionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.f, 0.f)); // Plan Addendum 19
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);
	SyncSliderFromGameInstance();
	SetPanelVisible(true);
}

FString UIH_P1C08_SunPositionWidget::FormatTimeOfDayLabel(float TimeOfDay)
{
	const float T = FMath::Clamp(TimeOfDay, 0.f, 1.f);
	if (FMath::IsNearlyEqual(T, 0.f, 0.02f))
	{
		return TEXT("Sunrise");
	}
	if (FMath::IsNearlyEqual(T, 0.5f, 0.02f))
	{
		return TEXT("Noon");
	}
	if (FMath::IsNearlyEqual(T, 1.f, 0.02f))
	{
		return TEXT("Sunset");
	}
	if (T < 0.5f)
	{
		return FString::Printf(TEXT("Morning (%.0f%% to noon)"), (T / 0.5f) * 100.f);
	}
	return FString::Printf(TEXT("Afternoon (%.0f%% to sunset)"), ((T - 0.5f) / 0.5f) * 100.f);
}

void UIH_P1C08_SunPositionWidget::UpdateStatusText(float TimeOfDay)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FormatTimeOfDayLabel(TimeOfDay)));
	}
}

void UIH_P1C08_SunPositionWidget::ApplySunTimeOfDay(float TimeOfDay)
{
	const float ClampedTime = FMath::Clamp(TimeOfDay, 0.f, 1.f);
	if (UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		GI->SetSunTimeOfDay(ClampedTime);
		if (UWorld* World = GetWorld())
		{
			if (AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>())
			{
				GM->ApplySunFromGameInstance();
			}
			else
			{
				constexpr float LowSunPitch = -8.f;
				constexpr float NoonPitch = -80.f;
				constexpr float SunriseYaw = 90.f;
				constexpr float NoonYaw = 0.f;
				constexpr float SunsetYaw = -90.f;
				float Pitch = NoonPitch;
				float Yaw = NoonYaw;
				if (ClampedTime <= 0.5f)
				{
					const float Alpha = ClampedTime / 0.5f;
					Pitch = FMath::Lerp(LowSunPitch, NoonPitch, Alpha);
					Yaw = FMath::Lerp(SunriseYaw, NoonYaw, Alpha);
				}
				else
				{
					const float Alpha = (ClampedTime - 0.5f) / 0.5f;
					Pitch = FMath::Lerp(NoonPitch, LowSunPitch, Alpha);
					Yaw = FMath::Lerp(NoonYaw, SunsetYaw, Alpha);
				}

				for (TActorIterator<ADirectionalLight> It(World); It; ++It)
				{
					if (ADirectionalLight* Sun = *It)
					{
						Sun->SetActorRotation(FRotator(Pitch, Yaw, 0.f));
						break;
					}
				}
			}
		}
	}
	UpdateStatusText(ClampedTime);
}

void UIH_P1C08_SunPositionWidget::SyncSliderFromValue(float TimeOfDay)
{
	bSuppressSliderValueChanged = true;
	if (TimeOfDaySlider)
	{
		TimeOfDaySlider->SetValue(FMath::Clamp(TimeOfDay, 0.f, 1.f));
	}
	bSuppressSliderValueChanged = false;
	UpdateStatusText(TimeOfDay);
}

void UIH_P1C08_SunPositionWidget::SyncSliderFromGameInstance()
{
	float TimeOfDay = 0.22f;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		TimeOfDay = GI->GetSunTimeOfDay();
	}

	bSuppressSliderValueChanged = true;
	if (TimeOfDaySlider)
	{
		TimeOfDaySlider->SetValue(TimeOfDay);
	}
	bSuppressSliderValueChanged = false;
	UpdateStatusText(TimeOfDay);
}

TArray<float> UIH_P1C08_SunPositionWidget::GatherSliderValues() const
{
	return { TimeOfDaySlider ? TimeOfDaySlider->GetValue() : 0.22f };
}

void UIH_P1C08_SunPositionWidget::ApplyPreviewValues(const TArray<float>& NormalizedValues)
{
	if (NormalizedValues.IsValidIndex(0))
	{
		ApplySunTimeOfDay(NormalizedValues[0]);
	}
}

bool UIH_P1C08_SunPositionWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return PanelBorder && bPanelVisible && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_SunPositionWidget::IsPointOverSlider(const FVector2D& ScreenAbsolute) const
{
	return TimeOfDaySlider && TimeOfDaySlider->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_SunPositionWidget::IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return IsPointOverPanel(ScreenAbsolute);
}

void UIH_P1C08_SunPositionWidget::EnterKeyboardFocus()
{
	KeyboardFocus.BeginFocus(GatherSliderValues(), 0);
}

void UIH_P1C08_SunPositionWidget::ApplySliderValueFromScreen(USlider* Slider, const FVector2D& ScreenAbsolute)
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

bool UIH_P1C08_SunPositionWidget::TryActivateKeyboardFocusFromPanelClick(const FVector2D& ScreenAbsolute)
{
	if (!IsPointOverPanel(ScreenAbsolute) || IsPointOverSlider(ScreenAbsolute))
	{
		return false;
	}

	EnterKeyboardFocus();
	return true;
}

bool UIH_P1C08_SunPositionWidget::ProcessPanelPointerDown(const FVector2D& ScreenAbsolute)
{
	return HandleScreenPointerDown(ScreenAbsolute);
}

bool UIH_P1C08_SunPositionWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (!bPanelVisible)
	{
		return false;
	}

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
		ApplySliderValueFromScreen(TimeOfDaySlider, ScreenAbsolute);
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

void UIH_P1C08_SunPositionWidget::HandleScreenPointerMove(const FVector2D& ScreenAbsolute)
{
	if (!bActiveSliderDrag || !TimeOfDaySlider)
	{
		return;
	}

	ApplySliderValueFromScreen(TimeOfDaySlider, ScreenAbsolute);
}

void UIH_P1C08_SunPositionWidget::HandleScreenPointerUp(const FVector2D& ScreenAbsolute)
{
	if (!bActiveSliderDrag || !TimeOfDaySlider)
	{
		return;
	}

	ApplySliderValueFromScreen(TimeOfDaySlider, ScreenAbsolute);
	bActiveSliderDrag = false;
	HandleSliderCaptureEnd();
}

void UIH_P1C08_SunPositionWidget::TickKeyboardFocusInput(APlayerController* PC, float DeltaTime)
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
			KeyboardFocus.TryHandleNavigationKey(Key, !bJustPressed, SliderCount);
			if (KeyboardFocus.BufferedValues.IsValidIndex(0))
			{
				SyncSliderFromValue(KeyboardFocus.BufferedValues[0]);
			}
			ApplyPreviewValues(KeyboardFocus.BufferedValues);
			LastNudgeTime = Now;
		}
	};

	TryNudge(EKeys::Left, LastLeftNudgeTime);
	TryNudge(EKeys::Right, LastRightNudgeTime);

	if (PC->WasInputKeyJustPressed(EKeys::Enter))
	{
		CommitKeyboardFocus();
	}

	KeyboardFocus.TickDebouncedPreview(DeltaTime, [this](const TArray<float>& Values) {
		ApplyPreviewValues(Values);
	});
}

void UIH_P1C08_SunPositionWidget::CancelKeyboardFocus()
{
	if (!KeyboardFocus.IsActive())
	{
		return;
	}

	KeyboardFocus.RevertToCommitted([this](const TArray<float>& Values) {
		if (TimeOfDaySlider && Values.IsValidIndex(0))
		{
			bSuppressSliderValueChanged = true;
			TimeOfDaySlider->SetValue(Values[0]);
			bSuppressSliderValueChanged = false;
		}
		ApplyPreviewValues(Values);
	});
	KeyboardFocus.EndFocus();
	SyncSliderFromGameInstance();
}

void UIH_P1C08_SunPositionWidget::CommitKeyboardFocus(bool bOnlyIfDirty)
{
	if (!KeyboardFocus.IsActive())
	{
		return;
	}

	if (!bOnlyIfDirty || KeyboardFocus.IsDirty())
	{
		KeyboardFocus.CommitToSubsystem([this](const TArray<float>& Values) {
			if (TimeOfDaySlider && Values.IsValidIndex(0))
			{
				bSuppressSliderValueChanged = true;
				TimeOfDaySlider->SetValue(Values[0]);
				bSuppressSliderValueChanged = false;
			}
			ApplyPreviewValues(Values);
		});
	}
	KeyboardFocus.EndFocus();
}

void UIH_P1C08_SunPositionWidget::HandleTimeOfDayChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}

	ApplySunTimeOfDay(Value);
}

void UIH_P1C08_SunPositionWidget::HandleSliderCaptureBegin()
{
	if (KeyboardFocus.IsActive())
	{
		CommitKeyboardFocus();
	}
	bSliderCaptureActive = true;
}

void UIH_P1C08_SunPositionWidget::HandleSliderCaptureEnd()
{
	bSliderCaptureActive = false;
}
