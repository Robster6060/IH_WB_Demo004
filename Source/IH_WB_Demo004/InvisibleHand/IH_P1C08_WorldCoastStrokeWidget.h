// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C08_WorldCoastStrokeWidget.generated.h"

class AIH_Cube2FlyPlayerController;
class UIH_P1C08_MinimapSubsystem;

/** Screen-projected gold coast stroke — same MainCoast bake as minimap (2D Slate, not proc mesh). */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_WorldCoastStrokeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeOverlay(UIH_P1C08_MinimapSubsystem* InSubsystem, AIH_Cube2FlyPlayerController* InPC);
	void RequestRepaint();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	void DrawClosedPolylineScreen(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const TArray<FVector2D>& LocalPoints,
		const FLinearColor& Color,
		float Thickness) const;

	void BuildProjectedScreenLoop(
		const TArray<FVector2D>& CoastlineWorld,
		const FGeometry& WidgetGeometry,
		float StrokeZCm,
		float ZoomFactor,
		TArray<FVector2D>& OutStrokeScreen) const;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_MinimapSubsystem> Subsystem;

	UPROPERTY(Transient)
	TObjectPtr<AIH_Cube2FlyPlayerController> PlayerController;

	/** Throttle screen projection — invalidate only on meaningful camera delta. */
	mutable FVector CachedOverlayCameraLocation = FVector::ZeroVector;
	mutable FRotator CachedOverlayCameraRotation = FRotator::ZeroRotator;
	mutable bool bOverlayRepaintPending = true;
};
