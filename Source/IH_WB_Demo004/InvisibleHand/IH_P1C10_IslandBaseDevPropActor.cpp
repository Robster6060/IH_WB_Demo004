// Copyright Invisible Hand. All Rights Reserved.

#include "IH_P1C10_IslandBaseDevPropActor.h"
#include "IH_WB_IslandActor.h"
#include "Components/SceneComponent.h"

AIH_P1C10_IslandBaseDevPropActor::AIH_P1C10_IslandBaseDevPropActor()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AIH_P1C10_IslandBaseDevPropActor::InitializeForIsland(const AIH_WB_IslandActor* Island)
{
	if (Island)
	{
		TankIslandIndex = Island->GetTankIslandIndex();
		SyncTransformFromIsland(Island);
	}
}

void AIH_P1C10_IslandBaseDevPropActor::SyncTransformFromIsland(const AIH_WB_IslandActor* Island)
{
	if (Island)
	{
		SetActorTransform(Island->GetActorTransform());
	}
}
