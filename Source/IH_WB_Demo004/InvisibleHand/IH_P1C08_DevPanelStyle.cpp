// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_DevPanelStyle.h"

#include "IHUIColorSchemeLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	float StackContentHeightBefore(IH_P1C08_DevPanelStyle::EStackSlot Slot, int32 IslandCount)
	{
		float Y = IH_P1C08_DevPanelStyle::TopMargin;
		for (uint8 Index = 0; Index < static_cast<uint8>(Slot); ++Index)
		{
			const IH_P1C08_DevPanelStyle::EStackSlot PriorSlot = static_cast<IH_P1C08_DevPanelStyle::EStackSlot>(Index);
			Y += IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(PriorSlot, IslandCount)
				+ IH_P1C08_DevPanelStyle::PanelSpacing;
		}
		return Y;
	}
}

float IH_P1C08_DevPanelStyle::GetStandardStackContentHeight(EStackSlot Slot, int32 IslandCount)
{
	switch (Slot)
	{
	case EStackSlot::RealmSeed:
		// Plan Addendum 20: +60 to fit the status line ("Regenerated N from SEED - templates
		// [...]") now that it wraps (SetAutoWrapText) instead of overflowing the panel width -
		// the longest realistic message (7 islands, full template list) wraps to ~2-3 lines.
		return 278.f;
	case EStackSlot::IslandNav:
	{
		const int32 Count = FMath::Clamp(IslandCount, 2, 7);
		return IslandNavFixedHeight + static_cast<float>(Count) * IslandNavRowHeight;
	}
	case EStackSlot::TemplateGallery:
		return 118.f;
	case EStackSlot::CoastlineTuning:
		return CoastlineTuningCollapsedHeight;
	case EStackSlot::SunPosition:
		return 132.f;
	default:
		return 80.f;
	}
}

FVector2D IH_P1C08_DevPanelStyle::GetStackPosition(EStackSlot Slot, int32 IslandCount)
{
	return FVector2D(LeftMargin, StackContentHeightBefore(Slot, IslandCount));
}

void IH_P1C08_DevPanelStyle::ApplyTopLeftPanelSlotAtY(
	UCanvasPanelSlot* Slot, float TopY, float ContentHeight)
{
	if (!Slot)
	{
		return;
	}

	Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	Slot->SetAlignment(FVector2D(0.f, 0.f));
	Slot->SetPosition(FVector2D(LeftMargin, TopY));
	Slot->SetAutoSize(false);
	Slot->SetSize(FVector2D(PanelWidth, ContentHeight));
}

void IH_P1C08_DevPanelStyle::ConfigureTopLeftPanelSlot(
	UCanvasPanelSlot* Slot, EStackSlot StackSlot, float ContentHeight, int32 IslandCount)
{
	if (!Slot)
	{
		return;
	}

	ApplyTopLeftPanelSlotAtY(Slot, GetStackPosition(StackSlot, IslandCount).Y, ContentHeight);
}

void IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(UBorder* Border, float BackgroundAlpha, bool bCompactPadding)
{
	if (!Border)
	{
		return;
	}

	Border->SetPadding(bCompactPadding
		? FMargin(CompactPanelPaddingH, CompactPanelPaddingV)
		: FMargin(12.f, 10.f));

	FSlateBrush PanelBrush;
	PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PanelBrush.TintColor = FSlateColor(
		UIHUIColorSchemeLibrary::GetHUDStartingColorWithAlpha(FName(TEXT("PanelBackground")), BackgroundAlpha));
	PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	PanelBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
	PanelBrush.OutlineSettings.Color = UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("BorderFrameTint")));
	PanelBrush.OutlineSettings.Width = 1.f;
	Border->SetBrush(PanelBrush);
}

void IH_P1C08_DevPanelStyle::ApplyHUDLabelFont(UTextBlock* TextBlock, int32 Size)
{
	if (TextBlock)
	{
		TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
	}
}

UTextBlock* IH_P1C08_DevPanelStyle::MakeHUDLabel(
	UWidgetTree* Tree,
	const FName& Name,
	const FString& Text,
	FName ColorRole)
{
	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Label->SetText(FText::FromString(Text));
	Label->SetColorAndOpacity(FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(ColorRole)));
	ApplyHUDLabelFont(Label);
	return Label;
}

UButton* IH_P1C08_DevPanelStyle::MakeHUDButton(
	UWidgetTree* Tree,
	UUserWidget* Owner,
	const FName& Name,
	const FString& Label,
	const FName& ClickHandler,
	bool bCompact)
{
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* BtnLabel = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("_Label"))));
	BtnLabel->SetText(FText::FromString(Label));
	BtnLabel->SetColorAndOpacity(FSlateColor(UIHUIColorSchemeLibrary::GetHUDStartingColor(FName(TEXT("ButtonNormalText")))));
	ApplyHUDLabelFont(BtnLabel, bCompact ? CompactLabelFontSize : LabelFontSize);
	Button->AddChild(BtnLabel);

	if (bCompact)
	{
		FButtonStyle ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		ButtonStyle.NormalPadding = FMargin(6.f, 2.f);
		ButtonStyle.PressedPadding = FMargin(6.f, 3.f);
		Button->SetStyle(ButtonStyle);
	}

	if (ClickHandler != NAME_None && Owner)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Owner, ClickHandler);
		Button->OnClicked.Add(Delegate);
	}

	return Button;
}

void IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(UUserWidget* Widget, const FVector2D& CornerPivot, float Scale)
{
	if (!Widget)
	{
		return;
	}
	Widget->SetRenderTransformPivot(CornerPivot);
	Widget->SetRenderScale(FVector2D(Scale, Scale));
}
