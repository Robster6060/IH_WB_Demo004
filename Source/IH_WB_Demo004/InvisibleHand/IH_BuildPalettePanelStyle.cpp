// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_BuildPalettePanelStyle.h"

#include "IH_P1C08_DevPanelStyle.h"

#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"

void IH_BuildPalettePanelStyle::ApplyTopRightPanelCanvasSlot(
	UCanvasPanelSlot* Slot, float TopX, float TopY, float PanelW, float PanelH)
{
	if (!Slot)
	{
		return;
	}

	Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	Slot->SetAlignment(FVector2D(0.f, 0.f));
	Slot->SetPosition(FVector2D(TopX, TopY));
	Slot->SetAutoSize(false);
	Slot->SetSize(FVector2D(FMath::Max(PanelW, 1.f), FMath::Max(PanelH, 1.f)));
}

void IH_BuildPalettePanelStyle::ApplyRightFlyOutBorderStyle(UBorder* Border, bool bFocusOutline)
{
	if (!Border)
	{
		return;
	}
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(Border, IH_P1C08_DevPanelStyle::PanelBackgroundAlpha, true);

	if (bFocusOutline)
	{
		FSlateBrush PanelBrush = Border->Background;
		PanelBrush.OutlineSettings.Color = FSlateColor(FocusBlue);
		PanelBrush.OutlineSettings.Width = 2.f;
		Border->SetBrush(PanelBrush);
	}
}

void IH_BuildPalettePanelStyle::ApplyRightTabStripBorderStyle(UBorder* Border)
{
	if (!Border)
	{
		return;
	}
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(Border, IH_P1C08_DevPanelStyle::PanelBackgroundAlpha * 0.85f, true);
}

void IH_BuildPalettePanelStyle::ApplyGridTemplateTileBorderStyle(UBorder* Border)
{
	if (!Border)
	{
		return;
	}
	IH_P1C08_DevPanelStyle::ApplyPanelBorderStyle(Border, 0.92f, true);
	FSlateBrush Brush = Border->Background;
	Brush.OutlineSettings.Color = FSlateColor(FocusBlue);
	Brush.OutlineSettings.Width = 1.f;
	Border->SetBrush(Brush);
}
