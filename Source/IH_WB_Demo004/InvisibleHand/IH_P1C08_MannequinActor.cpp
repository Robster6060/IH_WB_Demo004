// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_MannequinActor.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AIH_P1C08_MannequinActor::AIH_P1C08_MannequinActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	SetActorEnableCollision(true);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(TEXT(
		"/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPFinder(TEXT(
		"/Game/Waterline/2_WaterSim_Content/3_Dev/Blueprints/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP_C"));
	if (AnimBPFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPFinder.Class);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
	}
}
