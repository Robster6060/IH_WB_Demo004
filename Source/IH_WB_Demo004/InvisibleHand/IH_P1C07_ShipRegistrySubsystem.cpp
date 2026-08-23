// Copyright Epic Games, Inc. All Rights Reserved.

#include "IH_P1C07_ShipRegistrySubsystem.h"
#include "IH_P1C07_MoveDestinationBuoy.h"
#include "IH_P1C07_ShipFormation.h"
#include "IH_P1C07_NavAvoidanceSubsystem.h"
#include "IH_P1C07_CommandableShipActor.h"
#include "IH_P1C07_WaterQueryHelpers.h"
#include "IH_P1C07_NavAvoidanceSubsystem.h"
#include "IH_P1C07_IslandCollisionSubsystem.h"
#include "GameFramework/PlayerController.h"

void UIH_P1C07_ShipRegistrySubsystem::RegisterShip(AActor* Ship)
{
	if (!Ship)
	{
		return;
	}
	RegisteredShips.AddUnique(Ship);
}

void UIH_P1C07_ShipRegistrySubsystem::UnregisterShip(AActor* Ship)
{
	if (!Ship)
	{
		return;
	}
	RegisteredShips.Remove(Ship);
	SelectedShips.Remove(Ship);
}

TArray<AActor*> UIH_P1C07_ShipRegistrySubsystem::GetSelectedShips() const
{
	TArray<AActor*> Out;
	for (const TWeakObjectPtr<AActor>& Ptr : SelectedShips)
	{
		if (Ptr.IsValid())
		{
			Out.Add(Ptr.Get());
		}
	}
	return Out;
}

void UIH_P1C07_ShipRegistrySubsystem::ClearSelection()
{
	for (const TWeakObjectPtr<AActor>& Ptr : SelectedShips)
	{
		if (AActor* Ship = Ptr.Get())
		{
			if (IIH_P1C07_SelectableShip* Sel = Cast<IIH_P1C07_SelectableShip>(Ship))
			{
				Sel->SetShipSelected(false);
			}
		}
	}
	SelectedShips.Reset();
}

void UIH_P1C07_ShipRegistrySubsystem::ToggleSelectShip(AActor* Ship)
{
	if (!Ship || !Ship->Implements<UIH_P1C07_SelectableShip>())
	{
		return;
	}

	if (SelectedShips.Contains(Ship))
	{
		SelectedShips.Remove(Ship);
		if (IIH_P1C07_SelectableShip* Sel = Cast<IIH_P1C07_SelectableShip>(Ship))
		{
			Sel->SetShipSelected(false);
		}
	}
	else
	{
		SelectedShips.Add(Ship);
		if (IIH_P1C07_SelectableShip* Sel = Cast<IIH_P1C07_SelectableShip>(Ship))
		{
			Sel->SetShipSelected(true);
		}
	}
}

void UIH_P1C07_ShipRegistrySubsystem::SetSelection(const TArray<AActor*>& Ships)
{
	ClearSelection();
	for (AActor* Ship : Ships)
	{
		if (Ship && Ship->Implements<UIH_P1C07_SelectableShip>())
		{
			SelectedShips.Add(Ship);
			if (IIH_P1C07_SelectableShip* Sel = Cast<IIH_P1C07_SelectableShip>(Ship))
			{
				Sel->SetShipSelected(true);
			}
		}
	}
}

void UIH_P1C07_ShipRegistrySubsystem::SelectShipsInScreenRect(
	APlayerController* PC,
	const FVector2D& ScreenA,
	const FVector2D& ScreenB,
	const bool bAdditive)
{
	if (!PC)
	{
		return;
	}

	const float MinX = FMath::Min(ScreenA.X, ScreenB.X);
	const float MaxX = FMath::Max(ScreenA.X, ScreenB.X);
	const float MinY = FMath::Min(ScreenA.Y, ScreenB.Y);
	const float MaxY = FMath::Max(ScreenA.Y, ScreenB.Y);

	TArray<AActor*> Boxed;
	for (const TWeakObjectPtr<AActor>& Ptr : RegisteredShips)
	{
		AActor* Ship = Ptr.Get();
		if (!Ship || !Ship->Implements<UIH_P1C07_SelectableShip>())
		{
			continue;
		}

		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(Ship->GetActorLocation(), ScreenPos, true))
		{
			continue;
		}

		if (ScreenPos.X >= MinX && ScreenPos.X <= MaxX && ScreenPos.Y >= MinY && ScreenPos.Y <= MaxY)
		{
			Boxed.Add(Ship);
		}
	}

	if (!bAdditive)
	{
		SetSelection(Boxed);
		return;
	}

	for (AActor* Ship : Boxed)
	{
		if (!IsShipSelected(Ship))
		{
			SelectedShips.Add(Ship);
			if (IIH_P1C07_SelectableShip* Sel = Cast<IIH_P1C07_SelectableShip>(Ship))
			{
				Sel->SetShipSelected(true);
			}
		}
	}
}

bool UIH_P1C07_ShipRegistrySubsystem::IsShipSelected(AActor* Ship) const
{
	return Ship && SelectedShips.Contains(Ship);
}

void UIH_P1C07_ShipRegistrySubsystem::PruneActiveFleetBuoys()
{
	ActiveFleetBuoys.RemoveAll([](const TWeakObjectPtr<AIH_P1C07_MoveDestinationBuoy>& Ptr) {
		return !Ptr.IsValid();
	});
}

TArray<AIH_P1C07_MoveDestinationBuoy*> UIH_P1C07_ShipRegistrySubsystem::GetActiveFleetBuoys()
{
	PruneActiveFleetBuoys();
	TArray<AIH_P1C07_MoveDestinationBuoy*> Out;
	Out.Reserve(ActiveFleetBuoys.Num());
	for (const TWeakObjectPtr<AIH_P1C07_MoveDestinationBuoy>& Ptr : ActiveFleetBuoys)
	{
		if (AIH_P1C07_MoveDestinationBuoy* Buoy = Ptr.Get())
		{
			Out.Add(Buoy);
		}
	}
	return Out;
}

void UIH_P1C07_ShipRegistrySubsystem::RemoveShipsFromActiveBuoys(const TArray<AActor*>& Ships)
{
	for (const TWeakObjectPtr<AIH_P1C07_MoveDestinationBuoy>& Ptr : ActiveFleetBuoys)
	{
		if (AIH_P1C07_MoveDestinationBuoy* Buoy = Ptr.Get())
		{
			Buoy->RemoveTrackedShips(Ships);
		}
	}

	PruneActiveFleetBuoys();
}

bool UIH_P1C07_ShipRegistrySubsystem::IssueMoveOrderToSelection(
	APlayerController* PC,
	const FVector& WaterClickWorld,
	const FRotator& ApproachHeading,
	const bool bAppendWaypoint)
{
	TArray<AActor*> Selected = GetSelectedShips();
	if (Selected.Num() == 0 || !PC)
	{
		return false;
	}

	UWorld* World = PC->GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<TScriptInterface<IIH_P1C07_SelectableShip>> Ifaces;
	Ifaces.Reserve(Selected.Num());

	const int32 Total = Selected.Num();
	UIH_P1C07_NavAvoidanceSubsystem* Avoidance = nullptr;
	UIH_P1C07_IslandCollisionSubsystem* IslandCollision = nullptr;
	if (UGameInstance* GI = World->GetGameInstance())
	{
		Avoidance = GI->GetSubsystem<UIH_P1C07_NavAvoidanceSubsystem>();
		IslandCollision = GI->GetSubsystem<UIH_P1C07_IslandCollisionSubsystem>();
	}

	FVector ValidatedAnchor = WaterClickWorld;
	if (!IH_P1C07WaterQuery::ResolveOpenOceanMoveDestination(World, WaterClickWorld, ValidatedAnchor, IslandCollision))
	{
		return false;
	}

	uint32 MoveGroupId = 0;
	if (Avoidance)
	{
		MoveGroupId = Avoidance->AllocateMoveOrderGroupId();
	}

	for (int32 Idx = 0; Idx < Total; ++Idx)
	{
		AActor* Ship = Selected[Idx];
		IIH_P1C07_SelectableShip* Sel = Cast<IIH_P1C07_SelectableShip>(Ship);
		if (!Sel)
		{
			continue;
		}

		AIH_P1C07_CommandableShipActor* CmdShip = Cast<AIH_P1C07_CommandableShipActor>(Ship);
		if (CmdShip)
		{
			CmdShip->SetMoveOrderGroupId(MoveGroupId);
		}

		const FVector Offset = IH_P1C07Formation::ComputeBerthOffset(Idx, Total, ApproachHeading.Vector());
		FVector Dest = ValidatedAnchor + Offset;
		FVector ValidatedDest;
		if (!IH_P1C07WaterQuery::ResolveOpenOceanMoveDestination(World, Dest, ValidatedDest, IslandCollision))
		{
			ValidatedDest = ValidatedAnchor;
		}
		Dest = ValidatedDest;

		if (CmdShip)
		{
			if (bAppendWaypoint)
			{
				CmdShip->EnqueueSailWaypoint(Dest, ApproachHeading);
			}
			else
			{
				CmdShip->ReplaceSailOrder(Dest, ApproachHeading);
			}
		}
		else
		{
			Sel->CommandSailTo(Dest, ApproachHeading);
		}

		TScriptInterface<IIH_P1C07_SelectableShip> Entry;
		Entry.SetObject(Ship);
		Entry.SetInterface(Sel);
		Ifaces.Add(Entry);
	}

	if (Ifaces.Num() == 0)
	{
		return false;
	}

	if (!bAppendWaypoint)
	{
		RemoveShipsFromActiveBuoys(Selected);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AIH_P1C07_MoveDestinationBuoy* Buoy = World->SpawnActor<AIH_P1C07_MoveDestinationBuoy>(
			AIH_P1C07_MoveDestinationBuoy::StaticClass(), WaterClickWorld, FRotator::ZeroRotator, Params))
	{
		Buoy->InitOrder(WaterClickWorld, Ifaces);
		ActiveFleetBuoys.Add(Buoy);
	}

	return true;
}
