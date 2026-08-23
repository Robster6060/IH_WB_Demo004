// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "FIHTerrainStampTypes.h"
#include "IHCellHeightmapTypes.h"

class UProceduralMeshComponent;

/** Stub library — stamp height edits deferred until Azgaar IslandMesh signoff. */
class IH_WB_DEMO004_API FIHTerrainStampLibrary
{
public:
	static int32 ApplyStampToHeightGrid(
		FIHCellHeightmapGrid& InOutGrid,
		const FIHTerrainStampDefinition& Definition,
		const FVector2D& CenterLocalCm,
		float RadiusKm,
		float AmplitudeAzgaar,
		bool bInvertHeight,
		float RotationDeg = 0.f);

	static void BuildStampPreviewProcMesh(
		const FIHTerrainStampDefinition& Definition,
		float RadiusKm,
		float AmplitudeAzgaar,
		bool bInvertHeight,
		float BaseZCm,
		float TopZCm,
		float RotationDeg,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FLinearColor>& OutVertexColors);

	static void BuildSurfaceConformingPreviewProcMesh(
		const FIHTerrainStampDefinition& Definition,
		float RadiusKm,
		float AmplitudeAzgaar,
		bool bInvertHeight,
		float RotationDeg,
		const TFunctionRef<float(float, float)>& SampleSurfaceZCm,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FLinearColor>& OutVertexColors);
};
