// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"

class UBorder;
class UCanvasPanelSlot;

namespace IH_BuildPalettePanelStyle
{
	static constexpr float RightMargin = 9.f;
	static constexpr float TopMargin = 23.f;
	/** Gap between viewport right edge and top-right HUD / tab strip. */
	static constexpr float RightHUDInset = 20.f;
	static constexpr float TabStripWidth = 36.f;
	/**
	 * Top row: ASL | Game Speed | DEV View flush to the right edge.
	 * G/W/B/C/D sits below that row (see TabStripTopMargin) — not beside it.
	 */
	static constexpr float TopRightHudClusterRightClearPx = RightHUDInset;
	static constexpr float TopRightHudClusterTopY = 20.f;
	static constexpr float TopRightHudClusterGapPx = 8.f;
	// Plan Addendum 20: all four Approx width/height constants below bumped ~1.3x, matching the
	// LabelFontSize 12->16 increase - they were stale estimates of each widget's rendered size
	// tuned for the smaller pre-bump font, so every widget positioned "to the left of" a
	// neighbor was sitting too close (Top Down View was visibly overlapping Place Ship).
	/** Approx Game Speed panel width — keep ASL tight against it (annotated grab). */
	static constexpr float TopRightHudGameSpeedApproxW = 192.f;
	static constexpr float TopRightHudDevViewW = 224.f;
	static constexpr float TopRightHudAslApproxW = 114.f;
	static constexpr float TopRightHudDevViewApproxH = 247.f;
	/** G/W/B/C/D strip — below DEV View so it does not crowd the HUD row. */
	static constexpr float TabStripTopMargin =
		TopRightHudClusterTopY + TopRightHudDevViewApproxH + 12.f;
	static constexpr float TopRightHudPlaceShipApproxW = 130.f;
	static constexpr float FlyOutWidth = 220.f;
	/** Gap between fly-out panel right edge and the G/W/B/C/D tab strip. */
	static constexpr float FlyOutTabStripGap = 3.f;
	/** Hover focus ring nudge after icon-tile rect (usually 0). */
	static constexpr float FocusOutlineOffsetX = 0.f;
	static constexpr float FocusOutlineOffsetY = 0.f;

	static const FLinearColor FocusBlue = FLinearColor(0.4f, 0.75f, 1.0f, 1.f);
	static const FLinearColor DisabledTabText = FLinearColor(0.45f, 0.48f, 0.52f, 1.f);

	IH_WB_DEMO004_API void ApplyRightFlyOutBorderStyle(UBorder* Border, bool bFocusOutline = false);
	IH_WB_DEMO004_API void ApplyRightTabStripBorderStyle(UBorder* Border);
	/** 48×48 drag tile chrome for Grid / TownTemplates list. */
	static constexpr float GridTemplateTileSize = 48.f;
	static constexpr float GridTemplateRowBorderPaddingY = 4.f;
	static constexpr float GridTemplateRowGap = 2.f;
	static constexpr float GridTemplateRowHeight = GridTemplateTileSize + GridTemplateRowBorderPaddingY;
	static constexpr float GridFlyOutHeaderBlockH = 28.f;
	static constexpr float GridFlyOutStubSectionsH = 80.f;
	static constexpr float GridFlyOutContentInsetX = 8.f;
	static constexpr float GridFlyOutHeaderY = 8.f;
	static constexpr float GridFlyOutRowStartY = 32.f;
	static constexpr float GridFlyOutIconLabelGap = 6.f;

	/** W tab — 7×3 active terrain stamps + special row + 7×1 reserved mod slots. */
	static constexpr int32 WorldStampGridColumns = 7;
	static constexpr int32 WorldStampActiveRows = 3;
	static constexpr int32 WorldStampReservedRowSlots = 7;
	static constexpr float WorldStampTileSize = 48.f;
	static constexpr float WorldStampTileGap = 4.f;
	static constexpr float WorldStampGridStartY = 28.f;
	static constexpr float WorldStampSectionHeaderH = 18.f;
	static constexpr float WorldStampSpecialGap = 6.f;
	static constexpr float WorldStampReservedGap = 4.f;
	/** W tab fly-out — 7 columns + insets (220 + 3 column strides = 376). */
	static constexpr float WorldStampColumnStride = WorldStampTileSize + WorldStampTileGap;
	static constexpr float WorldFlyOutWidth =
		GridFlyOutContentInsetX
		+ static_cast<float>(WorldStampGridColumns) * WorldStampTileSize
		+ static_cast<float>(WorldStampGridColumns - 1) * WorldStampTileGap
		+ GridFlyOutContentInsetX;

	IH_WB_DEMO004_API void ApplyGridTemplateTileBorderStyle(UBorder* Border);
	/** Mirror of DevPanel top-left slot, but with explicit X for right-edge placement. */
	IH_WB_DEMO004_API void ApplyTopRightPanelCanvasSlot(
		UCanvasPanelSlot* Slot, float TopX, float TopY, float PanelW, float PanelH);
}
