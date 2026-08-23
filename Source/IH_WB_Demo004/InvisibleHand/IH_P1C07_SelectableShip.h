// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "UObject/Interface.h"
#include "IH_P1C07_SelectableShip.generated.h"

class UPrimitiveComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UIH_P1C07_SelectableShip : public UInterface
{
	GENERATED_BODY()
};

class IIH_P1C07_SelectableShip
{
	GENERATED_BODY()

public:
	virtual UPrimitiveComponent* GetShipSelectionPrimitive() const = 0;
	virtual void SetShipSelected(bool bSelected) = 0;
	virtual void CommandSailTo(const FVector& WorldDestination, const FRotator& FinalApproachHeading) = 0;
	virtual bool HasArrivedAtDestination() const = 0;
	/** Active move order: sail until true destination regardless of contingency waypoints. */
	virtual bool HasActiveTransitOrder() const = 0;
	virtual FVector GetTrueDestinationWorld() const = 0;
	virtual FVector GetShipFeetLocation() const = 0;
};
