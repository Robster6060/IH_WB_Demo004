// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C08_AltitudeStoryStickActor.generated.h"

class UStaticMeshComponent;

/** Stacked 50 m × 50 m elevation bands from abysmal floor (−250 m ASL) to summit (+2,400 m ASL). */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C08_AltitudeStoryStickActor : public AActor
{
	GENERATED_BODY()

public:
	AIH_P1C08_AltitudeStoryStickActor();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	void BuildElevationBands();

	UPROPERTY(VisibleAnywhere, Category = "P1C07")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "P1C07")
	TArray<TObjectPtr<UStaticMeshComponent>> BandMeshes;
};
