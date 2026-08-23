// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Components/ActorComponent.h"
#include "IH_OceanFromZoneComponent.generated.h"

/** Stub — Arbor WaterZone ocean path not used (custom ocean plane). */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class IH_WB_DEMO004_API UIH_OceanFromZoneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void RequestGPUWaveUpdateAfterWavesChanged() {}
};
