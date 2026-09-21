// Copyright Invisible Hand. All Rights Reserved.

#include "IH_TerrainStampActor.h"
#include "FIHTerrainStampMeshTypes.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_WB_IslandActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Forward-declared here, defined below (after the constructor) - both refer to the same
	// translation-unit-local anonymous namespace.
	UMaterialInstanceDynamic* CreateStampTintMaterial(UObject* Outer, const FLinearColor& Tint);
}

FVector IHStampGripHandleToSignedAxisMask(EIHStampGripHandle Handle)
{
	switch (Handle)
	{
	case EIHStampGripHandle::PosX: return FVector(1.0, 0.0, 0.0);
	case EIHStampGripHandle::NegX: return FVector(-1.0, 0.0, 0.0);
	case EIHStampGripHandle::PosY: return FVector(0.0, 1.0, 0.0);
	case EIHStampGripHandle::NegY: return FVector(0.0, -1.0, 0.0);
	case EIHStampGripHandle::Top: return FVector(0.0, 0.0, 1.0);
	case EIHStampGripHandle::CornerPP: return FVector(1.0, 1.0, 0.0);
	case EIHStampGripHandle::CornerPN: return FVector(1.0, -1.0, 0.0);
	case EIHStampGripHandle::CornerNP: return FVector(-1.0, 1.0, 0.0);
	case EIHStampGripHandle::CornerNN: return FVector(-1.0, -1.0, 0.0);
	default: return FVector::ZeroVector;
	}
}

AIH_TerrainStampActor::AIH_TerrainStampActor()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StampMesh"));
	MeshComponent->SetupAttachment(SceneRoot);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetGenerateOverlapEvents(false);

	// --- Stage 12b bounding-box scale grips (2026-09-11) ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Translucent so the box doesn't hide the stamp mesh inside it - falls back to no material
	// (engine default opaque) if the content asset hasn't been built yet, same "if Succeeded()"
	// never-a-hard-dependency pattern used everywhere else in this constructor.
	BoundingBoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoundingBoxMesh"));
	BoundingBoxMesh->SetupAttachment(SceneRoot);
	BoundingBoxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundingBoxMesh->SetCanEverAffectNavigation(false);
	BoundingBoxMesh->SetCastShadow(false);
	BoundingBoxMesh->SetVisibility(false);
	if (CubeMeshFinder.Succeeded())
	{
		BoundingBoxMesh->SetStaticMesh(CubeMeshFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GizmoTranslucentMaterialFinder(
		TEXT("/Game/InvisibleHand/World/TerrainStamps/M_TerrainStampGizmoTranslucent.M_TerrainStampGizmoTranslucent"));
	if (GizmoTranslucentMaterialFinder.Succeeded())
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GizmoTranslucentMaterialFinder.Object, this))
		{
			MID->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(IHTerrainStampColors::BoundingBoxGripColor));
			BoundingBoxMesh->SetMaterial(0, MID);
		}
	}

	static const EIHStampGripHandle GripHandleOrder[9] = {
		EIHStampGripHandle::PosX, EIHStampGripHandle::NegX,
		EIHStampGripHandle::PosY, EIHStampGripHandle::NegY,
		EIHStampGripHandle::Top,
		EIHStampGripHandle::CornerPP, EIHStampGripHandle::CornerPN,
		EIHStampGripHandle::CornerNP, EIHStampGripHandle::CornerNN,
	};
	GripMarkers.Reserve(UE_ARRAY_COUNT(GripHandleOrder));
	GripMarkerHandles.Reserve(UE_ARRAY_COUNT(GripHandleOrder));
	GripMarkerWorldPositions.SetNumZeroed(UE_ARRAY_COUNT(GripHandleOrder));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(GripHandleOrder); ++Index)
	{
		UStaticMeshComponent* Grip = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("GripMarker%d"), Index));
		Grip->SetupAttachment(SceneRoot);
		Grip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Grip->SetCanEverAffectNavigation(false);
		Grip->SetCastShadow(false);
		Grip->SetVisibility(false);
		// Sized in RegenerateBoundingBoxGizmo (proportional to the stamp's own current extent, not
		// a fixed constant - a fixed size that looked fine on a small stamp would be imperceptible
		// on a ~1400m-wide landform). Children of SceneRoot (not MeshComponent) still means this
		// world size is driven by the extent alone, never distorted by the mesh's own scale.
		if (SphereMeshFinder.Succeeded())
		{
			Grip->SetStaticMesh(SphereMeshFinder.Object);
		}
		if (UMaterialInstanceDynamic* MID = CreateStampTintMaterial(
			this, FLinearColor(IHTerrainStampColors::BoundingBoxGripColor)))
		{
			Grip->SetMaterial(0, MID);
		}
		GripMarkers.Add(Grip);
		GripMarkerHandles.Add(GripHandleOrder[Index]);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AIH_TerrainStampActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromIslandCollisionIfNeeded();
	Super::EndPlay(EndPlayReason);
}

void AIH_TerrainStampActor::InitializeStamp(EIHTerrainStampId InStampId, bool bInInvertHeight, bool bInGalleryPreviewOnly)
{
	StampId = InStampId;
	bInvertHeight = bInInvertHeight;
	bGalleryPreviewOnly = bInGalleryPreviewOnly;

	if (const FIHTerrainStampMeshRow* Row = FIHTerrainStampMeshCatalog::Get(InStampId))
	{
		if (UStaticMesh* Mesh = Row->Mesh.LoadSynchronous())
		{
			MeshComponent->SetStaticMesh(Mesh);
		}
	}
}

void AIH_TerrainStampActor::SetTargetIsland(AIH_WB_IslandActor* InIsland)
{
	TargetIsland = InIsland;
}

void AIH_TerrainStampActor::SetDragPreviewMode(bool bInDragPreview)
{
	bIsDragPreview = bInDragPreview;

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(
			bInDragPreview ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	SetActorEnableCollision(!bInDragPreview);

	if (bInDragPreview)
	{
		// A drag preview never counts as "really placed" - drop any prior real-placement
		// registration (e.g. re-entering edit mode on an already-placed stamp) rather than leaving
		// a stale island-collision entry pointing at a component that's about to move freely.
		UnregisterFromIslandCollisionIfNeeded();
	}
}

void AIH_TerrainStampActor::ApplyWorldSurfacePlacement(AIH_WB_IslandActor* InIsland, const FVector& SurfaceWorld)
{
	TargetIsland = InIsland;

	// Ground-sample-then-offset-by-mesh-bounds - same pattern as
	// AIH_StructurePlacementActor::AlignToTerrainCenter/ComputeActorOriginFromMeshBottom.
	FVector SurfacePoint = SurfaceWorld;
	if (UWorld* World = GetWorld())
	{
		bool bFoundSurface = false;
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
				GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
			{
				FVector IslandSurface = FVector::ZeroVector;
				if (IslandCollision->TrySampleIslandSurfaceAtXY(
					FVector2D(SurfaceWorld.X, SurfaceWorld.Y),
					SurfaceWorld.Z,
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
			const FVector TraceStart = SurfaceWorld + FVector(0.f, 0.f, 500000.f);
			const FVector TraceEnd = SurfaceWorld - FVector(0.f, 0.f, 500000.f);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TerrainStampSurfaceAlign), true, this);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
			{
				SurfacePoint = Hit.ImpactPoint;
			}
		}
	}

	FVector NewLocation(SurfacePoint.X, SurfacePoint.Y, SurfacePoint.Z - CurrentDepthBelowSurfaceCm);
	if (const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr)
	{
		// 2026-09-11: read through the mesh's own CURRENT scale (Stage 12b's grip-drag can leave
		// this non-uniform/non-identity) rather than raw unscaled asset bounds - otherwise a resized
		// stamp would place using its ORIGINAL size, not its current one.
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector MeshScale = MeshComponent->GetRelativeScale3D();
		const double BottomLocalZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * MeshScale.Z;
		const double TopLocalZ = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * MeshScale.Z;

		double DesiredActorZ = (SurfacePoint.Z - CurrentDepthBelowSurfaceCm) - BottomLocalZ;

		// 2026-09-11: "merge into IslandMesh" canon - absolute safety backstop, independent of
		// CurrentDepthBelowSurfaceCm (which can now grow via AdjustManualSinkDepth): the highest
		// point must always stay at least MinVisibleTopMarginCm above the CURRENT surface, so a
		// stamp dragged deeper and deeper simply stops sinking further past this line instead of
		// ever fully disappearing. Previously anchored to DepthBelowSurfaceCm itself, which would
		// have sunk right along with an adjustable depth and stopped being a real safety net.
		const double TopWorldZ = DesiredActorZ + TopLocalZ;
		const double MinTopWorldZ = SurfacePoint.Z + MinVisibleTopMarginCm;
		if (TopWorldZ < MinTopWorldZ)
		{
			DesiredActorZ += (MinTopWorldZ - TopWorldZ);
		}

		// 2026-09-18: the single-origin-point sink above can't guarantee the WHOLE bottom stays buried
		// on sloped terrain (or when planting against another stamp's sloped surface, now that stamps
		// can stack - see TrySampleIslandSurfaceAtXY's OutIslandActor resolution). Sample the mesh's
		// actual current footprint perimeter and re-clamp deeper if any sampled point is lower than
		// what the origin-only sink already accounts for - a flat bottom below the LOWEST sampled
		// point is automatically below every higher one too, so this guarantees no visible gap
		// anywhere under the footprint, not just at the drop point.
		if (const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
				GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
			{
				double LowestFootprintSurfaceZ = SurfacePoint.Z;
				for (const FVector2D& OffsetXY : ComputeFootprintSampleXYPoints())
				{
					FVector PointSurface = FVector::ZeroVector;
					const FVector2D SampleXY(SurfacePoint.X + OffsetXY.X, SurfacePoint.Y + OffsetXY.Y);
					if (IslandCollision->TrySampleIslandSurfaceAtXY(
						SampleXY, SurfacePoint.Z, 0.f, this, PointSurface))
					{
						LowestFootprintSurfaceZ = FMath::Min(LowestFootprintSurfaceZ, (double)PointSurface.Z);
					}
					// Unresolved point (e.g. footprint edge overhangs open water past a cliff) -
					// skip it rather than forcing a degenerate full-depth dive toward the sea floor.
				}

				const double RequiredMaxBottomWorldZ = LowestFootprintSurfaceZ - MinFootprintBurialDepthCm;
				const double CandidateBottomWorldZ = DesiredActorZ + BottomLocalZ;
				if (CandidateBottomWorldZ > RequiredMaxBottomWorldZ)
				{
					DesiredActorZ += (RequiredMaxBottomWorldZ - CandidateBottomWorldZ);
				}
			}
		}

		NewLocation.Z = DesiredActorZ;
	}

	SetActorLocation(NewLocation);

	if (!bIsDragPreview)
	{
		RegisterWithIslandCollisionIfNeeded();
	}
}

TArray<FVector2D> AIH_TerrainStampActor::ComputeFootprintSampleXYPoints() const
{
	TArray<FVector2D> Offsets;

	const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return Offsets;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector MeshScale = MeshComponent->GetRelativeScale3D();
	const double HalfExtentX = Bounds.BoxExtent.X * MeshScale.X;
	const double HalfExtentY = Bounds.BoxExtent.Y * MeshScale.Y;

	// 4 corners + 4 edge midpoints, in the mesh's own local XY space before rotation.
	const FVector2D LocalPoints[8] = {
		FVector2D(HalfExtentX, HalfExtentY), FVector2D(-HalfExtentX, HalfExtentY),
		FVector2D(HalfExtentX, -HalfExtentY), FVector2D(-HalfExtentX, -HalfExtentY),
		FVector2D(HalfExtentX, 0.0), FVector2D(-HalfExtentX, 0.0),
		FVector2D(0.0, HalfExtentY), FVector2D(0.0, -HalfExtentY),
	};

	const double YawRad = FMath::DegreesToRadians(GetActorRotation().Yaw);
	const double CosYaw = FMath::Cos(YawRad);
	const double SinYaw = FMath::Sin(YawRad);

	Offsets.Reserve(8);
	for (const FVector2D& Local : LocalPoints)
	{
		Offsets.Add(FVector2D(
			Local.X * CosYaw - Local.Y * SinYaw,
			Local.X * SinYaw + Local.Y * CosYaw));
	}

	return Offsets;
}

void AIH_TerrainStampActor::SetManualSinkDepthFromDragTotal(float BaselineDepthCm, float TotalScreenDeltaYFromDragStart)
{
	// Re-derived fresh from the drag's own baseline each tick (never compounds a running total) -
	// same "total distance from drag start" pattern as the X/Y/corner grips' own scale math.
	// Positive delta (mouse moved down) sinks deeper - same TerrainStampHeightDragCmPerPixel scale
	// as the Top grip uses (Top negates this since dragging UP grows height there; here dragging
	// DOWN sinks deeper, so no negation).
	CurrentDepthBelowSurfaceCm = FMath::Max(
		DefaultDepthBelowSurfaceCm,
		BaselineDepthCm + TotalScreenDeltaYFromDragStart * TerrainStampHeightDragCmPerPixel);
}

void AIH_TerrainStampActor::RegisterWithIslandCollisionIfNeeded()
{
	if (bRegisteredWithIslandCollision || !MeshComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
		GI ? GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>() : nullptr)
	{
		Tags.AddUnique(UIH_P1C07_IslandCollisionSubsystem::IslandActorTag);
		IslandCollision->RegisterIslandCollision(this, MeshComponent);
		bRegisteredWithIslandCollision = true;
	}
}

void AIH_TerrainStampActor::UnregisterFromIslandCollisionIfNeeded()
{
	if (!bRegisteredWithIslandCollision)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
				GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
			{
				IslandCollision->UnregisterIslandCollision(this);
			}
		}
	}
	bRegisteredWithIslandCollision = false;
}

void AIH_TerrainStampActor::SyncStampActorYaw()
{
	// 2026-09-10: previously a no-op - RotateSelectedTerrainStamp already updated StampRotationDeg
	// and called this every time, so rotation was silently doing nothing visually until this fix.
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw = StampRotationDeg;
	SetActorRotation(NewRotation);
}

namespace
{
	// Mirrors AIH_StructurePlacementActor's own CreateDragGhostMaterial pattern (try several
	// candidate color-parameter names, since different base materials name their tint parameter
	// differently) - here for a persistent "this stamp is selected" tint rather than a transient
	// drag-ghost one.
	UMaterialInstanceDynamic* CreateStampTintMaterial(UObject* Outer, const FLinearColor& Tint)
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

		static const FName ColorNames[] = {
			FName(TEXT("Color")), FName(TEXT("BaseColor")), FName(TEXT("TintColor")), FName(TEXT("Vector")),
		};
		for (const FName& ColorName : ColorNames)
		{
			MID->SetVectorParameterValue(ColorName, Tint);
		}
		MID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.6f);
		return MID;
	}
}

void AIH_TerrainStampActor::SetStampSelected(bool bInSelected)
{
	if (bStampSelected == bInSelected)
	{
		return;
	}
	bStampSelected = bInSelected;
	RefreshStampVisualTint();
	UpdateBoundingBoxGizmoVisibility();
}

void AIH_TerrainStampActor::SetPassiveToggleTinted(bool bInTinted)
{
	if (bPassiveToggleTinted == bInTinted)
	{
		return;
	}
	bPassiveToggleTinted = bInTinted;
	RefreshStampVisualTint();
}

void AIH_TerrainStampActor::RefreshStampVisualTint()
{
	if (!MeshComponent)
	{
		return;
	}

	// Selected beats passive-toggle beats the real material - see Task 1/2 spec (2026-09-11).
	const bool bWantTint = bStampSelected || bPassiveToggleTinted;
	if (bWantTint)
	{
		if (CachedSourceMaterials.Num() == 0)
		{
			const int32 SlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
			CachedSourceMaterials.Reset(SlotCount);
			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				CachedSourceMaterials.Add(MeshComponent->GetMaterial(SlotIndex));
			}
		}

		const FLinearColor Tint = bStampSelected
			? FLinearColor(IHTerrainStampColors::SelectedStampColor)
			: FLinearColor(IHTerrainStampColors::AllStampsToggleColor);
		if (UMaterialInstanceDynamic* TintMID = CreateStampTintMaterial(this, Tint))
		{
			for (int32 SlotIndex = 0; SlotIndex < CachedSourceMaterials.Num(); ++SlotIndex)
			{
				MeshComponent->SetMaterial(SlotIndex, TintMID);
			}
		}
	}
	else
	{
		for (int32 SlotIndex = 0; SlotIndex < CachedSourceMaterials.Num(); ++SlotIndex)
		{
			MeshComponent->SetMaterial(SlotIndex, CachedSourceMaterials[SlotIndex]);
		}
		CachedSourceMaterials.Reset();
	}
	MeshComponent->MarkRenderStateDirty();
}

// --- Stage 12b bounding-box scale grips (2026-09-11) ---

void AIH_TerrainStampActor::UpdateBoundingBoxGizmoVisibility()
{
	if (!BoundingBoxMesh)
	{
		return;
	}
	if (bStampSelected)
	{
		RegenerateBoundingBoxGizmo();
	}
	BoundingBoxMesh->SetVisibility(bStampSelected);
	for (UStaticMeshComponent* Grip : GripMarkers)
	{
		if (Grip)
		{
			Grip->SetVisibility(bStampSelected);
		}
	}
}

void AIH_TerrainStampActor::RegenerateBoundingBoxGizmo()
{
	if (!MeshComponent || !BoundingBoxMesh || GripMarkers.Num() != GripMarkerHandles.Num())
	{
		return;
	}
	const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector MeshScale = MeshComponent->GetRelativeScale3D();
	// MeshComponent's own RelativeLocation is always (0,0,0) - its local space is the same as
	// SceneRoot's, so these are directly usable as GripMarkers'/BoundingBoxMesh's own relative
	// (SceneRoot-local) positions with no extra conversion.
	const FVector LocalCenter = Bounds.Origin * MeshScale;
	const FVector LocalExtent = Bounds.BoxExtent * MeshScale;

	// Engine Cube is 100x100x100 (extent 50) - scale relative to that to fill the actual bounds.
	BoundingBoxMesh->SetRelativeLocation(LocalCenter);
	BoundingBoxMesh->SetRelativeScale3D(LocalExtent * 2.0 / 100.0);

	// Grip visual size scales with the stamp's own extent (2026-09-11 fix - a fixed absolute size
	// was invisible against a ~1400m-wide landform), clamped so it never gets silly-huge or -tiny.
	// Engine Sphere is 100x100x100 (diameter 100) - scale relative to that.
	const double AvgExtent = (LocalExtent.X + LocalExtent.Y + LocalExtent.Z) / 3.0;
	const double GripWorldSizeCm = FMath::Clamp(AvgExtent * 0.12, 500.0, 6000.0);
	const FVector GripScale(GripWorldSizeCm / 100.0);

	for (int32 Index = 0; Index < GripMarkers.Num(); ++Index)
	{
		if (!GripMarkers[Index])
		{
			continue;
		}
		const EIHStampGripHandle Handle = GripMarkerHandles[Index];
		const FVector Mask = IHStampGripHandleToSignedAxisMask(Handle);
		// User request (2026-09-11): no grips on the bottom plane - side grips sit at mid-height,
		// the 4 corner grips sit at the TOP (not mid-height, despite corners never touching Z for
		// scaling purposes - Mask.Z is always 0 for a corner). Only Top itself uses Mask.Z.
		const bool bIsCorner = !FMath::IsNearlyZero(Mask.X) && !FMath::IsNearlyZero(Mask.Y);
		FVector LocalPos = LocalCenter;
		LocalPos.X += Mask.X * LocalExtent.X;
		LocalPos.Y += Mask.Y * LocalExtent.Y;
		LocalPos.Z += bIsCorner ? LocalExtent.Z : (Mask.Z * LocalExtent.Z);
		GripMarkers[Index]->SetRelativeLocation(LocalPos);
		GripMarkers[Index]->SetRelativeScale3D(GripScale);
		GripMarkerWorldPositions[Index] = GetActorTransform().TransformPosition(LocalPos);
	}
}

void AIH_TerrainStampActor::BeginGripDrag(EIHStampGripHandle Handle, const FVector& WorldPoint, bool bSymmetric)
{
	if (Handle == EIHStampGripHandle::None || !MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	ActiveGripHandle = Handle;
	bGripDragSymmetric = bSymmetric;
	DragStartMeshScale3D = MeshComponent->GetRelativeScale3D();
	DragStartWorldPoint = WorldPoint;

	const FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	DragStartLocalExtent = Bounds.BoxExtent * DragStartMeshScale3D;

	if (!bSymmetric)
	{
		// Opposite face/corner's FIXED world position at drag-start - the target UpdateGripDrag's
		// per-tick pin compensation holds steady. Recomputed as a LOCAL point fresh each tick there
		// (from whatever the mesh's extent has grown to), not reused from here - a local offset
		// captured once at the OLD (smaller) extent would never reflect where the opposite face
		// actually renders after the mesh grows.
		const FVector Mask = IHStampGripHandleToSignedAxisMask(Handle);
		const FVector LocalCenter = Bounds.Origin * DragStartMeshScale3D;
		FVector PinLocal = LocalCenter;
		PinLocal.X -= Mask.X * DragStartLocalExtent.X;
		PinLocal.Y -= Mask.Y * DragStartLocalExtent.Y;
		PinLocal.Z -= Mask.Z * DragStartLocalExtent.Z;
		DragPinWorldPosition = GetActorTransform().TransformPosition(PinLocal);
	}
}

void AIH_TerrainStampActor::UpdateGripDrag(const FVector& WorldPointForXY, float ScreenMouseDeltaYForZ)
{
	if (ActiveGripHandle == EIHStampGripHandle::None || !MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}

	const FVector Mask = IHStampGripHandleToSignedAxisMask(ActiveGripHandle);
	const FVector UnscaledExtent = MeshComponent->GetStaticMesh()->GetBounds().BoxExtent;

	FVector RawLocalDelta = FVector::ZeroVector;
	if (ActiveGripHandle == EIHStampGripHandle::Top)
	{
		// Vertical screen-drag, same technique as free-mouse camera pan
		// (IH_Cube2FlyPlayerController's MouseDragDelta.Y * cm-per-pixel elsewhere in that file) - a
		// horizontal terrain-surface raycast can't carry a vertical delta, so Top drives Z directly
		// from raw screen pixels instead. Dragging the mouse UP (negative screen Y) grows height.
		RawLocalDelta.Z = -ScreenMouseDeltaYForZ * TerrainStampHeightDragCmPerPixel;
	}
	else
	{
		// InverseTransformVECTOR (not two positions inverse-transformed and subtracted) so this
		// stays correct even mid-drag, after the pin-compensation below has already shifted the
		// actor's own location this frame - mirrors the old lab project's exact BoundingBoxGrip math.
		RawLocalDelta = GetActorTransform().InverseTransformVector(WorldPointForXY - DragStartWorldPoint);
	}

	FVector NewScale = DragStartMeshScale3D;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::IsNearlyZero(Mask[Axis]) || UnscaledExtent[Axis] <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const double SignedDelta = RawLocalDelta[Axis] * Mask[Axis];
		const double MinExtent = UnscaledExtent[Axis] * MinStampScaleFactor;
		const double MaxExtent = UnscaledExtent[Axis] * MaxStampScaleFactor;

		if (ActiveGripHandle == EIHStampGripHandle::Top)
		{
			// Top's delta is an incremental per-tick request (built from a per-tick screen delta),
			// not "total distance from drag start" like the raycast-driven axes below - fold it onto
			// the CURRENT extent rather than DragStartLocalExtent.
			const double CurrentExtent = UnscaledExtent[Axis] * MeshComponent->GetRelativeScale3D()[Axis];
			const double NewExtent = FMath::Clamp(CurrentExtent + SignedDelta, MinExtent, MaxExtent);
			NewScale[Axis] = NewExtent / UnscaledExtent[Axis];
		}
		else if (DragStartLocalExtent[Axis] > KINDA_SMALL_NUMBER)
		{
			const double NewExtent = FMath::Clamp(DragStartLocalExtent[Axis] + SignedDelta, MinExtent, MaxExtent);
			NewScale[Axis] = DragStartMeshScale3D[Axis] * (NewExtent / DragStartLocalExtent[Axis]);
		}
	}

	// 2026-09-18 (user request): the per-axis Min/MaxStampScaleFactor clamp above bounds absolute
	// size but not PROPORTION - cap how far any two axes' scale factors may drift apart from each
	// other (relative to the mesh's own natural shape, not raw extent - a naturally elongated mesh
	// keeps its own aspect ratio untouched by this). Fixed axis order (X, Y, Z) so a two-axis
	// corner-grip drag's second axis is clamped against the first axis's ALREADY-finalized value,
	// guaranteeing every pairwise ratio holds once the loop completes.
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::IsNearlyZero(Mask[Axis]) || UnscaledExtent[Axis] <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		for (int32 OtherAxis = 0; OtherAxis < 3; ++OtherAxis)
		{
			if (OtherAxis == Axis || UnscaledExtent[OtherAxis] <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const double OtherScale = NewScale[OtherAxis];
			if (OtherScale <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			NewScale[Axis] = FMath::Clamp(
				NewScale[Axis], OtherScale / MaxStampAxisRatio, OtherScale * MaxStampAxisRatio);
		}
	}

	MeshComponent->SetRelativeScale3D(NewScale);

	if (!bGripDragSymmetric)
	{
		// 2026-09-11 bug fix: this MUST recompute the pin point fresh from the mesh's just-applied
		// NewScale, not reuse the fixed local offset captured in BeginGripDrag. That offset was
		// computed from the OLD (drag-start) extent, so re-transforming the SAME fixed local point
		// through the actor's transform always gave back the SAME world position turn after turn
		// (nothing about that calculation depended on the new, larger extent) - the compensation
		// shift was silently always zero, so the object grew symmetrically about its pivot every
		// time regardless of Ctrl, instead of pinning the opposite face/corner as intended.
		const FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		const FVector NewLocalExtent = Bounds.BoxExtent * NewScale;
		const FVector NewLocalCenter = Bounds.Origin * NewScale;
		FVector CurrentPinLocal = NewLocalCenter;
		CurrentPinLocal.X -= Mask.X * NewLocalExtent.X;
		CurrentPinLocal.Y -= Mask.Y * NewLocalExtent.Y;
		CurrentPinLocal.Z -= Mask.Z * NewLocalExtent.Z;
		const FVector CurrentPinWorldPos = GetActorTransform().TransformPosition(CurrentPinLocal);
		SetActorLocation(GetActorLocation() + (DragPinWorldPosition - CurrentPinWorldPos));
	}

	RegenerateBoundingBoxGizmo();
}

void AIH_TerrainStampActor::EndGripDrag()
{
	ActiveGripHandle = EIHStampGripHandle::None;
	bGripDragSymmetric = false;
}
