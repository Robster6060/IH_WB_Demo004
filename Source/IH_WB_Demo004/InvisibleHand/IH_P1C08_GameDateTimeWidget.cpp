// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_GameDateTimeWidget.h"

#include "IH_P1C08_DevPanelStyle.h"
#include "IH_WB_Demo004GameInstance.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

void UIH_P1C08_GameDateTimeWidget::EnsureWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameDateTimeRootCanvas"));
	WidgetTree->RootWidget = Canvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameDateTimePanel"));
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(PanelBorder, IH_P1C08_DevPanelStyle::PanelBackgroundAlpha, /*bCompactPadding=*/true);

	DateTimeText = IH_P1C08_DevPanelStyle::MakeHUDLabel(WidgetTree, TEXT("GameDateTimeLabel"), TEXT(""));
	PanelBorder->AddChild(DateTimeText);

	if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(PanelBorder))
	{
		// Right edge sits at screen-center, so Play Atmospherics (left edge at center) sits
		// immediately to the right of this panel regardless of either one's rendered width.
		IH_P1C08_DevPanelStyle::ApplyTopCenterPanelSlot(CSlot, IH_P1C08_DevPanelStyle::TopMargin, 0.f, 1.f);
	}
}

void UIH_P1C08_GameDateTimeWidget::TogglePanelVisible()
{
	SetPanelVisible(!bPanelVisible);
}

void UIH_P1C08_GameDateTimeWidget::SetPanelVisible(bool bVisible)
{
	bPanelVisible = bVisible;
	SetVisibility(bPanelVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bPanelVisible)
	{
		RefreshFromGameInstance();
	}
}

void UIH_P1C08_GameDateTimeWidget::UpdatePanelLayout()
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
				IH_P1C08_DevPanelStyle::ApplyTopCenterPanelSlot(CSlot, IH_P1C08_DevPanelStyle::TopMargin, 0.f, 1.f);
			}
		}
	}
}

bool UIH_P1C08_GameDateTimeWidget::IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const
{
	return PanelBorder && bPanelVisible && PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenAbsolute);
}

TSharedRef<SWidget> UIH_P1C08_GameDateTimeWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UIH_P1C08_GameDateTimeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	IH_P1C08_DevPanelStyle::ApplyDevHudCornerScale(this, FVector2D(0.5f, 0.f));
	RefreshFromGameInstance();
	SetPanelVisible(true);
}

FString UIH_P1C08_GameDateTimeWidget::SeasonLabelFromMonth(int32 Month)
{
	switch (Month)
	{
	case 12: case 1: case 2: return TEXT("Winter");
	case 3: case 4: case 5: return TEXT("Spring");
	case 6: case 7: case 8: return TEXT("Summer");
	default: return TEXT("Autumn");
	}
}

const TCHAR* UIH_P1C08_GameDateTimeWidget::MonthAbbrev(int32 Month)
{
	static const TCHAR* Names[12] = {
		TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"),
		TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")
	};
	return Names[FMath::Clamp(Month, 1, 12) - 1];
}

FText UIH_P1C08_GameDateTimeWidget::TimeBracketLabel(EIHTimeBracket Bracket)
{
	switch (Bracket)
	{
	case EIHTimeBracket::Midnight: return FText::FromString(TEXT("Midnight"));
	case EIHTimeBracket::Daybreak: return FText::FromString(TEXT("Daybreak"));
	case EIHTimeBracket::Sunrise: return FText::FromString(TEXT("Sunrise"));
	case EIHTimeBracket::Midmorning: return FText::FromString(TEXT("Midmorning"));
	case EIHTimeBracket::HighNoon: return FText::FromString(TEXT("High-Noon"));
	case EIHTimeBracket::Afternoon: return FText::FromString(TEXT("Afternoon"));
	case EIHTimeBracket::Eventide: return FText::FromString(TEXT("Eventide"));
	case EIHTimeBracket::Sunset: return FText::FromString(TEXT("Sunset"));
	case EIHTimeBracket::Twilight: return FText::FromString(TEXT("Twilight"));
	case EIHTimeBracket::Nightfall: return FText::FromString(TEXT("Nightfall"));
	default: return FText::FromString(TEXT("—"));
	}
}

void UIH_P1C08_GameDateTimeWidget::RefreshFromGameInstance()
{
	if (!DateTimeText)
	{
		return;
	}

	int32 Year = 1000, Month = 4, Day = 1;
	EIHTimeBracket Bracket = EIHTimeBracket::Afternoon;
	if (const UIH_WB_Demo004GameInstance* GI = GetGameInstance<UIH_WB_Demo004GameInstance>())
	{
		Year = GI->GetRealmYear();
		Month = GI->GetRealmMonth();
		Day = GI->GetRealmDay();
		Bracket = GI->GetRealmHourBracket();
	}

	const FString Formatted = FString::Printf(
		TEXT("Year %d  |  %s  |  %s  |  Day %d  |  %s"),
		Year, *SeasonLabelFromMonth(Month), MonthAbbrev(Month), Day, *TimeBracketLabel(Bracket).ToString());
	DateTimeText->SetText(FText::FromString(Formatted));
}
