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

/** Same free-fly presentation as P1C06 NWG: WASD / arrow keys + QE / PgUp PgDn, RMB look, MMB yaw drag, wheel dolly. */
UCLASS()
class IH_WB_DEMO004_API AIH_Cube2FlyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
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
	bool AbsoluteToViewportLocal(const FVector2D& AbsolutePos, FVector2D& OutViewportPos) const;
	void EnsureViewportKeyboardFocus();
	bool IsLeftMouseButtonDown() const;
	bool IsMiddleMouseButtonDown() const;
	bool IsRightMouseButtonDown() const;
	void ApplyFlyCameraRotationDelta(float DeltaX, float DeltaY, bool bAllowPitch);
	void UpdateNavCollisionDebugDraw(float DeltaTime);
	void TickDevMousePointerEcho(float DeltaTime);
	void DrawDevIslandClickBursts(UWorld* World, float DeltaTime);
	void SpawnDevIslandClickBurst(const FVector& WorldLocation);

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
	FVector2D PrevMousePixels = FVector2D::ZeroVector;
	TObjectPtr<UIH_P1C07_SelectionLassoWidget> LassoWidget;
	TObjectPtr<UIH_P1C08_CoastlineTuningWidget> CoastlineTuningWidget;
	TObjectPtr<UIH_P1C08_GameSpeedWidget> GameSpeedWidget;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_DevViewWidget> DevViewWidget;

	UPROPERTY(Transient)
	TObjectPtr<UIH_P1C08_PlaceShipWidget> PlaceShipWidget;

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

	struct FDevIslandClickBurst
	{
		FVector Location = FVector::ZeroVector;
		float AgeSeconds = 0.f;
	};

	TArray<FDevIslandClickBurst> ActiveDevClickBursts;

	static constexpr float NavDebugDrawAfterMoveOrderSec = 15.f;

	static constexpr float DragSelectThresholdPx = 12.f;

	static constexpr float ZoomCmPerWheelUnit = 6720.f;
	static constexpr float PanCmPerMousePixel = 14.f;
	static constexpr float LookDegPerMousePixel = 0.22f;
	static constexpr float KeyboardFlySpeedCmPerSec = 28000.f;
};
