// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_RealmRegenProgressWidget.generated.h"

class UBorder;
class UProgressBar;
class UTextBlock;

/** Decorative PIE overlay while realm/island regen runs (not CPU-accurate). */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_RealmRegenProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowProgress(const FString& Label);
	void UpdateFakeProgress(float Percent);
	void SetProgressLabel(const FString& Label);
	void CompleteAndHide();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void EnsureWidgetTree();
	void SetDisplayPercent(float Percent);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBar;
};
