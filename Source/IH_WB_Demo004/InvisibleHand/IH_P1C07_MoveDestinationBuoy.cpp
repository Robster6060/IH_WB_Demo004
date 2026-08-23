// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_MoveDestinationBuoy.h"
#include "IH_P1C07_SelectableShip.h"
#include "IH_P1C07_WaterQueryHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	static UMaterialInstanceDynamic* CreateBrightOrangeBasicShapeMaterial(UObject* Outer)
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

		// Vivid safety-orange marker (BasicShapeMaterial uses "Color", not "BaseColor"/"EmissiveColor").
		static const FLinearColor BrightOrange(1.f, 0.42f, 0.f, 1.f);
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& N : ColorNames)
		{
			Mid->SetVectorParameterValue(N, BrightOrange);
		}
		Mid->SetScalarParameterValue(FName(TEXT("Roughness")), 0.25f);
		Mid->SetScalarParameterValue(FName(TEXT("Specular")), 0.45f);
		return Mid;
	}
}

AIH_P1C07_MoveDestinationBuoy::AIH_P1C07_MoveDestinationBuoy()
{
	PrimaryActorTick.bCanEverTick = true;

	BuoyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuoyMesh"));
	SetRootComponent(BuoyMesh);
	BuoyMesh->SetMobility(EComponentMobility::Movable);
	BuoyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BuoyMesh->SetCastShadow(false);

	if (UStaticMesh* Capsule = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		BuoyMesh->SetStaticMesh(Capsule);
	}
	BuoyMesh->SetRelativeScale3D(FVector(3.5f, 3.5f, 12.f));

	if (UMaterialInstanceDynamic* Mid = CreateBrightOrangeBasicShapeMaterial(this))
	{
		BuoyMesh->SetMaterial(0, Mid);
	}
}

void AIH_P1C07_MoveDestinationBuoy::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* Mid = CreateBrightOrangeBasicShapeMaterial(this))
	{
		BuoyMesh->SetMaterial(0, Mid);
	}
}

void AIH_P1C07_MoveDestinationBuoy::InitOrder(
	const FVector& AnchorWorld,
	const TArray<TScriptInterface<IIH_P1C07_SelectableShip>>& AssignedShips)
{
	AnchorWorldCm = AnchorWorld;

	FVector SurfaceLoc = AnchorWorld;
	FVector Normal = FVector::UpVector;
	FVector Velocity = FVector::ZeroVector;
	if (IH_P1C07WaterQuery::QueryBestSurfaceInWorld(GetWorld(), AnchorWorld, SurfaceLoc, Normal, Velocity))
	{
		SetActorLocation(SurfaceLoc + FVector(0.f, 0.f, 1200.f));
		AnchorWorldCm = SurfaceLoc;
	}
	else
	{
		SetActorLocation(AnchorWorld + FVector(0.f, 0.f, 1200.f));
	}

	TrackedShips.Reset();
	for (const TScriptInterface<IIH_P1C07_SelectableShip>& ShipIface : AssignedShips)
	{
		if (AActor* ShipActor = Cast<AActor>(ShipIface.GetObject()))
		{
			TrackedShips.Add(ShipActor);
		}
	}
}

void AIH_P1C07_MoveDestinationBuoy::RemoveTrackedShips(const TArray<AActor*>& Ships)
{
	for (AActor* Ship : Ships)
	{
		if (!Ship)
		{
			continue;
		}

		TrackedShips.RemoveAll([Ship](const TWeakObjectPtr<AActor>& Ptr) {
			return Ptr.Get() == Ship;
		});
	}

	if (TrackedShips.Num() == 0 && AliveTimeSec >= MinAliveSeconds)
	{
		Destroy();
	}
}

void AIH_P1C07_MoveDestinationBuoy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AliveTimeSec += DeltaTime;

	for (int32 Idx = TrackedShips.Num() - 1; Idx >= 0; --Idx)
	{
		AActor* Ship = TrackedShips[Idx].Get();
		if (!Ship)
		{
			TrackedShips.RemoveAt(Idx);
			continue;
		}

		IIH_P1C07_SelectableShip* Selectable = Cast<IIH_P1C07_SelectableShip>(Ship);
		if (!Selectable)
		{
			TrackedShips.RemoveAt(Idx);
			continue;
		}

		// Persist until this ship reaches THIS buoy (multi-waypoint: not HasActiveTransitOrder).
		const float Dist2D = FVector::Dist2D(Selectable->GetShipFeetLocation(), AnchorWorldCm);
		if (Dist2D <= ArrivalRadiusCm)
		{
			TrackedShips.RemoveAt(Idx);
		}
	}

	if (TrackedShips.Num() == 0 && AliveTimeSec >= MinAliveSeconds)
	{
		Destroy();
	}
}
