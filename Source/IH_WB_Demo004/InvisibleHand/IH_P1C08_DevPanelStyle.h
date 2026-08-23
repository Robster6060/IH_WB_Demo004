// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"

class UBorder;
class UButton;
class UCanvasPanelSlot;
class UTextBlock;
class UWidgetTree;

/** Shared layout + styling for top-left pre-bake dev HUD panels (Phase 5b). */
namespace IH_P1C08_DevPanelStyle
{
	static constexpr float LeftMargin = 20.f;
	static constexpr float TopMargin = 20.f;
	// Plan Addendum 20: widened 340->440 to fit the Island Nav Name column (see
	// IslandNavColNameWidth) sized for the longest real geographical name
	// ("Hodmezovasarhely", 20 chars, IH_Geographical_Names.csv) without crowding, and to give
	// the Realm Seed panel's status text room before wrapping. Shared by every top-left panel
	// (ApplyTopLeftPanelSlotAtY), so this widens the whole stack uniformly, not just one panel.
	// Addendum 21 follow-up: 440 was computed too tight - summing the 5 Island Nav column
	// widths (28+180+96+46+54=404) + 4 inter-column gaps (16) + border padding (24) = 444,
	// already exceeding 440, so Acres was clipping against the panel's own right edge. Widened
	// to 480 for real headroom instead of another exact-fit guess.
	static constexpr float PanelWidth = 480.f;
	static constexpr float IslandNavColIslandWidth = 28.f;
	// Plan Addendum 21: tightened 200->180 - the prior estimate left a visible gap before the
	// Origin column (user-reported). Sized for the longest real geographical name
	// ("Hodmezovasarhely", 20 chars, IH_Geographical_Names.csv) at LabelFontSize (16), ~9px/char.
	static constexpr float IslandNavColNameWidth = 180.f;
	static constexpr float IslandNavColOriginWidth = 96.f;
	// Widened 36->46 so "High"/"Volc" (the widest Type abbreviations) fit within the nominal
	// column width at LabelFontSize=16 instead of overflowing it and throwing off the
	// (already-Center-justified) alignment.
	static constexpr float IslandNavColShapeWidth = 46.f;
	// Plan Addendum 20: bumped proportionally to the LabelFontSize 12->16 increase (row height
	// was sized for the smaller pre-bump font and read as crowded at the larger size once
	// stacked across the full 2-7 island range). The panel already grows dynamically with
	// island count (GetStandardStackContentHeight's IslandNav case, triggered by
	// UIH_P1C08_IslandNavSubsystem::OnIslandNavChanged) - this is a per-row budget fix, not a
	// new growth mechanism.
	static constexpr float IslandNavRowHeight = 38.f;
	static constexpr float IslandNavFixedHeight = 62.f;
	// Widened for a right-justify margin at LabelFontSize=16 (5-digit acreage plus breathing room).
	static constexpr float IslandNavColSectorsWidth = 54.f;
	static constexpr float PanelSpacing = 12.f;
	/** Collapsed coast fine-tune bar (seed-driven default; sliders hidden). */
	static constexpr float CoastlineTuningCollapsedHeight = 40.f;
	/** Expanded coast fine-tune panel (dev sliders). */
	static constexpr float CoastlineTuningExpandedHeight = 300.f;
	// Plan Addendum 19 follow-up: bumped 1.3x (12->16, 11->14, 10->13) after the 0.5x
	// SetRenderScale made text render soft/small - source font size increases so the effective
	// on-screen result is larger, not just a scale-transform workaround.
	static constexpr int32 LabelFontSize = 16;
	static constexpr int32 CompactLabelFontSize = 14;
	static constexpr int32 OriginComboFontSize = 13;
	static constexpr float CompactPanelPaddingH = 10.f;
	static constexpr float CompactPanelPaddingV = 6.f;
	static constexpr float PanelBackgroundAlpha = 0.85f;
	static constexpr float RowSelectionOutlineThickness = 2.f;
	static const FLinearColor RowSelectionOutlineColor = FLinearColor(0.4f, 0.75f, 1.0f, 1.f);
	/** Island Nav origin combo: active / inactive text on dark dropdown field. */
	static const FLinearColor IslandNavOriginActiveTextColor = RowSelectionOutlineColor;
	static const FLinearColor IslandNavOriginInactiveTextColor = FLinearColor(0.62f, 0.65f, 0.68f, 1.f);

	enum class EStackSlot : uint8
	{
		RealmSeed = 0,
		IslandNav = 1,
		TemplateGallery = 2,
		CoastlineTuning = 3,
		// Plan Addendum 20: Water Choppiness widget removed at user request (unused). Slot 4
		// intentionally left retired rather than renumbered, so no other code needs to change.
		SunPosition = 5,
		Max
	};

	/** Plan Addendum 19: uniform shrink for the whole dev HUD, applied via UMG RenderTransform
	 * at each top-level panel widget so none of this file's own stacking/offset constants need
	 * to change - see ApplyDevHudCornerScale. */
	static constexpr float DevHudScale = 0.5f;

	IH_WB_DEMO004_API float GetStandardStackContentHeight(EStackSlot Slot, int32 IslandCount = 3);
	IH_WB_DEMO004_API FVector2D GetStackPosition(EStackSlot Slot, int32 IslandCount = 3);
	IH_WB_DEMO004_API void ConfigureTopLeftPanelSlot(
		UCanvasPanelSlot* Slot, EStackSlot StackSlot, float ContentHeight, int32 IslandCount = 3);
	IH_WB_DEMO004_API void ApplyTopLeftPanelSlotAtY(
		UCanvasPanelSlot* Slot, float TopY, float ContentHeight);
	IH_WB_DEMO004_API void ApplyPanelBorderStyle(
		UBorder* Border, float BackgroundAlpha = PanelBackgroundAlpha, bool bCompactPadding = false);
	IH_WB_DEMO004_API void ApplyHUDLabelFont(UTextBlock* TextBlock, int32 Size = LabelFontSize);
	IH_WB_DEMO004_API UTextBlock* MakeHUDLabel(
		UWidgetTree* Tree,
		const FName& Name,
		const FString& Text,
		FName ColorRole = FName(TEXT("HeadingText")));
	IH_WB_DEMO004_API UButton* MakeHUDButton(
		UWidgetTree* Tree,
		UUserWidget* Owner,
		const FName& Name,
		const FString& Label,
		const FName& ClickHandler,
		bool bCompact = false);

	/** Plan Addendum 19: scale a top-level dev-HUD widget (and its whole subtree - text, borders,
	 * sliders, checkboxes) toward the screen corner it's anchored to, so it shrinks without
	 * drifting away from that corner. Call once from the widget's own NativeConstruct with
	 * CornerPivot = (0,0) for the top-left stack, (1,0) for the top-right cluster. */
	IH_WB_DEMO004_API void ApplyDevHudCornerScale(
		UUserWidget* Widget, const FVector2D& CornerPivot, float Scale = DevHudScale);
}
