// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_CoastlineTuningWidget.h"

#include "IH_Cube2FlyPlayerController.h"
#include "IH_P1C08_MinimapSubsystem.h"

#include "IH_P1C08_CoastlineTuningSubsystem.h"
#include "IH_P1C08_DevPanelStyle.h"
#include "IH_P1C08_IslandNavSubsystem.h"
#include "IH_P1C08_TemplateGallerySubsystem.h"
#include "IH_WB_Demo004GameInstance.h"
#include "IHIslandTemplateProfileLibrary.h"
#include "IHMapSeedFrameworkTypes.h"

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
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
	static USlider* MakeCoastSlider(
		UWidgetTree* Tree,
		const FName& Name,
		UUserWidget* Owner,
		const FName& ChangedHandler,
		const FName& BeginHandler,
		const FName& EndHandler,
		float DefaultValue)
	{
		USlider* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
		Slider->SetMinValue(0.f);
		Slider->SetMaxValue(1.f);
		Slider->SetStepSize(0.01f);
		Slider->SetValue(DefaultValue);
		UIHUIColorSchemeLibrary::ApplyHUDSliderStyle(Slider);

		if (ChangedHandler != NAME_None)
		{
			FScriptDelegate D;
			D.BindUFunction(Owner, ChangedHandler);
			Slider->OnValueChanged.Add(D);
		}
		if (BeginHandler != NAME_None)
		{
			FScriptDelegate D;
			D.BindUFunction(Owner, BeginHandler);
			Slider->OnMouseCaptureBegin.Add(D);
		}
		if (EndHandler != NAME_None)
		{
			FScriptDelegate D;
			D.BindUFunction(Owner, EndHandler);
			Slider->OnMouseCaptureEnd.Add(D);
		}
		return Slider;
	}
}

float UIH_P1C08_CoastlineTuningWidget::AmplitudeToSlider(float Value)
{
	return FMath::Clamp(Value / 3.f, 0.f, 1.f);
}

float UIH_P1C08_CoastlineTuningWidget::FrequencyToSlider(float Value)
{
	return FMath::Clamp((Value - 0.25f) / 2.75f, 0.f, 1.f);
}

float UIH_P1C08_CoastlineTuningWidget::WarpToSlider(float Value)
{
	return AmplitudeToSlider(Value);
}

float UIH_P1C08_CoastlineTuningWidget::SummitAltitudeToSlider(float Value)
{
	return FMath::Clamp((Value - 0.25f) / 2.25f, 0.f, 1.f);
}

float UIH_P1C08_CoastlineTuningWidget::SummitAltitudeFromSlider(float Slider)
{
	return FMath::Lerp(0.25f, 2.5f, FMath::Clamp(Slider, 0.f, 1.f));
}

TArray<float> UIH_P1C08_CoastlineTuningWidget::ActiveTuningToSliderValues(const FIHIslandCoastlineTuning& Tuning)
{
	return {
		AmplitudeToSlider(Tuning.FbmAmplitudeScale),
		FrequencyToSlider(Tuning.FbmFrequencyScale),
		WarpToSlider(Tuning.DomainWarpStrengthScale),
		WarpToSlider(Tuning.LobeStrengthScale),
		WarpToSlider(Tuning.RippleStrengthScale),
		SummitAltitudeToSlider(Tuning.SummitAltitudeScale),
	};
}

void UIH_P1C08_CoastlineTuningWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CoastRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CoastPanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, IH_P1C08_DevPanelStyle::PanelBackgroundAlpha, true);

	UVerticalBox* OuterVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CoastOuterVBox"));

	UHorizontalBox* HeaderHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CoastHeaderHBox"));
	TitleText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("CoastTitle"), TEXT("Relief — summit altitude"));
	if (UHorizontalBoxSlot* TitleSlot = HeaderHBox->AddChildToHorizontalBox(TitleText))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	FlyoutToggleButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("CoastFlyoutToggleBtn"), TEXT("Fine-tune"),
		FName(TEXT("HandleFlyoutToggleClicked")), true);
	if (UHorizontalBoxSlot* ToggleSlot = HeaderHBox->AddChildToHorizontalBox(FlyoutToggleButton))
	{
		ToggleSlot->SetHorizontalAlignment(HAlign_Right);
		ToggleSlot->SetVerticalAlignment(VAlign_Center);
		ToggleSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	}
	OuterVBox->AddChildToVerticalBox(HeaderHBox);

	ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CoastContentVBox"));

	StatusText = IH_P1C08_DevPanelStyle::MakeHUDLabel(
		WidgetTree, TEXT("CoastStatus"), TEXT("Coast shape is seed height-field (not slider-tuned)."), FName(TEXT("SecondaryText")));
	if (UVerticalBoxSlot* StatusSlot = ContentVBox->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
	}

	auto AddSliderRow = [&](const FName& LabelName, const FString& Label, TObjectPtr<USlider>& OutSlider, const FName& Handler)
	{
		ContentVBox->AddChildToVerticalBox(IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, LabelName, Label));
		OutSlider = MakeCoastSlider(
			WidgetTree, *FString(LabelName.ToString() + TEXT("Slider")), this,
			Handler, FName(TEXT("HandleSliderCaptureBegin")), FName(TEXT("HandleSliderCaptureEnd")), 0.5f);
		if (UVerticalBoxSlot* S = ContentVBox->AddChildToVerticalBox(OutSlider))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	};

	AddSliderRow(TEXT("SummitAltLabel"), TEXT("Summit altitude"), SummitAltitudeSlider, FName(TEXT("HandleSummitAltitudeChanged")));

	ResetToTemplateButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("ResetToTemplateBtn"), TEXT("Reset to template"),
		FName(TEXT("HandleResetToTemplateClicked")), true);
	if (UVerticalBoxSlot* ResetSlot = ContentVBox->AddChildToVerticalBox(ResetToTemplateButton))
	{
		ResetSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		ResetSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ApplyChangesButton = IH_P1C08_DevPanelStyle::MakeHUDButton(
		WidgetTree, this, TEXT("ApplyChangesBtn"), TEXT("Apply Changes"), FName(TEXT("HandleApplyChangesClicked")));
	if (UVerticalBoxSlot* ApplySlot = ContentVBox->AddChildToVerticalBox(ApplyChangesButton))
	{
		ApplySlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		ApplySlot->SetHorizontalAlignment(HAlign_Center);
	}

	OuterVBox->AddChildToVerticalBox(ContentVBox);
	PanelBorder->AddChild(OuterVBox);
	SliderWidgets = { SummitAltitudeSlider };

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
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
			IH_P1C08_DevPanelStyle::EStackSlot::CoastlineTuning,
			IH_P1C08_DevPanelStyle::CoastlineTuningCollapsedHeight,
			IslandCount);
	}
}

void UIH_P1C08_CoastlineTuningWidget::ApplyDevPanelStackPosition(float TopY, float ContentHeight)
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

float UIH_P1C08_CoastlineTuningWidget::GetStackContentHeight() const
{
	return bFlyoutExpanded
		? IH_P1C08_DevPanelStyle::CoastlineTuningExpandedHeight
		: IH_P1C08_DevPanelStyle::CoastlineTuningCollapsedHeight;
}

void UIH_P1C08_CoastlineTuningWidget::UpdatePanelLayout()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC))
		{
			FlyPC->RefreshDevPanelStackLayout();
			return;
		}
	}

	int32 IslandCount = 3;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		IslandCount = GI->GetProceduralIslandCount();
	}

	ApplyDevPanelStackPosition(
		IH_P1C08_DevPanelStyle::GetStackPosition(
			IH_P1C08_DevPanelStyle::EStackSlot::CoastlineTuning, IslandCount).Y,
		GetStackContentHeight());
}

TSharedRef<SWidget> UIH_P1C08_CoastlineTuningWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_CoastlineTuningWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.f, 0.f)); // Plan Addendum 19
	IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(TitleText);
	BindSubsystems();
	UpdateFlyoutVisibility();
	SyncSlidersFromActiveTuning();
}

void UIH_P1C08_CoastlineTuningWidget::NativeDestruct()
{
	UnbindSubsystems();
	Super::NativeDestruct();
}

void UIH_P1C08_CoastlineTuningWidget::BindSubsystems()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			if (!SelectionChangedHandle.IsValid())
			{
				SelectionChangedHandle = Nav->OnSelectionChanged.AddUObject(
					this, &UIH_P1C08_CoastlineTuningWidget::HandleIslandSelectionChanged);
			}
			if (!IslandNavChangedHandle.IsValid())
			{
				IslandNavChangedHandle = Nav->OnIslandNavChanged.AddUObject(
					this, &UIH_P1C08_CoastlineTuningWidget::HandleIslandNavChanged);
			}
		}
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (!CoastlineTuningChangedHandle.IsValid())
			{
				CoastlineTuningChangedHandle = Tuning->OnCoastlineTuningChanged.AddUObject(
					this, &UIH_P1C08_CoastlineTuningWidget::HandleCoastlineTuningChanged);
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::UnbindSubsystems()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
		{
			if (SelectionChangedHandle.IsValid())
			{
				Nav->OnSelectionChanged.Remove(SelectionChangedHandle);
				SelectionChangedHandle.Reset();
			}
			if (IslandNavChangedHandle.IsValid())
			{
				Nav->OnIslandNavChanged.Remove(IslandNavChangedHandle);
				IslandNavChangedHandle.Reset();
			}
		}
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (CoastlineTuningChangedHandle.IsValid())
			{
				Tuning->OnCoastlineTuningChanged.Remove(CoastlineTuningChangedHandle);
				CoastlineTuningChangedHandle.Reset();
			}
		}
	}
}

bool UIH_P1C08_CoastlineTuningWidget::HasUncommittedDraft() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			return Tuning->HasUncommittedDraft();
		}
	}
	return false;
}

void UIH_P1C08_CoastlineTuningWidget::HandleIslandSelectionChanged(int32 IslandIndex)
{
	(void)IslandIndex;
	SyncSlidersFromActiveTuning();
}

void UIH_P1C08_CoastlineTuningWidget::HandleIslandNavChanged()
{
	UpdatePanelTitle();
}

void UIH_P1C08_CoastlineTuningWidget::HandleCoastlineTuningChanged(int32 IslandIndex)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Tuning->GetActiveIslandIndex() == IslandIndex)
			{
				SyncSlidersFromActiveTuning();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::UpdatePanelTitle()
{
	if (!TitleText)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		TitleText->SetText(FText::FromString(TEXT("Coast — auto from seed")));
		UpdateDraftStatusText();
		return;
	}

	UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>();
	if (!Tuning || !Tuning->HasActiveIsland())
	{
		TitleText->SetText(FText::FromString(TEXT("Coast — auto from seed (select island)")));
		UpdateDraftStatusText();
		return;
	}

	const int32 IslandIndex = Tuning->GetActiveIslandIndex();
	FString IslandLabel = FString::Printf(TEXT("Island %d"), IslandIndex + 1);
	if (UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>())
	{
		FIHIslandNavRecord Record;
		if (Nav->TryGetNavRecord(IslandIndex, Record) && !Record.Name.IsEmpty())
		{
			IslandLabel = FString::Printf(TEXT("Island %d: %s"), IslandIndex + 1, *Record.Name);
		}
	}

	FString Title;
	if (bFlyoutExpanded)
	{
		Title = FString::Printf(TEXT("Coastline fine-tune — %s"), *IslandLabel);
	}
	else
	{
		Title = FString::Printf(TEXT("Coast — auto from seed (%s)"), *IslandLabel);
		if (Tuning->HasUncommittedDraft())
		{
			Title += TEXT(" · draft");
		}
	}
	TitleText->SetText(FText::FromString(Title));
	UpdateDraftStatusText();
}

void UIH_P1C08_CoastlineTuningWidget::UpdateDraftStatusText()
{
	if (!StatusText)
	{
		return;
	}

	FString Status;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Tuning->HasActiveIsland())
			{
				Status = Tuning->HasUncommittedDraft() ? TEXT("Status: uncommitted draft") : TEXT("Status: applied");
			}
		}
	}
	StatusText->SetText(FText::FromString(Status));
	StatusText->SetVisibility(
		bFlyoutExpanded && !Status.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UIH_P1C08_CoastlineTuningWidget::UpdateFlyoutVisibility()
{
	if (ContentVBox)
	{
		ContentVBox->SetVisibility(bFlyoutExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (FlyoutToggleButton)
	{
		if (UTextBlock* BtnLabel = Cast<UTextBlock>(FlyoutToggleButton->GetChildAt(0)))
		{
			BtnLabel->SetText(FText::FromString(bFlyoutExpanded ? TEXT("Hide") : TEXT("Fine-tune")));
		}
	}

	if (PanelBorder)
	{
		IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(
			PanelBorder, IH_P1C08_DevPanelStyle::PanelBackgroundAlpha, !bFlyoutExpanded);
	}

	if (!bFlyoutExpanded)
	{
		if (ActiveDragSliderIndex != INDEX_NONE)
		{
			ActiveDragSliderIndex = INDEX_NONE;
			HandleSliderCaptureEnd();
		}
		if (KeyboardFocus.IsActive())
		{
			CancelKeyboardFocus();
		}
	}

	UpdatePanelTitle();
	UpdatePanelLayout();
}

void UIH_P1C08_CoastlineTuningWidget::SyncSlidersFromActiveTuning()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			SyncSlidersFromValues(ActiveTuningToSliderValues(Tuning->GetActiveTuning()));
			UpdatePanelTitle();
			return;
		}
	}
	UpdateDraftStatusText();
}

void UIH_P1C08_CoastlineTuningWidget::RefreshKeyboardFocusHighlight()
{
	Invalidate(EInvalidateWidget::Paint);
}

int32 UIH_P1C08_CoastlineTuningWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (!KeyboardFocus.IsActive())
	{
		return MaxLayer;
	}

	const int32 FocusIndex = KeyboardFocus.GetFocusedSliderIndex();
	if (!SliderWidgets.IsValidIndex(FocusIndex))
	{
		return MaxLayer;
	}

	USlider* FocusedSlider = SliderWidgets[FocusIndex].Get();
	if (!FocusedSlider || !PanelBorder)
	{
		return MaxLayer;
	}

	UWidget* PanelContent = PanelBorder->GetContent();
	if (!PanelContent)
	{
		return MaxLayer;
	}

	const FGeometry& SliderGeo = FocusedSlider->GetCachedGeometry();
	const FGeometry& ContentGeo = PanelContent->GetCachedGeometry();
	if (SliderGeo.GetLocalSize().IsNearlyZero() || ContentGeo.GetLocalSize().IsNearlyZero())
	{
		return MaxLayer;
	}

	const FVector2D ContentAbsMin = ContentGeo.GetAbsolutePosition();
	const FVector2D ContentAbsMax = ContentAbsMin + ContentGeo.GetAbsoluteSize();
	const FVector2D SliderAbsMin = SliderGeo.GetAbsolutePosition();
	const FVector2D SliderAbsMax = SliderAbsMin + SliderGeo.GetAbsoluteSize();
	const FVector2D AbsFocusMin(ContentAbsMin.X, SliderAbsMin.Y);
	const FVector2D AbsFocusMax(ContentAbsMax.X + KeyboardFocusOutlineHorizontalRightPadding, SliderAbsMax.Y);
	const FVector2D VerticalOffset(0.f, KeyboardFocusOutlineVerticalOffset);
	const FVector2D LocalMin = AllottedGeometry.AbsoluteToLocal(AbsFocusMin)
		- FVector2D(KeyboardFocusOutlinePadding, KeyboardFocusOutlinePadding)
		+ VerticalOffset
		+ FVector2D(KeyboardFocusOutlineHorizontalOffset, 0.f);
	const FVector2D LocalMax = AllottedGeometry.AbsoluteToLocal(AbsFocusMax)
		+ FVector2D(KeyboardFocusOutlinePadding, KeyboardFocusOutlinePadding)
		+ VerticalOffset
		+ FVector2D(KeyboardFocusOutlineHorizontalOffset, 0.f);
	const FVector2f BoxSize(LocalMax.X - LocalMin.X, LocalMax.Y - LocalMin.Y);
	if (BoxSize.X < 1.f || BoxSize.Y < 1.f)
	{
		return MaxLayer;
	}

	static const FSlateRoundedBoxBrush FocusOutlineBrush(
		FLinearColor::Transparent,
		FVector4(3.f, 3.f, 3.f, 3.f),
		KeyboardFocusOutlineColor,
		KeyboardFocusOutlineThickness);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer + 1,
		AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(FVector2f(LocalMin))),
		&FocusOutlineBrush,
		ESlateDrawEffect::None,
		FLinearColor::Transparent);

	return MaxLayer + 1;
}

TArray<float> UIH_P1C08_CoastlineTuningWidget::GatherSliderValues() const
{
	return {
		AmplitudeSlider ? AmplitudeSlider->GetValue() : 0.5f,
		FrequencySlider ? FrequencySlider->GetValue() : 0.5f,
		WarpSlider ? WarpSlider->GetValue() : 0.5f,
		LobeSlider ? LobeSlider->GetValue() : 0.5f,
		RippleSlider ? RippleSlider->GetValue() : 0.5f,
		SummitAltitudeSlider ? SummitAltitudeSlider->GetValue() : SummitAltitudeToSlider(1.f),
	};
}

void UIH_P1C08_CoastlineTuningWidget::SyncSlidersFromValues(const TArray<float>& NormalizedValues)
{
	bSuppressSliderValueChanged = true;
	if (AmplitudeSlider && NormalizedValues.IsValidIndex(0)) AmplitudeSlider->SetValue(NormalizedValues[0]);
	if (FrequencySlider && NormalizedValues.IsValidIndex(1)) FrequencySlider->SetValue(NormalizedValues[1]);
	if (WarpSlider && NormalizedValues.IsValidIndex(2)) WarpSlider->SetValue(NormalizedValues[2]);
	if (LobeSlider && NormalizedValues.IsValidIndex(3)) LobeSlider->SetValue(NormalizedValues[3]);
	if (RippleSlider && NormalizedValues.IsValidIndex(4)) RippleSlider->SetValue(NormalizedValues[4]);
	if (SummitAltitudeSlider && NormalizedValues.IsValidIndex(5)) SummitAltitudeSlider->SetValue(NormalizedValues[5]);
	bSuppressSliderValueChanged = false;
}

void UIH_P1C08_CoastlineTuningWidget::ApplyPreviewValues(const TArray<float>& NormalizedValues)
{
	if (NormalizedValues.IsValidIndex(0)) HandleAmplitudeChanged(NormalizedValues[0]);
	if (NormalizedValues.IsValidIndex(1)) HandleFrequencyChanged(NormalizedValues[1]);
	if (NormalizedValues.IsValidIndex(2)) HandleWarpChanged(NormalizedValues[2]);
	if (NormalizedValues.IsValidIndex(3)) HandleLobeChanged(NormalizedValues[3]);
	if (NormalizedValues.IsValidIndex(4)) HandleRippleChanged(NormalizedValues[4]);
	if (NormalizedValues.IsValidIndex(5)) HandleSummitAltitudeChanged(NormalizedValues[5]);
	UpdatePanelTitle();
}

bool UIH_P1C08_CoastlineTuningWidget::IsPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return PanelBorder && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_CoastlineTuningWidget::IsPointOverAnySlider(const FVector2D& ScreenAbsolute) const
{
	if (!bFlyoutExpanded)
	{
		return false;
	}
	for (const TObjectPtr<USlider>& Slider : SliderWidgets)
	{
		if (Slider && Slider->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
		{
			return true;
		}
	}
	return false;
}

bool UIH_P1C08_CoastlineTuningWidget::IsPointOverApplyButton(const FVector2D& ScreenAbsolute) const
{
	return bFlyoutExpanded && ApplyChangesButton && ApplyChangesButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_CoastlineTuningWidget::IsPointOverFlyoutToggle(const FVector2D& ScreenAbsolute) const
{
	return FlyoutToggleButton && FlyoutToggleButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

bool UIH_P1C08_CoastlineTuningWidget::IsPointOverResetButton(const FVector2D& ScreenAbsolute) const
{
	return bFlyoutExpanded && ResetToTemplateButton && ResetToTemplateButton->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

void UIH_P1C08_CoastlineTuningWidget::ApplySliderValueFromScreen(USlider* Slider, const FVector2D& ScreenAbsolute)
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

void UIH_P1C08_CoastlineTuningWidget::EnterKeyboardFocus()
{
	KeyboardFocus.BeginFocus(GatherSliderValues(), 0);
	RefreshKeyboardFocusHighlight();
}

bool UIH_P1C08_CoastlineTuningWidget::TryActivateKeyboardFocusFromPanelClick(const FVector2D& ScreenAbsolute)
{
	if (!bFlyoutExpanded)
	{
		return false;
	}
	if (!IsPointOverPanel(ScreenAbsolute) || IsPointOverAnySlider(ScreenAbsolute) || IsPointOverApplyButton(ScreenAbsolute)
		|| IsPointOverFlyoutToggle(ScreenAbsolute) || IsPointOverResetButton(ScreenAbsolute))
	{
		return false;
	}
	EnterKeyboardFocus();
	return true;
}

bool UIH_P1C08_CoastlineTuningWidget::ProcessPanelPointerDown(const FVector2D& ScreenAbsolute)
{
	return HandleScreenPointerDown(ScreenAbsolute);
}

bool UIH_P1C08_CoastlineTuningWidget::HandleScreenPointerDown(const FVector2D& ScreenAbsolute)
{
	if (IsPointOverFlyoutToggle(ScreenAbsolute))
	{
		HandleFlyoutToggleClicked();
		return true;
	}

	if (IsPointOverResetButton(ScreenAbsolute))
	{
		HandleResetToTemplateClicked();
		return true;
	}

	if (IsPointOverApplyButton(ScreenAbsolute))
	{
		ApplyChanges();
		return true;
	}

	if (!bFlyoutExpanded)
	{
		return IsPointOverPanel(ScreenAbsolute);
	}

	if (KeyboardFocus.IsActive())
	{
		if (IsPointOverPanel(ScreenAbsolute))
		{
			for (int32 Index = 0; Index < SliderWidgets.Num(); ++Index)
			{
				if (USlider* Slider = SliderWidgets[Index].Get())
				{
					if (Slider->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
					{
						KeyboardFocus.SetFocusedSliderIndex(Index, SliderCount);
						RefreshKeyboardFocusHighlight();
						return true;
					}
				}
			}
			if (!IsPointOverAnySlider(ScreenAbsolute))
			{
				EnterKeyboardFocus();
			}
			return true;
		}
		KeyboardFocus.EndFocus();
		RefreshKeyboardFocusHighlight();
		return false;
	}

	for (int32 Index = 0; Index < SliderWidgets.Num(); ++Index)
	{
		USlider* Slider = SliderWidgets[Index].Get();
		if (Slider && Slider->GetCachedGeometry().IsUnderLocation(ScreenAbsolute))
		{
			ActiveDragSliderIndex = Index;
			ApplySliderValueFromScreen(Slider, ScreenAbsolute);
			HandleSliderCaptureBegin();
			return true;
		}
	}

	if (IsPointOverPanel(ScreenAbsolute))
	{
		EnterKeyboardFocus();
		return true;
	}

	return false;
}

void UIH_P1C08_CoastlineTuningWidget::HandleScreenPointerMove(const FVector2D& ScreenAbsolute)
{
	if (ActiveDragSliderIndex == INDEX_NONE)
	{
		return;
	}
	if (USlider* Slider = SliderWidgets.IsValidIndex(ActiveDragSliderIndex) ? SliderWidgets[ActiveDragSliderIndex].Get() : nullptr)
	{
		ApplySliderValueFromScreen(Slider, ScreenAbsolute);
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleScreenPointerUp(const FVector2D& ScreenAbsolute)
{
	if (ActiveDragSliderIndex == INDEX_NONE)
	{
		return;
	}
	if (USlider* Slider = SliderWidgets.IsValidIndex(ActiveDragSliderIndex) ? SliderWidgets[ActiveDragSliderIndex].Get() : nullptr)
	{
		ApplySliderValueFromScreen(Slider, ScreenAbsolute);
	}
	ActiveDragSliderIndex = INDEX_NONE;
	HandleSliderCaptureEnd();
}

void UIH_P1C08_CoastlineTuningWidget::TickKeyboardFocusInput(APlayerController* PC, float DeltaTime)
{
	if (!PC || !KeyboardFocus.IsActive())
	{
		return;
	}

	const int32 PrevFocusedIndex = KeyboardFocus.GetFocusedSliderIndex();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bShiftDown = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);

	const auto TryNudge = [&](FKey Key, float& LastNudgeTime)
	{
		if (!PC->IsInputKeyDown(Key))
		{
			return;
		}
		const bool bJustPressed = PC->WasInputKeyJustPressed(Key);
		if (bJustPressed || (Now - LastNudgeTime) >= NavigationKeyRepeatSec)
		{
			KeyboardFocus.TryHandleNavigationKey(Key, !bJustPressed, SliderCount);
			SyncSlidersFromValues(KeyboardFocus.BufferedValues);
			LastNudgeTime = Now;
		}
	};

	if (bShiftDown)
	{
		const auto TryIslandNudge = [&](FKey Key, const FVector2D& Delta)
		{
			if (!PC->WasInputKeyJustPressed(Key))
			{
				return;
			}
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
				{
					Tuning->NudgeDraftManualOffsetCm(Delta);
				}
			}
		};
		TryIslandNudge(EKeys::Up, FVector2D(0.f, IslandNudgeCm));
		TryIslandNudge(EKeys::Down, FVector2D(0.f, -IslandNudgeCm));
		TryIslandNudge(EKeys::Left, FVector2D(-IslandNudgeCm, 0.f));
		TryIslandNudge(EKeys::Right, FVector2D(IslandNudgeCm, 0.f));
	}
	else
	{
		TryNudge(EKeys::Left, LastLeftNudgeTime);
		TryNudge(EKeys::Right, LastRightNudgeTime);
		if (PC->WasInputKeyJustPressed(EKeys::Up))
		{
			KeyboardFocus.TryHandleNavigationKey(EKeys::Up, false, SliderCount);
			SyncSlidersFromValues(KeyboardFocus.BufferedValues);
		}
		if (PC->WasInputKeyJustPressed(EKeys::Down))
		{
			KeyboardFocus.TryHandleNavigationKey(EKeys::Down, false, SliderCount);
			SyncSlidersFromValues(KeyboardFocus.BufferedValues);
		}
	}

	if (PC->WasInputKeyJustPressed(EKeys::Enter))
	{
		ApplyChanges();
	}

	KeyboardFocus.FlushPreview([this](const TArray<float>& Values) {
		ApplyPreviewValues(Values);
	});

	if (KeyboardFocus.GetFocusedSliderIndex() != PrevFocusedIndex)
	{
		RefreshKeyboardFocusHighlight();
	}
}

void UIH_P1C08_CoastlineTuningWidget::CancelKeyboardFocus()
{
	if (!KeyboardFocus.IsActive())
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			Tuning->RevertActiveDraft();
		}
	}

	KeyboardFocus.RevertToCommitted([this](const TArray<float>& Values) {
		SyncSlidersFromValues(Values);
	});
	KeyboardFocus.EndFocus();
	SyncSlidersFromActiveTuning();
	RefreshKeyboardFocusHighlight();
}

void UIH_P1C08_CoastlineTuningWidget::CommitKeyboardFocus(bool bOnlyIfDirty)
{
	if (!KeyboardFocus.IsActive())
	{
		return;
	}

	if (!bOnlyIfDirty || KeyboardFocus.IsDirty())
	{
		KeyboardFocus.CommitToSubsystem([this](const TArray<float>& Values) {
			SyncSlidersFromValues(Values);
			ApplyPreviewValues(Values);
		});
	}
	KeyboardFocus.EndFocus();
	RefreshKeyboardFocusHighlight();
}

void UIH_P1C08_CoastlineTuningWidget::ApplyChanges()
{
	CommitActiveDraftOnly();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC))
			{
				FlyPC->SetIslandSelectionVisualVisible(false);
			}
		}
		if (UIH_P1C08_MinimapSubsystem* Minimap = GI->GetSubsystem<UIH_P1C08_MinimapSubsystem>())
		{
			Minimap->RequestMinimapRepaint();
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::CommitActiveDraftOnly()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (KeyboardFocus.IsActive())
			{
				KeyboardFocus.CommitToSubsystem([this](const TArray<float>& Values) {
					SyncSlidersFromValues(Values);
					ApplyPreviewValues(Values);
				});
				KeyboardFocus.EndFocus();
				RefreshKeyboardFocusHighlight();
			}
			if (!Tuning->HasUncommittedDraft())
			{
				return;
			}
			Tuning->ApplyActiveDraft();
			SyncSlidersFromActiveTuning();
			UpdateDraftStatusText();
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleApplyChangesClicked()
{
	ApplyChanges();
}

void UIH_P1C08_CoastlineTuningWidget::HandleFlyoutToggleClicked()
{
	bFlyoutExpanded = !bFlyoutExpanded;
	UpdateFlyoutVisibility();
}

void UIH_P1C08_CoastlineTuningWidget::HandleResetToTemplateClicked()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UIH_P1C08_CoastlineTuningSubsystem* Tuning = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>();
	UIH_P1C08_IslandNavSubsystem* Nav = GI->GetSubsystem<UIH_P1C08_IslandNavSubsystem>();
	const UIH_WB_Demo004GameInstance* CellsGI = Cast<UIH_WB_Demo004GameInstance>(GI);
	if (!Tuning || !Nav || !CellsGI || !Tuning->HasActiveIsland() || !CellsGI->GetMapSeedPhase1().bSuccess)
	{
		return;
	}

	const int32 IslandIndex = Tuning->GetActiveIslandIndex();
	EIHIslandTemplateType TemplateType = EIHIslandTemplateType::Low;
	bool bFoundPlan = false;
	for (const FIHIslandSpawnPlan& Plan : CellsGI->GetMapSeedPhase1().SpawnPlans)
	{
		if (Plan.IslandIndex == IslandIndex)
		{
			TemplateType = Plan.TemplateType;
			bFoundPlan = true;
			break;
		}
	}
	if (!bFoundPlan)
	{
		return;
	}

	FIHIslandTemplateProfileV1 Profile;
	const UIH_P1C08_TemplateGallerySubsystem* Gallery = GI->GetSubsystem<UIH_P1C08_TemplateGallerySubsystem>();
	if (!Gallery || !Gallery->GetProfileForTemplate(TemplateType, Profile))
	{
		return;
	}

	Nav->SetCommittedCoastlineTuningFromProfile(
		IslandIndex, UIHIslandTemplateProfileLibrary::MakeCoastlineTuningFromProfile(Profile));
	Tuning->RevertActiveDraft();
	SyncSlidersFromActiveTuning();
}

void UIH_P1C08_CoastlineTuningWidget::HandleAmplitudeChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Sub = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Sub->HasActiveIsland())
			{
				Sub->SetFbmAmplitudeScale(FMath::Lerp(0.f, 3.f, Value));
				UpdatePanelTitle();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleFrequencyChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Sub = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Sub->HasActiveIsland())
			{
				Sub->SetFbmFrequencyScale(FMath::Lerp(0.25f, 3.f, Value));
				UpdatePanelTitle();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleWarpChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Sub = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Sub->HasActiveIsland())
			{
				Sub->SetDomainWarpStrengthScale(FMath::Lerp(0.f, 3.f, Value));
				UpdatePanelTitle();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleLobeChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Sub = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Sub->HasActiveIsland())
			{
				Sub->SetLobeStrengthScale(FMath::Lerp(0.f, 3.f, Value));
				UpdatePanelTitle();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleRippleChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Sub = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Sub->HasActiveIsland())
			{
				Sub->SetRippleStrengthScale(FMath::Lerp(0.f, 3.f, Value));
				UpdatePanelTitle();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleSummitAltitudeChanged(float Value)
{
	if (bSuppressSliderValueChanged)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UIH_P1C08_CoastlineTuningSubsystem* Sub = GI->GetSubsystem<UIH_P1C08_CoastlineTuningSubsystem>())
		{
			if (Sub->HasActiveIsland())
			{
				Sub->SetSummitAltitudeScale(SummitAltitudeFromSlider(Value));
				UpdatePanelTitle();
			}
		}
	}
}

void UIH_P1C08_CoastlineTuningWidget::HandleSliderCaptureBegin()
{
	bSliderCaptureActive = true;
}

void UIH_P1C08_CoastlineTuningWidget::HandleSliderCaptureEnd()
{
	bSliderCaptureActive = false;
}
