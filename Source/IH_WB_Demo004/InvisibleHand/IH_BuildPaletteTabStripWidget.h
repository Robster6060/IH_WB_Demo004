// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_BuildPaletteTypes.h"
#include "Blueprint/UserWidget.h"
#include "IH_BuildPaletteTabStripWidget.generated.h"

class UIH_BuildPaletteSubsystem;
class AIH_Cube2FlyPlayerController;

/** Right-edge G/W/B/C/D tab strip — fullscreen overlay + viewport-anchored NativePaint (minimap pattern). */
UCLASS()
class IH_WB_DEMO004_API UIH_BuildPaletteTabStripWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UIH_BuildPaletteTabStripWidget(const FObjectInitializer& ObjectInitializer);

	void InitializeTabStrip(UIH_BuildPaletteSubsystem* InSubsystem, AIH_Cube2FlyPlayerController* InPC);
	void RequestRepaint();
	void InvalidateHitCache();

	bool IsScreenPointOverTabStrip(const FVector2D& ScreenAbsolute) const;
	bool IsViewportPointOverTabStrip(const FVector2D& ViewportLocal) const;
	bool TryGetTabStripScreenRect(FSlateRect& OutRect) const;
	bool TryGetTabStripViewportRect(FSlateRect& OutRect) const;
	int32 HitTestTabIndex(const FVector2D& ScreenAbsolute) const;
	int32 HitTestTabIndexAtViewport(const FVector2D& ViewportLocal) const;
	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);
	/** Viewport-local X of the painted strip left edge (for fly-out placement). */
	bool TryGetTabStripLeftViewportX(float& OutLeftViewportX) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void EnsureMinimalWidgetTree();
	FGeometry GetBaseGeometry() const;
	FGeometry ResolveBaseGeometry(const FGeometry& AllottedGeometry) const;
	FVector2D ResolveOverlaySize(const FGeometry& Geometry) const;
	FVector2D GetTabStripSize() const;
	FVector2D GetTabStripTopLeftLocal(const FGeometry& Geometry) const;
	FGeometry MakeOverlayGeometry(const FGeometry& AllottedGeometry) const;
	FGeometry GetHitTestGeometry() const;
	FGeometry MakeTabStripGeometry(const FGeometry& AllottedGeometry) const;
	FSlateRect GetTabStripScreenRect(const FGeometry& Geometry) const;
	FSlateRect GetTabStripViewportRect(const FGeometry& Geometry) const;
	FSlateRect GetTabRowRect(const FGeometry& TabStripGeometry, int32 TabIndex) const;
	bool IsPointInTabStrip(const FGeometry& Geometry, const FVector2D& ScreenPos) const;
	void DrawSolidLocalRect(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FSlateRect& LocalRect,
		const FLinearColor& Color) const;

	TWeakObjectPtr<UIH_BuildPaletteSubsystem> BuildPaletteSubsystem;
	TWeakObjectPtr<AIH_Cube2FlyPlayerController> OwnerPC;

	mutable bool bLoggedPaintGeometryOnce = false;
	mutable bool bHasCachedTabStripScreenRect = false;
	mutable bool bHasCachedTabStripViewportRect = false;
	mutable FSlateRect CachedTabStripScreenRect;
	mutable FSlateRect CachedTabStripViewportRect;
};
