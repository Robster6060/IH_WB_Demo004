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
#include "TimerManager.h"
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

	/** Parallel to ShoreManagerInstances (same index = same Shore Manager's owning island) — used
	 * by UpdateShoreManagerVisibilityGating() to check AActor::WasRecentlyRendered() per island. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AIH_WB_IslandActor>> ShoreManagerIslands;

	/** Parallel to the two arrays above — whether each island was on-screen as of the last
	 * visibility-gating check, so the gate only acts on real visible/not-visible transitions
	 * instead of writing the same Mode value every check. */
	TArray<bool> ShoreManagerWasVisible;

	/** Bounded re-call of Force Update/Force Transfer on every Shore Manager, a few seconds apart,
	 * for a fixed small count — NOT an indefinite per-Tick loop. Clears "Shore Warmup"'s one-shot
	 * gate (needs a 2nd call, made >1s after the 1st) and gives Force Transfer a couple of chances
	 * to push real data once Capture Actors/capture output are confirmed populated, then stops and
	 * leaves BP_Shore_Manager_Gen4's own internal "Full Dynamic Gen" timer to continue on its own —
	 * this used to run every 3s indefinitely via Tick(), which caused real, escalating PIE
	 * sluggishness/memory pressure once Resolution was raised to 2048 (repeated real GPU Capture
	 * Scene passes on 3 instances, plus a ReadPixels GPU stall, forever). */
	void RefreshShoreManagersBounded();

	FTimerHandle ShoreRefreshTimerHandle;
	int32 ShoreRefreshCallsRemaining = 0;

	/** 2026-08-28, Observation 1 (Q1/Q2): every island's Shore Manager previously ran its own
	 * internal regen cycle continuously for the whole session regardless of camera framing, all
	 * contending for the ONE shared ocean actor/material's shore-texture parameters — confirmed via
	 * grep there was no visibility/frustum/distance gating anywhere in this file. Checked on a
	 * cheap periodic (NOT per-Tick) timer, NOT per-frame — matches this file's own established
	 * bounded-timer discipline (see RefreshShoreManagersBounded's header comment on why an
	 * unbounded per-Tick diagnostic already caused a real PIE memory-pressure crash once this
	 * session). Toggles "Mode" between FULL_DYNAMIC_MODE (3, the confirmed-working value) when the
	 * camera is within the island's own activation radius and STATIC_MODE (0, no ongoing capture
	 * cost) when it isn't, so only nearby islands keep writing into the shared material. Gates on
	 * camera-to-island DISTANCE, not AActor::WasRecentlyRendered() — that render-thread-derived
	 * signal was tried first and had to be abandoned: real PIE log evidence showed it flapping on a
	 * mechanical ~4s cycle for an island plainly on screen the whole time, which was itself
	 * producing the exact disappear/reappear symptom this gating was meant to fix. Distance is
	 * plain game-thread math against PlayerCameraManager and cannot flap the same way. */
	void UpdateShoreManagerVisibilityGating();

	FTimerHandle ShoreVisibilityGateTimerHandle;
};
