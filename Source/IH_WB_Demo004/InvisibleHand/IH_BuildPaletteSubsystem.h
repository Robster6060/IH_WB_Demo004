// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "CoreMinimal.h"

#include "IH_WB_Demo004.h"

#include "IH_BuildPaletteItemRow.h"

#include "IH_BuildPaletteTypes.h"
#include "FIHTerrainStampTypes.h"

#include "Subsystems/GameInstanceSubsystem.h"

#include "IH_BuildPaletteSubsystem.generated.h"



class AIH_Cube2FlyPlayerController;

class AIH_TownGridManager;

class AIH_StructurePlacementActor;

class AIH_TerrainStampActor;

class AIH_WB_IslandActor;

class UIH_BuildPaletteHostWidget;

class UIHTownGridDataSubsystem;

class UWorld;



UCLASS()

class IH_WB_DEMO004_API UIH_BuildPaletteSubsystem : public UGameInstanceSubsystem

{

	GENERATED_BODY()



public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;



	/** Tab strip is pinned on the right viewport edge (PIE / Editor). */
	bool IsTabStripVisible() const;

	/** Subsystem intent for tab strip visibility (independent of widget collapsed state). */
	bool HasTabStripEnabled() const { return bTabStripVisible; }

	/** Ensure tab strip + flyout host exist, are in the viewport, and match the current PIE session. */
	void EnsureBuildPaletteReady(AIH_Cube2FlyPlayerController* PC);

	/** @deprecated Use IsTabStripVisible — strip stays visible; fly-out toggles separately. */
	bool IsGridPanelOpen() const { return bTabStripVisible; }

	bool IsFlyOutOpen() const { return bFlyOutOpen; }
	/**
	 * Canonical gate — viewport island double-click / shift-select / CommitSelectionChange
	 * must not run while W terrain-stamp fly-out is active (FIX-001d).
	 */
	bool IsViewportIslandSelectionBlocked() const;
	/** W-tab fly-out open — island whole-select/drag suppressed; stamp mesh edit active. */
	bool IsWorldStampEditModeActive() const { return IsViewportIslandSelectionBlocked(); }

	bool IsGridFlyOutOpen() const { return bFlyOutOpen && ActiveTab == EIHBuildPaletteTab::Grid; }

	bool IsDragActive() const { return bDragActive; }

	/** B tab structure DropActor drag (mesh ghost + placement). */
	bool IsStructureBuildDragActive() const;

	/** W tab terrain stamp drag (heightfield edit). */
	bool IsTerrainStampDragActive() const;

	EIHBuildPaletteTab GetActiveTab() const { return ActiveTab; }

	const FIHBuildPaletteItemRow& GetDragPayload() const { return DragPayload; }



	UIHTownGridDataSubsystem* GetBuildPaletteDataSubsystem() const;

	UIH_BuildPaletteHostWidget* GetBuildPaletteWidget() const { return BuildPaletteWidget; }



	/** Create palette host widget and add to viewport (collapsed) so G toggle only flips visibility. */

	void PrepareBuildPaletteWidget(AIH_Cube2FlyPlayerController* PC);



	/** M0 full close; cancels active drag. */

	void ToggleGridPanel(AIH_Cube2FlyPlayerController* PC);

	void OpenGridPanel(AIH_Cube2FlyPlayerController* PC);

	void CloseGridPanel();



	/** M1 — G tab / G hotkey toggles Grid fly-out. */
	void ToggleGridFlyOut(AIH_Cube2FlyPlayerController* PC);

	/** Toggle fly-out for any palette tab (G/W/B/C/D). Only one fly-out open at a time. */
	void ToggleTabFlyOut(EIHBuildPaletteTab Tab, AIH_Cube2FlyPlayerController* PC);

	void OpenTabFlyOut(EIHBuildPaletteTab Tab, AIH_Cube2FlyPlayerController* PC);

	void CloseFlyOut();

	void SetGridFlyOutOpen(bool bOpen);



	bool TryFindPaletteItem(FName ItemID, FIHBuildPaletteItemRow& OutRow) const;

	bool BeginDragFromItem(FName ItemID, AIH_Cube2FlyPlayerController* PC = nullptr);

	/** W tab — begin drag from canonical terrain stamp catalog (no DT row required). */
	bool BeginDragFromTerrainStamp(EIHTerrainStampId StampId, AIH_Cube2FlyPlayerController* PC = nullptr);

	bool HasValidDragGhostLocation() const { return bDragGhostLocationValid; }

	FVector GetDragGhostWorldLocation() const { return DragGhostDrawCenterWorld; }

	FVector GetDragPlacementActorOrigin() const { return DragPlacementActorOrigin; }

	bool GetActiveDragFootprintCm(FVector& OutExtentCm) const;

	AIH_StructurePlacementActor* GetBuildDragPreviewActor() const { return BuildDragPreviewActor.Get(); }

	AIH_TerrainStampActor* GetTerrainStampDragPreviewActor() const { return TerrainStampDragPreviewActor.Get(); }

	/** Ignore during island surface traces (structure or stamp drag preview). */
	AActor* GetDragPreviewIgnoreActor() const;

	void CancelDrag();

	bool TryCompleteDropAtScreen(APlayerController* PC, const FVector2D& ScreenPos);

	/** Minimap map-area pick: resolve placement from world XY (island surface sample). */
	bool TryCompleteDropAtWorldXY(APlayerController* PC, const FVector2D& WorldXY);

	/** Commit structure at last valid drag origin (sticky ghost). */
	bool TryCommitStructureDropAtStoredPlacement(APlayerController* PC);

	/** Commit terrain stamp at last valid island surface (sticky ghost). */
	bool TryCommitTerrainStampDropAtStoredPlacement(APlayerController* PC);

	/** B2b-3 — W-tab only: double-click stamp select (mirrors island pattern; separate state). */
	bool TryHandleStampSelectionClickAtViewport(AIH_Cube2FlyPlayerController* FlyPC, const FVector2D& ViewportPick);

	/** B2b-3 — select / move / rotate / scale placed stamp actors (pre-bake). */
	bool TryFindTerrainStampAtScreen(APlayerController* PC, const FVector2D& ScreenPos, AIH_TerrainStampActor*& OutStamp) const;
	void SelectTerrainStamp(AIH_TerrainStampActor* Stamp);
	/** When cursor is on island but not on a stamp mesh, pick nearest stamp on that island. */
	bool TrySelectNearestTerrainStampOnIsland(AIH_WB_IslandActor* Island, const FVector& SurfaceWorld);
	void ClearTerrainStampSelection();
	AIH_TerrainStampActor* GetSelectedTerrainStamp() const { return SelectedTerrainStamp.Get(); }
	bool HasSelectedTerrainStamp() const { return SelectedTerrainStamp.IsValid(); }
	bool IsStampMoveDragActive() const { return bStampMoveDragActive; }
	void BeginStampMoveDrag(AIH_TerrainStampActor* Stamp);
	void UpdateStampMoveDrag(APlayerController* PC, const FVector2D& ScreenPos);
	void EndStampMoveDrag();
	void TickTerrainStampManipulation(APlayerController* PC);
	/** W-open + stamp selected: wheel rotate (or shift+wheel scale); blocks fly zoom at FlyPC. */
	void ApplySelectedStampMouseWheel(APlayerController* PC, float WheelDelta);
	void DrawSelectedTerrainStampGizmo(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC = nullptr) const;
	void DrawPlacedTerrainStampPickHints(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC = nullptr) const;
	bool TryRemoveSelectedTerrainStamp();
	void LogTerrainStampReplayHeaderStub() const;

	void UpdateDragGhostFromScreen(APlayerController* PC, const FVector2D& ScreenPos);

	void UpdateDragGhostFromWorldXY(APlayerController* PC, const FVector2D& WorldXY);

	void DrawDragGhost(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC = nullptr) const;



private:

	void EnsureWidget(AIH_Cube2FlyPlayerController* PC);

	/** Ensure widget exists and is in viewport without collapsing or resetting panel state. */
	void EnsureWidgetInViewport(AIH_Cube2FlyPlayerController* PC);

	void RefreshCachedGridRowCounts();
	void RefreshCachedBuildRowCounts();

	void LogFirstOpenIfNeeded();

	void SyncWidgetFlyOutState();

	void SyncWidgetFlyOutStateIfChanged();

	void EnsureBuildDragPreview(AIH_Cube2FlyPlayerController* PC, FName ItemID);

	void DestroyBuildDragPreview();

	void UpdateBuildDragPreviewTransform();

	void EnsureTerrainStampDragPreview(AIH_Cube2FlyPlayerController* PC, EIHTerrainStampId StampId);

	void DestroyTerrainStampDragPreview();

	void UpdateTerrainStampDragPreviewTransform();

	bool CommitActiveStructureDrop(AIH_Cube2FlyPlayerController* FlyPC, UWorld* World, const FVector& SpawnLocation);

	static bool IsTabImplemented(EIHBuildPaletteTab Tab);



	UPROPERTY()

	TObjectPtr<UIH_BuildPaletteHostWidget> BuildPaletteWidget;



	EIHBuildPaletteTab ActiveTab = EIHBuildPaletteTab::Grid;

	FIHBuildPaletteItemRow DragPayload;

	/** Terrain-centered draw point (matches G Grid ghost). */
	FVector DragGhostDrawCenterWorld = FVector::ZeroVector;

	/** Structure actor root for preview spawn / drop. */
	FVector DragPlacementActorOrigin = FVector::ZeroVector;

	/** Last good island placement (sticky while cursor is over water / sky). */
	FVector StickyBuildDragDrawCenterWorld = FVector::ZeroVector;
	FVector StickyBuildDragActorOrigin = FVector::ZeroVector;

	bool bDragGhostLocationValid = false;
	bool bLoggedBuildDragGhostValid = false;
	bool bLoggedTerrainStampDragGhostValid = false;



	bool bTabStripVisible = false;

	bool bFlyOutOpen = false;

	bool bDragActive = false;

	bool bLoggedFirstOpen = false;

	bool bPaletteHostViewportReady = false;

	bool bLastSyncedFlyOutOpen = false;

	EIHBuildPaletteTab LastSyncedFlyOutTab = EIHBuildPaletteTab::Grid;

	int32 CachedGridTemplateRowCount = 0;
	int32 CachedBuildRowCount = 0;

	TWeakObjectPtr<AIH_Cube2FlyPlayerController> PaletteOwnerPC;

	UPROPERTY(Transient)
	TObjectPtr<AIH_StructurePlacementActor> BuildDragPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AIH_TerrainStampActor> TerrainStampDragPreviewActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AIH_StructurePlacementActor>> PlacedStructureActors;

	TWeakObjectPtr<class AIH_WB_IslandActor> StickyStampTargetIsland;

	TWeakObjectPtr<AIH_TerrainStampActor> SelectedTerrainStamp;
	bool bStampMoveDragActive = false;

	/** W-tab stamp double-click tracking — never shared with island LastClickedIslandIndex. */
	TWeakObjectPtr<AIH_TerrainStampActor> LastClickedStamp;
	float LastStampClickTimeSec = -1.f;
	static constexpr float StampDoubleClickWindowSec = 0.45f;

	void CommitStickyBuildDragPlacement(const FVector& DrawCenterWorld, const FVector& ActorOriginWorld);

	void CommitStickyTerrainStampPlacement(
		const FVector& SurfaceWorld,
		class AIH_WB_IslandActor* TargetIsland);

	bool CommitActiveTerrainStampDrop(
		AIH_Cube2FlyPlayerController* FlyPC,
		class AIH_WB_IslandActor* Island,
		const FVector& SurfaceWorld,
		EIHTerrainStampId StampId);

	static FIHBuildPaletteItemRow MakeSyntheticTerrainStampPaletteRow(EIHTerrainStampId StampId);

	static bool TryGetTerrainStampIdFromRow(const FIHBuildPaletteItemRow& Row, EIHTerrainStampId& OutStampId);

	void RotateSelectedTerrainStamp(float DeltaDeg);
	void ScaleSelectedTerrainStampRadius(float Factor);
	void ApplySelectedTerrainStampTransform();

	static bool TryGetStructureFootprintCm(FName ItemID, FVector& OutExtentCm);

};

