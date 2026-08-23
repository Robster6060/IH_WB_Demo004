// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "IH_P1C07_NavAvoidanceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C07_NavAvoidanceSubsystem.generated.h"

class IIH_P1C07_NavAvoidanceParticipant;

UCLASS()
class IH_WB_DEMO004_API UIH_P1C07_NavAvoidanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxAvoidingParticipants = 10;

	void RegisterParticipant(AActor* Actor);
	void UnregisterParticipant(AActor* Actor);

	void RegisterEllipticalObstacle(AActor* Source, const FIH_P1C07_NavEllipticalObstacle& Obstacle);
	void UnregisterEllipticalObstacle(AActor* Source);

	void RegisterPolygonObstacle(AActor* Source, const FIH_P1C07_NavPolygonObstacle& Obstacle);
	void UnregisterPolygonObstacle(AActor* Source);

	uint32 AllocateMoveOrderGroupId();

	bool ComputeAvoidance(
		const IIH_P1C07_NavAvoidanceParticipant* Self,
		const FIH_P1C07_NavIntent& Intent,
		FIH_P1C07_NavAvoidanceResult& OutResult,
		bool bIncludeStaticObstacleAvoidance = false) const;

	/** True when another sailing participant is within CongestedEnvelopeRadiusCm. */
	bool HasCongestedTraffic(
		const IIH_P1C07_NavAvoidanceParticipant* Self,
		float& OutNearestNeighborDistCm) const;

	/** True when ship is within IslandProximityBandCm of an expanded island standoff. */
	bool IsWithinIslandProximity(const FVector& WorldLoc, float HullRadiusCm) const;

	/** Projects InOutWorldLoc to the nearest valid water point outside island standoff envelopes. */
	bool ClampPointOutsideStaticObstacles(
		FVector& InOutWorldLoc,
		float HullRadiusCm,
		float ExtraClearanceMarginCm = 0.f) const;

	/** True when segment A→B passes through the hull-expanded island standoff ellipse. */
	bool DoesSegmentIntersectExpandedObstacle(
		const FVector& AWorld,
		const FVector& BWorld,
		float HullRadiusCm) const;

	/** True when Ship→Dest crosses an island standoff envelope, or ship is already in island proximity. */
	bool NeedsIslandReroute(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm) const;

	/**
	 * When Ship→Dest crosses an island standoff envelope, fills OutWaypoints with 1–2 arc
	 * waypoints on the shorter side (then caller resumes direct sail to Dest).
	 */
	bool TryBuildGoAroundWaypoints(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm,
		TArray<FVector>& OutWaypoints) const;

	/** Single contingency step along the island standoff ring when hugging the shore. */
	bool TryBuildShoreFollowStep(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm,
		FVector& OutWaypoint,
		float ExtraArcBiasRadians = 0.f) const;

	/** True when ship can resume direct sail to Dest without crossing island standoff. */
	bool CanResumeDirectSailToDestination(
		const FVector& ShipPos,
		const FVector& DestPos,
		float HullRadiusCm) const;

	/** Draw orange hull / standoff envelopes for ships and islands (call each frame while debugging). */
	void DrawCollisionEnvelopesDebug(
		UWorld* World,
		const TArray<AActor*>& ShipsToDraw,
		FColor Color = FColor::Orange,
		float DurationSec = 0.05f) const;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float SeparationRadiusCm = 12000.f;

	/** Ship-ship avoidance is off in open ocean until a neighbor enters this radius (cm). */
	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float CongestedEnvelopeRadiusCm = 15000.f;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float WarningRadiusCm = 8000.f;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float CriticalRadiusCm = 3500.f;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float CPAHorizonSec = 20.f;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float CPAMinDistanceCm = 6000.f;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float NPCStandOnPriority = 50;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance")
	float PlayerStandOnPriority = 100;

	/** Hull-to-shore standoff beyond the registered obstacle edge (cm). */
	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance|Static")
	float StaticObstacleClearanceMarginCm = 2200.f;

	/** Soft steering band outside the standoff envelope (cm). */
	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance|Static")
	float StaticObstacleRepulsionBandCm = 2500.f;

	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance|Static")
	float StaticObstacleHardStopGapCm = 800.f;

	/** Island mesh sweep / proximity checks activate within this band outside standoff (cm). */
	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance|Static")
	float IslandProximityBandCm = 10000.f;

	/** Go-around waypoints sit on this scale of the expanded standoff ring (mesh clearance). */
	UPROPERTY(EditAnywhere, Category = "P1C07|Avoidance|Static")
	float GoAroundWaypointRingScale = 1.52f;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Participants;

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FIH_P1C07_NavEllipticalObstacle> EllipticalObstacles;

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FIH_P1C07_NavPolygonObstacle> PolygonObstacles;

	uint32 NextMoveOrderGroupId = 1;

	void GatherNeighbors(
		const IIH_P1C07_NavAvoidanceParticipant* Self,
		TArray<const IIH_P1C07_NavAvoidanceParticipant*>& OutNeighbors) const;

	bool ComputeStaticObstacleAvoidance(
		const IIH_P1C07_NavAvoidanceParticipant* Self,
		const FVector& SteerIn,
		FVector& InOutSteer,
		float& InOutSpeedScale,
		bool& InOutHardStop) const;
};
