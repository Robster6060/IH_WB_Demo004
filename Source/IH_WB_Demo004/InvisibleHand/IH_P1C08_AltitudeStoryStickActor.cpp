// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_AltitudeStoryStickActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	static constexpr int32 AltitudeBandCount = 8;

	struct FAltitudeBandSpec
	{
		float MinAslM;
		float MaxAslM;
		FColor Color;
	};

	static constexpr float StickFootprintHalfCm = 2500.f;

	// IH-DEC-056 (48-row landform grid) tier boundaries, matching the current diagnostic rainbow
	// pass on DT_ASLSlopeBiome (biomeColor temporarily set per-tier to these exact hex values) -
	// bands 1-7 are the real WWF..Alpine tiers; band 0 (-250 to -25m) is below the DT's own floor
	// (ShelfFloorMeters=-25m, nothing classifies that deep today) and keeps its prior color as an
	// unclassified "abyss" marker, not part of the 7-tier match.
	static const FAltitudeBandSpec AltitudeBands[AltitudeBandCount] = {
		{-250.f, -25.f, FColor(0x3F, 0x00, 0xFF)},   // Abyss (unclassified, below WWF floor)
		{-25.f, 0.f, FColor(0xFF, 0x00, 0x00)},      // WWF
		{0.f, 200.f, FColor(0xFF, 0x88, 0x00)},      // Shorelands
		{200.f, 600.f, FColor(0xFF, 0xFF, 0x00)},    // Lowlands
		{600.f, 1100.f, FColor(0x00, 0xFF, 0x00)},   // Midlands
		{1100.f, 1600.f, FColor(0x00, 0xFF, 0xFF)},  // Highlands
		{1600.f, 2000.f, FColor(0x00, 0x00, 0xFF)},  // Montane
		{2000.f, 2400.f, FColor(0x88, 0x00, 0xFF)},  // Alpine
	};

	static UMaterialInstanceDynamic* CreateBandMaterial(AActor* Outer, const FColor& Color)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Parent || !Outer)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, Outer);
		if (!Mid)
		{
			return nullptr;
		}
		const FLinearColor Tint(
			static_cast<float>(Color.R) / 255.f,
			static_cast<float>(Color.G) / 255.f,
			static_cast<float>(Color.B) / 255.f);
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& N : ColorNames)
		{
			Mid->SetVectorParameterValue(N, Tint);
		}
		return Mid;
	}

	static FName BandComponentName(int32 Index)
	{
		return *FString::Printf(TEXT("Band_%02d"), Index);
	}
}

AIH_P1C08_AltitudeStoryStickActor::AIH_P1C08_AltitudeStoryStickActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	BandMeshes.SetNum(AltitudeBandCount);
	for (int32 Index = 0; Index < AltitudeBandCount; ++Index)
	{
		UStaticMeshComponent* Band = CreateDefaultSubobject<UStaticMeshComponent>(BandComponentName(Index));
		Band->SetupAttachment(SceneRoot);
		Band->SetMobility(EComponentMobility::Movable);
		Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BandMeshes[Index] = Band;
	}
}

void AIH_P1C08_AltitudeStoryStickActor::BeginPlay()
{
	Super::BeginPlay();
	BuildElevationBands();
}

void AIH_P1C08_AltitudeStoryStickActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildElevationBands();
}

void AIH_P1C08_AltitudeStoryStickActor::BuildElevationBands()
{
	if (!GetWorld())
	{
		return;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh || BandMeshes.Num() != AltitudeBandCount)
	{
		return;
	}

	for (int32 Index = 0; Index < AltitudeBandCount; ++Index)
	{
		const FAltitudeBandSpec& Band = AltitudeBands[Index];
		const float HeightCm = (Band.MaxAslM - Band.MinAslM) * 100.f;
		const float CenterZCm = 0.5f * (Band.MinAslM + Band.MaxAslM) * 100.f;

		UStaticMeshComponent* MeshComp = BandMeshes[Index];
		if (!MeshComp)
		{
			continue;
		}

		MeshComp->SetStaticMesh(CubeMesh);
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, CenterZCm));
		MeshComp->SetRelativeScale3D(FVector(
			(2.f * StickFootprintHalfCm) * 0.01f,
			(2.f * StickFootprintHalfCm) * 0.01f,
			HeightCm * 0.01f));
		MeshComp->SetCastShadow(true);
		if (UMaterialInstanceDynamic* Mat = CreateBandMaterial(this, Band.Color))
		{
			MeshComp->SetMaterial(0, Mat);
		}
	}
}
