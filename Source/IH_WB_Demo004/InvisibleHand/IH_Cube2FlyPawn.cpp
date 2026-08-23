// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_Cube2FlyPawn.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IH_Cube2FlyPawn)

AIH_Cube2FlyPawn::AIH_Cube2FlyPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(8.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetHiddenInGame(true);
	CollisionComponent->SetCastShadow(false);
	CollisionComponent->bVisibleInReflectionCaptures = false;
	CollisionComponent->bVisibleInRealTimeSkyCaptures = false;
	RootComponent = CollisionComponent;

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	MovementComponent->UpdatedComponent = CollisionComponent;
}
