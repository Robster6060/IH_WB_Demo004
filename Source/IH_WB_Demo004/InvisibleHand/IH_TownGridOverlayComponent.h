// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Components/ActorComponent.h"
#include "IH_TownGridTypes.h"
#include "IH_TownGridOverlayComponent.generated.h"

UCLASS(ClassGroup = (InvisibleHand), meta = (BlueprintSpawnableComponent))
class IH_WB_DEMO004_API UTownGridOverlayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTownGridOverlayComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	bool bShowOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	float OverlayLiftCm = 25.f;

	void RebuildOverlay(const FTownGridOverlayData& OverlayData);

	/** Shared M3 DrawDebug overlay (component tick + palette drag ghost). */
	static void DrawOverlayData(
		UWorld* World,
		const FTownGridOverlayData& OverlayData,
		const FVector& Center,
		float YawDeg,
		float OverlayLiftCm = 25.f,
		bool bPreviewAlpha = false,
		const AActor* IgnoreActor = nullptr,
		bool bConformToTerrain = true);

	/** Build palette drag ghost — same conformed DrawDebug path as M3 grid preview. */
	static void DrawBuildFootprintPreview(
		UWorld* World,
		const FVector& SurfaceCenterWorld,
		const FVector2D& FootprintHalfExtentCm,
		float YawDeg = 0.f,
		const AActor* IgnoreActor = nullptr);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(Transient)
	FTownGridOverlayData CachedOverlay;
};
