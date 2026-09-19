// Copyright Epic Games, Inc. All Rights Reserved.
#include "IH_BuildPaletteSubsystem.h"
#include "IH_BuildPaletteHostWidget.h"
#include "IH_BuildPaletteItemRow.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_Cube2FlyPlayerController.h"
#include "IH_TownGridDataSubsystem.h"
#include "IH_StructurePlacementActor.h"
#include "IH_TownGridManager.h"
#include "IH_TownGridOverlayComponent.h"
#include "IH_TownGridSquaredGenerator.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_P1C07_ShipRegistrySubsystem.h"
#include "IH_WB_IslandActor.h"
#include "FIHTerrainStampTypes.h"
#include "FIHTerrainStampMeshTypes.h"
#include "IH_TerrainStampLibrary.h"
#include "IH_TerrainStampActor.h"
#include "IH_WB_Demo004GameMode.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace IH_BuildPaletteSubsystemPrivate
{
	static TAutoConsoleVariable<int32> CVarBuildPaletteDrawDebugFootprint(
		TEXT("ih.BuildPalette.DrawDebugFootprint"),
		0,
		TEXT("When 1, draw debug footprint box during B Build structure drag (mesh ghost is primary preview)."),
		ECVF_Default);

	static bool TrySampleIslandSurfaceForBuildDrag(
		const AIH_Cube2FlyPlayerController* FlyPC,
		const FVector2D& WorldXY,
		float ReferenceZ,
		const AActor* IgnoreActor,
		FVector& OutIslandSurface,
		AActor** OutIslandActor = nullptr)
	{
		if (!FlyPC)
		{
			return false;
		}

		const UGameInstance* GI = FlyPC->GetGameInstance();
		if (!GI)
		{
			return false;
		}

		const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
			GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>();
		if (!IslandCollision)
		{
			return false;
		}

		return IslandCollision->TrySampleIslandSurfaceAtXY(
			WorldXY, ReferenceZ, 0.f, IgnoreActor, OutIslandSurface, OutIslandActor);
	}

	static float GetBuildDragReferenceZ(const AIH_Cube2FlyPlayerController* FlyPC, float FallbackZ)
	{
		if (const APawn* Pawn = FlyPC ? FlyPC->GetPawn() : nullptr)
		{
			return Pawn->GetActorLocation().Z;
		}
		return FallbackZ;
	}

	static bool IsAboveWaterSurface(
		const AIH_Cube2FlyPlayerController* FlyPC,
		const FVector& SurfacePoint,
		float MinClearanceCm = 40.f)
	{
		if (!FlyPC)
		{
			return false;
		}

		FVector WaterPoint = FVector::ZeroVector;
		if (!FlyPC->TryGetWorldPointOnWaterPlane(
			FVector2D(SurfacePoint.X, SurfacePoint.Y), WaterPoint))
		{
			return SurfacePoint.Z > MinClearanceCm;
		}

		return SurfacePoint.Z >= WaterPoint.Z + MinClearanceCm;
	}

	/** Slim selection ring at proc-mesh footprint (selected stamp only). */
	static void DrawStampSelectionRing(
		UWorld* World,
		const AIH_Cube2FlyPlayerController* FlyPC,
		const AIH_TerrainStampActor* Stamp,
		const FColor& Color)
	{
		if (!World || !Stamp)
		{
			return;
		}

		const FVector Anchor = Stamp->GetActorLocation();
		const float FootprintRadiusCm = Stamp->GetPreviewFootprintRadiusCm();
		const float RimZ = Anchor.Z + IHInvisibleHandSpec::StampPlacedPreviewSurfaceOffsetCm;
		const FVector Hub(Anchor.X, Anchor.Y, RimZ);
		const float LineThick = FMath::Max(
			FlyPC ? FlyPC->ComputeWorldSizeForScreenPixels(Hub, 4.f) : 0.f,
			FMath::Max(FootprintRadiusCm * 0.004f, 800.f));

		DrawDebugCircle(
			World,
			Hub,
			FootprintRadiusCm * 0.98f,
			40,
			Color,
			false,
			-1.f,
			0,
			LineThick,
			FVector(0.f, 1.f, 0.f),
			FVector(1.f, 0.f, 0.f),
			false);
	}

	static bool TryResolveTerrainStampDragAtScreen(
		AIH_Cube2FlyPlayerController* FlyPC,
		const FVector2D& ScreenPos,
		FVector& OutSurfaceWorld,
		AIH_WB_IslandActor*& OutIsland)
	{
		OutIsland = nullptr;
		if (!FlyPC)
		{
			return false;
		}

		FVector IslandSurface = FVector::ZeroVector;
		AActor* IslandActor = nullptr;
		if (!FlyPC->TrySampleIslandSurfaceAtScreen(ScreenPos, IslandSurface, &IslandActor))
		{
			return false;
		}

		if (!IsAboveWaterSurface(FlyPC, IslandSurface))
		{
			return false;
		}

		OutSurfaceWorld = IslandSurface;
		OutIsland = Cast<AIH_WB_IslandActor>(IslandActor);
		return OutIsland != nullptr;
	}

	static bool TryResolveTerrainStampDragAtWorldXY(
		AIH_Cube2FlyPlayerController* FlyPC,
		const FVector2D& WorldXY,
		FVector& OutSurfaceWorld,
		AIH_WB_IslandActor*& OutIsland)
	{
		OutIsland = nullptr;
		if (!FlyPC)
		{
			return false;
		}

		FVector IslandSurface = FVector::ZeroVector;
		AActor* IslandActor = nullptr;
		const float ReferenceZ = GetBuildDragReferenceZ(FlyPC, 0.f);
		if (!TrySampleIslandSurfaceForBuildDrag(
			FlyPC, WorldXY, ReferenceZ, nullptr, IslandSurface, &IslandActor))
		{
			return false;
		}

		if (!IsAboveWaterSurface(FlyPC, IslandSurface))
		{
			return false;
		}

		OutSurfaceWorld = IslandSurface;
		OutIsland = Cast<AIH_WB_IslandActor>(IslandActor);
		return OutIsland != nullptr;
	}

	static void SetBuildDragGhostFromSurfaceImpact(
		FName PaletteItemID,
		const FVector& SurfaceImpactPoint,
		FVector& OutDrawCenterWorld,
		FVector& OutActorOrigin);

	static bool TryCommitBuildDragAtIslandSurface(
		AIH_Cube2FlyPlayerController* FlyPC,
		FName PaletteItemID,
		const FVector& IslandSurface,
		FVector& OutDrawCenterWorld,
		FVector& OutActorOrigin)
	{
		if (!IsAboveWaterSurface(FlyPC, IslandSurface))
		{
			return false;
		}

		SetBuildDragGhostFromSurfaceImpact(
			PaletteItemID, IslandSurface, OutDrawCenterWorld, OutActorOrigin);
		return true;
	}

	static bool TryResolveValidBuildDragAtScreen(
		AIH_Cube2FlyPlayerController* FlyPC,
		FName PaletteItemID,
		const FVector2D& ScreenPos,
		const AActor* IgnoreActor,
		FVector& OutDrawCenterWorld,
		FVector& OutActorOrigin)
	{
		if (!FlyPC)
		{
			return false;
		}

		FVector ActorOrigin = FVector::ZeroVector;
		if (FlyPC->TryResolveStructurePlacementAtScreen(ScreenPos, PaletteItemID, ActorOrigin))
		{
			FVector IslandSurface = FVector::ZeroVector;
			const float ReferenceZ = GetBuildDragReferenceZ(FlyPC, ActorOrigin.Z);
			if (TrySampleIslandSurfaceForBuildDrag(
				FlyPC,
				FVector2D(ActorOrigin.X, ActorOrigin.Y),
				ReferenceZ,
				IgnoreActor,
				IslandSurface))
			{
				return TryCommitBuildDragAtIslandSurface(
					FlyPC, PaletteItemID, IslandSurface, OutDrawCenterWorld, OutActorOrigin);
			}

			FVector SolidSurface = FVector::ZeroVector;
			if (FlyPC->TryTraceSolidSurfaceAtScreen(ScreenPos, SolidSurface))
			{
				return TryCommitBuildDragAtIslandSurface(
					FlyPC, PaletteItemID, SolidSurface, OutDrawCenterWorld, OutActorOrigin);
			}
		}

		FVector WaterPoint = FVector::ZeroVector;
		if (!FlyPC->TryGetWorldPointOnWaterPlane(ScreenPos, WaterPoint))
		{
			return false;
		}

		FVector IslandSurface = FVector::ZeroVector;
		const float ReferenceZ = GetBuildDragReferenceZ(FlyPC, WaterPoint.Z);
		if (!TrySampleIslandSurfaceForBuildDrag(
			FlyPC, FVector2D(WaterPoint.X, WaterPoint.Y), ReferenceZ, IgnoreActor, IslandSurface))
		{
			return false;
		}

		return TryCommitBuildDragAtIslandSurface(
			FlyPC, PaletteItemID, IslandSurface, OutDrawCenterWorld, OutActorOrigin);
	}

	static bool TryResolveValidBuildDragAtWorldXY(
		AIH_Cube2FlyPlayerController* FlyPC,
		FName PaletteItemID,
		const FVector2D& WorldXY,
		const AActor* IgnoreActor,
		FVector& OutDrawCenterWorld,
		FVector& OutActorOrigin)
	{
		if (!FlyPC)
		{
			return false;
		}

		FVector ActorOrigin = FVector::ZeroVector;
		if (!FlyPC->TryResolveStructurePlacementAtWorldXY(WorldXY, PaletteItemID, ActorOrigin))
		{
			return false;
		}

		FVector IslandSurface = FVector::ZeroVector;
		const float ReferenceZ = GetBuildDragReferenceZ(FlyPC, ActorOrigin.Z);
		if (!TrySampleIslandSurfaceForBuildDrag(FlyPC, WorldXY, ReferenceZ, IgnoreActor, IslandSurface))
		{
			return false;
		}

		if (!TryCommitBuildDragAtIslandSurface(
			FlyPC, PaletteItemID, IslandSurface, OutDrawCenterWorld, OutActorOrigin))
		{
			return false;
		}
		return true;
	}
	static UClass* ResolveStructurePlacementActorClass(const FIHBuildPaletteItemRow& Row)
	{
		if (!Row.actorClass.IsNull())
		{
			if (UClass* LoadedClass = Row.actorClass.LoadSynchronous())
			{
				if (LoadedClass->IsChildOf(AIH_StructurePlacementActor::StaticClass()))
				{
					return LoadedClass;
				}
			}
		}

		return AIH_StructurePlacementActor::StaticClass();
	}

	static void DrawBuildStructureFootprintOutline(
		const UWorld* World,
		const FVector& SurfaceCenterWorld,
		const FVector& FootprintExtentCm,
		FName /*PaletteItemID*/,
		const AIH_StructurePlacementActor* /*PreviewActor*/,
		bool /*bMeshGhostActive*/)
	{
		FVector BoxCenter = SurfaceCenterWorld + FVector(0.f, 0.f, FootprintExtentCm.Z * 0.5f);
		FVector HalfExtent = FootprintExtentCm * 0.5f;

		// Restored visible path (request daa6fff0 era): Foreground solid + wire on top of world pass.
		const uint8 DepthPriorities[] = { SDPG_Foreground, 0 };
		for (const uint8 GhostDPG : DepthPriorities)
		{
			DrawDebugSolidBox(
				World,
				BoxCenter,
				HalfExtent,
				FColor(30, 140, 255, 120),
				false,
				-1.f,
				GhostDPG);
			DrawDebugBox(
				World,
				BoxCenter,
				HalfExtent,
				FQuat::Identity,
				FColor(0, 200, 255, 255),
				false,
				-1.f,
				GhostDPG,
				18.f);
		DrawDebugBox(
			World,
			BoxCenter,
			HalfExtent,
			FQuat::Identity,
			FColor(120, 230, 255, 255),
			false,
			-1.f,
			GhostDPG,
			40.f);
		}
	}

	static bool TryResolveBuildDragLocation(
		AIH_Cube2FlyPlayerController* FlyPC,
		FName PaletteItemID,
		const FVector2D& ScreenPos,
		FVector& OutActorOrigin)
	{
		if (!FlyPC)
		{
			return false;
		}

		if (FlyPC->TryResolveStructurePlacementAtScreen(ScreenPos, PaletteItemID, OutActorOrigin))
		{
			return true;
		}

		FVector WaterPoint = FVector::ZeroVector;
		if (FlyPC->TryGetWorldPointOnWaterPlane(ScreenPos, WaterPoint))
		{
			FVector SurfacePoint = WaterPoint;
			if (const UGameInstance* GI = FlyPC->GetGameInstance())
			{
				const AActor* IgnoreActor = nullptr;
				if (const UIH_BuildPaletteSubsystem* BuildPalette = GI->GetSubsystem<UIH_BuildPaletteSubsystem>())
				{
					IgnoreActor = BuildPalette->GetDragPreviewIgnoreActor();
				}
				if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
					GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
				{
					FVector IslandSurface = FVector::ZeroVector;
					if (IslandCollision->TrySampleIslandSurfaceAtXY(
						FVector2D(WaterPoint.X, WaterPoint.Y),
						WaterPoint.Z,
						0.f,
						IgnoreActor,
						IslandSurface))
					{
						SurfacePoint = IslandSurface;
					}
				}
			}
			return AIH_StructurePlacementActor::ComputePlacementOriginFromSurface(
				PaletteItemID, SurfacePoint, OutActorOrigin);
		}

		return false;
	}

	static bool TryResolveBuildDragLocationFromWorldXY(
		AIH_Cube2FlyPlayerController* FlyPC,
		FName PaletteItemID,
		const FVector2D& WorldXY,
		FVector& OutActorOrigin)
	{
		if (!FlyPC)
		{
			return false;
		}

		return FlyPC->TryResolveStructurePlacementAtWorldXY(WorldXY, PaletteItemID, OutActorOrigin);
	}

	static void SetBuildDragGhostFromSurfaceImpact(
		FName PaletteItemID,
		const FVector& SurfaceImpactPoint,
		FVector& OutDrawCenterWorld,
		FVector& OutActorOrigin)
	{
		OutDrawCenterWorld = SurfaceImpactPoint + FVector(0.f, 0.f, 50.f);
		AIH_StructurePlacementActor::ComputePlacementOriginFromSurface(
			PaletteItemID, SurfaceImpactPoint, OutActorOrigin);
	}


	static bool TryResolveGridDragLocationFromWorldXY(
		AIH_Cube2FlyPlayerController* FlyPC,
		const FVector2D& WorldXY,
		FVector& OutSpawnLocation)
	{
		if (!FlyPC)
		{
			return false;
		}

		float ReferenceZ = 0.f;
		if (const APawn* Pawn = FlyPC->GetPawn())
		{
			ReferenceZ = Pawn->GetActorLocation().Z;
		}

		FVector SurfacePoint(WorldXY.X, WorldXY.Y, 0.f);
		if (const UGameInstance* GI = FlyPC->GetGameInstance())
		{
			if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
				GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
			{
				FVector IslandSurface = FVector::ZeroVector;
				if (IslandCollision->TrySampleIslandSurfaceAtXY(
					WorldXY, ReferenceZ, 0.f, nullptr, IslandSurface))
				{
					SurfacePoint = IslandSurface;
				}
			}
		}

		OutSpawnLocation = SurfacePoint + FVector(0.f, 0.f, 50.f);
		return true;
	}

	static bool AreDevPlaceholderMeshesImported()
	{
		return AIH_StructurePlacementActor::LoadDevPlaceholderMesh(FName(TEXT("Build_DEV_SmallHouse"))) != nullptr;
	}
}
void UIH_BuildPaletteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UIHTownGridDataSubsystem>();
	Super::Initialize(Collection);
	if (GetBuildPaletteDataSubsystem())
	{
		RefreshCachedGridRowCounts();
		RefreshCachedBuildRowCounts();
	}
	else
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("BuildPaletteSubsystem: UIHTownGridDataSubsystem unavailable during Initialize"));
	}

	if (IH_BuildPaletteSubsystemPrivate::AreDevPlaceholderMeshesImported())
	{
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("BuildPaletteSubsystem: dev structure placeholder meshes FOUND in Content"));
	}
	else
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("BuildPaletteSubsystem: dev structure placeholder meshes NOT imported — run Scripts/Editor/ImportStructurePlaceholders.py (using engine-cube fallback until then)"));
	}
}
void UIH_BuildPaletteSubsystem::RefreshCachedGridRowCounts()
{
	CachedGridTemplateRowCount = 0;
	if (const UIHTownGridDataSubsystem* Data = GetBuildPaletteDataSubsystem())
	{
		if (const UDataTable* ItemTable = Data->GetBuildPaletteItemTable())
		{
			for (const FName& RowName : ItemTable->GetRowNames())
			{
				const FIHBuildPaletteItemRow* Row = ItemTable->FindRow<FIHBuildPaletteItemRow>(
					RowName, TEXT("BuildPaletteSubsystem::RefreshCachedGridRowCounts"));
				if (Row && Row->paletteTab == EIHBuildPaletteTab::Grid)
				{
					++CachedGridTemplateRowCount;
				}
				else if (!Row)
				{
					UE_LOG(
						LogIH_WB_Demo004, Warning,
						TEXT("BuildPaletteSubsystem: FindRow failed for %s (row struct mismatch?)"),
						*RowName.ToString());
				}
			}
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("BuildPaletteSubsystem: DT_BuildPaletteItem ready (%d total rows, %d Grid tab rows)"),
				ItemTable->GetRowNames().Num(), CachedGridTemplateRowCount);
		}
		else
		{
			UE_LOG(LogIH_WB_Demo004, Warning, TEXT("BuildPaletteSubsystem: DT_BuildPaletteItem not loaded"));
		}
	}
}

void UIH_BuildPaletteSubsystem::RefreshCachedBuildRowCounts()
{
	CachedBuildRowCount = 0;
	if (const UIHTownGridDataSubsystem* Data = GetBuildPaletteDataSubsystem())
	{
		if (const UDataTable* ItemTable = Data->GetBuildPaletteItemTable())
		{
			for (const FName& RowName : ItemTable->GetRowNames())
			{
				const FIHBuildPaletteItemRow* Row = ItemTable->FindRow<FIHBuildPaletteItemRow>(
					RowName, TEXT("BuildPaletteSubsystem::RefreshCachedBuildRowCounts"));
				if (Row && Row->paletteTab == EIHBuildPaletteTab::Build
					&& Row->interactionType == EIHBuildPaletteInteraction::DropActor)
				{
					++CachedBuildRowCount;
				}
			}
			UE_LOG(
				LogIH_WB_Demo004, Log,
				TEXT("BuildPaletteSubsystem: DT_BuildPaletteItem ready (%d Build tab DropActor rows)"),
				CachedBuildRowCount);
		}
	}
}

bool UIH_BuildPaletteSubsystem::IsStructureBuildDragActive() const
{
	return bDragActive
		&& DragPayload.paletteTab == EIHBuildPaletteTab::Build
		&& DragPayload.interactionType == EIHBuildPaletteInteraction::DropActor;
}

bool UIH_BuildPaletteSubsystem::IsTerrainStampDragActive() const
{
	return bDragActive
		&& DragPayload.paletteTab == EIHBuildPaletteTab::World
		&& DragPayload.interactionType == EIHBuildPaletteInteraction::TerrainStamp;
}

FIHBuildPaletteItemRow UIH_BuildPaletteSubsystem::MakeSyntheticTerrainStampPaletteRow(
	const EIHTerrainStampId StampId)
{
	const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(StampId);
	FIHBuildPaletteItemRow Row;
	Row.itemID = Def.RowName;
	Row.stampRowID = Def.RowName;
	Row.paletteTab = EIHBuildPaletteTab::World;
	Row.interactionType = EIHBuildPaletteInteraction::TerrainStamp;
	Row.displayName = Def.RowName.ToString().Replace(TEXT("Stamp_"), TEXT(""));
	Row.categoryPath = Def.Family == IHInvisibleHandSpec::ETerrainStampFamily::Inverted
		? TEXT("World/Stamps/Inverted")
		: TEXT("World/Stamps/Vertical");
	if (StampId == EIHTerrainStampId::IslandShelf)
	{
		Row.categoryPath = TEXT("World/Stamps/Special");
	}
	Row.levelRequired = EIHBuildPaletteLevel::WorldBuilder;
	Row.phaseMin = 1;
	Row.sortOrder = static_cast<int32>(StampId);
	return Row;
}

bool UIH_BuildPaletteSubsystem::TryGetTerrainStampIdFromRow(
	const FIHBuildPaletteItemRow& Row,
	EIHTerrainStampId& OutStampId)
{
	const FName LookupName = Row.stampRowID.IsNone() ? Row.itemID : Row.stampRowID;
	if (const FIHTerrainStampDefinition* Def = FIHTerrainStampCatalog::FindByRowName(LookupName))
	{
		OutStampId = Def->StampId;
		return true;
	}
	return false;
}

bool UIH_BuildPaletteSubsystem::GetActiveDragFootprintCm(FVector& OutExtentCm) const
{
	if (!bDragActive)
	{
		return false;
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Build)
	{
		return TryGetStructureFootprintCm(DragPayload.itemID, OutExtentCm);
	}

	OutExtentCm = FVector(
		AIH_TownGridManager::GetDefaultBboxHalfExtentCm().X * 2.f,
		AIH_TownGridManager::GetDefaultBboxHalfExtentCm().Y * 2.f,
		50.f);
	return true;
}

bool UIH_BuildPaletteSubsystem::TryGetStructureFootprintCm(FName ItemID, FVector& OutExtentCm)
{
	static const TMap<FName, FVector> Footprints = {
		{FName(TEXT("Build_DEV_SmallHouse")), FVector(800.f, 600.f, 400.f)},
		{FName(TEXT("Build_DEV_SmallDockHouse")), FVector(800.f, 600.f, 400.f)},
		{FName(TEXT("Build_DEV_MediumWorkshop")), FVector(1200.f, 800.f, 500.f)},
		{FName(TEXT("Build_DEV_LargeChurch")), FVector(1600.f, 1000.f, 600.f)},
		{FName(TEXT("Build_DEV_GrandTheater")), FVector(2000.f, 1200.f, 700.f)},
	};
	if (const FVector* Found = Footprints.Find(ItemID))
	{
		OutExtentCm = *Found;
		return true;
	}
	return false;
}

UIHTownGridDataSubsystem* UIH_BuildPaletteSubsystem::GetBuildPaletteDataSubsystem() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UIHTownGridDataSubsystem>();
	}
	return nullptr;
}
void UIH_BuildPaletteSubsystem::EnsureWidget(AIH_Cube2FlyPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	if (BuildPaletteWidget && BuildPaletteWidget->GetOwningPlayer() != PC)
	{
		if (BuildPaletteWidget->IsInViewport())
		{
			BuildPaletteWidget->RemoveFromParent();
		}
		BuildPaletteWidget = nullptr;
	}
	if (BuildPaletteWidget)
	{
		return;
	}
	BuildPaletteWidget = CreateWidget<UIH_BuildPaletteHostWidget>(
		PC, UIH_BuildPaletteHostWidget::StaticClass());
	if (BuildPaletteWidget)
	{
		BuildPaletteWidget->EnsureWidgetTreeBuilt();
		BuildPaletteWidget->InitializeBuildPalette(this, PC);
		BuildPaletteWidget->SetIsFocusable(false);
		BuildPaletteWidget->SetIsEnabled(true);
		BuildPaletteWidget->SetRenderOpacity(1.f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EnsureWidget: failed to create UIH_BuildPaletteHostWidget"));
	}
}
void UIH_BuildPaletteSubsystem::EnsureWidgetInViewport(AIH_Cube2FlyPlayerController* PC)
{
	EnsureWidget(PC);
	if (!BuildPaletteWidget || !PC)
	{
		return;
	}
	static constexpr int32 BuildPaletteViewportZOrder = 56;
	if (!BuildPaletteWidget->IsInViewport())
	{
		BuildPaletteWidget->AddToViewport(BuildPaletteViewportZOrder);
		// Fullscreen overlay; panel content is positioned on the internal canvas (matches legacy tab-strip paint overlay).
		BuildPaletteWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
		BuildPaletteWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
		BuildPaletteWidget->SetPositionInViewport(FVector2D::ZeroVector);
		BuildPaletteWidget->RequestLayoutRefresh();
	}
}
void UIH_BuildPaletteSubsystem::PrepareBuildPaletteWidget(AIH_Cube2FlyPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const UWorld* World = PC->GetWorld();
	const bool bShowTabStrip = World
		&& (World->WorldType == EWorldType::PIE
			|| World->WorldType == EWorldType::Editor
			|| World->WorldType == EWorldType::Game);
	bTabStripVisible = bShowTabStrip;
	bFlyOutOpen = false;
	ActiveTab = EIHBuildPaletteTab::Grid;
	EnsureWidgetInViewport(PC);
	if (!BuildPaletteWidget)
	{
		return;
	}
	BuildPaletteWidget->SetTabStripVisible(bShowTabStrip);
	BuildPaletteWidget->SetVisibility(bShowTabStrip ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SyncWidgetFlyOutState();
	BuildPaletteWidget->EnsureWidgetTreeBuilt();
	BuildPaletteWidget->RequestLayoutRefresh();
	if (UWorld* LayoutWorld = PC->GetWorld())
	{
		LayoutWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
			BuildPaletteWidget,
			[Widget = BuildPaletteWidget]()
			{
				if (Widget)
				{
					Widget->RequestLayoutRefresh();
					Widget->LogLayoutDiagnostics(TEXT("PrepareBuildPaletteNextTick"));
				}
			}));
	}
	UE_LOG(
		LogTemp, Warning,
		TEXT("PrepareBuildPaletteWidget: tabStrip=%d hostInViewport=%d worldType=%d"),
		bShowTabStrip ? 1 : 0,
		BuildPaletteWidget->IsInViewport() ? 1 : 0,
		World ? static_cast<int32>(World->WorldType) : -1);
}
bool UIH_BuildPaletteSubsystem::IsTabStripVisible() const
{
	return bTabStripVisible && BuildPaletteWidget != nullptr;
}
void UIH_BuildPaletteSubsystem::EnsureBuildPaletteReady(AIH_Cube2FlyPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	PaletteOwnerPC = PC;
	const UWorld* World = PC->GetWorld();
	const bool bShowTabStrip = World
		&& (World->WorldType == EWorldType::PIE
			|| World->WorldType == EWorldType::Editor
			|| World->WorldType == EWorldType::Game);
	if (!bShowTabStrip)
	{
		return;
	}
	bTabStripVisible = true;
	EnsureWidgetInViewport(PC);
	if (!BuildPaletteWidget)
	{
		return;
	}

	BuildPaletteWidget->SetTabStripVisible(true);
	BuildPaletteWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	BuildPaletteWidget->EnsureWidgetTreeBuilt();

	if (!bPaletteHostViewportReady)
	{
		SyncWidgetFlyOutState();
		bPaletteHostViewportReady = true;
	}
	else
	{
		SyncWidgetFlyOutStateIfChanged();
	}
}
bool UIH_BuildPaletteSubsystem::IsTabImplemented(EIHBuildPaletteTab Tab)
{
	switch (Tab)
	{
	case EIHBuildPaletteTab::Grid:
	case EIHBuildPaletteTab::Build:
		return true;
	default:
		return false;
	}
}
void UIH_BuildPaletteSubsystem::LogFirstOpenIfNeeded()
{
	if (bLoggedFirstOpen)
	{
		return;
	}
	bLoggedFirstOpen = true;
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("M2 Build Palette — %d grid template rows loaded"),
		CachedGridTemplateRowCount);
}
void UIH_BuildPaletteSubsystem::SyncWidgetFlyOutState()
{
	const bool bWasWorldOpen = bLastSyncedFlyOutOpen && LastSyncedFlyOutTab == EIHBuildPaletteTab::World;
	const bool bNowWorldOpen = bFlyOutOpen && ActiveTab == EIHBuildPaletteTab::World;
	if (bWasWorldOpen != bNowWorldOpen)
	{
		RefreshAllTerrainStampsPassiveTint(bNowWorldOpen);
	}

	if (!BuildPaletteWidget)
	{
		return;
	}
	if (bFlyOutOpen)
	{
		BuildPaletteWidget->SetActiveFlyOutTab(ActiveTab);
	}
	else
	{
		BuildPaletteWidget->SetActiveFlyOutTab(TOptional<EIHBuildPaletteTab>());
	}
	bLastSyncedFlyOutOpen = bFlyOutOpen;
	LastSyncedFlyOutTab = ActiveTab;
	BuildPaletteWidget->RequestLayoutRefresh();
}

void UIH_BuildPaletteSubsystem::SyncWidgetFlyOutStateIfChanged()
{
	if (!BuildPaletteWidget)
	{
		return;
	}

	const bool bTabChanged = bFlyOutOpen != bLastSyncedFlyOutOpen
		|| (bFlyOutOpen && ActiveTab != LastSyncedFlyOutTab);
	if (!bTabChanged)
	{
		return;
	}

	const bool bWasWorldOpen = bLastSyncedFlyOutOpen && LastSyncedFlyOutTab == EIHBuildPaletteTab::World;
	const bool bNowWorldOpen = bFlyOutOpen && ActiveTab == EIHBuildPaletteTab::World;
	if (bWasWorldOpen != bNowWorldOpen)
	{
		RefreshAllTerrainStampsPassiveTint(bNowWorldOpen);
	}

	if (bFlyOutOpen)
	{
		BuildPaletteWidget->SetActiveFlyOutTab(ActiveTab);
	}
	else
	{
		BuildPaletteWidget->SetActiveFlyOutTab(TOptional<EIHBuildPaletteTab>());
	}
	bLastSyncedFlyOutOpen = bFlyOutOpen;
	LastSyncedFlyOutTab = ActiveTab;
	BuildPaletteWidget->RequestLayoutRefresh();
}
void UIH_BuildPaletteSubsystem::SetGridFlyOutOpen(bool bOpen)
{
	if (bOpen)
	{
		ActiveTab = EIHBuildPaletteTab::Grid;
	}
	else if (ActiveTab == EIHBuildPaletteTab::Grid)
	{
		bFlyOutOpen = false;
		SyncWidgetFlyOutState();
		return;
	}
	bFlyOutOpen = bOpen;
	SyncWidgetFlyOutState();
}
void UIH_BuildPaletteSubsystem::CloseFlyOut()
{
	CancelDrag();
	ClearTerrainStampSelection();
	bFlyOutOpen = false;
	ClearSelectionsOutsideActiveScope(nullptr);
	SyncWidgetFlyOutState();
	if (BuildPaletteWidget)
	{
		BuildPaletteWidget->RequestLayoutRefresh();
	}
}

void UIH_BuildPaletteSubsystem::ClearSelectionsOutsideActiveScope(AIH_Cube2FlyPlayerController* PC)
{
	// 2026-09-13: selectable-actor-hierarchy - centralizes what used to be two special-cased
	// deselect calls inline in OpenTabFlyOut (Build->TownGrid, World->Island). Now every category
	// not selectable under the CURRENT tab state gets force-deselected uniformly, from both
	// OpenTabFlyOut and CloseFlyOut, not just on the next stray click.
	if (!PC)
	{
		if (UWorld* World = GetWorld())
		{
			PC = Cast<AIH_Cube2FlyPlayerController>(World->GetFirstPlayerController());
		}
	}
	if (!PC)
	{
		return;
	}

	if (!IsCategorySelectableNow(EIHBuildPaletteTab::Grid))
	{
		PC->DeselectTownGridManager();
	}
	if (!IsCategorySelectableNow(EIHBuildPaletteTab::Build))
	{
		PC->DeselectStructurePlacement();
	}
	if (!IsCategorySelectableNow(EIHBuildPaletteTab::World))
	{
		ClearTerrainStampSelection();
	}
	if (!IsCategorySelectableNow(EIHBuildPaletteTab::Convey))
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UIH_P1C07_ShipRegistrySubsystem* ShipRegistry = GI->GetSubsystem<UIH_P1C07_ShipRegistrySubsystem>())
			{
				ShipRegistry->ClearSelection();
			}
		}
	}
	// Mannequin deliberately excluded - stays outside the GWBCD hierarchy, always selectable.

	if (bFlyOutOpen)
	{
		// Island is the "nothing open" default - any tab opening blocks it immediately. Closing
		// back down to "nothing open" makes it selectable again, not something to deselect.
		PC->RequestDeselectIsland();
	}
}
void UIH_BuildPaletteSubsystem::OpenTabFlyOut(EIHBuildPaletteTab Tab, AIH_Cube2FlyPlayerController* PC)
{
	EnsureBuildPaletteReady(PC);
	if (!bTabStripVisible)
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("OpenTabFlyOut: aborted — tab strip not visible (tab=%d)"),
			static_cast<int32>(Tab));
		return;
	}
	EnsureWidgetInViewport(PC);
	if (!BuildPaletteWidget || !PC)
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("OpenTabFlyOut: aborted — host=%s PC=%s"),
			BuildPaletteWidget ? *BuildPaletteWidget->GetName() : TEXT("null"),
			PC ? *PC->GetName() : TEXT("null"));
		return;
	}
	RefreshCachedGridRowCounts();
	RefreshCachedBuildRowCounts();
	if (Tab == EIHBuildPaletteTab::Grid)
	{
		BuildPaletteWidget->RefreshGridTemplateList();
	}
	else if (Tab == EIHBuildPaletteTab::Build)
	{
		BuildPaletteWidget->RefreshBuildTemplateList();
	}
	else if (Tab == EIHBuildPaletteTab::Convey)
	{
		BuildPaletteWidget->RefreshConveyTemplateList();
	}
	ActiveTab = Tab;
	bFlyOutOpen = true;
	if (Tab == EIHBuildPaletteTab::World)
	{
		PC->ResetIslandViewportDoubleClickTracking();
	}
	ClearSelectionsOutsideActiveScope(PC);
	BuildPaletteWidget->SetIsEnabled(true);
	BuildPaletteWidget->SetRenderOpacity(1.f);
	SyncWidgetFlyOutState();
	BuildPaletteWidget->LogLayoutDiagnostics(TEXT("OpenTabFlyOut"));
	UE_LOG(
		LogTemp, Warning,
		TEXT("OpenTabFlyOut: OPEN tab=%d hostInViewport=%d flyOutTabSet=%d"),
		static_cast<int32>(Tab),
		BuildPaletteWidget->IsInViewport() ? 1 : 0,
		BuildPaletteWidget->GetVisibility() != ESlateVisibility::Collapsed ? 1 : 0);
	LogFirstOpenIfNeeded();
	if (UWorld* World = PC->GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
			BuildPaletteWidget,
			[Widget = BuildPaletteWidget]()
			{
				if (Widget)
				{
					Widget->RequestLayoutRefresh();
					Widget->LogLayoutDiagnostics(TEXT("OpenTabFlyOutNextTick"));
				}
			}));
	}
}
bool UIH_BuildPaletteSubsystem::IsViewportIslandSelectionBlocked() const
{
	// 2026-09-13: selectable-actor-hierarchy canon — Island is the "nothing open" default, so ANY
	// open fly-out blocks it now, not just World's (widened from the original FIX-001d W-only gate).
	if (!IsIslandSelectableNow())
	{
		return true;
	}

	// Widget paint state is authoritative — catches subsystem/widget desync.
	if (BuildPaletteWidget && BuildPaletteWidget->IsWorldFlyOutVisible())
	{
		return true;
	}

	return false;
}

bool UIH_BuildPaletteSubsystem::IsWorldStampEditModeActive() const
{
	// Terrain Stamp selection stays scoped to W specifically (or an active stamp drag) even though
	// Island's own block now covers every tab — these are deliberately independent questions.
	if (IsTerrainStampDragActive())
	{
		return true;
	}
	return IsCategorySelectableNow(EIHBuildPaletteTab::World);
}
void UIH_BuildPaletteSubsystem::OpenGridPanel(AIH_Cube2FlyPlayerController* PC)
{
	OpenTabFlyOut(EIHBuildPaletteTab::Grid, PC);
}
void UIH_BuildPaletteSubsystem::CloseGridPanel()
{
	CloseFlyOut();
}
void UIH_BuildPaletteSubsystem::ToggleGridPanel(AIH_Cube2FlyPlayerController* PC)
{
	ToggleTabFlyOut(EIHBuildPaletteTab::Grid, PC);
}
void UIH_BuildPaletteSubsystem::ToggleTabFlyOut(EIHBuildPaletteTab Tab, AIH_Cube2FlyPlayerController* PC)
{
	EnsureBuildPaletteReady(PC);
	if (!bTabStripVisible)
	{
		UE_LOG(LogTemp, Warning, TEXT("ToggleTabFlyOut: aborted — tab strip disabled (tab=%d)"), static_cast<int32>(Tab));
		return;
	}
	if (bFlyOutOpen && ActiveTab == Tab)
	{
		CloseFlyOut();
		return;
	}
	OpenTabFlyOut(Tab, PC);
}
void UIH_BuildPaletteSubsystem::ToggleGridFlyOut(AIH_Cube2FlyPlayerController* PC)
{
	ToggleTabFlyOut(EIHBuildPaletteTab::Grid, PC);
}
bool UIH_BuildPaletteSubsystem::TryFindPaletteItem(FName ItemID, FIHBuildPaletteItemRow& OutRow) const
{
	if (ItemID.IsNone())
	{
		return false;
	}
	if (const UIHTownGridDataSubsystem* Data = GetBuildPaletteDataSubsystem())
	{
		if (const UDataTable* ItemTable = Data->GetBuildPaletteItemTable())
		{
			if (const FIHBuildPaletteItemRow* Row = ItemTable->FindRow<FIHBuildPaletteItemRow>(
				ItemID, TEXT("BuildPaletteSubsystem::TryFindPaletteItem")))
			{
				OutRow = *Row;
				return true;
			}
		}
	}
	return false;
}
bool UIH_BuildPaletteSubsystem::BeginDragFromItem(FName ItemID, AIH_Cube2FlyPlayerController* PC)
{
	FIHBuildPaletteItemRow Row;
	if (!TryFindPaletteItem(ItemID, Row))
	{
		return false;
	}

	const bool bGridGrip = Row.paletteTab == EIHBuildPaletteTab::Grid
		&& Row.interactionType == EIHBuildPaletteInteraction::GripTemplate;
	const bool bBuildDrop = Row.paletteTab == EIHBuildPaletteTab::Build
		&& Row.interactionType == EIHBuildPaletteInteraction::DropActor;
	if (!bGridGrip && !bBuildDrop)
	{
		return false;
	}

	AIH_Cube2FlyPlayerController* FlyPC = PC ? PC : PaletteOwnerPC.Get();
	if (FlyPC)
	{
		PaletteOwnerPC = FlyPC;
	}

	ActiveTab = Row.paletteTab;
	DragPayload = Row;
	bDragActive = true;
	bDragGhostLocationValid = false;
	bLoggedBuildDragGhostValid = false;
	DragGhostDrawCenterWorld = FVector::ZeroVector;
	DragPlacementActorOrigin = FVector::ZeroVector;
	StickyBuildDragDrawCenterWorld = FVector::ZeroVector;
	StickyBuildDragActorOrigin = FVector::ZeroVector;
	if (bBuildDrop && FlyPC)
	{
		EnsureBuildDragPreview(FlyPC, ItemID);
		FVector2D ViewportCur = FVector2D::ZeroVector;
		if (FlyPC->TryGetViewportMousePosition(ViewportCur))
		{
			UpdateDragGhostFromScreen(FlyPC, ViewportCur);
		}
	}
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("BuildPalette drag started — item=%s tab=%d interaction=%d"),
		*ItemID.ToString(),
		static_cast<int32>(Row.paletteTab),
		static_cast<int32>(Row.interactionType));
	return true;
}

bool UIH_BuildPaletteSubsystem::BeginDragForMerchantmanTile(AIH_Cube2FlyPlayerController* PC)
{
	AIH_Cube2FlyPlayerController* FlyPC = PC ? PC : PaletteOwnerPC.Get();
	if (!FlyPC)
	{
		return false;
	}
	PaletteOwnerPC = FlyPC;

	FIHBuildPaletteItemRow Row;
	Row.itemID = FName(TEXT("Merchantman"));
	Row.paletteTab = EIHBuildPaletteTab::Convey;
	Row.interactionType = EIHBuildPaletteInteraction::DropActor;
	Row.displayName = TEXT("Merchantman");

	ActiveTab = Row.paletteTab;
	DragPayload = Row;
	bDragActive = true;
	bDragGhostLocationValid = false;
	DragGhostDrawCenterWorld = FVector::ZeroVector;
	DragPlacementActorOrigin = FVector::ZeroVector;
	// No ghost preview - dev tile, resolved/spawned directly on drop (TryCompleteDropAtScreen's
	// Convey branch), unlike Build's structure-mesh ghost which this deliberately skips.

	UE_LOG(LogIH_WB_Demo004, Log, TEXT("BuildPalette drag started — Merchantman (Convey dev tile)"));
	return true;
}

bool UIH_BuildPaletteSubsystem::BeginDragFromTerrainStamp(
	const EIHTerrainStampId StampId,
	AIH_Cube2FlyPlayerController* PC)
{
	if (!IHInvisibleHandSpec::IsCoastB2bWorldStampPaletteEnabled())
	{
		return false;
	}
	if (StampId >= EIHTerrainStampId::MAX)
	{
		return false;
	}

	AIH_Cube2FlyPlayerController* FlyPC = PC ? PC : PaletteOwnerPC.Get();
	if (FlyPC)
	{
		PaletteOwnerPC = FlyPC;
	}

	ActiveTab = EIHBuildPaletteTab::World;
	if (!bFlyOutOpen)
	{
		bFlyOutOpen = true;
		SyncWidgetFlyOutState();
	}
	DragPayload = MakeSyntheticTerrainStampPaletteRow(StampId);
	bDragActive = true;
	bDragGhostLocationValid = false;
	bLoggedBuildDragGhostValid = false;
	bLoggedTerrainStampDragGhostValid = false;
	DragGhostDrawCenterWorld = FVector::ZeroVector;
	DragPlacementActorOrigin = FVector::ZeroVector;
	StickyBuildDragDrawCenterWorld = FVector::ZeroVector;
	StickyBuildDragActorOrigin = FVector::ZeroVector;
	StickyStampTargetIsland = nullptr;

	if (FlyPC)
	{
		EnsureTerrainStampDragPreview(FlyPC, StampId);
		FVector2D ViewportCur = FVector2D::ZeroVector;
		if (FlyPC->TryGetViewportMousePosition(ViewportCur))
		{
			UpdateDragGhostFromScreen(FlyPC, ViewportCur);
		}
	}

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase B2b stamp drag started — stamp=%s"),
		*FIHTerrainStampCatalog::Get(StampId).RowName.ToString());
	return true;
}

void UIH_BuildPaletteSubsystem::CancelDrag()
{
	if (!bDragActive)
	{
		return;
	}
	DestroyBuildDragPreview();
	DestroyTerrainStampDragPreview();
	bDragActive = false;
	bDragGhostLocationValid = false;
	bLoggedBuildDragGhostValid = false;
	bLoggedTerrainStampDragGhostValid = false;
	DragGhostDrawCenterWorld = FVector::ZeroVector;
	DragPlacementActorOrigin = FVector::ZeroVector;
	StickyBuildDragDrawCenterWorld = FVector::ZeroVector;
	StickyBuildDragActorOrigin = FVector::ZeroVector;
	StickyStampTargetIsland = nullptr;
	DragPayload = FIHBuildPaletteItemRow();
}

void UIH_BuildPaletteSubsystem::CommitStickyTerrainStampPlacement(
	const FVector& SurfaceWorld,
	AIH_WB_IslandActor* TargetIsland)
{
	DragGhostDrawCenterWorld = SurfaceWorld;
	StickyBuildDragDrawCenterWorld = SurfaceWorld;
	StickyStampTargetIsland = TargetIsland;
	bDragGhostLocationValid = true;
	UpdateTerrainStampDragPreviewTransform();
	if (!bLoggedTerrainStampDragGhostValid)
	{
		bLoggedTerrainStampDragGhostValid = true;
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase B2b stamp ghost VALID — surface=%s island=%d"),
			*SurfaceWorld.ToString(),
			TargetIsland ? TargetIsland->GetTankIslandIndex() : INDEX_NONE);
	}
}

bool UIH_BuildPaletteSubsystem::CommitActiveTerrainStampDrop(
	AIH_Cube2FlyPlayerController* FlyPC,
	AIH_WB_IslandActor* Island,
	const FVector& SurfaceWorld,
	const EIHTerrainStampId StampId)
{
	// 2026-09-09: the procedural height-grid path this gate used to require
	// (Island->HasCellHeightGrid()) is dead code - HasCellHeightGrid() always returns false, so
	// every stamp drop already silently failed. Real static-mesh stamps (FIHTerrainStampMeshCatalog/
	// DT_TerrainStamp) replace it - a stamp can be dropped once its DataTable row has a real mesh.
	if (!Island || StampId >= EIHTerrainStampId::MAX || !FIHTerrainStampMeshCatalog::IsAvailable(StampId))
	{
		return false;
	}

	const int32 ExistingStampCount = Island->GetPlacedTerrainStamps().Num();
	if (ExistingStampCount >= IHInvisibleHandSpec::TerrainStampMeshHardStopCountPerIsland)
	{
		UE_LOG(LogIH_WB_Demo004, Warning,
			TEXT("Terrain Stamp drop blocked: island=%d already has %d placed stamps (hard stop %d)"),
			Island->GetTankIslandIndex(), ExistingStampCount, IHInvisibleHandSpec::TerrainStampMeshHardStopCountPerIsland);
		return false;
	}
	if (ExistingStampCount >= IHInvisibleHandSpec::TerrainStampMeshWarnCountPerIsland)
	{
		UE_LOG(LogIH_WB_Demo004, Warning,
			TEXT("Terrain Stamp count warning: island=%d has %d placed stamps (soft warn threshold %d, hard stop %d)"),
			Island->GetTankIslandIndex(), ExistingStampCount, IHInvisibleHandSpec::TerrainStampMeshWarnCountPerIsland, IHInvisibleHandSpec::TerrainStampMeshHardStopCountPerIsland);
	}

	UWorld* World = FlyPC ? FlyPC->GetWorld() : Island->GetWorld();
	if (!World)
	{
		return false;
	}

	AIH_TerrainStampActor* PlacedStamp = SpawnBareStampActor(World, Island, StampId);
	if (!PlacedStamp)
	{
		return false;
	}

	PlacedStamp->ApplyWorldSurfacePlacement(Island, SurfaceWorld);
	Island->ReapplyAllTerrainStampsToHeightGrid();
	Island->SyncPlacedTerrainStampSurfaceAnchors();
	SelectTerrainStamp(PlacedStamp);
	LogTerrainStampReplayHeaderStub();

#if !UE_BUILD_SHIPPING
	const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(StampId);
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase B2b stamp placed island=%d stamp=%s stamps=%d previewVerts=%d surface=%s"),
		Island->GetTankIslandIndex(),
		*Def.RowName.ToString(),
		Island->GetPlacedTerrainStamps().Num(),
		PlacedStamp->GetPreviewMeshVertexCount(),
		*SurfaceWorld.ToString());
#endif
	(void)FlyPC;
	return true;
}

AIH_TerrainStampActor* UIH_BuildPaletteSubsystem::SpawnBareStampActor(
	UWorld* World, AIH_WB_IslandActor* Island, EIHTerrainStampId StampId)
{
	if (!World || !Island)
	{
		return nullptr;
	}

	const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(StampId);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Island;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AIH_TerrainStampActor* NewStamp = World->SpawnActor<AIH_TerrainStampActor>(
		AIH_TerrainStampActor::StaticClass(),
		FVector::ZeroVector,
		FRotator(0.f, 0.f, 0.f),
		SpawnParams);
	if (!NewStamp)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Phase B2b stamp spawn failed island=%d stamp=%s"),
			Island->GetTankIslandIndex(), *Def.RowName.ToString());
		return nullptr;
	}

	NewStamp->InitializeStamp(StampId, Def.bDefaultInvert, false);
	NewStamp->SetDragPreviewMode(false);
	NewStamp->RegisterAllComponents();
	NewStamp->RefreshPreviewMesh();
	NewStamp->SetActorHiddenInGame(false);
	NewStamp->SetActorEnableCollision(true);
	Island->RegisterTerrainStamp(NewStamp);
	// 2026-09-11: a stamp can only be spawned while the W tab is open, but that's a pre-existing
	// state, not a fresh open/close transition - RefreshAllTerrainStampsPassiveTint never runs for
	// it. Without this, a stamp placed then deselected (panel still open) would wrongly fall back
	// to its real material instead of AllStampsToggleColor per the Task 1/2 spec.
	NewStamp->SetPassiveToggleTinted(bFlyOutOpen && ActiveTab == EIHBuildPaletteTab::World);

#if WITH_EDITOR
	// 2026-09-10: Outliner previously showed the generic class name ("IH_TerrainStampActor1") -
	// matches the Ship/Mannequin placement convention (SetActorLabel), using the actual mesh name
	// so e.g. a placed Mesa reads "TableMesa001" (UE auto-dedupes additional copies with a suffix).
	if (const UStaticMeshComponent* MeshComp = NewStamp->GetMeshComponent())
	{
		if (const UStaticMesh* Mesh = MeshComp->GetStaticMesh())
		{
			NewStamp->SetActorLabel(Mesh->GetName());
		}
	}
#endif

	return NewStamp;
}

bool UIH_BuildPaletteSubsystem::TryCommitTerrainStampDropAtStoredPlacement(APlayerController* PC)
{
	if (!bDragActive || !PC || !bDragGhostLocationValid || !IsTerrainStampDragActive())
	{
		return false;
	}

	EIHTerrainStampId StampId = EIHTerrainStampId::Hill;
	if (!TryGetTerrainStampIdFromRow(DragPayload, StampId))
	{
		return false;
	}

	AIH_WB_IslandActor* Island = StickyStampTargetIsland.Get();
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!Island || !FlyPC)
	{
		return false;
	}

	const bool bCommitted = CommitActiveTerrainStampDrop(
		FlyPC, Island, StickyBuildDragDrawCenterWorld, StampId);
	if (bCommitted)
	{
		DestroyTerrainStampDragPreview();
		bDragActive = false;
		bDragGhostLocationValid = false;
		StickyStampTargetIsland = nullptr;
		DragPayload = FIHBuildPaletteItemRow();
	}
	return bCommitted;
}

void UIH_BuildPaletteSubsystem::CommitStickyBuildDragPlacement(
	const FVector& DrawCenterWorld,
	const FVector& ActorOriginWorld)
{
	DragGhostDrawCenterWorld = DrawCenterWorld;
	DragPlacementActorOrigin = ActorOriginWorld;
	StickyBuildDragDrawCenterWorld = DrawCenterWorld;
	StickyBuildDragActorOrigin = ActorOriginWorld;
	bDragGhostLocationValid = true;
}

void UIH_BuildPaletteSubsystem::EnsureBuildDragPreview(AIH_Cube2FlyPlayerController* PC, FName ItemID)
{
	if (!PC)
	{
		return;
	}

	UWorld* World = PC->GetWorld();
	if (!World)
	{
		return;
	}

	DestroyBuildDragPreview();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PC;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Keep preview in world during drag; CommitActiveStructureDrop clears RF_Transient on place.
	// Native class for drag preview — avoids BP spawn/load issues during transient ghost.
	AIH_StructurePlacementActor* Preview = World->SpawnActor<AIH_StructurePlacementActor>(
		AIH_StructurePlacementActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!Preview)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("BuildPalette drag preview: SpawnActor failed for %s"), *ItemID.ToString());
		return;
	}

	Preview->SetPlacementPaletteItem(ItemID);
	Preview->EnsureDevFootprintCubeMesh(ItemID);
	Preview->SetBuildDragPreviewMode(true);
	Preview->ApplyPlacementVisualStyle(true);
	Preview->SetActorHiddenInGame(false);
	Preview->MarkComponentsRenderStateDirty();
	BuildDragPreviewActor = Preview;

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("BuildPalette drag preview — item=%s mesh=%s"),
		*ItemID.ToString(),
		Preview->HasPlacementMesh() ? TEXT("OK") : TEXT("NONE"));
}

void UIH_BuildPaletteSubsystem::DestroyBuildDragPreview()
{
	if (AIH_StructurePlacementActor* Preview = BuildDragPreviewActor.Get())
	{
		Preview->Destroy();
	}
	BuildDragPreviewActor = nullptr;
}

void UIH_BuildPaletteSubsystem::UpdateBuildDragPreviewTransform()
{
	if (!BuildDragPreviewActor || !bDragGhostLocationValid)
	{
		return;
	}

	if (AIH_StructurePlacementActor* Preview = BuildDragPreviewActor.Get())
	{
		const FVector PreviewOrigin = StickyBuildDragActorOrigin;
		Preview->EnsureDevFootprintCubeMesh(DragPayload.itemID);
		Preview->SetActorLocation(PreviewOrigin);
		Preview->SetActorRotation(FRotator::ZeroRotator);
		Preview->SetBuildDragPreviewMode(true);
		Preview->SetActorHiddenInGame(false);
		Preview->SetActorEnableCollision(false);
		Preview->ApplyPlacementVisualStyle(true);
		Preview->MarkComponentsRenderStateDirty();
	}
}

AActor* UIH_BuildPaletteSubsystem::GetDragPreviewIgnoreActor() const
{
	if (TerrainStampDragPreviewActor)
	{
		return TerrainStampDragPreviewActor.Get();
	}
	return BuildDragPreviewActor.Get();
}

void UIH_BuildPaletteSubsystem::EnsureTerrainStampDragPreview(
	AIH_Cube2FlyPlayerController* PC,
	const EIHTerrainStampId StampId)
{
	if (!PC || StampId >= EIHTerrainStampId::MAX)
	{
		return;
	}

	UWorld* World = PC->GetWorld();
	if (!World)
	{
		return;
	}

	DestroyTerrainStampDragPreview();

	const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(StampId);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PC;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AIH_TerrainStampActor* Preview = World->SpawnActor<AIH_TerrainStampActor>(
		AIH_TerrainStampActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!Preview)
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("Phase B2b stamp drag preview spawn failed stamp=%s"),
			*Def.RowName.ToString());
		return;
	}

	Preview->InitializeStamp(StampId, Def.bDefaultInvert, false);
	Preview->SetDragPreviewMode(true);
	Preview->SetActorHiddenInGame(false);
	Preview->SetActorEnableCollision(false);
	Preview->RefreshPreviewMesh();
	TerrainStampDragPreviewActor = Preview;

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase B2b stamp drag preview — stamp=%s"),
		*Def.RowName.ToString());
}

void UIH_BuildPaletteSubsystem::DestroyTerrainStampDragPreview()
{
	if (AIH_TerrainStampActor* Preview = TerrainStampDragPreviewActor.Get())
	{
		Preview->Destroy();
	}
	TerrainStampDragPreviewActor = nullptr;
}

void UIH_BuildPaletteSubsystem::UpdateTerrainStampDragPreviewTransform()
{
	if (!TerrainStampDragPreviewActor)
	{
		return;
	}

	if (AIH_TerrainStampActor* Preview = TerrainStampDragPreviewActor.Get())
	{
		Preview->SetTargetIsland(StickyStampTargetIsland.Get());
		if (bDragGhostLocationValid)
		{
			Preview->ApplyWorldSurfacePlacement(StickyStampTargetIsland.Get(), StickyBuildDragDrawCenterWorld);
		}
		Preview->SetDragPreviewMode(true);
		Preview->SetActorHiddenInGame(false);
		Preview->SetActorEnableCollision(false);
		Preview->MarkComponentsRenderStateDirty();
	}
}
void UIH_BuildPaletteSubsystem::UpdateDragGhostFromWorldXY(APlayerController* PC, const FVector2D& WorldXY)
{
	if (!bDragActive || !PC)
	{
		return;
	}

	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!FlyPC)
	{
		return;
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Build)
	{
		FVector NewDrawCenter = DragGhostDrawCenterWorld;
		FVector NewActorOrigin = DragPlacementActorOrigin;
		if (IH_BuildPaletteSubsystemPrivate::TryResolveValidBuildDragAtWorldXY(
			FlyPC,
			DragPayload.itemID,
			WorldXY,
			BuildDragPreviewActor.Get(),
			NewDrawCenter,
			NewActorOrigin))
		{
			CommitStickyBuildDragPlacement(NewDrawCenter, NewActorOrigin);
		}
		UpdateBuildDragPreviewTransform();
		return;
	}

	if (IsTerrainStampDragActive())
	{
		FVector SurfaceWorld = FVector::ZeroVector;
		AIH_WB_IslandActor* Island = nullptr;
		if (IH_BuildPaletteSubsystemPrivate::TryResolveTerrainStampDragAtWorldXY(
			FlyPC, WorldXY, SurfaceWorld, Island))
		{
			CommitStickyTerrainStampPlacement(SurfaceWorld, Island);
		}
		UpdateTerrainStampDragPreviewTransform();
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (IH_BuildPaletteSubsystemPrivate::TryResolveGridDragLocationFromWorldXY(FlyPC, WorldXY, SpawnLocation))
	{
		DragGhostDrawCenterWorld = SpawnLocation;
		bDragGhostLocationValid = true;
	}
}

void UIH_BuildPaletteSubsystem::UpdateDragGhostFromScreen(APlayerController* PC, const FVector2D& ScreenPos)
{
	if (!bDragActive || !PC)
	{
		return;
	}
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!FlyPC)
	{
		return;
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Build)
	{
		// 2026-09-18 fix: "Structure drag is far away from mouse indicator" - the world-space raycast
		// TryResolveValidBuildDragAtScreen uses has no concept of 2D HUD panels drawn over the
		// viewport. Since a Build-tab drag STARTS by clicking a tile inside the palette panel itself,
		// the cursor's early movement is very likely to still be over that same panel - the raycast
		// would then resolve against whatever real-world geometry happens to sit behind it (often
		// distant terrain/ocean, since the palette sits at the screen edge), snapping the "sticky"
		// ghost to a spurious point far from where the user is actually aiming. Hold the ghost at its
		// last valid position instead while the cursor is over any interactive panel, exactly like the
		// existing sticky-on-invalid-point behavior for other unresolvable cursor positions.
		if (FlyPC->IsScreenPointOverInteractiveHUDPanel(ScreenPos))
		{
			UpdateBuildDragPreviewTransform();
			return;
		}

		FVector NewDrawCenter = DragGhostDrawCenterWorld;
		FVector NewActorOrigin = DragPlacementActorOrigin;
		if (IH_BuildPaletteSubsystemPrivate::TryResolveValidBuildDragAtScreen(
			FlyPC,
			DragPayload.itemID,
			ScreenPos,
			BuildDragPreviewActor.Get(),
			NewDrawCenter,
			NewActorOrigin))
		{
			CommitStickyBuildDragPlacement(NewDrawCenter, NewActorOrigin);
			if (!bLoggedBuildDragGhostValid)
			{
				bLoggedBuildDragGhostValid = true;
				UE_LOG(
					LogIH_WB_Demo004, Log,
					TEXT("BuildPalette drag ghost VALID — drawCenter=%s actorOrigin=%s"),
					*DragGhostDrawCenterWorld.ToString(),
					*DragPlacementActorOrigin.ToString());
			}
		}
		UpdateBuildDragPreviewTransform();
		return;
	}

	if (IsTerrainStampDragActive())
	{
		FVector SurfaceWorld = FVector::ZeroVector;
		AIH_WB_IslandActor* Island = nullptr;
		if (IH_BuildPaletteSubsystemPrivate::TryResolveTerrainStampDragAtScreen(
			FlyPC, ScreenPos, SurfaceWorld, Island))
		{
			CommitStickyTerrainStampPlacement(SurfaceWorld, Island);
		}
		UpdateTerrainStampDragPreviewTransform();
		return;
	}

	FVector ImpactPoint = FVector::ZeroVector;
	if (FlyPC->TryTraceTerrainAtScreen(ScreenPos, ImpactPoint))
	{
		DragGhostDrawCenterWorld = ImpactPoint + FVector(0.f, 0.f, 50.f);
		bDragGhostLocationValid = true;
	}
	else
	{
		bDragGhostLocationValid = false;
	}
}
void UIH_BuildPaletteSubsystem::DrawDragGhost(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC) const
{
	if (!bDragActive || !World)
	{
		return;
	}

	if (!FlyPC)
	{
		FlyPC = Cast<AIH_Cube2FlyPlayerController>(World->GetFirstPlayerController());
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Build)
	{
		if (!bDragGhostLocationValid)
		{
			return;
		}

		const FVector DrawCenterWorld = StickyBuildDragDrawCenterWorld;
		const FVector ActorOriginWorld = StickyBuildDragActorOrigin;

		FVector FootprintCm = FVector(800.f, 600.f, 400.f);
		TryGetStructureFootprintCm(DragPayload.itemID, FootprintCm);
		AIH_StructurePlacementActor* PreviewActor = BuildDragPreviewActor.Get();
		if (PreviewActor)
		{
			PreviewActor->EnsureDevFootprintCubeMesh(DragPayload.itemID);
			PreviewActor->SetActorLocation(ActorOriginWorld);
			PreviewActor->ApplyPlacementVisualStyle(true);
		}

		// Visible 2×2 module preview (matches G Grid — known good in PIE).
		FIHTownGridGeneratorParams Params;
		Params.CenterWorldCm = DrawCenterWorld;
		Params.BboxHalfExtentCm = FVector2D(
			FMath::Max(400.f, FootprintCm.X * 0.5f),
			FMath::Max(300.f, FootprintCm.Y * 0.5f));
		Params.YawDeg = 0.f;
		Params.ModuleSizeCm = AIH_TownGridManager::ModuleSizeCm;
		Params.CollectorIntervalModules = AIH_TownGridManager::DefaultCollectorIntervalModules;
		Params.CommonsModules = 4;
		Params.CommonsZonePrimary = EIHParcelZoneCode::CIV;
		Params.CommonsZoneSecondary = EIHParcelZoneCode::SPD;

		FTownGridOverlayData PreviewOverlay;
		IH_TownGridSquaredGenerator::GenerateSquared(Params, PreviewOverlay);
		UTownGridOverlayComponent::DrawOverlayData(
			World,
			PreviewOverlay,
			DrawCenterWorld,
			0.f,
			25.f,
			true,
			PreviewActor,
			true);

		// Blue solid/wire cube at terrain draw center (always visible; not tied to mesh bounds).
		IH_BuildPaletteSubsystemPrivate::DrawBuildStructureFootprintOutline(
			World,
			DrawCenterWorld,
			FootprintCm,
			DragPayload.itemID,
			PreviewActor,
			false);
		return;
	}

	if (IsTerrainStampDragActive())
	{
		if (const AIH_TerrainStampActor* Preview = TerrainStampDragPreviewActor.Get())
		{
			const FColor RingColor = bDragGhostLocationValid ? FColor(255, 60, 255) : FColor(255, 80, 80);
			IH_BuildPaletteSubsystemPrivate::DrawStampSelectionRing(World, FlyPC, Preview, RingColor);
		}
		return;
	}

	if (!bDragGhostLocationValid)
	{
		return;
	}

	if (DragPayload.townGridTemplate != EIHTownGridTemplate::Squared)
	{
		return;
	}

	FIHTownGridGeneratorParams Params;
	Params.CenterWorldCm = DragGhostDrawCenterWorld;
	Params.BboxHalfExtentCm = AIH_TownGridManager::GetDefaultBboxHalfExtentCm();
	Params.YawDeg = 0.f;
	Params.ModuleSizeCm = AIH_TownGridManager::ModuleSizeCm;
	Params.CollectorIntervalModules = AIH_TownGridManager::DefaultCollectorIntervalModules;
	Params.CommonsModules = 4;
	Params.CommonsZonePrimary = EIHParcelZoneCode::CIV;
	Params.CommonsZoneSecondary = EIHParcelZoneCode::SPD;

	FTownGridOverlayData PreviewOverlay;
	IH_TownGridSquaredGenerator::GenerateSquared(Params, PreviewOverlay);

	UTownGridOverlayComponent::DrawOverlayData(
		World, PreviewOverlay, DragGhostDrawCenterWorld, 0.f, 25.f, true, nullptr, true);
}

bool UIH_BuildPaletteSubsystem::CommitActiveStructureDrop(
	AIH_Cube2FlyPlayerController* FlyPC,
	UWorld* World,
	const FVector& SpawnLocation)
{
	if (!FlyPC || !World || SpawnLocation.ContainsNaN())
	{
		return false;
	}

	const FName PlacedItemID = DragPayload.itemID;
	const FVector StickyOriginAtDrop = StickyBuildDragActorOrigin;
	const FVector StickyDrawAtDrop = StickyBuildDragDrawCenterWorld;
	auto FinalizePlacedStructure = [this, PlacedItemID, StickyOriginAtDrop](
		AIH_StructurePlacementActor* Structure,
		const FVector& Location)
	{
		if (!Structure)
		{
			return;
		}
		const FVector PlaceLocation = StickyOriginAtDrop.IsNearlyZero() ? Location : StickyOriginAtDrop;
		Structure->SetActorLocation(PlaceLocation);
		Structure->SetBuildDragPreviewMode(false);
		Structure->EnsureDevFootprintCubeMesh(PlacedItemID);
		Structure->SetPlacementPaletteItem(PlacedItemID);
		Structure->ApplyPlacedDevVisualStyle();
		Structure->ClearFlags(RF_Transient);
		Structure->SetOwner(nullptr);
		Structure->SetActorHiddenInGame(false);
		Structure->SetActorEnableCollision(true);
		Structure->MarkComponentsRenderStateDirty();
		PlacedStructureActors.Add(Structure);
	};

	AIH_StructurePlacementActor* Structure = nullptr;
	if (AIH_StructurePlacementActor* Preview = BuildDragPreviewActor.Get())
	{
		BuildDragPreviewActor = nullptr;
		Structure = Preview;
		FinalizePlacedStructure(Structure, SpawnLocation);
	}
	else
	{
		const FRotator SpawnRotation = FRotator::ZeroRotator;
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* SpawnClass =
			IH_BuildPaletteSubsystemPrivate::ResolveStructurePlacementActorClass(DragPayload);
		Structure = World->SpawnActor<AIH_StructurePlacementActor>(
			SpawnClass, SpawnLocation, SpawnRotation, SpawnParams);
		if (Structure)
		{
			FinalizePlacedStructure(Structure, SpawnLocation);
		}
	}

	if (!Structure || !Structure->HasPlacementMesh())
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("BuildPalette drop — structure missing mesh item=%s at %s"),
			*DragPayload.itemID.ToString(),
			*SpawnLocation.ToString());
	}

	if (Structure)
	{
		FVector FootprintCm(800.f, 600.f, 400.f);
		TryGetStructureFootprintCm(PlacedItemID, FootprintCm);
		const FVector BoxCenter = Structure->GetActorLocation() + FVector(0.f, 0.f, FootprintCm.Z * 0.5f);
		const FVector HalfExtent = FootprintCm * 0.5f;
		DrawDebugSolidBox(
			World,
			BoxCenter,
			HalfExtent,
			FColor(40, 160, 255, 200),
			true,
			120.f,
			SDPG_Foreground);

		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("BuildPalette drop — placed item=%s stickyOrigin=%s stickyDraw=%s actorAt=%s fallback=%d"),
			*PlacedItemID.ToString(),
			*StickyOriginAtDrop.ToString(),
			*StickyDrawAtDrop.ToString(),
			*Structure->GetActorLocation().ToString(),
			Structure->UsesFallbackFootprintMesh() ? 1 : 0);
	}

	bDragActive = false;
	bDragGhostLocationValid = false;
	bLoggedBuildDragGhostValid = false;
	DragGhostDrawCenterWorld = FVector::ZeroVector;
	DragPlacementActorOrigin = FVector::ZeroVector;
	StickyBuildDragDrawCenterWorld = FVector::ZeroVector;
	StickyBuildDragActorOrigin = FVector::ZeroVector;
	DragPayload = FIHBuildPaletteItemRow();
	return Structure != nullptr;
}

bool UIH_BuildPaletteSubsystem::TryCommitStructureDropAtStoredPlacement(APlayerController* PC)
{
	if (!bDragActive || !PC || !bDragGhostLocationValid)
	{
		return false;
	}
	if (DragPayload.paletteTab != EIHBuildPaletteTab::Build
		|| DragPayload.interactionType != EIHBuildPaletteInteraction::DropActor)
	{
		return false;
	}

	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	UWorld* World = PC->GetWorld();
	if (!FlyPC || !World)
	{
		return false;
	}

	const FVector StoredOrigin = StickyBuildDragActorOrigin;
	if (StoredOrigin.SizeSquared() < FMath::Square(100.f))
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("BuildPalette drop STORED rejected — origin too close to zero item=%s"),
			*DragPayload.itemID.ToString());
		return false;
	}

	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("BuildPalette drop STORED — item=%s origin=%s drawCenter=%s"),
		*DragPayload.itemID.ToString(),
		*StoredOrigin.ToString(),
		*StickyBuildDragDrawCenterWorld.ToString());

	return CommitActiveStructureDrop(FlyPC, World, StoredOrigin);
}

bool UIH_BuildPaletteSubsystem::TryCompleteDropAtWorldXY(APlayerController* PC, const FVector2D& WorldXY)
{
	if (!bDragActive || !PC)
	{
		return false;
	}

	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	UWorld* World = PC->GetWorld();
	if (!FlyPC || !World)
	{
		CancelDrag();
		return false;
	}

	if (IsTerrainStampDragActive())
	{
		UpdateDragGhostFromWorldXY(FlyPC, WorldXY);
		return TryCommitTerrainStampDropAtStoredPlacement(PC);
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Build
		&& DragPayload.interactionType == EIHBuildPaletteInteraction::DropActor)
	{
		FVector SpawnLocation = FVector::ZeroVector;
		const bool bResolvedAtDrop = IH_BuildPaletteSubsystemPrivate::TryResolveBuildDragLocationFromWorldXY(
			FlyPC, DragPayload.itemID, WorldXY, SpawnLocation);
		if (!bResolvedAtDrop)
		{
			if (!bDragGhostLocationValid)
			{
				CancelDrag();
				return false;
			}
			SpawnLocation = DragPlacementActorOrigin;
		}

		if (SpawnLocation.SizeSquared() < FMath::Square(100.f) && !bDragGhostLocationValid)
		{
			CancelDrag();
			return false;
		}

		return CommitActiveStructureDrop(FlyPC, World, SpawnLocation);
	}

	// 2026-09-13: same fix as TryCompleteDropAtScreen's own Grid-fallback - this is the minimap-drop
	// twin of that function and had the identical bug (any non-Build DragPayload, e.g. the new
	// Convey Merchantman tile if ever dropped over an open minimap, silently spawned a Town Grid
	// instead). The Convey dev tile does not yet support minimap-drop placement (screen-space water
	// resolution only) - this now safely no-ops instead of mis-spawning if that's ever attempted.
	if (DragPayload.paletteTab != EIHBuildPaletteTab::Grid)
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("BuildPalette drop (minimap) — unhandled paletteTab=%d interaction=%d item=%s, ignoring"),
			static_cast<int32>(DragPayload.paletteTab),
			static_cast<int32>(DragPayload.interactionType),
			*DragPayload.itemID.ToString());
		CancelDrag();
		return false;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (!IH_BuildPaletteSubsystemPrivate::TryResolveGridDragLocationFromWorldXY(
		FlyPC, WorldXY, SpawnLocation))
	{
		CancelDrag();
		return false;
	}

	const FRotator SpawnRotation = FRotator::ZeroRotator;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIH_TownGridManager* Manager = World->SpawnActor<AIH_TownGridManager>(
		AIH_TownGridManager::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	if (Manager)
	{
		Manager->TownGridTemplate = DragPayload.townGridTemplate;
		Manager->InitializeFromTemplate(DragPayload.townGridTemplate);
		Manager->AlignActorToTerrainCenter();
		Manager->RebuildFromTemplate();
		FlyPC->SelectTownGridManager(Manager);
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("BuildPalette drop (minimap) — spawned TownGridManager item=%s template=%d at %s"),
			*DragPayload.itemID.ToString(),
			static_cast<int32>(DragPayload.townGridTemplate),
			*SpawnLocation.ToString());
	}
	CancelDrag();
	return Manager != nullptr;
}

bool UIH_BuildPaletteSubsystem::TryCompleteDropAtScreen(APlayerController* PC, const FVector2D& ScreenPos)
{
	if (!bDragActive || !PC)
	{
		return false;
	}
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	UWorld* World = PC->GetWorld();
	if (!FlyPC || !World)
	{
		CancelDrag();
		return false;
	}

	if (IsTerrainStampDragActive())
	{
		UpdateDragGhostFromScreen(FlyPC, ScreenPos);
		return TryCommitTerrainStampDropAtStoredPlacement(PC);
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Build
		&& DragPayload.interactionType == EIHBuildPaletteInteraction::DropActor)
	{
		FVector SpawnLocation = FVector::ZeroVector;
		const bool bResolvedAtDrop = IH_BuildPaletteSubsystemPrivate::TryResolveBuildDragLocation(
			FlyPC, DragPayload.itemID, ScreenPos, SpawnLocation);
		if (!bResolvedAtDrop)
		{
			if (!bDragGhostLocationValid)
			{
				CancelDrag();
				return false;
			}
			SpawnLocation = DragPlacementActorOrigin;
		}

		if (SpawnLocation.SizeSquared() < FMath::Square(100.f) && !bDragGhostLocationValid)
		{
			CancelDrag();
			return false;
		}

		return CommitActiveStructureDrop(FlyPC, World, SpawnLocation);
	}

	if (DragPayload.paletteTab == EIHBuildPaletteTab::Convey
		&& DragPayload.interactionType == EIHBuildPaletteInteraction::DropActor)
	{
		// 2026-09-13: dev-only Merchantman drag tile - must be branched explicitly here, ahead of
		// the Grid fallback below, which otherwise assumes "not Stamp, not Build-DropActor" means
		// Grid and would incorrectly spawn a AIH_TownGridManager for this payload instead.
		const bool bSpawned = FlyPC->TrySpawnMerchantmanAtScreen(ScreenPos);
		CancelDrag();
		return bSpawned;
	}

	// 2026-09-13: was an unconditional "else = Grid" fallback - made explicit after a report of the
	// new Convey tile spawning a Town Grid instead of a Merchantman. Any DragPayload that doesn't
	// match Stamp/Build/Convey above AND isn't actually tagged Grid now safely no-ops (with a log)
	// instead of silently defaulting to a Town Grid spawn - whatever the real trigger turns out to
	// be, it can no longer masquerade as a Grid drop.
	if (DragPayload.paletteTab != EIHBuildPaletteTab::Grid)
	{
		UE_LOG(
			LogIH_WB_Demo004, Warning,
			TEXT("BuildPalette drop — unhandled paletteTab=%d interaction=%d item=%s, ignoring (was falling through to Grid spawn before this fix)"),
			static_cast<int32>(DragPayload.paletteTab),
			static_cast<int32>(DragPayload.interactionType),
			*DragPayload.itemID.ToString());
		CancelDrag();
		return false;
	}

	FVector ImpactPoint = FVector::ZeroVector;
	if (!FlyPC->TryTraceTerrainAtScreen(ScreenPos, ImpactPoint))
	{
		CancelDrag();
		return false;
	}

	const FVector SpawnLocation = ImpactPoint + FVector(0.f, 0.f, 50.f);
	const FRotator SpawnRotation = FRotator::ZeroRotator;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIH_TownGridManager* Manager = World->SpawnActor<AIH_TownGridManager>(
		AIH_TownGridManager::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	if (Manager)
	{
		Manager->TownGridTemplate = DragPayload.townGridTemplate;
		Manager->InitializeFromTemplate(DragPayload.townGridTemplate);
		Manager->AlignActorToTerrainCenter();
		Manager->RebuildFromTemplate();
		FlyPC->SelectTownGridManager(Manager);
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("BuildPalette drop — spawned TownGridManager item=%s template=%d at %s"),
			*DragPayload.itemID.ToString(),
			static_cast<int32>(DragPayload.townGridTemplate),
			*SpawnLocation.ToString());
	}
	CancelDrag();
	return Manager != nullptr;
}

namespace IH_BuildPaletteTerrainStampManipulation
{
	static constexpr float TerrainStampRotateStepDeg = 5.f;
	static constexpr float TerrainStampWheelRotateDeg = 3.f;
	static constexpr float TerrainStampMinRadiusKm = 0.05f;
	static constexpr float TerrainStampMaxRadiusKm = 0.75f;
}

bool UIH_BuildPaletteSubsystem::TryFindTerrainStampAtScreen(
	APlayerController* PC,
	const FVector2D& ScreenPos,
	AIH_TerrainStampActor*& OutStamp) const
{
	OutStamp = nullptr;
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!FlyPC)
	{
		return false;
	}

	UWorld* World = FlyPC->GetWorld();
	if (!World)
	{
		return false;
	}

	auto TryPickStampOnIsland = [&OutStamp](
		AIH_WB_IslandActor* Island,
		const FVector2D& SurfaceXY) -> bool
	{
		if (!Island)
		{
			return false;
		}

		AIH_TerrainStampActor* BestStamp = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (AIH_TerrainStampActor* Stamp : Island->GetPlacedTerrainStamps())
		{
			if (!Stamp || Stamp->IsDragPreview() || Stamp->IsGalleryPreviewOnly())
			{
				continue;
			}
			const FVector2D StampXY(Stamp->GetActorLocation().X, Stamp->GetActorLocation().Y);
			const float DistSq = FVector2D::DistSquared(SurfaceXY, StampXY);
			// 2026-09-18 fix: RadiusKm is a leftover property from the old procedural-heightfield
			// system and does not track the stamp's actual current static-mesh footprint (nor any
			// Stage-12b grip resize) - a stamp visually larger than RadiusKm*100000cm implies could
			// be clicked confidently inside its real shape yet still miss this check, silently
			// falling through to the raycast fallback below (which is not always reached first,
			// depending on whether TrySampleIslandSurfaceAtScreen itself resolved). Derive the pick
			// radius from the mesh's real current XY bounds (scaled) instead, same pattern already
			// used for ComputeFootprintSampleXYPoints.
			float PickRadiusCm = Stamp->RadiusKm * 100000.f * 1.05f;
			if (const UStaticMeshComponent* MeshComp = Stamp->GetMeshComponent())
			{
				if (const UStaticMesh* Mesh = MeshComp->GetStaticMesh())
				{
					const FBoxSphereBounds Bounds = Mesh->GetBounds();
					const FVector MeshScale = MeshComp->GetRelativeScale3D();
					const float HalfExtentX = Bounds.BoxExtent.X * MeshScale.X;
					const float HalfExtentY = Bounds.BoxExtent.Y * MeshScale.Y;
					PickRadiusCm = FMath::Max(HalfExtentX, HalfExtentY) * 1.15f;
				}
			}
			if (DistSq <= FMath::Square(PickRadiusCm) && DistSq < BestDistSq)
			{
				BestStamp = Stamp;
				BestDistSq = DistSq;
			}
		}

		if (BestStamp)
		{
			OutStamp = BestStamp;
			return true;
		}
		return false;
	};

	FVector SurfaceWorld = FVector::ZeroVector;
	AActor* IslandActor = nullptr;
	if (FlyPC->TrySampleIslandSurfaceAtScreen(ScreenPos, SurfaceWorld, &IslandActor))
	{
		if (TryPickStampOnIsland(Cast<AIH_WB_IslandActor>(IslandActor), FVector2D(SurfaceWorld.X, SurfaceWorld.Y)))
		{
			return true;
		}
	}

	auto TryResolveStampFromHit = [&OutStamp](const FHitResult& Hit) -> bool
	{
		if (AIH_TerrainStampActor* Stamp = Cast<AIH_TerrainStampActor>(Hit.GetActor()))
		{
			if (!Stamp->IsDragPreview() && !Stamp->IsGalleryPreviewOnly())
			{
				OutStamp = Stamp;
				return true;
			}
		}
		if (UPrimitiveComponent* HitComp = Hit.GetComponent())
		{
			if (AIH_TerrainStampActor* Stamp = Cast<AIH_TerrainStampActor>(HitComp->GetOwner()))
			{
				if (!Stamp->IsDragPreview() && !Stamp->IsGalleryPreviewOnly())
				{
					OutStamp = Stamp;
					return true;
				}
			}
		}
		return false;
	};

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (FlyPC->DeprojectScreenToWorldRay(ScreenPos, WorldOrigin, WorldDirection))
	{
		const FVector TraceEnd = WorldOrigin + WorldDirection * 5.0e8f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(P1C10TerrainStampPick), true, FlyPC);
		Params.bTraceComplex = true;
		if (AActor* Ignore = GetDragPreviewIgnoreActor())
		{
			Params.AddIgnoredActor(Ignore);
		}

		TArray<FHitResult> Hits;
		if (World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (TryResolveStampFromHit(Hit))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool UIH_BuildPaletteSubsystem::TryHandleStampSelectionClickAtViewport(
	AIH_Cube2FlyPlayerController* FlyPC,
	const FVector2D& ViewportPick)
{
	if (!FlyPC || !IsWorldStampEditModeActive())
	{
		return false;
	}

	UWorld* World = FlyPC->GetWorld();
	if (!World)
	{
		return false;
	}

	AIH_TerrainStampActor* HitStamp = nullptr;
	if (TryFindTerrainStampAtScreen(FlyPC, ViewportPick, HitStamp) && HitStamp)
	{
		// 2026-09-11 fix: GetTimeSeconds() is DILATED game time - this project's own dev "Game Speed"
		// slider (screenshotted at 7.5x) shrinks a real half-second double-click into ~0.06s of game
		// time, making the window physically impossible to hit whenever speed != 1.0x (root cause of
		// a "can't reselect a stamp" report). Double-click is fundamentally a real-world human-input
		// timing, so it needs GetRealTimeSeconds() regardless of simulation speed.
		const float Now = World->GetRealTimeSeconds();
		const bool bDoubleClick = HitStamp == LastClickedStamp.Get()
			&& (Now - LastStampClickTimeSec) <= StampDoubleClickWindowSec;
		LastClickedStamp = HitStamp;
		LastStampClickTimeSec = Now;

#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase stamp click — stamp=%s dblClick=%d (W open, viewportPick)"),
			*FIHTerrainStampCatalog::Get(HitStamp->GetStampId()).RowName.ToString(),
			bDoubleClick ? 1 : 0);
#endif

		if (bDoubleClick)
		{
			SelectTerrainStamp(HitStamp);
		}
		return true;
	}

	FVector IslandSurface = FVector::ZeroVector;
	if (FlyPC->TrySampleIslandSurfaceAtScreen(ViewportPick, IslandSurface))
	{
		ClearTerrainStampSelection();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase stamp click — outside footprint, selection cleared"));
#endif
		return true;
	}

	return false;
}

void UIH_BuildPaletteSubsystem::SelectTerrainStamp(AIH_TerrainStampActor* Stamp, bool bPreserveUndoStack)
{
	if (SelectedTerrainStamp.Get() == Stamp)
	{
		return;
	}

	if (AIH_TerrainStampActor* Previous = SelectedTerrainStamp.Get())
	{
		Previous->SetStampSelected(false);
	}
	// 2026-09-10: the undo stack is scoped to one continuous selection session - moving selection
	// to a different stamp starts a fresh, empty stack (matches ClearTerrainStampSelection below).
	// Skipped when re-selecting a just-respawned stamp during undo itself (bPreserveUndoStack) -
	// otherwise popping one record would immediately erase every record still below it.
	if (!bPreserveUndoStack)
	{
		StampUndoStack.Reset();
	}

	SelectedTerrainStamp = Stamp;
	if (Stamp)
	{
		Stamp->SetStampSelected(true);
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("Phase B2b stamp selected — stamp=%s island=%d"),
			*FIHTerrainStampCatalog::Get(Stamp->GetStampId()).RowName.ToString(),
			Stamp->GetTargetIsland() ? Stamp->GetTargetIsland()->GetTankIslandIndex() : INDEX_NONE);
	}
}

bool UIH_BuildPaletteSubsystem::TrySelectNearestTerrainStampOnIsland(
	AIH_WB_IslandActor* Island,
	const FVector& SurfaceWorld)
{
	if (!Island)
	{
		return false;
	}

	AIH_TerrainStampActor* BestStamp = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector2D SurfaceXY(SurfaceWorld.X, SurfaceWorld.Y);
	for (AIH_TerrainStampActor* Stamp : Island->GetPlacedTerrainStamps())
	{
		if (!Stamp || Stamp->IsDragPreview() || Stamp->IsGalleryPreviewOnly())
		{
			continue;
		}
		const FVector2D StampXY(Stamp->GetActorLocation().X, Stamp->GetActorLocation().Y);
		const float DistSq = FVector2D::DistSquared(SurfaceXY, StampXY);
		if (DistSq < BestDistSq)
		{
			BestStamp = Stamp;
			BestDistSq = DistSq;
		}
	}

	if (BestStamp)
	{
		SelectTerrainStamp(BestStamp);
		return true;
	}
	return false;
}

void UIH_BuildPaletteSubsystem::RefreshAllTerrainStampsPassiveTint(bool bWorldTabOpen)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AIH_TerrainStampActor> It(World); It; ++It)
	{
		AIH_TerrainStampActor* Stamp = *It;
		if (Stamp && !Stamp->IsDragPreview())
		{
			Stamp->SetPassiveToggleTinted(bWorldTabOpen);
		}
	}
}

void UIH_BuildPaletteSubsystem::ClearTerrainStampSelection()
{
	if (AIH_TerrainStampActor* Previous = SelectedTerrainStamp.Get())
	{
		Previous->SetStampSelected(false);
	}
	SelectedTerrainStamp = nullptr;
	bStampMoveDragActive = false;
	StampUndoStack.Reset();
}

void UIH_BuildPaletteSubsystem::BeginStampMoveDrag(AIH_TerrainStampActor* Stamp)
{
	if (!Stamp || Stamp->IsDragPreview())
	{
		return;
	}
	SelectTerrainStamp(Stamp);
	bStampMoveDragActive = true;
	PendingMoveDragStartTransform = Stamp->GetActorTransform();
	PendingMoveDragStartDepthCm = Stamp->GetCurrentDepthBelowSurfaceCm();
	LastStampMoveDragRealTimeSec = -1.f;
}

void UIH_BuildPaletteSubsystem::UpdateStampMoveDrag(APlayerController* PC, const FVector2D& ScreenPos, float TotalScreenDeltaYFromDragStart)
{
	if (!bStampMoveDragActive)
	{
		return;
	}

	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!Stamp || !FlyPC)
	{
		return;
	}

	FVector SurfaceWorld = FVector::ZeroVector;
	AIH_WB_IslandActor* Island = nullptr;
	if (!IH_BuildPaletteSubsystemPrivate::TryResolveTerrainStampDragAtScreen(
		FlyPC, ScreenPos, SurfaceWorld, Island))
	{
		return;
	}

	if (Island != Stamp->GetTargetIsland())
	{
		return;
	}

	// 2026-09-17: "molasses" damping, per explicit user request - relocating an already-placed stamp
	// while it's over ITS OWN island (this whole function only ever runs in that case; the target-
	// island check above already excludes open water / other islands) used to snap the stamp directly
	// onto the raw cursor-traced surface point every tick, which reads as jerky/stuttery and makes
	// fine positioning hard. Smoothly easing the XY toward the cursor instead (VInterpTo, re-sampling
	// the real terrain height at the eased XY via ApplyWorldSurfacePlacement) gives it deliberate
	// weight without changing the SEPARATE initial drag-from-W-gallery-across-open-ocean placement
	// flow (UpdateDragGhostFromScreen/TryCommitTerrainStampDropAtStoredPlacement), which stays snappy
	// on purpose - flying a brand-new stamp roughly into place doesn't need fine control.
	// 2026-09-17 fix: GetDeltaSeconds() is dilated by the dev Game Speed slider (same pitfall this
	// project already root-caused for double-click timing) - at any speed above 1.0x this made the
	// eased position jump proportionally further per tick, reading as "jumps out of frame." Track
	// real (undilated) time manually instead, same GetRealTimeSeconds() pattern used elsewhere.
	float DeltaSeconds = 0.f;
	if (UWorld* World = FlyPC->GetWorld())
	{
		const float NowReal = World->GetRealTimeSeconds();
		if (LastStampMoveDragRealTimeSec >= 0.f)
		{
			DeltaSeconds = FMath::Max(0.f, NowReal - LastStampMoveDragRealTimeSec);
		}
		LastStampMoveDragRealTimeSec = NowReal;
	}
	// 2026-09-17 fix: Stamp->GetActorLocation().Z is the actor's PIVOT - already offset well below
	// the visible surface by CurrentDepthBelowSurfaceCm plus the mesh's own bottom-to-origin distance
	// (see ApplyWorldSurfacePlacement below) - not comparable to SurfaceWorld.Z, a raw traced ground
	// height. Blending the two in VInterpTo produced a garbage mid-air Z that made the floating-guard
	// validation above fail on nearly every tick, freezing the stamp in place (rotation still worked
	// since it's a separate code path untouched by this). Only the horizontal XY needs "molasses"
	// easing for fine positioning - Z should always come fresh from the real traced surface height,
	// exactly like ApplyWorldSurfacePlacement's own re-sampling already does.
	const FVector CurrentLoc = Stamp->GetActorLocation();
	const FVector2D DampedXY = DeltaSeconds > 0.f
		? FMath::Vector2DInterpTo(
			FVector2D(CurrentLoc.X, CurrentLoc.Y), FVector2D(SurfaceWorld.X, SurfaceWorld.Y),
			DeltaSeconds, StampRelocateDampingInterpSpeed)
		: FVector2D(SurfaceWorld.X, SurfaceWorld.Y);
	const FVector DampedTarget(DampedXY.X, DampedXY.Y, SurfaceWorld.Z);

	// 2026-09-17 fix: the eased XY above is a straight-line lerp between the stamp's last position
	// and the fresh cursor point - on a non-convex island (or a fast drag that briefly outpaces the
	// damping) that line can pass through a gap with no island collision under it at all. Previously
	// ApplyWorldSurfacePlacement just accepted whatever raw point it was given when BOTH its own
	// surface-sample and fallback trace failed to find real terrain, so the stamp would render
	// "floating"/"detached" out over open water instead of on the island. Validate the damped XY
	// resolves to real island surface BEFORE committing to it; if not, hold the stamp at its last
	// known-good position this tick rather than moving it into invalid space - it resumes moving the
	// instant the eased path comes back over the island.
	bool bDampedTargetOnIsland = false;
	if (const UGameInstance* GI = FlyPC->GetGameInstance())
	{
		if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
			GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
		{
			FVector ValidationSurface = FVector::ZeroVector;
			bDampedTargetOnIsland = IslandCollision->TrySampleIslandSurfaceAtXY(
				FVector2D(DampedTarget.X, DampedTarget.Y), DampedTarget.Z, 0.f, Stamp, ValidationSurface);
		}
	}
	if (!bDampedTargetOnIsland)
	{
		return;
	}

	// 2026-09-11: "merge into IslandMesh" canon - set sink depth from TOTAL vertical mouse movement
	// since drag-start BEFORE ApplyWorldSurfacePlacement, so the new depth is already in effect when
	// that recomputes Z.
	Stamp->SetManualSinkDepthFromDragTotal(PendingMoveDragStartDepthCm, TotalScreenDeltaYFromDragStart);
	Stamp->ApplyWorldSurfacePlacement(Island, DampedTarget);
}

void UIH_BuildPaletteSubsystem::EndStampMoveDrag()
{
	if (!bStampMoveDragActive)
	{
		return;
	}
	bStampMoveDragActive = false;

	if (AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get())
	{
		// 2026-09-10: push an undo record only if the drag actually moved the stamp - a click that
		// starts and ends the drag without moving the mouse shouldn't create a no-op undo step.
		// 2026-09-11: also checks sink depth, since a purely-vertical mouse movement could change
		// depth without moving the transform enough to trip the Equals() check on its own (unlikely
		// in practice - depth changes always move Location.Z too - but cheap to check explicitly).
		const bool bTransformChanged = !Stamp->GetActorTransform().Equals(PendingMoveDragStartTransform, KINDA_SMALL_NUMBER);
		const bool bDepthChanged = !FMath::IsNearlyEqual(Stamp->GetCurrentDepthBelowSurfaceCm(), PendingMoveDragStartDepthCm, 0.01f);
		if (bTransformChanged || bDepthChanged)
		{
			FIHStampUndoRecord Record;
			Record.ActionType = EIHStampUndoActionType::Transform;
			Record.Stamp = Stamp;
			Record.PreviousTransform = PendingMoveDragStartTransform;
			Record.PreviousDepthBelowSurfaceCm = PendingMoveDragStartDepthCm;
			PushStampUndoRecord(Record);
		}

		if (AIH_WB_IslandActor* Island = Stamp->GetTargetIsland())
		{
			Island->ReapplyAllTerrainStampsToHeightGrid();
			LogTerrainStampReplayHeaderStub();
		}
	}
}

// --- Stage 12b (2026-09-11): bounding-box scale grips. Mirrors Begin/UpdateStampMoveDrag/
// EndStampMoveDrag's own split of responsibility - the actor (AIH_TerrainStampActor) owns the live
// drag math, this subsystem owns undo-push and height-grid reapply. ---

bool UIH_BuildPaletteSubsystem::IsStampGripDragActive() const
{
	const AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	return Stamp && Stamp->IsGripDragActive();
}

bool UIH_BuildPaletteSubsystem::TryFindTerrainStampGripAtScreen(
	APlayerController* PC, const FVector2D& ScreenPos, EIHStampGripHandle& OutHandle, FVector& OutWorldPoint) const
{
	OutHandle = EIHStampGripHandle::None;
	OutWorldPoint = FVector::ZeroVector;
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp || !PC)
	{
		return false;
	}

	// Screen-space projection, not a terrain-surface raycast/3D-distance check - a grip can sit
	// over empty space past a tapered mesh's silhouette (e.g. a corner grip on a dome-shaped
	// stamp), where a world-space trace would miss it or hit ground far below instead.
	constexpr float GripPickRadiusPx = 24.f;
	float BestDistSq = GripPickRadiusPx * GripPickRadiusPx;
	const TArray<EIHStampGripHandle>& Handles = Stamp->GetGripMarkerHandles();
	const TArray<FVector>& Positions = Stamp->GetGripMarkerWorldPositions();
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		FVector2D GripScreenPos;
		if (!PC->ProjectWorldLocationToScreen(Positions[Index], GripScreenPos))
		{
			continue;
		}
		const float DistSq = static_cast<float>(FVector2D::DistSquared(GripScreenPos, ScreenPos));
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutHandle = Handles[Index];
			OutWorldPoint = Positions[Index];
		}
	}
	return OutHandle != EIHStampGripHandle::None;
}

void UIH_BuildPaletteSubsystem::BeginStampGripDrag(APlayerController* PC, EIHStampGripHandle Handle, const FVector& WorldPoint, bool bSymmetric)
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp || Handle == EIHStampGripHandle::None)
	{
		return;
	}
	PendingGripDragStartTransform = Stamp->GetActorTransform();
	GripDragPlaneOrigin = WorldPoint;
	GripDragPlaneNormal = (PC && PC->PlayerCameraManager)
		? PC->PlayerCameraManager->GetCameraRotation().Vector()
		: FVector::ForwardVector;
	Stamp->BeginGripDrag(Handle, WorldPoint, bSymmetric);
}

void UIH_BuildPaletteSubsystem::UpdateStampGripDrag(APlayerController* PC, const FVector2D& ScreenPos, float ScreenMouseDeltaY)
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!Stamp || !Stamp->IsGripDragActive() || !FlyPC)
	{
		return;
	}

	// Top's math never reads WorldPointForXY (driven entirely by ScreenMouseDeltaY instead), so
	// falling back to the plane origin itself when the ray-plane intersection is degenerate only
	// matters for X/Y/corner handles.
	FVector WorldPointForXY = GripDragPlaneOrigin;
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (FlyPC->DeprojectScreenToWorldRay(ScreenPos, RayOrigin, RayDirection))
	{
		const double Denominator = FVector::DotProduct(RayDirection, GripDragPlaneNormal);
		if (!FMath::IsNearlyZero(Denominator))
		{
			const double T = FVector::DotProduct(GripDragPlaneOrigin - RayOrigin, GripDragPlaneNormal) / Denominator;
			if (T > 0.0)
			{
				WorldPointForXY = RayOrigin + RayDirection * T;
			}
		}
	}

	Stamp->UpdateGripDrag(WorldPointForXY, ScreenMouseDeltaY);
}

void UIH_BuildPaletteSubsystem::EndStampGripDrag()
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp || !Stamp->IsGripDragActive())
	{
		return;
	}
	Stamp->EndGripDrag();

	// 2026-09-11: push an undo record only if the drag actually changed the transform - matches
	// EndStampMoveDrag's own "skip a no-op click" guard.
	if (!Stamp->GetActorTransform().Equals(PendingGripDragStartTransform, KINDA_SMALL_NUMBER))
	{
		FIHStampUndoRecord Record;
		Record.ActionType = EIHStampUndoActionType::Transform;
		Record.Stamp = Stamp;
		Record.PreviousTransform = PendingGripDragStartTransform;
		Record.PreviousDepthBelowSurfaceCm = Stamp->GetCurrentDepthBelowSurfaceCm();
		PushStampUndoRecord(Record);
	}

	ApplySelectedTerrainStampTransform();
}

void UIH_BuildPaletteSubsystem::RotateSelectedTerrainStamp(const float DeltaDeg)
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp)
	{
		return;
	}

	// 2026-09-10: one undo step per discrete rotation call (each R-press or wheel-tick), matching
	// the granularity DeltaDeg is already applied at.
	{
		FIHStampUndoRecord Record;
		Record.ActionType = EIHStampUndoActionType::Transform;
		Record.Stamp = Stamp;
		Record.PreviousTransform = Stamp->GetActorTransform();
		Record.PreviousDepthBelowSurfaceCm = Stamp->GetCurrentDepthBelowSurfaceCm();
		PushStampUndoRecord(Record);
	}

	Stamp->StampRotationDeg = FMath::Fmod(Stamp->StampRotationDeg + DeltaDeg + 360.f, 360.f);
	Stamp->SyncStampActorYaw();
	Stamp->RefreshPreviewMesh();
	ApplySelectedTerrainStampTransform();
#if !UE_BUILD_SHIPPING
	const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(Stamp->GetStampId());
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase B2b stamp rotate — stamp=%s yaw=%.1f delta=%.1f"),
		*Def.RowName.ToString(),
		Stamp->StampRotationDeg,
		DeltaDeg);
#endif
}

void UIH_BuildPaletteSubsystem::ScaleSelectedTerrainStampRadius(const float Factor)
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp || Factor <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	using namespace IH_BuildPaletteTerrainStampManipulation;
	Stamp->RadiusKm = FMath::Clamp(
		Stamp->RadiusKm * Factor, TerrainStampMinRadiusKm, TerrainStampMaxRadiusKm);
	Stamp->RefreshPreviewMesh();
	ApplySelectedTerrainStampTransform();
}

void UIH_BuildPaletteSubsystem::ApplySelectedTerrainStampTransform()
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp)
	{
		return;
	}
	if (AIH_WB_IslandActor* Island = Stamp->GetTargetIsland())
	{
		Island->ReapplyAllTerrainStampsToHeightGrid();
		LogTerrainStampReplayHeaderStub();
	}
}

void UIH_BuildPaletteSubsystem::TickTerrainStampManipulation(APlayerController* PC)
{
	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!FlyPC || !HasSelectedTerrainStamp() || !IsWorldStampEditModeActive())
	{
		return;
	}

	using namespace IH_BuildPaletteTerrainStampManipulation;
	const bool bShift = FlyPC->IsInputKeyDown(EKeys::LeftShift) || FlyPC->IsInputKeyDown(EKeys::RightShift);
	if (FlyPC->WasInputKeyJustPressed(EKeys::R))
	{
		const float Delta = bShift ? -TerrainStampRotateStepDeg : TerrainStampRotateStepDeg;
		RotateSelectedTerrainStamp(Delta);
	}
	if (FlyPC->WasInputKeyJustPressed(EKeys::LeftBracket))
	{
		ScaleSelectedTerrainStampRadius(0.95f);
	}
	if (FlyPC->WasInputKeyJustPressed(EKeys::RightBracket))
	{
		ScaleSelectedTerrainStampRadius(1.05f);
	}
	if (FlyPC->WasInputKeyJustPressed(EKeys::Delete))
	{
		TryRemoveSelectedTerrainStamp();
	}
}

void UIH_BuildPaletteSubsystem::ApplySelectedStampMouseWheel(APlayerController* PC, float WheelDelta)
{
	if (FMath::IsNearlyZero(WheelDelta) || !HasSelectedTerrainStamp() || !IsWorldStampEditModeActive())
	{
		return;
	}

	AIH_Cube2FlyPlayerController* FlyPC = Cast<AIH_Cube2FlyPlayerController>(PC);
	if (!FlyPC)
	{
		return;
	}

	using namespace IH_BuildPaletteTerrainStampManipulation;
	const bool bShift = FlyPC->IsInputKeyDown(EKeys::LeftShift) || FlyPC->IsInputKeyDown(EKeys::RightShift);
	const float Sign = WheelDelta > 0.f ? 1.f : -1.f;
	// 2026-09-10: inverted to match the canonical IslandMesh convention (Shift+Wheel rotates -
	// IslandShiftWheelRotateDeg, IH_Cube2FlyPlayerController.cpp) - previously the opposite way
	// round (Shift=scale, plain=rotate). Plain-wheel scale is still inert for a static-mesh stamp
	// (ScaleSelectedTerrainStampRadius only mutates RadiusKm, which nothing reads yet) - real scale-
	// grip interaction is a separate follow-up round.
	if (bShift)
	{
		RotateSelectedTerrainStamp(Sign * TerrainStampWheelRotateDeg);
	}
	else
	{
		ScaleSelectedTerrainStampRadius(Sign > 0.f ? 1.05f : 0.95f);
	}
}

bool UIH_BuildPaletteSubsystem::TryRemoveSelectedTerrainStamp()
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp)
	{
		return false;
	}

	AIH_WB_IslandActor* Island = Stamp->GetTargetIsland();

	// 2026-09-10: push a Delete undo record BEFORE destroying, capturing enough to respawn (StampId/
	// transform/island) - one Ctrl+Z brings it back. Deliberately does NOT call
	// ClearTerrainStampSelection() here (that flushes StampUndoStack, which would erase the record
	// this just pushed before the player ever got to press Ctrl+Z) - a delete is a "deselect" that
	// must still leave its own undo step reachable, unlike a normal click-away deselect.
	{
		FIHStampUndoRecord Record;
		Record.ActionType = EIHStampUndoActionType::Delete;
		Record.StampId = Stamp->GetStampId();
		Record.PreviousTransform = Stamp->GetActorTransform();
		Record.PreviousDepthBelowSurfaceCm = Stamp->GetCurrentDepthBelowSurfaceCm();
		Record.TargetIsland = Island;
		PushStampUndoRecord(Record);
	}

	if (Island)
	{
		Island->UnregisterTerrainStamp(Stamp);
	}
	Stamp->SetStampSelected(false);
	SelectedTerrainStamp = nullptr;
	bStampMoveDragActive = false;
	Stamp->Destroy();

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogIH_WB_Demo004, Log,
		TEXT("Phase B2b stamp removed island=%d remaining=%d"),
		Island ? Island->GetTankIslandIndex() : INDEX_NONE,
		Island ? Island->GetPlacedTerrainStamps().Num() : 0);
#endif

	if (Island)
	{
		Island->ReapplyAllTerrainStampsToHeightGrid();
		LogTerrainStampReplayHeaderStub();
	}
	return true;
}

void UIH_BuildPaletteSubsystem::PushStampUndoRecord(const FIHStampUndoRecord& Record)
{
	if (StampUndoStack.Num() >= MaxStampUndoStackDepth)
	{
		StampUndoStack.RemoveAt(0);
	}
	StampUndoStack.Add(Record);
}

bool UIH_BuildPaletteSubsystem::UndoLastStampAction()
{
	if (StampUndoStack.Num() == 0)
	{
		return false;
	}

	const FIHStampUndoRecord Record = StampUndoStack.Pop();

	if (Record.ActionType == EIHStampUndoActionType::Transform)
	{
		AIH_TerrainStampActor* Stamp = Record.Stamp.Get();
		if (!Stamp)
		{
			return false;
		}
		Stamp->SetActorTransform(Record.PreviousTransform);
		Stamp->SetCurrentDepthBelowSurfaceCm(Record.PreviousDepthBelowSurfaceCm);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase B2b stamp undo (Transform) — stamp=%s"),
			*FIHTerrainStampCatalog::Get(Stamp->GetStampId()).RowName.ToString());
#endif
		return true;
	}

	// Delete record: respawn at the exact previous transform (not a fresh surface trace) and
	// re-select it, preserving whatever else remains on the stack.
	AIH_WB_IslandActor* Island = Record.TargetIsland.Get();
	UWorld* World = Island ? Island->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	AIH_TerrainStampActor* Respawned = SpawnBareStampActor(World, Island, Record.StampId);
	if (!Respawned)
	{
		return false;
	}
	Respawned->SetActorTransform(Record.PreviousTransform);
	Respawned->SetCurrentDepthBelowSurfaceCm(Record.PreviousDepthBelowSurfaceCm);
	// A fresh drop's island-collision registration happens inside ApplyWorldSurfacePlacement - an
	// undo-restore bypasses that (SetActorTransform must land at the exact previous pose, not
	// re-snap to a fresh surface trace), so register explicitly here instead.
	Respawned->RegisterWithIslandCollisionIfNeeded();
	Island->ReapplyAllTerrainStampsToHeightGrid();
	Island->SyncPlacedTerrainStampSurfaceAnchors();
	SelectTerrainStamp(Respawned, /*bPreserveUndoStack=*/true);
	LogTerrainStampReplayHeaderStub();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase B2b stamp undo (Delete restore) — stamp=%s island=%d"),
		*FIHTerrainStampCatalog::Get(Record.StampId).RowName.ToString(),
		Island->GetTankIslandIndex());
#endif
	return true;
}

void UIH_BuildPaletteSubsystem::DrawSelectedTerrainStampGizmo(UWorld* World, AIH_Cube2FlyPlayerController* FlyPC) const
{
	const AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!World || !Stamp || !IsWorldStampEditModeActive())
	{
		return;
	}

	if (!FlyPC)
	{
		FlyPC = Cast<AIH_Cube2FlyPlayerController>(World->GetFirstPlayerController());
	}

	const FColor RingColor(255, 170, 30);
	IH_BuildPaletteSubsystemPrivate::DrawStampSelectionRing(World, FlyPC, Stamp, RingColor);

	const FVector Anchor = Stamp->GetActorLocation();
	const float FootprintRadiusCm = Stamp->GetPreviewFootprintRadiusCm();
	const FVector Center = Anchor + FVector(0.f, 0.f, IHInvisibleHandSpec::StampPlacedPreviewSurfaceOffsetCm);
	const float YawRad = FMath::DegreesToRadians(Stamp->StampRotationDeg);
	const FVector YawDir(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f);
	const float LineThick = FlyPC
		? FlyPC->ComputeWorldSizeForScreenPixels(Center, 4.f)
		: FMath::Max(FootprintRadiusCm * 0.003f, 800.f);
	DrawDebugLine(
		World,
		Center,
		Center + YawDir * FootprintRadiusCm * 0.45f,
		RingColor,
		false,
		-1.f,
		0,
		LineThick);
}

void UIH_BuildPaletteSubsystem::DrawPlacedTerrainStampPickHints(
	UWorld* World,
	AIH_Cube2FlyPlayerController* FlyPC) const
{
	(void)World;
	(void)FlyPC;
}

void UIH_BuildPaletteSubsystem::LogTerrainStampReplayHeaderStub() const
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AIH_WB_Demo004GameMode* GM = World->GetAuthGameMode<AIH_WB_Demo004GameMode>();
	if (!GM)
	{
		return;
	}

	TArray<FIHPlacedTerrainStampReplayEntry> Entries;
	for (int32 IslandIdx = 0; IslandIdx < 32; ++IslandIdx)
	{
		if (const AIH_WB_IslandActor* Island = GM->GetSpawnedIsland(IslandIdx))
		{
			Island->CollectTerrainStampReplayEntries(Entries);
		}
	}

	UE_LOG(LogIH_WB_Demo004, Log, TEXT("Phase B2b replayHeader stub — stampCount=%d"), Entries.Num());
	for (const FIHPlacedTerrainStampReplayEntry& Entry : Entries)
	{
		UE_LOG(
			LogIH_WB_Demo004, Log,
			TEXT("  replayStamp island=%d id=%d xy=(%.0f,%.0f) radius=%.0fm amp=%.1f rot=%.0f invert=%d"),
			Entry.IslandIndex,
			static_cast<int32>(Entry.StampId),
			Entry.CenterLocalCm.X,
			Entry.CenterLocalCm.Y,
			Entry.RadiusKm * 1000.f,
			Entry.AmplitudeAzgaar,
			Entry.RotationDeg,
			Entry.bInvertHeight ? 1 : 0);
	}
#endif
}
