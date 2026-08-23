// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C07_WaterTankRig.generated.h"

/** Stub — Gate 0 prefers custom ocean plane; WaterTankRig no-op for heightmap project. */
UCLASS()
class IH_WB_DEMO004_API AIH_P1C07_WaterTankRig : public AActor
{
	GENERATED_BODY()

public:
	AIH_P1C07_WaterTankRig();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P1C07|Water")
	FVector2D RealmHalfExtentKm = FVector2D(21.034f, 13.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P1C07|Water")
	float WaterSurfaceZCm = 0.f;

	void ApplyRealmHalfExtents(float RealmHalfExtentNSKm, float RealmHalfExtentEWKm = 0.f);
	void ApplyDevOceanVisibility(bool bOceanVisible);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;
};
