// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_WaterlineOceanAdapter.h"

namespace
{
	/** Same FFloatProperty/FDoubleProperty-aware pattern established for UDS this session
	 * (IH_WB_Demo004GameMode.cpp) — UE5 Blueprint "Float" variables commonly compile to
	 * FDoubleProperty under Large World Coordinates, not FFloatProperty, so a cast-only-to-float
	 * helper silently no-ops on most Waterline properties too. Duplicated here rather than shared
	 * to keep this adapter self-contained. */
	static bool SetWaterlineFloatProperty(AActor* Actor, const TCHAR* PropertyName, float Value)
	{
		if (!Actor) { return false; }
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(PropertyName));
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			FloatProp->SetPropertyValue_InContainer(Actor, Value);
			return true;
		}
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			DoubleProp->SetPropertyValue_InContainer(Actor, static_cast<double>(Value));
			return true;
		}
		return false;
	}
}

AIH_WaterlineOceanAdapter::AIH_WaterlineOceanAdapter()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

bool AIH_WaterlineOceanAdapter::InitializeWaterlineOcean()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UClass* OceanClass = StaticLoadClass(AActor::StaticClass(), nullptr,
		TEXT("/Game/Waterline/9_Ocean_Sim_BETA/BP_Waterline_Ocean_Gen_4.BP_Waterline_Ocean_Gen_4_C"));
	if (!OceanClass)
	{
		UE_LOG(LogIH_WB_Demo004, Warning,
			TEXT("Waterline adapter: failed to load BP_Waterline_Ocean_Gen_4 class — falling back to legacy ocean."));
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* OceanActor = World->SpawnActor<AActor>(OceanClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!OceanActor)
	{
		UE_LOG(LogIH_WB_Demo004, Warning, TEXT("Waterline adapter: SpawnActor failed — falling back to legacy ocean."));
		return false;
	}

	// "1 Water Level" confirmed real via headless reflection (already ~0 by vendor default, set
	// explicitly here so canonical sea level Z=0 is never left to an unverified default).
	const bool bWaterLevelSet = SetWaterlineFloatProperty(OceanActor, TEXT("1 Water Level"), 0.f);

	OceanActor->Tags.Add(TEXT("IH.Ocean.Primary"));
	WaterlineOceanInstance = OceanActor;

	UE_LOG(LogIH_WB_Demo004, Log,
		TEXT("Waterline adapter: spawned BP_Waterline_Ocean_Gen_4 ('%s'), waterLevelSet=%d. FFT simulation and Enable Ocean confirmed already true by vendor default (verified via headless reflection, not assumed)."),
		*OceanActor->GetName(), bWaterLevelSet ? 1 : 0);

	return true;
}

bool AIH_WaterlineOceanAdapter::SampleOceanSurface(const FVector& WorldLocation, FIHOceanSurfaceSample& OutSample) const
{
	// Phase 5 stub — see FIHOceanSurfaceSample's header comment. Canonical flat fallback only.
	OutSample.Location = FVector(WorldLocation.X, WorldLocation.Y, 0.f);
	OutSample.Normal = FVector::UpVector;
	OutSample.Velocity = FVector::ZeroVector;
	OutSample.DepthCm = 0.f;
	OutSample.bHasDynamicWaves = false;
	return true;
}

void AIH_WaterlineOceanAdapter::SetOceanVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	if (IsValid(WaterlineOceanInstance))
	{
		WaterlineOceanInstance->SetActorHiddenInGame(!bVisible);
	}
}
