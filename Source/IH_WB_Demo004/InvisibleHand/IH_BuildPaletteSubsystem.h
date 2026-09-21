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
enum class EIHStampGripHandle : uint8;

class AIH_WB_IslandActor;

class UIH_BuildPaletteHostWidget;

class UIHTownGridDataSubsystem;

class UWorld;

// LIFO undo (2026-09-10) for placed Terrain Stamp move/rotate/delete actions - scoped to one
// continuous selection session (the stack is flushed on deselect or when selection moves to a
// different stamp, per the user's own "before deselect" framing), not a project-wide undo system.
enum class EIHStampUndoActionType : uint8
{
	Transform, // Stamp still exists - just reset its transform.
	Delete,    // Stamp was destroyed - respawn it (StampId/TargetIsland) then reset its transform.
};

struct FIHStampUndoRecord
{
	EIHStampUndoActionType ActionType = EIHStampUndoActionType::Transform;
	TWeakObjectPtr<AIH_TerrainStampActor> Stamp; // valid for Transform records only
	FTransform PreviousTransform;
	// 2026-09-11: sink depth ("merge into IslandMesh") is real per-actor state, not derived from
	// the transform alone - SetActorTransform won't roll it back on its own, so every Transform-type
	// push snapshots it here too (harmless no-op for actions, like Rotate, that don't touch it).
	float PreviousDepthBelowSurfaceCm = 100.f;
	EIHTerrainStampId StampId = EIHTerrainStampId::Hill; // Delete-record respawn data
	TWeakObjectPtr<AIH_WB_IslandActor> TargetIsland;      // Delete-record respawn data
};



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
	 * Selectable-actor hierarchy (2026-09-13): only one GWBCD category is ever selectable at a
	 * time, gated by which tab (if any) is currently open. A category is selectable only while
	 * its own tab's fly-out is open.
	 */
	bool IsCategorySelectableNow(EIHBuildPaletteTab Category) const
	{
		return bFlyOutOpen && ActiveTab == Category;
	}
	/** Island is the "nothing open" default — selectable only when every fly-out is closed. */
	bool IsIslandSelectableNow() const { return !bFlyOutOpen; }

	/**
	 * Force-deselects every category that ISN'T selectable under the current tab state (called
	 * from both OpenTabFlyOut and CloseFlyOut, right after ActiveTab/bFlyOutOpen settle) - replaces
	 * the two ad hoc special cases that used to live inline in OpenTabFlyOut (Build->TownGrid,
	 * World->Island) with one uniform sweep across every gated category. Mannequin is deliberately
	 * NOT touched here - it stays outside the GWBCD hierarchy, always selectable, per canon.
	 */
	void ClearSelectionsOutsideActiveScope(AIH_Cube2FlyPlayerController* PC);

	/**
	 * Canonical gate — viewport island double-click / shift-select / CommitSelectionChange
	 * must not run while any fly-out is open (FIX-001d, widened 2026-09-13 from "W open" to
	 * "any tab open" per the selectable-actor-hierarchy canon).
	 */
	bool IsViewportIslandSelectionBlocked() const;
	/** W-tab fly-out open — island whole-select/drag suppressed; stamp mesh edit active. */
	bool IsWorldStampEditModeActive() const;

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

	/**
	 * 2026-09-13: Convey (C) dev tile - no DT_BuildPaletteItem row backs "Merchantman" (unlike Grid/
	 * Build tiles), so this sets up the same drag state BeginDragFromItem would, directly, instead
	 * of going through a DataTable lookup that would just fail.
	 */
	bool BeginDragForMerchantmanTile(AIH_Cube2FlyPlayerController* PC = nullptr);

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
	/** bPreserveUndoStack: used only by UndoLastStampAction's Delete-record restore, so re-selecting
	 * the just-respawned stamp doesn't erase undo records still below the one just popped. */
	void SelectTerrainStamp(AIH_TerrainStampActor* Stamp, bool bPreserveUndoStack = false);
	/** When cursor is on island but not on a stamp mesh, pick nearest stamp on that island. */
	bool TrySelectNearestTerrainStampOnIsland(AIH_WB_IslandActor* Island, const FVector& SurfaceWorld);
	void ClearTerrainStampSelection();
	AIH_TerrainStampActor* GetSelectedTerrainStamp() const { return SelectedTerrainStamp.Get(); }
	bool HasSelectedTerrainStamp() const { return SelectedTerrainStamp.IsValid(); }
	bool IsStampMoveDragActive() const { return bStampMoveDragActive; }
	void BeginStampMoveDrag(AIH_TerrainStampActor* Stamp);
	// TotalScreenDeltaYFromDragStart (2026-09-11, "merge into IslandMesh" canon): TOTAL vertical
	// cursor movement since the drag began (current Y minus mouse-down Y, not a per-tick delta -
	// re-derived fresh from the drag's starting depth every tick rather than accumulated, so it
	// can't drift from per-frame mouse jitter) additionally sinks the stamp deeper into the surface
	// (never shallower) - see AIH_TerrainStampActor::SetManualSinkDepthFromDragTotal. Independent of
	// the X/Y repositioning ScreenPos already drives, so a purely-vertical mouse movement mid-drag
	// adjusts depth without fighting it.
	void UpdateStampMoveDrag(APlayerController* PC, const FVector2D& ScreenPos, float TotalScreenDeltaYFromDragStart);
	void EndStampMoveDrag();

	// Stage 12b (2026-09-11) - bounding-box scale grips. Mirrors the Begin/Update/End split above:
	// only ever checks the currently-selected stamp (grips only ever show on it). Hit-testing is
	// screen-space projection of the stamp's cached grip world positions (not a terrain-surface
	// raycast/3D-distance check) - a grip can hover over empty space past a tapered mesh's
	// silhouette (e.g. a corner grip on a dome-shaped stamp), where a world-space trace would miss
	// or hit ground far below instead. Dragging X/Y/corner similarly resolves its per-tick world
	// point via a camera-facing plane through the grip's own drag-start position (GripDragPlaneOrigin/
	// Normal, captured in BeginStampGripDrag), not a terrain raycast, for the same reason.
	bool IsStampGripDragActive() const;
	bool TryFindTerrainStampGripAtScreen(APlayerController* PC, const FVector2D& ScreenPos, EIHStampGripHandle& OutHandle, FVector& OutWorldPoint) const;
	void BeginStampGripDrag(APlayerController* PC, EIHStampGripHandle Handle, const FVector& WorldPoint, bool bSymmetric);
	void UpdateStampGripDrag(APlayerController* PC, const FVector2D& ScreenPos, float ScreenMouseDeltaY);
	void EndStampGripDrag();

	void TickTerrainStampManipulation(APlayerController* PC);
	/** W-open + stamp selected: wheel rotate (or shift+wheel scale); blocks fly zoom at FlyPC. */
	void ApplySelectedStampMouseWheel(APlayerController* PC, float WheelDelta);
	void DrawSelectedTerrainStampGizmo(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC = nullptr) const;
	void DrawPlacedTerrainStampPickHints(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC = nullptr) const;
	bool TryRemoveSelectedTerrainStamp();
	void LogTerrainStampReplayHeaderStub() const;

	/** Pops and applies the most recent stamp move/rotate/delete undo record, if any. Returns false
	 * if the stack (scoped to the current stamp-selection session) is empty. */
	bool UndoLastStampAction();

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

	// 2026-09-11: Task 1/2 color spec - while the W (World) tab is open, every placed stamp not
	// individually selected shows IHTerrainStampColors::AllStampsToggleColor; the instant it closes
	// every stamp reverts to its real ASL-band material. Called from both Sync*FlyOutState
	// functions (the two places bFlyOutOpen/ActiveTab actually change) whenever World-tab-open-ness
	// flips, so it never needs its own dedicated call site at every open/close/toggle entry point.
	void RefreshAllTerrainStampsPassiveTint(bool bWorldTabOpen);

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

	// LIFO undo stack for the current stamp-selection session (see FIHStampUndoRecord's own
	// comment). Capped so a very long editing session can't grow this unboundedly.
	TArray<FIHStampUndoRecord> StampUndoStack;
	static constexpr int32 MaxStampUndoStackDepth = 50;
	FTransform PendingMoveDragStartTransform;
	float PendingMoveDragStartDepthCm = 100.f;
	/** 2026-09-17: real-world (undilated) timestamp of the last UpdateStampMoveDrag tick, so the
	 * "molasses" damping's VInterpTo uses a REAL delta time - GetWorld()->GetDeltaSeconds() is
	 * dilated by the dev Game Speed slider (same pitfall already root-caused for double-click
	 * timing elsewhere in this project), so at any speed above 1.0x the eased position would jump
	 * proportionally further per tick, reading as "jumps out of frame" at higher speeds. -1 means
	 * no prior tick yet this drag (BeginStampMoveDrag resets it).
	 */
	float LastStampMoveDragRealTimeSec = -1.f;
	FTransform PendingGripDragStartTransform;

	// Stage 12b: camera-facing drag plane, captured once in BeginStampGripDrag and reused by every
	// UpdateStampGripDrag tick for the X/Y/corner handles (Origin = the grabbed grip's own world
	// position at grab time; Normal = camera forward at grab time).
	FVector GripDragPlaneOrigin = FVector::ZeroVector;
	FVector GripDragPlaneNormal = FVector::ForwardVector;

	void PushStampUndoRecord(const FIHStampUndoRecord& Record);

	/** W-tab stamp double-click tracking — never shared with island LastClickedIslandIndex. */
	TWeakObjectPtr<AIH_TerrainStampActor> LastClickedStamp;
	float LastStampClickTimeSec = -1.f;
	static constexpr float StampDoubleClickWindowSec = 0.45f;

	/**
	 * 2026-09-17: "molasses" fine-positioning feel for UpdateStampMoveDrag (Shift+Drag relocate of an
	 * already-placed stamp over its own island) - lower = heavier/slower catch-up to the cursor.
	 * Deliberately NOT used by the separate W-gallery-to-open-ocean initial placement drag, which
	 * stays snappy on purpose.
	 */
	static constexpr float StampRelocateDampingInterpSpeed = 4.f;

	void CommitStickyBuildDragPlacement(const FVector& DrawCenterWorld, const FVector& ActorOriginWorld);

	void CommitStickyTerrainStampPlacement(
		const FVector& SurfaceWorld,
		class AIH_WB_IslandActor* TargetIsland);

	bool CommitActiveTerrainStampDrop(
		AIH_Cube2FlyPlayerController* FlyPC,
		class AIH_WB_IslandActor* Island,
		const FVector& SurfaceWorld,
		EIHTerrainStampId StampId);

	/** Spawns + initializes a stamp actor (mesh, registration, Outliner label) but does NOT place
	 * it - caller is responsible for either AIH_TerrainStampActor::ApplyWorldSurfacePlacement (a
	 * fresh drop) or SetActorTransform directly (an undo-restore, which must land at the exact
	 * previous pose rather than re-snapping to a fresh surface trace). Shared by
	 * CommitActiveTerrainStampDrop and UndoLastStampAction's Delete-record restore path. */
	AIH_TerrainStampActor* SpawnBareStampActor(UWorld* World, AIH_WB_IslandActor* Island, EIHTerrainStampId StampId);

	static FIHBuildPaletteItemRow MakeSyntheticTerrainStampPaletteRow(EIHTerrainStampId StampId);

	static bool TryGetTerrainStampIdFromRow(const FIHBuildPaletteItemRow& Row, EIHTerrainStampId& OutStampId);

	void RotateSelectedTerrainStamp(float DeltaDeg);
	void ScaleSelectedTerrainStampRadius(float Factor);
	void ApplySelectedTerrainStampTransform();

	static bool TryGetStructureFootprintCm(FName ItemID, FVector& OutExtentCm);

};

