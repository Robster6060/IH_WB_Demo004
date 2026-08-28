// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_MannequinWidget.generated.h"

class UBorder;
class UTextBlock;

/** Compact Mannequin button — sits between Top Down View and Place Ship in the top-right HUD cluster. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_MannequinWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const;
	/** Returns true if click toggled place mode. */
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool IsPlaceModeActive() const { return bPlaceModeActive; }
	void SetPlaceModeActive(bool bActive);
	void ClearPlaceMode() { SetPlaceModeActive(false); }

private:
	void EnsureWidgetTree();
	void RefreshVisual();

	UPROPERTY(Transient) TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LabelText;
	bool bPlaceModeActive = false;
};
