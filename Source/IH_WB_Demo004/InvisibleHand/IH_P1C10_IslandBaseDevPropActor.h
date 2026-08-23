// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C10_IslandBaseDevPropActor.generated.h"

class AIH_WB_IslandActor;

/** Stub — Island Base Dev Prop excluded from heightmap rebuild; no-op for harness compile. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C10_IslandBaseDevPropActor : public AActor
{
	GENERATED_BODY()

public:
	AIH_P1C10_IslandBaseDevPropActor();
	void InitializeForIsland(const AIH_WB_IslandActor* Island);
	void SyncTransformFromIsland(const AIH_WB_IslandActor* Island);
	int32 GetTankIslandIndex() const { return TankIslandIndex; }

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;
	int32 TankIslandIndex = INDEX_NONE;
};
