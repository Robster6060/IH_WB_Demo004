// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_CameraAslWidget.generated.h"

class UBorder;
class UTextBlock;

/** PIE HUD: integer camera altitude (m ASL) beside Game Speed panel. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_CameraAslWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

public:
	void UpdateAltitudeMeters(int32 AltitudeMeters);

private:
	void EnsureWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;
};
