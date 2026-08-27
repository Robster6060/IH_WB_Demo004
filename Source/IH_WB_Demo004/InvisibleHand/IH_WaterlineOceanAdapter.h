// Copyright Epic Games, Inc. All Rights Reserved.
// IH Waterline PRO 6 conversion (IH-DEC-043) — Phase 1/2 adapter actor.
// Owns a spawned BP_Waterline_Ocean_Gen_4 instance and applies IH's stable canonical settings to
// it via Blueprint reflection (the same FindPropertyByName/ProcessEvent pattern used for Ultra
// Dynamic Sky all session) — Waterline ships as a Content-only Blueprint pack with no compile-time
// C++ type, so a true C++ subclass of it is not possible; this wrapper owns/drives it instead.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_WaterlineOceanAdapter.generated.h"

class AIH_WB_IslandActor;

/** Real-world-position ocean surface sample. Phase 5 (real Waterline height/normal/velocity
 * sampling) is not implemented yet — SampleOceanSurface() currently returns the canonical flat
 * fallback (Z=0, up normal, zero velocity), per IH_WaterlinePro_Conversion.md Phase 4 step 7:
 * "Keep IH gameplay queries temporarily returning canonical Z=0... Do not delete legacy code." */
USTRUCT()
struct FIHOceanSurfaceSample
{
	GENERATED_BODY()

	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FVector Velocity = FVector::ZeroVector;
	float DepthCm = 0.f;
	bool bHasDynamicWaves = false;
};

/** Spawns and owns one BP_Waterline_Ocean_Gen_4 instance, applying IH's canonical Water Level and
 * a stable single-cascade configuration. Tagged "IH.Ocean.Primary" so dev-visibility toggles and
 * future systems can find the active ocean without caring which provider spawned it. */
UCLASS(NotBlueprintable)
class IH_WB_DEMO004_API AIH_WaterlineOceanAdapter : public AActor
{
	GENERATED_BODY()

public:
	AIH_WaterlineOceanAdapter();

	virtual void Tick(float DeltaTime) override;

	/** Loads and spawns BP_Waterline_Ocean_Gen_4, applies canonical settings via reflection.
	 * Returns false (and spawns nothing) if the soft-load fails — caller should fall back to the
	 * legacy ocean, per Phase 2 step 7's fail-safe requirement. */
	bool InitializeWaterlineOcean();

	bool IsWaterlineOceanValid() const { return IsValid(WaterlineOceanInstance); }
	AActor* GetWaterlineOceanInstance() const { return WaterlineOceanInstance; }

	/** Stub — see FIHOceanSurfaceSample comment above. Always returns the canonical flat
	 * fallback until Phase 5 implements real Waterline surface sampling. */
	bool SampleOceanSurface(const FVector& WorldLocation, FIHOceanSurfaceSample& OutSample) const;

	void SetOceanVisible(bool bVisible);

	/** Phase 6 (shore integration): spawns one BP_Shore_Manager_Gen4 per island, each sized to
	 * that island's real post-generation footprint (GetActorLocation()/GetMainLandFootprintRadiusCm(),
	 * same evidence-based approach proven in the IH-DEC-042 spike) and wired to this adapter's
	 * Waterline ocean instance via the real "WaterBody" property. No-ops if the Waterline ocean
	 * failed to initialize. */
	void SpawnShoreManagersForIslands(const TArray<TObjectPtr<AIH_WB_IslandActor>>& Islands);

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> WaterlineOceanInstance = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ShoreManagerInstances;

	/** Location each Shore Manager instance was placed at, recorded right after FinishSpawning() —
	 * compared against its live GetActorLocation() every few seconds in Tick() so the Output Log
	 * shows directly whether "Full Dynamic Gen"'s Set Actor Location is moving it, and to where. */
	UPROPERTY(Transient)
	TArray<FVector> ShoreManagerSpawnOrigins;

	float ShoreManagerDiagnosticLogAccumSec = 0.f;
};
