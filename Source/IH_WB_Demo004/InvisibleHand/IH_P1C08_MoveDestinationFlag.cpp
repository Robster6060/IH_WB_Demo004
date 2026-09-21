// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_MoveDestinationFlag.h"
#include "IH_P1C08_SelectableMannequin.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UMaterialInstanceDynamic* CreateFlagTintMaterial(UObject* Outer, const FLinearColor& Tint)
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
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& N : ColorNames)
		{
			Mid->SetVectorParameterValue(N, Tint);
		}
		return Mid;
	}
}

AIH_P1C08_MoveDestinationFlag::AIH_P1C08_MoveDestinationFlag()
{
	PrimaryActorTick.bCanEverTick = true;

	// PoleMesh and FlagMesh are both attached to this unscaled root as siblings, never to each
	// other - see the header comment on FlagRoot for why (a scaled parent silently distorts any
	// child's own scale/offset).
	FlagRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FlagRoot"));
	SetRootComponent(FlagRoot);

	PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoleMesh"));
	PoleMesh->SetupAttachment(FlagRoot);
	PoleMesh->SetMobility(EComponentMobility::Movable);
	PoleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PoleMesh->SetCanEverAffectNavigation(false);
	PoleMesh->SetCastShadow(false);

	FlagMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FlagMesh"));
	FlagMesh->SetupAttachment(FlagRoot);
	FlagMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlagMesh->SetCanEverAffectNavigation(false);
	FlagMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PoleMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// Engine Cylinder is 100x100x100 - scale relative to that.
	if (PoleMeshFinder.Succeeded())
	{
		PoleMesh->SetStaticMesh(PoleMeshFinder.Object);
	}
	PoleMesh->SetRelativeScale3D(FVector(PoleDiameterCm / 100.f, PoleDiameterCm / 100.f, PoleHeightCm / 100.f));
	PoleMesh->SetRelativeLocation(FVector(0.f, 0.f, PoleHeightCm * 0.5f));

	// 2026-09-12: 3x the original 100x60cm per the user's explicit request, now an isosceles
	// triangle pennant instead of a rectangle. Authored directly in FlagRoot's own (unrotated,
	// world-aligned) space rather than via a scaled-then-rotated primitive, so there's no ambiguity
	// about which axis ends up vertical after a rotation - the vertical base sits flush against the
	// pole (near its top edge, not dipping below the anchor), with the apex pointing horizontally
	// away from it, "rotated 90 degrees" from lying flat per the user's request.
	const float BaseX = PoleDiameterCm * 0.5f;
	const float BaseTopZ = PoleHeightCm;
	const float BaseBottomZ = PoleHeightCm - FlagHeightCm;
	const float ApexZ = PoleHeightCm - (FlagHeightCm * 0.5f);

	const FVector BaseTop(BaseX, 0.f, BaseTopZ);
	const FVector BaseBottom(BaseX, 0.f, BaseBottomZ);
	const FVector Apex(BaseX + FlagWidthCm, 0.f, ApexZ);

	// Two triangles (front + reversed-winding back) sharing the same three points, so the pennant
	// reads from either side rather than vanishing when viewed from behind - per this session's
	// own observation that a single-sided flat panel can disappear at some camera angles while the
	// fully-3D pole stays visible from any angle.
	TArray<FVector> Verts = { BaseTop, BaseBottom, Apex, BaseTop, BaseBottom, Apex };
	TArray<int32> Tris = { 0, 1, 2, 3, 5, 4 };
	TArray<FVector> Normals = {
		FVector(0.f, 1.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, 1.f, 0.f),
		FVector(0.f, -1.f, 0.f), FVector(0.f, -1.f, 0.f), FVector(0.f, -1.f, 0.f),
	};
	TArray<FVector2D> UVs = {
		FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 0.5f),
		FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 0.5f),
	};
	TArray<FColor> VertColors;
	VertColors.Init(FColor::White, Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	FlagMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, VertColors, Tangents, /*bCreateCollision=*/false);

	if (UMaterialInstanceDynamic* PoleMid = CreateFlagTintMaterial(this, FLinearColor(0.25f, 0.25f, 0.25f)))
	{
		PoleMesh->SetMaterial(0, PoleMid);
	}
	// Pure red (HEX FF0000, converted from sRGB) - the user's own explicit choice, standing in for
	// the ship system's orange buoy.
	if (UMaterialInstanceDynamic* FlagMid = CreateFlagTintMaterial(this, FLinearColor(FColor(0xFF, 0x00, 0x00))))
	{
		FlagMesh->SetMaterial(0, FlagMid);
	}

	FlutterPhaseOffset = FMath::FRandRange(0.f, 2.f * PI);
}

void AIH_P1C08_MoveDestinationFlag::InitOrder(
	const FVector& AnchorWorld,
	const TArray<TScriptInterface<IIH_P1C08_SelectableMannequin>>& AssignedMannequins)
{
	AnchorWorldCm = AnchorWorld;
	// Unlike the ship buoy (which re-queries a water surface since it can float above waves at any
	// height), the anchor here was already resolved against walkable IslandMesh by the caller - just
	// plant the flag's own root at that exact point; PoleMesh's own relative offset lifts the pole
	// and flag up from it.
	SetActorLocation(AnchorWorld);

	TrackedMannequins.Reset();
	for (const TScriptInterface<IIH_P1C08_SelectableMannequin>& Iface : AssignedMannequins)
	{
		if (AActor* MannequinActor = Cast<AActor>(Iface.GetObject()))
		{
			TrackedMannequins.Add(MannequinActor);
		}
	}
}

void AIH_P1C08_MoveDestinationFlag::RemoveTrackedMannequins(const TArray<AActor*>& Mannequins)
{
	for (AActor* Mannequin : Mannequins)
	{
		if (!Mannequin)
		{
			continue;
		}
		TrackedMannequins.RemoveAll([Mannequin](const TWeakObjectPtr<AActor>& Ptr) {
			return Ptr.Get() == Mannequin;
		});
	}

	if (TrackedMannequins.Num() == 0 && AliveTimeSec >= MinAliveSeconds)
	{
		Destroy();
	}
}

void AIH_P1C08_MoveDestinationFlag::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AliveTimeSec += DeltaTime;

	// Flutter: the base edge (against the pole) stays fixed - only the free tip sways side to side,
	// like a real pennant's loose corner. A single-triangle pennant only has one free vertex to
	// animate, so this is a simple sway rather than a rippling multi-point wave, but reads as
	// "flapping" rather than static.
	const float BaseX = PoleDiameterCm * 0.5f;
	const float ApexZ = PoleHeightCm - (FlagHeightCm * 0.5f);
	const FVector BaseTop(BaseX, 0.f, PoleHeightCm);
	const FVector BaseBottom(BaseX, 0.f, PoleHeightCm - FlagHeightCm);
	const float FlutterY = FMath::Sin(GetWorld()->GetTimeSeconds() * FlagFlutterFrequencyHz * 2.f * PI + FlutterPhaseOffset) * FlagFlutterAmplitudeCm;
	const FVector Apex(BaseX + FlagWidthCm, FlutterY, ApexZ);

	TArray<FVector> Verts = { BaseTop, BaseBottom, Apex, BaseTop, BaseBottom, Apex };
	TArray<FVector> Normals = {
		FVector(0.f, 1.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, 1.f, 0.f),
		FVector(0.f, -1.f, 0.f), FVector(0.f, -1.f, 0.f), FVector(0.f, -1.f, 0.f),
	};
	TArray<FVector2D> UVs = {
		FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 0.5f),
		FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 0.5f),
	};
	TArray<FColor> VertColors;
	VertColors.Init(FColor::White, Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	FlagMesh->UpdateMeshSection(0, Verts, Normals, UVs, VertColors, Tangents);

	for (int32 Idx = TrackedMannequins.Num() - 1; Idx >= 0; --Idx)
	{
		AActor* Mannequin = TrackedMannequins[Idx].Get();
		if (!Mannequin)
		{
			TrackedMannequins.RemoveAt(Idx);
			continue;
		}

		IIH_P1C08_SelectableMannequin* Selectable = Cast<IIH_P1C08_SelectableMannequin>(Mannequin);
		if (!Selectable)
		{
			TrackedMannequins.RemoveAt(Idx);
			continue;
		}

		const float Dist2D = FVector::Dist2D(Selectable->GetMannequinFeetLocation(), AnchorWorldCm);
		if (Dist2D <= ArrivalRadiusCm)
		{
			// 2026-09-13: breadcrumb waypoint chain - this Mannequin just reached the leg THIS flag
			// represents; hand it off to whatever it queued next (a no-op if nothing's queued, i.e.
			// this really was the final destination).
			Selectable->AdvanceToNextWaypoint();
			TrackedMannequins.RemoveAt(Idx);
		}
	}

	if (TrackedMannequins.Num() == 0 && AliveTimeSec >= MinAliveSeconds)
	{
		Destroy();
	}
}
