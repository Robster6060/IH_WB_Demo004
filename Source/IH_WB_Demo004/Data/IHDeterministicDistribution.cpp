#include "IHDeterministicDistribution.h"

namespace
{
	int32 HashCombineLocal(int32 A, int32 B)
	{
		return A ^ (B + 0x9e3779b9 + (A << 6) + (A >> 2));
	}
}

int32 FIHDeterministicDistribution::DeriveDecisionSeed(
	const uint64 MasterSeed, const FString& StreamName,
	const int32 StableEntityId, const int32 DecisionOrdinal)
{
	int32 Hash = 5381;
	const FString VersionedStream = FString::Printf(
		TEXT("IH_DDU_V%d|%s|D%d|E%d"),
		GeneratorVersion, *StreamName.ToUpper(), DecisionOrdinal, StableEntityId);
	for (TCHAR C : VersionedStream)
	{
		Hash = ((Hash << 5) + Hash) + static_cast<int32>(C);
	}
	return HashCombineLocal(Hash, static_cast<int32>(MasterSeed ^ (MasterSeed >> 32)));
}

int32 FIHDeterministicDistribution::SelectWeightedId(
	const uint64 MasterSeed, const FString& StreamName,
	const int32 StableEntityId,
	const TConstArrayView<FIHWeightedChoice> Choices,
	const int32 DecisionOrdinal)
{
	int64 TotalWeight = 0;
	for (const FIHWeightedChoice& Choice : Choices)
	{
		TotalWeight += FMath::Max(0, Choice.Weight);
	}
	if (Choices.IsEmpty() || TotalWeight <= 0)
	{
		return INDEX_NONE;
	}

	FRandomStream Stream(DeriveDecisionSeed(
		MasterSeed, StreamName, StableEntityId, DecisionOrdinal));
	const int64 Ticket = FMath::Min(
		TotalWeight - 1,
		static_cast<int64>(FMath::FloorToDouble(Stream.FRand() * TotalWeight)));
	int64 CumulativeWeight = 0;
	for (const FIHWeightedChoice& Choice : Choices)
	{
		CumulativeWeight += FMath::Max(0, Choice.Weight);
		if (Ticket < CumulativeWeight)
		{
			return Choice.StableId;
		}
	}
	return Choices.Last().StableId;
}

FVector2D FIHDeterministicDistribution::DeriveNoiseOffset2D(
	const uint64 MasterSeed, const FString& StreamName,
	const int32 StableEntityId, const int32 DecisionOrdinal,
	const double CoordinateRange)
{
	FRandomStream Stream(DeriveDecisionSeed(
		MasterSeed, StreamName, StableEntityId, DecisionOrdinal));
	return FVector2D(
		Stream.FRandRange(-CoordinateRange, CoordinateRange),
		Stream.FRandRange(-CoordinateRange, CoordinateRange));
}

double FIHDeterministicDistribution::SamplePerlin2D(
	const FVector2D& WorldOrLogicalPosition,
	const FVector2D& SeededOffset,
	const double Frequency, const double Amplitude)
{
	return FMath::PerlinNoise2D(
		(WorldOrLogicalPosition + SeededOffset) * Frequency) * Amplitude;
}
