// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_ConfirmRevertWidget.generated.h"

DECLARE_DELEGATE_OneParam(FOnConfirmRevertChoice, bool);

/** Modal Yes/No when deselecting with uncommitted island edits. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_ConfirmRevertWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowDialog(FOnConfirmRevertChoice InOnChoice);
	void ShowDialog(
		const FString& Title,
		const FString& Body,
		FOnConfirmRevertChoice InOnChoice);
	void HideDialog();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleYesClicked();

	UFUNCTION()
	void HandleNoClicked();

	void EnsureWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<class UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> BodyText;

	FOnConfirmRevertChoice OnChoice;
};
