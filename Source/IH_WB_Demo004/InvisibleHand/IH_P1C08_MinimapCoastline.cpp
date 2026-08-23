// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C08_MinimapCoastline.h"

#include "IHInvisibleHandDesignSpec.h"
#include "IH_P1C08_MinimapTypes.h"
#include "ProceduralMeshComponent.h"

namespace
{
	void MergeNearbyPoints(TArray<FVector2D>& Points, float ToleranceCm)
	{
		if (Points.Num() < 2)
		{
			return;
		}

		TArray<FVector2D> Merged;
		Merged.Reserve(Points.Num());
		for (const FVector2D& Point : Points)
		{
			bool bDuplicate = false;
			for (const FVector2D& Existing : Merged)
			{
				if (FVector2D::Distance(Point, Existing) <= ToleranceCm)
				{
					bDuplicate = true;
					break;
				}
			}
			if (!bDuplicate)
			{
				Merged.Add(Point);
			}
		}
		Points = MoveTemp(Merged);
	}

	void SortPointsByPolarAngle(TArray<FVector2D>& Points)
	{
		if (Points.Num() < 3)
		{
			return;
		}

		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& Point : Points)
		{
			Centroid += Point;
		}
		Centroid /= static_cast<float>(Points.Num());

		Points.Sort([&Centroid](const FVector2D& A, const FVector2D& B) {
			const float AngleA = FMath::Atan2(A.Y - Centroid.Y, A.X - Centroid.X);
			const float AngleB = FMath::Atan2(B.Y - Centroid.Y, B.X - Centroid.X);
			return AngleA < AngleB;
		});
	}
}

bool IH_P1C08_MinimapCoastline::BuildWaterlinePolylineFromMeshSection(
	UProceduralMeshComponent* MeshComp,
	int32 SectionIndex,
	float WorldZSliceCm,
	TArray<FVector2D>& OutWorldXY,
	float MergeToleranceCm)
{
	OutWorldXY.Reset();
	if (!MeshComp)
	{
		return false;
	}

	FProcMeshSection* Section = MeshComp->GetProcMeshSection(SectionIndex);
	if (!Section || Section->ProcVertexBuffer.Num() == 0 || Section->ProcIndexBuffer.Num() < 3)
	{
		return false;
	}

	MeshComp->UpdateComponentToWorld();
	const FTransform ComponentTransform = MeshComp->GetComponentTransform();
	const TArray<FProcMeshVertex>& Vertices = Section->ProcVertexBuffer;
	const TArray<uint32>& Indices = Section->ProcIndexBuffer;

	auto VertexWorld = [&](uint32 Index) -> FVector {
		return ComponentTransform.TransformPosition(Vertices[Index].Position);
	};

	for (int32 TriIdx = 0; TriIdx + 2 < Indices.Num(); TriIdx += 3)
	{
		const FVector WorldVerts[3] = {
			VertexWorld(Indices[TriIdx]),
			VertexWorld(Indices[TriIdx + 1]),
			VertexWorld(Indices[TriIdx + 2]),
		};

		for (int32 EdgeIdx = 0; EdgeIdx < 3; ++EdgeIdx)
		{
			const FVector& A = WorldVerts[EdgeIdx];
			const FVector& B = WorldVerts[(EdgeIdx + 1) % 3];
			const float DeltaZ = B.Z - A.Z;
			if (FMath::Abs(DeltaZ) <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float TA = A.Z - WorldZSliceCm;
			const float TB = B.Z - WorldZSliceCm;
			if (TA * TB > 0.f)
			{
				continue;
			}

			const float T = (WorldZSliceCm - A.Z) / DeltaZ;
			if (T < 0.f || T > 1.f)
			{
				continue;
			}

			const FVector Hit = FMath::Lerp(A, B, T);
			OutWorldXY.Add(FVector2D(Hit.X, Hit.Y));
		}
	}

	MergeNearbyPoints(OutWorldXY, MergeToleranceCm);
	SortPointsByPolarAngle(OutWorldXY);
	return OutWorldXY.Num() >= 3;
}

void IH_P1C08_MinimapCoastline::DecimatePolylineMapPx(
	const TArray<FVector2D>& InPoints,
	float MinSpacingPx,
	int32 MaxVertices,
	TArray<FVector2D>& OutPoints)
{
	OutPoints.Reset();
	if (InPoints.Num() < 2 || MinSpacingPx <= KINDA_SMALL_NUMBER)
	{
		OutPoints = InPoints;
		return;
	}

	TArray<FVector2D> Loop = InPoints;
	if (Loop.Num() >= 2 && Loop[0].Equals(Loop.Last(), KINDA_SMALL_NUMBER))
	{
		Loop.RemoveAt(Loop.Num() - 1, 1, EAllowShrinking::No);
	}
	if (Loop.Num() < 2)
	{
		OutPoints = InPoints;
		return;
	}

	OutPoints.Reserve(Loop.Num());
	OutPoints.Add(Loop[0]);
	for (int32 i = 1; i < Loop.Num(); ++i)
	{
		if (FVector2D::Distance(Loop[i], OutPoints.Last()) >= MinSpacingPx)
		{
			OutPoints.Add(Loop[i]);
		}
	}

	if (OutPoints.Num() >= 2
		&& FVector2D::Distance(OutPoints.Last(), OutPoints[0]) < MinSpacingPx)
	{
		OutPoints.RemoveAt(OutPoints.Num() - 1, 1, EAllowShrinking::No);
	}

	if (OutPoints.Num() < 3)
	{
		OutPoints = Loop;
	}

	if (MaxVertices > 0 && OutPoints.Num() > MaxVertices)
	{
		TArray<FVector2D> Capped;
		CapClosedPolylineUniformCount(OutPoints, MaxVertices, Capped);
		OutPoints = MoveTemp(Capped);
	}
}

void IH_P1C08_MinimapCoastline::CapClosedPolylineUniformCount(
	const TArray<FVector2D>& InPoints,
	const int32 MaxVertices,
	TArray<FVector2D>& OutPoints)
{
	OutPoints.Reset();
	if (InPoints.Num() < 3 || MaxVertices < 3)
	{
		OutPoints = InPoints;
		return;
	}

	if (InPoints.Num() <= MaxVertices)
	{
		OutPoints = InPoints;
		return;
	}

	OutPoints.Reserve(MaxVertices);
	const int32 N = InPoints.Num();
	for (int32 k = 0; k < MaxVertices; ++k)
	{
		const int32 Idx = static_cast<int32>((static_cast<int64>(k) * static_cast<int64>(N)) / static_cast<int64>(MaxVertices));
		OutPoints.Add(InPoints[FMath::Clamp(Idx, 0, N - 1)]);
	}
}

void IH_P1C08_MinimapCoastline::BuildWorldPolygonFromAzimuthRadiiCm(
	const FVector2D& CenterWorldCm,
	const float YawDegrees,
	const FVector2D& AzimuthOriginLocalCm,
	const TArray<float>& RadiiCm,
	TArray<FVector2D>& OutWorldXY)
{
	OutWorldXY.Reset();
	const int32 NumSamples = RadiiCm.Num();
	if (NumSamples < 3)
	{
		return;
	}

	const float YawRad = FMath::DegreesToRadians(YawDegrees);
	const float CosYaw = FMath::Cos(YawRad);
	const float SinYaw = FMath::Sin(YawRad);
	OutWorldXY.Reserve(NumSamples);
	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Theta = (static_cast<float>(i) / static_cast<float>(NumSamples)) * 2.f * UE_PI;
		const float LocalX = AzimuthOriginLocalCm.X + FMath::Cos(Theta) * RadiiCm[i];
		const float LocalY = AzimuthOriginLocalCm.Y + FMath::Sin(Theta) * RadiiCm[i];
		OutWorldXY.Emplace(
			CenterWorldCm.X + LocalX * CosYaw - LocalY * SinYaw,
			CenterWorldCm.Y + LocalX * SinYaw + LocalY * CosYaw);
	}
}

void IH_P1C08_MinimapCoastline::BuildWorldPolygonFromLocalPolylineCm(
	const FVector2D& CenterWorldCm,
	const float YawDegrees,
	const TArray<FVector2D>& LocalPolylineCm,
	TArray<FVector2D>& OutWorldXY)
{
	OutWorldXY.Reset();
	if (LocalPolylineCm.Num() < 3)
	{
		return;
	}

	const float YawRad = FMath::DegreesToRadians(YawDegrees);
	const float CosYaw = FMath::Cos(YawRad);
	const float SinYaw = FMath::Sin(YawRad);
	OutWorldXY.Reserve(LocalPolylineCm.Num());
	for (const FVector2D& P : LocalPolylineCm)
	{
		OutWorldXY.Emplace(
			CenterWorldCm.X + P.X * CosYaw - P.Y * SinYaw,
			CenterWorldCm.Y + P.X * SinYaw + P.Y * CosYaw);
	}
}

void IH_P1C08_MinimapCoastline::BuildLocalPolylineFromWorldPolygonCm(
	const FVector2D& CenterWorldCm,
	const float YawDegrees,
	const TArray<FVector2D>& WorldPolylineXY,
	TArray<FVector2D>& OutLocalPolylineCm)
{
	OutLocalPolylineCm.Reset();
	if (WorldPolylineXY.Num() < 3)
	{
		return;
	}

	const float YawRad = FMath::DegreesToRadians(-YawDegrees);
	const float CosYaw = FMath::Cos(YawRad);
	const float SinYaw = FMath::Sin(YawRad);
	OutLocalPolylineCm.Reserve(WorldPolylineXY.Num());
	for (const FVector2D& WorldPt : WorldPolylineXY)
	{
		const float Dx = WorldPt.X - CenterWorldCm.X;
		const float Dy = WorldPt.Y - CenterWorldCm.Y;
		OutLocalPolylineCm.Emplace(
			Dx * CosYaw - Dy * SinYaw,
			Dx * SinYaw + Dy * CosYaw);
	}
}

void IH_P1C08_MinimapCoastline::DensifyClosedPolylineForStroke(
	const TArray<FVector2D>& InPoints,
	const float MaxSegmentLen,
	TArray<FVector2D>& OutPoints,
	const int32 MaxOutputVerts)
{
	OutPoints.Reset();
	if (InPoints.Num() < 2 || MaxSegmentLen <= KINDA_SMALL_NUMBER)
	{
		OutPoints = InPoints;
		return;
	}

	const int32 NumPoints = InPoints.Num();
	OutPoints.Reserve(MaxOutputVerts > 0 ? MaxOutputVerts : NumPoints * 2);
	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		if (MaxOutputVerts > 0 && OutPoints.Num() >= MaxOutputVerts)
		{
			break;
		}

		const FVector2D& Start = InPoints[PointIdx];
		const FVector2D& End = InPoints[(PointIdx + 1) % NumPoints];
		OutPoints.Add(Start);

		const float SegmentLength = FVector2D::Distance(Start, End);
		if (SegmentLength <= MaxSegmentLen)
		{
			continue;
		}

		const int32 Steps = FMath::Min(FMath::CeilToInt(SegmentLength / MaxSegmentLen), 128);
		for (int32 Step = 1; Step < Steps; ++Step)
		{
			if (MaxOutputVerts > 0 && OutPoints.Num() >= MaxOutputVerts)
			{
				break;
			}

			const float T = static_cast<float>(Step) / static_cast<float>(Steps);
			OutPoints.Add(FMath::Lerp(Start, End, T));
		}
	}

	if (OutPoints.Num() < 3)
	{
		OutPoints = InPoints;
	}
}

void IH_P1C08_MinimapCoastline::PrepareCoastlineForMinimapDraw(
	const TArray<FVector2D>& LocalCoastline,
	const float ZoomFactor,
	TArray<FVector2D>& OutStrokeCoastline)
{
	TArray<FVector2D> DrawCoastline;
	const bool bAuthorityFidelity =
		LocalCoastline.Num() >= 3
		&& LocalCoastline.Num() <= IHInvisibleHandSpec::CoastRenderPolyVerts;
	if (bAuthorityFidelity)
	{
		DrawCoastline = LocalCoastline;
	}
	else
	{
		const float MinSpacingPx = IH_P1C08_Minimap::ResolveCoastMinVertexSpacingPx(ZoomFactor);
		DecimatePolylineMapPx(
			LocalCoastline,
			MinSpacingPx,
			IH_P1C08_Minimap::CoastMinimapMaxVerticesPerFeature,
			DrawCoastline);
	}

	DensifyClosedPolylineForStroke(
		DrawCoastline,
		IH_P1C08_Minimap::CoastMinimapMaxDrawSegmentPx,
		OutStrokeCoastline);
	// Display-only: soften ContourGold marching-squares stairs. Not used on SeaRoots bands.
	PolishClosedPolylineCornersForStroke(OutStrokeCoastline);
}

void IH_P1C08_MinimapCoastline::PrepareSeaRootsBandRingForMinimapDraw(
	const TArray<FVector2D>& LocalBandRing,
	TArray<FVector2D>& OutStrokeRing)
{
	// Cap first (keeps closed topology), then densify uncapped.
	// Passing MaxOutputVerts into densify aborts mid-ring (640→384) and DrawClosedPolyline
	// chords last→first through land. No hairpin/Chaikin — SSOT consumer only.
	TArray<FVector2D> CappedRing;
	CapClosedPolylineUniformCount(
		LocalBandRing,
		IH_P1C08_Minimap::CoastMinimapMaxStrokeOutputVerts,
		CappedRing);
	DensifyClosedPolylineForStroke(
		CappedRing,
		IH_P1C08_Minimap::CoastMinimapMaxDrawSegmentPx,
		OutStrokeRing);
}

void IH_P1C08_MinimapCoastline::PrepareCoastlineForScreenOverlayDraw(
	const TArray<FVector2D>& ScreenCoastline,
	const float ZoomFactor,
	TArray<FVector2D>& OutStrokeCoastline)
{
	const int32 MaxVerts = IH_P1C08_Minimap::CoastWorldOverlayMaxScreenVertsPerIsland;
	TArray<FVector2D> DrawCoastline;
	const float MinSpacingPx = IH_P1C08_Minimap::ResolveCoastMinVertexSpacingPx(ZoomFactor);
	DecimatePolylineMapPx(ScreenCoastline, MinSpacingPx, MaxVerts, DrawCoastline);
	DensifyClosedPolylineForStroke(
		DrawCoastline,
		IH_P1C08_Minimap::CoastMinimapMaxDrawSegmentPx,
		OutStrokeCoastline,
		MaxVerts);

	if (OutStrokeCoastline.Num() > MaxVerts)
	{
		TArray<FVector2D> Capped;
		CapClosedPolylineUniformCount(OutStrokeCoastline, MaxVerts, Capped);
		OutStrokeCoastline = MoveTemp(Capped);
	}
}

void IH_P1C08_MinimapCoastline::RemoveHairpinBacktrackVertices(
	TArray<FVector2D>& InOutPoints,
	const float MaxBacktrackDot)
{
	if (InOutPoints.Num() < 4)
	{
		return;
	}

	if (InOutPoints.Num() >= 2 && InOutPoints[0].Equals(InOutPoints.Last(), KINDA_SMALL_NUMBER))
	{
		InOutPoints.RemoveAt(InOutPoints.Num() - 1, 1, EAllowShrinking::No);
	}

	bool bRemoved = true;
	while (bRemoved && InOutPoints.Num() >= 4)
	{
		bRemoved = false;
		const int32 N = InOutPoints.Num();
		for (int32 VertexIdx = 0; VertexIdx < N; ++VertexIdx)
		{
			const int32 PrevIdx = (VertexIdx + N - 1) % N;
			const int32 NextIdx = (VertexIdx + 1) % N;
			const FVector2D InEdge = InOutPoints[VertexIdx] - InOutPoints[PrevIdx];
			const FVector2D OutEdge = InOutPoints[NextIdx] - InOutPoints[VertexIdx];
			const float InLen = InEdge.Size();
			const float OutLen = OutEdge.Size();
			if (InLen < KINDA_SMALL_NUMBER || OutLen < KINDA_SMALL_NUMBER)
			{
				InOutPoints.RemoveAt(VertexIdx, 1, EAllowShrinking::No);
				bRemoved = true;
				break;
			}

			const float Dot = FVector2D::DotProduct(InEdge / InLen, OutEdge / OutLen);
			if (Dot < MaxBacktrackDot)
			{
				InOutPoints.RemoveAt(VertexIdx, 1, EAllowShrinking::No);
				bRemoved = true;
				break;
			}
		}
	}
}

namespace
{
	void ChaikinClosedPolyline2D(
		TArray<FVector2D>& InOutPoints,
		const int32 Iterations,
		const float CutRatio)
	{
		if (InOutPoints.Num() < 4 || Iterations <= 0)
		{
			return;
		}

		if (InOutPoints.Num() >= 2 && InOutPoints[0].Equals(InOutPoints.Last(), KINDA_SMALL_NUMBER))
		{
			InOutPoints.RemoveAt(InOutPoints.Num() - 1, 1, EAllowShrinking::No);
		}

		const float T = FMath::Clamp(CutRatio, 0.05f, 0.45f);
		for (int32 Iter = 0; Iter < Iterations; ++Iter)
		{
			const int32 N = InOutPoints.Num();
			if (N < 3)
			{
				break;
			}

			TArray<FVector2D> Smoothed;
			Smoothed.Reserve(N * 2);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& A = InOutPoints[i];
				const FVector2D& B = InOutPoints[(i + 1) % N];
				Smoothed.Add(FMath::Lerp(A, B, T));
				Smoothed.Add(FMath::Lerp(A, B, 1.f - T));
			}
			InOutPoints = MoveTemp(Smoothed);
		}
	}
}

void IH_P1C08_MinimapCoastline::PolishClosedPolylineCornersForStroke(TArray<FVector2D>& InOutPoints)
{
	if (InOutPoints.Num() < 4)
	{
		return;
	}

	RemoveHairpinBacktrackVertices(InOutPoints, IHInvisibleHandSpec::CoastHairpinBacktrackDot);
	ChaikinClosedPolyline2D(InOutPoints, 1, 0.22f);
}

void IH_P1C08_MinimapCoastline::PrepareCoastlineForWorldStrokeDraw(
	const TArray<FVector2D>& WorldCoastlineCm,
	const float MaxSegmentWorldCm,
	TArray<FVector2D>& OutStrokeWorldCm)
{
	// Densify-only consumer of IslandActor MainCoast / ContourGold (bake Chaikin×2 @0.22).
	// Must not reintroduce raw MS stairs; do not Chaikin here (bake owns XY soften).
	OutStrokeWorldCm = WorldCoastlineCm;
	if (OutStrokeWorldCm.Num() < 3)
	{
		return;
	}

	TArray<FVector2D> DensifiedCm;
	DensifyClosedPolylineForStroke(
		OutStrokeWorldCm,
		FMath::Max(MaxSegmentWorldCm, 50.f),
		DensifiedCm);

	if (DensifiedCm.Num() >= 3)
	{
		OutStrokeWorldCm = MoveTemp(DensifiedCm);
	}

	// Display-only hairpin trim — does not write back to MainCoastPolylineLocalCm.
	RemoveHairpinBacktrackVertices(OutStrokeWorldCm, IHInvisibleHandSpec::CoastHairpinBacktrackDot);

	if (OutStrokeWorldCm.Num() < 3)
	{
		OutStrokeWorldCm = WorldCoastlineCm;
	}
}
