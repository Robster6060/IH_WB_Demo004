// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_TownGridPlacementStub.h"

#include "IHInvisibleHandDesignSpec.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"

FVector AIH_TownGridPlacementStub::GetDefaultFlatHalfExtentCm()
{
	const float HalfFootprint = ModuleSizeCm * static_cast<float>(DefaultModuleCount) * 0.5f;
	return FVector(HalfFootprint, HalfFootprint, 50.f);
}

AIH_TownGridPlacementStub::AIH_TownGridPlacementStub()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	DebugFlatBBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DebugFlatBBox"));
	DebugFlatBBox->SetupAttachment(RootScene);
	DebugFlatBBox->SetBoxExtent(GetDefaultFlatHalfExtentCm());
	DebugFlatBBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugFlatBBox->SetHiddenInGame(false);
	DebugFlatBBox->ShapeColor = IHInvisibleHandSpec::TownGridFocusOutlineBlue.ToFColor(true);
	DebugFlatBBox->SetLineThickness(2.f);
}

void AIH_TownGridPlacementStub::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("TownGridPlacementStub spawned — template=%d at %s"),
		static_cast<int32>(TownGridTemplate), *GetActorLocation().ToString());
}
