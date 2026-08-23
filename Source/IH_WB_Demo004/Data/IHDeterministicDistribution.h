#pragma once

#include "CoreMinimal.h"

struct FIHWeightedChoice
{
    int32 StableId = 0;
    int32 Weight = 0;
};

// Canonical IH Deterministic Distribution Utility (IH DDU).
// Select proposes a stable weighted category; Sample provides coherent spatial
// variation; the calling domain remains responsible for validation/fallback.
class IH_WB_DEMO004_API FIHDeterministicDistribution
{
public:
    static constexpr int32 GeneratorVersion = 1;

    static int32 DeriveDecisionSeed(
        uint64 MasterSeed, const FString& StreamName,
        int32 StableEntityId, int32 DecisionOrdinal = 0);

    static int32 SelectWeightedId(
        uint64 MasterSeed, const FString& StreamName, int32 StableEntityId,
        TConstArrayView<FIHWeightedChoice> Choices,
        int32 DecisionOrdinal = 0);

    static FVector2D DeriveNoiseOffset2D(
        uint64 MasterSeed, const FString& StreamName, int32 StableEntityId,
        int32 DecisionOrdinal = 0, double CoordinateRange = 10000.0);

    static double SamplePerlin2D(
        const FVector2D& WorldOrLogicalPosition,
        const FVector2D& SeededOffset,
        double Frequency, double Amplitude = 1.0);
};
