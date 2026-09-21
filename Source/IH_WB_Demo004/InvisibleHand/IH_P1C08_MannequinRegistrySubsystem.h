// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C08_SelectableMannequin.h"
#include "IH_P1C08_MannequinRegistrySubsystem.generated.h"

class AIH_P1C08_MoveDestinationFlag;
class APlayerController;

// Mirrors UIH_P1C07_ShipRegistrySubsystem's own box-select/selection-state/move-order pattern
// exactly (registration, ClearSelection/ToggleSelectMannequin/SetSelection, screen-rect box-select,
// move-order + destination-marker spawn) - a parallel, independent subsystem, never a
// generalization of the ship one, per this project's own Do No Harm convention.
UCLASS()
class IH_WB_DEMO004_API UIH_P1C08_MannequinRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void RegisterMannequin(AActor* Mannequin);
	void UnregisterMannequin(AActor* Mannequin);

	const TArray<TWeakObjectPtr<AActor>>& GetRegisteredMannequins() const { return RegisteredMannequins; }

	TArray<AActor*> GetSelectedMannequins() const;
	void ClearSelection();
	void ToggleSelectMannequin(AActor* Mannequin);
	void SetSelection(const TArray<AActor*>& Mannequins);
	/** Box-select; bAdditive=true unions into current selection (Shift+drag), mirrors
	 * UIH_P1C07_ShipRegistrySubsystem::SelectShipsInScreenRect exactly. */
	void SelectMannequinsInScreenRect(
		APlayerController* PC,
		const FVector2D& ScreenA,
		const FVector2D& ScreenB,
		bool bAdditive = false);
	bool IsMannequinSelected(AActor* Mannequin) const;

	/** LandClickWorld is already resolved/validated against walkable IslandMesh by the caller (the
	 * PlayerController) - unlike the ship version, this doesn't re-validate a destination itself. */
	bool IssueMoveOrderToSelection(APlayerController* PC, const FVector& LandClickWorld, bool bAppendWaypoint = false);

	TArray<AIH_P1C08_MoveDestinationFlag*> GetActiveDestinationFlags();

private:
	void PruneActiveDestinationFlags();
	void RemoveMannequinsFromActiveFlags(const TArray<AActor*>& Mannequins);

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredMannequins;

	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> SelectedMannequins;

	UPROPERTY()
	TArray<TWeakObjectPtr<AIH_P1C08_MoveDestinationFlag>> ActiveDestinationFlags;
};
