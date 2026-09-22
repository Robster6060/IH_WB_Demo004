// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/PlayerController.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_Cube2FlyPlayerController.generated.h"

class UIH_P1C08_CoastlineTuningWidget;
class UIH_P1C07_SelectionLassoWidget;
class UIH_P1C08_GameSpeedWidget;
class UIH_P1C08_DevViewWidget;
class UIH_P1C08_CameraAslWidget;
class UIH_P1C08_PlaceShipWidget;
class UIH_P1C08_MannequinWidget;
class UIH_P1C08_TopDownViewWidget;
class UIH_P1C08_WeatherPreviewWidget;
class UIH_P1C08_GameDateTimeWidget;
class UIH_P1C08_PlayAtmosphericsWidget;
class UIH_P1C08_DevSeedPanelWidget;
class UIH_P1C08_TemplateGalleryWidget;
class UIH_P1C08_IslandNavWidget;
class UIH_P1C08_IslandCaptionWidget;
class UIH_P1C08_ConfirmRevertWidget;
class UIH_P1C08_IslandEditHintWidget;
class UIH_P1C08_RealmRegenProgressWidget;
class UIH_P1C07_ShipRegistrySubsystem;
class UIH_P1C07_NavAvoidanceSubsystem;
class UIH_P1C08_MinimapSubsystem;
class UIH_BuildPaletteSubsystem;
class AIH_TownGridManager;
class AIH_P1C07_MerchantmanShipActor;
class AIH_StructurePlacementActor;

/** Same free-fly presentation as P1C06 NWG: WASD / arrow keys + QE / PgUp PgDn, RMB look, MMB yaw drag, wheel dolly. */
UCLASS()
class IH_WB_DEMO004_API AIH_Cube2FlyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 2026-09-21: manual, console-only trigger for the camera-settle proximity tessellation
	 * feature (RunProximityTessellation) — deliberately NOT wired into any automatic tick yet.
	 * Type "TestProximityTessellation" (optionally "TestProximityTessellation 5000" for a 50m
	 * radius, default 3000/30m) in the in-game console (~) while looking at an already-baked
	 * island, to directly validate ApplySelectiveTessellation's ConcentricRings pattern produces no
	 * visible cracks at the patch boundary before this gets wired into the real settle-gated tick. */
	UFUNCTION(Exec)
	void TestProximityTessellation(float RadiusCm = 3000.f);
	/** 2026-09-22 (IH_WB_PCG_Architecture_Canon.md, Phase 0): triggers the nearest baked island's
	 * PCGValidationComponent (GenerateOnDemand, does nothing until called) and logs the resulting
	 * point count - confirms PCGDynamicMeshData can actually sample BakedIslandMesh before any real
	 * PGC groundcover graph is built on that assumption. Type "TestPCGGroundcoverValidation" in the
	 * in-game console (~) while near a First-Baked island. */
	UFUNCTION(Exec)
	void TestPCGGroundcoverValidation();
	/** 2026-09-21: keeps the fly camera from dipping below registered island/stamp terrain — called
	 * every PlayerTick. See its own .cpp comment for why it's scoped to island collision specifically
	 * rather than a blanket ECC_WorldStatic trace (the ocean plane shares that channel), and for the
	 * ~10m look-ahead sample along actual travel direction. */
	void ClampFlyCameraAboveTerrain();
	/** Camera world position at the PREVIOUS tick — lets ClampFlyCameraAboveTerrain derive actual
	 * travel direction for its look-ahead sample. Sentinel (TNumericLimits<float>::Max()) means "no
	 * prior tick yet", matching the pattern already used for the same purpose on AIH_WB_IslandActor. */
	FVector LastCameraTickWorldLoc = FVector(TNumericLimits<float>::Max());

	void RequestFocusIsland(int32 IslandIndex);
	/** Pure X/Y recenter over the target island - preserves current camera angle and zoom/altitude. */
	void BeginCameraFlyToIsland(int32 IslandIndex);
	void RequestDeselectIsland();
	/** Clears viewport double-click tracking when W fly-out opens (FIX-001d). */
	void ResetIslandViewportDoubleClickTracking();
	bool ShouldShowIslandSelectionVisual() const { return bShowIslandSelectionVisual; }
	/** PIE gold coast overlay: skip tick invalidation during look, drag, island edit. */
	bool ShouldSuspendWorldCoastStrokeOverlay() const;
	void SetIslandSelectionVisualVisible(bool bVisible);
	void SyncIslandSelectionMeshGlow();
	bool TryGetIslandIndexAtWorldXY(const FVector2D& WorldXY, int32& OutIslandIndex) const;
	/** Polygon interior only — no coastline margin or center-radius fallback. */
	bool TryGetIslandIndexAtWorldXYStrict(const FVector2D& WorldXY, int32& OutIslandIndex) const;
	/** MainCoast scaled toward center by WBIslandSelectCoreFraction (WB island select). */
	bool TryGetIslandIndexAtWorldXYSelectCore(const FVector2D& WorldXY, int32& OutIslandIndex) const;
	/** Inner 60% lane-radius disk — island select/deselect core (excludes near-shore band). */
	bool TryGetIslandIndexAtWorldXYInnerCore(const FVector2D& WorldXY, int32& OutIslandIndex) const;
	bool TryGetIslandIndexAtScreenInnerCore(const FVector2D& ScreenPos, int32& OutIslandIndex) const;
	/** True when cursor is over open ocean (not island surface or generous pick zone). */
	bool TryIsOpenWaterClickAtScreen(const FVector2D& ScreenPos) const;
	/** Surface / select-core resolve for W-closed island double-click. */
	bool TryResolveIslandIndexAtScreen(const FVector2D& ScreenPos, int32& OutIslandIndex) const;

	/** Active min-lane violation flash (viewport + minimap). */
	bool TryGetLaneViolationPair(int32& OutIslandA, int32& OutIslandB) const;

	/** Viewport-local mouse; works with NoCapture (GetMousePosition requires attached mouse). */
	bool TryGetViewportMousePosition(FVector2D& OutViewportPos) const;

	// 2026-09-18: exposed so UIH_BuildPaletteHostWidget can gate diagnostic logging while
	// investigating the "D&D doesn't work in Top Down View" report without spamming Regular View.
	bool IsTopDownViewActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION()
	void HandleMinimapTogglePressed();

	UFUNCTION()
	void HandleBuildPaletteGridTogglePressed();

	/** Ctrl+Z, active only while a Terrain Stamp is selected in W-tab edit mode - LIFO undo of
	 * move/rotate/delete for the current stamp-selection session (2026-09-10). */
	UFUNCTION()
	void HandleStampUndoPressed();

	UFUNCTION()
	void HandlePauseTogglePressed();

private:
	void BindFlyMovementKeys();
	void BindNavDebugToggleKeys();
	void BindGlobalHUDKeys();
	bool IsKeyDownAnywhere(FKey Key) const;
	void TryMinimapToggleFromTick();
	void TryBuildPaletteTabKeysFromTick();
	void HandleBuildPaletteTabKeyPressed(EIHBuildPaletteTab Tab);
	void TickBuildPaletteAndTownGrid(float DeltaTime);
	void TryMinimapCloseFromTick();
	void TryPauseToggleFromTick();
	void ApplyKeyboardFlyMovement(float DeltaTime);
	void ApplyFreeMouseViewportSettings();
	void ApplyPresentationInputMode();
	void ApplyMouseLookInputMode();
	void ReleaseUnwantedMouseCapture();
	void BeginLassoDragCapture();
	void EndLassoDragCapture();
	bool IsHUDSliderConsumingKeyboard() const;
	void TickHUDSliderKeyboardFocus(float DeltaTime);
	void ProcessEarlyHUDPanelPointerDown(const FVector2D& ViewportCur, const FVector2D& CursorAbsolute);
	void CancelActiveHUDKeyboardFocus();
	void HandleBuildPalettePointerPress(const FVector2D& CursorAbsolute);
	void TickHUDSliderPointerMove(const FVector2D& CursorAbsolute);
	void FinishHUDSliderPointerUp(const FVector2D& CursorAbsolute);
	void HandleLeftMouseRelease(const FVector2D& ViewportPick);
	/** W-closed island double-click select + inner-core deselect gate. Returns true when click is consumed. */
	bool TryHandleIslandSelectionClickAtViewport(const FVector2D& ViewportPick);
	/** FIX-001d — delegates to BuildPalette IsViewportIslandSelectionBlocked(). */
	bool IsViewportIslandSelectionBlocked() const;
	bool TryIssueMoveOrderAtScreen(
		const FVector2D& ScreenPos,
		UIH_P1C07_ShipRegistrySubsystem* Registry,
		bool bAppendWaypoint = false);
	void HandleRightMouseReleaseForShipOrders(const FVector2D& ViewportPick);
	AActor* TraceSelectableShipAtScreen(const FVector2D& ScreenPos) const;
	/** Visibility miss fallback: nearest registered ship within ScreenRadiusPx. */
	AActor* FindNearestRegisteredShipAtScreen(const FVector2D& ScreenPos, float ScreenRadiusPx) const;
	bool TryPlaceMerchantmanAtScreen(const FVector2D& ScreenPos);
	/**
	 * 2026-09-13: shared resolution/spawn helpers factored out of TryPlaceMerchantmanAtScreen so the
	 * new Convey (C) flyout's dev Merchantman drag tile spawns with EXACTLY identical placement
	 * behavior to the Place Ship widget - same water-point resolution, same spawn/label/select/log -
	 * with zero duplicated logic to drift out of sync.
	 */
	bool TryResolveShipPlacementWorldPoint(const FVector2D& ScreenPos, FVector& OutPoint) const;
	AIH_P1C07_MerchantmanShipActor* SpawnAndSelectMerchantmanAt(const FVector& SpawnPoint);
	bool TryPlaceMannequinAtScreen(const FVector2D& ScreenPos);

	// 2026-09-12: Mannequin troop movement - mirrors TryIssueMoveOrderAtScreen/
	// HandleRightMouseReleaseForShipOrders exactly, resolving against walkable land (the same
	// multi-hit-trace + IslandActorTag technique TryPlaceMannequinAtScreen already uses) instead of
	// open water. A parallel, independent path - never touches the ship functions above.
	bool TryIssueMannequinMoveOrderAtScreen(
		const FVector2D& ScreenPos,
		class UIH_P1C08_MannequinRegistrySubsystem* Registry,
		bool bAppendWaypoint = false);
	void HandleRightMouseReleaseForMannequinOrders(const FVector2D& ViewportPick);
	AActor* TraceSelectableMannequinAtScreen(const FVector2D& ScreenPos) const;
	bool AbsoluteToViewportLocal(const FVector2D& AbsolutePos, FVector2D& OutViewportPos) const;
	void EnsureViewportKeyboardFocus();
	bool IsLeftMouseButtonDown() const;
	bool IsMiddleMouseButtonDown() const;
	bool IsRightMouseButtonDown() const;
	void ApplyFlyCameraRotationDelta(float DeltaX, float DeltaY, bool bAllowPitch);
	void UpdateNavCollisionDebugDraw(float DeltaTime);
	void TickDevMousePointerEcho(float DeltaTime);

	bool bMouseLookActive = false;
	bool bLassoDragCaptureActive = false;
	bool bLeftMouseDown = false;
	bool bLeftMouseStartedOverMinimap = false;
	bool bMinimapPointerCapture = false;
	bool bBuildPalettePointerCapture = false;
	bool bBuildPaletteDragFromPalette = false;
	bool bHUDSliderPointerCapture = false;
	bool bLeftMouseConsumedByHUDPanel = false;
	bool bPrevLeftMouseDown = false;
	bool bPrevMiddleMouseDown = false;
	bool bPrevRightMouseDown = false;
	bool bPrevMinimapPageUp = false;
	bool bPrevMinimapPageDown = false;
	bool bPrevMinimapKeyDown = false;
	bool bPrevBuildPaletteTabKeyDown[5] = {};
	bool bPrevMinimapCloseKeyDown = false;
	bool bPrevPauseKeyDown = false;
	int32 KeyboardFocusWarmupTicksRemaining = 0;
	int32 MouseCaptureWarmupTicksRemaining = 0;
	TSet<FKey> PressedFlyKeys;
	FVector2D LeftMouseDragStart = FVector2D::ZeroVector;
	FVector2D LeftMouseDragStartAbsolute = FVector2D::ZeroVector;
	FVector2D RightMouseDragStart = FVector2D::ZeroVector;
	/** 2026-09-14: was Shift held at ANY point during the current RMB press-hold-release cycle -
	 * more forgiving than polling Shift fresh at the exact release frame (see PlayerTick). */
	bool bShiftHeldDuringRightMouseHold = false;
	FVector2D PrevMousePixels = FVector2D::ZeroVector;
	TObjectPtr<UIH_P1C07_SelectionLassoWidget> LassoWidget;
	TObjectPtr<UIH_P1C08_CoastlineTuningWidget> CoastlineTuningWidget;
	TObjectPtr<UIH_P1C08_GameSpeedWidget> GameSpeedWidget;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_DevViewWidget> DevViewWidget;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_PlaceShipWidget> PlaceShipWidget;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_MannequinWidget> MannequinWidget;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_TopDownViewWidget> TopDownViewWidget;

	/** Pawn transform saved on Top Down View toggle-ON, restored on toggle-OFF. */
	FVector PreTopDownViewLocation = FVector::ZeroVector;
	FRotator PreTopDownViewRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_CameraAslWidget> CameraAslWidget;
	TObjectPtr<UIH_P1C08_WeatherPreviewWidget> WeatherPreviewWidget;
	TObjectPtr<UIH_P1C08_GameDateTimeWidget> GameDateTimeWidget;
	TObjectPtr<UIH_P1C08_PlayAtmosphericsWidget> PlayAtmosphericsWidget;
	TObjectPtr<UIH_P1C08_DevSeedPanelWidget> DevSeedPanelWidget;
	TObjectPtr<UIH_P1C08_TemplateGalleryWidget> TemplateGalleryWidget;
	TObjectPtr<UIH_P1C08_IslandNavWidget> IslandNavWidget;
	void HandleIslandSelectionChanged(int32 IslandIndex);
	void TickCameraFly(float DeltaTime);
	FVector ComputeIslandCaptionAnchorCm(int32 IslandIndex) const;
	void ShowIslandCaptionForNavIndex(int32 IslandIndex);

	void ApplyPendingSelectionChange();
	void ShowConfirmRevertDialog(TFunction<void(bool bRevertConfirmed)> OnComplete);

public:
	/**
	 * New Convey (C) flyout dev tile entry point, called from UIH_BuildPaletteSubsystem's generic
	 * DropActor commit path - see TryResolveShipPlacementWorldPoint/SpawnAndSelectMerchantmanAt.
	 */
	bool TrySpawnMerchantmanAtScreen(const FVector2D& ScreenPos);
	void ShowConfirmDialog(
		const FString& Title,
		const FString& Body,
		TFunction<void(bool bConfirmed)> OnComplete);
	bool HasUncommittedIslandDraft() const;
	void CommitActiveIslandDraft();
	void RevertActiveIslandDraft();
	void CommitSelectionChange(int32 NewIslandIndex);
	void RequestRegenerateIslandsFromSeed(TFunction<void()> OnComplete = TFunction<void()>());
	/** Show progress immediately, then defer SetCurrentWorldSeed + island regen (Phase1 is slow). */
	void PrepareRealmRegenFromSeed(const FString& NormalizedSeed, TFunction<void()> OnComplete = TFunction<void()>());
	void StartRealmRegenWork(bool bShowProgress = true);
	void HandleSeedPanelRegenPrepareTick();
	void BeginRealmRegenProgress(const FString& Label);
	void EndRealmRegenProgress();
	void ScheduleEndRealmRegenProgress();
	void HandleRealmRegenWorkTick();
	void HandleRealmRegenFinishTimer();
	void TickRealmRegenFakeProgress();
	void UpdateEditingHint();
	void RefreshDevPanelStackLayout();
	void RefreshIslandNavFromSubsystem();
	void HandleManualTransformChanged(int32 IslandIndex);
	void ApplyDraftTransformPreview(int32 IslandIndex, bool bRefreshMinimap = true);
	bool TryGetIslandIndexAtScreen(const FVector2D& ScreenPos, int32& OutIslandIndex) const;
	bool TryGetWorldPointOnWaterPlane(const FVector2D& ScreenPos, FVector& OutPoint) const;
	bool TryTraceTerrainAtScreen(const FVector2D& ScreenPos, FVector& OutImpactPoint) const;

	bool DeprojectScreenToWorldRay(const FVector2D& ScreenPos, FVector& OutOrigin, FVector& OutDirection) const;

	/** Visibility/land traces only — no water-plane fallback. */
	bool TryTraceSolidSurfaceAtScreen(const FVector2D& ScreenPos, FVector& OutImpactPoint) const;

	/** Island collision surface under cursor (false over open ocean). */
	bool TrySampleIslandSurfaceAtScreen(
		const FVector2D& ScreenPos, FVector& OutIslandSurface, AActor** OutIslandActor = nullptr) const;

	/** Island surface sample + actor-root Z for Build palette drag/drop. */
	bool TryResolveStructurePlacementAtScreen(
		const FVector2D& ScreenPos, FName PaletteItemID, FVector& OutActorOriginWorld) const;

	/** Same as screen resolve, but from minimap / map-local world XY. */
	bool TryResolveStructurePlacementAtWorldXY(
		const FVector2D& WorldXY, FName PaletteItemID, FVector& OutActorOriginWorld) const;

	/** Dev-only world-space pointer echo (called from build drag ghost path). */
	void DrawDevMousePointerEchoWorld(UWorld* World, const FVector2D& ViewportCur, float DeltaTime);

	bool TryFindTownGridManagerAtScreen(const FVector2D& ScreenPos, AIH_TownGridManager*& OutManager) const;
	void SelectTownGridManager(AIH_TownGridManager* Manager);
	void DeselectTownGridManager();
	AIH_TownGridManager* GetSelectedTownGridManager() const { return SelectedTownGridManager.Get(); }

	/** 2026-09-13: selectable-actor-hierarchy - Structure (B) double-click select, mirrors Town Grid. */
	AIH_StructurePlacementActor* TraceSelectableStructureAtScreen(const FVector2D& ScreenPos) const;
	void SelectStructurePlacement(AIH_StructurePlacementActor* Structure);
	void DeselectStructurePlacement();
	AIH_StructurePlacementActor* GetSelectedStructurePlacement() const { return SelectedStructurePlacement.Get(); }
	void TickIslandManipulationInput(float DeltaTime);
	void TickIslandManipulationGizmo(float DeltaTime);
	void TickTerrainStampManipulationInput(float DeltaTime);
	void TickTerrainStampManipulationGizmo(float DeltaTime);
	void DrawLaneViolationFlash(float DeltaTime);
	bool ValidateAndApplyDraftOffset(int32 IslandIndex, const FVector2D& ProposedOffsetCm);
	bool IsIslandOffsetPlacementValid(int32 IslandIndex, const FVector2D& ProposedOffsetCm, int32& OutViolatingIndex) const;
	void ApplyIslandDragOffsetPreview(int32 IslandIndex, const FVector2D& ProposedOffsetCm);
	void FinalizeIslandDrag(int32 IslandIndex);
	float ComputeWorldSizeForScreenPixels(const FVector& WorldPoint, float ScreenPixels) const;
	bool IsScreenPointOverInteractiveHUDPanel(const FVector2D& CursorAbsolute) const;
	FVector2D GetLastValidDraftOffsetCm(int32 IslandIndex) const;
	void SetLastValidDraftOffsetCm(int32 IslandIndex, const FVector2D& OffsetCm);

	bool bCameraFlyActive = false;
	float CameraFlyElapsedSec = 0.f;
	FVector CameraFlyStartLoc = FVector::ZeroVector;
	FVector CameraFlyTargetLoc = FVector::ZeroVector;
	FRotator CameraFlyStartRot = FRotator::ZeroRotator;
	FRotator CameraFlyTargetRot = FRotator::ZeroRotator;

	static constexpr float IslandCameraFlyDurationSec = 1.5f;
	static constexpr float IslandCameraPitchDeg = -15.f;
	static constexpr float IslandCameraDistanceScale = 1.35f;
	static constexpr float IslandCameraMinDistanceCm = 80000.f;
	/** Near-sea perspective FOV (deg). High ASL lerps down to kill fish-eye on flat ocean. */
	static constexpr float FlyFovNearSeaDeg = 90.f;
	static constexpr float FlyFovHighAltDeg = 58.f;
	static constexpr float FlyFovLerpStartAslM = 1500.f;
	static constexpr float FlyFovLerpEndAslM = 5500.f;

	TObjectPtr<UIH_P1C08_IslandCaptionWidget> IslandCaptionWidget;
	TObjectPtr<UIH_P1C08_ConfirmRevertWidget> ConfirmRevertWidget;
	TObjectPtr<UIH_P1C08_IslandEditHintWidget> IslandEditHintWidget;
	TObjectPtr<UIH_P1C08_RealmRegenProgressWidget> RealmRegenProgressWidget;
	FTimerHandle RealmRegenProgressTimer;
	FTimerHandle RealmRegenFinishTimer;
	FTimerHandle RealmRegenWorkTimer;
	TFunction<void()> PendingRealmRegenCompleteCallback;
	FString PendingRegenSeedWord;
	float RealmRegenProgressShownAt = 0.f;
	float RealmRegenFakeProgress = 0.f;
	/** Kept for API compat; overlay min-display delay retired (slim status path). */
	static constexpr float RealmRegenProgressMinDisplaySec = 0.f;

	FDelegateHandle IslandNavChangedHandle;
	FDelegateHandle IslandSelectionChangedHandle;
	FDelegateHandle ManualTransformChangedHandle;

	int32 PendingSelectionIndex = INDEX_NONE;
	bool bAwaitingConfirmRevert = false;
	bool bShowIslandSelectionVisual = false;

	int32 LastClickedIslandIndex = INDEX_NONE;
	float LastIslandClickTimeSec = -1.f;
	bool bIslandDragActive = false;
	int32 IslandDragIndex = INDEX_NONE;
	FVector2D IslandDragStartWorldCm = FVector2D::ZeroVector;
	FVector2D IslandDragStartOffsetCm = FVector2D::ZeroVector;
	TMap<int32, FVector2D> LastValidDraftOffsetCm;
	int32 LaneFlashOtherIndex = INDEX_NONE;
	float LaneFlashRemainingSec = 0.f;

	TWeakObjectPtr<AIH_TownGridManager> SelectedTownGridManager;
	bool bTownGridMovePointerCapture = false;
	bool bStampMovePointerCapture = false;
	bool bStampGripPointerCapture = false;
	TWeakObjectPtr<AIH_StructurePlacementActor> SelectedStructurePlacement;
	bool bStructureMovePointerCapture = false;
	static constexpr float StructureWheelRotateDeg = 5.f;

	// 2026-09-13: selectable-actor-hierarchy - Ship/Mannequin/Town Grid select gesture
	// standardized to double-click, matching Island/Terrain Stamp (mirrors LastClickedIslandIndex/
	// LastIslandClickTimeSec's own GetRealTimeSeconds()-based pattern, immune to the Game Speed
	// slider's time dilation).
	TWeakObjectPtr<AActor> LastClickedShip;
	float LastShipClickTimeSec = -1.f;
	TWeakObjectPtr<AActor> LastClickedMannequin;
	float LastMannequinClickTimeSec = -1.f;
	TWeakObjectPtr<AIH_TownGridManager> LastClickedTownGridForDoubleClick;
	float LastTownGridClickTimeSec = -1.f;
	TWeakObjectPtr<AIH_StructurePlacementActor> LastClickedStructure;
	float LastStructureClickTimeSec = -1.f;

	static constexpr float ActorDoubleClickWindowSec = 0.45f;
	static constexpr float IslandDoubleClickWindowSec = 0.45f;
	static constexpr float IslandRotateStepDeg = 5.f;
	static constexpr float IslandShiftWheelRotateDeg = 3.f;
	static constexpr float LaneFlashDurationSec = 5.f;
	static constexpr float GizmoMoveHandleRadiusCm = 8000.f;
	static constexpr float GizmoYawRingRadiusCm = 14000.f;
	static constexpr float IslandCoastlinePickThresholdCm = 3500.f;
	/** Legacy inner-core fraction (edit/deselect); select uses WBIslandSelectCoreFraction. */
	static constexpr float IslandInnerPickRadiusFraction = 0.6f;
	/** Screen-space ship pick slop when Visibility hull miss. */
	static constexpr float ShipScreenPickRadiusPx = 40.f;
	static constexpr float SelectionRingLiftCm = 0.1f;
	static constexpr float SelectionRingScreenThicknessPx = 6.f;
	static constexpr float SelectionRingDashLengthPx = 10.f;
	static constexpr float SelectionRingDashGapPx = 8.f;

	bool bNavDebugDrawPersistent = false;
	float NavDebugDrawRemainingSec = 0.f;

	static constexpr float NavDebugDrawAfterMoveOrderSec = 15.f;

	static constexpr float DragSelectThresholdPx = 12.f;

	static constexpr float ZoomCmPerWheelUnit = 6720.f;
	static constexpr float PanCmPerMousePixel = 14.f;
	static constexpr float LookDegPerMousePixel = 0.22f;
	static constexpr float KeyboardFlySpeedCmPerSec = 28000.f;
	/** 2026-08-28: 1.0 = fully lock the sea-plane crosshair point during PageUp/PageDown (the exact
	 * geometric correction), 0.0 = pure vertical with no compensation at all. User-tuned down from
	 * 1.0 after reporting the full lock felt like "too much forward motion... I fly over my target"
	 * — a real, expected consequence of exactly re-deriving the ground point every tick at shallower
	 * pitch angles, not a bug. Re-tune this single constant on further feedback rather than the
	 * underlying math. */
	static constexpr float PgUpDownGroundLockStrength = 0.35f;
};
