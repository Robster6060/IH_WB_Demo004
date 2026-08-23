// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_StructurePlacementActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace IH_StructurePlacementPrivate
{
	static const TCHAR* MeshPathForItem(FName PaletteItemID)
	{
		static const TMap<FName, const TCHAR*> ItemToMeshPath = {
			{FName(TEXT("Build_DEV_SmallHouse")),
				TEXT("/Game/InvisibleHand/Structures/Meshes/Placeholders/SM_Structure_SmallHouse.SM_Structure_SmallHouse")},
			{FName(TEXT("Build_DEV_SmallDockHouse")),
				TEXT("/Game/InvisibleHand/Structures/Meshes/Placeholders/SM_Structure_SmallDockHouse.SM_Structure_SmallDockHouse")},
			{FName(TEXT("Build_DEV_MediumWorkshop")),
				TEXT("/Game/InvisibleHand/Structures/Meshes/Placeholders/SM_Structure_MediumWorkshop.SM_Structure_MediumWorkshop")},
			{FName(TEXT("Build_DEV_LargeChurch")),
				TEXT("/Game/InvisibleHand/Structures/Meshes/Placeholders/SM_Structure_LargeChurch.SM_Structure_LargeChurch")},
			{FName(TEXT("Build_DEV_GrandTheater")),
				TEXT("/Game/InvisibleHand/Structures/Meshes/Placeholders/SM_Structure_GrandTheater.SM_Structure_GrandTheater")},
		};

		if (const TCHAR* const* Path = ItemToMeshPath.Find(PaletteItemID))
		{
			return *Path;
		}
		return nullptr;
	}

	static const TMap<FName, FVector>& FootprintExtentsCm()
	{
		static const TMap<FName, FVector> Footprints = {
			{FName(TEXT("Build_DEV_SmallHouse")), FVector(800.f, 600.f, 400.f)},
			{FName(TEXT("Build_DEV_SmallDockHouse")), FVector(800.f, 600.f, 400.f)},
			{FName(TEXT("Build_DEV_MediumWorkshop")), FVector(1200.f, 800.f, 500.f)},
			{FName(TEXT("Build_DEV_LargeChurch")), FVector(1600.f, 1000.f, 600.f)},
			{FName(TEXT("Build_DEV_GrandTheater")), FVector(2000.f, 1200.f, 700.f)},
		};
		return Footprints;
	}
}

AIH_StructurePlacementActor::AIH_StructurePlacementActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetGenerateOverlapEvents(false);
}

void AIH_StructurePlacementActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyStructureMesh();
}

void AIH_StructurePlacementActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyStructureMesh();
}

void AIH_StructurePlacementActor::ApplyStructureMesh()
{
	if (!MeshComponent)
	{
		return;
	}

	if (StructureMesh.IsNull())
	{
		return;
	}

	if (UStaticMesh* LoadedMesh = StructureMesh.LoadSynchronous())
	{
		MeshComponent->SetStaticMesh(LoadedMesh);
	}
}

bool AIH_StructurePlacementActor::HasPlacementMesh() const
{
	return MeshComponent && MeshComponent->GetStaticMesh() != nullptr;
}

bool AIH_StructurePlacementActor::GetFootprintExtentCm(FName PaletteItemID, FVector& OutExtentCm)
{
	if (const FVector* Found = IH_StructurePlacementPrivate::FootprintExtentsCm().Find(PaletteItemID))
	{
		OutExtentCm = *Found;
		return true;
	}
	OutExtentCm = FVector(800.f, 600.f, 400.f);
	return false;
}

bool AIH_StructurePlacementActor::ComputeActorOriginFromMeshBottom(
	const UStaticMesh* Mesh,
	const FVector& SurfacePoint,
	float DoorSillOffsetCm,
	FVector& OutActorOrigin)
{
	if (!Mesh)
	{
		return false;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const float BottomLocalZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	OutActorOrigin = SurfacePoint - FVector(0.f, 0.f, BottomLocalZ) + FVector(0.f, 0.f, DoorSillOffsetCm);
	return true;
}

bool AIH_StructurePlacementActor::ComputePlacementOriginFromSurface(
	FName PaletteItemID,
	const FVector& SurfacePoint,
	FVector& OutActorOrigin)
{
	if (UStaticMesh* Mesh = LoadDevPlaceholderMesh(PaletteItemID))
	{
		return ComputeActorOriginFromMeshBottom(
			Mesh, SurfacePoint, DefaultDoorOriginTerrainOffsetCm, OutActorOrigin);
	}

	FVector ExtentCm(800.f, 600.f, 400.f);
	GetFootprintExtentCm(PaletteItemID, ExtentCm);
	OutActorOrigin = SurfacePoint + FVector(0.f, 0.f, ExtentCm.Z * 0.5f);
	return true;
}

UStaticMesh* AIH_StructurePlacementActor::LoadDevPlaceholderMesh(FName PaletteItemID)
{
	const TCHAR* MeshPath = IH_StructurePlacementPrivate::MeshPathForItem(PaletteItemID);
	if (!MeshPath)
	{
		return nullptr;
	}

	return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, MeshPath));
}

bool AIH_StructurePlacementActor::TryApplyDevPlaceholderMesh(FName PaletteItemID)
{
	if (!MeshComponent)
	{
		return false;
	}

	if (MeshComponent->GetStaticMesh() && !bUsesFallbackFootprintMesh
		&& ActivePaletteItemID == PaletteItemID)
	{
		return true;
	}

	if (UStaticMesh* LoadedMesh = LoadDevPlaceholderMesh(PaletteItemID))
	{
		bUsesFallbackFootprintMesh = false;
		StructureMesh = LoadedMesh;
		MeshComponent->SetStaticMesh(LoadedMesh);
		MeshComponent->SetRelativeLocation(FVector::ZeroVector);
		MeshComponent->SetWorldScale3D(FVector::OneVector);
		return true;
	}

	return false;
}

bool AIH_StructurePlacementActor::TryApplyFallbackFootprintMesh(FName PaletteItemID)
{
	if (!MeshComponent)
	{
		return false;
	}

	if (MeshComponent->GetStaticMesh() && bUsesFallbackFootprintMesh)
	{
		return true;
	}

	if (TryApplyDevPlaceholderMesh(PaletteItemID))
	{
		return true;
	}

	UStaticMesh* CubeMesh = Cast<UStaticMesh>(StaticLoadObject(
		UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	if (!CubeMesh)
	{
		return false;
	}

	FVector ExtentCm(800.f, 600.f, 400.f);
	GetFootprintExtentCm(PaletteItemID, ExtentCm);
	FootprintHalfHeightCm = ExtentCm.Z * 0.5f;
	bUsesFallbackFootprintMesh = true;

	MeshComponent->SetStaticMesh(CubeMesh);
	MeshComponent->SetRelativeLocation(FVector::ZeroVector);
	MeshComponent->SetWorldScale3D(FVector(
		ExtentCm.X / 100.f,
		ExtentCm.Y / 100.f,
		ExtentCm.Z / 100.f));
	return true;
}

bool AIH_StructurePlacementActor::EnsureVisiblePlacementMesh(FName PaletteItemID)
{
	ActivePaletteItemID = PaletteItemID;
	return TryApplyDevPlaceholderMesh(PaletteItemID) || TryApplyFallbackFootprintMesh(PaletteItemID);
}

bool AIH_StructurePlacementActor::EnsureDevFootprintCubeMesh(FName PaletteItemID)
{
	ActivePaletteItemID = PaletteItemID;
	if (MeshComponent && MeshComponent->GetStaticMesh() && bUsesFallbackFootprintMesh)
	{
		return true;
	}
	return TryApplyFallbackFootprintMesh(PaletteItemID);
}

void AIH_StructurePlacementActor::SetPlacementPaletteItem(FName PaletteItemID)
{
	ActivePaletteItemID = PaletteItemID;
	EnsureVisiblePlacementMesh(PaletteItemID);
}

void AIH_StructurePlacementActor::SetBuildDragPreviewMode(bool bPreview)
{
	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(
			bPreview ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetCastShadow(!bPreview);
		MeshComponent->SetVisibility(true, true);
		MeshComponent->SetHiddenInGame(false);
		MeshComponent->SetRenderInMainPass(true);
		MeshComponent->SetRenderInDepthPass(true);
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->MarkRenderStateDirty();
	}

	SetActorEnableCollision(!bPreview);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(false);
}

void AIH_StructurePlacementActor::RestoreDefaultPlacementMaterials()
{
	if (!MeshComponent)
	{
		return;
	}

	UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	bDragGhostVisualStyleActive = false;
	CachedSourceMaterials.Reset();
	MeshComponent->SetTranslucentSortPriority(0);

	static UMaterialInterface* FallbackMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* FallbackMID = nullptr;
	if (FallbackMaterial)
	{
		FallbackMID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		if (FallbackMID)
		{
			static const FLinearColor PlacedTint(0.72f, 0.74f, 0.78f, 1.f);
			FallbackMID->SetVectorParameterValue(TEXT("Color"), PlacedTint);
		}
	}

	const int32 SlotCount = FMath::Max(1, Mesh->GetStaticMaterials().Num());
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (UMaterialInterface* Material = Mesh->GetMaterial(SlotIndex))
		{
			MeshComponent->SetMaterial(SlotIndex, Material);
		}
		else if (FallbackMID)
		{
			MeshComponent->SetMaterial(SlotIndex, FallbackMID);
		}
	}

	MeshComponent->MarkRenderStateDirty();
}

namespace IH_StructurePlacementPrivate
{
	static UMaterialInstanceDynamic* CreateDragGhostMaterial(UObject* Outer)
	{
		const TCHAR* ParentPaths[] = {
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"),
			TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"),
		};

		UMaterialInterface* ParentMaterial = nullptr;
		for (const TCHAR* Path : ParentPaths)
		{
			if (UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, Path))
			{
				ParentMaterial = Loaded;
				break;
			}
		}
		if (!ParentMaterial || !Outer)
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMaterial, Outer);
		if (!MID)
		{
			return nullptr;
		}

		static const FLinearColor GhostBlue(0.12f, 0.48f, 1.f, 0.88f);
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& ColorName : ColorNames)
		{
			MID->SetVectorParameterValue(ColorName, GhostBlue);
		}
		MID->SetScalarParameterValue(FName(TEXT("Opacity")), 0.88f);
		MID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.35f);
		return MID;
	}
}

void AIH_StructurePlacementActor::ApplyPlacementVisualStyle(bool bDragGhost)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	if (!bDragGhost)
	{
		RestoreDefaultPlacementMaterials();
		return;
	}

	bDragGhostVisualStyleActive = false;

	UMaterialInstanceDynamic* GhostMID = IH_StructurePlacementPrivate::CreateDragGhostMaterial(this);
	const int32 SlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (GhostMID)
		{
			MeshComponent->SetMaterial(SlotIndex, GhostMID);
		}
	}

	MeshComponent->SetTranslucentSortPriority(100);
	MeshComponent->SetVisibility(true, true);
	MeshComponent->SetHiddenInGame(false);
	MeshComponent->SetRenderInMainPass(true);
	bDragGhostVisualStyleActive = true;
	MeshComponent->MarkRenderStateDirty();
}

void AIH_StructurePlacementActor::ApplyPlacedDevVisualStyle()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	bDragGhostVisualStyleActive = false;
	CachedSourceMaterials.Reset();
	MeshComponent->SetTranslucentSortPriority(0);

	UMaterialInstanceDynamic* PlacedMID = IH_StructurePlacementPrivate::CreateDragGhostMaterial(this);
	if (PlacedMID)
	{
		static const FLinearColor PlacedBlue(0.1f, 0.35f, 0.95f, 1.f);
		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& ColorName : ColorNames)
		{
			PlacedMID->SetVectorParameterValue(ColorName, PlacedBlue);
		}
		PlacedMID->SetScalarParameterValue(FName(TEXT("Opacity")), 1.f);
		PlacedMID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.45f);

		const int32 SlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			MeshComponent->SetMaterial(SlotIndex, PlacedMID);
		}
	}

	MeshComponent->SetVisibility(true, true);
	MeshComponent->SetHiddenInGame(false);
	MeshComponent->MarkRenderStateDirty();
}

void AIH_StructurePlacementActor::AlignToTerrainCenter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector ActorLoc = GetActorLocation();
	FVector SurfacePoint = ActorLoc;
	bool bFoundSurface = false;

	if (const UGameInstance* GI = World->GetGameInstance())
	{
		if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
			GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
		{
			FVector IslandSurface = FVector::ZeroVector;
			if (IslandCollision->TrySampleIslandSurfaceAtXY(
				FVector2D(ActorLoc.X, ActorLoc.Y),
				ActorLoc.Z,
				0.f,
				this,
				IslandSurface))
			{
				SurfacePoint = IslandSurface;
				bFoundSurface = true;
			}
		}
	}

	if (!bFoundSurface)
	{
		const FVector TraceStart = ActorLoc + FVector(0.f, 0.f, 500000.f);
		const FVector TraceEnd = ActorLoc - FVector(0.f, 0.f, 500000.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(StructureTerrainAlign), true, this);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			SurfacePoint = Hit.ImpactPoint;
			bFoundSurface = true;
		}
	}

	if (!bFoundSurface)
	{
		return;
	}

	FVector Loc = ActorLoc;
	if (bUsesFallbackFootprintMesh)
	{
		Loc.Z = SurfacePoint.Z + FootprintHalfHeightCm;
	}
	else if (const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr)
	{
		FVector AlignedOrigin = ActorLoc;
		if (ComputeActorOriginFromMeshBottom(Mesh, SurfacePoint, DoorOriginTerrainOffsetCm, AlignedOrigin))
		{
			Loc.Z = AlignedOrigin.Z;
		}
		else
		{
			Loc.Z = SurfacePoint.Z + DoorOriginTerrainOffsetCm;
		}
	}
	else
	{
		Loc.Z = SurfacePoint.Z + DoorOriginTerrainOffsetCm;
	}
	SetActorLocation(Loc);
}
