// Copyright Epic Games, Inc. All Rights Reserved.
// IH P1C12 — Custom ocean plane actor.
// Uses UProceduralMeshComponent with per-tick Gerstner vertex animation.
// Flat horizon skirt fills sky past the Gerstner tile. No Water Plugin actors.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_P1C12_OceanPlane.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** One Gerstner wave train. */
USTRUCT()
struct FIHGerstnerWave
{
	GENERATED_BODY()

	/** Direction of travel (world XY unit vector). */
	FVector2D Direction = FVector2D(1.f, 0.f);

	/** Peak-to-trough amplitude (cm). */
	float AmplitudeCm = 50.f;

	/** Wavelength (cm). */
	float WavelengthCm = 100000.f;

	/** Phase speed multiplier on deep-water c = sqrt(g/k). */
	float PhaseSpeed = 1.f;

	/** Wave steepness 0–1 (0 = sinusoidal, 1 = sharpest Gerstner peak). */
	float Steepness = 0.3f;
};

/**
 * Camera-following Gerstner ocean tile at Z=0 (endless sea look) plus a flat
 * far-field horizon skirt. Island packing still uses the φ water tank.
 */
UCLASS(NotBlueprintable)
class IH_WB_DEMO004_API AIH_P1C12_OceanPlane : public AActor
{
	GENERATED_BODY()

public:
	AIH_P1C12_OceanPlane();

	/**
	 * Half-extent E-W/N-S (cm). Default 400,000 (4 km) → 8×8 km Gerstner tile
	 * for hull-proportioned waves (Merchantman 37.8 m). Horizon skirt fills beyond.
	 * Cell size = 2×half / GridDivisions.
	 */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|Grid")
	float PlaneHalfExtentCm = 400000.f;

	/**
	 * Flat non-animated skirt half-extent (cm). Default 15,000,000 (150 km) →
	 * 300×300 km horizon fill under the Gerstner tile.
	 */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|HorizonSkirt")
	float HorizonSkirtHalfExtentCm = 15000000.f;

	/** Skirt Z offset (cm) below Gerstner so it only reads past the tile rim. */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|HorizonSkirt")
	float HorizonSkirtZOffsetCm = -3.f;

	/**
	 * Vertex grid divisions per side.
	 * 256 @ 4 km half → cell ≈ 31.3 m; ship λ clamped ≥ ~2.0× cell.
	 */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|Grid", meta = (ClampMin = "32", ClampMax = "256"))
	int32 GridDivisions = 256;

	/** Snap actor XY to player/view each tick (Z stays 0). Tank layout is unchanged. */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|Endless")
	bool bEndlessSeaFollowCamera = true;

	/** Master wave animation speed multiplier (0 = frozen). */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|Waves", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float WaveTimeScale = 1.f;

	/** DEV translucent ocean so Sea Shelf WWF reads under the waterline. */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|Material", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float OceanOpacity = 0.40f;

	/** User-supplied material. Null = built-in translucent deep-blue. */
	UPROPERTY(EditAnywhere, Category = "P1C12|Ocean|Material")
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	/** Apply endless-sea defaults and rebuild (called from GameMode after spawn). */
	void ConfigureEndlessSea();

	/** Rebuild Gerstner tile + horizon skirt. */
	void RebuildOceanMesh();

	/** DEV View Ocean checkbox — hide/show Gerstner + skirt. */
	void ApplyDevOceanVisibility(bool bOceanVisible);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	void BuildPlaceholderOceanMaterial();
	void ApplyOceanMaterials();
	void RebuildHorizonSkirt();
	void InitWaveTrains();
	void FollowCameraXY();
	float GetGridCellCm() const;

	void TickWaveDisplacement(float WorldTimeSec, float FadeIn);
	FVector SampleGerstner(const FIHGerstnerWave& W, float WorldX, float WorldY, float T) const;
	FVector ComputeNormal(float WorldX, float WorldY, float T) const;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> OceanMesh = nullptr;

	/** Flat far-field quad — no Gerstner tick cost. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> HorizonSkirtMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlaceholderMID = nullptr;

	/** Flat base XY in actor-local cm (mesh centered on actor). */
	TArray<FVector2D> BaseXY;

	TArray<FIHGerstnerWave> WaveTrains;

	float FadeInAlpha = 0.f;
	int32 CachedN = 0;
};
