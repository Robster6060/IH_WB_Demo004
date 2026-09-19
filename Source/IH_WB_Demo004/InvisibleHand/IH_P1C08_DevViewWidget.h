// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IHDevViewRuntime.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_DevViewWidget.generated.h"

class UBorder;
class UCheckBox;
class UTextBlock;
class USizeBox;

/**
 * WB PIE-only view toggles (adjacent to Game Speed).
 * Ocean / BANDS / BIOME / PGC / Clouds.
 *
 * 2026-09-12: the GrabContrast checkbox was retasked to "Show Nav" (toggles
 * FEngineShowFlags::Navigation on the PIE viewport) per explicit user request, since the console
 * `show Navigation` command wasn't a reliable option in this session and 'P' is already this
 * project's own game-speed pause key. IHDevViewRuntime's underlying GrabContrast system (albedo/
 * roughness dev toggle) is untouched and still callable - this UI slot just no longer drives it.
 *
 * 2026-09-18: the "Show Nav" slot (and Contours/Features before it) is retired the same way -
 * these three checkboxes now form a mutually-exclusive BANDS/BIOME/PGC island-coloring radio group
 * (IHDevViewRuntime::EIHDevColorMode). The underlying Contours/Features ribbon-overlay toggles and
 * the Show Nav engine-showflag path are untouched and still callable, just no longer wired to any
 * checkbox here - same retirement pattern as GrabContrast above.
 */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_DevViewWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const;
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);

private:
	void EnsureWidgetTree();
	void SyncChecksFromRuntime();
	bool TryToggleCheckAtScreen(UCheckBox* Check, void (UIH_P1C08_DevViewWidget::*Handler)(bool), const FVector2D& ScreenAbsolute);
	/** Shared BANDS/BIOME/PGC radio-group logic: checking Self activates Mode and force-unchecks the
	 * other two; unchecking the box that represents the CURRENTLY active mode is rejected (re-checked)
	 * since exactly one mode must always be active - there's no "none" island-coloring state. */
	void ApplyColorModeCheckboxChange(IHDevViewRuntime::EIHDevColorMode Mode, UCheckBox* Self, bool bIsChecked);

	UFUNCTION()
	void HandleOceanChanged(bool bIsChecked);

	UFUNCTION()
	void HandleBandsChanged(bool bIsChecked);

	UFUNCTION()
	void HandleBiomeChanged(bool bIsChecked);

	UFUNCTION()
	void HandleCloudsChanged(bool bIsChecked);

	UFUNCTION()
	void HandlePgcChanged(bool bIsChecked);

	UPROPERTY(Transient) TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient) TObjectPtr<USizeBox> PanelSizeBox;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> OceanCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> BandsCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> BiomeCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> CloudsCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> PgcCheck;

	bool bSuppressCheckNotify = false;
};
