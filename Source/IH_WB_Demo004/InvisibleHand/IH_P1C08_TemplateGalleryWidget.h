// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_TemplateGalleryWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

/** Phase 2 gallery harness — step review matrix cells and apply preview regen. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_TemplateGalleryWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	bool IsScreenPointOverPanel(const FVector2D& ScreenAbsolute) const { return IsPointOverPanel(ScreenAbsolute); }
	void UpdatePanelLayout();
	void ApplyDevPanelStackPosition(float TopY, float ContentHeight);
	void RefreshFromGallery();

private:
	bool IsPointOverPanel(const FVector2D& ScreenAbsolute) const;
	bool DispatchButtonClickIfUnder(UButton* Button, const FVector2D& ScreenAbsolute);
	void EnsureWidgetTree();

	UFUNCTION()
	void HandlePrevClicked();
	UFUNCTION()
	void HandleNextClicked();
	UFUNCTION()
	void HandleApplyPreviewClicked();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PrevButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> NextButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ApplyButton;
};
