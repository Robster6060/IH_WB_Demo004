// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "IHInvisibleHandDesignSpec.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "IH_P1C12_OceanPlane.h"
#include "IH_WB_Demo004.h"

/** Shared water queries for ships, cubes, and buoys (WaterBody and/or Gate0 OceanPlane). */
namespace IH_P1C07WaterQuery
{
	inline bool QuerySurface(
		UWaterBodyComponent* Wbc,
		const FVector& AtLocation,
		FVector& OutSurfaceLoc,
		FVector& OutNormal,
		FVector& OutVelocity,
		float& OutDepth)
	{
		if (!Wbc)
		{
			return false;
		}
		return Wbc->GetWaterSurfaceInfoAtLocation(AtLocation, OutSurfaceLoc, OutNormal, OutVelocity, OutDepth, true);
	}

	/** Gate0: ASL waterline from OceanPlane actor Z, else DesignSpec sea level (canon 0). */
	inline float QueryGate0OceanSurfaceZCm(UWorld* World)
	{
		if (World)
		{
			for (TActorIterator<AIH_P1C12_OceanPlane> It(World); It; ++It)
			{
				return It->GetActorLocation().Z;
			}
		}
		return IHInvisibleHandSpec::GetDevDemoOceanSurfaceZCm();
	}

	inline bool QueryBestSurfaceInWorld(
		UWorld* World,
		const FVector& AtLocation,
		FVector& OutSurfaceLoc,
		FVector& OutNormal,
		FVector& OutVelocity)
	{
		if (!World)
		{
			return false;
		}

		float BestZ = -TNumericLimits<float>::Max();
		bool bFound = false;
		FVector Loc = AtLocation;
		FVector Normal = FVector::UpVector;
		FVector Velocity = FVector::ZeroVector;

		for (TActorIterator<class AWaterBody> It(World); It; ++It)
		{
			FVector CandidateLoc;
			FVector CandidateNormal;
			FVector CandidateVelocity;
			float CandidateDepth = 0.f;
			if (!QuerySurface(It->GetWaterBodyComponent(), AtLocation, CandidateLoc, CandidateNormal, CandidateVelocity, CandidateDepth))
			{
				continue;
			}
			if (CandidateLoc.Z >= BestZ)
			{
				BestZ = CandidateLoc.Z;
				Loc = CandidateLoc;
				Normal = CandidateNormal;
				Velocity = CandidateVelocity;
				bFound = true;
			}
		}

		if (!bFound)
		{
			// Gate0 custom ocean: no WaterBody surface — use OceanPlane / ASL 0 waterline.
			const float SurfaceZ = QueryGate0OceanSurfaceZCm(World);
			Loc = FVector(AtLocation.X, AtLocation.Y, SurfaceZ);
			Normal = FVector::UpVector;
			Velocity = FVector::ZeroVector;
			bFound = true;
		}

		if (bFound)
		{
			OutSurfaceLoc = Loc;
			OutNormal = Normal.GetSafeNormal();
			OutVelocity = Velocity;
		}
		return bFound;
	}

	/**
	 * Validates LMB move / Place Ship clicks: water surface exists and point is not dry land
	 * (bays / inlets / harbors OK).
	 */
	inline bool ResolveOpenOceanMoveDestination(
		UWorld* World,
		const FVector& CandidateWorld,
		FVector& OutWaterSurfaceDest,
		UIH_P1C07_IslandCollisionSubsystem* IslandCollision = nullptr)
	{
		if (!World)
		{
			return false;
		}

		FVector SurfaceLoc;
		FVector Normal;
		FVector Velocity;
		if (!QueryBestSurfaceInWorld(World, CandidateWorld, SurfaceLoc, Normal, Velocity))
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogIH_WB_Demo004, Log,
				TEXT("ResolveOpenOcean: no water surface at (%.0f,%.0f,%.0f)"),
				CandidateWorld.X, CandidateWorld.Y, CandidateWorld.Z);
#endif
			return false;
		}

		if (IslandCollision
			&& IslandCollision->IsDryLandAtWaterSurface(
				FVector(SurfaceLoc.X, SurfaceLoc.Y, 0.f), SurfaceLoc.Z))
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogIH_WB_Demo004, Log,
				TEXT("ResolveOpenOcean: dry land reject at (%.0f,%.0f) surfaceZ=%.0f"),
				SurfaceLoc.X, SurfaceLoc.Y, SurfaceLoc.Z);
#endif
			return false;
		}

		OutWaterSurfaceDest = SurfaceLoc;
		return true;
	}
}
