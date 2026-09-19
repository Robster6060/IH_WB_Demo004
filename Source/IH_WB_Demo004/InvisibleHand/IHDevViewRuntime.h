// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * WB / PIE-only runtime view toggles (not gameplay progression).
 * Defaults match compile-time DesignSpec; HUD may change at runtime.
 */
namespace IHDevViewRuntime
{
	/** 2026-09-18: DEV View's island-coloring mode, mutually exclusive - replaces the standalone
	 * Contours/Features/Show Nav checkboxes in the HUD (their underlying AreContoursVisible/
	 * AreFeaturesVisible functions below and the Show Nav engine-showflag path stay callable, just
	 * no longer wired to any checkbox, mirroring this project's existing GrabContrast-checkbox
	 * retirement precedent). BANDS = today's always-on flat per-elevation-tier coloring (unchanged
	 * default). BIOME = finer per-biome coloring from ASLSlopeBiomeFinalChart.xlsx's own hexColor
	 * chart (FIHASLSlopeBiomeRow::biomeDetailColorHex). PGC = reserved for a future procedural-
	 * scatter view; renders identically to BANDS until that exists (never a blank/broken island). */
	enum class EIHDevColorMode : uint8
	{
		Bands,
		Biome,
		PGC
	};

	EIHDevColorMode GetDevColorMode();
	void SetDevColorMode(EIHDevColorMode Mode);
	void ApplyDevColorModeToWorld(UWorld* World);

	/** WaterBodyOcean + WaterZone / custom ocean plane visible. */
	bool IsOceanVisible();
	void SetOceanVisible(bool bVisible);

	/** ASL Contours: gold waterline + magenta −25 m + white +25 m ribbons. */
	bool AreContoursVisible();
	void SetContoursVisible(bool bVisible);

	/** Coast-character Features overlay: Beach / Gentle / Bluff strokes. */
	bool AreFeaturesVisible();
	void SetFeaturesVisible(bool bVisible);

	/** Template / volumetric clouds visible (default OFF for WB productivity). */
	bool AreCloudsVisible();
	void SetCloudsVisible(bool bVisible);

	/**
	 * Fidelity grab lighting: TankSun ~5.5 + darker TOPO albedo (vs pie Intensity 12 washout).
	 * Compare to typical UE 5.8 outdoor gameplay (directional ~3–8, lit terrain, auto-exposure).
	 */
	bool IsGrabContrastEnabled();
	void SetGrabContrastEnabled(bool bEnabled);

	void ApplyContoursVisibilityToWorld(UWorld* World);
	void ApplyFeaturesVisibilityToWorld(UWorld* World);
	void ApplyOceanVisibilityToWorld(UWorld* World);
	void ApplyCloudsVisibilityToWorld(UWorld* World);
	void ApplyGrabContrastToWorld(UWorld* World);
}
