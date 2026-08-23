// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * WB / PIE-only runtime view toggles (not gameplay progression).
 * Defaults match compile-time DesignSpec; HUD may change at runtime.
 */
namespace IHDevViewRuntime
{
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
