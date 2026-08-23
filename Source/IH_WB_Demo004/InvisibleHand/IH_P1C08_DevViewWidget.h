// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_DevViewWidget.generated.h"

class UBorder;
class UCheckBox;
class UTextBlock;
class USizeBox;

/**
 * WB PIE-only view toggles (adjacent to Game Speed).
 * Ocean / Contours / Features / Clouds / GrabContrast.
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

	UFUNCTION()
	void HandleOceanChanged(bool bIsChecked);

	UFUNCTION()
	void HandleContoursChanged(bool bIsChecked);

	UFUNCTION()
	void HandleFeaturesChanged(bool bIsChecked);

	UFUNCTION()
	void HandleCloudsChanged(bool bIsChecked);

	UFUNCTION()
	void HandleGrabContrastChanged(bool bIsChecked);

	UPROPERTY(Transient) TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient) TObjectPtr<USizeBox> PanelSizeBox;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> OceanCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> ContoursCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> FeaturesCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> CloudsCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> GrabContrastCheck;

	bool bSuppressCheckNotify = false;
};
