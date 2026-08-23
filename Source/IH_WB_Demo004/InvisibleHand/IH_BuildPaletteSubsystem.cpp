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
#include "IH_WB_IslandActor.h"
#include "FIHTerrainStampTypes.h"
#include "IH_TerrainStampLibrary.h"
#include "IH_TerrainStampActor.h"
#include "IH_WB_Demo004GameMode.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
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
	SyncWidgetFlyOutState();
	if (BuildPaletteWidget)
	{
		BuildPaletteWidget->RequestLayoutRefresh();
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
	ActiveTab = Tab;
	bFlyOutOpen = true;
	if (Tab == EIHBuildPaletteTab::Build)
	{
		PC->DeselectTownGridManager();
	}
	else if (Tab == EIHBuildPaletteTab::World)
	{
		PC->ResetIslandViewportDoubleClickTracking();
		PC->RequestDeselectIsland();
	}
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
	if (IsTerrainStampDragActive())
	{
		return true;
	}

	// Widget paint state is authoritative — catches subsystem/widget desync.
	if (BuildPaletteWidget && BuildPaletteWidget->IsWorldFlyOutVisible())
	{
		return true;
	}

	if (bFlyOutOpen && ActiveTab == EIHBuildPaletteTab::World)
	{
		return true;
	}

	return false;
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
	if (!Island || !Island->HasCellHeightGrid() || StampId >= EIHTerrainStampId::MAX)
	{
		return false;
	}

	UWorld* World = FlyPC ? FlyPC->GetWorld() : Island->GetWorld();
	if (!World)
	{
		return false;
	}

	const FIHTerrainStampDefinition& Def = FIHTerrainStampCatalog::Get(StampId);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Island;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AIH_TerrainStampActor* PlacedStamp = World->SpawnActor<AIH_TerrainStampActor>(
		AIH_TerrainStampActor::StaticClass(),
		SurfaceWorld,
		FRotator(0.f, 0.f, 0.f),
		SpawnParams);
	if (!PlacedStamp)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Phase B2b stamp spawn failed island=%d stamp=%s"),
			Island->GetTankIslandIndex(), *Def.RowName.ToString());
		return false;
	}

	PlacedStamp->InitializeStamp(StampId, Def.bDefaultInvert, false);
	PlacedStamp->SetDragPreviewMode(false);
	PlacedStamp->ApplyWorldSurfacePlacement(Island, SurfaceWorld);
	PlacedStamp->RegisterAllComponents();
	PlacedStamp->RefreshPreviewMesh();
	PlacedStamp->SetActorHiddenInGame(false);
	PlacedStamp->SetActorEnableCollision(false);
	Island->RegisterTerrainStamp(PlacedStamp);
	Island->ReapplyAllTerrainStampsToHeightGrid();
	Island->SyncPlacedTerrainStampSurfaceAnchors();
	SelectTerrainStamp(PlacedStamp);
	LogTerrainStampReplayHeaderStub();

#if !UE_BUILD_SHIPPING
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
			const float PickRadiusCm = Stamp->RadiusKm * 100000.f * 1.05f;
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
		const float Now = World->GetTimeSeconds();
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

void UIH_BuildPaletteSubsystem::SelectTerrainStamp(AIH_TerrainStampActor* Stamp)
{
	if (SelectedTerrainStamp.Get() == Stamp)
	{
		return;
	}

	if (AIH_TerrainStampActor* Previous = SelectedTerrainStamp.Get())
	{
		Previous->SetStampSelected(false);
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

void UIH_BuildPaletteSubsystem::ClearTerrainStampSelection()
{
	if (AIH_TerrainStampActor* Previous = SelectedTerrainStamp.Get())
	{
		Previous->SetStampSelected(false);
	}
	SelectedTerrainStamp = nullptr;
	bStampMoveDragActive = false;
}

void UIH_BuildPaletteSubsystem::BeginStampMoveDrag(AIH_TerrainStampActor* Stamp)
{
	if (!Stamp || Stamp->IsDragPreview())
	{
		return;
	}
	SelectTerrainStamp(Stamp);
	bStampMoveDragActive = true;
}

void UIH_BuildPaletteSubsystem::UpdateStampMoveDrag(APlayerController* PC, const FVector2D& ScreenPos)
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

	Stamp->ApplyWorldSurfacePlacement(Island, SurfaceWorld);
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
		if (AIH_WB_IslandActor* Island = Stamp->GetTargetIsland())
		{
			Island->ReapplyAllTerrainStampsToHeightGrid();
			LogTerrainStampReplayHeaderStub();
		}
	}
}

void UIH_BuildPaletteSubsystem::RotateSelectedTerrainStamp(const float DeltaDeg)
{
	AIH_TerrainStampActor* Stamp = SelectedTerrainStamp.Get();
	if (!Stamp)
	{
		return;
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
	if (bShift)
	{
		ScaleSelectedTerrainStampRadius(Sign > 0.f ? 1.05f : 0.95f);
	}
	else
	{
		RotateSelectedTerrainStamp(Sign * TerrainStampWheelRotateDeg);
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
	if (Island)
	{
		Island->UnregisterTerrainStamp(Stamp);
	}
	ClearTerrainStampSelection();
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
