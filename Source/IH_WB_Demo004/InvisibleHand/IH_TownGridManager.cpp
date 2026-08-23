// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_TownGridManager.h"

#include "IH_WB_Demo004.h"
#include "IH_TownGridDataSubsystem.h"
#include "IH_TownGridOverlayComponent.h"
#include "IH_TownGridSquaredGenerator.h"
#include "IHInvisibleHandDesignSpec.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace IH_TownGridManagerPrivate
{
	static const TCHAR* GripNames[] = {
		TEXT("GripCornerNE"),
		TEXT("GripCornerNW"),
		TEXT("GripCornerSE"),
		TEXT("GripCornerSW"),
		TEXT("GripEdgeNorth"),
		TEXT("GripEdgeSouth"),
		TEXT("GripEdgeEast"),
		TEXT("GripEdgeWest"),
	};

	static EIHTownGridGripHandle GripIndexToHandle(int32 Index)
	{
		switch (Index)
		{
		case 0: return EIHTownGridGripHandle::CornerNE;
		case 1: return EIHTownGridGripHandle::CornerNW;
		case 2: return EIHTownGridGripHandle::CornerSE;
		case 3: return EIHTownGridGripHandle::CornerSW;
		case 4: return EIHTownGridGripHandle::EdgeNorth;
		case 5: return EIHTownGridGripHandle::EdgeSouth;
		case 6: return EIHTownGridGripHandle::EdgeEast;
		case 7: return EIHTownGridGripHandle::EdgeWest;
		default: return EIHTownGridGripHandle::None;
		}
	}

	static FVector2D GripLocalOffset(EIHTownGridGripHandle Handle, const FVector2D& HalfExtent)
	{
		switch (Handle)
		{
		case EIHTownGridGripHandle::CornerNE: return FVector2D(HalfExtent.X, HalfExtent.Y);
		case EIHTownGridGripHandle::CornerNW: return FVector2D(-HalfExtent.X, HalfExtent.Y);
		case EIHTownGridGripHandle::CornerSE: return FVector2D(HalfExtent.X, -HalfExtent.Y);
		case EIHTownGridGripHandle::CornerSW: return FVector2D(-HalfExtent.X, -HalfExtent.Y);
		case EIHTownGridGripHandle::EdgeNorth: return FVector2D(0.f, HalfExtent.Y);
		case EIHTownGridGripHandle::EdgeSouth: return FVector2D(0.f, -HalfExtent.Y);
		case EIHTownGridGripHandle::EdgeEast: return FVector2D(HalfExtent.X, 0.f);
		case EIHTownGridGripHandle::EdgeWest: return FVector2D(-HalfExtent.X, 0.f);
		default: return FVector2D::ZeroVector;
		}
	}

	static FVector LocalToWorld(const AIH_TownGridManager* Manager, const FVector2D& LocalXY)
	{
		const FVector Center = Manager->GetActorLocation();
		const float YawRad = FMath::DegreesToRadians(Manager->GetActorRotation().Yaw);
		const float CosA = FMath::Cos(YawRad);
		const float SinA = FMath::Sin(YawRad);
		return FVector(
			Center.X + LocalXY.X * CosA - LocalXY.Y * SinA,
			Center.Y + LocalXY.X * SinA + LocalXY.Y * CosA,
			Center.Z);
	}

	static FVector2D WorldToLocal(const AIH_TownGridManager* Manager, const FVector& World)
	{
		const FVector Center = Manager->GetActorLocation();
		const float YawRad = FMath::DegreesToRadians(Manager->GetActorRotation().Yaw);
		const float CosA = FMath::Cos(YawRad);
		const float SinA = FMath::Sin(YawRad);
		const float Dx = World.X - Center.X;
		const float Dy = World.Y - Center.Y;
		return FVector2D(Dx * CosA + Dy * SinA, -Dx * SinA + Dy * CosA);
	}
}

FVector2D AIH_TownGridManager::GetDefaultBboxHalfExtentCm()
{
	const float HalfFootprint = ModuleSizeCm * static_cast<float>(DefaultModuleCount) * 0.5f;
	return FVector2D(HalfFootprint, HalfFootprint);
}

FName AIH_TownGridManager::TownGridTemplateToRowName(EIHTownGridTemplate Template)
{
	const UEnum* EnumPtr = StaticEnum<EIHTownGridTemplate>();
	if (!EnumPtr)
	{
		return NAME_None;
	}
	return FName(*EnumPtr->GetNameStringByIndex(static_cast<int32>(Template)));
}

AIH_TownGridManager::AIH_TownGridManager()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	GridOverlay = CreateDefaultSubobject<UTownGridOverlayComponent>(TEXT("GridOverlay"));

	FocusBBox = CreateDefaultSubobject<UBoxComponent>(TEXT("FocusBBox"));
	FocusBBox->SetupAttachment(RootScene);
	FocusBBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FocusBBox->SetHiddenInGame(false);
	FocusBBox->ShapeColor = IHInvisibleHandSpec::TownGridFocusOutlineBlue.ToFColor(true);
	FocusBBox->SetLineThickness(2.f);

	BboxHalfExtentCm = GetDefaultBboxHalfExtentCm();
	const FVector BboxExtent(BboxHalfExtentCm.X, BboxHalfExtentCm.Y, 50.f);
	FocusBBox->SetBoxExtent(BboxExtent);

	GripHandles.Reserve(8);
	for (int32 GripIndex = 0; GripIndex < 8; ++GripIndex)
	{
		UBoxComponent* Grip = CreateDefaultSubobject<UBoxComponent>(IH_TownGridManagerPrivate::GripNames[GripIndex]);
		Grip->SetupAttachment(RootScene);
		Grip->SetBoxExtent(FVector(GripBoxHalfSizeCm));
		Grip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Grip->SetHiddenInGame(false);
		Grip->ShapeColor = IHInvisibleHandSpec::TownGridFocusOutlineBlue.ToFColor(true);
		Grip->SetLineThickness(3.f);
		Grip->SetVisibility(false);
		GripHandles.Add(Grip);
	}
}

void AIH_TownGridManager::BeginPlay()
{
	Super::BeginPlay();
	LoadTemplateRow();
	AlignActorToTerrainCenter();
	RebuildFromTemplate();
	SetGripsVisible(false);
}

void AIH_TownGridManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bSelected)
	{
		return;
	}

	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		const bool bQ = PC->IsInputKeyDown(EKeys::Q);
		const bool bE = PC->IsInputKeyDown(EKeys::E);
		if (bQ != bE)
		{
			ApplyYawStep(bQ ? -YawStepDeg : YawStepDeg);
		}
	}
}

void AIH_TownGridManager::LoadTemplateRow()
{
	TemplateData = FIHTownGridTemplateRow();
	const FName RowName = TownGridTemplateToRowName(TownGridTemplate);
	if (RowName.IsNone())
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UIHTownGridDataSubsystem* Data = GI->GetSubsystem<UIHTownGridDataSubsystem>())
			{
				if (const UDataTable* Table = Data->GetTownGridTemplateTable())
				{
					if (const FIHTownGridTemplateRow* Row = Table->FindRow<FIHTownGridTemplateRow>(
						RowName, TEXT("AIH_TownGridManager::LoadTemplateRow")))
					{
						TemplateData = *Row;
					}
				}
			}
		}
	}
}

void AIH_TownGridManager::InitializeFromTemplate(EIHTownGridTemplate Template)
{
	TownGridTemplate = Template;
	LoadTemplateRow();
	RebuildFromTemplate();
}

void AIH_TownGridManager::RebuildFromTemplate()
{
	BboxHalfExtentCm = IH_TownGridSquaredGenerator::SnapHalfExtentToModules(
		BboxHalfExtentCm, ModuleSizeCm, 4);

	if (FocusBBox)
	{
		FocusBBox->SetBoxExtent(FVector(BboxHalfExtentCm.X, BboxHalfExtentCm.Y, 50.f));
	}

	UpdateGripTransforms();

	FTownGridOverlayData OverlayData;
	if (TownGridTemplate == EIHTownGridTemplate::Squared)
	{
		FIHTownGridGeneratorParams Params;
		Params.CenterWorldCm = GetActorLocation();
		Params.BboxHalfExtentCm = BboxHalfExtentCm;
		Params.YawDeg = GetActorRotation().Yaw;
		Params.ModuleSizeCm = ModuleSizeCm;
		Params.CollectorIntervalModules = DefaultCollectorIntervalModules;
		Params.CommonsModules = 4;
		Params.CommonsZonePrimary = TemplateData.defaultCommonsZonePrimary;
		Params.CommonsZoneSecondary = TemplateData.defaultCommonsZoneSecondary;
		IH_TownGridSquaredGenerator::GenerateSquared(Params, OverlayData);
	}
	else
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("TownGridManager: template %d not implemented (M4+) — overlay empty"),
			static_cast<int32>(TownGridTemplate));
	}

	if (GridOverlay)
	{
		GridOverlay->RebuildOverlay(OverlayData);
	}
}

void AIH_TownGridManager::AlignActorToTerrainCenter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, 500000.f);
	FVector TraceEnd = GetActorLocation() - FVector(0.f, 0.f, 500000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TownGridTerrainAlign), true, this);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		FVector Loc = GetActorLocation();
		Loc.Z = Hit.ImpactPoint.Z + 50.f;
		SetActorLocation(Loc);
	}
}

void AIH_TownGridManager::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	EditMode = bInSelected ? EIHTownGridEditMode::GripEdit : EIHTownGridEditMode::Place;
	SetGripsVisible(bInSelected);
	if (!bInSelected)
	{
		EndGripDrag();
		EndMoveDrag();
	}
}

void AIH_TownGridManager::SetGripsVisible(bool bVisible)
{
	for (UBoxComponent* Grip : GripHandles)
	{
		if (Grip)
		{
			Grip->SetVisibility(bVisible);
		}
	}
}

void AIH_TownGridManager::UpdateGripTransforms()
{
	for (int32 Index = 0; Index < GripHandles.Num(); ++Index)
	{
		UBoxComponent* Grip = GripHandles[Index];
		if (!Grip)
		{
			continue;
		}
		const EIHTownGridGripHandle Handle = IH_TownGridManagerPrivate::GripIndexToHandle(Index);
		const FVector2D Local = IH_TownGridManagerPrivate::GripLocalOffset(Handle, BboxHalfExtentCm);
		Grip->SetRelativeLocation(FVector(Local.X, Local.Y, 80.f));
	}
}

bool AIH_TownGridManager::TryHitGripAtWorld(const FVector& WorldPoint, EIHTownGridGripHandle& OutHandle) const
{
	if (!bSelected)
	{
		return false;
	}

	OutHandle = EIHTownGridGripHandle::None;
	float BestDistSq = GripPickRadiusCm * GripPickRadiusCm;

	for (int32 Index = 0; Index < GripHandles.Num(); ++Index)
	{
		const UBoxComponent* Grip = GripHandles[Index];
		if (!Grip || !Grip->IsVisible())
		{
			continue;
		}

		const EIHTownGridGripHandle Handle = IH_TownGridManagerPrivate::GripIndexToHandle(Index);
		const bool bEdgeGrip = Handle == EIHTownGridGripHandle::EdgeNorth
			|| Handle == EIHTownGridGripHandle::EdgeSouth
			|| Handle == EIHTownGridGripHandle::EdgeEast
			|| Handle == EIHTownGridGripHandle::EdgeWest;
		if (!bEdgeGrip)
		{
			continue;
		}

		const FVector GripWorld = Grip->GetComponentLocation();
		const float DistSq = FVector::DistSquared2D(GripWorld, WorldPoint);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutHandle = Handle;
		}
	}

	return OutHandle != EIHTownGridGripHandle::None;
}

void AIH_TownGridManager::BeginGripDrag(EIHTownGridGripHandle Handle, const FVector& WorldPoint)
{
	if (Handle == EIHTownGridGripHandle::None)
	{
		return;
	}
	bMoveDragActive = false;
	bGripDragActive = true;
	ActiveGripHandle = Handle;
	GripDragStartHalfExtentCm = BboxHalfExtentCm;
	GripDragStartWorld = WorldPoint;
	EditMode = EIHTownGridEditMode::GripEdit;
}

void AIH_TownGridManager::UpdateGripDrag(const FVector& WorldPoint)
{
	if (!bGripDragActive || ActiveGripHandle == EIHTownGridGripHandle::None)
	{
		return;
	}

	const FVector2D LocalNow = IH_TownGridManagerPrivate::WorldToLocal(this, WorldPoint);
	const FVector2D LocalStart = IH_TownGridManagerPrivate::WorldToLocal(this, GripDragStartWorld);
	const FVector2D LocalDelta = LocalNow - LocalStart;

	FVector2D NewHalf = GripDragStartHalfExtentCm;
	switch (ActiveGripHandle)
	{
	case EIHTownGridGripHandle::EdgeNorth:
		NewHalf.Y = GripDragStartHalfExtentCm.Y + LocalDelta.Y;
		break;
	case EIHTownGridGripHandle::EdgeSouth:
		NewHalf.Y = GripDragStartHalfExtentCm.Y - LocalDelta.Y;
		break;
	case EIHTownGridGripHandle::EdgeEast:
		NewHalf.X = GripDragStartHalfExtentCm.X + LocalDelta.X;
		break;
	case EIHTownGridGripHandle::EdgeWest:
		NewHalf.X = GripDragStartHalfExtentCm.X - LocalDelta.X;
		break;
	default:
		return;
	}

	BboxHalfExtentCm = IH_TownGridSquaredGenerator::SnapHalfExtentToModules(NewHalf, ModuleSizeCm, 4);
	RebuildFromTemplate();
}

void AIH_TownGridManager::EndGripDrag()
{
	bGripDragActive = false;
	ActiveGripHandle = EIHTownGridGripHandle::None;
}

void AIH_TownGridManager::BeginMoveDrag(const FVector& WorldPoint)
{
	bMoveDragActive = true;
	MoveDragStartWorld = WorldPoint;
	MoveDragStartActorLoc = GetActorLocation();
	EditMode = EIHTownGridEditMode::Move;
	EndGripDrag();
}

void AIH_TownGridManager::UpdateMoveDrag(const FVector& WorldPoint)
{
	if (!bMoveDragActive)
	{
		return;
	}

	const FVector Delta = WorldPoint - MoveDragStartWorld;
	FVector NewLoc = MoveDragStartActorLoc;
	NewLoc.X += Delta.X;
	NewLoc.Y += Delta.Y;
	SetActorLocation(NewLoc);
	RebuildFromTemplate();
}

void AIH_TownGridManager::EndMoveDrag()
{
	if (!bMoveDragActive)
	{
		return;
	}

	bMoveDragActive = false;
	AlignActorToTerrainCenter();
	RebuildFromTemplate();
}

void AIH_TownGridManager::ApplyYawStep(float DeltaDeg)
{
	if (FMath::IsNearlyZero(DeltaDeg))
	{
		return;
	}
	FRotator Rot = GetActorRotation();
	Rot.Yaw = FMath::UnwindDegrees(Rot.Yaw + DeltaDeg);
	SetActorRotation(Rot);
	RebuildFromTemplate();
}

void AIH_TownGridManager::ApplyWheelYaw(float WheelDelta)
{
	if (FMath::IsNearlyZero(WheelDelta))
	{
		return;
	}
	ApplyYawStep(WheelDelta > 0.f ? WheelYawDegPerUnit : -WheelYawDegPerUnit);
}

bool AIH_TownGridManager::ContainsWorldPointXY(const FVector& WorldPoint) const
{
	const FVector2D Local = IH_TownGridManagerPrivate::WorldToLocal(this, WorldPoint);
	return FMath::Abs(Local.X) <= BboxHalfExtentCm.X && FMath::Abs(Local.Y) <= BboxHalfExtentCm.Y;
}
