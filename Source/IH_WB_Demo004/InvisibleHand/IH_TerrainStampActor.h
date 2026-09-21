// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "FIHTerrainStampTypes.h"
#include "GameFramework/Actor.h"
#include "IH_TerrainStampActor.generated.h"

class AIH_WB_IslandActor;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;

// Stage 12b (2026-09-11): which bounding-box scale grip (if any) is active. X/Y/corner grips
// pin the opposite face/corner in world space by default (one-sided pull) or grow symmetrically
// about the pivot with Ctrl held; Top (Z) is always one-sided - growing a landform's height
// downward would sink it into the terrain, so there's no symmetric variant for it.
enum class EIHStampGripHandle : uint8
{
	None, PosX, NegX, PosY, NegY, Top, CornerPP, CornerPN, CornerNP, CornerNN
};

// Maps a grip handle to which local axes it affects and in which signed direction (0 = untouched,
// +-1 = elongate that axis in that direction) - drives the generic masked-delta math in
// AIH_TerrainStampActor::UpdateGripDrag. Mirrors the old lab project's FIHBoundingBoxGripInfo
// concept, without needing its per-instance descriptor array since this actor only ever has the
// 9 grips below (a fixed layout, not one generated per ring/spline).
FVector IHStampGripHandleToSignedAxisMask(EIHStampGripHandle Handle);

/**
 * Terrain Stamp actor (2026-09-09: real static-mesh path landed, replacing the old procedural
 * height-grid stub). Wraps a UStaticMeshComponent sourced from FIHTerrainStampMeshCatalog/
 * DT_TerrainStamp - Nanite and collision are both baked into the imported mesh asset itself, no
 * per-component setup needed here. ApplyWorldSurfacePlacement resolves a real depth-constrained
 * placement (lowest point ~1m under the sampled surface, never floating with a gap; a safety
 * backstop keeps the highest point from ever sinking below that same line) and, on a real (non-
 * drag-preview) placement, registers with UIH_P1C07_IslandCollisionSubsystem so Ship/Mannequin
 * placement tools recognize the stamp the same way they recognize IslandMesh/ShelfMesh.
 */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_TerrainStampActor : public AActor
{
	GENERATED_BODY()

public:
	AIH_TerrainStampActor();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void InitializeStamp(EIHTerrainStampId InStampId, bool bInInvertHeight = false, bool bInGalleryPreviewOnly = false);
	void SetTargetIsland(AIH_WB_IslandActor* InIsland);
	void SetDragPreviewMode(bool bInDragPreview);
	bool IsDragPreview() const { return bIsDragPreview; }
	void ApplyWorldSurfacePlacement(AIH_WB_IslandActor* InIsland, const FVector& SurfaceWorld);
	// 2026-09-11: "merge into IslandMesh" canon - Shift+Drag's vertical mouse movement pushes the
	// stamp deeper into the surface (never shallower than DefaultDepthBelowSurfaceCm - that would
	// create a gap) on top of whatever X/Y repositioning the same drag is already doing. Called
	// BEFORE ApplyWorldSurfacePlacement each drag tick so the new depth is already in effect when
	// that recomputes the actor's Z. TotalScreenDeltaYFromDragStart is the TOTAL vertical distance
	// moved since the drag began (current cursor Y minus mouse-down Y), not a per-tick delta -
	// re-derived fresh from BaselineDepthCm every tick like every other grip's scale math, rather
	// than accumulated frame to frame, so it can't drift/amplify tiny per-frame mouse jitter into a
	// visibly jerky sink (an early per-tick-accumulation version of this had exactly that problem).
	// Positive delta (mouse moved down) sinks deeper.
	void SetManualSinkDepthFromDragTotal(float BaselineDepthCm, float TotalScreenDeltaYFromDragStart);
	float GetCurrentDepthBelowSurfaceCm() const { return CurrentDepthBelowSurfaceCm; }
	// Exposed for undo restore (FIHStampUndoRecord::PreviousDepthBelowSurfaceCm) - sink depth is
	// real per-actor state, not derived from the transform alone, so restoring a transform via
	// SetActorTransform doesn't roll it back on its own.
	void SetCurrentDepthBelowSurfaceCm(float NewDepthCm) { CurrentDepthBelowSurfaceCm = FMath::Max(DefaultDepthBelowSurfaceCm, NewDepthCm); }
	int32 ApplyToHeightGrid() { return 0; }
	void CommitStampToTargetIsland() {}
	void RefreshPreviewMesh() {}
	// 2026-09-10: was a no-op stub - rotation (via "R"/Shift+R keys or Shift+Wheel,
	// UIH_BuildPaletteSubsystem::RotateSelectedTerrainStamp) updated StampRotationDeg but this never
	// actually turned the actor, so rotation never visibly did anything for any stamp.
	void SyncStampActorYaw();
	// 2026-09-10: real selection-tint implementation (was a bare bool flip). Tints
	// IHTerrainStampColors::SelectedStampColor via a UMaterialInstanceDynamic on select, mirroring
	// AIH_StructurePlacementActor's own drag-ghost-tint pattern; restores the cached original
	// materials on deselect.
	void SetStampSelected(bool bInSelected);
	bool IsStampSelected() const { return bStampSelected; }

	// 2026-09-11: WHILE the W (World) build-palette tab is open, every placed stamp not
	// individually selected shows IHTerrainStampColors::AllStampsToggleColor so players can tell
	// stamps apart from natural terrain while editing; the instant the panel closes every stamp
	// reverts to its real ASL-band material regardless of selection. Selection (SetStampSelected)
	// always wins over this when both are true - see RefreshStampVisualTint.
	void SetPassiveToggleTinted(bool bInTinted);

	// Stage 12b (2026-09-11): whole-stamp non-uniform scale via 9 bounding-box grips (X/Y/Z face
	// centers + 4 XY corners), shown/hidden together with the selection tint. Hit-testing lives in
	// UIH_BuildPaletteSubsystem/the PlayerController (screen-space projection - a click that lands
	// on empty space just past a tapered mesh's silhouette, e.g. a corner grip on a dome-shaped
	// stamp, still needs to register, which a 3D-world-point trace/distance check against the
	// terrain surface can't guarantee), so these accessors expose the cached per-grip world
	// positions/handles that RegenerateBoundingBoxGizmo keeps up to date. Begin/Update/End mirror
	// the existing Move-drag split of responsibility: the actor owns the live drag math,
	// UIH_BuildPaletteSubsystem owns undo-push and height-grid reapply.
	const TArray<EIHStampGripHandle>& GetGripMarkerHandles() const { return GripMarkerHandles; }
	const TArray<FVector>& GetGripMarkerWorldPositions() const { return GripMarkerWorldPositions; }
	// WorldPoint is the grip's OWN world position at grab time (from GetGripMarkerWorldPositions) -
	// also becomes this drag's fixed reference point for the delta math in UpdateGripDrag.
	void BeginGripDrag(EIHStampGripHandle Handle, const FVector& WorldPoint, bool bSymmetric);
	// WorldPointForXY drives every grip except Top: the caller resolves it each tick via a ray
	// against a camera-facing plane through the grip's own drag-start world position (not a terrain
	// raycast - the grip may hover over empty space past a tapered mesh's silhouette, e.g. a corner
	// grip on a dome-shaped stamp, where a terrain trace would either miss or hit ground far below).
	// ScreenMouseDeltaYForZ drives Top directly from raw per-tick screen-space mouse delta instead -
	// the same technique already used for free-mouse camera pan (IH_Cube2FlyPlayerController.cpp's
	// MouseDragDelta.Y * cm-per-pixel), since a horizontal delta can't carry a vertical one anyway.
	void UpdateGripDrag(const FVector& WorldPointForXY, float ScreenMouseDeltaYForZ);
	void EndGripDrag();
	bool IsGripDragActive() const { return ActiveGripHandle != EIHStampGripHandle::None; }
	EIHStampGripHandle GetActiveGripHandle() const { return ActiveGripHandle; }

	bool IsGalleryPreviewOnly() const { return bGalleryPreviewOnly; }
	int32 GetPreviewMeshVertexCount() const { return 0; }
	float GetPreviewFootprintRadiusCm() const { return FMath::Max(100.f, RadiusKm * 100000.f); }
	EIHTerrainStampId GetStampId() const { return StampId; }
	bool IsInvertHeight() const { return bInvertHeight; }
	AIH_WB_IslandActor* GetTargetIsland() const { return TargetIsland.Get(); }
	FVector2D GetCenterLocalCmOnIsland() const { return FVector2D::ZeroVector; }
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	// 2026-09-10: exposed publicly for UIH_BuildPaletteSubsystem::UndoLastStampAction's Delete-record
	// restore path, which sets the respawned actor's transform directly (SetActorTransform, not
	// ApplyWorldSurfacePlacement - an undo-restore must land at the exact previous pose, not re-snap
	// to a fresh surface trace) and so needs to register island-collision itself instead of relying
	// on ApplyWorldSurfacePlacement's own call to this. Idempotent either way (bRegisteredWithIslandCollision guard).
	void RegisterWithIslandCollisionIfNeeded();

	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	EIHTerrainStampId StampId = EIHTerrainStampId::Hill;

	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	bool bInvertHeight = false;

	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	float RadiusKm = 0.18f;

	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	float AmplitudeAzgaar = 18.f;

	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	float StampRotationDeg = 0.f;

	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	bool bGalleryPreviewOnly = false;

	UPROPERTY(Transient)
	bool bIsDragPreview = false;

	bool bStampSelected = false;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<AIH_WB_IslandActor> TargetIsland;

	// The stamp's lowest local-space point rests AT LEAST this far under the sampled surface -
	// never floating with a gap (user's own figure, 2026-09-09). Never allowed to go below this
	// floor, but CAN go deeper - see CurrentDepthBelowSurfaceCm.
	static constexpr float DefaultDepthBelowSurfaceCm = 100.f;

	// 2026-09-11: adjustable per-stamp sink depth ("merge into IslandMesh" canon request) - starts
	// at the fixed default above, adjustable deeper (never shallower - that would create a gap) via
	// Shift+Drag's vertical mouse delta (AdjustManualSinkDepth). Persists as real per-actor state
	// (like StampRotationDeg), not reset each drag session.
	float CurrentDepthBelowSurfaceCm = DefaultDepthBelowSurfaceCm;

	// Absolute safety backstop (see ApplyWorldSurfacePlacement): the HIGHEST point must always stay
	// at least this far ABOVE the sampled surface, regardless of how deep CurrentDepthBelowSurfaceCm
	// has grown - "never fully invisible" needs a floor independent of the sink depth itself, since
	// the old backstop (anchored to DepthBelowSurfaceCm) would sink right along with it otherwise.
	static constexpr float MinVisibleTopMarginCm = 50.f;

	// 2026-09-18: sink depth above is derived from a SINGLE origin-point raycast, which can't keep
	// the whole mesh buried on sloped terrain - one side of the footprint can poke above the surface,
	// showing a harsh visible seam instead of a blended/welded transition (user's explicit ask).
	// ApplyWorldSurfacePlacement additionally samples the stamp's actual current footprint (8 rotated
	// bounding-box perimeter points, tracking Stage 12b's non-uniform scale/rotation) and, independent
	// of the single-point sink math above, pushes the mesh deeper if needed so its bottom stays at
	// least this far under the LOWEST point found anywhere under the footprint - a flat plane below
	// the lowest sampled point is automatically below every higher one too, guaranteeing no gap.
	UPROPERTY(EditAnywhere, Category = "IH|TerrainStamp")
	float MinFootprintBurialDepthCm = 2000.f;

	// Have we registered MeshComponent with UIH_P1C07_IslandCollisionSubsystem? Only true once
	// placed for real (never during drag-preview) - tracked so EndPlay only unregisters when it
	// actually registered.
	bool bRegisteredWithIslandCollision = false;

	void UnregisterFromIslandCollisionIfNeeded();

	// Materials cached the moment ANY tint (selected or passive-toggle) is first applied, so
	// reverting to the real material restores exactly what was there before - not necessarily
	// always the same asset (a future round could swap stamp materials while tinted).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> CachedSourceMaterials;

	// 2026-09-11: true while the W tab is open and this stamp isn't the individually-selected one.
	bool bPassiveToggleTinted = false;

	// Single source of truth for what a stamp's materials should show right now, given
	// bStampSelected/bPassiveToggleTinted: Selected > PassiveToggle > real material. Handles the
	// cache-on-first-tint / restore-on-no-tint bookkeeping so SetStampSelected and
	// SetPassiveToggleTinted don't duplicate it.
	void RefreshStampVisualTint();

	// 2026-09-18: returns 8 world-space XY OFFSET vectors (relative displacements, NOT absolute
	// points - ApplyWorldSurfacePlacement is called with a CANDIDATE origin the actor hasn't moved to
	// yet, so these must be added to that candidate, not to GetActorLocation()) around the mesh's
	// current rotated bounding-box perimeter (4 corners + 4 edge midpoints). Uses the same
	// Bounds/RelativeScale3D pattern already read in ApplyWorldSurfacePlacement (BoxExtent.X/Y this
	// time, not .Z) plus the actor's current yaw - used to find the lowest terrain point anywhere
	// under the footprint, not just at the origin.
	TArray<FVector2D> ComputeFootprintSampleXYPoints() const;

	// --- Stage 12b bounding-box scale grips (2026-09-11) ---

	// Both children of SceneRoot (NOT MeshComponent) so their own on-screen size stays constant
	// regardless of the stamp's current scale - the old lab project's grips were children of its
	// scaled mesh and needed a whole counter-scale-by-geometric-mean scheme to stay a constant
	// visual size; parenting to the unscaled root sidesteps that entirely.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BoundingBoxMesh;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> GripMarkers;

	// Parallel to GripMarkers - which handle each marker/cached world position represents, and
	// where it currently sits, refreshed every RegenerateBoundingBoxGizmo() call.
	TArray<EIHStampGripHandle> GripMarkerHandles;
	TArray<FVector> GripMarkerWorldPositions;

	static constexpr float TerrainStampHeightDragCmPerPixel = 150.f;
	static constexpr float MinStampScaleFactor = 0.4f;
	static constexpr float MaxStampScaleFactor = 3.0f;

	// 2026-09-18 (user request): bounds absolute per-axis size but says nothing about PROPORTION -
	// X could sit at 0.4x while Y sits at 3.0x, a 7.5:1 spread with no natural landform analog
	// (unnaturally thin wedge/pancake shapes). MaxStampAxisRatio caps how far any two axes' SCALE
	// FACTORS (relative to the mesh's OWN natural per-axis proportions, not raw extent) may drift
	// apart from each other - a naturally elongated mesh (Ridge/Canyon/Escarpment) keeps its own
	// aspect ratio at the default (1,1,1) scale; grips can then only warp that up to this much
	// further out of proportion. See UpdateGripDrag's post-pass.
	static constexpr float MaxStampAxisRatio = 2.0f;

	// Recomputes BoundingBoxMesh/GripMarkers positions (and GripMarkerWorldPositions, read via
	// GetGripMarkerWorldPositions() for hit-testing) from the mesh's current local extent - called
	// once on select and again after every grip-drag update.
	void RegenerateBoundingBoxGizmo();

	void UpdateBoundingBoxGizmoVisibility();

	EIHStampGripHandle ActiveGripHandle = EIHStampGripHandle::None;
	bool bGripDragSymmetric = false;
	FVector DragStartMeshScale3D = FVector::OneVector;
	FVector DragStartLocalExtent = FVector::ZeroVector;
	FVector DragStartWorldPoint = FVector::ZeroVector;

	// Captured once at BeginGripDrag (non-symmetric drags only): the opposite face/corner's FIXED
	// world position at drag-start. UpdateGripDrag recomputes that point's CURRENT local position
	// fresh every tick (from whatever the mesh has grown to) and compares it against this fixed
	// target - the difference is exactly the actor-location shift needed to hold it in place. The
	// old lab project's pin-opposite-face technique, generalized here to all 9 handles (including
	// Top) uniformly.
	FVector DragPinWorldPosition = FVector::ZeroVector;
};
