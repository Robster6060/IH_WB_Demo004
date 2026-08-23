// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"
#include "IH_P1C08_MinimapTypes.h"
#include "Blueprint/UserWidget.h"

#include "IH_P1C08_MinimapWidget.generated.h"



class UIH_P1C08_MinimapSubsystem;

class AIH_Cube2FlyPlayerController;

class UTexture2D;



UCLASS()

class IH_WB_DEMO004_API UIH_P1C08_MinimapWidget : public UUserWidget

{

	GENERATED_BODY()



public:

	UIH_P1C08_MinimapWidget(const FObjectInitializer& ObjectInitializer);



	void InitializeMinimap(UIH_P1C08_MinimapSubsystem* InSubsystem, AIH_Cube2FlyPlayerController* InPC);

	void RequestRepaint();

	void RequestLayoutRefresh();

	bool HitTestPanelAtScreen(const FVector2D& ScreenPos) const;
	bool IsScreenPointerOverPanel(const FVector2D& ScreenPos) const;
	bool TryGetMapLocalFromScreen(const FVector2D& ScreenPos, FVector2D& OutMapLocal, FVector2D& OutMapSize) const;
	bool TryGetWorldXYFromScreen(const FVector2D& ScreenPos, FVector2D& OutWorldXY) const;
	bool HandleScreenPointerDown(const FVector2D& ScreenPos);
	void HandleScreenPointerMove(const FVector2D& ScreenPos);
	void HandleScreenPointerUp(const FVector2D& ScreenPos);
	void CancelPointerInteraction();
	void ResetPaintDiagnostics();



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

	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;



private:
	void EnsureMinimalWidgetTree();
	void SyncSpacerToPanelLayout();
	IH_P1C08_Minimap::FPanelLayout ResolvePanelLayout() const;
	FGeometry GetBaseGeometry() const;
	FGeometry ResolveBaseGeometry(const FGeometry& AllottedGeometry) const;
	FGeometry GetHitTestGeometry() const;
	FVector2D ResolveOverlaySize(const FGeometry& Geometry) const;
	FVector2D GetPanelTopLeftLocal(const FGeometry& Geometry) const;
	FGeometry MakeOverlayGeometry(const FGeometry& AllottedGeometry) const;
	FGeometry MakePanelGeometry(const FGeometry& AllottedGeometry) const;
	FSlateRect GetPanelScreenRect(const FGeometry& Geometry) const;
	bool IsPointInPanel(const FGeometry& Geometry, const FVector2D& ScreenPos) const;
	FVector2D ScreenToPanelLocal(const FGeometry& Geometry, const FVector2D& ScreenPos) const;

	FVector2D GetMapContentSize() const;

	FVector2D GetPanelSize() const;

	FVector2D GetMapContentOrigin() const;

	FSlateRect GetTitleBarRect(const FGeometry& PanelGeometry) const;

	FSlateRect GetCloseButtonRect(const FGeometry& PanelGeometry, bool bIncludeHitPadding = false) const;

	FSlateRect GetMapContentRect(const FGeometry& PanelGeometry) const;

	bool MapLocalToWorld(

		const FGeometry& PanelGeometry,

		const FVector2D& MapLocal,

		FVector2D& OutWorldXY) const;



	void DrawClosedPolyline(

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FGeometry& Geometry,

		const TArray<FVector2D>& LocalPoints,

		const FLinearColor& Color,

		float Thickness,

		bool bAntialias = true) const;

	void DrawFilledAnnulus(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const TArray<FVector2D>& InnerLocal,
		const TArray<FVector2D>& OuterLocal,
		const FLinearColor& Color,
		bool bLenientMinimapFill = false) const;

	/** Scanline fill of a single closed ring (no inner hole) - used to fill landmass coastline
	 * rings solid instead of leaving them as unfilled stroke outlines. */
	void DrawFilledPolygon(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const TArray<FVector2D>& RingLocal,
		const FLinearColor& Color) const;

	void DrawDashedClosedPolyline(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const TArray<FVector2D>& LocalPoints,
		const FLinearColor& Color,
		float Thickness,
		float DashLengthPx,
		float GapLengthPx) const;

	void DrawShipGlyph(

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FGeometry& Geometry,

		const FVector2D& TipLocal,

		float YawDegrees,

		bool bSelected) const;



	void DrawCameraV(

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FGeometry& Geometry,

		const FVector2D& ApexLocal,

		float YawDegrees) const;

	void DrawMapDot(

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FGeometry& Geometry,

		const FVector2D& CenterLocal,

		float Radius,

		const FLinearColor& Color) const;

	void DrawMapPlus(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& CenterLocal,
		float HalfLengthPx,
		float LineThicknessPx,
		const FLinearColor& Color) const;

	void DrawSolidLocalRect(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FSlateRect& LocalRect,
		const FLinearColor& Color) const;



	TWeakObjectPtr<UIH_P1C08_MinimapSubsystem> MinimapSubsystem;

	TWeakObjectPtr<AIH_Cube2FlyPlayerController> OwnerPC;

	// Plan Addendum 15: north-up compass rose sprite, top-left corner of the minimap panel.
	TObjectPtr<UTexture2D> CompassRoseTexture;



	bool bDraggingPanel = false;

	FVector2D DragGrabOffset = FVector2D::ZeroVector;

	mutable bool bLoggedPaintGeometryOnce = false;
	mutable int32 LastLoggedCoastlineRingCount = -1;

};

