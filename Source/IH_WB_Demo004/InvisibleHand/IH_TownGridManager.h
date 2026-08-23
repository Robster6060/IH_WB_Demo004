// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "GameFramework/Actor.h"
#include "IH_BuildPaletteTypes.h"
#include "IH_TownGridTemplateRow.h"
#include "IH_TownGridTypes.h"
#include "IH_TownGridManager.generated.h"

class UBoxComponent;
class USceneComponent;
class UTownGridOverlayComponent;

UCLASS()
class IH_WB_DEMO004_API AIH_TownGridManager : public AActor
{
	GENERATED_BODY()

public:
	AIH_TownGridManager();

	static constexpr float ModuleSizeCm = 800.f;
	static constexpr int32 DefaultModuleCount = 16;
	static constexpr int32 DefaultCollectorIntervalModules = 4;

	static FVector2D GetDefaultBboxHalfExtentCm();
	static FName TownGridTemplateToRowName(EIHTownGridTemplate Template);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Town Grid")
	void InitializeFromTemplate(EIHTownGridTemplate Template);

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Town Grid")
	void RebuildFromTemplate();

	UFUNCTION(BlueprintCallable, Category = "Invisible Hand|Town Grid")
	void AlignActorToTerrainCenter();

	void SetSelected(bool bInSelected);
	bool IsSelected() const { return bSelected; }
	bool IsGripDragActive() const { return bGripDragActive; }

	bool TryHitGripAtWorld(const FVector& WorldPoint, EIHTownGridGripHandle& OutHandle) const;
	void BeginGripDrag(EIHTownGridGripHandle Handle, const FVector& WorldPoint);
	void UpdateGripDrag(const FVector& WorldPoint);
	void EndGripDrag();

	void BeginMoveDrag(const FVector& WorldPoint);
	void UpdateMoveDrag(const FVector& WorldPoint);
	void EndMoveDrag();
	bool IsMoveDragActive() const { return bMoveDragActive; }

	void ApplyYawStep(float DeltaDeg);
	void ApplyWheelYaw(float WheelDelta);

	/** True when WorldPoint (XY) lies inside the rotated town-grid footprint. */
	bool ContainsWorldPointXY(const FVector& WorldPoint) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	EIHTownGridTemplate TownGridTemplate = EIHTownGridTemplate::Squared;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Town Grid")
	FIHTownGridTemplateRow TemplateData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	FVector2D BboxHalfExtentCm = FVector2D(6400.f, 6400.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invisible Hand|Town Grid")
	EIHTownGridEditMode EditMode = EIHTownGridEditMode::Place;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Town Grid")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Town Grid")
	TObjectPtr<UTownGridOverlayComponent> GridOverlay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Invisible Hand|Town Grid")
	TObjectPtr<UBoxComponent> FocusBBox;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void LoadTemplateRow();
	void UpdateGripTransforms();
	void SetGripsVisible(bool bVisible);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> GripHandles;

	bool bSelected = false;
	bool bGripDragActive = false;
	EIHTownGridGripHandle ActiveGripHandle = EIHTownGridGripHandle::None;
	FVector2D GripDragStartHalfExtentCm = FVector2D::ZeroVector;
	FVector GripDragStartWorld = FVector::ZeroVector;

	bool bMoveDragActive = false;
	FVector MoveDragStartWorld = FVector::ZeroVector;
	FVector MoveDragStartActorLoc = FVector::ZeroVector;

	static constexpr float GripBoxHalfSizeCm = 400.f;
	static constexpr float GripPickRadiusCm = 1200.f;
	static constexpr float YawStepDeg = 5.f;
	static constexpr float WheelYawDegPerUnit = 3.f;
};
