// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IHP1C10_AzgaarTypes.h"
#include "FIHTerrainStampTypes.h"
#include "IHCoastGenerationTypes.h"
#include "IHDevViewRuntime.h"
#include "IH_WB_IslandActor.generated.h"

class UProceduralMeshComponent;
class USceneComponent;
class UArrowComponent;
class UIH_P1C08_MinimapSubsystem;
class AIH_TerrainStampActor;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UDynamicMeshComponent;

/** One groundcover-eligible triangle, cached once per island so PGC proximity refreshes never have
 * to re-classify terrain or re-walk DT_ASLSlopeBiome after the initial cache build.
 * 2026-09-19: an earlier version of this cache also bucketed triangles into a fixed-size grid
 * keyed by centroid, and copied each triangle's own DT_BiomeRecommendations groundcover tag list
 * inline. Both were wrong: this project's terrain triangles average ~1,600 sq m each (confirmed
 * via PIE log - Fougeres alone has 287,334 triangles across 462M sq m) - far bigger than any
 * sane streaming-cell size, so centroid-only bucketing left most of a huge triangle's real
 * footprint undiscoverable from the cells the player was actually standing in (root cause of PGC
 * showing zero instances everywhere in PIE). And copying a TArray<FName> per triangle meant one
 * heap allocation per triangle - ~600K of them across 3 islands - a real contributor to the
 * reported memory pressure warning. Fixed by (a) storing BoundingRadiusCm so a proximity check can
 * conservatively test "does this triangle's footprint possibly reach the stream radius", not just
 * its centroid, and (b) storing only a shared BiomeRowIndex (resolved back to
 * DT_BiomeRecommendations only for the handful of triangles actually near the camera each
 * refresh), not a per-triangle tag copy. This is also closer to how UE's own PCG runtime
 * generation actually works: a coarse streaming grid gates WHICH area is active, but the content
 * within an active area is sampled fresh against the real surface, not indexed by a pre-bucketed
 * grid sized independently of the source geometry. */
struct FIHPGCEligibleTri
{
	FVector P0 = FVector::ZeroVector;
	FVector P1 = FVector::ZeroVector;
	FVector P2 = FVector::ZeroVector;
	FVector Centroid = FVector::ZeroVector;
	float AreaSqM = 0.f;
	/** Max distance from Centroid to any of the 3 vertices - lets a proximity check conservatively
	 * include a triangle whose centroid is just outside the stream radius but whose real footprint
	 * still reaches it. */
	float BoundingRadiusCm = 0.f;
	/** Index into GetBiomeRowsSortedForClassification()'s Rows array - resolved back to a real
	 * FIHASLSlopeBiomeRow/FIHBiomeRecommendationsRow only when this triangle is actually near the
	 * camera, not cached per-triangle. */
	int32 BiomeRowIndex = INDEX_NONE;
};

/**
 * Detachable IslandMesh + contiguous Sea Shelf WWF actor.
 * Terrain from the Azgaar-style FDelaunay2 cell graph (FIHTerrainCellGraphGenerator +
 * FIHTerrainCellDiffusion) — see BuildMeshesFromCellGraph.
 * Does NOT use Arbor floating-iceberg / SeaRoots frustum generation.
 */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_WB_IslandActor : public AActor
{
	GENERATED_BODY()

public:
	AIH_WB_IslandActor();

	void GetShorelinePolygonWorldCm(TArray<FVector2D>& OutWorldCm) const;
	void GetSelectionRingWorldCm(TArray<FVector2D>& OutWorldCm) const;
	void GetWaterlineFootprintCm(float& OutSemiMajorCm, float& OutSemiMinorCm) const;
	float GetCoastEnvelopeWorldCm() const { return CachedCoastEnvelopeWorldCm; }
	float GetSemiMajorAxisCm() const { return SemiMajorAxisCm; }
	float GetSummitTopZCm() const { return SummitTopZCm; }
	/** Island slot index — not the layout/aquarium AABB. */
	int32 GetTankIslandIndex() const { return TankIslandIndex; }
	const FIHSeaRootsExtent& GetSeaRootsExtent() const { return SeaRootsExtent; }
	bool HasSeaRootsExtent() const { return bHasSeaRootsExtent; }
	const TArray<FVector2D>& GetMainCoastPolylineLocalCm() const { return MainCoastPolylineLocalCm; }
	const TArray<FIHRiverTerminusSocket>& GetRiverTerminusSockets() const { return RiverTerminusSockets; }
	/** Plan Addendum 10: cell-averaged center of the main landmass - robust to concave coastlines,
	 * unlike a coastline-polygon centroid. Anchors the selection reticle and the island caption. */
	FVector GetMainLandCentroidWorldCm() const
	{
		const FVector2D WorldXY = LocalCmToWorldCm(MainLandCentroidLocalCm);
		return FVector(WorldXY.X, WorldXY.Y, GetActorLocation().Z);
	}
	/** Plan Addendum 11: realized footprint radius (max distance from the true landmass center to
	 * any of its own land cells) - use for camera/reticle framing instead of SemiMajorAxisCm (the
	 * pre-generation layout envelope, which can be much larger than what actually got rendered). */
	float GetMainLandFootprintRadiusCm() const { return MainLandFootprintRadiusCm; }
	/** Plan Addendum 10: lets the player controller rescale the reticle per-tick for a constant
	 * on-screen size regardless of camera distance (mirrors the move-gizmo's own sizing). */
	UArrowComponent* GetSelectionReticleComponent() const { return SelectionReticle; }

	bool HasCellHeightGrid() const { return false; }
	void SetSelectionHighlighted(bool bHighlighted);

	void MarkCoastDirtyFromStamp() {}
	void TickStampRecompute(float /*DeltaTime*/) {}
	void ApplyDevContoursVisibility(bool bVisible);
	void ApplyDevFeaturesVisibility(bool bVisible);
	/** DEV GrabContrast: darken TOPO tier MIDs for fidelity grabs (no regen). */
	void ApplyDevGrabContrastMaterials(bool bGrabContrast);
	/** 2026-09-18: DEV View BANDS/BIOME/PGC toggle - recolors IslandMesh's already-built biome
	 * sections in place (cheap material swap via BiomeSectionRowIndices, no mesh rebuild). */
	void ApplyDevColorMode(IHDevViewRuntime::EIHDevColorMode Mode);
	void RebuildCoastFromCachedHeightfield() {}
	/** 2026-09-18: PGC round 1 - groundcover-only scatter, built lazily the first time PGC mode is
	 * activated on this island (not eagerly on every generation) since it's an opt-in DEV
	 * visualization, not mainline content. Reuses IslandMesh's already-built biome sections via
	 * BiomeSectionRowIndices - no re-classification. Called from ApplyDevColorMode. */
	void ApplyPGCScatterVisibility(bool bVisible);

	/** First Bake (World Builder Phase Order Canon): welds IslandMesh's per-biome-row
	 * UProceduralMeshComponent sections into one seamless UDynamicMeshComponent, smooths the
	 * result (fixes the low-poly triangular-tiling look), and swaps rendering/collision over to
	 * it. No Nanite, no native World Partition — see IH_WB_Phase_Order_Canon.md. Called
	 * automatically the moment the player commits an island's position/rotation
	 * (UIH_P1C08_CoastlineTuningSubsystem::ApplyActiveDraft). Safe to call again (e.g. after a
	 * tuning-only commit) — always rebuilds BakedIslandMesh fresh from IslandMesh's current
	 * sections, a pure function of that state with no hidden per-call randomness (Host-Authoritative
	 * Game Map forward-compatibility, per canon doc). */
	void RunFirstBake();
	bool IsFirstBaked() const { return bFirstBaked; }
	/** Reverts to IslandMesh's own rendering/collision, discarding BakedIslandMesh's stale content
	 * (still resident, just hidden — RunFirstBake will overwrite it via SetMesh on the next bake).
	 * Call before any in-place terrain regeneration (e.g. RegenerateSingleIsland) that rebuilds
	 * IslandMesh's sections out from under an already-baked island — otherwise the old baked mesh
	 * would keep rendering/colliding as the new terrain silently regenerates hidden underneath it. */
	void ResetFirstBake();

	// 2026-09-09: real bookkeeping now (was a no-op stub alongside the retired procedural
	// height-grid path) - static-mesh stamps use this array for the concurrent-placed-stamp soft
	// limit (IHInvisibleHandSpec::TerrainStampMeshWarnCountPerIsland/HardStopCountPerIsland).
	void RegisterTerrainStamp(AIH_TerrainStampActor* Stamp) { if (Stamp) { PlacedTerrainStamps.AddUnique(Stamp); } }
	void UnregisterTerrainStamp(AIH_TerrainStampActor* Stamp) { if (Stamp) { PlacedTerrainStamps.Remove(Stamp); } }
	void ClearPlacedTerrainStamps() { PlacedTerrainStamps.Reset(); }
	void ReapplyAllTerrainStampsToHeightGrid() {}
	void SyncPlacedTerrainStampSurfaceAnchors() {}
	void CollectTerrainStampReplayEntries(TArray<FIHPlacedTerrainStampReplayEntry>& OutEntries) const { OutEntries.Reset(); }
	const TArray<TObjectPtr<AIH_TerrainStampActor>>& GetPlacedTerrainStamps() const { return PlacedTerrainStamps; }

	void ApplyTankLayout(int32 InTankIslandIndex, float InSemiMajorAxisCm, float InSummitTopZCm, float InAreaKm2, int32 MasterSeed);
	/** Optional explicit profile override (from MapSeed Phase1 3:2:1 assignment). */
	void ApplyTankLayout(int32 InTankIslandIndex, float InSemiMajorAxisCm, float InSummitTopZCm, float InAreaKm2, int32 MasterSeed, EIHIslandProfile Profile);
	static void SetAslContourRibbonBakeDeferred(bool bDeferred);
	static bool IsAslContourRibbonBakeDeferred();
	void FlushDeferredAslContourRibbonBake();
	void RefreshIslandActorTickEnabled();
	void RefreshMinimapCoastline();
	void UpdateMinimapCoastlineTransformOnly();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BuildMeshesFromCellGraph(int32 MasterSeed);
	void BuildSeaShelfExtentFromShelfSegments();
	void RebuildRiverTerminusSocketMarkers();
	void BakeAslContourRibbons();
	void EnsureAslContourRibbonsBaked();
	void BakeFeatureRibbons();
	void EnsureFeatureRibbonsBaked();
	void RegisterCollision();
	void UnregisterCollision();
	void BuildPGCEligibilityCache();
	void RefreshPGCGroundcoverProximity();
	UHierarchicalInstancedStaticMeshComponent* GetOrCreatePGCGroundcoverHISM(UStaticMesh* Mesh);
	FVector2D LocalCmToWorldCm(const FVector2D& LocalCm) const;
	/**
	 * Walk all closed contour components. OutLargestLocalCm = longest perimeter (MainCoast authority).
	 * OutAllRingsLocalCm (optional) = all rings above min length, largest first (Contours bake).
	 */
	void SegmentsToClosedPolyline(
		const TArray<TPair<FVector2D, FVector2D>>& SegmentsMeters,
		TArray<FVector2D>& OutLargestLocalCm,
		TArray<TArray<FVector2D>>* OutAllRingsLocalCm = nullptr) const;

	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<UProceduralMeshComponent> IslandMesh;

	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<UProceduralMeshComponent> ShelfMesh;

	/** Optional sand polish (default OFF — IslandMesh waterline clamp owns rim). */
	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<UProceduralMeshComponent> SandApronMesh;

	/** DEV Contours: gold ASL 0 + magenta gold-governed WWF rim + white +25 m ribbons. */
	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<UProceduralMeshComponent> ContourRibbonMesh;

	/** DEV Features: Beach / Gentle / Bluff coast-character strokes. */
	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<UProceduralMeshComponent> FeatureRibbonMesh;

	UPROPERTY(VisibleAnywhere, Category = "IH|Island")
	TObjectPtr<UArrowComponent> SelectionReticle;

	UPROPERTY(Transient)
	int32 TankIslandIndex = INDEX_NONE;

	UPROPERTY(Transient)
	float SemiMajorAxisCm = 0.f;

	UPROPERTY(Transient)
	float SummitTopZCm = 0.f;

	UPROPERTY(Transient)
	float AreaKm2 = 0.f;

	UPROPERTY(Transient)
	float CachedCoastEnvelopeWorldCm = 0.f;

	UPROPERTY(Transient)
	bool bSelectionHighlighted = false;

	TArray<FVector2D> MainCoastPolylineLocalCm;
	/** Plan Addendum 10: cell-averaged center of the main landmass, set in BuildMeshesFromCellGraph.
	 * Plan Addendum 11: always (0,0) post-recenter - the actor's own origin IS this point now. */
	FVector2D MainLandCentroidLocalCm = FVector2D::ZeroVector;
	/** Plan Addendum 11: realized footprint radius, set in BuildMeshesFromCellGraph. */
	float MainLandFootprintRadiusCm = 0.f;
	TArray<FVector2D> ShelfPolylineLocalCm;
	TArray<FVector2D> Plus25PolylineLocalCm;
	/** Contours: all significant ASL0 / −25 / +25 rings (largest first). MainCoast = gold[0]. */
	TArray<TArray<FVector2D>> ContourGoldRingsLocalCm;
	/** Parallel to ContourGoldRingsLocalCm - true if that ring's interior is water (an enclosed
	 * inland sea/lake hole), false for the main coastline or a land islet. */
	TArray<bool> ContourGoldRingsIsInlandSea;
	TArray<TArray<FVector2D>> ContourShelfRingsLocalCm;
	TArray<TArray<FVector2D>> ContourPlus25RingsLocalCm;
	/**
	 * Gold-governed WWF outer (presentation): MainCoast + 40 m inland slope LUT DispM.
	 * Magenta Contours + cyan ShelfMesh loft use these — not HF −25 isoline XY.
	 */
	TArray<FVector2D> GovernedWwfOuterLocalCm;
	TArray<TArray<FVector2D>> ContourGovernedWwfRingsLocalCm;
	TArray<uint8> CoastCharacterRing;
	FIHSeaRootsExtent SeaRootsExtent;
	bool bHasSeaRootsExtent = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AIH_TerrainStampActor>> PlacedTerrainStamps;

	// 2026-09-18: parallel to IslandMesh's biome-classified mesh sections (built by
	// ApplyDtBiomeColorBands) - BiomeSectionRowIndices[SectionIdx] is that section's index into a
	// freshly-sorted GetBiomeRowsSortedForClassification() array, letting ApplyDevColorMode recolor
	// every section in place via SetMaterial without re-classifying triangles or rebuilding geometry.
	TArray<int32> BiomeSectionRowIndices;

	/** PGC round 1: one HISM per distinct groundcover mesh actually scattered on this island, keyed
	 * by the mesh's own path. Reused across proximity refreshes (cleared + repopulated, never
	 * destroyed/recreated) so toggling PGC or wandering around doesn't churn components. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PGCGroundcoverHISMs;

	// 2026-09-19: PGC round 1 first shipped as a one-shot whole-island scatter; PIE testing showed
	// that's the wrong architecture at this project's real island scale (hundreds of millions of
	// m² eligible per island) - any instance budget safe enough to bound cost renders as sparse to
	// invisible from ground level, since it has to spread across the WHOLE island at once. Replaced
	// with camera-proximity streaming (same "cheap periodic timer + game-thread distance math, not
	// per-Tick" pattern as AIH_WaterlineOceanAdapter::UpdateShoreManagerVisibilityGating): eligible
	// triangles are cached flat once (lazy, first PGC activation); each refresh does a linear
	// distance scan (cheap - tens to low hundreds of thousands of plain FVector::Dist calls, no
	// rendering/GPU work) and only the handful actually within PGCStreamRadiusCm of the camera get
	// scattered, so local density can be visually real regardless of island size. See
	// FIHPGCEligibleTri's own comment for why an earlier grid-bucketed version of this was wrong.
	TArray<FIHPGCEligibleTri> PGCEligibleTris;
	bool bPGCEligibilityCacheBuilt = false;
	/** World-irrelevant sentinel far outside any real island so the very first refresh after PGC
	 * activation always populates regardless of where the camera actually is. */
	FVector LastPGCRefreshLocalPos = FVector(TNumericLimits<float>::Max());
	/** Camera position at the PREVIOUS timer tick (not the previous actual rebuild) - lets
	 * RefreshPGCGroundcoverProximity measure instantaneous camera speed and skip rebuilding while
	 * the camera is flying fast (a deliberate zoom/relocate), leaving current instances frozen
	 * as-is until it settles back to a normal exploring pace. */
	FVector LastPGCTickLocalPos = FVector(TNumericLimits<float>::Max());
	/** Accumulated time the camera has held a speed below PGCSettleSpeedCmPerSec, reset to 0 the
	 * instant it exceeds that speed - a refresh is only allowed once this reaches
	 * PGCSettleDurationSec ("redraw only when the viewport is steady"), not merely "not sprinting". */
	float PGCTimeBelowSettleSpeedSec = 0.f;
	FTimerHandle PGCProximityRefreshTimerHandle;

	/** 2026-09-20: full-suspend layer on top of the settle gate above. The settle gate only ever
	 * skips the EXPENSIVE rescatter branch — RefreshPGCGroundcoverProximity itself (one FApp::
	 * HasFocus() call + one FVector::Dist) still fires every PGCProximityRefreshIntervalSec forever,
	 * camera moving or not, window focused or not. This accumulates time the camera has been
	 * stationary OR the window unfocused; once it crosses PGCIdleSuspendThresholdSec, the real
	 * 0.5s timer is cleared outright (see SuspendPGCProximityTimer) and a much cheaper 1Hz watchdog
	 * (CheckPGCIdleWatchdog) takes over just to detect when to resume. */
	float PGCTimeIdleForSuspendSec = 0.f;
	bool bPGCProximityTimerSuspended = false;
	/** Focus state captured at the moment of suspension, so the watchdog only resumes on a real
	 * focus-REGAINED transition rather than re-triggering every tick because focus was never lost
	 * (the idle-while-still-focused case). */
	bool bPGCWasFocusedAtSuspend = true;
	FTimerHandle PGCIdleWatchdogTimerHandle;
	void SuspendPGCProximityTimer();
	void ResumePGCProximityTimer();
	void CheckPGCIdleWatchdog();

	TArray<float> HeightsMeters;
	int32 SamplesPerSide = 0;
	double HalfExtentMeters = 0.0;
	double SampleSpacingMeters = 0.0;
	EIHIslandProfile CachedProfile = EIHIslandProfile::Low;
	TArray<FIHRiverTerminusSocket> RiverTerminusSockets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArrowComponent>> RiverTerminusSocketMarkers;

	bool bAslContourRibbonsBaked = false;
	bool bFeatureRibbonsBaked = false;

	/** First Bake output — one welded+smoothed UDynamicMeshComponent replacing IslandMesh's
	 * per-section rendering/collision once RunFirstBake() has run. Created lazily (NewObject, not
	 * CreateDefaultSubobject, mirroring GetOrCreatePGCGroundcoverHISM's pattern) since most islands
	 * may sit uncommitted for a while before the player ever bakes them. IslandMesh itself is never
	 * destroyed — hidden and collision-disabled only — so BuildPGCEligibilityCache and other code
	 * reading its ProcMeshSections keeps working unchanged. */
	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshComponent> BakedIslandMesh;
	bool bFirstBaked = false;

	static bool bAslContourRibbonBakeDeferred;
};
