// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_P1C07_NavAvoidanceTypes.generated.h"

/** Where avoidance runs (layer stack varies by mode). */
UENUM(BlueprintType)
enum class EIH_P1C07_NavDomainMode : uint8
{
	/** Sea, open field, open air cruise — A+B+C+D. */
	OpenSurface,
	/** Road splines + junction actors — E+B+D (+A at junction). Phase P4. */
	RoadNetwork,
	/** Port berth / aerodrome moor queue — E+priority queue+D. Phase P6. */
	TerminalApproach,
};

/** Layer identifiers for the shared avoidance stack (A–E). */
UENUM(BlueprintType)
enum class EIH_P1C07_NavAvoidanceLayer : uint8
{
	Separation = 0,
	CPAYield,
	GroupCohesion,
	HardSafety,
	CorridorSlot,
};

/** Reserved route / lane / terminal slot (layer E — data only in P0–P2). */
USTRUCT(BlueprintType)
struct FIH_P1C07_NavCorridor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CorridorId = NAME_None;

	/** Road spline, approach path, or hold pattern id (future). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 JunctionTokenId = INDEX_NONE;
};

/** Desired motion before avoidance modifiers. */
USTRUCT(BlueprintType)
struct FIH_P1C07_NavIntent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DesiredDirection2D = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DesiredSpeedCmPerSec = 0.f;
};

/** Output of avoidance pass. */
USTRUCT(BlueprintType)
struct FIH_P1C07_NavAvoidanceResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SteerDirection2D = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHardStop = false;
};

/** Static open-surface obstacle (island shorelines, reefs). Axes align with world yaw. */
USTRUCT(BlueprintType)
struct FIH_P1C07_NavEllipticalObstacle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector CenterWorld = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawDegrees = 0.f;

	/** Waterline semi-axes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SemiMajorCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SemiMinorCm = 0.f;

	/** Standoff beyond hull to obstacle edge (cm). Uses subsystem default when <= 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClearanceMarginCm = 0.f;

	/** Soft steer band outside the standoff envelope (cm). Uses subsystem default when <= 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RepulsionBandCm = 0.f;
};

/** Static open-surface shoreline polygon (world XY, closed loop). */
USTRUCT(BlueprintType)
struct FIH_P1C07_NavPolygonObstacle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector2D> WorldVertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClearanceMarginCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RepulsionBandCm = 0.f;
};
