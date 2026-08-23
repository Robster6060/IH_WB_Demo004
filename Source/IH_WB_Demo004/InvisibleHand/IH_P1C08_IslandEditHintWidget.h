// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_IslandEditHintWidget.generated.h"

class UTextBlock;

/** Bottom HUD hint while an island is selected for editing. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_IslandEditHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHintText(const FString& Text);
	void SetPieDevHint();
	void ClearHint();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void EnsureWidgetTree();
	void ApplyHintText(const FString& Text);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	bool bPieDevHintActive = false;
	FString PieDevHintText;
};
