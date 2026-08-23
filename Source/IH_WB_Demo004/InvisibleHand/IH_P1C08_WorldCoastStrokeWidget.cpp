// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_WorldCoastStrokeWidget.h"
#include "IH_P1C08_MinimapCoastline.h"
#include "IH_P1C08_MinimapSubsystem.h"
#include "IH_P1C08_MinimapTypes.h"
#include "IH_Cube2FlyPlayerController.h"
#include "IHInvisibleHandDesignSpec.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SViewport.h"

namespace
{
	bool ProjectWorldToWidgetLocal(
		const AIH_Cube2FlyPlayerController* PC,
		const FGeometry& WidgetGeometry,
		const FVector& WorldPos,
		FVector2D& OutWidgetLocal)
	{
		if (!PC)
		{
			return false;
		}

		FVector2D ViewportPos;
		if (!PC->ProjectWorldLocationToScreen(WorldPos, ViewportPos, false))
		{
			return false;
		}

		FVector2D AbsolutePos = ViewportPos;
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
			{
				if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())
				{
					AbsolutePos = ViewportWidget->GetTickSpaceGeometry().GetAbsolutePosition() + ViewportPos;
				}
			}
		}

		OutWidgetLocal = WidgetGeometry.AbsoluteToLocal(AbsolutePos);
		return true;
	}

	float ResolveStrokeZoomFactor(
		const UIH_P1C08_MinimapSubsystem* Subsystem,
		const APawn* ViewPawn)
	{
		if (Subsystem && Subsystem->IsMinimapOpen())
		{
			return Subsystem->GetZoomFactor();
		}

		if (ViewPawn)
		{
			const float AltitudeM = ViewPawn->GetActorLocation().Z / 100.f;
			return FMath::Clamp(1.f - (AltitudeM - 30.f) / 350.f, 0.15f, 1.f);
		}

		return 0.5f;
	}

	void AddScreenPointIfNew(TArray<FVector2D>& OutScreen, const FVector2D& ScreenPt)
	{
		if (OutScreen.Num() == 0 || !OutScreen.Last().Equals(ScreenPt, 0.5f))
		{
			OutScreen.Add(ScreenPt);
		}
	}

	void AppendProjectedWorldEdge(
		const AIH_Cube2FlyPlayerController* PC,
		const FGeometry& WidgetGeometry,
		const FVector2D& WorldA,
		const FVector2D& WorldB,
		const float WorldZCm,
		const float MaxScreenSegmentPx,
		TArray<FVector2D>& OutScreen,
		const bool bIncludeFirstPoint,
		const int32 Depth)
	{
		constexpr int32 MaxDepth = 10;
		FVector2D ScreenA;
		FVector2D ScreenB;
		const bool bProjA = ProjectWorldToWidgetLocal(
			PC, WidgetGeometry, FVector(WorldA.X, WorldA.Y, WorldZCm), ScreenA);
		const bool bProjB = ProjectWorldToWidgetLocal(
			PC, WidgetGeometry, FVector(WorldB.X, WorldB.Y, WorldZCm), ScreenB);

		if (bProjA && bProjB)
		{
			const float ScreenLen = FVector2D::Distance(ScreenA, ScreenB);
			if (ScreenLen > MaxScreenSegmentPx && Depth < MaxDepth)
			{
				const FVector2D WorldMid = (WorldA + WorldB) * 0.5f;
				AppendProjectedWorldEdge(
					PC, WidgetGeometry, WorldA, WorldMid, WorldZCm, MaxScreenSegmentPx,
					OutScreen, bIncludeFirstPoint, Depth + 1);
				AppendProjectedWorldEdge(
					PC, WidgetGeometry, WorldMid, WorldB, WorldZCm, MaxScreenSegmentPx,
					OutScreen, false, Depth + 1);
				return;
			}

			if (bIncludeFirstPoint)
			{
				AddScreenPointIfNew(OutScreen, ScreenA);
			}
			AddScreenPointIfNew(OutScreen, ScreenB);
			return;
		}

		if (Depth >= MaxDepth)
		{
			return;
		}

		const FVector2D WorldMid = (WorldA + WorldB) * 0.5f;
		AppendProjectedWorldEdge(
			PC, WidgetGeometry, WorldA, WorldMid, WorldZCm, MaxScreenSegmentPx,
			OutScreen, bIncludeFirstPoint, Depth + 1);
		AppendProjectedWorldEdge(
			PC, WidgetGeometry, WorldMid, WorldB, WorldZCm, MaxScreenSegmentPx,
			OutScreen, false, Depth + 1);
	}

	void DrawPolylineScreen(
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FGeometry& Geometry,
		const TArray<FVector2D>& LocalPoints,
		const FLinearColor& Color,
		const float Thickness,
		const bool bCloseLoop)
	{
		if (LocalPoints.Num() < 2)
		{
			return;
		}

		TArray<FVector2D> LinePoints;
		LinePoints.Reserve(LocalPoints.Num() + 1);
		for (const FVector2D& Point : LocalPoints)
		{
			LinePoints.Add(Point);
		}
		if (bCloseLoop && !LocalPoints[0].Equals(LocalPoints.Last(), KINDA_SMALL_NUMBER))
		{
			LinePoints.Add(LocalPoints[0]);
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}
}

void UIH_P1C08_WorldCoastStrokeWidget::InitializeOverlay(
	UIH_P1C08_MinimapSubsystem* InSubsystem,
	AIH_Cube2FlyPlayerController* InPC)
{
	Subsystem = InSubsystem;
	PlayerController = InPC;
}

void UIH_P1C08_WorldCoastStrokeWidget::RequestRepaint()
{
	bOverlayRepaintPending = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UIH_P1C08_WorldCoastStrokeWidget::BuildProjectedScreenLoop(
	const TArray<FVector2D>& CoastlineWorld,
	const FGeometry& WidgetGeometry,
	const float StrokeZCm,
	const float ZoomFactor,
	TArray<FVector2D>& OutStrokeScreen) const
{
	OutStrokeScreen.Reset();
	if (!PlayerController || CoastlineWorld.Num() < 3)
	{
		return;
	}

	TArray<FVector2D> DrawWorld = CoastlineWorld;
	if (DrawWorld.Num() > IH_P1C08_Minimap::CoastMinimapMaxVerticesPerFeature)
	{
		TArray<FVector2D> DecimatedWorld;
		IH_P1C08_MinimapCoastline::CapClosedPolylineUniformCount(
			DrawWorld,
			IH_P1C08_Minimap::CoastMinimapMaxVerticesPerFeature,
			DecimatedWorld);
		DrawWorld = MoveTemp(DecimatedWorld);
	}

	const float MaxScreenEdgePx = FMath::Max(
		IH_P1C08_Minimap::CoastMinimapMaxDrawSegmentPx * 12.f,
		32.f);

	TArray<FVector2D> ScreenPts;
	ScreenPts.Reserve(DrawWorld.Num() * 2);
	const int32 NumPoints = DrawWorld.Num();
	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		AppendProjectedWorldEdge(
			PlayerController,
			WidgetGeometry,
			DrawWorld[PointIdx],
			DrawWorld[(PointIdx + 1) % NumPoints],
			StrokeZCm,
			MaxScreenEdgePx,
			ScreenPts,
			PointIdx == 0,
			0);
	}

	if (ScreenPts.Num() < 3)
	{
		return;
	}

	IH_P1C08_MinimapCoastline::PrepareCoastlineForScreenOverlayDraw(ScreenPts, ZoomFactor, OutStrokeScreen);
	IH_P1C08_MinimapCoastline::RemoveHairpinBacktrackVertices(
		OutStrokeScreen,
		IHInvisibleHandSpec::CoastHairpinBacktrackDot);
}

void UIH_P1C08_WorldCoastStrokeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IHInvisibleHandSpec::IsCoastStrokeWorldScreenOverlayEnabled()
		|| !Subsystem || !PlayerController)
	{
		return;
	}

	if (Subsystem->IsMinimapOpen())
	{
		return;
	}

	if (PlayerController->ShouldSuspendWorldCoastStrokeOverlay())
	{
		bOverlayRepaintPending = true;
		return;
	}

	const APawn* ViewPawn = PlayerController->GetPawn();
	if (!ViewPawn)
	{
		return;
	}

	const FVector CameraLocation = ViewPawn->GetActorLocation();
	const FRotator CameraRotation = PlayerController->GetControlRotation();
	const float DistMovedCm = FVector::Dist(CameraLocation, CachedOverlayCameraLocation);
	const float YawDeltaDeg = FMath::Abs(
		FMath::FindDeltaAngleDegrees(CachedOverlayCameraRotation.Yaw, CameraRotation.Yaw));
	const float PitchDeltaDeg = FMath::Abs(
		FMath::FindDeltaAngleDegrees(CachedOverlayCameraRotation.Pitch, CameraRotation.Pitch));

	static constexpr float MinCameraMoveCm = 5000.f;
	static constexpr float MinCameraAngleDeg = 5.f;

	if (!bOverlayRepaintPending
		&& DistMovedCm < MinCameraMoveCm
		&& YawDeltaDeg < MinCameraAngleDeg
		&& PitchDeltaDeg < MinCameraAngleDeg)
	{
		return;
	}

	CachedOverlayCameraLocation = CameraLocation;
	CachedOverlayCameraRotation = CameraRotation;
	bOverlayRepaintPending = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UIH_P1C08_WorldCoastStrokeWidget::DrawClosedPolylineScreen(
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FGeometry& Geometry,
	const TArray<FVector2D>& LocalPoints,
	const FLinearColor& Color,
	const float Thickness) const
{
	DrawPolylineScreen(OutDrawElements, LayerId, Geometry, LocalPoints, Color, Thickness, true);
}

int32 UIH_P1C08_WorldCoastStrokeWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (!IHInvisibleHandSpec::IsCoastStrokeWorldScreenOverlayEnabled()
		|| !Subsystem || !PlayerController)
	{
		return MaxLayer;
	}

	if (Subsystem->IsMinimapOpen() || PlayerController->ShouldSuspendWorldCoastStrokeOverlay())
	{
		return MaxLayer;
	}

	const float StrokeZCm = IHInvisibleHandSpec::CoastStrokeScreenProjectZCm;
	const APawn* ViewPawn = PlayerController->GetPawn();
	const float ZoomFactor = ResolveStrokeZoomFactor(Subsystem, ViewPawn);

	float ThicknessPx = 3.5f;
	if (ViewPawn)
	{
		const float AltitudeM = ViewPawn->GetActorLocation().Z / 100.f;
		ThicknessPx = FMath::Clamp(2.5f + AltitudeM * 0.012f, 3.f, 8.f);
	}

	const FLinearColor StrokeColor = IHInvisibleHandSpec::CoastStrokeColor;
	for (const TArray<FVector2D>& CoastlineWorld : Subsystem->GetCoastlinePolylinesWorld())
	{
		TArray<FVector2D> StrokeScreen;
		BuildProjectedScreenLoop(CoastlineWorld, AllottedGeometry, StrokeZCm, ZoomFactor, StrokeScreen);
		if (StrokeScreen.Num() >= 3)
		{
			DrawClosedPolylineScreen(
				OutDrawElements, MaxLayer + 1, AllottedGeometry, StrokeScreen, StrokeColor, ThicknessPx);
		}
	}

	return MaxLayer + 1;
}
