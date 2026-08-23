// Copyright Invisible Hand. All Rights Reserved.

#include "IH_TerrainStampLibrary.h"

int32 FIHTerrainStampLibrary::ApplyStampToHeightGrid(
	FIHCellHeightmapGrid& /*InOutGrid*/,
	const FIHTerrainStampDefinition& /*Definition*/,
	const FVector2D& /*CenterLocalCm*/,
	float /*RadiusKm*/,
	float /*AmplitudeAzgaar*/,
	bool /*bInvertHeight*/,
	float /*RotationDeg*/)
{
	return 0;
}

void FIHTerrainStampLibrary::BuildStampPreviewProcMesh(
	const FIHTerrainStampDefinition& /*Definition*/,
	float RadiusKm,
	float /*AmplitudeAzgaar*/,
	bool /*bInvertHeight*/,
	float BaseZCm,
	float TopZCm,
	float /*RotationDeg*/,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FLinearColor>& OutVertexColors)
{
	OutVertices.Reset();
	OutTriangles.Reset();
	OutNormals.Reset();
	OutVertexColors.Reset();
	const float R = FMath::Max(100.f, RadiusKm * 100000.f);
	const float Z0 = BaseZCm;
	const float Z1 = TopZCm;
	OutVertices = {
		FVector(-R, -R, Z0), FVector(R, -R, Z0), FVector(R, R, Z0), FVector(-R, R, Z0),
		FVector(-R, -R, Z1), FVector(R, -R, Z1), FVector(R, R, Z1), FVector(-R, R, Z1)
	};
	OutTriangles = { 0,1,2, 0,2,3, 4,6,5, 4,7,6 };
	OutNormals.Init(FVector::UpVector, OutVertices.Num());
	OutVertexColors.Init(FLinearColor::Gray, OutVertices.Num());
}

void FIHTerrainStampLibrary::BuildSurfaceConformingPreviewProcMesh(
	const FIHTerrainStampDefinition& Definition,
	float RadiusKm,
	float AmplitudeAzgaar,
	bool bInvertHeight,
	float RotationDeg,
	const TFunctionRef<float(float, float)>& /*SampleSurfaceZCm*/,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FLinearColor>& OutVertexColors)
{
	BuildStampPreviewProcMesh(Definition, RadiusKm, AmplitudeAzgaar, bInvertHeight, 0.f, 200.f, RotationDeg,
		OutVertices, OutTriangles, OutNormals, OutVertexColors);
}
