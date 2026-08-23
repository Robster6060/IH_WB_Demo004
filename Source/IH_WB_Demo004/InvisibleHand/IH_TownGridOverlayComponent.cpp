// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_TownGridOverlayComponent.h"

#include "IHInvisibleHandDesignSpec.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_P1C07_SelectableShip.h"
#include "IH_TownGridManager.h"

#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "WaterBodyActor.h"

namespace IH_TownGridOverlayPrivate
{
	static constexpr float TerrainTraceHalfHeightCm = 500000.f;

	static bool IsTerrainOverlayHit(const FHitResult& Hit)
	{
		const AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor->IsA(AWaterBody::StaticClass()))
		{
			return false;
		}
		if (HitActor->Implements<UIH_P1C07_SelectableShip>())
		{
			return false;
		}
		return true;
	}

	static bool TrySampleTerrainSurface(
		UWorld* World,
		const FVector2D& WorldXY,
		float ReferenceZ,
		float LiftCm,
		const AActor* IgnoreActor,
		FVector& OutLocation)
	{
		if (!World)
		{
			OutLocation = FVector(WorldXY.X, WorldXY.Y, ReferenceZ + LiftCm);
			return false;
		}

		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UIH_P1C07_IslandCollisionSubsystem* IslandCollision =
				GameInstance->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>())
			{
				if (IslandCollision->TrySampleIslandSurfaceAtXY(WorldXY, ReferenceZ, LiftCm, IgnoreActor, OutLocation))
				{
					return true;
				}
			}
		}

		const FVector TraceStart(WorldXY.X, WorldXY.Y, ReferenceZ + TerrainTraceHalfHeightCm);
		const FVector TraceEnd(WorldXY.X, WorldXY.Y, ReferenceZ - TerrainTraceHalfHeightCm);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TownGridOverlayTerrain), false, IgnoreActor);
		Params.bTraceComplex = true;

		TArray<FHitResult> Hits;
		if (World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
			for (const FHitResult& Hit : Hits)
			{
				if (!IsTerrainOverlayHit(Hit))
				{
					continue;
				}

				OutLocation = Hit.ImpactPoint + Hit.ImpactNormal * LiftCm;
				return true;
			}
		}

		OutLocation = FVector(WorldXY.X, WorldXY.Y, ReferenceZ + LiftCm);
		return false;
	}

	static FVector LocalXYToWorld(const FVector& Center, float YawDeg, const FVector2D& LocalXY)
	{
		const float Rad = FMath::DegreesToRadians(YawDeg);
		const float CosA = FMath::Cos(Rad);
		const float SinA = FMath::Sin(Rad);
		return FVector(
			Center.X + LocalXY.X * CosA - LocalXY.Y * SinA,
			Center.Y + LocalXY.X * SinA + LocalXY.Y * CosA,
			Center.Z);
	}

	static void DrawConformedDebugPolyline(
		UWorld* World,
		const FVector& StartWorld,
		const FVector& EndWorld,
		float ReferenceZ,
		float LiftCm,
		float SampleSpacingCm,
		const AActor* IgnoreActor,
		const FColor& Color,
		float ThicknessWorld)
	{
		const FVector2D StartXY(StartWorld.X, StartWorld.Y);
		const FVector2D EndXY(EndWorld.X, EndWorld.Y);
		const float Length = FVector2D::Distance(StartXY, EndXY);
		const int32 NumSteps = FMath::Max(2, FMath::CeilToInt(Length / FMath::Max(SampleSpacingCm, 1.f)) + 1);

		FVector PrevPoint = FVector::ZeroVector;
		bool bHasPrev = false;
		for (int32 StepIndex = 0; StepIndex < NumSteps; ++StepIndex)
		{
			const float Alpha = (NumSteps <= 1) ? 0.f : static_cast<float>(StepIndex) / static_cast<float>(NumSteps - 1);
			const FVector2D SampleXY = FMath::Lerp(StartXY, EndXY, Alpha);

			FVector SamplePoint = FVector::ZeroVector;
			TrySampleTerrainSurface(World, SampleXY, ReferenceZ, LiftCm, IgnoreActor, SamplePoint);

			if (bHasPrev)
			{
				DrawDebugLine(
					World,
					PrevPoint,
					SamplePoint,
					Color,
					false,
					-1.f,
					0,
					ThicknessWorld);
			}

			PrevPoint = SamplePoint;
			bHasPrev = true;
		}
	}

	static void DrawConformedDebugRect(
		UWorld* World,
		const FVector& CenterWorld,
		const FVector2D& HalfExtentLocalCm,
		float YawDeg,
		float ReferenceZ,
		float LiftCm,
		float SampleSpacingCm,
		const AActor* IgnoreActor,
		const FColor& Color,
		float ThicknessWorld)
	{
		const FVector CornersLocal[4] = {
			LocalXYToWorld(CenterWorld, YawDeg, FVector2D(-HalfExtentLocalCm.X, -HalfExtentLocalCm.Y)),
			LocalXYToWorld(CenterWorld, YawDeg, FVector2D(HalfExtentLocalCm.X, -HalfExtentLocalCm.Y)),
			LocalXYToWorld(CenterWorld, YawDeg, FVector2D(HalfExtentLocalCm.X, HalfExtentLocalCm.Y)),
			LocalXYToWorld(CenterWorld, YawDeg, FVector2D(-HalfExtentLocalCm.X, HalfExtentLocalCm.Y)),
		};

		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			DrawConformedDebugPolyline(
				World,
				CornersLocal[EdgeIndex],
				CornersLocal[(EdgeIndex + 1) % 4],
				ReferenceZ,
				LiftCm,
				SampleSpacingCm,
				IgnoreActor,
				Color,
				ThicknessWorld);
		}
	}
}

UTownGridOverlayComponent::UTownGridOverlayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UTownGridOverlayComponent::RebuildOverlay(const FTownGridOverlayData& OverlayData)
{
	CachedOverlay = OverlayData;
}

void UTownGridOverlayComponent::DrawBuildFootprintPreview(
	UWorld* World,
	const FVector& SurfaceCenterWorld,
	const FVector2D& FootprintHalfExtentCm,
	float YawDeg,
	const AActor* IgnoreActor)
{
	if (!World)
	{
		return;
	}

	const float ReferenceZ = SurfaceCenterWorld.Z;
	constexpr float LiftCm = 25.f;
	const float SampleSpacingCm = AIH_TownGridManager::ModuleSizeCm * 0.25f;
	const FColor OutlineColor(0, 200, 255, 255);
	const FColor FillColor(30, 140, 255, 70);

	FVector CellCenter = SurfaceCenterWorld;
	CellCenter.Z = ReferenceZ + LiftCm + 4.f;
	const FVector Extent(
		FMath::Max(FootprintHalfExtentCm.X, 50.f),
		FMath::Max(FootprintHalfExtentCm.Y, 50.f),
		20.f);
	const FQuat Orientation(FRotator(0.f, YawDeg, 0.f));
	DrawDebugBox(World, CellCenter, Extent, Orientation, FillColor, false, -1.f, 0, 2.f);

	IH_TownGridOverlayPrivate::DrawConformedDebugRect(
		World,
		SurfaceCenterWorld,
		FootprintHalfExtentCm,
		YawDeg,
		ReferenceZ,
		LiftCm,
		SampleSpacingCm,
		IgnoreActor,
		OutlineColor,
		10.f);
}

void UTownGridOverlayComponent::DrawOverlayData(
	UWorld* World,
	const FTownGridOverlayData& OverlayData,
	const FVector& Center,
	float YawDeg,
	float OverlayLiftCm,
	bool bPreviewAlpha,
	const AActor* IgnoreActor,
	bool bConformToTerrain)
{
	if (!World)
	{
		return;
	}

	const float ReferenceZ = Center.Z;
	const float PreviewAlphaScale = bPreviewAlpha ? 0.65f : 1.f;
	const float SampleSpacingCm = AIH_TownGridManager::ModuleSizeCm * 0.25f;

	for (const FTownGridOverlaySegment& Segment : OverlayData.Segments)
	{
		FLinearColor LineColor = IHInvisibleHandSpec::GetTownGridRoadOverlayColor(Segment.RoadClass);
		if (Segment.bCommonsParcelLine)
		{
			LineColor = FLinearColor(0.55f, 0.85f, 1.f, 1.f);
		}
		LineColor.A *= PreviewAlphaScale;

		const float BaseThickness = IHInvisibleHandSpec::GetTownGridDebugOverlayLineThicknessCm(Segment.RoadClass);
		const float ThicknessWorld = Segment.bCommonsParcelLine ? BaseThickness * 1.25f : BaseThickness;
		const FColor DrawColor = LineColor.ToFColor(true);

		if (bConformToTerrain)
		{
			IH_TownGridOverlayPrivate::DrawConformedDebugPolyline(
				World,
				Segment.StartWorld,
				Segment.EndWorld,
				ReferenceZ,
				OverlayLiftCm,
				SampleSpacingCm,
				IgnoreActor,
				DrawColor,
				ThicknessWorld);
		}
		else
		{
			FVector Start = Segment.StartWorld;
			FVector End = Segment.EndWorld;
			Start.Z = ReferenceZ + OverlayLiftCm;
			End.Z = ReferenceZ + OverlayLiftCm;
			DrawDebugLine(World, Start, End, DrawColor, false, -1.f, 0, ThicknessWorld);
		}
	}

	for (const FTownGridOverlayCommonsCell& Cell : OverlayData.CommonsCells)
	{
		const uint8 FillAlpha = bPreviewAlpha ? 25 : 40;
		const FColor CommonsFill(102, 191, 255, FillAlpha);

		if (bConformToTerrain)
		{
			IH_TownGridOverlayPrivate::DrawConformedDebugRect(
				World,
				Cell.CenterWorld,
				Cell.HalfExtentLocalCm,
				YawDeg,
				ReferenceZ,
				OverlayLiftCm + 2.f,
				SampleSpacingCm,
				IgnoreActor,
				CommonsFill,
				2.f);
		}
		else
		{
			FVector CellCenter = Cell.CenterWorld;
			CellCenter.Z = ReferenceZ + OverlayLiftCm + 4.f;
			const FVector Extent(Cell.HalfExtentLocalCm.X, Cell.HalfExtentLocalCm.Y, 20.f);
			const FQuat OrientationQuat = FRotator(0.f, YawDeg, 0.f).Quaternion();
			DrawDebugBox(
				World,
				CellCenter,
				Extent,
				OrientationQuat,
				CommonsFill,
				false,
				-1.f,
				0,
				2.f);
		}

#if ENABLE_DRAW_DEBUG
		if (!bPreviewAlpha)
		{
			FVector LabelAnchor = Cell.CenterWorld;
			if (bConformToTerrain)
			{
				IH_TownGridOverlayPrivate::TrySampleTerrainSurface(
					World,
					FVector2D(Cell.CenterWorld.X, Cell.CenterWorld.Y),
					ReferenceZ,
					OverlayLiftCm,
					IgnoreActor,
					LabelAnchor);
			}
			else
			{
				LabelAnchor.Z = ReferenceZ + OverlayLiftCm + 4.f;
			}

			DrawDebugString(
				World,
				LabelAnchor + FVector(0.f, 0.f, 120.f),
				TEXT("CIV+SPD"),
				nullptr,
				FColor::Cyan,
				0.f,
				true,
				1.2f);
		}
#endif
	}
}

void UTownGridOverlayComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShowOverlay)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	const FVector Center = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	const float YawDeg = Owner ? Owner->GetActorRotation().Yaw : 0.f;

	DrawOverlayData(World, CachedOverlay, Center, YawDeg, OverlayLiftCm, false, Owner, true);
}
