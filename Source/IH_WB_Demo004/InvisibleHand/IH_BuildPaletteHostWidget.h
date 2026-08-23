// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "IH_BuildPaletteItemRow.h"

#include "IH_BuildPaletteTypes.h"
#include "FIHTerrainStampTypes.h"
#include "Blueprint/UserWidget.h"

#include "IH_BuildPaletteHostWidget.generated.h"



class UBorder;

class UCanvasPanel;

class UHorizontalBox;

class USizeBox;

class UTextBlock;

class UVerticalBox;

class UIH_BuildPaletteSubsystem;

class AIH_Cube2FlyPlayerController;



/** M1 C++ shell — right tab strip + G fly-out (Town Grids drag source, stub sections). */

UCLASS()

class IH_WB_DEMO004_API UIH_BuildPaletteHostWidget : public UUserWidget

{

	GENERATED_BODY()



public:

	UIH_BuildPaletteHostWidget(const FObjectInitializer& ObjectInitializer);


	void InitializeBuildPalette(UIH_BuildPaletteSubsystem* InSubsystem, AIH_Cube2FlyPlayerController* InPC);

	void RefreshGridTemplateList();
	void RefreshBuildTemplateList();
	void RefreshWorldStampPalette();

	void SetTabStripVisible(bool bVisible);

	void SetActiveFlyOutTab(TOptional<EIHBuildPaletteTab> Tab);
	void SetActiveFlyOutTab(EIHBuildPaletteTab Tab) { SetActiveFlyOutTab(TOptional<EIHBuildPaletteTab>(Tab)); }
	/** True when W terrain-stamp fly-out panel is visible (cross-check for FIX-001d). */
	bool IsWorldFlyOutVisible() const;

	void SetGridFlyOutOpen(bool bOpen);

	void RequestLayoutRefresh();

	void LogLayoutDiagnostics(const TCHAR* Context) const;

	/** Build programmatic widget tree if missing (safe before AddToViewport). */
	void EnsureWidgetTreeBuilt();



	bool IsScreenPointOverBuildPalette(const FVector2D& ScreenAbsolute) const;
	bool IsScreenPointOverTabStrip(const FVector2D& ScreenAbsolute) const;
	int32 HitTestTabIndex(const FVector2D& ScreenAbsolute) const;

	bool TryGetFlyOutScreenRect(FSlateRect& OutRect) const;

	bool HandleScreenPointerDown(const FVector2D& ScreenAbsolute);

	bool HandleScreenPointerMove(const FVector2D& ScreenAbsolute);

	int32 HitTestGridTemplateTile(const FVector2D& ScreenAbsolute) const;
	int32 HitTestBuildTemplateTile(const FVector2D& ScreenAbsolute) const;
	int32 HitTestWorldStampTile(const FVector2D& ScreenAbsolute) const;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeConstruct() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(

		const FPaintArgs& Args,

		const FGeometry& AllottedGeometry,

		const FSlateRect& MyCullingRect,

		FSlateWindowElementList& OutDrawElements,

		int32 LayerId,

		const FWidgetStyle& InWidgetStyle,

		bool bParentEnabled) const override;



private:

	struct FGridTemplateTileWidgets

	{

		FName ItemID;

		TObjectPtr<UBorder> RowBorder;

		TObjectPtr<USizeBox> IconSizeBox;

		TObjectPtr<class UImage> IconImage;

		TObjectPtr<UTextBlock> LabelText;

	};

	struct FWorldStampPaletteSlot
	{
		EIHTerrainStampId StampId = EIHTerrainStampId::Hill;
		bool bActive = false;
		bool bReserved = false;
		FString ShortLabel;
	};



	void EnsureWidgetTree();

	void SyncHostLayout();

	void ResolveViewportMetrics(FVector2D& OutViewportSize, FVector2D& OutViewportAbs) const;

	/** Anchor overlay paint/hit-test to the game viewport (AllottedGeometry can differ in PIE).
	 *
	 * Viewport-overlay HUD contract (Build Palette, minimap strip, etc.):
	 * - Widget is fullscreen in viewport (HitTestInvisible); visuals are often NativePaint, not UMG layout.
	 * - UMG canvas slots (RootSizeBox / FlyOutSizeBox) can diverge from painted geometry in PIE — do not
	 *   use GetCachedGeometry on those widgets for paint or hit-test.
	 * - Single source of truth: GetHitTestGeometry() + MakeTabStripGeometry / MakeFlyOutPaintGeometry /
	 *   TryGetFlyOutScreenRect / TryGetTabStripScreenRect. Paint, hover, and drag must share these rects.
	 */
	FGeometry GetBaseGeometry() const;
	FGeometry ResolveBaseGeometry(const FGeometry& AllottedGeometry) const;
	FVector2D ResolveOverlaySize(const FGeometry& Geometry) const;
	FGeometry MakeOverlayGeometry(const FGeometry& AllottedGeometry) const;
	FVector2D GetTabStripTopLeftLocal(const FGeometry& Geometry) const;
	FGeometry MakeTabStripGeometry(const FGeometry& AllottedGeometry) const;
	FGeometry MakeFlyOutPaintGeometry(const FGeometry& AllottedGeometry) const;
	FGeometry GetHitTestGeometry() const;

	float ComputeHostPanelHeight() const;

	float GetActiveFlyOutWidth() const;

	float ResolveTabStripLeftViewportX() const;

	FVector2D ResolveViewportSize() const;

	bool TryGetTabStripScreenRect(FSlateRect& OutRect) const;

	int32 HitTestTabIndexInStripRect(const FSlateRect& StripRect, const FVector2D& ScreenAbsolute) const;

	void DrawSolidLocalRect(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FSlateRect& LocalRect,
		const FLinearColor& Color) const;

	bool TryGetGridTemplateRowLocalRect(int32 RowIndex, FSlateRect& OutLocalRect) const;
	bool TryGetBuildTemplateRowLocalRect(int32 RowIndex, FSlateRect& OutLocalRect) const;
	bool TryGetWorldStampSlotLocalRect(int32 SlotIndex, FSlateRect& OutLocalRect) const;

	int32 PaintGridFlyOutContent(
		const FGeometry& FlyOutGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintBuildFlyOutContent(
		const FGeometry& FlyOutGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintWorldFlyOutContent(
		const FGeometry& FlyOutGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintWorldStampSlot(
		const FGeometry& FlyOutGeometry,
		const FWorldStampPaletteSlot& Slot,
		const FSlateRect& LocalRect,
		bool bHovered,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintFlyOutRowHoverOutline(
		const FGeometry& AllottedGeometry,
		const FGeometry& FlyOutGeometry,
		const FSlateRect& RowLocal,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintGridTemplateHoverOutline(
		const FGeometry& AllottedGeometry,
		const FGeometry& FlyOutGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintGridTemplateHoverTooltip(
		const FGeometry& FlyOutGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	int32 PaintBuildDragPlacementMarker(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	bool IsPointInsideWidget(const UWidget* Widget, const FVector2D& ScreenAbsolute) const;


	UTextBlock* MakeSectionHeader(const FString& Label, bool bStubSection = false);

	UTextBlock* MakeStubLine(const FString& Label);

	void SyncFlyOutContentVisibility();
	void SyncTabStripVisuals();
	float GetTabStripHeight() const;

	EIHBuildPaletteTab TabIndexToEnum(int32 TabIndex) const;

	static int32 EnumToTabIndex(EIHBuildPaletteTab Tab);



	TWeakObjectPtr<UIH_BuildPaletteSubsystem> BuildPaletteSubsystem;

	TWeakObjectPtr<AIH_Cube2FlyPlayerController> OwnerPC;



	UPROPERTY()

	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()

	TObjectPtr<USizeBox> RootSizeBox;



	UPROPERTY()

	TObjectPtr<UHorizontalBox> HostHBox;

	UPROPERTY()

	TObjectPtr<USizeBox> TabStripSizeBox;

	UPROPERTY()

	TObjectPtr<UBorder> TabStripBorder;

	UPROPERTY()

	TObjectPtr<UVerticalBox> TabStripVBox;

	UPROPERTY()

	TArray<TObjectPtr<UTextBlock>> TabKeyLabels;

	UPROPERTY()

	TObjectPtr<USizeBox> FlyOutSizeBox;

	UPROPERTY()

	TObjectPtr<UBorder> FlyOutBorder;

	UPROPERTY()

	TObjectPtr<UVerticalBox> TemplateListVBox;

	UPROPERTY()

	TObjectPtr<UVerticalBox> GridFlyOutVBox;

	UPROPERTY()

	TObjectPtr<UVerticalBox> WorldFlyOutVBox;

	UPROPERTY()

	TObjectPtr<UVerticalBox> BuildFlyOutVBox;

	UPROPERTY()

	TObjectPtr<UVerticalBox> ConveyFlyOutVBox;

	UPROPERTY()

	TObjectPtr<UVerticalBox> DefenseFlyOutVBox;



	TArray<FGridTemplateTileWidgets> TemplateTiles;

	TArray<FIHBuildPaletteItemRow> CachedGridRows;
	TArray<FIHBuildPaletteItemRow> CachedBuildRows;
	TArray<FWorldStampPaletteSlot> CachedWorldStampSlots;

	int32 HoveredTemplateIndex = INDEX_NONE;
	int32 HoveredBuildTemplateIndex = INDEX_NONE;
	int32 HoveredWorldStampIndex = INDEX_NONE;

	bool bTabStripVisible = false;

	TOptional<EIHBuildPaletteTab> ActiveFlyOutTab;

	mutable bool bLoggedPaintGeometryOnce = false;

	mutable bool bLoggedTabStripPaintOnce = false;

	mutable FSlateRect CachedTabStripScreenRect = FSlateRect();

	mutable bool bHasCachedTabStripScreenRect = false;

};

