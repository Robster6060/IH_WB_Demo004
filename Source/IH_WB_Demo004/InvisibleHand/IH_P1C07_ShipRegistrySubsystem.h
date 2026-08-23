// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IH_WB_Demo004.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IH_P1C07_SelectableShip.h"
#include "IH_P1C07_ShipRegistrySubsystem.generated.h"

class AIH_P1C07_MoveDestinationBuoy;
class APlayerController;

UCLASS()
class IH_WB_DEMO004_API UIH_P1C07_ShipRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void RegisterShip(AActor* Ship);
	void UnregisterShip(AActor* Ship);

	const TArray<TWeakObjectPtr<AActor>>& GetRegisteredShips() const { return RegisteredShips; }

	TArray<AActor*> GetSelectedShips() const;
	void ClearSelection();
	void ToggleSelectShip(AActor* Ship);
	void SetSelection(const TArray<AActor*>& Ships);
	/** Box-select; bAdditive=true unions into current selection (Shift+drag). */
	void SelectShipsInScreenRect(
		APlayerController* PC,
		const FVector2D& ScreenA,
		const FVector2D& ScreenB,
		bool bAdditive = false);
	bool IsShipSelected(AActor* Ship) const;

	bool IssueMoveOrderToSelection(
		APlayerController* PC,
		const FVector& WaterClickWorld,
		const FRotator& ApproachHeading,
		bool bAppendWaypoint = false);

	TArray<AIH_P1C07_MoveDestinationBuoy*> GetActiveFleetBuoys();

private:
	void PruneActiveFleetBuoys();
	void RemoveShipsFromActiveBuoys(const TArray<AActor*>& Ships);

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredShips;

	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> SelectedShips;

	UPROPERTY()
	TArray<TWeakObjectPtr<AIH_P1C07_MoveDestinationBuoy>> ActiveFleetBuoys;
};
