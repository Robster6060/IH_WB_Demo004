// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C07_NavAvoidanceTypes.h"
#include "UObject/Interface.h"
#include "IH_P1C07_NavAvoidanceParticipant.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UIH_P1C07_NavAvoidanceParticipant : public UInterface
{
	GENERATED_BODY()
};

/** Shared participant surface for ships, vehicles, and airships. */
class IH_WB_DEMO004_API IIH_P1C07_NavAvoidanceParticipant
{
	GENERATED_BODY()

public:
	virtual AActor* GetNavActor() const = 0;
	virtual EIH_P1C07_NavDomainMode GetNavDomainMode() const = 0;
	virtual bool IsNavAvoidanceActive() const = 0;
	virtual float GetNavAvoidanceRadiusCm() const = 0;
	virtual FVector GetNavVelocity2D() const = 0;
	virtual uint32 GetNavMoveOrderGroupId() const = 0;
	virtual int32 GetNavStandOnPriority() const = 0;
};
