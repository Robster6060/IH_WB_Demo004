// Copyright Epic Games, Inc. All Rights Reserved.
// Starter HUD color scheme (UIColorHUDStartingScheme.csv) — sRGB hex → linear for Canvas / Slate.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_WB_Demo004.h"

class USlider;

/**
 * Lookup for the **starter** HUD/UI palette. Keys are PascalCase, no spaces (see UI_HUD_Starting_Color_Scheme_Table.xlsx).
 * Future schemes can add parallel tables or load from DataTable; call sites should use these APIs rather than hardcoded FLinearColor.
 */
class IH_WB_DEMO004_API UIHUIColorSchemeLibrary
{
public:
	/** Returns the color for RoleKey, or White if unknown. */
	static FLinearColor GetHUDStartingColor(FName RoleKey);

	/** RGB from scheme, alpha overridden (e.g. translucent overlay panels). */
	static FLinearColor GetHUDStartingColorWithAlpha(FName RoleKey, float Alpha);

	/** Multiply RGB by Scale (A unchanged unless bScaleAlpha). */
	static FLinearColor ScaleRGB(FLinearColor C, float Scale, bool bScaleAlpha = false);

	/** Apply canonical slider track/thumb tints to a programmatic HUD slider. */
	static void ApplyHUDSliderStyle(USlider* Slider);
};
