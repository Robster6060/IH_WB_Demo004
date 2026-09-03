// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * Island internal topography profile. HIGH/VOLC retired (IH-DEC-064/069): procedural HIGH/VOLC
 * generation is replaced by player-placed Terrain Stamps, not by anything in this pipeline - every
 * island generates via Low. Kept as a single-value enum (not collapsed to a bare bool/removed)
 * since ApplyTankLayout/BuildMeshesFromCellGraph's Profile parameter and switch are still real,
 * live call sites - reducing churn on those signatures for a suspension, not the enum's role.
 */
enum class EIHIslandProfile : uint8
{
	Low
};

/** C1 → Phase H: Firth-head snap target for large river spline actors. */
struct FIHRiverTerminusSocket
{
	int32 SocketId = INDEX_NONE;
	int32 IslandIndex = INDEX_NONE;
	int32 FirthIndex = INDEX_NONE;
	/** Island-local cm on MainCoast / dry-wet interface at Firth head. */
	FVector2D LocationLocalCm = FVector2D::ZeroVector;
	/** From land into Firth water (unit). Spline arrives along −Outward / valley axis. */
	FVector2D OutwardTangentXY = FVector2D(1.f, 0.f);
	/** Preferred inland approach for Phase H pathfind heuristic. */
	FVector2D UpstreamValleyAxisXY = FVector2D(-1.f, 0.f);
	float AcceptanceRadiusCm = 5000.f;
	float MinChannelWidthCm = 8000.f;
	bool bOceanConnected = false;
};

/** DEV Features overlay: generator coast-character class (Perlin field, not SeaRoots slope). */
enum class EIHCoastCharacterClass : uint8
{
	Beach = 0,
	Gentle = 1,
	Bluff = 2
};

struct FIHHeightfieldCoastRequest
{
	int32 IslandId = 1;
	int32 NumericSeed = 0;
	int64 TargetAcres = 0;
	int32 SamplesPerSide = 257;
	EIHIslandProfile Profile = EIHIslandProfile::Low;
	/** Organic summit cap (m ASL). Interior relief is normalized to this after profile stamps. */
	float TargetSummitMeters = 140.f;
};

struct FIHHeightfieldCoastMetrics
{
	double TotalMilliseconds = 0.0;
	double FieldMilliseconds = 0.0;
	double RepairMilliseconds = 0.0;
	double ContourMilliseconds = 0.0;
	double DryAcres = 0.0;
	double WWFFootprintAcres = 0.0;
	int32 ExteriorWaterSamples = 0;
	int32 EnclosedWaterSamplesRaised = 0;
	int32 LandComponentCount = 0;
	int32 ResidualIsletCandidateCount = 0;
	int32 HillOperatorCount = 0;
	int32 RangeOperatorCount = 0;
	int32 TroughOperatorCount = 0;
	int32 PitOperatorCount = 0;
	int32 StraitOperatorCount = 0;
	int32 WWFComponentCount = 0;
	/** C1 classification counts (outputs from emergent troughs — not carve quotas). */
	int32 FirthOperatorCount = 0;
	int32 HarborageOperatorCount = 0;
	int32 CoveOperatorCount = 0;
	int32 RiverTerminusSocketCount = 0;
	double MaximumHeightMeters = 0.0;
	float TargetSummitMeters = 0.f;
	float SummitScaleApplied = 1.f;
};

struct FIHHeightfieldCoastResult
{
	int32 SamplesPerSide = 0;
	double HalfExtentMeters = 0.0;
	double SampleSpacingMeters = 0.0;
	TArray<float> HeightsMeters;
	TArray<TPair<FVector2D, FVector2D>> CoastlineSegments;
	TArray<TPair<FVector2D, FVector2D>> ShelfSegments;
	/** Isoline at +25 m ASL (dry Contours ribbon). */
	TArray<TPair<FVector2D, FVector2D>> Plus25Segments;
	/**
	 * Azimuth ring of coast character (256 samples, −π..π).
	 * Values are EIHCoastCharacterClass as uint8.
	 */
	TArray<uint8> CoastCharacterRing;
	TArray<FIHRiverTerminusSocket> RiverTerminusSockets;
	FIHHeightfieldCoastMetrics Metrics;

	bool IsValid() const
	{
		return SamplesPerSide > 1 &&
			HeightsMeters.Num() == SamplesPerSide * SamplesPerSide &&
			!CoastlineSegments.IsEmpty() && !ShelfSegments.IsEmpty();
	}
};

// IH-DEC-064/069: IHPickIslandProfile321 (3:2:1 weighted High/Volc/Low pick) and
// IHComputeTargetSummitMeters (old profile-capped 50-620m summit formula, superseded by
// IHSeedIslandLibrary::ComputeSummitTopZCmForAreas but never actually disconnected from its one
// real call site until this pass - IH_WB_Demo004GameMode.cpp's RegenerateIslandsFromSeed) are
// both retired. Every island resolves to EIHIslandProfile::Low directly; summit height comes
// from the already-computed ComputeSummitTopZCmForAreas array.
