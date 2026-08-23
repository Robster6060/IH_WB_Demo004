// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "FIHTerrainStampTypes.h"
#include "GameFramework/Actor.h"
#include "IH_TerrainStampActor.generated.h"

class AIH_WB_IslandActor;
class UProceduralMeshComponent;

/** Stub — terrain stamp mesh path deferred; keeps G/W Build Palette compile. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_TerrainStampActor : public AActor
{
	GENERATED_BODY()

public:
	AIH_TerrainStampActor();
	void InitializeStamp(EIHTerrainStampId InStampId, bool bInInvertHeight = false, bool bInGalleryPreviewOnly = false);
	void SetTargetIsland(AIH_WB_IslandActor* InIsland);
	void SetDragPreviewMode(bool bInDragPreview);
	bool IsDragPreview() const { return bIsDragPreview; }
	void ApplyWorldSurfacePlacement(AIH_WB_IslandActor* InIsland, const FVector& SurfaceWorld);
	int32 ApplyToHeightGrid() { return 0; }
	void CommitStampToTargetIsland() {}
	void RefreshPreviewMesh() {}
	void SyncStampActorYaw() {}
	void SetStampSelected(bool bInSelected) { bStampSelected = bInSelected; }
	bool IsStampSelected() const { return bStampSelected; }
	bool IsGalleryPreviewOnly() const { return bGalleryPreviewOnly; }
	int32 GetPreviewMeshVertexCount() const { return 0; }
	float GetPreviewFootprintRadiusCm() const { return FMath::Max(100.f, RadiusKm * 100000.f); }
	EIHTerrainStampId GetStampId() const { return StampId; }
	bool IsInvertHeight() const { return bInvertHeight; }
	AIH_WB_IslandActor* GetTargetIsland() const { return TargetIsland.Get(); }
	FVector2D GetCenterLocalCmOnIsland() const { return FVector2D::ZeroVector; }

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

	UPROPERTY(Transient)
	TObjectPtr<AIH_WB_IslandActor> TargetIsland;
};
