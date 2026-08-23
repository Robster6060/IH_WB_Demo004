// Copyright Invisible Hand. All Rights Reserved.

#include "IH_TerrainStampActor.h"
#include "IH_WB_IslandActor.h"
#include "Components/SceneComponent.h"

AIH_TerrainStampActor::AIH_TerrainStampActor()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AIH_TerrainStampActor::InitializeStamp(EIHTerrainStampId InStampId, bool bInInvertHeight, bool bInGalleryPreviewOnly)
{
	StampId = InStampId;
	bInvertHeight = bInInvertHeight;
	bGalleryPreviewOnly = bInGalleryPreviewOnly;
}

void AIH_TerrainStampActor::SetTargetIsland(AIH_WB_IslandActor* InIsland)
{
	TargetIsland = InIsland;
}

void AIH_TerrainStampActor::SetDragPreviewMode(bool bInDragPreview)
{
	bIsDragPreview = bInDragPreview;
}

void AIH_TerrainStampActor::ApplyWorldSurfacePlacement(AIH_WB_IslandActor* InIsland, const FVector& SurfaceWorld)
{
	TargetIsland = InIsland;
	SetActorLocation(SurfaceWorld);
}
