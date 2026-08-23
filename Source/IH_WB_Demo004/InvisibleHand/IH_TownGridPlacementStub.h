// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridPlacementStub.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * M1 placement placeholder — deprecated; use AIH_TownGridManager (M2).
 * Spawns on palette drag-drop with a flat focus-blue debug bbox (T1 16×16 @ 8 m modules default).
 */
UCLASS()
class IH_WB_DEMO004_API AIH_TownGridPlacementStub : public AActor
{
	GENERATED_BODY()

public:
	AIH_TownGridPlacementStub();

	/** 8 m module (800 cm) — IHInvisibleHandDesignSpec Town Grid parcel grid. */
	static constexpr float ModuleSizeCm = 800.f;

	/** T1 default footprint (16×16 modules). */
	static constexpr int32 DefaultModuleCount = 16;

	static FVector GetDefaultFlatHalfExtentCm();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	EIHTownGridTemplate TownGridTemplate = EIHTownGridTemplate::Squared;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Town Grid")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Town Grid")
	TObjectPtr<UBoxComponent> DebugFlatBBox;

protected:
	virtual void BeginPlay() override;
};
