// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IHP1C10_AzgaarTypes.h"
#include "FIHTerrainStampTypes.h"
#include "IHCoastGenerationTypes.h"
#include "IH_WB_IslandActor.generated.h"

class UProceduralMeshComponent;
class USceneComponent;
class UArrowComponent;
class UIH_P1C08_MinimapSubsystem;
class AIH_TerrainStampActor;

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
	void RebuildCoastFromCachedHeightfield() {}

	void RegisterTerrainStamp(AIH_TerrainStampActor* /*Stamp*/) {}
	void UnregisterTerrainStamp(AIH_TerrainStampActor* /*Stamp*/) {}
	void ClearPlacedTerrainStamps() {}
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

	static bool bAslContourRibbonBakeDeferred;
};
