// Copyright Invisible Hand. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/** Island internal topography profile (3:2:1 Low:High:Volc). Independent of coastline shape. */
enum class EIHIslandProfile : uint8
{
	Low,
	High,
	Volc
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

/** Weighted 3:2:1 profile pick from master seed + island index. */
inline EIHIslandProfile IHPickIslandProfile321(int32 MasterSeed, int32 IslandIndex)
{
	const uint32 H = static_cast<uint32>(MasterSeed) ^ (static_cast<uint32>(IslandIndex + 1) * 2654435761u);
	const uint32 Bucket = H % 6u;
	if (Bucket == 5u) { return EIHIslandProfile::Volc; }
	if (Bucket >= 3u) { return EIHIslandProfile::High; }
	return EIHIslandProfile::Low;
}

/**
 * Organic summit height (m ASL) from waterline area + Low/High/Volc H/D table.
 * D_km = 2 * sqrt(Area_km2 / pi); H = clamp(H/D * D_m).
 * H/D ratios are φ-derived lookup constants (no runtime φ math):
 * Low=1/φ^5, High=1/φ^4, Volc=1/φ^3.
 */
inline float IHComputeTargetSummitMeters(float AreaKm2, EIHIslandProfile Profile)
{
	const float Area = FMath::Max(AreaKm2, 0.05f);
	const float DiameterM = 2000.f * FMath::Sqrt(Area / PI);
	// Baked φ^{-n} constants (φ = 1.6180339887…).
	constexpr float InvPhi5 = 0.0901699437f; // Low
	constexpr float InvPhi4 = 0.1458980338f; // High
	constexpr float InvPhi3 = 0.2360679775f; // Volc
	float Ratio = InvPhi5;
	float MinH = 50.f;
	float MaxH = 180.f;
	switch (Profile)
	{
	case EIHIslandProfile::High:
		Ratio = InvPhi4;
		MinH = 160.f;
		MaxH = 420.f;
		break;
	case EIHIslandProfile::Volc:
		Ratio = InvPhi3;
		MinH = 240.f;
		MaxH = 620.f;
		break;
	default:
		break;
	}
	return FMath::Clamp(Ratio * DiameterM, MinH, MaxH);
}
