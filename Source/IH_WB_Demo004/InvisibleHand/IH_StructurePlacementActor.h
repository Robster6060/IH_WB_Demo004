// Copyright Epic Games, Inc. All Rights Reserved.
// Invisible Hand — dev structure placement actor (SketchUp-origin door threshold).

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_StructurePlacementActor.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Minimal structure placeholder actor.
 * SketchUp origin sits at door threshold, 10 cm below door sill.
 * AlignToTerrainCenter() places origin 10 cm above terrain impact so the door meets the surface.
 */
UCLASS(Blueprintable)
class IH_WB_DEMO004_API AIH_StructurePlacementActor : public AActor
{
	GENERATED_BODY()

public:
	static constexpr float DefaultDoorOriginTerrainOffsetCm = 10.f;

	AIH_StructurePlacementActor();

	/** Vertical trace from actor XY; sets Z to impact + DoorOriginTerrainOffsetCm. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void AlignToTerrainCenter();

	/** Optional mesh override when not set on a Blueprint subclass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Structure")
	TSoftObjectPtr<UStaticMesh> StructureMesh;

	/** Dev mesh if imported, otherwise scaled engine cube. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	bool EnsureVisiblePlacementMesh(FName PaletteItemID);

	/** Scaled engine cube — always visible for B Build dev drag/place preview. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	bool EnsureDevFootprintCubeMesh(FName PaletteItemID);

	/** Load dev placeholder mesh from Build_DEV_* itemID when StructureMesh / BP CDO is unset. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	bool TryApplyDevPlaceholderMesh(FName PaletteItemID);

	/** Engine cube scaled to palette footprint when placeholder mesh is missing. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	bool TryApplyFallbackFootprintMesh(FName PaletteItemID);

	static bool GetFootprintExtentCm(FName PaletteItemID, FVector& OutExtentCm);

	/** World origin for placement actor root from a terrain/island surface hit. */
	static bool ComputePlacementOriginFromSurface(FName PaletteItemID, const FVector& SurfacePoint, FVector& OutActorOrigin);

	/** Drag ghost: visible mesh, no collision; call again with false on drop. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void SetBuildDragPreviewMode(bool bPreview);

	/** Translucent blue ghost materials on all mesh slots during drag preview. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void ApplyPlacementVisualStyle(bool bDragGhost);

	/** Restore materials from the assigned static mesh asset. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void RestoreDefaultPlacementMaterials();

	/** Solid dev tint after drop so structures stay visible on bright sand / shore shelf. */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void ApplyPlacedDevVisualStyle();

	/**
	 * 2026-09-13: selectable-actor-hierarchy - amber tint on select (mirrors Ship/Mannequin's own
	 * select-tint pattern), caching whatever material is CURRENTLY active (the permanent dev-blue
	 * from ApplyPlacedDevVisualStyle, not the original asset materials) and restoring exactly that
	 * on deselect - same cache-current/restore-cached approach as the other selectable types.
	 */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void SetStructureSelected(bool bSelected);

	bool IsStructureSelected() const { return bStructureSelected; }

	/**
	 * 2026-09-14: Shift+Drag relocate / Shift+Wheel rotate, per canon parity with Town Grid/Terrain
	 * Stamp - X/Y delta-move like Town Grid's own BeginMoveDrag/UpdateMoveDrag (simpler than Terrain
	 * Stamp's depth-constrained version; Structures don't have a "sink into terrain" concept), then
	 * re-align to the terrain surface on release so the door threshold still sits flush.
	 */
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void BeginMoveDrag(const FVector& WorldPoint);
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void UpdateMoveDrag(const FVector& WorldPoint);
	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void EndMoveDrag();
	bool IsMoveDragActive() const { return bMoveDragActive; }

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Structure")
	void ApplyYawStep(float DeltaDeg);

	/** Remember palette item so mesh + placement math stay in sync during drag/drop. */
	void SetPlacementPaletteItem(FName PaletteItemID);

	FName GetPlacementPaletteItem() const { return ActivePaletteItemID; }

	bool HasPlacementMesh() const;

	bool UsesFallbackFootprintMesh() const { return bUsesFallbackFootprintMesh; }

	/** Synchronous load of dev SM_Structure_* for palette item (may be null). */
	static UStaticMesh* LoadDevPlaceholderMesh(FName PaletteItemID);

	/** Z lift applied after terrain trace (cm). Matches SU origin -10 cm under door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Structure")
	float DoorOriginTerrainOffsetCm = DefaultDoorOriginTerrainOffsetCm;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Structure")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	bool bUsesFallbackFootprintMesh = false;
	float FootprintHalfHeightCm = 200.f;

	FName ActivePaletteItemID;

	// 2026-09-14 crash fix: this array previously had no UPROPERTY, so any UMaterialInstanceDynamic
	// referenced ONLY through it (e.g. the dev-blue MID from ApplyPlacedDevVisualStyle, cached here
	// the moment SetStructureSelected(true) first tints a structure) was invisible to GC and could
	// be reclaimed while a structure sat selected for more than a few seconds - the very next
	// deselect (SetStructureSelected(false)) then dereferenced a dangling/null pointer restoring it,
	// crashing with EXCEPTION_ACCESS_VIOLATION (confirmed via Saved/Crashes minidump callstack).
	// Mirrors AIH_P1C08_MannequinActor's own CachedSourceMaterials, which already had this UPROPERTY.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> CachedSourceMaterials;

	bool bDragGhostVisualStyleActive = false;
	bool bStructureSelected = false;

	bool bMoveDragActive = false;
	FVector MoveDragStartWorld = FVector::ZeroVector;
	FVector MoveDragStartActorLoc = FVector::ZeroVector;

	void ApplyStructureMesh();

	static bool ComputeActorOriginFromMeshBottom(
		const UStaticMesh* Mesh,
		const FVector& SurfacePoint,
		float DoorSillOffsetCm,
		FVector& OutActorOrigin);
};
