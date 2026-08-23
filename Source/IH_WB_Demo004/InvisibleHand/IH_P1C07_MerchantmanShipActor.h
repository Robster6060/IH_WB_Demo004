// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C07_CommandableShipActor.h"
#include "IH_P1C07_MerchantmanShipActor.generated.h"

/** Merchantman hull — commandable ship base with default mesh from blend1 export. */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_P1C07_MerchantmanShipActor : public AIH_P1C07_CommandableShipActor
{
	GENERATED_BODY()

public:
	AIH_P1C07_MerchantmanShipActor();

protected:
	virtual void ResolveHullMesh() override;
};
