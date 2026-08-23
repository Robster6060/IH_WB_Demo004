// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Blueprint/UserWidget.h"
#include "IH_P1C07_SelectionLassoWidget.generated.h"

/** Screen-space drag rectangle for fleet lasso / box select. */
UCLASS()
class IH_WB_DEMO004_API UIH_P1C07_SelectionLassoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDragRect(const FVector2D& InStart, const FVector2D& InEnd, bool bInActive);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FVector2D DragStart = FVector2D::ZeroVector;
	FVector2D DragEnd = FVector2D::ZeroVector;
	bool bDragActive = false;
};
