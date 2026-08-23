// Copyright Invisible Hand. All Rights Reserved.

#include "IH_TerrainStampGalleryActor.h"
#include "Components/SceneComponent.h"

AIH_TerrainStampGalleryActor::AIH_TerrainStampGalleryActor()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SetActorHiddenInGame(true);
}

void AIH_TerrainStampGalleryActor::BuildGallery(bool /*bIncludeInvertDoubleDutyRow*/)
{
}
