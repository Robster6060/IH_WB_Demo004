// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Pawn.h"
#include "IH_Cube2FlyPawn.generated.h"

class USphereComponent;
class UFloatingPawnMovement;

/** Fly viewport pawn with no visible mesh (avoids DefaultPawn's EngineMeshes/Sphere showing as a white blob underwater). */
UCLASS(NotPlaceable)
class IH_WB_DEMO004_API AIH_Cube2FlyPawn : public APawn
{
	GENERATED_BODY()

public:
	AIH_Cube2FlyPawn();

protected:
	UPROPERTY(VisibleAnywhere, Category = "P1C07")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "P1C07")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;
};
